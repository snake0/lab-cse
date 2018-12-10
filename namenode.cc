#include "namenode.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sys/stat.h>
#include <unistd.h>
#include "threader.h"

using namespace std;

void NameNode::init(const string &extent_dst, const string &lock_dst) {
    ec = new extent_client(extent_dst);
    lc = new lock_client_cache(lock_dst);
    yfs = new yfs_client(extent_dst, lock_dst);

    /* Add your init logic here */
    NewThread(this, &NameNode::CheckAlive);
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
    list<LocatedBlock> ret;
    extent_protocol::attr attr{};
    CHECK(ec->getattr(ino, attr), "ec: getattr in GetBlockLocations", ret);
    uint file_size = attr.size;

    list<blockid_t> block_ids;
    CHECK(ec->get_block_ids(ino, block_ids), "ec: get_blocks_ids", ret);
    uint offset = 0;
    for (auto &iter : block_ids) {
        ret.emplace_back(iter, offset, MIN(file_size - offset, BLOCK_SIZE), live_datanodes);
        offset += BLOCK_SIZE;
    }
    return ret;
}

bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) {
    bool ret = true;
    if (ec->complete(ino, new_size)) {
        fprintf(stderr, "ec: complete error\n");
        fflush(stderr);
        ret = false;
    }
    lc->release(ino);
    return ret;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
    extent_protocol::attr attr{};
    CHECK(ec->getattr(ino, attr), "ec: getattr in AppendBlock", LocatedBlock(0, 0, 0, live_datanodes));
    uint file_size = attr.size;

    blockid_t bid;
    CHECK(ec->append_block(ino, bid), "ec: append_block", LocatedBlock(0, 0, 0, live_datanodes));
    datanode_info[master_datanode].blocks.insert(bid);
    for (auto &iter:live_datanodes)
        if (!SameDatanode(master_datanode, iter))
            ReplicateBlock(bid, master_datanode, iter);
    return LocatedBlock(bid, file_size, 0, live_datanodes);
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
    src_name += "/";
    dst_name += "/";

    string src_buf, dst_buf;
    CHECK(ec->get(src_dir_ino, src_buf), "ec: get src dir", false);
    CHECK(ec->get(dst_dir_ino, dst_buf), "ec: get dst dir", false);

    unsigned long start = src_buf.find(src_name);

    if (src_dir_ino == dst_dir_ino) {
        dst_buf.replace(start, src_name.length(), dst_name);
        src_buf = dst_buf;
    } else {
        unsigned long end = src_buf.substr(start + src_name.length()).find('/');
        dst_buf += src_buf.substr(start, end - start).replace(0, src_name.length(), dst_name);
        src_buf.replace(start, end - start, "");
    }

    CHECK(ec->put(src_dir_ino, src_buf), "ec: put src dir", false);
    CHECK(ec->put(dst_dir_ino, dst_buf), "ec: put dst dir", false);

    return true;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
    CHECK(yfs->_mkdir(parent, name.c_str(), mode, ino_out), "yfs: _mkdir", false);
    return true;
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
    CHECK(yfs->_create(parent, name.c_str(), mode, ino_out), "yfs: _create", false);
    fprintf(stderr, "---- Create file : %llu\n", ino_out);
    fflush(stderr);

    lc->acquire(ino_out);
    return true;
}

bool NameNode::Isfile(yfs_client::inum ino) {
    return yfs->_isfile(ino);
}

bool NameNode::Isdir(yfs_client::inum ino) {
    return yfs->_isdir(ino);
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &info) {
    CHECK(yfs->_getfile(ino, info), "yfs: _getfile", false);
    return true;
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &info) {
    CHECK(yfs->_getdir(ino, info), "yfs: _getdir", false);
    return true;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
    CHECK(yfs->_readdir(ino, dir), "yfs: _readdir", false);
    return true;
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
    CHECK(yfs->_unlink(parent, name.c_str()), "yfs: _unlink", false);
    return true;
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
    datanode_info[id].silent = 0;
}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
    set<blockid_t> to_replicate;

    if (!InDatanodeList(reg_datanodes, id)) {
        reg_datanodes.push_back(id);
        live_datanodes.push_back(id);
        if (reg_datanodes.empty()) {
            datanode_info[id] = DatanodeInfo();
            return;
        }
    } else
        live_datanodes.push_back(id);

    to_replicate = datanode_info[master_datanode].blocks;
    for (auto &iter : to_replicate)
        ReplicateBlock(iter, master_datanode, id);
    datanode_info[id] = DatanodeInfo(0, to_replicate);
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
    return live_datanodes;
}

bool NameNode::Readlink(yfs_client::inum ino, std::string &dest) {
    return false;
}

void NameNode::GetFileInfo() {

}

void NameNode::CheckAlive() {
    for (;;) {
        for (auto &iter : datanode_info) {
            iter.second.silent += 1;
            if (iter.second.silent > 3)
                live_datanodes.remove(iter.first);
        }
        sleep(1);
    }
}

bool NameNode::InDatanodeList(const std::list<DatanodeIDProto> &list, const DatanodeIDProto &node) {
    for (auto &iter : list)
        if (SameDatanode(iter, node))
            return true;
    return false;
}

bool NameNode::SameDatanode(const DatanodeIDProto &node1, const DatanodeIDProto &node2) {
    return node1.hostname() == node2.hostname();
}
