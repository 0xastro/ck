/*
 * Copyright 2010-2013 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CK_SPINLOCK_CLH_H
#define _CK_SPINLOCK_CLH_H

#include <ck_cc.h>
#include <ck_limits.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef CK_F_SPINLOCK_CLH
#define CK_F_SPINLOCK_CLH

struct ck_spinlock_clh {
	unsigned int wait;
	struct ck_spinlock_clh *previous;
};
typedef struct ck_spinlock_clh ck_spinlock_clh_t;

CK_CC_INLINE static void
ck_spinlock_clh_init(struct ck_spinlock_clh **lock, struct ck_spinlock_clh *unowned)
{

	ck_pr_store_ptr(&unowned->previous, NULL);
	ck_pr_store_uint(&unowned->wait, false);
	ck_pr_store_ptr(lock, unowned);
	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static bool
ck_spinlock_clh_locked(struct ck_spinlock_clh **queue)
{
	struct ck_spinlock_clh *head;

	ck_pr_fence_load();
	head = ck_pr_load_ptr(queue);
	ck_pr_fence_load();
	return ck_pr_load_uint(&head->wait);
}

CK_CC_INLINE static void
ck_spinlock_clh_lock(struct ck_spinlock_clh **queue, struct ck_spinlock_clh *thread)
{
	struct ck_spinlock_clh *previous;

	/* Indicate to the next thread on queue that they will have to block. */
	thread->wait = true;
	ck_pr_fence_store();

	/* Mark current request as last request. Save reference to previous request. */
	previous = ck_pr_fas_ptr(queue, thread);
	thread->previous = previous;

	/* Wait until previous thread is done with lock. */
	ck_pr_fence_load();
	while (ck_pr_load_uint(&previous->wait) == true)
		ck_pr_stall();

	return;
}

CK_CC_INLINE static void
ck_spinlock_clh_unlock(struct ck_spinlock_clh **thread)
{
	struct ck_spinlock_clh *previous;

	/*
	 * If there are waiters, they are spinning on the current node wait
	 * flag. The flag is cleared so that the successor may complete an
	 * acquisition. If the caller is pre-empted then the predecessor field
	 * may be updated by a successor's lock operation. In order to avoid
	 * this, save a copy of the predecessor before setting the flag.
	 */
	previous = thread[0]->previous;

	/* We have to pay this cost anyways, use it as a compiler barrier too. */
	ck_pr_fence_memory();
	ck_pr_store_uint(&(*thread)->wait, false);

	/*
	 * Predecessor is guaranteed not to be spinning on previous request,
	 * so update caller to use previous structure. This allows successor
	 * all the time in the world to successfully read updated wait flag.
	 */
	*thread = previous;
	return;
}
#endif /* CK_F_SPINLOCK_CLH */
#endif /* _CK_SPINLOCK_CLH_H */
