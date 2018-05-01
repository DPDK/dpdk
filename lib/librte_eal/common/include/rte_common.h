/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_COMMON_H_
#define _RTE_COMMON_H_

/**
 * @file
 *
 * Generic, commonly-used macro and inline function definitions
 * for DPDK.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include <rte_config.h>

#ifndef typeof
#define typeof __typeof__
#endif

#ifndef asm
#define asm __asm__
#endif

/** C extension macro for environments lacking C11 features. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#define RTE_STD_C11 __extension__
#else
#define RTE_STD_C11
#endif

/** Define GCC_VERSION **/
#ifdef RTE_TOOLCHAIN_GCC
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 +	\
		__GNUC_PATCHLEVEL__)
#endif

#ifdef RTE_ARCH_STRICT_ALIGN
typedef uint64_t unaligned_uint64_t __attribute__ ((aligned(1)));
typedef uint32_t unaligned_uint32_t __attribute__ ((aligned(1)));
typedef uint16_t unaligned_uint16_t __attribute__ ((aligned(1)));
#else
typedef uint64_t unaligned_uint64_t;
typedef uint32_t unaligned_uint32_t;
typedef uint16_t unaligned_uint16_t;
#endif

/**
 * Force alignment
 */
#define __rte_aligned(a) __attribute__((__aligned__(a)))

/**
 * Force a structure to be packed
 */
#define __rte_packed __attribute__((__packed__))

/******* Macro to mark functions and fields scheduled for removal *****/
#define __rte_deprecated	__attribute__((__deprecated__))

/*********** Macros to eliminate unused variable warnings ********/

/**
 * short definition to mark a function parameter unused
 */
#define __rte_unused __attribute__((__unused__))

/**
 * definition to mark a variable or function parameter as used so
 * as to avoid a compiler warning
 */
#define RTE_SET_USED(x) (void)(x)

#define RTE_PRIORITY_LOG 101
#define RTE_PRIORITY_BUS 110
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
#define RTE_INIT_PRIO(func, prio) \
static void __attribute__((constructor(RTE_PRIO(prio)), used)) func(void)

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
 * Force a function to be inlined
 */
#define __rte_always_inline inline __attribute__((always_inline))

/**
 * Force a function to be noinlined
 */
#define __rte_noinline  __attribute__((noinline))

/*********** Macros for pointer arithmetic ********/

/**
 * add a byte-value offset to a pointer
 */
#define RTE_PTR_ADD(ptr, x) ((void*)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define RTE_PTR_SUB(ptr, x) ((void*)((uintptr_t)ptr - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define RTE_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/*********** Macros/static functions for doing alignment ********/


/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no higher than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_FLOOR(ptr, align) \
	((typeof(ptr))RTE_ALIGN_FLOOR((uintptr_t)ptr, align))

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
	(((v + (typeof(v))(mul) - 1) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no higher
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_FLOOR(v, mul) \
	((v / ((typeof(v))(mul))) * (typeof(v))(mul))

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
rte_is_aligned(void *ptr, unsigned align)
{
	return RTE_PTR_ALIGN(ptr, align) == ptr;
}

/*********** Macros for compile type checks ********/

/**
 * Triggers an error at compilation time if the condition is true.
 */
#ifndef __OPTIMIZE__
#define RTE_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#else
extern int RTE_BUILD_BUG_ON_detected_error;
#define RTE_BUILD_BUG_ON(condition) do {             \
	((void)sizeof(char[1 - 2*!!(condition)]));   \
	if (condition)                               \
		RTE_BUILD_BUG_ON_detected_error = 1; \
} while(0)
#endif

/**
 * Combines 32b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param x
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint32_t
rte_combine32ms1b(register uint32_t x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return x;
}

/**
 * Combines 64b inputs most significant set bits into the least
 * significant bits to construct a value with the same MSBs as x
 * but all 1's under it.
 *
 * @param v
 *    The integer whose MSBs need to be combined with its LSBs
 * @return
 *    The combined value.
 */
static inline uint64_t
rte_combine64ms1b(register uint64_t v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;

	return v;
}

/*********** Macros to work with powers of 2 ********/

/**
 * Returns true if n is a power of 2
 * @param n
 *     Number to check
 * @return 1 if true, 0 otherwise
 */
static inline int
rte_is_power_of_2(uint32_t n)
{
	return n && !(n & (n - 1));
}

/**
 * Aligns input parameter to the next power of 2
 *
 * @param x
 *   The integer value to algin
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint32_t
rte_align32pow2(uint32_t x)
{
	x--;
	x = rte_combine32ms1b(x);

	return x + 1;
}

/**
 * Aligns input parameter to the previous power of 2
 *
 * @param x
 *   The integer value to algin
 *
 * @return
 *   Input parameter aligned to the previous power of 2
 */
static inline uint32_t
rte_align32prevpow2(uint32_t x)
{
	x = rte_combine32ms1b(x);

	return x - (x >> 1);
}

/**
 * Aligns 64b input parameter to the next power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   Input parameter aligned to the next power of 2
 */
static inline uint64_t
rte_align64pow2(uint64_t v)
{
	v--;
	v = rte_combine64ms1b(v);

	return v + 1;
}

/**
 * Aligns 64b input parameter to the previous power of 2
 *
 * @param v
 *   The 64b value to align
 *
 * @return
 *   Input parameter aligned to the previous power of 2
 */
static inline uint64_t
rte_align64prevpow2(uint64_t v)
{
	v = rte_combine64ms1b(v);

	return v - (v >> 1);
}

/*********** Macros for calculating min and max **********/

/**
 * Macro to return the minimum of two numbers
 */
#define RTE_MIN(a, b) \
	__extension__ ({ \
		typeof (a) _a = (a); \
		typeof (b) _b = (b); \
		_a < _b ? _a : _b; \
	})

/**
 * Macro to return the maximum of two numbers
 */
#define RTE_MAX(a, b) \
	__extension__ ({ \
		typeof (a) _a = (a); \
		typeof (b) _b = (b); \
		_a > _b ? _a : _b; \
	})

/*********** Other general functions / macros ********/

/**
 * Searches the input parameter for the least significant set bit
 * (starting from zero).
 * If a least significant 1 bit is found, its bit index is returned.
 * If the content of the input parameter is zero, then the content of the return
 * value is undefined.
 * @param v
 *     input parameter, should not be zero.
 * @return
 *     least significant set bit in the input parameter.
 */
static inline uint32_t
rte_bsf32(uint32_t v)
{
	return (uint32_t)__builtin_ctz(v);
}

/**
 * Return the rounded-up log2 of a integer.
 *
 * @param v
 *     The input parameter.
 * @return
 *     The rounded-up log2 of the input, or 0 if the input is 0.
 */
static inline uint32_t
rte_log2_u32(uint32_t v)
{
	if (v == 0)
		return 0;
	v = rte_align32pow2(v);
	return rte_bsf32(v);
}

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
#define container_of(ptr, type, member)	__extension__ ({		\
			const typeof(((type *)0)->member) *_ptr = (ptr); \
			__attribute__((unused)) type *_target_ptr =	\
				(type *)(ptr);				\
			(type *)(((uintptr_t)_ptr) - offsetof(type, member)); \
		})
#endif

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
static inline uint64_t
rte_str_to_size(const char *str)
{
	char *endptr;
	unsigned long long size;

	while (isspace((int)*str))
		str++;
	if (*str == '-')
		return 0;

	errno = 0;
	size = strtoull(str, &endptr, 0);
	if (errno)
		return 0;

	if (*endptr == ' ')
		endptr++; /* allow 1 space gap */

	switch (*endptr){
	case 'G': case 'g': size *= 1024; /* fall-through */
	case 'M': case 'm': size *= 1024; /* fall-through */
	case 'K': case 'k': size *= 1024; /* fall-through */
	default:
		break;
	}
	return size;
}

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
void
rte_exit(int exit_code, const char *format, ...)
	__attribute__((noreturn))
	__attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif
