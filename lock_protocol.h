// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
public:
    enum xxstatus {
        OK, RETRY, RPCERR, NOENT, IOERR
    };
    typedef int status;
    typedef unsigned long long lockid_t;
    enum rpc_numbers {
        acquire = 0x7001,
        release,
        stat
    };
};

class rlock_protocol {
public:
    enum xxstatus {
        OK, RPCERR
    };
    typedef int status;
    enum rpc_numbers {
        revoke = 0x8001,
        retry = 0x8002
    };
};

struct mutex_t {
    pthread_mutex_t mutex;

    mutex_t() : mutex() {
        VERIFY(pthread_mutex_init(&mutex, nullptr) == 0);
    }

    void lock() {
        VERIFY(pthread_mutex_lock(&mutex) == 0);
    }

    void unlock() {
        VERIFY(pthread_mutex_unlock(&mutex) == 0);
    }
};

struct cond_t {
    pthread_cond_t cond;

    cond_t() : cond() {
        VERIFY(pthread_cond_init(&cond, nullptr) == 0);
    }

    void wait(mutex_t &lock) {
        VERIFY(pthread_cond_wait(&cond, &lock.mutex) == 0);
    }

    void signal() {
        VERIFY(pthread_cond_signal(&cond) == 0);
    }
};


#endif 
