/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_string_fns.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_atomic.h>

#include "opdl_ring.h"
#include "opdl_log.h"

#define LIB_NAME "opdl_ring"

#define OPDL_NAME_SIZE 64


#define OPDL_EVENT_MASK  (0x00000000000FFFFFULL)
#define OPDL_FLOWID_MASK (0xFFFFF)
#define OPDL_OPA_MASK    (0xFF)
#define OPDL_OPA_OFFSET  (0x38)

/* Types of dependency between stages */
enum dep_type {
	DEP_NONE = 0,  /* no dependency */
	DEP_DIRECT,  /* stage has direct dependency */
	DEP_INDIRECT,  /* in-direct dependency through other stage(s) */
	DEP_SELF,  /* stage dependency on itself, used to detect loops */
};

/* Shared section of stage state.
 * Care is needed when accessing and the layout is important, especially to
 * limit the adjacent cache-line HW prefetcher from impacting performance.
 */
struct shared_state {
	/* Last known minimum sequence number of dependencies, used for multi
	 * thread operation
	 */
	uint32_t available_seq;
	char _pad1[RTE_CACHE_LINE_SIZE * 3];
	uint32_t head;  /* Head sequence number (for multi thread operation) */
	char _pad2[RTE_CACHE_LINE_SIZE * 3];
	struct opdl_stage *stage;  /* back pointer */
	uint32_t tail;  /* Tail sequence number */
	char _pad3[RTE_CACHE_LINE_SIZE * 2];
} __rte_cache_aligned;

/* A structure to keep track of "unfinished" claims. This is only used for
 * stages that are threadsafe. Each lcore accesses its own instance of this
 * structure to record the entries it has claimed. This allows one lcore to make
 * multiple claims without being blocked by another. When disclaiming it moves
 * forward the shared tail when the shared tail matches the tail value recorded
 * here.
 */
struct claim_manager {
	uint32_t num_to_disclaim;
	uint32_t num_claimed;
	uint32_t mgr_head;
	uint32_t mgr_tail;
	struct {
		uint32_t head;
		uint32_t tail;
	} claims[OPDL_DISCLAIMS_PER_LCORE];
} __rte_cache_aligned;

/* Context for each stage of opdl_ring.
 * Calculations on sequence numbers need to be done with other uint32_t values
 * so that results are modulus 2^32, and not undefined.
 */
struct opdl_stage {
	struct opdl_ring *t;  /* back pointer, set at init */
	uint32_t num_slots;  /* Number of slots for entries, set at init */
	uint32_t index;  /* ID for this stage, set at init */
	bool threadsafe;  /* Set to 1 if this stage supports threadsafe use */
	/* Last known min seq number of dependencies for used for single thread
	 * operation
	 */
	uint32_t available_seq;
	uint32_t head;  /* Current head for single-thread operation */
	uint32_t nb_instance;  /* Number of instances */
	uint32_t instance_id;  /* ID of this stage instance */
	uint16_t num_claimed;  /* Number of slots claimed */
	uint16_t num_event;		/* Number of events */
	uint32_t seq;			/* sequence number  */
	uint32_t num_deps;  /* Number of direct dependencies */
	/* Keep track of all dependencies, used during init only */
	enum dep_type *dep_tracking;
	/* Direct dependencies of this stage */
	struct shared_state **deps;
	/* Other stages read this! */
	struct shared_state shared __rte_cache_aligned;
	/* For managing disclaims in multi-threaded processing stages */
	struct claim_manager pending_disclaims[RTE_MAX_LCORE]
					       __rte_cache_aligned;
	uint32_t shadow_head;  /* Shadow head for single-thread operation */
	uint32_t queue_id;     /* ID of Queue which is assigned to this stage */
	uint32_t pos;		/* Atomic scan position */
} __rte_cache_aligned;

/* Context for opdl_ring */
struct opdl_ring {
	char name[OPDL_NAME_SIZE];  /* OPDL queue instance name */
	int socket;  /* NUMA socket that memory is allocated on */
	uint32_t num_slots;  /* Number of slots for entries */
	uint32_t mask;  /* Mask for sequence numbers (num_slots - 1) */
	uint32_t slot_size;  /* Size of each slot in bytes */
	uint32_t num_stages;  /* Number of stages that have been added */
	uint32_t max_num_stages;  /* Max number of stages */
	/* Stages indexed by ID */
	struct opdl_stage *stages;
	/* Memory for storing slot data */
	uint8_t slots[0] __rte_cache_aligned;
};


/* Return input stage of a opdl_ring */
static __rte_always_inline struct opdl_stage *
input_stage(const struct opdl_ring *t)
{
	return &t->stages[0];
}

/* Check if a stage is the input stage */
static __rte_always_inline bool
is_input_stage(const struct opdl_stage *s)
{
	return s->index == 0;
}

/* Get slot pointer from sequence number */
static __rte_always_inline void *
get_slot(const struct opdl_ring *t, uint32_t n)
{
	return (void *)(uintptr_t)&t->slots[(n & t->mask) * t->slot_size];
}

/* Find how many entries are available for processing */
static __rte_always_inline uint32_t
available(const struct opdl_stage *s)
{
	if (s->threadsafe == true) {
		uint32_t n = __atomic_load_n(&s->shared.available_seq,
				__ATOMIC_ACQUIRE) -
				__atomic_load_n(&s->shared.head,
				__ATOMIC_ACQUIRE);

		/* Return 0 if available_seq needs to be updated */
		return (n <= s->num_slots) ? n : 0;
	}

	/* Single threaded */
	return s->available_seq - s->head;
}

/* Read sequence number of dependencies and find minimum */
static __rte_always_inline void
update_available_seq(struct opdl_stage *s)
{
	uint32_t i;
	uint32_t this_tail = s->shared.tail;
	uint32_t min_seq = __atomic_load_n(&s->deps[0]->tail, __ATOMIC_ACQUIRE);
	/* Input stage sequence numbers are greater than the sequence numbers of
	 * its dependencies so an offset of t->num_slots is needed when
	 * calculating available slots and also the condition which is used to
	 * determine the dependencies minimum sequence number must be reverted.
	 */
	uint32_t wrap;

	if (is_input_stage(s)) {
		wrap = s->num_slots;
		for (i = 1; i < s->num_deps; i++) {
			uint32_t seq = __atomic_load_n(&s->deps[i]->tail,
					__ATOMIC_ACQUIRE);
			if ((this_tail - seq) > (this_tail - min_seq))
				min_seq = seq;
		}
	} else {
		wrap = 0;
		for (i = 1; i < s->num_deps; i++) {
			uint32_t seq = __atomic_load_n(&s->deps[i]->tail,
					__ATOMIC_ACQUIRE);
			if ((seq - this_tail) < (min_seq - this_tail))
				min_seq = seq;
		}
	}

	if (s->threadsafe == false)
		s->available_seq = min_seq + wrap;
	else
		__atomic_store_n(&s->shared.available_seq, min_seq + wrap,
				__ATOMIC_RELEASE);
}

/* Wait until the number of available slots reaches number requested */
static __rte_always_inline void
wait_for_available(struct opdl_stage *s, uint32_t n)
{
	while (available(s) < n) {
		rte_pause();
		update_available_seq(s);
	}
}

/* Return number of slots to process based on number requested and mode */
static __rte_always_inline uint32_t
num_to_process(struct opdl_stage *s, uint32_t n, bool block)
{
	/* Don't read tail sequences of dependencies if not needed */
	if (available(s) >= n)
		return n;

	update_available_seq(s);

	if (block == false) {
		uint32_t avail = available(s);

		if (avail == 0) {
			rte_pause();
			return 0;
		}
		return (avail <= n) ? avail : n;
	}

	if (unlikely(n > s->num_slots)) {
		PMD_DRV_LOG(ERR, "%u entries is more than max (%u)",
				n, s->num_slots);
		return 0;  /* Avoid infinite loop */
	}
	/* blocking */
	wait_for_available(s, n);
	return n;
}

/* Copy entries in to slots with wrap-around */
static __rte_always_inline void
copy_entries_in(struct opdl_ring *t, uint32_t start, const void *entries,
		uint32_t num_entries)
{
	uint32_t slot_size = t->slot_size;
	uint32_t slot_index = start & t->mask;

	if (slot_index + num_entries <= t->num_slots) {
		rte_memcpy(get_slot(t, start), entries,
				num_entries * slot_size);
	} else {
		uint32_t split = t->num_slots - slot_index;

		rte_memcpy(get_slot(t, start), entries, split * slot_size);
		rte_memcpy(get_slot(t, 0),
				RTE_PTR_ADD(entries, split * slot_size),
				(num_entries - split) * slot_size);
	}
}

/* Copy entries out from slots with wrap-around */
static __rte_always_inline void
copy_entries_out(struct opdl_ring *t, uint32_t start, void *entries,
		uint32_t num_entries)
{
	uint32_t slot_size = t->slot_size;
	uint32_t slot_index = start & t->mask;

	if (slot_index + num_entries <= t->num_slots) {
		rte_memcpy(entries, get_slot(t, start),
				num_entries * slot_size);
	} else {
		uint32_t split = t->num_slots - slot_index;

		rte_memcpy(entries, get_slot(t, start), split * slot_size);
		rte_memcpy(RTE_PTR_ADD(entries, split * slot_size),
				get_slot(t, 0),
				(num_entries - split) * slot_size);
	}
}

/* Input function optimised for single thread */
static __rte_always_inline uint32_t
opdl_ring_input_singlethread(struct opdl_ring *t, const void *entries,
		uint32_t num_entries, bool block)
{
	struct opdl_stage *s = input_stage(t);
	uint32_t head = s->head;

	num_entries = num_to_process(s, num_entries, block);
	if (num_entries == 0)
		return 0;

	copy_entries_in(t, head, entries, num_entries);

	s->head += num_entries;
	__atomic_store_n(&s->shared.tail, s->head, __ATOMIC_RELEASE);

	return num_entries;
}

/* Convert head and tail of claim_manager into valid index */
static __rte_always_inline uint32_t
claim_mgr_index(uint32_t n)
{
	return n & (OPDL_DISCLAIMS_PER_LCORE - 1);
}

/* Check if there are available slots in claim_manager */
static __rte_always_inline bool
claim_mgr_available(struct claim_manager *mgr)
{
	return (mgr->mgr_head < (mgr->mgr_tail + OPDL_DISCLAIMS_PER_LCORE)) ?
			true : false;
}

/* Record a new claim. Only use after first checking an entry is available */
static __rte_always_inline void
claim_mgr_add(struct claim_manager *mgr, uint32_t tail, uint32_t head)
{
	if ((mgr->mgr_head != mgr->mgr_tail) &&
			(mgr->claims[claim_mgr_index(mgr->mgr_head - 1)].head ==
			tail)) {
		/* Combine with previous claim */
		mgr->claims[claim_mgr_index(mgr->mgr_head - 1)].head = head;
	} else {
		mgr->claims[claim_mgr_index(mgr->mgr_head)].head = head;
		mgr->claims[claim_mgr_index(mgr->mgr_head)].tail = tail;
		mgr->mgr_head++;
	}

	mgr->num_claimed += (head - tail);
}

/* Read the oldest recorded claim */
static __rte_always_inline bool
claim_mgr_read(struct claim_manager *mgr, uint32_t *tail, uint32_t *head)
{
	if (mgr->mgr_head == mgr->mgr_tail)
		return false;

	*head = mgr->claims[claim_mgr_index(mgr->mgr_tail)].head;
	*tail = mgr->claims[claim_mgr_index(mgr->mgr_tail)].tail;
	return true;
}

/* Remove the oldest recorded claim. Only use after first reading the entry */
static __rte_always_inline void
claim_mgr_remove(struct claim_manager *mgr)
{
	mgr->num_claimed -= (mgr->claims[claim_mgr_index(mgr->mgr_tail)].head -
			mgr->claims[claim_mgr_index(mgr->mgr_tail)].tail);
	mgr->mgr_tail++;
}

/* Update tail in the oldest claim. Only use after first reading the entry */
static __rte_always_inline void
claim_mgr_move_tail(struct claim_manager *mgr, uint32_t num_entries)
{
	mgr->num_claimed -= num_entries;
	mgr->claims[claim_mgr_index(mgr->mgr_tail)].tail += num_entries;
}

static __rte_always_inline void
opdl_stage_disclaim_multithread_n(struct opdl_stage *s,
		uint32_t num_entries, bool block)
{
	struct claim_manager *disclaims = &s->pending_disclaims[rte_lcore_id()];
	uint32_t head;
	uint32_t tail;

	while (num_entries) {
		bool ret = claim_mgr_read(disclaims, &tail, &head);

		if (ret == false)
			break;  /* nothing is claimed */
		/* There should be no race condition here. If shared.tail
		 * matches, no other core can update it until this one does.
		 */
		if (__atomic_load_n(&s->shared.tail, __ATOMIC_ACQUIRE) ==
				tail) {
			if (num_entries >= (head - tail)) {
				claim_mgr_remove(disclaims);
				__atomic_store_n(&s->shared.tail, head,
						__ATOMIC_RELEASE);
				num_entries -= (head - tail);
			} else {
				claim_mgr_move_tail(disclaims, num_entries);
				__atomic_store_n(&s->shared.tail,
						num_entries + tail,
						__ATOMIC_RELEASE);
				num_entries = 0;
			}
		} else if (block == false)
			break;  /* blocked by other thread */
		/* Keep going until num_entries are disclaimed. */
		rte_pause();
	}

	disclaims->num_to_disclaim = num_entries;
}

/* Move head atomically, returning number of entries available to process and
 * the original value of head. For non-input stages, the claim is recorded
 * so that the tail can be updated later by opdl_stage_disclaim().
 */
static __rte_always_inline void
move_head_atomically(struct opdl_stage *s, uint32_t *num_entries,
		uint32_t *old_head, bool block, bool claim_func)
{
	uint32_t orig_num_entries = *num_entries;
	uint32_t ret;
	struct claim_manager *disclaims = &s->pending_disclaims[rte_lcore_id()];

	/* Attempt to disclaim any outstanding claims */
	opdl_stage_disclaim_multithread_n(s, disclaims->num_to_disclaim,
			false);

	*old_head = __atomic_load_n(&s->shared.head, __ATOMIC_ACQUIRE);
	while (true) {
		bool success;
		/* If called by opdl_ring_input(), claim does not need to be
		 * recorded, as there will be no disclaim.
		 */
		if (claim_func) {
			/* Check that the claim can be recorded */
			ret = claim_mgr_available(disclaims);
			if (ret == false) {
				/* exit out if claim can't be recorded */
				*num_entries = 0;
				return;
			}
		}

		*num_entries = num_to_process(s, orig_num_entries, block);
		if (*num_entries == 0)
			return;

		success = __atomic_compare_exchange_n(&s->shared.head, old_head,
				*old_head + *num_entries,
				true,  /* may fail spuriously */
				__ATOMIC_RELEASE,  /* memory order on success */
				__ATOMIC_ACQUIRE);  /* memory order on fail */
		if (likely(success))
			break;
		rte_pause();
	}

	if (claim_func)
		/* Store the claim record */
		claim_mgr_add(disclaims, *old_head, *old_head + *num_entries);
}

/* Input function that supports multiple threads */
static __rte_always_inline uint32_t
opdl_ring_input_multithread(struct opdl_ring *t, const void *entries,
		uint32_t num_entries, bool block)
{
	struct opdl_stage *s = input_stage(t);
	uint32_t old_head;

	move_head_atomically(s, &num_entries, &old_head, block, false);
	if (num_entries == 0)
		return 0;

	copy_entries_in(t, old_head, entries, num_entries);

	/* If another thread started inputting before this one, but hasn't
	 * finished, we need to wait for it to complete to update the tail.
	 */
	rte_wait_until_equal_32(&s->shared.tail, old_head, __ATOMIC_ACQUIRE);

	__atomic_store_n(&s->shared.tail, old_head + num_entries,
			__ATOMIC_RELEASE);

	return num_entries;
}

static __rte_always_inline uint32_t
opdl_first_entry_id(uint32_t start_seq, uint8_t nb_p_lcores,
		uint8_t this_lcore)
{
	return ((nb_p_lcores <= 1) ? 0 :
			(nb_p_lcores - (start_seq % nb_p_lcores) + this_lcore) %
			nb_p_lcores);
}

/* Claim slots to process, optimised for single-thread operation */
static __rte_always_inline uint32_t
opdl_stage_claim_singlethread(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block, bool atomic)
{
	uint32_t i = 0, j = 0,  offset;
	uint32_t opa_id   = 0;
	uint32_t flow_id  = 0;
	uint64_t event    = 0;
	void *get_slots;
	struct rte_event *ev;
	RTE_SET_USED(seq);
	struct opdl_ring *t = s->t;
	uint8_t *entries_offset = (uint8_t *)entries;

	if (!atomic) {

		offset = opdl_first_entry_id(s->seq, s->nb_instance,
				s->instance_id);

		num_entries = s->nb_instance * num_entries;

		num_entries = num_to_process(s, num_entries, block);

		for (; offset < num_entries; offset += s->nb_instance) {
			get_slots = get_slot(t, s->head + offset);
			memcpy(entries_offset, get_slots, t->slot_size);
			entries_offset += t->slot_size;
			i++;
		}
	} else {
		num_entries = num_to_process(s, num_entries, block);

		for (j = 0; j < num_entries; j++) {
			ev = (struct rte_event *)get_slot(t, s->head+j);

			event  = __atomic_load_n(&(ev->event),
					__ATOMIC_ACQUIRE);

			opa_id = OPDL_OPA_MASK & (event >> OPDL_OPA_OFFSET);
			flow_id  = OPDL_FLOWID_MASK & event;

			if (opa_id >= s->queue_id)
				continue;

			if ((flow_id % s->nb_instance) == s->instance_id) {
				memcpy(entries_offset, ev, t->slot_size);
				entries_offset += t->slot_size;
				i++;
			}
		}
	}
	s->shadow_head = s->head;
	s->head += num_entries;
	s->num_claimed = num_entries;
	s->num_event = i;
	s->pos = 0;

	/* automatically disclaim entries if number of rte_events is zero */
	if (unlikely(i == 0))
		opdl_stage_disclaim(s, 0, false);

	return i;
}

/* Thread-safe version of function to claim slots for processing */
static __rte_always_inline uint32_t
opdl_stage_claim_multithread(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block)
{
	uint32_t old_head;
	struct opdl_ring *t = s->t;
	uint32_t i = 0, offset;
	uint8_t *entries_offset = (uint8_t *)entries;

	if (seq == NULL) {
		PMD_DRV_LOG(ERR, "Invalid seq PTR");
		return 0;
	}
	offset = opdl_first_entry_id(*seq, s->nb_instance, s->instance_id);
	num_entries = offset + (s->nb_instance * num_entries);

	move_head_atomically(s, &num_entries, &old_head, block, true);

	for (; offset < num_entries; offset += s->nb_instance) {
		memcpy(entries_offset, get_slot(t, s->head + offset),
			t->slot_size);
		entries_offset += t->slot_size;
		i++;
	}

	*seq = old_head;

	return i;
}

/* Claim and copy slot pointers, optimised for single-thread operation */
static __rte_always_inline uint32_t
opdl_stage_claim_copy_singlethread(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block)
{
	num_entries = num_to_process(s, num_entries, block);
	if (num_entries == 0)
		return 0;
	copy_entries_out(s->t, s->head, entries, num_entries);
	if (seq != NULL)
		*seq = s->head;
	s->head += num_entries;
	return num_entries;
}

/* Thread-safe version of function to claim and copy pointers to slots */
static __rte_always_inline uint32_t
opdl_stage_claim_copy_multithread(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block)
{
	uint32_t old_head;

	move_head_atomically(s, &num_entries, &old_head, block, true);
	if (num_entries == 0)
		return 0;
	copy_entries_out(s->t, old_head, entries, num_entries);
	if (seq != NULL)
		*seq = old_head;
	return num_entries;
}

static __rte_always_inline void
opdl_stage_disclaim_singlethread_n(struct opdl_stage *s,
		uint32_t num_entries)
{
	uint32_t old_tail = s->shared.tail;

	if (unlikely(num_entries > (s->head - old_tail))) {
		PMD_DRV_LOG(WARNING, "Attempt to disclaim (%u) more than claimed (%u)",
				num_entries, s->head - old_tail);
		num_entries = s->head - old_tail;
	}
	__atomic_store_n(&s->shared.tail, num_entries + old_tail,
			__ATOMIC_RELEASE);
}

uint32_t
opdl_ring_input(struct opdl_ring *t, const void *entries, uint32_t num_entries,
		bool block)
{
	if (input_stage(t)->threadsafe == false)
		return opdl_ring_input_singlethread(t, entries, num_entries,
				block);
	else
		return opdl_ring_input_multithread(t, entries, num_entries,
				block);
}

uint32_t
opdl_ring_copy_from_burst(struct opdl_ring *t, struct opdl_stage *s,
		const void *entries, uint32_t num_entries, bool block)
{
	uint32_t head = s->head;

	num_entries = num_to_process(s, num_entries, block);

	if (num_entries == 0)
		return 0;

	copy_entries_in(t, head, entries, num_entries);

	s->head += num_entries;
	__atomic_store_n(&s->shared.tail, s->head, __ATOMIC_RELEASE);

	return num_entries;

}

uint32_t
opdl_ring_copy_to_burst(struct opdl_ring *t, struct opdl_stage *s,
		void *entries, uint32_t num_entries, bool block)
{
	uint32_t head = s->head;

	num_entries = num_to_process(s, num_entries, block);
	if (num_entries == 0)
		return 0;

	copy_entries_out(t, head, entries, num_entries);

	s->head += num_entries;
	__atomic_store_n(&s->shared.tail, s->head, __ATOMIC_RELEASE);

	return num_entries;
}

uint32_t
opdl_stage_find_num_available(struct opdl_stage *s, uint32_t num_entries)
{
	/* return (num_to_process(s, num_entries, false)); */

	if (available(s) >= num_entries)
		return num_entries;

	update_available_seq(s);

	uint32_t avail = available(s);

	if (avail == 0) {
		rte_pause();
		return 0;
	}
	return (avail <= num_entries) ? avail : num_entries;
}

uint32_t
opdl_stage_claim(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block, bool atomic)
{
	if (s->threadsafe == false)
		return opdl_stage_claim_singlethread(s, entries, num_entries,
				seq, block, atomic);
	else
		return opdl_stage_claim_multithread(s, entries, num_entries,
				seq, block);
}

uint32_t
opdl_stage_claim_copy(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block)
{
	if (s->threadsafe == false)
		return opdl_stage_claim_copy_singlethread(s, entries,
				num_entries, seq, block);
	else
		return opdl_stage_claim_copy_multithread(s, entries,
				num_entries, seq, block);
}

void
opdl_stage_disclaim_n(struct opdl_stage *s, uint32_t num_entries,
		bool block)
{

	if (s->threadsafe == false) {
		opdl_stage_disclaim_singlethread_n(s, s->num_claimed);
	} else {
		struct claim_manager *disclaims =
			&s->pending_disclaims[rte_lcore_id()];

		if (unlikely(num_entries > s->num_slots)) {
			PMD_DRV_LOG(WARNING, "Attempt to disclaim (%u) more than claimed (%u)",
					num_entries, disclaims->num_claimed);
			num_entries = disclaims->num_claimed;
		}

		num_entries = RTE_MIN(num_entries + disclaims->num_to_disclaim,
				disclaims->num_claimed);
		opdl_stage_disclaim_multithread_n(s, num_entries, block);
	}
}

int
opdl_stage_disclaim(struct opdl_stage *s, uint32_t num_entries, bool block)
{
	if (num_entries != s->num_event) {
		rte_errno = EINVAL;
		return 0;
	}
	if (s->threadsafe == false) {
		__atomic_store_n(&s->shared.tail, s->head, __ATOMIC_RELEASE);
		s->seq += s->num_claimed;
		s->shadow_head = s->head;
		s->num_claimed = 0;
	} else {
		struct claim_manager *disclaims =
				&s->pending_disclaims[rte_lcore_id()];
		opdl_stage_disclaim_multithread_n(s, disclaims->num_claimed,
				block);
	}
	return num_entries;
}

uint32_t
opdl_ring_available(struct opdl_ring *t)
{
	return opdl_stage_available(&t->stages[0]);
}

uint32_t
opdl_stage_available(struct opdl_stage *s)
{
	update_available_seq(s);
	return available(s);
}

void
opdl_ring_flush(struct opdl_ring *t)
{
	struct opdl_stage *s = input_stage(t);

	wait_for_available(s, s->num_slots);
}

/******************** Non performance sensitive functions ********************/

/* Initial setup of a new stage's context */
static int
init_stage(struct opdl_ring *t, struct opdl_stage *s, bool threadsafe,
		bool is_input)
{
	uint32_t available = (is_input) ? t->num_slots : 0;

	s->t = t;
	s->num_slots = t->num_slots;
	s->index = t->num_stages;
	s->threadsafe = threadsafe;
	s->shared.stage = s;

	/* Alloc memory for deps */
	s->dep_tracking = rte_zmalloc_socket(LIB_NAME,
			t->max_num_stages * sizeof(enum dep_type),
			0, t->socket);
	if (s->dep_tracking == NULL)
		return -ENOMEM;

	s->deps = rte_zmalloc_socket(LIB_NAME,
			t->max_num_stages * sizeof(struct shared_state *),
			0, t->socket);
	if (s->deps == NULL) {
		rte_free(s->dep_tracking);
		return -ENOMEM;
	}

	s->dep_tracking[s->index] = DEP_SELF;

	if (threadsafe == true)
		s->shared.available_seq = available;
	else
		s->available_seq = available;

	return 0;
}

/* Add direct or indirect dependencies between stages */
static int
add_dep(struct opdl_stage *dependent, const struct opdl_stage *dependency,
		enum dep_type type)
{
	struct opdl_ring *t = dependent->t;
	uint32_t i;

	/* Add new direct dependency */
	if ((type == DEP_DIRECT) &&
			(dependent->dep_tracking[dependency->index] ==
					DEP_NONE)) {
		PMD_DRV_LOG(DEBUG, "%s:%u direct dependency on %u",
				t->name, dependent->index, dependency->index);
		dependent->dep_tracking[dependency->index] = DEP_DIRECT;
	}

	/* Add new indirect dependency or change direct to indirect */
	if ((type == DEP_INDIRECT) &&
			((dependent->dep_tracking[dependency->index] ==
			DEP_NONE) ||
			(dependent->dep_tracking[dependency->index] ==
			DEP_DIRECT))) {
		PMD_DRV_LOG(DEBUG, "%s:%u indirect dependency on %u",
				t->name, dependent->index, dependency->index);
		dependent->dep_tracking[dependency->index] = DEP_INDIRECT;
	}

	/* Shouldn't happen... */
	if ((dependent->dep_tracking[dependency->index] == DEP_SELF) &&
			(dependent != input_stage(t))) {
		PMD_DRV_LOG(ERR, "Loop in dependency graph %s:%u",
				t->name, dependent->index);
		return -EINVAL;
	}

	/* Keep going to dependencies of the dependency, until input stage */
	if (dependency != input_stage(t))
		for (i = 0; i < dependency->num_deps; i++) {
			int ret = add_dep(dependent, dependency->deps[i]->stage,
					DEP_INDIRECT);

			if (ret < 0)
				return ret;
		}

	/* Make list of sequence numbers for direct dependencies only */
	if (type == DEP_DIRECT)
		for (i = 0, dependent->num_deps = 0; i < t->num_stages; i++)
			if (dependent->dep_tracking[i] == DEP_DIRECT) {
				if ((i == 0) && (dependent->num_deps > 1))
					rte_panic("%s:%u depends on > input",
							t->name,
							dependent->index);
				dependent->deps[dependent->num_deps++] =
						&t->stages[i].shared;
			}

	return 0;
}

struct opdl_ring *
opdl_ring_create(const char *name, uint32_t num_slots, uint32_t slot_size,
		uint32_t max_num_stages, int socket)
{
	struct opdl_ring *t;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int mz_flags = 0;
	struct opdl_stage *st = NULL;
	const struct rte_memzone *mz = NULL;
	size_t alloc_size = RTE_CACHE_LINE_ROUNDUP(sizeof(*t) +
			(num_slots * slot_size));

	/* Compile time checking */
	RTE_BUILD_BUG_ON((sizeof(struct shared_state) & RTE_CACHE_LINE_MASK) !=
			0);
	RTE_BUILD_BUG_ON((offsetof(struct opdl_stage, shared) &
			RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct opdl_ring, slots) &
			RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON(!rte_is_power_of_2(OPDL_DISCLAIMS_PER_LCORE));

	/* Parameter checking */
	if (name == NULL) {
		PMD_DRV_LOG(ERR, "name param is NULL");
		return NULL;
	}
	if (!rte_is_power_of_2(num_slots)) {
		PMD_DRV_LOG(ERR, "num_slots (%u) for %s is not power of 2",
				num_slots, name);
		return NULL;
	}

	/* Alloc memory for stages */
	st = rte_zmalloc_socket(LIB_NAME,
		max_num_stages * sizeof(struct opdl_stage),
		RTE_CACHE_LINE_SIZE, socket);
	if (st == NULL)
		goto exit_fail;

	snprintf(mz_name, sizeof(mz_name), "%s%s", LIB_NAME, name);

	/* Alloc memory for memzone */
	mz = rte_memzone_reserve(mz_name, alloc_size, socket, mz_flags);
	if (mz == NULL)
		goto exit_fail;

	t = mz->addr;

	/* Initialise opdl_ring queue */
	memset(t, 0, sizeof(*t));
	strlcpy(t->name, name, sizeof(t->name));
	t->socket = socket;
	t->num_slots = num_slots;
	t->mask = num_slots - 1;
	t->slot_size = slot_size;
	t->max_num_stages = max_num_stages;
	t->stages = st;

	PMD_DRV_LOG(DEBUG, "Created %s at %p (num_slots=%u,socket=%i,slot_size=%u)",
			t->name, t, num_slots, socket, slot_size);

	return t;

exit_fail:
	PMD_DRV_LOG(ERR, "Cannot reserve memory");
	rte_free(st);
	rte_memzone_free(mz);

	return NULL;
}

void *
opdl_ring_get_slot(const struct opdl_ring *t, uint32_t index)
{
	return get_slot(t, index);
}

bool
opdl_ring_cas_slot(struct opdl_stage *s, const struct rte_event *ev,
		uint32_t index, bool atomic)
{
	uint32_t i = 0, offset;
	struct opdl_ring *t = s->t;
	struct rte_event *ev_orig = NULL;
	bool ev_updated = false;
	uint64_t ev_temp    = 0;
	uint64_t ev_update  = 0;

	uint32_t opa_id   = 0;
	uint32_t flow_id  = 0;
	uint64_t event    = 0;

	if (index > s->num_event) {
		PMD_DRV_LOG(ERR, "index is overflow");
		return ev_updated;
	}

	ev_temp = ev->event & OPDL_EVENT_MASK;

	if (!atomic) {
		offset = opdl_first_entry_id(s->seq, s->nb_instance,
				s->instance_id);
		offset += index*s->nb_instance;
		ev_orig = get_slot(t, s->shadow_head+offset);
		if ((ev_orig->event&OPDL_EVENT_MASK) != ev_temp) {
			ev_orig->event = ev->event;
			ev_updated = true;
		}
		if (ev_orig->u64 != ev->u64) {
			ev_orig->u64 = ev->u64;
			ev_updated = true;
		}

	} else {
		for (i = s->pos; i < s->num_claimed; i++) {
			ev_orig = (struct rte_event *)
				get_slot(t, s->shadow_head+i);

			event  = __atomic_load_n(&(ev_orig->event),
					__ATOMIC_ACQUIRE);

			opa_id = OPDL_OPA_MASK & (event >> OPDL_OPA_OFFSET);
			flow_id  = OPDL_FLOWID_MASK & event;

			if (opa_id >= s->queue_id)
				continue;

			if ((flow_id % s->nb_instance) == s->instance_id) {
				ev_update = s->queue_id;
				ev_update = (ev_update << OPDL_OPA_OFFSET)
					| ev->event;

				s->pos = i + 1;

				if ((event & OPDL_EVENT_MASK) !=
						ev_temp) {
					__atomic_store_n(&(ev_orig->event),
							ev_update,
							__ATOMIC_RELEASE);
					ev_updated = true;
				}
				if (ev_orig->u64 != ev->u64) {
					ev_orig->u64 = ev->u64;
					ev_updated = true;
				}

				break;
			}
		}

	}

	return ev_updated;
}

int
opdl_ring_get_socket(const struct opdl_ring *t)
{
	return t->socket;
}

uint32_t
opdl_ring_get_num_slots(const struct opdl_ring *t)
{
	return t->num_slots;
}

const char *
opdl_ring_get_name(const struct opdl_ring *t)
{
	return t->name;
}

/* Check dependency list is valid for a given opdl_ring */
static int
check_deps(struct opdl_ring *t, struct opdl_stage *deps[],
		uint32_t num_deps)
{
	unsigned int i;

	for (i = 0; i < num_deps; ++i) {
		if (!deps[i]) {
			PMD_DRV_LOG(ERR, "deps[%u] is NULL", i);
			return -EINVAL;
		}
		if (t != deps[i]->t) {
			PMD_DRV_LOG(ERR, "deps[%u] is in opdl_ring %s, not %s",
					i, deps[i]->t->name, t->name);
			return -EINVAL;
		}
	}

	return 0;
}

struct opdl_stage *
opdl_stage_add(struct opdl_ring *t, bool threadsafe, bool is_input)
{
	struct opdl_stage *s;

	/* Parameter checking */
	if (!t) {
		PMD_DRV_LOG(ERR, "opdl_ring is NULL");
		return NULL;
	}
	if (t->num_stages == t->max_num_stages) {
		PMD_DRV_LOG(ERR, "%s has max number of stages (%u)",
				t->name, t->max_num_stages);
		return NULL;
	}

	s = &t->stages[t->num_stages];

	if (((uintptr_t)&s->shared & RTE_CACHE_LINE_MASK) != 0)
		PMD_DRV_LOG(WARNING, "Tail seq num (%p) of %s stage not cache aligned",
				&s->shared, t->name);

	if (init_stage(t, s, threadsafe, is_input) < 0) {
		PMD_DRV_LOG(ERR, "Cannot reserve memory");
		return NULL;
	}
	t->num_stages++;

	return s;
}

uint32_t
opdl_stage_deps_add(struct opdl_ring *t, struct opdl_stage *s,
		uint32_t nb_instance, uint32_t instance_id,
		struct opdl_stage *deps[],
		uint32_t num_deps)
{
	uint32_t i;
	int ret = 0;

	if ((num_deps > 0) && (!deps)) {
		PMD_DRV_LOG(ERR, "%s stage has NULL dependencies", t->name);
		return -1;
	}
	ret = check_deps(t, deps, num_deps);
	if (ret < 0)
		return ret;

	for (i = 0; i < num_deps; i++) {
		ret = add_dep(s, deps[i], DEP_DIRECT);
		if (ret < 0)
			return ret;
	}

	s->nb_instance = nb_instance;
	s->instance_id = instance_id;

	return ret;
}

struct opdl_stage *
opdl_ring_get_input_stage(const struct opdl_ring *t)
{
	return input_stage(t);
}

int
opdl_stage_set_deps(struct opdl_stage *s, struct opdl_stage *deps[],
		uint32_t num_deps)
{
	unsigned int i;
	int ret;

	if ((num_deps == 0) || (!deps)) {
		PMD_DRV_LOG(ERR, "cannot set NULL dependencies");
		return -EINVAL;
	}

	ret = check_deps(s->t, deps, num_deps);
	if (ret < 0)
		return ret;

	/* Update deps */
	for (i = 0; i < num_deps; i++)
		s->deps[i] = &deps[i]->shared;
	s->num_deps = num_deps;

	return 0;
}

struct opdl_ring *
opdl_stage_get_opdl_ring(const struct opdl_stage *s)
{
	return s->t;
}

void
opdl_stage_set_queue_id(struct opdl_stage *s,
		uint32_t queue_id)
{
	s->queue_id = queue_id;
}

void
opdl_ring_dump(const struct opdl_ring *t, FILE *f)
{
	uint32_t i;

	if (t == NULL) {
		fprintf(f, "NULL OPDL!\n");
		return;
	}
	fprintf(f, "OPDL \"%s\": num_slots=%u; mask=%#x; slot_size=%u; num_stages=%u; socket=%i\n",
			t->name, t->num_slots, t->mask, t->slot_size,
			t->num_stages, t->socket);
	for (i = 0; i < t->num_stages; i++) {
		uint32_t j;
		const struct opdl_stage *s = &t->stages[i];

		fprintf(f, "  %s[%u]: threadsafe=%s; head=%u; available_seq=%u; tail=%u; deps=%u",
				t->name, i, (s->threadsafe) ? "true" : "false",
				(s->threadsafe) ? s->shared.head : s->head,
				(s->threadsafe) ? s->shared.available_seq :
				s->available_seq,
				s->shared.tail, (s->num_deps > 0) ?
				s->deps[0]->stage->index : 0);
		for (j = 1; j < s->num_deps; j++)
			fprintf(f, ",%u", s->deps[j]->stage->index);
		fprintf(f, "\n");
	}
	fflush(f);
}

void
opdl_ring_free(struct opdl_ring *t)
{
	uint32_t i;
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];

	if (t == NULL) {
		PMD_DRV_LOG(DEBUG, "Freeing NULL OPDL Ring!");
		return;
	}

	PMD_DRV_LOG(DEBUG, "Freeing %s opdl_ring at %p", t->name, t);

	for (i = 0; i < t->num_stages; ++i) {
		rte_free(t->stages[i].deps);
		rte_free(t->stages[i].dep_tracking);
	}

	rte_free(t->stages);

	snprintf(mz_name, sizeof(mz_name), "%s%s", LIB_NAME, t->name);
	mz = rte_memzone_lookup(mz_name);
	if (rte_memzone_free(mz) != 0)
		PMD_DRV_LOG(ERR, "Cannot free memzone for %s", t->name);
}

/* search a opdl_ring from its name */
struct opdl_ring *
opdl_ring_lookup(const char *name)
{
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];

	snprintf(mz_name, sizeof(mz_name), "%s%s", LIB_NAME, name);

	mz = rte_memzone_lookup(mz_name);
	if (mz == NULL)
		return NULL;

	return mz->addr;
}

void
opdl_ring_set_stage_threadsafe(struct opdl_stage *s, bool threadsafe)
{
	s->threadsafe = threadsafe;
}
