/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021-2023 Broadcom
 * All rights reserved.
 */

#ifndef CFA_TCAM_MGR_SBMP_H
#define CFA_TCAM_MGR_SBMP_H

#include <inttypes.h>

#include "cfa_tcam_mgr.h"

#define SBMP_SESSION_MAX TF_TCAM_MAX_SESSIONS
#if SBMP_SESSION_MAX <= 16
#define SBMP_WORD_WIDTH  16
#else
#define SBMP_WORD_WIDTH  32
#endif

#define SBMP_WIDTH       (((SBMP_SESSION_MAX + SBMP_WORD_WIDTH - 1) / \
			   SBMP_WORD_WIDTH) * SBMP_WORD_WIDTH)
#define	SBMP_WORD_MAX    ((SBMP_WIDTH + SBMP_WORD_WIDTH - 1) / SBMP_WORD_WIDTH)

struct sbmp {
#if SBMP_WORD_WIDTH == 16
	uint16_t bits[SBMP_WORD_MAX];
#elif SBMP_WORD_WIDTH == 32
	uint32_t bits[SBMP_WORD_MAX];
#else
	uint64_t bits[SBMP_WORD_MAX];
#endif
};

#define	SBMP_WORD_GET(bm, word)		((bm).bits[(word)])

#if SBMP_WORD_MAX == 1
#define	SBMP_WENT(session)		(0)
#define	SBMP_WBIT(session)		(1U << (session))
#define SBMP_CLEAR(bm)                  (SBMP_WORD_GET(bm, 0) = 0)
#define SBMP_IS_NULL(bm)		(SBMP_WORD_GET(bm, 0) == 0)
#define	SBMP_COUNT(bm, count)	\
	(count = __builtin_popcount(SBMP_WORD_GET(bm, 0)))
#elif SBMP_WORD_MAX == 2
#define	SBMP_WENT(session)		((session) / SBMP_WORD_WIDTH)
#define	SBMP_WBIT(session)		(1U << ((session) % SBMP_WORD_WIDTH))
#define SBMP_CLEAR(bm)							\
	do {								\
		typeof(bm) *_bm = &(bm);				\
		SBMP_WORD_GET(*_bm, 0) = SBMP_WORD_GET(*_bm, 1) = 0;	\
	} while (0)
#define SBMP_IS_NULL(bm)		\
	(SBMP_WORD_GET(bm, 0) == 0 && SBMP_WORD_GET(bm, 1) == 0)
#define	SBMP_COUNT(bm, count)						\
	do {								\
		typeof(bm) *_bm = &(bm);				\
		count = __builtin_popcount(SBMP_WORD_GET(*_bm, 0)) +	\
			__builtin_popcount(SBMP_WORD_GET(*_bm, 1)));	\
	} while (0)
#elif SBMP_WORD_MAX == 3
#define	SBMP_WENT(session)		((session) / SBMP_WORD_WIDTH)
#define	SBMP_WBIT(session)		(1U << ((session) % SBMP_WORD_WIDTH))
#define SBMP_CLEAR(bm)							\
	do {								\
		typeof(bm) *_bm = &(bm);				\
		SBMP_WORD_GET(*_bm, 0) = SBMP_WORD_GET(*_bm, 1) =	\
			SBMP_WORD_GET(*_bm, 2) = 0;			\
	} while (0)
#define SBMP_IS_NULL(bm)		\
	(SBMP_WORD_GET(bm, 0) == 0 && SBMP_WORD_GET(bm, 1) == 0 && \
	 SBMP_WORD_GET(bm, 2) == 0)
#define	SBMP_COUNT(bm, count)						\
	do {								\
		typeof(bm) *_bm = &(bm);				\
		count = __builtin_popcount(SBMP_WORD_GET(*_bm, 0)) +	\
			__builtin_popcount(SBMP_WORD_GET(*_bm, 1)) +	\
			__builtin_popcount(SBMP_WORD_GET(*_bm, 2));	\
	} while (0)
#else  /* SBMP_WORD_MAX > 3 */
#define	SBMP_WENT(session)		((session) / SBMP_WORD_WIDTH)
#define	SBMP_WBIT(session)		(1U << ((session) % SBMP_WORD_WIDTH))
#define SBMP_CLEAR(bm)							\
	do {								\
		typeof(bm) *_bm = &(bm);				\
		int	_w;						\
		for (_w = 0; _w < SBMP_WORD_MAX; _w++) {		\
			SBMP_WORD_GET(*_bm, _w) = 0;			\
		}							\
	} while (0)
#define SBMP_IS_NULL(bm)		(sbmp_bmnull(&(bm)))
#define	SBMP_COUNT(bm, count)						\
	do {								\
		typeof(bm) *_bm = &(bm);				\
		int	_count, _w;					\
		_count = 0;						\
		for (_w = 0; _w < SBMP_WORD_MAX; _w++) {		\
			_count += __builtin_popcount(SBMP_WORD_GET(*_bm, _w)); \
		}							\
		count = _count;						\
	} while (0)

/* Only needed if SBMP_WORD_MAX > 3 */
static int
sbmp_bmnull(struct ebmp *bmp)
{
	int	i;

	for (i = 0; i < SBMP_WORD_MAX; i++) {
		if (SBMP_WORD_GET(*bmp, i) != 0)
			return 0;
	}
	return 1;
}
#endif

/* generics that use the previously defined helpers */
#define SBMP_NOT_NULL(bm)		(!SBMP_IS_NULL(bm))

#define	SBMP_ENTRY(bm, session)	\
	(SBMP_WORD_GET(bm, SBMP_WENT(session)))
#define SBMP_MEMBER(bm, session)	\
	((SBMP_ENTRY(bm, session) & SBMP_WBIT(session)) != 0)
#define SBMP_SESSION_ADD(bm, session)	\
	(SBMP_ENTRY(bm, session) |= SBMP_WBIT(session))
#define SBMP_SESSION_REMOVE(bm, session)	\
	(SBMP_ENTRY(bm, session) &= ~SBMP_WBIT(session))
#endif  /* CFA_TCAM_MGR_SBMP_H */
