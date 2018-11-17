// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server() : nacquire(0), lock_mutex() {
    VERIFY(pthread_mutex_init(&lock_mutex, NULL) == 0);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r) {
    printf("stat request from clt %d\n", clt);
    r = nacquire;
    return lock_protocol::OK;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {
    pthread_mutex_lock(&lock_mutex);
    if (lock_pool.find(lid) != lock_pool.end()) {
        while (lock_pool[lid]->status == server_lock_t::LOCKED)
            pthread_cond_wait(&lock_pool[lid]->lcond, &lock_mutex);
        lock_pool[lid]->status = server_lock_t::LOCKED;
    } else {
        server_lock_t *lock;
        lock = new server_lock_t(server_lock_t::LOCKED);
        lock_pool[lid] = lock;
    }
    pthread_mutex_unlock(&lock_mutex);
    return lock_protocol::OK;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r) {
    pthread_mutex_lock(&lock_mutex);
    if (lock_pool.find(lid) == lock_pool.end()) {
        pthread_mutex_unlock(&lock_mutex);
        return lock_protocol::NOENT;
    } else {
        lock_pool[lid]->status = server_lock_t::FREE;
        pthread_cond_signal(&lock_pool[lid]->lcond);
        pthread_mutex_unlock(&lock_mutex);
        return lock_protocol::OK;
    }
}
