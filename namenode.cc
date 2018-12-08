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
    fprintf(stderr, "----init\n");
    fflush(stderr);

    /* Add your init logic here */
    datanodes_id.clear();
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
    fprintf(stderr,"-----  GetBlockLocations\n");fflush(stderr);
    extent_protocol::attr attr{};
    ec->getattr(ino, attr);
    uint file_size = attr.size;
    list<blockid_t> block_ids;
    ec->get_block_ids(ino, block_ids);
    auto iter = block_ids.begin();
    list<LocatedBlock> ret;

    uint offset = 0;
    for (; iter != block_ids.end(); ++iter) {
        assert(file_size > offset);
        LocatedBlock block(*iter, offset, MIN(file_size - offset, BLOCK_SIZE), master_datanode);
        ret.push_back(block);
        offset += BLOCK_SIZE;
    }
    return ret;
}

bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) {
    fprintf(stderr,"-----  Complete\n");fflush(stderr);

    ec->complete(ino, new_size);
    lc->release(ino);
    return false;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
    fprintf(stderr,"-----  AppendBlock\n");fflush(stderr);

    extent_protocol::attr attr{};
    ec->getattr(ino, attr);
    uint file_size = attr.size;
    blockid_t bid;
    ec->append_block(ino, bid);
    return LocatedBlock(bid, file_size, 0, master_datanode);
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
    fprintf(stderr,"-----  Rename\n");fflush(stderr);

    string src_buf, dst_buf;
    ec->get(src_dir_ino, src_buf);
    ec->get(dst_dir_ino, dst_buf);

    /* remove dir entry */
    unsigned long start = src_buf.find(src_name);
    string temp = src_buf.substr(start);
    unsigned long end = temp.find('/') + start;
    temp = temp.substr(temp.find('/') + 1);
    end += temp.find('/') + 2;
    string dir_ent = src_buf.substr(start, end - start);
    src_buf.replace(start, end - start, "");
    ec->put(src_dir_ino, src_buf);

    /* add dir entry */
    dir_ent.replace(0, dir_ent.find('/'), dst_name);
    dst_buf += dir_ent;
    ec->put(dst_dir_ino, dst_buf);
    return false;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
    fprintf(stderr,"-----  Mkdir\n");fflush(stderr);

    yfs->_mkdir(parent, name.c_str(), mode, ino_out);
    return false;
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
    fprintf(stderr,"-----  Create\n");fflush(stderr);

    yfs->create(parent, name.c_str(), mode, ino_out);
    lc->acquire(ino_out);
    return false;
}

bool NameNode::Isfile(yfs_client::inum ino) {
    fprintf(stderr,"-----  Isfile\n");fflush(stderr);

    return yfs->_isfile(ino);
}

bool NameNode::Isdir(yfs_client::inum ino) {
    fprintf(stderr,"-----  Isdir\n");fflush(stderr);

    return yfs->_isdir(ino);
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &info) {
    fprintf(stderr,"-----  Getfile\n");fflush(stderr);

    yfs->_getfile(ino, info);
    return false;
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &info) {
    fprintf(stderr,"-----  Getdir\n");fflush(stderr);

    yfs->_getdir(ino, info);
    return false;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
    fprintf(stderr,"-----  Readdir\n");fflush(stderr);

    yfs->_readdir(ino, dir);
    return false;
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
    fprintf(stderr,"-----  Unlink\n");fflush(stderr);

    yfs->_unlink(parent, name.c_str());
    return false;
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
    datanodes_id.push_back(id);
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
    return datanodes_id;
}
