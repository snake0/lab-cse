#ifndef lock_server_cache_h
#define lock_server_cache_h

#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_server_cache {
private:
    struct lock_t {
        std::string owner_client;
        std::string retry_client;
        std::set<std::string> wait_clients;
        enum {
            FREE, LOCKED, REVOKING, RETRYING
        } state;

        lock_t() : state(FREE) {}

        ~lock_t() { wait_clients.clear(); }
    };

    int nacquire;
    mutex_t server_mutex;
    std::map<lock_protocol::lockid_t, lock_t> lock_pool;

    lock_protocol::status call_client(std::string cid,
                                      rlock_protocol::rpc_numbers func,
                                      lock_protocol::lockid_t lid, int &r);

public:
    lock_server_cache();

    lock_protocol::status stat(lock_protocol::lockid_t, int &);

    int acquire(lock_protocol::lockid_t, std::string id, int &);

    int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
