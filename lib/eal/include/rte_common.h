/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_COMMON_H_
#define _RTE_COMMON_H_

/**
 * @file
 *
 * Generic, commonly-used macro and inline function definitions
 * for DPDK.
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h>

#include <rte_compat.h>
#include <rte_config.h>

/* OS specific include */
#include <rte_os.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RTE_TOOLCHAIN_MSVC
#ifndef typeof
#define typeof __typeof__
#endif
#endif

#ifndef __cplusplus
#ifndef asm
#define asm __asm__
#endif
#endif

#ifdef RTE_TOOLCHAIN_MSVC
#ifdef __cplusplus
#define __extension__
#endif
#endif

/*
 * Macro __rte_constant checks if an expression's value can be determined at
 * compile time. It takes a single argument, the expression to test, and
 * returns 1 if the expression is a compile-time constant, and 0 otherwise.
 * For most compilers it uses built-in function __builtin_constant_p, but for
 * MSVC it uses a different method because MSVC does not have an equivalent
 * to __builtin_constant_p.
 *
 * The trick used with MSVC relies on the way null pointer constants interact
 * with the type of a ?: expression:
 * An integer constant expression with the value 0, or such an expression cast
 * to type void *, is called a null pointer constant.
 * If both the second and third operands (of the ?: expression) are pointers or
 * one is a null pointer constant and the other is a pointer, the result type
 * is a pointer to a type qualified with all the type qualifiers of the types
 * referenced by both operands. Furthermore, if both operands are pointers to
 * compatible types or to differently qualified versions of compatible types,
 * the result type is a pointer to an appropriately qualified version of the
 * composite type; if one operand is a null pointer constant, the result has
 * the type of the other operand; otherwise, one operand is a pointer to void
 * or a qualified version of void, in which case the result type is a pointer
 * to an appropriately qualified version of void.
 *
 * The _Generic keyword then checks the type of the expression
 * (void *) ((e) * 0ll). It matches this type against the types listed in the
 * _Generic construct:
 *     - If the type is int *, the result is 1.
 *     - If the type is void *, the result is 0.
 *
 * This explanation with some more details can be found at:
 * https://stackoverflow.com/questions/49480442/detecting-integer-constant-expressions-in-macros
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_constant(e) _Generic((1 ? (void *) ((e) * 0ll) : (int *) 0), int * : 1, void * : 0)
#else
#define __rte_constant(e) __extension__(__builtin_constant_p(e))
#endif

/*
 * RTE_TOOLCHAIN_GCC is defined if the target is built with GCC,
 * while a host application (like pmdinfogen) may have another compiler.
 * RTE_CC_IS_GNU is true if the file is compiled with GCC,
 * no matter it is a target or host application.
 */
#define RTE_CC_IS_GNU 0
#if defined __clang__
#define RTE_CC_CLANG
#elif defined __GNUC__
#define RTE_CC_GCC
#undef RTE_CC_IS_GNU
#define RTE_CC_IS_GNU 1
#endif
#if RTE_CC_IS_GNU
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 +	\
		__GNUC_PATCHLEVEL__)
#endif

/**
 * Force type alignment
 *
 * This macro should be used when alignment of a struct or union type
 * is required. For toolchain compatibility it should appear between
 * the {struct,union} keyword and tag. e.g.
 *
 *   struct __rte_aligned(8) tag { ... };
 *
 * If alignment of an object/variable is required then this macro should
 * not be used, instead prefer C11 alignas(a).
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_aligned(a) __declspec(align(a))
#else
#define __rte_aligned(a) __attribute__((__aligned__(a)))
#endif

#ifdef RTE_ARCH_STRICT_ALIGN
typedef uint64_t unaligned_uint64_t __rte_aligned(1);
typedef uint32_t unaligned_uint32_t __rte_aligned(1);
typedef uint16_t unaligned_uint16_t __rte_aligned(1);
#else
typedef uint64_t unaligned_uint64_t;
typedef uint32_t unaligned_uint32_t;
typedef uint16_t unaligned_uint16_t;
#endif

/**
 * @deprecated
 * @see __rte_packed_begin
 * @see __rte_packed_end
 *
 * Force a structure to be packed
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_packed RTE_DEPRECATED(__rte_packed)
#else
#define __rte_packed (RTE_DEPRECATED(__rte_packed) __attribute__((__packed__)))
#endif

/**
 * Force a structure to be packed
 * Usage:
 *     struct __rte_packed_begin mystruct { ... } __rte_packed_end;
 *     union __rte_packed_begin myunion { ... } __rte_packed_end;
 * Note: alignment attributes when present should precede __rte_packed_begin.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_packed_begin __pragma(pack(push, 1))
#define __rte_packed_end __pragma(pack(pop))
#else
#define __rte_packed_begin
#define __rte_packed_end __attribute__((__packed__))
#endif

/**
 * Macro to mark a type that is not subject to type-based aliasing rules
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_may_alias
#else
#define __rte_may_alias __attribute__((__may_alias__))
#endif

/******* Macro to mark functions and fields scheduled for removal *****/
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_deprecated
#define __rte_deprecated_msg(msg)
#else
#define __rte_deprecated	__attribute__((__deprecated__))
#define __rte_deprecated_msg(msg)	__attribute__((__deprecated__(msg)))
#endif

/**
 *  Macro to mark macros and defines scheduled for removal
 */
#if defined(RTE_CC_GCC) || defined(RTE_CC_CLANG)
#define RTE_PRAGMA(x)  _Pragma(#x)
#define RTE_PRAGMA_WARNING(w) RTE_PRAGMA(GCC warning #w)
#define RTE_DEPRECATED(x)  RTE_PRAGMA_WARNING(#x is deprecated)
#else
#define RTE_DEPRECATED(x)
#endif

/**
 * Macros to cause the compiler to remember the state of the diagnostics as of
 * each push, and restore to that point at each pop.
 */
#if !defined(RTE_TOOLCHAIN_MSVC)
#define __rte_diagnostic_push _Pragma("GCC diagnostic push")
#define __rte_diagnostic_pop  _Pragma("GCC diagnostic pop")
#else
#define __rte_diagnostic_push
#define __rte_diagnostic_pop
#endif

/**
 * Macro to disable compiler warnings about removing a type
 * qualifier from the target type.
 */
#if !defined(RTE_TOOLCHAIN_MSVC)
#define __rte_diagnostic_ignored_wcast_qual _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#else
#define __rte_diagnostic_ignored_wcast_qual
#endif

/**
 * Mark a function or variable to a weak reference.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_weak RTE_DEPRECATED(__rte_weak)
#else
#define __rte_weak RTE_DEPRECATED(__rte_weak) __attribute__((__weak__))
#endif

/**
 * Mark a function to be pure.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_pure
#else
#define __rte_pure __attribute__((pure))
#endif

/**
 * Force symbol to be generated even if it appears to be unused.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_used
#else
#define __rte_used __attribute__((used))
#endif

/*********** Macros to eliminate unused variable warnings ********/

/**
 * short definition to mark a function parameter unused
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_unused
#else
#define __rte_unused __attribute__((__unused__))
#endif

/**
 * Mark pointer as restricted with regard to pointer aliasing.
 */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#define __rte_restrict __restrict
#else
#define __rte_restrict restrict
#endif

/**
 * definition to mark a variable or function parameter as used so
 * as to avoid a compiler warning
 */
#define RTE_SET_USED(x) (void)(x)

/**
 * Check format string and its arguments at compile-time.
 *
 * GCC on Windows assumes MS-specific format string by default,
 * even if the underlying stdio implementation is ANSI-compliant,
 * so this must be overridden.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_format_printf(format_index, first_arg)
#else
#if RTE_CC_IS_GNU
#define __rte_format_printf(format_index, first_arg) \
	__attribute__((format(gnu_printf, format_index, first_arg)))
#else
#define __rte_format_printf(format_index, first_arg) \
	__attribute__((format(printf, format_index, first_arg)))
#endif
#endif

/**
 * Specify data or function section/segment.
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_section(name) \
	__pragma(data_seg(name)) __declspec(allocate(name))
#else
#define __rte_section(name) \
	__attribute__((section(name)))
#endif

/**
 * Tells compiler that the function returns a value that points to
 * memory, where the size is given by the one or two arguments.
 * Used by compiler to validate object size.
 */
#if defined(RTE_CC_GCC) || defined(RTE_CC_CLANG)
#define __rte_alloc_size(...) \
	__attribute__((alloc_size(__VA_ARGS__)))
#else
#define __rte_alloc_size(...)
#endif

/**
 * Tells the compiler that the function returns a value that points to
 * memory aligned by a function argument.
 *
 * Note: not enabled on Clang because it warns if align argument is zero.
 */
#if defined(RTE_CC_GCC)
#define __rte_alloc_align(argno) \
	__attribute__((alloc_align(argno)))
#else
#define __rte_alloc_align(argno)
#endif

/**
 * Tells the compiler this is a function like malloc and that the pointer
 * returned cannot alias any other pointer (ie new memory).
 */
#if defined(RTE_CC_GCC) || defined(RTE_CC_CLANG)
#define __rte_malloc __attribute__((__malloc__))
#else
#define __rte_malloc
#endif

/**
 * With recent GCC versions also able to track that proper
 * deallocator function is used for this pointer.
 */
#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 110000)
#define __rte_dealloc(dealloc, argno) \
	__attribute__((malloc(dealloc, argno)))
#else
#define __rte_dealloc(dealloc, argno)
#endif

#define RTE_PRIORITY_LOG 101
#define RTE_PRIORITY_BUS 110
#define RTE_PRIORITY_CLASS 120
#define RTE_PRIORITY_LAST 65535

#define RTE_PRIO(prio) \
	RTE_PRIORITY_ ## prio

/**
 * Run function before main() with high priority.
 *
 * @param func
 *   Constructor function.
 * @param prio
 *   Priority number must be above 100.
 *   Lowest number is the first to run.
 */
#ifndef RTE_INIT_PRIO /* Allow to override from EAL */
#ifndef RTE_TOOLCHAIN_MSVC
#define RTE_INIT_PRIO(func, prio) \
static void __attribute__((constructor(RTE_PRIO(prio)), used)) func(void)
#else
/* definition from the Microsoft CRT */
typedef int(__cdecl *_PIFV)(void);

#define CTOR_SECTION_LOG ".CRT$XIB"
#define CTOR_SECTION_BUS ".CRT$XIC"
#define CTOR_SECTION_CLASS ".CRT$XID"
#define CTOR_SECTION_LAST ".CRT$XIY"

#define CTOR_PRIORITY_TO_SECTION(priority) CTOR_SECTION_ ## priority

#define RTE_INIT_PRIO(name, priority) \
	static void name(void); \
	static int __cdecl name ## _thunk(void) { name(); return 0; } \
	__pragma(const_seg(CTOR_PRIORITY_TO_SECTION(priority))) \
	__declspec(allocate(CTOR_PRIORITY_TO_SECTION(priority))) \
	    _PIFV name ## _pointer = &name ## _thunk; \
	__pragma(const_seg()) \
	static void name(void)
#endif
#endif

/**
 * Run function before main() with low priority.
 *
 * The constructor will be run after prioritized constructors.
 *
 * @param func
 *   Constructor function.
 */
#define RTE_INIT(func) \
	RTE_INIT_PRIO(func, LAST)

/**
 * Run after main() with low priority.
 *
 * @param func
 *   Destructor function name.
 * @param prio
 *   Priority number must be above 100.
 *   Lowest number is the last to run.
 */
#ifndef RTE_FINI_PRIO /* Allow to override from EAL */
#ifndef RTE_TOOLCHAIN_MSVC
#define RTE_FINI_PRIO(func, prio) \
static void __attribute__((destructor(RTE_PRIO(prio)), used)) func(void)
#else
#define DTOR_SECTION_LOG "mydtor$B"
#define DTOR_SECTION_BUS "mydtor$C"
#define DTOR_SECTION_CLASS "mydtor$D"
#define DTOR_SECTION_LAST "mydtor$Y"

#define DTOR_PRIORITY_TO_SECTION(priority) DTOR_SECTION_ ## priority

#define RTE_FINI_PRIO(name, priority) \
	static void name(void); \
	__pragma(const_seg(DTOR_PRIORITY_TO_SECTION(priority))) \
	__declspec(allocate(DTOR_PRIORITY_TO_SECTION(priority))) void *name ## _pointer = &name; \
	__pragma(const_seg()) \
	static void name(void)
#endif
#endif

/**
 * Run after main() with high priority.
 *
 * The destructor will be run *before* prioritized destructors.
 *
 * @param func
 *   Destructor function name.
 */
#define RTE_FINI(func) \
	RTE_FINI_PRIO(func, LAST)

/**
 * Hint never returning function
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_noreturn
#else
#define __rte_noreturn __attribute__((noreturn))
#endif

/**
 * Hint point in program never reached
 */
#if defined(RTE_TOOLCHAIN_GCC) || defined(RTE_TOOLCHAIN_CLANG)
#define __rte_unreachable() __extension__(__builtin_unreachable())
#else
#define __rte_unreachable() __assume(0)
#endif

/**
 * Issue a warning in case the function's return value is ignored.
 *
 * The use of this attribute should be restricted to cases where
 * ignoring the marked function's return value is almost always a
 * bug. With GCC, some effort is required to make clear that ignoring
 * the return value is intentional. The usual void-casting method to
 * mark something unused as used does not suppress the warning with
 * this compiler.
 *
 * @code{.c}
 * __rte_warn_unused_result int foo();
 *
 * void ignore_foo_result(void) {
 *         foo(); // generates a warning with all compilers
 *
 *         (void)foo(); // still generates the warning with GCC (but not clang)
 *
 *         int unused __rte_unused;
 *         unused = foo(); // does the trick with all compilers
 *  }
 * @endcode
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_warn_unused_result
#else
#define __rte_warn_unused_result __attribute__((warn_unused_result))
#endif

/**
 * Force a function to be inlined
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_always_inline __forceinline
#else
#define __rte_always_inline inline __attribute__((always_inline))
#endif

/**
 * Force a function to be noinlined
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_noinline __declspec(noinline)
#else
#define __rte_noinline __attribute__((noinline))
#endif

/**
 * Hint function in the hot path
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_hot
#else
#define __rte_hot __attribute__((hot))
#endif

/**
 * Hint function in the cold path
 */
#ifdef RTE_TOOLCHAIN_MSVC
#define __rte_cold
#else
#define __rte_cold __attribute__((cold))
#endif

/**
 * Hint precondition
 *
 * @warning Depending on the compiler, any code in ``condition`` might be executed.
 * This currently only occurs with GCC prior to version 13.
 */
#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 130000)
#define __rte_assume(condition) __attribute__((assume(condition)))
#elif defined(RTE_TOOLCHAIN_GCC)
#define __rte_assume(condition) do { if (!(condition)) __rte_unreachable(); } while (0)
#elif defined(RTE_TOOLCHAIN_CLANG)
#define __rte_assume(condition) __extension__(__builtin_assume(condition))
#else
#define __rte_assume(condition) __assume(condition)
#endif

/**
 * Disable AddressSanitizer on some code
 */
#ifdef RTE_MALLOC_ASAN
#ifdef RTE_CC_CLANG
#define __rte_no_asan __attribute__((no_sanitize("address", "hwaddress")))
#else
#define __rte_no_asan __attribute__((no_sanitize_address))
#endif
#else /* ! RTE_MALLOC_ASAN */
#define __rte_no_asan
#endif

/*********** Macros for pointer arithmetic ********/

/**
 * add a byte-value offset to a pointer
 */
#define RTE_PTR_ADD(ptr, x) ((void*)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define RTE_PTR_SUB(ptr, x) ((void *)((uintptr_t)(ptr) - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define RTE_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/*********** Macros for casting pointers ********/

/**
 * Macro to discard qualifiers (such as const, volatile, restrict) from a pointer,
 * without the compiler emitting a warning.
 */
#define RTE_PTR_UNQUAL(X) ((void *)(uintptr_t)(X))

/**
 * Macro to cast a pointer to a specific type,
 * without the compiler emitting a warning about discarding qualifiers.
 *
 * @warning
 * When casting a pointer to point to a larger type, the resulting pointer may
 * be misaligned, which results in undefined behavior.
 * E.g.:
 *
 * struct s {
 *       uint16_t a;
 *       uint8_t  b;
 *       uint8_t  c;
 *       uint8_t  d;
 *   } v;
 *   uint16_t * p = RTE_CAST_PTR(uint16_t *, &v.c); // "p" is not 16 bit aligned!
 */
#define RTE_CAST_PTR(type, ptr) ((type)(uintptr_t)(ptr))

/**
 * Workaround to cast a const field of a structure to non-const type.
 */
#define RTE_CAST_FIELD(var, field, type) \
	(*(type *)((uintptr_t)(var) + offsetof(typeof(*(var)), field)))

/*********** Macros/static functions for doing alignment ********/


/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no higher than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_FLOOR(ptr, align) \
	((typeof(ptr))RTE_ALIGN_FLOOR((uintptr_t)(ptr), align))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define RTE_ALIGN_FLOOR(val, align) \
	(typeof(val))((val) & (~((typeof(val))((align) - 1))))

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_CEIL(ptr, align) \
	RTE_PTR_ALIGN_FLOOR((typeof(ptr))RTE_PTR_ADD(ptr, (align) - 1), align)

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 */
#define RTE_ALIGN_CEIL(val, align) \
	RTE_ALIGN_FLOOR(((val) + ((typeof(val)) (align) - 1)), align)

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as RTE_PTR_ALIGN_CEIL
 */
#define RTE_PTR_ALIGN(ptr, align) RTE_PTR_ALIGN_CEIL(ptr, align)

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as RTE_ALIGN_CEIL
 */
#define RTE_ALIGN(val, align) RTE_ALIGN_CEIL(val, align)

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no lower
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_CEIL(v, mul) \
	((((v) + (typeof(v))(mul) - 1) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no higher
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_FLOOR(v, mul) \
	(((v) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align value to the nearest multiple of the given value.
 * The resultant value might be greater than or less than the first parameter
 * whichever difference is the lowest.
 */
#define RTE_ALIGN_MUL_NEAR(v, mul)				\
	__extension__ ({					\
		typeof(v) ceil = RTE_ALIGN_MUL_CEIL(v, mul);	\
		typeof(v) floor = RTE_ALIGN_MUL_FLOOR(v, mul);	\
		(ceil - (v)) > ((v) - floor) ? floor : ceil;	\
	})

/**
 * Checks if a pointer is aligned to a given power-of-two value
 *
 * @param ptr
 *   The pointer whose alignment is to be checked
 * @param align
 *   The power-of-two value to which the ptr should be aligned
 *
 * @return
 *   True(1) where the pointer is correctly aligned, false(0) otherwise
 */
static inline int
rte_is_aligned(const void * const __rte_restrict ptr, const unsigned int align)
{
	return ((uintptr_t)ptr & (align - 1)) == 0;
}

/*********** Macros for compile type checks ********/

/* Workaround for toolchain issues with missing C11 macro in FreeBSD */
#if !defined(static_assert) && !defined(__cplusplus)
#define	static_assert	_Static_assert
#endif

/**
 * Triggers an error at compilation time if the condition is true.
 *
 * The do { } while(0) exists to workaround a bug in clang (#55821)
 * where it would not handle _Static_assert in a switch case.
 */
#define RTE_BUILD_BUG_ON(condition) do { static_assert(!(condition), #condition); } while (0)

/*********** Cache line related macros ********/

/** Cache line mask. */
#define RTE_CACHE_LINE_MASK (RTE_CACHE_LINE_SIZE-1)

/** Return the first cache-aligned value greater or equal to size. */
#define RTE_CACHE_LINE_ROUNDUP(size) RTE_ALIGN_CEIL(size, RTE_CACHE_LINE_SIZE)

/** Cache line size in terms of log2 */
#if RTE_CACHE_LINE_SIZE == 64
#define RTE_CACHE_LINE_SIZE_LOG2 6
#elif RTE_CACHE_LINE_SIZE == 128
#define RTE_CACHE_LINE_SIZE_LOG2 7
#else
#error "Unsupported cache line size"
#endif

/** Minimum Cache line size. */
#define RTE_CACHE_LINE_MIN_SIZE 64

/** Force alignment to cache line. */
#define __rte_cache_aligned __rte_aligned(RTE_CACHE_LINE_SIZE)

/** Force minimum cache line alignment. */
#define __rte_cache_min_aligned __rte_aligned(RTE_CACHE_LINE_MIN_SIZE)

#define _RTE_CACHE_GUARD_HELPER2(unique) \
	alignas(RTE_CACHE_LINE_SIZE) \
	char cache_guard_ ## unique[RTE_CACHE_LINE_SIZE * RTE_CACHE_GUARD_LINES]
#define _RTE_CACHE_GUARD_HELPER1(unique) _RTE_CACHE_GUARD_HELPER2(unique)
/**
 * Empty cache lines, to guard against false sharing-like effects
 * on systems with a next-N-lines hardware prefetcher.
 *
 * Use as spacing between data accessed by different lcores,
 * to prevent cache thrashing on hardware with speculative prefetching.
 */
#define RTE_CACHE_GUARD _RTE_CACHE_GUARD_HELPER1(__COUNTER__)

/*********** PA/IOVA type definitions ********/

/** Physical address */
typedef uint64_t phys_addr_t;
#define RTE_BAD_PHYS_ADDR ((phys_addr_t)-1)

/**
 * IO virtual address type.
 * When the physical addressing mode (IOVA as PA) is in use,
 * the translation from an IO virtual address (IOVA) to a physical address
 * is a direct mapping, i.e. the same value.
 * Otherwise, in virtual mode (IOVA as VA), an IOMMU may do the translation.
 */
typedef uint64_t rte_iova_t;
#define RTE_BAD_IOVA ((rte_iova_t)-1)

/*********** Structure alignment markers ********/

#ifndef RTE_TOOLCHAIN_MSVC

/** Generic marker for any place in a structure. */
__extension__ typedef void    *RTE_MARKER[0];
/** Marker for 1B alignment in a structure. */
__extension__ typedef uint8_t  RTE_MARKER8[0];
/** Marker for 2B alignment in a structure. */
__extension__ typedef uint16_t RTE_MARKER16[0];
/** Marker for 4B alignment in a structure. */
__extension__ typedef uint32_t RTE_MARKER32[0];
/** Marker for 8B alignment in a structure. */
__extension__ typedef uint64_t RTE_MARKER64[0];

#endif

/*********** Macros for calculating min and max **********/

/**
 * Macro to return the minimum of two numbers
 */
#define RTE_MIN(a, b) \
	__extension__ ({ \
		typeof (a) _a_min = (a); \
		typeof (b) _b_min = (b); \
		_a_min < _b_min ? _a_min : _b_min; \
	})

/**
 * Macro to return the minimum of three numbers
 */
#define RTE_MIN3(a, b, c) \
	__extension__ ({ \
		typeof (a) _a_min3 = (a); \
		typeof (b) _b_min3 = (b); \
		typeof (c) _c_min3 = (c); \
		_a_min3 < _b_min3 ? \
			(_a_min3 < _c_min3 ? _a_min3 : _c_min3) : \
			(_b_min3 < _c_min3 ? _b_min3 : _c_min3); \
	})

/**
 * Macro to return the minimum of two numbers
 *
 * As opposed to RTE_MIN, it does not use temporary variables so it is not safe
 * if a or b is an expression. Yet it is guaranteed to be constant for use in
 * static_assert().
 */
#define RTE_MIN_T(a, b, t) \
	((t)(a) < (t)(b) ? (t)(a) : (t)(b))

/**
 * Macro to return the maximum of two numbers
 */
#define RTE_MAX(a, b) \
	__extension__ ({ \
		typeof (a) _a_max = (a); \
		typeof (b) _b_max = (b); \
		_a_max > _b_max ? _a_max : _b_max; \
	})

/**
 * Macro to return the maximum of three numbers
 */
#define RTE_MAX3(a, b, c) \
	__extension__ ({ \
		typeof (a) _a_max3 = (a); \
		typeof (b) _b_max3 = (b); \
		typeof (c) _c_max3 = (c); \
		_a_max3 > _b_max3 ? \
			(_a_max3 > _c_max3 ? _a_max3 : _c_max3) : \
			(_b_max3 > _c_max3 ? _b_max3 : _c_max3); \
	})

/**
 * Macro to return the maximum of two numbers
 *
 * As opposed to RTE_MAX, it does not use temporary variables so it is not safe
 * if a or b is an expression. Yet it is guaranteed to be constant for use in
 * static_assert().
 */
#define RTE_MAX_T(a, b, t) \
	((t)(a) > (t)(b) ? (t)(a) : (t)(b))

/*********** Other general functions / macros ********/

#ifndef offsetof
/** Return the offset of a field in a structure. */
#define offsetof(TYPE, MEMBER)  __builtin_offsetof (TYPE, MEMBER)
#endif

/**
 * Return pointer to the wrapping struct instance.
 *
 * Example:
 *
 *  struct wrapper {
 *      ...
 *      struct child c;
 *      ...
 *  };
 *
 *  struct child *x = obtain(...);
 *  struct wrapper *w = container_of(x, struct wrapper, c);
 */
#ifndef container_of
#ifdef RTE_TOOLCHAIN_MSVC
#define container_of(ptr, type, member) \
			((type *)((uintptr_t)(ptr) - offsetof(type, member)))
#else
#define container_of(ptr, type, member)	__extension__ ({		\
			const typeof(((type *)0)->member) *_ptr = (ptr); \
			__rte_unused type *_target_ptr =	\
				(type *)(ptr);				\
			(type *)(((uintptr_t)_ptr) - offsetof(type, member)); \
		})
#endif
#endif

/** Swap two variables. */
#define RTE_SWAP(a, b) \
	__extension__ ({ \
		typeof (a) _a = a; \
		a = b; \
		b = _a; \
	})

/**
 * Get the size of a field in a structure.
 *
 * @param type
 *   The type of the structure.
 * @param field
 *   The field in the structure.
 * @return
 *   The size of the field in the structure, in bytes.
 */
#define RTE_SIZEOF_FIELD(type, field) (sizeof(((type *)0)->field))

#define _RTE_STR(x) #x
/** Take a macro value and get a string version of it */
#define RTE_STR(x) _RTE_STR(x)

/**
 * ISO C helpers to modify format strings using variadic macros.
 * This is a replacement for the ", ## __VA_ARGS__" GNU extension.
 * An empty %s argument is appended to avoid a dangling comma.
 */
#define RTE_FMT(fmt, ...) fmt "%.0s", __VA_ARGS__ ""
#define RTE_FMT_HEAD(fmt, ...) fmt
#define RTE_FMT_TAIL(fmt, ...) __VA_ARGS__

/** Mask value of type "tp" for the first "ln" bit set. */
#define	RTE_LEN2MASK(ln, tp)	\
	((tp)((uint64_t)-1 >> (sizeof(uint64_t) * CHAR_BIT - (ln))))

/** Number of elements in the array. */
#define	RTE_DIM(a)	(sizeof (a) / sizeof ((a)[0]))

/**
 * Converts a numeric string to the equivalent uint64_t value.
 * As well as straight number conversion, also recognises the suffixes
 * k, m and g for kilobytes, megabytes and gigabytes respectively.
 *
 * If a negative number is passed in  i.e. a string with the first non-black
 * character being "-", zero is returned. Zero is also returned in the case of
 * an error with the strtoull call in the function.
 *
 * @param str
 *     String containing number to convert.
 * @return
 *     Number.
 */
uint64_t
rte_str_to_size(const char *str);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Converts the uint64_t value provided to a human-readable string.
 * It null-terminates the string, truncating the data if needed.
 * An optional unit (like "B") can be provided as a string. It will be
 * appended to the number, and a space will be inserted before the unit if needed.
 *
 * Sample outputs: (1) "use_iec" disabled, (2) "use_iec" enabled,
 *                 (3) "use_iec" enabled and "B" as unit.
 * 0 : "0", "0", "0 B"
 * 700 : "700", "700", "700 B"
 * 1000 : "1.00 k", "1000", "1000 B"
 * 1024 : "1.02 k", "1.00 ki", "1.00 kiB"
 * 21474836480 : "21.5 G", "20.0 Gi", "20.0 GiB"
 * 109951162777600 : "110 T", "100 Ti", "100 TiB"
 *
 * @param buf
 *     Buffer to write the string to.
 * @param buf_size
 *     Size of the buffer.
 * @param count
 *     Number to convert.
 * @param use_iec
 *     If true, use IEC units (1024-based), otherwise use SI units (1000-based).
 * @param unit
 *     Unit to append to the string (Like "B" for bytes). Can be NULL.
 * @return
 *     buf on success, NULL if the buffer is too small.
 */
__rte_experimental
char *
rte_size_to_str(char *buf, int buf_size, uint64_t count, bool use_iec, const char *unit);

/**
 * Function to terminate the application immediately, printing an error
 * message and returning the exit_code back to the shell.
 *
 * This function never returns
 *
 * @param exit_code
 *     The exit code to be returned by the application
 * @param format
 *     The format string to be used for printing the message. This can include
 *     printf format characters which will be expanded using any further parameters
 *     to the function.
 */
__rte_noreturn void
rte_exit(int exit_code, const char *format, ...)
	__rte_format_printf(2, 3);

#ifdef __cplusplus
}
#endif

#endif
