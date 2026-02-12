/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Red Hat, Inc.
 */

#ifndef RTE_LOCK_ANNOTATIONS_H
#define RTE_LOCK_ANNOTATIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RTE_ANNOTATE_LOCKS

#define __rte_lockable \
	__attribute__((lockable))

#define __rte_guarded_by(...) \
	__attribute__((guarded_by(__VA_ARGS__)))
#define __rte_guarded_var \
	__attribute__((guarded_var))

#define __rte_exclusive_locks_required(...) \
	__attribute__((exclusive_locks_required(__VA_ARGS__)))
#define __rte_exclusive_lock_function(...) \
	__attribute__((exclusive_lock_function(__VA_ARGS__)))
#define __rte_exclusive_trylock_function(ret, ...) \
	__attribute__((exclusive_trylock_function(ret, __VA_ARGS__)))
#define __rte_assert_exclusive_lock(...) \
	__attribute__((assert_exclusive_lock(__VA_ARGS__)))

#define __rte_shared_locks_required(...) \
	__attribute__((shared_locks_required(__VA_ARGS__)))
#define __rte_shared_lock_function(...) \
	__attribute__((shared_lock_function(__VA_ARGS__)))
#define __rte_shared_trylock_function(ret, ...) \
	__attribute__((shared_trylock_function(ret, __VA_ARGS__)))
#define __rte_assert_shared_lock(...) \
	__attribute__((assert_shared_lock(__VA_ARGS__)))

#define __rte_unlock_function(...) \
	__attribute__((unlock_function(__VA_ARGS__)))

#define __rte_locks_excluded(...) \
	__attribute__((locks_excluded(__VA_ARGS__)))


#if defined(__clang__) && __clang_major__ >= 11
#define __rte_capability(...) \
	__attribute__((capability(__VA_ARGS__)))
#else
#define __rte_capability(...) \
	__attribute__((capability("mutex")))
#endif

#define __rte_requires_capability(...) \
	__attribute__((requires_capability(__VA_ARGS__)))
#define __rte_acquire_capability(...) \
	__attribute__((acquire_capability(__VA_ARGS__)))
#define __rte_try_acquire_capability(ret, ...) \
	__attribute__((try_acquire_capability(ret, __VA_ARGS__)))
#define __rte_release_capability(...) \
	__attribute__((release_capability(__VA_ARGS__)))
#define __rte_assert_capability(...) \
	__attribute__((assert_capability(__VA_ARGS__)))

#define __rte_requires_shared_capability(...) \
	__attribute__((requires_shared_capability(__VA_ARGS__)))
#define __rte_acquire_shared_capability(...) \
	__attribute__((acquire_shared_capability(__VA_ARGS__)))
#define __rte_try_acquire_shared_capability(ret, ...) \
	__attribute__((try_acquire_shared_capability(ret, __VA_ARGS__)))
#define __rte_release_shared_capability(...) \
	__attribute__((release_shared_capability(__VA_ARGS__)))
#define __rte_assert_shared_capability(...) \
	__attribute__((assert_shared_capability(__VA_ARGS__)))

#define __rte_exclude_capability(...) \
	__attribute__((exclude_capability(__VA_ARGS__)))


#define __rte_no_thread_safety_analysis \
	__attribute__((no_thread_safety_analysis))

#else /* ! RTE_ANNOTATE_LOCKS */

#define __rte_lockable

#define __rte_guarded_by(...)
#define __rte_guarded_var

#define __rte_exclusive_locks_required(...)
#define __rte_exclusive_lock_function(...)
#define __rte_exclusive_trylock_function(...)
#define __rte_assert_exclusive_lock(...)

#define __rte_shared_locks_required(...)
#define __rte_shared_lock_function(...)
#define __rte_shared_trylock_function(...)
#define __rte_assert_shared_lock(...)

#define __rte_unlock_function(...)

#define __rte_locks_excluded(...)


#define __rte_capability(...)

#define __rte_requires_capability(...)
#define __rte_acquire_capability(...)
#define __rte_try_acquire_capability(...)
#define __rte_release_capability(...)
#define __rte_assert_capability(...)

#define __rte_requires_shared_capability(...)
#define __rte_acquire_shared_capability(...)
#define __rte_try_acquire_shared_capability(...)
#define __rte_release_shared_capability(...)
#define __rte_assert_shared_capability(...)

#define __rte_exclude_capability(...)


#define __rte_no_thread_safety_analysis

#endif /* RTE_ANNOTATE_LOCKS */

#ifdef __cplusplus
}
#endif

#endif /* RTE_LOCK_ANNOTATIONS_H */
