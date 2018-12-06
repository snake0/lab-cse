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
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
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
    ec->complete(ino, new_size);
    lc->release(ino);
    return false;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
    extent_protocol::attr attr{};
    ec->getattr(ino, attr);
    uint file_size = attr.size;

    blockid_t bid;
    ec->append_block(ino, bid);

    return LocatedBlock(bid, file_size, 0, master_datanode);
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
    
    return false;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
    return false;
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
    return false;
}

bool NameNode::Isfile(yfs_client::inum ino) {
    return false;
}

bool NameNode::Isdir(yfs_client::inum ino) {
    return false;
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &info) {
    return false;
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &info) {
    return false;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
    return false;
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
    return false;
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
}

list<DatanodeIDProto> NameNode::GetDatanodes() {
    return list<DatanodeIDProto>();
}
