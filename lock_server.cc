// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server() : nacquire(0), lock_mutex() {}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
    printf("stat request from clt %d\n", clt);
    r = nacquire;
    return lock_protocol::OK;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {
    lock_mutex.lock();
    if (lock_pool.find(lid) != lock_pool.end()) {
        while (lock_pool[lid].state == lock_t::LOCKED)
            lock_pool[lid].lcond.wait(lock_mutex);
        lock_pool[lid].state = lock_t::LOCKED;
    } else {
        lock_t lock;
        lock_pool[lid] = lock;
    }
    lock_mutex.unlock();
    return lock_protocol::OK;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
    lock_mutex.lock();
    if (lock_pool.find(lid) == lock_pool.end()) {
        lock_mutex.unlock();
        return lock_protocol::NOENT;
    } else {
        lock_pool[lid].state = lock_t::FREE;
        lock_pool[lid].lcond.signal();
        lock_mutex.unlock();
        return lock_protocol::OK;
    }
}
