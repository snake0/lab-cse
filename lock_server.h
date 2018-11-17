// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <set>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"


class server_lock_t {
public:
    enum lstatus {
        FREE, LOCKED
    } status;
    pthread_cond_t lcond;

    server_lock_t() : status(server_lock_t::FREE), lcond() {
        VERIFY(pthread_cond_init(&lcond, NULL) == 0);
    }

    explicit server_lock_t(server_lock_t::lstatus status) : status(status), lcond() {
        VERIFY(pthread_cond_init(&lcond, NULL) == 0);
    }

    ~server_lock_t() {}
};

class lock_server {

protected:
    int nacquire;
    pthread_mutex_t lock_mutex;
    std::map<lock_protocol::lockid_t, server_lock_t *> lock_pool;
public:
    lock_server();

    ~lock_server() {};

    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);

    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







