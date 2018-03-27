/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _OPDL_H_
#define _OPDL_H_

/**
 * @file
 * The "opdl_ring" is a data structure that contains a fixed number of slots,
 * with each slot having the same, but configurable, size. Entries are input
 * into the opdl_ring by copying into available slots. Once in the opdl_ring,
 * an entry is processed by a number of stages, with the ordering of stage
 * processing controlled by making stages dependent on one or more other stages.
 * An entry is not available for a stage to process until it has been processed
 * by that stages dependencies. Entries are always made available for
 * processing in the same order that they were input in to the opdl_ring.
 * Inputting is considered as a stage that depends on all other stages,
 * and is also a dependency of all stages.
 *
 * Inputting and processing in a stage can support multi-threading. Note that
 * multi-thread processing can also be done by making stages co-operate e.g. two
 * stages where one processes the even packets and the other processes odd
 * packets.
 *
 * A opdl_ring can be used as the basis for pipeline based applications. Instead
 * of each stage in a pipeline dequeueing from a ring, processing and enqueueing
 * to another ring, it can process entries in-place on the ring. If stages do
 * not depend on each other, they can run in parallel.
 *
 * The opdl_ring works with entries of configurable size, these could be
 * pointers to mbufs, pointers to mbufs with application specific meta-data,
 * tasks etc.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_eventdev.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPDL_DISCLAIMS_PER_LCORE
/** Multi-threaded processing allows one thread to process multiple batches in a
 * stage, while another thread is processing a single large batch. This number
 * controls how many non-contiguous batches one stage can process before being
 * blocked by the other stage.
 */
#define OPDL_DISCLAIMS_PER_LCORE 8
#endif

/** Opaque handle to a opdl_ring instance */
struct opdl_ring;

/** Opaque handle to a single stage in a opdl_ring */
struct opdl_stage;

/**
 * Create a new instance of a opdl_ring.
 *
 * @param name
 *   String containing the name to give the new opdl_ring instance.
 * @param num_slots
 *   How many slots the opdl_ring contains. Must be a power a 2!
 * @param slot_size
 *   How many bytes in each slot.
 * @param max_num_stages
 *   Maximum number of stages.
 * @param socket
 *   The NUMA socket (or SOCKET_ID_ANY) to allocate the memory used for this
 *   opdl_ring instance.
 * @param threadsafe
 *   Whether to support multiple threads inputting to the opdl_ring or not.
 *   Enabling this may have a negative impact on performance if only one thread
 *   will be inputting.
 *
 * @return
 *   A pointer to a new opdl_ring instance, or NULL on error.
 */
struct opdl_ring *
opdl_ring_create(const char *name, uint32_t num_slots, uint32_t slot_size,
		uint32_t max_num_stages, int socket);

/**
 * Get pointer to individual slot in a opdl_ring.
 *
 * @param t
 *   The opdl_ring.
 * @param index
 *   Index of slot. If greater than the number of slots it will be masked to be
 *   within correct range.
 *
 * @return
 *   A pointer to that slot.
 */
void *
opdl_ring_get_slot(const struct opdl_ring *t, uint32_t index);

/**
 * Get NUMA socket used by a opdl_ring.
 *
 * @param t
 *   The opdl_ring.
 *
 * @return
 *   NUMA socket.
 */
int
opdl_ring_get_socket(const struct opdl_ring *t);

/**
 * Get number of slots in a opdl_ring.
 *
 * @param t
 *   The opdl_ring.
 *
 * @return
 *   Number of slots.
 */
uint32_t
opdl_ring_get_num_slots(const struct opdl_ring *t);

/**
 * Get name of a opdl_ring.
 *
 * @param t
 *   The opdl_ring.
 *
 * @return
 *   Name string.
 */
const char *
opdl_ring_get_name(const struct opdl_ring *t);

/**
 * Adds a new processing stage to a specified opdl_ring instance. Adding a stage
 * while there are entries in the opdl_ring being processed will cause undefined
 * behaviour.
 *
 * @param t
 *   The opdl_ring to add the stage to.
 * @param deps
 *   An array of pointers to other stages that this stage depends on. The other
 *   stages must be part of the same opdl_ring! Note that input is an implied
 *   dependency. This can be NULL if num_deps is 0.
 * @param num_deps
 *   The size of the deps array.
 * @param threadsafe
 *   Whether to support multiple threads processing this stage or  not.
 *   Enabling this may have a negative impact on performance if only one thread
 *   will be processing this stage.
 * @param is_input
 *   Indication to nitialise the stage with all slots available or none
 *
 * @return
 *   A pointer to the new stage, or NULL on error.
 */
struct opdl_stage *
opdl_stage_add(struct opdl_ring *t, bool threadsafe, bool is_input);

/**
 * Returns the input stage of a opdl_ring to be used by other API functions.
 *
 * @param t
 *   The opdl_ring.
 *
 * @return
 *   A pointer to the input stage.
 */
struct opdl_stage *
opdl_ring_get_input_stage(const struct opdl_ring *t);

/**
 * Sets the dependencies for a stage (clears all the previous deps!). Changing
 * dependencies while there are entries in the opdl_ring being processed will
 * cause undefined behaviour.
 *
 * @param s
 *   The stage to set the dependencies for.
 * @param deps
 *   An array of pointers to other stages that this stage will depends on. The
 *   other stages must be part of the same opdl_ring!
 * @param num_deps
 *   The size of the deps array. This must be > 0.
 *
 * @return
 *   0 on success, a negative value on error.
 */
int
opdl_stage_set_deps(struct opdl_stage *s, struct opdl_stage *deps[],
		uint32_t num_deps);

/**
 * Returns the opdl_ring that a stage belongs to.
 *
 * @param s
 *   The stage
 *
 * @return
 *   A pointer to the opdl_ring that the stage belongs to.
 */
struct opdl_ring *
opdl_stage_get_opdl_ring(const struct opdl_stage *s);

/**
 * Inputs a new batch of entries into the opdl_ring. This function is only
 * threadsafe (with the same opdl_ring parameter) if the threadsafe parameter of
 * opdl_ring_create() was true. For performance reasons, this function does not
 * check input parameters.
 *
 * @param t
 *   The opdl_ring to input entries in to.
 * @param entries
 *   An array of entries that will be copied in to the opdl_ring.
 * @param num_entries
 *   The size of the entries array.
 * @param block
 *   If this is true, the function blocks until enough slots are available to
 *   input all the requested entries. If false, then the function inputs as
 *   many entries as currently possible.
 *
 * @return
 *   The number of entries successfully input.
 */
uint32_t
opdl_ring_input(struct opdl_ring *t, const void *entries, uint32_t num_entries,
		bool block);

/**
 * Inputs a new batch of entries into a opdl stage. This function is only
 * threadsafe (with the same opdl parameter) if the threadsafe parameter of
 * opdl_create() was true. For performance reasons, this function does not
 * check input parameters.
 *
 * @param t
 *   The opdl ring to input entries in to.
 * @param s
 *   The stage to copy entries to.
 * @param entries
 *   An array of entries that will be copied in to the opdl ring.
 * @param num_entries
 *   The size of the entries array.
 * @param block
 *   If this is true, the function blocks until enough slots are available to
 *   input all the requested entries. If false, then the function inputs as
 *   many entries as currently possible.
 *
 * @return
 *   The number of entries successfully input.
 */
uint32_t
opdl_ring_copy_from_burst(struct opdl_ring *t, struct opdl_stage *s,
			const void *entries, uint32_t num_entries, bool block);

/**
 * Copy a batch of entries from the opdl ring. This function is only
 * threadsafe (with the same opdl parameter) if the threadsafe parameter of
 * opdl_create() was true. For performance reasons, this function does not
 * check input parameters.
 *
 * @param t
 *   The opdl ring to copy entries from.
 * @param s
 *   The stage to copy entries from.
 * @param entries
 *   An array of entries that will be copied from the opdl ring.
 * @param num_entries
 *   The size of the entries array.
 * @param block
 *   If this is true, the function blocks until enough slots are available to
 *   input all the requested entries. If false, then the function inputs as
 *   many entries as currently possible.
 *
 * @return
 *   The number of entries successfully input.
 */
uint32_t
opdl_ring_copy_to_burst(struct opdl_ring *t, struct opdl_stage *s,
		void *entries, uint32_t num_entries, bool block);

/**
 * Before processing a batch of entries, a stage must first claim them to get
 * access. This function is threadsafe using same opdl_stage parameter if
 * the stage was created with threadsafe set to true, otherwise it is only
 * threadsafe with a different opdl_stage per thread. For performance
 * reasons, this function does not check input parameters.
 *
 * @param s
 *   The opdl_ring stage to read entries in.
 * @param entries
 *   An array of pointers to entries that will be filled in by this function.
 * @param num_entries
 *   The number of entries to attempt to claim for processing (and the size of
 *   the entries array).
 * @param seq
 *   If not NULL, this is set to the value of the internal stage sequence number
 *   associated with the first entry returned.
 * @param block
 *   If this is true, the function blocks until num_entries slots are available
 *   to process. If false, then the function claims as many entries as
 *   currently possible.
 *
 * @param atomic
 *   if this is true, the function will return event according to event flow id
 * @return
 *   The number of pointers to entries filled in to the entries array.
 */
uint32_t
opdl_stage_claim(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block, bool atomic);

uint32_t
opdl_stage_deps_add(struct opdl_ring *t, struct opdl_stage *s,
		uint32_t nb_instance, uint32_t instance_id,
		struct opdl_stage *deps[], uint32_t num_deps);

/**
 * A function to check how many entries are ready to be claimed.
 *
 * @param entries
 *   An array of pointers to entries.
 * @param num_entries
 *   Number of entries in an array.
 * @param arg
 *   An opaque pointer to data passed to the claim function.
 * @param block
 *   When set to true, the function should wait until num_entries are ready to
 *   be processed. Otherwise it should return immediately.
 *
 * @return
 *   Number of entries ready to be claimed.
 */
typedef uint32_t (opdl_ring_check_entries_t)(void *entries[],
		uint32_t num_entries, void *arg, bool block);

/**
 * Before processing a batch of entries, a stage must first claim them to get
 * access. Each entry is checked by the passed check() function and depending
 * on block value, it waits until num_entries are ready or returns immediately.
 * This function is only threadsafe with a different opdl_stage per thread.
 *
 * @param s
 *   The opdl_ring stage to read entries in.
 * @param entries
 *   An array of pointers to entries that will be filled in by this function.
 * @param num_entries
 *   The number of entries to attempt to claim for processing (and the size of
 *   the entries array).
 * @param seq
 *   If not NULL, this is set to the value of the internal stage sequence number
 *   associated with the first entry returned.
 * @param block
 *   If this is true, the function blocks until num_entries ready slots are
 *   available to process. If false, then the function claims as many ready
 *   entries as currently possible.
 * @param check
 *   Pointer to a function called to check entries.
 * @param arg
 *   Opaque data passed to check() function.
 *
 * @return
 *   The number of pointers to ready entries filled in to the entries array.
 */
uint32_t
opdl_stage_claim_check(struct opdl_stage *s, void **entries,
		uint32_t num_entries, uint32_t *seq, bool block,
		opdl_ring_check_entries_t *check, void *arg);

/**
 * Before processing a batch of entries, a stage must first claim them to get
 * access. This function is threadsafe using same opdl_stage parameter if
 * the stage was created with threadsafe set to true, otherwise it is only
 * threadsafe with a different opdl_stage per thread.
 *
 * The difference between this function and opdl_stage_claim() is that this
 * function copies the entries from the opdl_ring. Note that any changes made to
 * the copied entries will not be reflected back in to the entries in the
 * opdl_ring, so this function probably only makes sense if the entries are
 * pointers to other data. For performance reasons, this function does not check
 * input parameters.
 *
 * @param s
 *   The opdl_ring stage to read entries in.
 * @param entries
 *   An array of entries that will be filled in by this function.
 * @param num_entries
 *   The number of entries to attempt to claim for processing (and the size of
 *   the entries array).
 * @param seq
 *   If not NULL, this is set to the value of the internal stage sequence number
 *   associated with the first entry returned.
 * @param block
 *   If this is true, the function blocks until num_entries slots are available
 *   to process. If false, then the function claims as many entries as
 *   currently possible.
 *
 * @return
 *   The number of entries copied in to the entries array.
 */
uint32_t
opdl_stage_claim_copy(struct opdl_stage *s, void *entries,
		uint32_t num_entries, uint32_t *seq, bool block);

/**
 * This function must be called when a stage has finished its processing of
 * entries, to make them available to any dependent stages. All entries that are
 * claimed by the calling thread in the stage will be disclaimed. It is possible
 * to claim multiple batches before disclaiming. For performance reasons, this
 * function does not check input parameters.
 *
 * @param s
 *   The opdl_ring stage in which to disclaim all claimed entries.
 *
 * @param block
 *   Entries are always made available to a stage in the same order that they
 *   were input in the stage. If a stage is multithread safe, this may mean that
 *   full disclaiming of a batch of entries can not be considered complete until
 *   all earlier threads in the stage have disclaimed. If this parameter is true
 *   then the function blocks until all entries are fully disclaimed, otherwise
 *   it disclaims as many as currently possible, with non fully disclaimed
 *   batches stored until the next call to a claim or disclaim function for this
 *   stage on this thread.
 *
 *   If a thread is not going to process any more entries in this stage, it
 *   *must* first call this function with this parameter set to true to ensure
 *   it does not block the entire opdl_ring.
 *
 *   In a single threaded stage, this parameter has no effect.
 */
int
opdl_stage_disclaim(struct opdl_stage *s, uint32_t num_entries,
		bool block);

/**
 * This function can be called when a stage has finished its processing of
 * entries, to make them available to any dependent stages. The difference
 * between this function and opdl_stage_disclaim() is that here only a
 * portion of entries are disclaimed, not all of them. For performance reasons,
 * this function does not check input parameters.
 *
 * @param s
 *   The opdl_ring stage in which to disclaim entries.
 *
 * @param num_entries
 *   The number of entries to disclaim.
 *
 * @param block
 *   Entries are always made available to a stage in the same order that they
 *   were input in the stage. If a stage is multithread safe, this may mean that
 *   full disclaiming of a batch of entries can not be considered complete until
 *   all earlier threads in the stage have disclaimed. If this parameter is true
 *   then the function blocks until the specified number of entries has been
 *   disclaimed (or there are no more entries to disclaim). Otherwise it
 *   disclaims as many claims as currently possible and an attempt to disclaim
 *   them is made the next time a claim or disclaim function for this stage on
 *   this thread is called.
 *
 *   In a single threaded stage, this parameter has no effect.
 */
void
opdl_stage_disclaim_n(struct opdl_stage *s, uint32_t num_entries,
		bool block);

/**
 * Check how many entries can be input.
 *
 * @param t
 *   The opdl_ring instance to check.
 *
 * @return
 *   The number of new entries currently allowed to be input.
 */
uint32_t
opdl_ring_available(struct opdl_ring *t);

/**
 * Check how many entries can be processed in a stage.
 *
 * @param s
 *   The stage to check.
 *
 * @return
 *   The number of entries currently available to be processed in this stage.
 */
uint32_t
opdl_stage_available(struct opdl_stage *s);

/**
 * Check how many entries are available to be processed.
 *
 * NOTE : DOES NOT CHANGE ANY STATE WITHIN THE STAGE
 *
 * @param s
 *   The stage to check.
 *
 * @param num_entries
 *   The number of entries to check for availability.
 *
 * @return
 *   The number of entries currently available to be processed in this stage.
 */
uint32_t
opdl_stage_find_num_available(struct opdl_stage *s, uint32_t num_entries);

/**
 * Create empty stage instance and return the pointer.
 *
 * @param t
 *   The pointer of  opdl_ring.
 *
 * @param threadsafe
 *    enable multiple thread or not.
 * @return
 *   The pointer of one empty stage instance.
 */
struct opdl_stage *
opdl_stage_create(struct opdl_ring *t,  bool threadsafe);


/**
 * Set the internal queue id for each stage instance.
 *
 * @param s
 *   The pointer of  stage instance.
 *
 * @param queue_id
 *    The value of internal queue id.
 */
void
opdl_stage_set_queue_id(struct opdl_stage *s,
		uint32_t queue_id);

/**
 * Prints information on opdl_ring instance and all its stages
 *
 * @param t
 *   The stage to print info on.
 * @param f
 *   Where to print the info.
 */
void
opdl_ring_dump(const struct opdl_ring *t, FILE *f);

/**
 * Blocks until all entries in a opdl_ring have been processed by all stages.
 *
 * @param t
 *   The opdl_ring instance to flush.
 */
void
opdl_ring_flush(struct opdl_ring *t);

/**
 * Deallocates all resources used by a opdl_ring instance
 *
 * @param t
 *   The opdl_ring instance to free.
 */
void
opdl_ring_free(struct opdl_ring *t);

/**
 * Search for a opdl_ring by its name
 *
 * @param name
 *   The name of the opdl_ring.
 * @return
 *   The pointer to the opdl_ring matching the name, or NULL if not found.
 *
 */
struct opdl_ring *
opdl_ring_lookup(const char *name);

/**
 * Set a opdl_stage to threadsafe variable.
 *
 * @param s
 *   The opdl_stage.
 * @param threadsafe
 *   Threadsafe value.
 */
void
opdl_ring_set_stage_threadsafe(struct opdl_stage *s, bool threadsafe);


/**
 * Compare the event descriptor with original version in the ring.
 * if key field event descriptor is changed by application, then
 * update the slot in the ring otherwise do nothing with it.
 * the key field is flow_id, prioirty, mbuf, impl_opaque
 *
 * @param s
 *   The opdl_stage.
 * @param ev
 *   pointer of the event descriptor.
 * @param index
 *   index of the event descriptor.
 * @param atomic
 *   queue type associate with the stage.
 * @return
 *   if the evevnt key field is changed compare with previous record.
 */

bool
opdl_ring_cas_slot(struct opdl_stage *s, const struct rte_event *ev,
		uint32_t index, bool atomic);

#ifdef __cplusplus
}
#endif

#endif  /* _OPDL_H_ */
