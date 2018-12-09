#include <utility>

// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::inum
yfs_client::n2i(std::string n) {
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum) {
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum) {
    lc->acquire(inum);
    bool ret = _isfile(inum);
    lc->release(inum);
    return ret;
}

bool
yfs_client::_isfile(inum inum) {
    extent_protocol::attr a{};
    CHECK(ec->getattr(inum, a), "yfs: _isfile getattr", false);

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is not a file\n", inum);
    return false;
}

/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */
bool
yfs_client::isdir(inum inum) {
    lc->acquire(inum);
    bool ret = _isdir(inum);
    lc->release(inum);
    return ret;
}

bool
yfs_client::_isdir(inum inum) {
    // Oops! is this still correct when you implement symlink?

    extent_protocol::attr a{};
    CHECK(ec->getattr(inum, a), "yfs: _isdir getattr", false);

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin) {
    lc->acquire(inum);
    int ret = _getfile(inum, fin);
    lc->release(inum);
    return ret;
}

int
yfs_client::_getfile(inum inum, fileinfo &fin) {
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a{};
    CHECK(ec->getattr(inum, a), "yfs: _getfile getattr", IOERR);

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

    return OK;
}

int
yfs_client::getdir(inum inum, dirinfo &din) {
    lc->acquire(inum);
    int ret = _getdir(inum, din);
    lc->release(inum);
    return ret;
}

int
yfs_client::_getdir(inum inum, dirinfo &din) {
    printf("getdir %016llx\n", inum);

    extent_protocol::attr a{};
    CHECK(ec->getattr(inum, a), "yfs: _getdir getattr", IOERR);

    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
    return OK;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)


// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size) {
    lc->acquire(ino);
    int ret = _setattr(ino, size);
    lc->release(ino);
    return ret;
}

int
yfs_client::_setattr(inum ino, size_t size) {

    int r;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string buf;
    CHECK(r = ec->get(ino, buf), "yfs: _setattr get", r);

    buf.resize(size);
    CHECK(r = ec->put(ino, buf), "yfs: _setattr put", r);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out) {
    lc->acquire(parent);
    int ret = _create(parent, name, mode, ino_out);
    lc->release(parent);
    return ret;
}

int
yfs_client::_create(inum parent, const char *name, mode_t mode, inum &ino_out) {
    int r;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent info.
     */

    bool found = false;
    _lookup(parent, name, found, ino_out);
    CHECK (found, "yfs: _create found in parent", EXIST);
    CHECK(r = ec->create(extent_protocol::T_FILE, ino_out), "yfs: _create create", r);

    std::string buf;
    CHECK(r = ec->get(parent, buf), "yfs: _create get", r);

    std::string ent = std::string(name) + '/' + filename(ino_out) + '/';
    CHECK(r = ec->put(parent, buf + ent), "yfs: _create put", r);
    return OK;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out) {
    lc->acquire(parent);
    int ret = _mkdir(parent, name, mode, ino_out);
    lc->release(parent);
    return ret;
}

int

yfs_client::_mkdir(inum parent, const char *name, mode_t mode, inum &ino_out) {
    int r;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool found = false;
    _lookup(parent, name, found, ino_out);
    CHECK(found, "yfs: _mkdir found in parent", EXIST);

    CHECK(r = ec->create(extent_protocol::T_DIR, ino_out), "yfs: _mkdir create", r);

    std::string buf;
    CHECK(r = ec->get(parent, buf), "yfs: _mkdir get", r);

    std::string ent = std::string(name) + '/' + filename(ino_out) + '/';
    CHECK(r = ec->put(parent, buf + ent), "yfs: _mkdir put", r);
    return OK;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out) {
    lc->acquire(parent);
    int ret = _lookup(parent, name, found, ino_out);
    lc->release(parent);
    return ret;
}

int
yfs_client::_lookup(inum parent, const char *name, bool &found, inum &ino_out) {
    int r;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

    CHECK(!_isdir(parent), "yfs: _lookup in non-dir", IOERR);

    found = false;
    std::list<dirent> ents;
    CHECK(r = _readdir(parent, ents), "yfs: _lookup readdir", r);
    auto iter = ents.begin();
    for (; iter != ents.end(); ++iter)
        if (std::string(name) == iter->name) {
            found = true;
            ino_out = iter->inum;
            return OK;
        }
    return NOENT;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list) {
    lc->acquire(dir);
    int ret = _readdir(dir, list);
    lc->release(dir);
    return ret;
}

int
yfs_client::_readdir(inum dir, std::list<dirent> &list) {
    int r;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    std::string buf;
    CHECK(r = ec->get(dir, buf), "yfs: _readdir get", r);

    struct dirent ent;
    unsigned long pos;
    while (!buf.empty()) {
        pos = buf.find('/');
        ent.name = buf.substr(0, pos);
        buf = buf.substr(pos + 1);
        pos = buf.find('/');
        ent.inum = n2i(buf.substr(0, pos));
        buf = buf.substr(pos + 1);
        list.push_back(ent);
    }
    return OK;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data) {
    lc->acquire(ino);
    int ret = _read(ino, size, off, data);
    lc->release(ino);
    return ret;
}

int
yfs_client::_read(inum ino, size_t size, off_t off, std::string &data) {

    int r;

    /*
     * your code goes here.
     * note: read using ec->get().
     */

    std::string buf;
    CHECK(r = ec->get(ino, buf), "yfs: _read get", r);
    data = (off_t) buf.size() > off ?
           buf.substr((uint) off, size) : "";
    return OK;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data, size_t &bytes_written) {
    lc->acquire(ino);
    int ret = _write(ino, size, off, data, bytes_written);
    lc->release(ino);
    return ret;
}

int
yfs_client::_write(inum ino, size_t size, off_t off, const char *data,
                   size_t &bytes_written) {

    int r;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    std::string content;
    CHECK(r = ec->get(ino, content), "yfs: _write get", r);

    std::string buf;
    buf.assign(data, size);
    if ((unsigned int) off <= content.size()) {
        content.replace(static_cast<unsigned long>(off), size, buf);
        bytes_written = size;
    } else {
        size_t old_size = content.size();
        content.resize(size + off, '\0');
        content.replace(static_cast<unsigned long>(off), size, buf);
        bytes_written = size + off - old_size;
    }

    CHECK(r = ec->put(ino, content), "yfs: _write get", r);
    return OK;
}

int yfs_client::unlink(inum parent, const char *name) {
    lc->acquire(parent);
    int ret = _unlink(parent, name);
    lc->release(parent);
    return ret;
}

int yfs_client::_unlink(inum parent, const char *name) {

    int r;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    bool found = false;
    inum toremove;

    r = _lookup(parent, name, found, toremove);
    CHECK(!found, "yfs: _unlink non-exist file", NOENT);
    CHECK(r = ec->remove(toremove), "yfs: _unlink remove", r);

    std::string buf;
    CHECK(r = ec->get(parent, buf), "yfs: _unlink get", r);

    std::string ent = std::string(name) + '/' + filename(toremove) + '/';
    buf.replace(buf.find(ent), ent.size(), "");
    CHECK(r = ec->put(parent, buf), "yfs: _unlink put", r);

    return OK;
}

int
yfs_client::symlink(inum parent, const char *link, const char *name, inum &ino_out) {
    lc->acquire(parent);
    int ret = _symlink(parent, link, name, ino_out);
    lc->release(parent);
    return ret;
}

int yfs_client::_symlink(inum parent, const char *name, const char *link, inum &ino_out) {
    int r;

    bool found = false;
    _lookup(parent, name, found, ino_out);
    CHECK(found, "yfs: _symlink exist file", EXIST);

    CHECK(r = ec->create(extent_protocol::T_SYMLINK, ino_out), "yfs: _symlink create", r);
    std::string towrite = std::string(link);
    ec->put(ino_out, towrite);

    std::string buf;
    CHECK(r = ec->get(parent, buf), "yfs: _symlink get", r);

    std::string ent = std::string(name) + '/' + filename(ino_out) + '/';
    CHECK(r = ec->put(parent, buf + ent), "yfs: _symlink put", r);
    return OK;
}

int
yfs_client::readlink(inum ino, std::string &path) {
    lc->acquire(ino);
    int ret = _readlink(ino, path);
    lc->release(ino);
    return ret;
}

int
yfs_client::_readlink(inum ino, std::string &link) {
    int r;
    CHECK(r = ec->get(ino, link), "yfs: _readlink get", r);
    return OK;
}
