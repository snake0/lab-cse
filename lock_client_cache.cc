// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include "tprintf.h"


int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
        : lock_client(xdst), lu(_lu) {
    srand(static_cast<unsigned int>(time(NULL) ^ last_port));
    rlock_port = ((rand() % 32000) | (0x1 << 10));
    const char *hname;
    // VERIFY(gethostname(hname, 100) == 0);
    hname = "127.0.0.1";
    std::ostringstream host;
    host << hname << ":" << rlock_port;
    id = host.str();
    last_port = rlock_port;
    rpcs *rlsrpc = new rpcs(static_cast<unsigned int>(rlock_port));
    rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
    rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_client_cache::~lock_client_cache() {
    mutex.lock();
    lock_pool.clear();
    mutex.unlock();
}

lock_protocol::status lock_client_cache::
acquire_server(lock_protocol::lockid_t lid, lock_t &lock, cond_t *waiter) {
    int r, ret;
    lock.state = lock_t::ACQUIRING;
    while (lock.state == lock_t::ACQUIRING) {
        mutex.unlock();
        ret = cl->call(lock_protocol::acquire, lid, id, r);
        mutex.lock();
        if (ret == lock_protocol::OK) {
            // successfully receive a lock from server
            lock.state = lock_t::LOCKED;
            mutex.unlock();
            return lock_protocol::OK;
        } else if (ret == lock_protocol::RETRY) {
            // no retry rpc has been received
            if (!lock.retry_earlier)
                waiter->wait(mutex);
            lock.retry_earlier = false;
            // try again
        }
    }
    return 0;
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid) {
    mutex.lock();
    if (lock_pool.find(lid) == lock_pool.end())
        lock_pool[lid] = lock_t();
    lock_t &lock = lock_pool[lid];

    cond_t *waiter = new cond_t();
    // enqueue when client call acquire
    // dequeue when client call release

    if (lock.queue.empty()) {
        lock.queue.push(waiter);
        if (lock.state == lock_t::NONE)
            // to acquire lock from server
            return acquire_server(lid, lock, waiter);
        else if (lock.state == lock_t::FREE) {
            // get a lock from client cache
            lock.state = lock_t::LOCKED;
            mutex.unlock();
            return lock_protocol::OK;
        } else {
            // other client is acquiring or releasing the lock
            waiter->wait(mutex);
            return acquire_server(lid, lock, waiter);
        }
    } else {
        lock.queue.push(waiter);
        waiter->wait(mutex);
        if (lock.state == lock_t::NONE)
            return acquire_server(lid, lock, waiter);
        else if (lock.state == lock_t::FREE) {
            lock.state = lock_t::LOCKED;
            mutex.unlock();
            return lock_protocol::OK;
        } else if (lock.state == lock_t::LOCKED) {
            mutex.unlock();
            return lock_protocol::OK;
        } else
            return 0;
    }
}


lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
    mutex.lock();
    if (lock_pool.find(lid) == lock_pool.end()) {
        mutex.unlock();
        return lock_protocol::NOENT;
    }
    int ret = rlock_protocol::OK;
    lock_t &lock = lock_pool[lid];

    // revoke rpc has been received earlier
    if (lock.revoke_later) {
        lock.state = lock_t::RELEASING;
        mutex.unlock();
        int r;
        ret = cl->call(lock_protocol::release, lid, id, r);
        mutex.lock();
        lock.state = lock_t::NONE;
    } else
        lock.state = lock_t::FREE;

    // dequeue waiter
    delete lock.queue.front();
    lock.queue.pop();
    if (!lock.queue.empty()) {
        if (!lock.revoke_later)
            lock.state = lock_t::LOCKED;
        lock.queue.front()->signal();
    }
    // clear state
    lock.revoke_later = false;
    mutex.unlock();
    return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &) {
    int ret = rlock_protocol::OK;
    mutex.lock();
    lock_t &lock = lock_pool[lid];
    if (lock.state == lock_t::FREE) {
        // ok to release back to
        int r;
        lock.state = lock_t::RELEASING;
        mutex.unlock();
        ret = cl->call(lock_protocol::release, lid, id, r);
        mutex.lock();
        lock.state = lock_t::NONE;
        if (!lock.queue.empty())
            lock.queue.front()->signal();
    } else lock.revoke_later = true;
    mutex.unlock();
    return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &) {
    mutex.lock();
    lock_t &lock = lock_pool[lid];
    lock.retry_earlier = true;
    lock.queue.front()->signal();
    mutex.unlock();
    return rlock_protocol::OK;
}