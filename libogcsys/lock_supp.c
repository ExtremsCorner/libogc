#include <config.h>
#include <_ansi.h>
#include <_syslist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef REENTRANT_SYSCALLS_PROVIDED
#include <reent.h>
#endif
#include <errno.h>
#undef errno
extern int errno;

#include "asm.h"
#include "processor.h"
#include "lwp_wkspace.h"
#include "lwp_mutex.h"

typedef struct _lock {
	int id;
	lwp_mutex lck;
} lock_t;

static int _g_libc_lck_id = 0;

static u32 __libc_lock_supp(lock_t *lock,u32 timeout,u8 block)
{
	u32 level;
	_CPU_ISR_Disable(level);
	__lwp_mutex_seize(&lock->lck,lock->id,block,timeout,level);
	return _thr_executing->wait.ret_code;
}

void __libc_lock_init(int *lock,boolean recursive)
{
	lwp_mutex_attr attr;
	lock_t *retlck = NULL;

	if(!lock) return;

	__lwp_thread_dispatchdisable();
	retlck = (lock_t*)__lwp_wkspace_allocate(sizeof(lock_t));
	if(!retlck) {
		__lwp_thread_dispatchenable();
		return;
	}

	attr.mode = LWP_MUTEX_FIFO;
	attr.nest_behavior = recursive?LWP_MUTEX_NEST_ACQUIRE:LWP_MUTEX_NEST_ERROR;
	attr.onlyownerrelease = TRUE;
	attr.prioceil = 1; //__lwp_priotocore(LWP_PRIO_MAX-1);
	__lwp_mutex_initialize(&retlck->lck,&attr,LWP_MUTEX_UNLOCKED);

	retlck->id = ++_g_libc_lck_id;
	*lock = (int)retlck;
	__lwp_thread_dispatchenable();
}

void __libc_lock_close(int *lock)
{
	lock_t *plock = (lock_t*)*lock;
	
	__lwp_thread_dispatchdisable();
	if(__lwp_mutex_locked(&plock->lck)) {
		__lwp_thread_dispatchenable();
		return;
	}
	
	__lwp_mutex_flush(&plock->lck,EINVAL);
	__lwp_wkspace_free(plock);

	__lwp_thread_dispatchenable();
}

void __libc_lock_acquire(int *lock)
{
	lock_t *plock = (lock_t*)*lock;
	__libc_lock_supp(plock,LWP_THREADQ_NOTIMEOUT,TRUE);
}

void __libc_lock_try_acquire(int *lock)
{
	lock_t *plock = (lock_t*)*lock;
	__libc_lock_supp(plock,LWP_THREADQ_NOTIMEOUT,FALSE);
}

void __libc_lock_release(int *lock)
{
	lock_t *plock = (lock_t*)*lock;
	__lwp_thread_dispatchdisable();
	__lwp_mutex_surrender(&plock->lck);
	__lwp_wkspace_free(plock);
	__lwp_thread_dispatchenable();
}
