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

class mutex_t {
public:
    pthread_mutex_t mutex;

    mutex_t() : mutex() {
        VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
    }

    void lock() {
        VERIFY(pthread_mutex_lock(&mutex) == 0);
    }

    void unlock() {
        VERIFY(pthread_mutex_unlock(&mutex) == 0);
    }
};


class cond_t {
private:
    pthread_cond_t cv;
public:
    cond_t() : cv() {
        VERIFY(pthread_cond_init(&cv, NULL) == 0);
    }

    void wait(mutex_t &lock) {
        VERIFY(pthread_cond_wait(&cv, &lock.mutex) == 0);
    }

    void signal() {
        VERIFY(pthread_cond_signal(&cv) == 0);
    }
};


#endif 
