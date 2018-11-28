// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <set>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_server {
private:
    struct lock_t {
    public:
        enum {
            FREE, LOCKED
        } state;
        cond_t lcond;

        explicit lock_t() : state(FREE), lcond() {}

        ~lock_t() { state = FREE; }
    };

protected:
    int nacquire;
    mutex_t lock_mutex;
    std::map<lock_protocol::lockid_t, lock_t> lock_pool;
public:
    lock_server();

    ~lock_server() { lock_pool.clear(); };

    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);

    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);

    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







