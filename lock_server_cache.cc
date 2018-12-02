// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache() :
        nacquire(0), server_mutex() {}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &) {
    int r = 0, ret = lock_protocol::OK;
    server_mutex.lock();
    if (lock_pool.find(lid) == lock_pool.end())
        lock_pool[lid] = lock_t();
    lock_t &lock = lock_pool[lid];
    bool need_revoke = false;
    switch (lock.state) {
        case lock_t::FREE:
            VERIFY(lock.wait_clients.empty());
            lock.state = lock_t::LOCKED;
            lock.owner_client = id;
            break;
        case lock_t::LOCKED:
            VERIFY(lock.wait_clients.empty());
            VERIFY(!(lock.owner_client.empty() || id.empty() || lock.owner_client == id));
            lock.state = lock_t::REVOKING;
            lock.wait_clients.insert(id);
            need_revoke = true;
            ret = lock_protocol::RETRY;
            break;
        case lock_t::REVOKING:
            VERIFY(!lock.wait_clients.empty());
            VERIFY(!(lock.owner_client.empty() || id.empty() || lock.owner_client == id));
            lock.wait_clients.insert(id);
            ret = lock_protocol::RETRY;
            break;
        case lock_t::RETRYING:
            if (lock.retry_client == id) {
                lock.retry_client = std::string();
                lock.owner_client = id;
                if (!lock.wait_clients.empty()) {
                    lock.state = lock_t::REVOKING;
                    need_revoke = true;
                } else
                    lock.state = lock_t::LOCKED;
            } else {
                lock.wait_clients.insert(id);
                ret = lock_protocol::RETRY;
            }
    }
    server_mutex.unlock();
    if (need_revoke)
        call_client(lock.owner_client, rlock_protocol::revoke, lid, r);
    return ret;
}

int
lock_server_cache::release(lock_protocol::lockid_t lid,
                           std::string id, int &r) {
    server_mutex.lock();
    lock_protocol::status ret = lock_protocol::OK;
    if (lock_pool.find(lid) == lock_pool.end()) {
        server_mutex.unlock();
        return lock_protocol::NOENT;
    }
    lock_t &lock = lock_pool[lid];
    VERIFY(lock.state == lock_t::REVOKING);
    VERIFY(id == lock.owner_client);
    lock.state = lock_t::RETRYING;
    lock.retry_client = *lock.wait_clients.begin();
    lock.wait_clients.erase(lock.retry_client);
    lock.owner_client = std::string();
    server_mutex.unlock();
    call_client(lock.retry_client, rlock_protocol::retry, lid, r);
    return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r) {
    tprintf("stat request\n");
    r = nacquire;
    return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::call_client(std::string cid,
                               rlock_protocol::rpc_numbers func,
                               lock_protocol::lockid_t lid, int &r) {
    handle h(cid);
    lock_protocol::status ret;
    if (h.safebind()) {
        ret = h.safebind()->call(func, lid, r);
        return ret;
    } else return lock_protocol::RPCERR;
}

