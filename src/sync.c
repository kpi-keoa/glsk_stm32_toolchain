/**
 * libsk syncronization primitives
 */

#include "sync.h"
#include "intrinsics.h"
#include <stdbool.h>
#include <stddef.h>


// Follow guidelines from ARM_Cortex-M_Programming_Guide_to_Memory_Barrier_Instructions_(AN321)
// p. 49 (4.19 Semaphores and Mutual Exclusives (Mutex) - unicore and multicore)
// and p. 34 Programming Manual (2.2.7 Synchronization primitives)

// Unlock a lock. It's simple as we don't check who acquired it -- simply write 0
void sk_lock_unlock(sk_lock_t *lock)
{
	__DMB();	// ensure all previous memory transfers completed before next writes
	*lock = __SK_LOCK_UNLOCKED;
}


// Try to acquire lock atomically. Returns true if lock acquired, false otherwise
bool sk_lock_trylock(sk_lock_t *lock)
{
	// Load 8 bit value from memory pointed to.
	// And mark corresponding memory block for the following exclusive write with STREX
	sk_lock_t lockval = __LDREXB(lock);		// see p. 79 PM (3.4.8 LDREX and STREX instructions)
	if (__SK_LOCK_UNLOCKED != lockval) {
		// lock is captured by someone else

		// Maybe this CLREX could be omitted to get better performance, but side-effects are
        // unknown. Here we're following ARM mbed implementation from  mbed-util/atomic_ops.h
		__CLREX(); // Remove exclusive access mark
		return false;
	}
	// Try to set 8 bit memory pointed by lock to unlocked value
	// Result will be 0 on success and 1 on failure
	if (0 != __STREXB(__SK_LOCK_LOCKED, lock)) {
		// someone else set this value while we were processing it
		return false;
	}

	// Successfully captured the lock.
	__DMB();	// Ensure all previous writes have completed before continuing other writes
	return true;
}


// Busy-wait (loop) trying to capture a lock
void sk_lock_spinlock(sk_lock_t *lock)
{
	// Simple, but not very efficient implementation
	while (!sk_lock_trylock(lock));
	return;
}


// BFIFO-related

static inline sk_bfifo_len_t _sk_bfifo_numleft(sk_bfifo_t *fifo)
{
	// Nleft = buflen - USED, where
	// USED = wr - rd,   if wr > rd
	// USED = buflen - (rd - wr) = buflen + (wr - rd),   if wr < rd
	// which gives us:
	// (wr - rd) = D
	// Nleft = buflen - D,   if D > 0
	// Nleft = buflen - (buflen + D) = -D,    if D < 0
	// which is the same as:
	// Nleft = (buflen - D) % buflen		(prove this)
	sk_bfifo_len_t left = ((fifo->buflen - fifo->wridx) + fifo->rdidx) % fifo->buflen;
	// when wrptr == rdptr, we have either full or empty
	// formula above gives 0 left, so fix this manually
	if (0 == left)
		return (fifo->isfull) ? 0 : fifo->buflen;
	
	return left;
}


// Dynamically initialize bfifo
sk_err sk_bfifo_init(sk_bfifo_t *fifo, uint8_t *buf, sk_bfifo_len_t buflen)
{
	if ((NULL == fifo) || (NULL == buf) || (0 == buflen))
		return SK_EWRONGARG;

	SK_BFIFO_DECLARE(tmp, buf, buflen);
	*fifo = tmp;	// copy
	return SK_EOK;
}


// Put N bytes into BFIFO
sk_err sk_bfifo_put(sk_bfifo_t *fifo, uint8_t *barr, uint32_t len)
{
	if ((NULL == fifo) || (NULL == barr))
		return SK_EUNKNOWN;

	// item can not be put at all -- it is a different kind of error
	if (len > fifo->buflen)
		return SK_ERANGE;

	sk_bfifo_len_t left = _sk_bfifo_numleft(fifo);

	// Our buffer is all-or-nothing. We either put array in it, or return error. No partial writes
	if (left < len)
		return SK_EFULL;

	// Now we know there's enough space for elements we're trying to put. Do it

	// first put elements, only then update index
	// this (hopefully) will allow for lock-free use in one producer - one consumer case
	for (sk_bfifo_len_t i = 0; i < len; i++) {
		fifo->buf[(fifo->wridx + i) % fifo->buflen] = barr[i];
	}

	sk_bfifo_len_t newwr = (fifo->wridx + len) % fifo->buflen;
	if (newwr == fifo->rdidx)
		fifo->isfull = true;

	// memory barrier here

	// update index
	fifo->wridx = newwr;
	return SK_EOK;
}


// Try getting up to N bytes from BFIFO
int32_t sk_bfifo_get(sk_bfifo_t *fifo, uint8_t *barr, uint32_t len)
{
	if ((NULL == fifo) || (NULL == barr))
		return SK_EUNKNOWN;

	// trying to read 0 elements -- return 0 read to be consistent with api users
	if (0 == len)
		return 0;

	sk_bfifo_len_t used = fifo->buflen - _sk_bfifo_numleft(fifo);

	if (0 == used)
		return SK_EEMPTY;		// can not read from empty buffer

	// truncate length as we're returning number of elements actually read -- to simplify algo
	len = (len > used) ? used : len;

	// perform actual read
	for (sk_bfifo_len_t i = 0; i < len; i++) {
		barr[i] = fifo->buf[(fifo->rdidx + i) % fifo->buflen];
	}

	// now update the pointer
	fifo->rdidx = (fifo->rdidx + len) % fifo->buflen;

	// if we came here -- it means we have successfully read something
	// so buffer can not be full -- remove flag
	fifo->isfull = false;

	return len;		// number of elements actually read
}


