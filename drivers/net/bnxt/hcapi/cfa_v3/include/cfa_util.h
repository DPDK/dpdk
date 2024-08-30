/****************************************************************************
 * Copyright(c) 2021 - 2022 Broadcom Corporation, all rights reserved
 * Proprietary and Confidential Information.
 *
 * This source file is the property of Broadcom Corporation, and
 * may not be copied or distributed in any isomorphic form without
 * the prior written consent of Broadcom Corporation.
 *
 * @file cfa_util.h
 *
 * @brief CFA specific utility macros used by cfa libraries and manager
 *        sources.
 */

#ifndef _CFA_UTIL_H_
#define _CFA_UTIL_H_

/*!
 * \file
 * \brief CFA specific utility macros
 * \ingroup CFA_V3
 *  @{
 */

/* Bounds (closed interval) check helper macro */
#define CFA_CHECK_BOUNDS(x, l, h) (((x) >= (l)) && ((x) <= (h)))
#define CFA_CHECK_UPPER_BOUNDS(x, h) ((x) <= (h))

/*
 * Join macros to generate device specific object/function names for use by
 * firmware
 */
#define CFA_JOIN2(A, B) A##_##B
#define CFA_JOIN3(A, B, C) A##B##_##C
#define CFA_OBJ_NAME(PREFIX, VER, NAME) CFA_JOIN3(PREFIX, VER, NAME)
#define CFA_FUNC_NAME(PREFIX, VER, NAME) CFA_OBJ_NAME(PREFIX, VER, NAME)

/* clang-format off */
#define CFA_ALIGN_LN2(x) (((x) < 3U) ? (x) : 32U - __builtin_clz((x) - 1U) + 1U)
/* clang-format on */

/** @} */

#endif /* _CFA_UTIL_H_ */
