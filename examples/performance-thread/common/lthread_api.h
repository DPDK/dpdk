/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 Intel Corporation.
 * Copyright 2012 Hasan Alayli <halayli@gmail.com>
 */
/**
 *  @file lthread_api.h
 *
 *  @warning
 *  @b EXPERIMENTAL: this API may change without prior notice
 *
 *  This file contains the public API for the L-thread subsystem
 *
 *  The L_thread subsystem provides a simple cooperative scheduler to
 *  enable arbitrary functions to run as cooperative threads within a
 * single P-thread.
 *
 * The subsystem provides a P-thread like API that is intended to assist in
 * reuse of legacy code written for POSIX p_threads.
 *
 * The L-thread subsystem relies on cooperative multitasking, as such
 * an L-thread must possess frequent rescheduling points. Often these
 * rescheduling points are provided transparently when the application
 * invokes an L-thread API.
 *
 * In some applications it is possible that the program may enter a loop the
 * exit condition for which depends on the action of another thread or a
 * response from hardware. In such a case it is necessary to yield the thread
 * periodically in the loop body, to allow other threads an opportunity to
 * run. This can be done by inserting a call to lthread_yield() or
 * lthread_sleep(n) in the body of the loop.
 *
 * If the application makes expensive / blocking system calls or does other
 * work that would take an inordinate amount of time to complete, this will
 * stall the cooperative scheduler resulting in very poor performance.
 *
 * In such cases an L-thread can be migrated temporarily to another scheduler
 * running in a different P-thread on another core. When the expensive or
 * blocking operation is completed it can be migrated back to the original
 * scheduler.  In this way other threads can continue to run on the original
 * scheduler and will be completely unaffected by the blocking behaviour.
 * To migrate an L-thread to another scheduler the API lthread_set_affinity()
 * is provided.
 *
 * If L-threads that share data are running on the same core it is possible
 * to design programs where mutual exclusion mechanisms to protect shared data
 * can be avoided. This is due to the fact that the cooperative threads cannot
 * preempt each other.
 *
 * There are two cases where mutual exclusion mechanisms are necessary.
 *
 *  a) Where the L-threads sharing data are running on different cores.
 *  b) Where code must yield while updating data shared with another thread.
 *
 * The L-thread subsystem provides a set of mutex APIs to help with such
 * scenarios, however excessive reliance on on these will impact performance
 * and is best avoided if possible.
 *
 * L-threads can synchronise using a fast condition variable implementation
 * that supports signal and broadcast. An L-thread running on any core can
 * wait on a condition.
 *
 * L-threads can have L-thread local storage with an API modelled on either the
 * P-thread get/set specific API or using PER_LTHREAD macros modelled on the
 * RTE_PER_LCORE macros. Alternatively a simple user data pointer may be set
 * and retrieved from a thread.
 */
#ifndef LTHREAD_H
#define LTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <rte_cycles.h>


struct lthread;
struct lthread_cond;
struct lthread_mutex;

struct lthread_condattr;
struct lthread_mutexattr;

typedef void *(*lthread_func_t) (void *);

/*
 * Define the size of stack for an lthread
 * Then this is the size that will be allocated on lthread creation
 * This is a fixed size and will not grow.
 */
#define LTHREAD_MAX_STACK_SIZE (1024*64)

/**
 * Define the maximum number of TLS keys that can be created
 *
 */
#define LTHREAD_MAX_KEYS 1024

/**
 * Define the maximum number of attempts to destroy an lthread's
 * TLS data on thread exit
 */
#define LTHREAD_DESTRUCTOR_ITERATIONS 4


/**
 * Define the maximum number of lcores that will support lthreads
 */
#define LTHREAD_MAX_LCORES RTE_MAX_LCORE

/**
 * How many lthread objects to pre-allocate as the system grows
 * applies to lthreads + stacks, TLS, mutexs, cond vars.
 *
 * @see _lthread_alloc()
 * @see _cond_alloc()
 * @see _mutex_alloc()
 *
 */
#define LTHREAD_PREALLOC 100

/**
 * Set the number of schedulers in the system.
 *
 * This function may optionally be called before starting schedulers.
 *
 * If the number of schedulers is not set, or set to 0 then each scheduler
 * will begin scheduling lthreads immediately it is started.

 * If the number of schedulers is set to greater than 0, then each scheduler
 * will wait until all schedulers have started before beginning to schedule
 * lthreads.
 *
 * If an application wishes to have threads migrate between cores using
 * lthread_set_affinity(), or join threads running on other cores using
 * lthread_join(), then it is prudent to set the number of schedulers to ensure
 * that all schedulers are initialised beforehand.
 *
 * @param num
 *  the number of schedulers in the system
 * @return
 * the number of schedulers in the system
 */
int lthread_num_schedulers_set(int num);

/**
 * Return the number of schedulers currently running
 * @return
 *  the number of schedulers in the system
 */
int lthread_active_schedulers(void);

/**
  * Shutdown the specified scheduler
  *
  *  This function tells the specified scheduler to
  *  exit if/when there is no more work to do.
  *
  *  Note that although the scheduler will stop
  *  resources are not freed.
  *
  * @param lcore
  *	The lcore of the scheduler to shutdown
  *
  * @return
  *  none
  */
void lthread_scheduler_shutdown(unsigned lcore);

/**
  * Shutdown all schedulers
  *
  *  This function tells all schedulers  including the current scheduler to
  *  exit if/when there is no more work to do.
  *
  *  Note that although the schedulers will stop
  *  resources are not freed.
  *
  * @return
  *  none
  */
void lthread_scheduler_shutdown_all(void);

/**
  * Run the lthread scheduler
  *
  *  Runs the lthread scheduler.
  *  This function returns only if/when all lthreads have exited.
  *  This function must be the main loop of an EAL thread.
  *
  * @return
  *	 none
  */

void lthread_run(void);

/**
  * Create an lthread
  *
  *  Creates an lthread and places it in the ready queue on a particular
  *  lcore.
  *
  *  If no scheduler exists yet on the curret lcore then one is created.
  *
  * @param new_lt
  *  Pointer to an lthread pointer that will be initialized
  * @param lcore
  *  the lcore the thread should be started on or the current clore
  *    -1 the current lcore
  *    0 - LTHREAD_MAX_LCORES any other lcore
  * @param lthread_func
  *  Pointer to the function the for the thread to run
  * @param arg
  *  Pointer to args that will be passed to the thread
  *
  * @return
  *	 0    success
  *	 EAGAIN  no resources available
  *	 EINVAL  NULL thread or function pointer, or lcore_id out of range
  */
int
lthread_create(struct lthread **new_lt,
		int lcore, lthread_func_t func, void *arg);

/**
  * Cancel an lthread
  *
  *  Cancels an lthread and causes it to be terminated
  *  If the lthread is detached it will be freed immediately
  *  otherwise its resources will not be released until it is joined.
  *
  * @param new_lt
  *  Pointer to an lthread that will be cancelled
  *
  * @return
  *	 0    success
  *	 EINVAL  thread was NULL
  */
int lthread_cancel(struct lthread *lt);

/**
  * Join an lthread
  *
  *  Joins the current thread with the specified lthread, and waits for that
  *  thread to exit.
  *  Passes an optional pointer to collect returned data.
  *
  * @param lt
  *  Pointer to the lthread to be joined
  * @param ptr
  *  Pointer to pointer to collect returned data
  *
0  * @return
  *  0    success
  *  EINVAL lthread could not be joined.
  */
int lthread_join(struct lthread *lt, void **ptr);

/**
  * Detach an lthread
  *
  * Detaches the current thread
  * On exit a detached lthread will be freed immediately and will not wait
  * to be joined. The default state for a thread is not detached.
  *
  * @return
  *  none
  */
void lthread_detach(void);

/**
  *  Exit an lthread
  *
  * Terminate the current thread, optionally return data.
  * The data may be collected by lthread_join()
  *
  * After calling this function the lthread will be suspended until it is
  * joined. After it is joined then its resources will be freed.
  *
  * @param ptr
  *  Pointer to pointer to data to be returned
  *
  * @return
  *  none
  */
void lthread_exit(void *val);

/**
  * Cause the current lthread to sleep for n nanoseconds
  *
  * The current thread will be suspended until the specified time has elapsed
  * or has been exceeded.
  *
  * Execution will switch to the next lthread that is ready to run
  *
  * @param nsecs
  *  Number of nanoseconds to sleep
  *
  * @return
  *  none
  */
void lthread_sleep(uint64_t nsecs);

/**
  * Cause the current lthread to sleep for n cpu clock ticks
  *
  *  The current thread will be suspended until the specified time has elapsed
  *  or has been exceeded.
  *
  *	 Execution will switch to the next lthread that is ready to run
  *
  * @param clks
  *  Number of clock ticks to sleep
  *
  * @return
  *  none
  */
void lthread_sleep_clks(uint64_t clks);

/**
  * Yield the current lthread
  *
  *  The current thread will yield and execution will switch to the
  *  next lthread that is ready to run
  *
  * @return
  *  none
  */
void lthread_yield(void);

/**
  * Migrate the current thread to another scheduler
  *
  *  This function migrates the current thread to another scheduler.
  *  Execution will switch to the next lthread that is ready to run on the
  *  current scheduler. The current thread will be resumed on the new scheduler.
  *
  * @param lcore
  *	The lcore to migrate to
  *
  * @return
  *  0   success we are now running on the specified core
  *  EINVAL the destination lcore was not valid
  */
int lthread_set_affinity(unsigned lcore);

/**
  * Return the current lthread
  *
  *  Returns the current lthread
  *
  * @return
  *  pointer to the current lthread
  */
struct lthread
*lthread_current(void);

/**
  * Associate user data with an lthread
  *
  *  This function sets a user data pointer in the current lthread
  *  The pointer can be retrieved with lthread_get_data()
  *  It is the users responsibility to allocate and free any data referenced
  *  by the user pointer.
  *
  * @param data
  *  pointer to user data
  *
  * @return
  *  none
  */
void lthread_set_data(void *data);

/**
  * Get user data for the current lthread
  *
  *  This function returns a user data pointer for the current lthread
  *  The pointer must first be set with lthread_set_data()
  *  It is the users responsibility to allocate and free any data referenced
  *  by the user pointer.
  *
  * @return
  *  pointer to user data
  */
void
*lthread_get_data(void);

struct lthread_key;
typedef void (*tls_destructor_func) (void *);

/**
  * Create a key for lthread TLS
  *
  *  This function is modelled on pthread_key_create
  *  It creates a thread-specific data key visible to all lthreads on the
  *  current scheduler.
  *
  *  Key values may be used to locate thread-specific data.
  *  The same key value	may be used by different threads, the values bound
  *  to the key by	lthread_setspecific() are maintained on	a per-thread
  *  basis and persist for the life of the calling thread.
  *
  *  An	optional destructor function may be associated with each key value.
  *  At	thread exit, if	a key value has	a non-NULL destructor pointer, and the
  *  thread has	a non-NULL value associated with the key, the function pointed
  *  to	is called with the current associated value as its sole	argument.
  *
  * @param key
  *   Pointer to the key to be created
  * @param destructor
  *   Pointer to destructor function
  *
  * @return
  *  0 success
  *  EINVAL the key ptr was NULL
  *  EAGAIN no resources available
  */
int lthread_key_create(unsigned int *key, tls_destructor_func destructor);

/**
  * Delete key for lthread TLS
  *
  *  This function is modelled on pthread_key_delete().
  *  It deletes a thread-specific data key previously returned by
  *  lthread_key_create().
  *  The thread-specific data values associated with the key need not be NULL
  *  at the time that lthread_key_delete is called.
  *  It is the responsibility of the application to free any application
  *  storage or perform any cleanup actions for data structures related to the
  *  deleted key. This cleanup can be done either before or after
  * lthread_key_delete is called.
  *
  * @param key
  *  The key to be deleted
  *
  * @return
  *  0 Success
  *  EINVAL the key was invalid
  */
int lthread_key_delete(unsigned int key);

/**
  * Get lthread TLS
  *
  *  This function is modelled on pthread_get_specific().
  *  It returns the value currently bound to the specified key on behalf of the
  *  calling thread. Calling lthread_getspecific() with a key value not
  *  obtained from lthread_key_create() or after key has been deleted with
  *  lthread_key_delete() will result in undefined behaviour.
  *  lthread_getspecific() may be called from a thread-specific data destructor
  *  function.
  *
  * @param key
  *  The key for which data is requested
  *
  * @return
  *  Pointer to the thread specific data associated with that key
  *  or NULL if no data has been set.
  */
void
*lthread_getspecific(unsigned int key);

/**
  * Set lthread TLS
  *
  *  This function is modelled on pthread_set_sepcific()
  *  It associates a thread-specific value with a key obtained via a previous
  *  call to lthread_key_create().
  *  Different threads may bind different values to the same key. These values
  *  are typically pointers to dynamically allocated memory that have been
  *  reserved by the calling thread. Calling lthread_setspecific with a key
  *  value not obtained from lthread_key_create or after the key has been
  *  deleted with lthread_key_delete will result in undefined behaviour.
  *
  * @param key
  *  The key for which data is to be set
  * @param key
  *  Pointer to the user data
  *
  * @return
  *  0 success
  *  EINVAL the key was invalid
  */

int lthread_setspecific(unsigned int key, const void *value);

/**
 * The macros below provide an alternative mechanism to access lthread local
 *  storage.
 *
 * The macros can be used to declare define and access per lthread local
 * storage in a similar way to the RTE_PER_LCORE macros which control storage
 * local to an lcore.
 *
 * Memory for per lthread variables declared in this way is allocated when the
 * lthread is created and a pointer to this memory is stored in the lthread.
 * The per lthread variables are accessed via the pointer + the offset of the
 * particular variable.
 *
 * The total size of per lthread storage, and the variable offsets are found by
 * defining the variables in a unique global memory section, the start and end
 * of which is known. This global memory section is used only in the
 * computation of the addresses of the lthread variables, and is never actually
 * used to store any data.
 *
 * Due to the fact that variables declared this way may be scattered across
 * many files, the start and end of the section and variable offsets are only
 * known after linking, thus the computation of section size and variable
 * addresses is performed at run time.
 *
 * These macros are primarily provided to aid porting of code that makes use
 * of the existing RTE_PER_LCORE macros. In principle it would be more efficient
 * to gather all lthread local variables into a single structure and
 * set/retrieve a pointer to that struct using the alternative
 * lthread_data_set/get APIs.
 *
 * These macros are mutually exclusive with the lthread_data_set/get APIs.
 * If you define storage using these macros then the lthread_data_set/get APIs
 * will not perform as expected, the lthread_data_set API does nothing, and the
 * lthread_data_get API returns the start of global section.
 *
 */
/* start and end of per lthread section */
extern char __start_per_lt;
extern char __stop_per_lt;


#define RTE_DEFINE_PER_LTHREAD(type, name)                      \
__typeof__(type)__attribute((section("per_lt"))) per_lt_##name

/**
 * Macro to declare an extern per lthread variable "var" of type "type"
 */
#define RTE_DECLARE_PER_LTHREAD(type, name)                     \
extern __typeof__(type)__attribute((section("per_lt"))) per_lt_##name

/**
 * Read/write the per-lcore variable value
 */
#define RTE_PER_LTHREAD(name) ((typeof(per_lt_##name) *)\
((char *)lthread_get_data() +\
((char *) &per_lt_##name - &__start_per_lt)))

/**
  * Initialize a mutex
  *
  *  This function provides a mutual exclusion device, the need for which
  *  can normally be avoided in a cooperative multitasking environment.
  *  It is provided to aid porting of legacy code originally written for
  *   preemptive multitasking environments such as pthreads.
  *
  *  A mutex may be unlocked (not owned by any thread), or locked (owned by
  *  one thread).
  *
  *  A mutex can never be owned  by more than one thread simultaneously.
  *  A thread attempting to lock a mutex that is already locked by another
  *  thread is suspended until the owning thread unlocks the mutex.
  *
  *  lthread_mutex_init() initializes the mutex object pointed to by mutex
  *  Optional mutex attributes specified in mutexattr, are reserved for future
  *  use and are currently ignored.
  *
  *  If a thread calls lthread_mutex_lock() on the mutex, then if the mutex
  *  is currently unlocked,  it  becomes  locked  and  owned  by  the calling
  *  thread, and lthread_mutex_lock returns immediately. If the mutex is
  *  already locked by another thread, lthread_mutex_lock suspends the calling
  *  thread until the mutex is unlocked.
  *
  *  lthread_mutex_trylock behaves identically to rte_thread_mutex_lock, except
  *  that it does not block the calling  thread  if the mutex is already locked
  *  by another thread.
  *
  *  lthread_mutex_unlock() unlocks the specified mutex. The mutex is assumed
  *  to be locked and owned by the calling thread.
  *
  *  lthread_mutex_destroy() destroys a	mutex object, freeing its resources.
  *  The mutex must be unlocked with nothing blocked on it before calling
  *  lthread_mutex_destroy.
  *
  * @param name
  *  Optional pointer to string describing the mutex
  * @param mutex
  *  Pointer to pointer to the mutex to be initialized
  * @param attribute
  *  Pointer to attribute - unused reserved
  *
  * @return
  *  0 success
  *  EINVAL mutex was not a valid pointer
  *  EAGAIN insufficient resources
  */

int
lthread_mutex_init(char *name, struct lthread_mutex **mutex,
		   const struct lthread_mutexattr *attr);

/**
  * Destroy a mutex
  *
  *  This function destroys the specified mutex freeing its resources.
  *  The mutex must be unlocked before calling lthread_mutex_destroy.
  *
  * @see lthread_mutex_init()
  *
  * @param mutex
  *  Pointer to pointer to the mutex to be initialized
  *
  * @return
  *  0 success
  *  EINVAL mutex was not an initialized mutex
  *  EBUSY mutex was still in use
  */
int lthread_mutex_destroy(struct lthread_mutex *mutex);

/**
  * Lock a mutex
  *
  *  This function attempts to lock a mutex.
  *  If a thread calls lthread_mutex_lock() on the mutex, then if the mutex
  *  is currently unlocked,  it  becomes  locked  and  owned  by  the calling
  *  thread, and lthread_mutex_lock returns immediately. If the mutex is
  *  already locked by another thread, lthread_mutex_lock suspends the calling
  *  thread until the mutex is unlocked.
  *
  * @see lthread_mutex_init()
  *
  * @param mutex
  *  Pointer to pointer to the mutex to be initialized
  *
  * @return
  *  0 success
  *  EINVAL mutex was not an initialized mutex
  *  EDEADLOCK the mutex was already owned by the calling thread
  */

int lthread_mutex_lock(struct lthread_mutex *mutex);

/**
  * Try to lock a mutex
  *
  *  This function attempts to lock a mutex.
  *  lthread_mutex_trylock behaves identically to rte_thread_mutex_lock, except
  *  that it does not block the calling  thread  if the mutex is already locked
  *  by another thread.
  *
  *
  * @see lthread_mutex_init()
  *
  * @param mutex
  *  Pointer to pointer to the mutex to be initialized
  *
  * @return
  * 0 success
  * EINVAL mutex was not an initialized mutex
  * EBUSY the mutex was already locked by another thread
  */
int lthread_mutex_trylock(struct lthread_mutex *mutex);

/**
  * Unlock a mutex
  *
  * This function attempts to unlock the specified mutex. The mutex is assumed
  * to be locked and owned by the calling thread.
  *
  * The oldest of any threads blocked on the mutex is made ready and may
  * compete with any other running thread to gain the mutex, it fails it will
  *  be blocked again.
  *
  * @param mutex
  * Pointer to pointer to the mutex to be initialized
  *
  * @return
  *  0 mutex was unlocked
  *  EINVAL mutex was not an initialized mutex
  *  EPERM the mutex was not owned by the calling thread
  */

int lthread_mutex_unlock(struct lthread_mutex *mutex);

/**
  * Initialize a condition variable
  *
  *  This function initializes a condition variable.
  *
  *  Condition variables can be used to communicate changes in the state of data
  *  shared between threads.
  *
  * @see lthread_cond_wait()
  *
  * @param name
  *  Pointer to optional string describing the condition variable
  * @param c
  *  Pointer to pointer to the condition variable to be initialized
  * @param attr
  *  Pointer to optional attribute reserved for future use, currently ignored
  *
  * @return
  *  0 success
  *  EINVAL cond was not a valid pointer
  *  EAGAIN insufficient resources
  */
int
lthread_cond_init(char *name, struct lthread_cond **c,
		  const struct lthread_condattr *attr);

/**
  * Destroy a condition variable
  *
  *  This function destroys a condition variable that was created with
  *  lthread_cond_init() and releases its resources.
  *
  * @param cond
  *  Pointer to pointer to the condition variable to be destroyed
  *
  * @return
  *  0 Success
  *  EBUSY condition variable was still in use
  *  EINVAL was not an initialised condition variable
  */
int lthread_cond_destroy(struct lthread_cond *cond);

/**
  * Wait on a condition variable
  *
  *  The function blocks the current thread waiting on the condition variable
  *  specified by cond. The waiting thread unblocks only after another thread
  *  calls lthread_cond_signal, or lthread_cond_broadcast, specifying the
  *  same condition variable.
  *
  * @param cond
  *  Pointer to pointer to the condition variable to be waited on
  *
  * @param reserved
  *  reserved for future use
  *
  * @return
  *  0 The condition was signalled ( Success )
  *  EINVAL was not a an initialised condition variable
  */
int lthread_cond_wait(struct lthread_cond *c, uint64_t reserved);

/**
  * Signal a condition variable
  *
  *  The function unblocks one thread waiting for the condition variable cond.
  *  If no threads are waiting on cond, the rte_lthead_cond_signal() function
  *  has no effect.
  *
  * @param cond
  *  Pointer to pointer to the condition variable to be signalled
  *
  * @return
  *  0 The condition was signalled ( Success )
  *  EINVAL was not a an initialised condition variable
  */
int lthread_cond_signal(struct lthread_cond *c);

/**
  * Broadcast a condition variable
  *
  *  The function unblocks all threads waiting for the condition variable cond.
  *  If no threads are waiting on cond, the rte_lthead_cond_broadcast()
  *  function has no effect.
  *
  * @param cond
  *  Pointer to pointer to the condition variable to be signalled
  *
  * @return
  *  0 The condition was signalled ( Success )
  *  EINVAL was not a an initialised condition variable
  */
int lthread_cond_broadcast(struct lthread_cond *c);

#ifdef __cplusplus
}
#endif

#endif				/* LTHREAD_H */
