/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_RESID_H__
#define __NFP_RESID_H__

#if (!defined(_NFP_RESID_NO_C_FUNC) && \
	(defined(__NFP_TOOL_NFCC) || defined(__NFP_TOOL_NFAS)))
#define _NFP_RESID_NO_C_FUNC
#endif

#ifndef _NFP_RESID_NO_C_FUNC
#include "nfp_platform.h"
#endif

/*
 * NFP Chip Architectures
 *
 * These are semi-arbitrary values to indicate an NFP architecture.
 * They serve as a software view of a group of chip families, not necessarily a
 * direct mapping to actual hardware design.
 */
#define NFP_CHIP_ARCH_YD	1
#define NFP_CHIP_ARCH_TH	2

/*
 * NFP Chip Families.
 *
 * These are not enums, because they need to be microcode compatible.
 * They are also not maskable.
 *
 * Note: The NFP-4xxx family is handled as NFP-6xxx in most software
 * components.
 *
 */
#define NFP_CHIP_FAMILY_NFP6000 0x6000	/* ARCH_TH */

/* NFP Microengine/Flow Processing Core Versions */
#define NFP_CHIP_ME_VERSION_2_7 0x0207
#define NFP_CHIP_ME_VERSION_2_8 0x0208
#define NFP_CHIP_ME_VERSION_2_9 0x0209

/* NFP Chip Base Revisions. Minor stepping can just be added to these */
#define NFP_CHIP_REVISION_A0 0x00
#define NFP_CHIP_REVISION_B0 0x10
#define NFP_CHIP_REVISION_C0 0x20
#define NFP_CHIP_REVISION_PF 0xff /* Maximum possible revision */

/* CPP Targets for each chip architecture */
#define NFP6000_CPPTGT_NBI 1
#define NFP6000_CPPTGT_VQDR 2
#define NFP6000_CPPTGT_ILA 6
#define NFP6000_CPPTGT_MU 7
#define NFP6000_CPPTGT_PCIE 9
#define NFP6000_CPPTGT_ARM 10
#define NFP6000_CPPTGT_CRYPTO 12
#define NFP6000_CPPTGT_CTXPB 14
#define NFP6000_CPPTGT_CLS 15

/*
 * Wildcard indicating a CPP read or write action
 *
 * The action used will be either read or write depending on whether a read or
 * write instruction/call is performed on the NFP_CPP_ID.  It is recommended that
 * the RW action is used even if all actions to be performed on a NFP_CPP_ID are
 * known to be only reads or writes. Doing so will in many cases save NFP CPP
 * internal software resources.
 */
#define NFP_CPP_ACTION_RW 32

#define NFP_CPP_TARGET_ID_MASK 0x1f

/*
 *  NFP_CPP_ID - pack target, token, and action into a CPP ID.
 *
 * Create a 32-bit CPP identifier representing the access to be made.
 * These identifiers are used as parameters to other NFP CPP functions. Some
 * CPP devices may allow wildcard identifiers to be specified.
 *
 * @param[in]	target	NFP CPP target id
 * @param[in]	action	NFP CPP action id
 * @param[in]	token	NFP CPP token id
 * @return		NFP CPP ID
 */
#define NFP_CPP_ID(target, action, token)                   \
	((((target) & 0x7f) << 24) | (((token) & 0xff) << 16) | \
	 (((action) & 0xff) << 8))

#define NFP_CPP_ISLAND_ID(target, action, token, island)    \
	((((target) & 0x7f) << 24) | (((token) & 0xff) << 16) | \
	 (((action) & 0xff) << 8) | (((island) & 0xff) << 0))

#ifndef _NFP_RESID_NO_C_FUNC

/**
 * Return the NFP CPP target of a NFP CPP ID
 * @param[in]	id	NFP CPP ID
 * @return	NFP CPP target
 */
static inline uint8_t
NFP_CPP_ID_TARGET_of(uint32_t id)
{
	return (id >> 24) & NFP_CPP_TARGET_ID_MASK;
}

/*
 * Return the NFP CPP token of a NFP CPP ID
 * @param[in]	id	NFP CPP ID
 * @return	NFP CPP token
 */
static inline uint8_t
NFP_CPP_ID_TOKEN_of(uint32_t id)
{
	return (id >> 16) & 0xff;
}

/*
 * Return the NFP CPP action of a NFP CPP ID
 * @param[in]	id	NFP CPP ID
 * @return	NFP CPP action
 */
static inline uint8_t
NFP_CPP_ID_ACTION_of(uint32_t id)
{
	return (id >> 8) & 0xff;
}

/*
 * Return the NFP CPP action of a NFP CPP ID
 * @param[in]   id      NFP CPP ID
 * @return      NFP CPP action
 */
static inline uint8_t
NFP_CPP_ID_ISLAND_of(uint32_t id)
{
	return (id) & 0xff;
}

#endif /* _NFP_RESID_NO_C_FUNC */

/*
 *  Check if @p chip_family is an ARCH_TH chip.
 * @param chip_family One of NFP_CHIP_FAMILY_*
 */
#define NFP_FAMILY_IS_ARCH_TH(chip_family) \
	((int)(chip_family) == (int)NFP_CHIP_FAMILY_NFP6000)

/*
 *  Get the NFP_CHIP_ARCH_* of @p chip_family.
 * @param chip_family One of NFP_CHIP_FAMILY_*
 */
#define NFP_FAMILY_ARCH(x) \
	(__extension__ ({ \
		typeof(x) _x = (x); \
		(NFP_FAMILY_IS_ARCH_TH(_x) ? NFP_CHIP_ARCH_TH : \
		NFP_FAMILY_IS_ARCH_YD(_x) ? NFP_CHIP_ARCH_YD : -1) \
	}))

/*
 *  Check if @p chip_family is an NFP-6xxx chip.
 * @param chip_family One of NFP_CHIP_FAMILY_*
 */
#define NFP_FAMILY_IS_NFP6000(chip_family) \
	((int)(chip_family) == (int)NFP_CHIP_FAMILY_NFP6000)

/*
 *  Make microengine ID for NFP-6xxx.
 * @param island_id   Island ID.
 * @param menum       ME number, 0 based, within island.
 *
 * NOTE: menum should really be unsigned - MSC compiler throws error (not
 * warning) if a clause is always true i.e. menum >= 0 if cluster_num is type
 * unsigned int hence the cast of the menum to an int in that particular clause
 */
#define NFP6000_MEID(a, b)                       \
	(__extension__ ({ \
		typeof(a) _a = (a); \
		typeof(b) _b = (b); \
		(((((int)(_a) & 0x3F) == (int)(_a)) &&   \
		(((int)(_b) >= 0) && ((int)(_b) < 12))) ?    \
		(int)(((_a) << 4) | ((_b) + 4)) : -1) \
	}))

/*
 *  Do a general sanity check on the ME ID.
 * The check is on the highest possible island ID for the chip family and the
 * microengine number must  be a master ID.
 * @param meid      ME ID as created by NFP6000_MEID
 */
#define NFP6000_MEID_IS_VALID(meid) \
	(__extension__ ({ \
		typeof(meid) _a = (meid); \
		((((_a) >> 4) < 64) && (((_a) >> 4) >= 0) && \
		 (((_a) & 0xF) >= 4)) \
	}))

/*
 *  Extract island ID from ME ID.
 * @param meid   ME ID as created by NFP6000_MEID
 */
#define NFP6000_MEID_ISLAND_of(meid) (((meid) >> 4) & 0x3F)

/*
 * Extract microengine number (0 based) from ME ID.
 * @param meid   ME ID as created by NFP6000_MEID
 */
#define NFP6000_MEID_MENUM_of(meid) (((meid) & 0xF) - 4)

/*
 * Extract microengine group number (0 based) from ME ID.
 * The group is two code-sharing microengines, so group  0 refers to MEs 0,1,
 * group 1 refers to MEs 2,3 etc.
 * @param meid   ME ID as created by NFP6000_MEID
 */
#define NFP6000_MEID_MEGRP_of(meid) (NFP6000_MEID_MENUM_of(meid) >> 1)

#ifndef _NFP_RESID_NO_C_FUNC

/*
 *  Convert a string to an ME ID.
 *
 * @param s       A string of format iX.meY
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the ME ID part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return     ME ID on success, -1 on error.
 */
int nfp6000_idstr2meid(const char *s, const char **endptr);

/*
 *  Extract island ID from string.
 *
 * Example:
 * char *c;
 * int val = nfp6000_idstr2island("i32.me5", &c);
 * // val == 32, c == "me5"
 * val = nfp6000_idstr2island("i32", &c);
 * // val == 32, c == ""
 *
 * @param s       A string of format "iX.anything" or "iX"
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the island part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return        If successful, the island ID, -1 on error.
 */
int nfp6000_idstr2island(const char *s, const char **endptr);

/*
 *  Extract microengine number from string.
 *
 * Example:
 * char *c;
 * int menum = nfp6000_idstr2menum("me5.anything", &c);
 * // menum == 5, c == "anything"
 * menum = nfp6000_idstr2menum("me5", &c);
 * // menum == 5, c == ""
 *
 * @param s       A string of format "meX.anything" or "meX"
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the ME number part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return        If successful, the ME number, -1 on error.
 */
int nfp6000_idstr2menum(const char *s, const char **endptr);

/*
 * Extract context number from string.
 *
 * Example:
 * char *c;
 * int val = nfp6000_idstr2ctxnum("ctx5.anything", &c);
 * // val == 5, c == "anything"
 * val = nfp6000_idstr2ctxnum("ctx5", &c);
 * // val == 5, c == ""
 *
 * @param s       A string of format "ctxN.anything" or "ctxN"
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the context number part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return        If successful, the context number, -1 on error.
 */
int nfp6000_idstr2ctxnum(const char *s, const char **endptr);

/*
 * Extract microengine group number from string.
 *
 * Example:
 * char *c;
 * int val = nfp6000_idstr2megrp("tg2.anything", &c);
 * // val == 2, c == "anything"
 * val = nfp6000_idstr2megrp("tg5", &c);
 * // val == 2, c == ""
 *
 * @param s       A string of format "tgX.anything" or "tgX"
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the ME group part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return        If successful, the ME group number, -1 on error.
 */
int nfp6000_idstr2megrp(const char *s, const char **endptr);

/*
 * Create ME ID string of format "iX[.meY]".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param meid   Microengine ID.
 * @return       Pointer to "s" on success, NULL on error.
 */
const char *nfp6000_meid2str(char *s, int meid);

/*
 * Create ME ID string of format "name[.meY]" or "iX[.meY]".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param meid   Microengine ID.
 * @return       Pointer to "s" on success, NULL on error.
 *
 * Similar to nfp6000_meid2str() except use an alias instead of "iX"
 * if one exists for the island.
 */
const char *nfp6000_meid2altstr(char *s, int meid);

/*
 * Create string of format "iX".
 *
 * @param s         Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                  The resulting string is output here.
 * @param island_id Island ID.
 * @return          Pointer to "s" on success, NULL on error.
 */
const char *nfp6000_island2str(char *s, int island_id);

/*
 * Create string of format "name", an island alias.
 *
 * @param s         Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                  The resulting string is output here.
 * @param island_id Island ID.
 * @return          Pointer to "s" on success, NULL on error.
 */
const char *nfp6000_island2altstr(char *s, int island_id);

/*
 * Create string of format "meY".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param menum  Microengine number within island.
 * @return       Pointer to "s" on success, NULL on error.
 */
const char *nfp6000_menum2str(char *s, int menum);

/*
 * Create string of format "ctxY".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param ctxnum Context number within microengine.
 * @return       Pointer to "s" on success, NULL on error.
 */
const char *nfp6000_ctxnum2str(char *s, int ctxnum);

/*
 * Create string of format "tgY".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param megrp  Microengine group number within cluster.
 * @return       Pointer to "s" on success, NULL on error.
 */
const char *nfp6000_megrp2str(char *s, int megrp);

/*
 * Convert a string to an ME ID.
 *
 * @param chip_family Chip family ID
 * @param s           A string of format iX.meY (or clX.meY)
 * @param endptr      If non-NULL, *endptr will point to the trailing
 *                    string after the ME ID part of the string, which
 *                    is either an empty string or the first character
 *                    after the separating period.
 * @return            ME ID on success, -1 on error.
 */
int nfp_idstr2meid(int chip_family, const char *s, const char **endptr);

/*
 * Extract island ID from string.
 *
 * Example:
 * char *c;
 * int val = nfp_idstr2island(chip, "i32.me5", &c);
 * // val == 32, c == "me5"
 * val = nfp_idstr2island(chip, "i32", &c);
 * // val == 32, c == ""
 *
 * @param chip_family Chip family ID
 * @param s           A string of format "iX.anything" or "iX"
 * @param endptr      If non-NULL, *endptr will point to the trailing
 *                    string after the ME ID part of the string, which
 *                    is either an empty string or the first character
 *                    after the separating period.
 * @return            The island ID on succes, -1 on error.
 */
int nfp_idstr2island(int chip_family, const char *s, const char **endptr);

/*
 * Extract microengine number from string.
 *
 * Example:
 * char *c;
 * int menum = nfp_idstr2menum("me5.anything", &c);
 * // menum == 5, c == "anything"
 * menum = nfp_idstr2menum("me5", &c);
 * // menum == 5, c == ""
 *
 * @param chip_family Chip family ID
 * @param s           A string of format "meX.anything" or "meX"
 * @param endptr      If non-NULL, *endptr will point to the trailing
 *                    string after the ME ID part of the string, which
 *                    is either an empty string or the first character
 *                    after the separating period.
 * @return            The ME number on succes, -1 on error.
 */
int nfp_idstr2menum(int chip_family, const char *s, const char **endptr);

/*
 * Extract context number from string.
 *
 * Example:
 * char *c;
 * int val = nfp_idstr2ctxnum("ctx5.anything", &c);
 * // val == 5, c == "anything"
 * val = nfp_idstr2ctxnum("ctx5", &c);
 * // val == 5, c == ""
 *
 * @param s       A string of format "ctxN.anything" or "ctxN"
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the context number part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return        If successful, the context number, -1 on error.
 */
int nfp_idstr2ctxnum(int chip_family, const char *s, const char **endptr);

/*
 * Extract microengine group number from string.
 *
 * Example:
 * char *c;
 * int val = nfp_idstr2megrp("tg2.anything", &c);
 * // val == 2, c == "anything"
 * val = nfp_idstr2megrp("tg5", &c);
 * // val == 5, c == ""
 *
 * @param s       A string of format "tgX.anything" or "tgX"
 * @param endptr  If non-NULL, *endptr will point to the trailing string
 *                after the ME group part of the string, which is either
 *                an empty string or the first character after the separating
 *                period.
 * @return        If successful, the ME group number, -1 on error.
 */
int nfp_idstr2megrp(int chip_family, const char *s, const char **endptr);

/*
 * Create ME ID string of format "iX[.meY]".
 *
 * @param chip_family Chip family ID
 * @param s           Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                    The resulting string is output here.
 * @param meid        Microengine ID.
 * @return            Pointer to "s" on success, NULL on error.
 */
const char *nfp_meid2str(int chip_family, char *s, int meid);

/*
 * Create ME ID string of format "name[.meY]" or "iX[.meY]".
 *
 * @param chip_family Chip family ID
 * @param s           Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                    The resulting string is output here.
 * @param meid        Microengine ID.
 * @return            Pointer to "s" on success, NULL on error.
 *
 * Similar to nfp_meid2str() except use an alias instead of "iX"
 * if one exists for the island.
 */
const char *nfp_meid2altstr(int chip_family, char *s, int meid);

/*
 * Create string of format "iX".
 *
 * @param chip_family Chip family ID
 * @param s           Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                    The resulting string is output here.
 * @param island_id   Island ID.
 * @return            Pointer to "s" on success, NULL on error.
 */
const char *nfp_island2str(int chip_family, char *s, int island_id);

/*
 * Create string of format "name", an island alias.
 *
 * @param chip_family Chip family ID
 * @param s           Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                    The resulting string is output here.
 * @param island_id   Island ID.
 * @return            Pointer to "s" on success, NULL on error.
 */
const char *nfp_island2altstr(int chip_family, char *s, int island_id);

/*
 * Create string of format "meY".
 *
 * @param chip_family Chip family ID
 * @param s           Pointer to char buffer of size NFP_MEID_STR_SZ.
 *                    The resulting string is output here.
 * @param menum       Microengine number within island.
 * @return            Pointer to "s" on success, NULL on error.
 */
const char *nfp_menum2str(int chip_family, char *s, int menum);

/*
 * Create string of format "ctxY".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param ctxnum Context number within microengine.
 * @return       Pointer to "s" on success, NULL on error.
 */
const char *nfp_ctxnum2str(int chip_family, char *s, int ctxnum);

/*
 * Create string of format "tgY".
 *
 * @param s      Pointer to char buffer of size NFP_MEID_STR_SZ.
 *               The resulting string is output here.
 * @param megrp  Microengine group number within cluster.
 * @return       Pointer to "s" on success, NULL on error.
 */
const char *nfp_megrp2str(int chip_family, char *s, int megrp);

/*
 * Convert a two character string to revision number.
 *
 * Revision integer is 0x00 for A0, 0x11 for B1 etc.
 *
 * @param s     Two character string.
 * @return      Revision number, -1 on error
 */
int nfp_idstr2rev(const char *s);

/*
 * Create string from revision number.
 *
 * String will be upper case.
 *
 * @param s     Pointer to char buffer with size of at least 3
 *              for 2 characters and string terminator.
 * @param rev   Revision number.
 * @return      Pointer to "s" on success, NULL on error.
 */
const char *nfp_rev2str(char *s, int rev);

/*
 * Get the NFP CPP address from a string
 *
 * String is in the format [island@]target[:[action:[token:]]address]
 *
 * @param chip_family Chip family ID
 * @param tid           Pointer to string to parse
 * @param cpp_idp       Pointer to CPP ID
 * @param cpp_addrp     Pointer to CPP address
 * @return              0 on success, or -1 and errno
 */
int nfp_str2cpp(int chip_family,
		const char *tid,
		uint32_t *cpp_idp,
		uint64_t *cpp_addrp);


#endif /* _NFP_RESID_NO_C_FUNC */

#endif /* __NFP_RESID_H__ */
