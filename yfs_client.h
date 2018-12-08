#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_client_cache.h"
#include "extent_client.h"
#include <vector>

#define ERR(msg) {printf("%s error\n",msg);return r;}


class yfs_client {
    extent_client *ec;
    lock_client_cache *lc;
public:

    typedef unsigned long long inum;
    enum xxstatus {
        OK, RPCERR, NOENT, IOERR, EXIST
    };
    typedef int status;

    struct fileinfo {
        unsigned long long size;
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };

    struct dirinfo {
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };

    struct dirent {
        std::string name;
        yfs_client::inum inum;
    };

    yfs_client(std::string extent_dst, std::string lock_dst) {
        ec = new extent_client(extent_dst);
        lc = new lock_client_cache(lock_dst);
        if (ec->put(1, "") != extent_protocol::OK)
            printf("error init root dir\n"); // XYB: init root dir
    }

    ~yfs_client() {
        delete ec;
        delete lc;
    }

    static std::string filename(inum);
    static inum n2i(std::string);
    bool _isfile(inum);
    bool _isdir(inum);
    int _getfile(inum, fileinfo &);
    int _getdir(inum, dirinfo &);
    int _setattr(inum, size_t);
    int _lookup(inum, const char *, bool &, inum &);
    int _create(inum, const char *, mode_t, inum &);
    int _readdir(inum, std::list<dirent> &);
    int _write(inum, size_t, off_t, const char *, size_t &);
    int _read(inum, size_t, off_t, std::string &);
    int _unlink(inum, const char *);
    int _mkdir(inum, const char *, mode_t, inum &);
    int _symlink(inum, const char *, const char *, inum &);
    int _readlink(inum, std::string &);

    bool isfile(inum);
    bool isdir(inum);
    int getfile(inum, fileinfo &);
    int getdir(inum, dirinfo &);
    int setattr(inum, size_t);
    int lookup(inum, const char *, bool &, inum &);
    int create(inum, const char *, mode_t, inum &);
    int readdir(inum, std::list<dirent> &);
    int write(inum, size_t, off_t, const char *, size_t &);
    int read(inum, size_t, off_t, std::string &);
    int unlink(inum, const char *);
    int mkdir(inum, const char *, mode_t, inum &);
    int symlink(inum, const char *, const char *, inum &);
    int readlink(inum, std::string &);
};

#endif
