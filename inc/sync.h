/**
 * libsk syncronization primitives
 */

#include "errors.h"
#include <stdbool.h>
#include <stdint.h>


/** 
 * Type declaration for lock primitives.
 * 0 means unlocked, 1 means locked -- to create locks unlocked by default
 *
 * Locks are 8-bit variables, access to which should always be atomic.
 * Under hood, atomic access is achieved using STREXB and LDREXB instructions.
 * For user, the higher-level lock methods are implemented.
 */
typedef uint8_t sk_lock_t;

#define __SK_LOCK_UNLOCKED ((sk_lock_t)0)
#define __SK_LOCK_LOCKED   ((sk_lock_t)(! (__SK_LOCK_UNLOCKED)))
// Mimic linux/spinlock.h Kernel interface DEFINE_SPINLOCK()
#define SK_LOCK_DECLARE(name) sk_lock_t name = __SK_LOCK_UNLOCKED


/**
 * Unlock a lock
 * @lock: pointer to :c:type:`sk_lock_t` object
 *
 * Note: Unlock does not control who captured the lock and whether is was captured at all,
 *       it simply unlocks
 */
void sk_lock_unlock(sk_lock_t *lock);


/**
 * Try to capture a lock (non-blocking)
 * @lock: pointer to :c:type:`sk_lock_t` object
 * @return: `true` if lock was succesfully captured, `false` otherwise
 *
 * This call IS NOT blocking. It instantly returns status of lock capture procedure
 */
bool sk_lock_trylock(sk_lock_t *lock);


/**
 * Spin trying to capture a lock (blocking)
 * @lock: pointer to :c:type:`sk_lock_t` object
 *
 * This call IS blocking. It will return only after the lock was captured.
 * Thus deadlocks have more chances to occur
 */
void sk_lock_spinlock(sk_lock_t *lock);


// BFIFO-related

typedef uint16_t sk_bfifo_len_t;

struct sk_bfifo {
	uint8_t *buf;
	sk_bfifo_len_t buflen;
	sk_bfifo_len_t rdidx;
	sk_bfifo_len_t wridx;
	uint8_t isfull : 1;
};

typedef struct sk_bfifo sk_bfifo_t;



#define SK_BFIFO_INITIALIZER(_buf, _buflen) (\
	(sk_bfifo_t) {					  		 \
		.buf = (_buf),				  		 \
		.buflen = (_buflen),		  		 \
		.rdidx = 0u,				  		 \
		.wridx = 0u,				  		 \
		.isfull = false				  		 \
	})


/**
 * Statically declare and initialize bfifo
 * @name: name under which :c:type:`sk_bfifo_t` fifo object will be available
 * @buf: buffer used for bfifo
 * @buflen: length of the passed `buf` buffer
 */
#define SK_BFIFO_DECLARE(name, buf, buflen) sk_bfifo_t name = SK_BFIFO_INITIALIZER(buf, buflen)


/**
 * Dynamically initialize bfifo
 * @buf: buffer used for bfifo
 * @buflen: length of the passed `buf` buffer
 */
sk_err sk_bfifo_init(sk_bfifo_t *fifo, uint8_t *buf, sk_bfifo_len_t buflen);


sk_err sk_bfifo_put(sk_bfifo_t *fifo, uint8_t *barr, uint32_t len);

int32_t sk_bfifo_get(sk_bfifo_t *fifo, uint8_t *barr, uint32_t len);
