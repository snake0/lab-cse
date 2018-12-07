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
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

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
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a{};
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

    release:

    return r;
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
    int r = OK;
    printf("getdir %016llx\n", inum);
    extent_protocol::attr a{};
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
    release:
    return r;
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
    if ((r = ec->get(ino, buf)) != OK) ERR("setattr error");
    buf.resize(size);
    if ((r = ec->put(ino, buf)) != OK) ERR("setattr error");

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
    r = _lookup(parent, name, found, ino_out);
    if (found) ERR("create");
    ec->create(extent_protocol::T_FILE, ino_out);
    std::string buf, ent = std::string(name) + '/' + filename(ino_out) + '/';
    if ((r = ec->get(parent, buf)) != OK) ERR("create");
    buf += ent;
    if ((r = ec->put(parent, buf)) != OK) ERR("create");

    return r;
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

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    int r;
    bool found = false;
    r = _lookup(parent, name, found, ino_out);
    if (found) ERR("mkdir");
    ec->create(extent_protocol::T_DIR, ino_out);
    std::string buf, ent = std::string(name) + '/' + filename(ino_out) + '/';
    if ((r = ec->get(parent, buf)) != OK) ERR("mkdir");
    buf += ent;
    if ((r = ec->put(parent, buf)) != OK) ERR("mkdir");
    return r;
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

    found = false;
    if (!_isdir(parent))
        return IOERR;
    std::list<dirent> ents;
    if ((r = _readdir(parent, ents)) != OK) ERR("lookup");
    auto iterator = ents.begin();
    while (iterator != ents.end()) {
        if (iterator->name == std::string(name)) {
            found = true;
            ino_out = iterator->inum;
            return r;
        }
        ++iterator;
    }
    return r;
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
    if ((r = ec->get(dir, buf)) != OK) ERR("readdir");
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
    return r;
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
    if ((r = ec->get(ino, buf)) != OK) ERR("read");
    if ((int) buf.size() > off)
        data = buf.substr((uint) off, size);
    else data.resize(0);
    return r;
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
    std::string buf;
    if ((r = ec->get(ino, buf)) != OK) ERR("write");
    std::string towrite = std::string(data, size);
    bytes_written = size;
    if (off > (off_t) buf.size())
        buf.resize((unsigned long) off);
    buf.replace((unsigned long) off, size, towrite);
    if ((r = ec->put(ino, buf)) != OK) ERR("write");
    return r;
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
    if (!found) {
        r = NOENT;
        ERR("unlink");
    }
    ec->remove(toremove);
    std::string buf, ent = std::string(name) + '/' + filename(toremove) + '/';
    if ((r = ec->get(parent, buf)) != OK) ERR("unlink");
    buf.replace(buf.find(ent), ent.size(), "");
    if ((r = ec->put(parent, buf)) != OK) ERR("unlink\n");
    return r;
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
    if (found) {
        r = EXIST;
        ERR("symlink");
    }
    std::string towrite = std::string(link);
    ec->create(extent_protocol::T_SYMLINK, ino_out);
    ec->put(ino_out, towrite);
    std::string buf, ent = std::string(name) + '/' + filename(ino_out) + '/';
    if ((r = ec->get(parent, buf)) != OK) ERR("symlink");
    buf += ent;
    if ((r = ec->put(parent, buf)) != OK) ERR("symlink");
    return r;
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
    if ((r = ec->get(ino, link)) != OK) ERR("readlink");
    return r;
}
