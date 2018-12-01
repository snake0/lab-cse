// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <pthread.h>
#include <map>
#include <list>
#include <queue>

// Classes that inherit lock_release_user can override dorelease so that
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
public:
    virtual void dorelease(lock_protocol::lockid_t) = 0;

    virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
private:
    class lock_release_user *lu;

    struct lock_t {
        bool revoke_later;
        bool retry_earlier;
        enum {
            NONE, ACQUIRING, FREE, RELEASING, LOCKED
        } state;
        std::queue<cond_t *> queue;

        lock_t() : revoke_later(false),
                   retry_earlier(false), state(NONE) {}
    };

    int rlock_port;
    std::string hostname;
    std::string id;
    mutex_t mutex;
    std::map<lock_protocol::lockid_t, lock_t> lock_pool;

    lock_protocol::status acquire_server(lock_protocol::lockid_t, lock_t &, cond_t *);

public:
    static int last_port;

    explicit lock_client_cache(std::string xdst, class lock_release_user *l = 0);

    virtual ~lock_client_cache();

    lock_protocol::status acquire(lock_protocol::lockid_t);

    lock_protocol::status release(lock_protocol::lockid_t);

    rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);

    rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);
};


#endif
