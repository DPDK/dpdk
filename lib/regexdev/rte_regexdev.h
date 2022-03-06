/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 * Copyright 2020 Mellanox Technologies, Ltd
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_REGEXDEV_H_
#define _RTE_REGEXDEV_H_

/**
 * @file
 *
 * RTE RegEx Device API
 *
 * Defines RTE RegEx Device APIs for RegEx operations and its provisioning.
 *
 * The RegEx Device API is composed of two parts:
 *
 * - The application-oriented RegEx API that includes functions to setup
 *   a RegEx device (configure it, setup its queue pairs and start it),
 *   update the rule database and so on.
 *
 * - The driver-oriented RegEx API that exports a function allowing
 *   a RegEx poll Mode Driver (PMD) to simultaneously register itself as
 *   a RegEx device driver.
 *
 * RegEx device components and definitions:
 *
 *     +-----------------+
 *     |                 |
 *     |                 o---------+    rte_regexdev_[en|de]queue_burst()
 *     |   PCRE based    o------+  |               |
 *     |  RegEx pattern  |      |  |  +--------+   |
 *     | matching engine o------+--+--o        |   |    +------+
 *     |                 |      |  |  | queue  |<==o===>|Core 0|
 *     |                 o----+ |  |  | pair 0 |        |      |
 *     |                 |    | |  |  +--------+        +------+
 *     +-----------------+    | |  |
 *            ^               | |  |  +--------+
 *            |               | |  |  |        |        +------+
 *            |               | +--+--o queue  |<======>|Core 1|
 *        Rule|Database       |    |  | pair 1 |        |      |
 *     +------+----------+    |    |  +--------+        +------+
 *     |     Group 0     |    |    |
 *     | +-------------+ |    |    |  +--------+        +------+
 *     | | Rules 0..n  | |    |    |  |        |        |Core 2|
 *     | +-------------+ |    |    +--o queue  |<======>|      |
 *     |     Group 1     |    |       | pair 2 |        +------+
 *     | +-------------+ |    |       +--------+
 *     | | Rules 0..n  | |    |
 *     | +-------------+ |    |       +--------+
 *     |     Group 2     |    |       |        |        +------+
 *     | +-------------+ |    |       | queue  |<======>|Core n|
 *     | | Rules 0..n  | |    +-------o pair n |        |      |
 *     | +-------------+ |            +--------+        +------+
 *     |     Group n     |
 *     | +-------------+ |<-------rte_regexdev_rule_db_update()
 *     | |             | |<-------rte_regexdev_rule_db_compile_activate()
 *     | | Rules 0..n  | |<-------rte_regexdev_rule_db_import()
 *     | +-------------+ |------->rte_regexdev_rule_db_export()
 *     +-----------------+
 *
 * RegEx: A regular expression is a concise and flexible means for matching
 * strings of text, such as particular characters, words, or patterns of
 * characters. A common abbreviation for this is “RegEx”.
 *
 * RegEx device: A hardware or software-based implementation of RegEx
 * device API for PCRE based pattern matching syntax and semantics.
 *
 * PCRE RegEx syntax and semantics specification:
 * http://regexkit.sourceforge.net/Documentation/pcre/pcrepattern.html
 *
 * RegEx queue pair: Each RegEx device should have one or more queue pair to
 * transmit a burst of pattern matching request and receive a burst of
 * receive the pattern matching response. The pattern matching request/response
 * embedded in *rte_regex_ops* structure.
 *
 * Rule: A pattern matching rule expressed in PCRE RegEx syntax along with
 * Match ID and Group ID to identify the rule upon the match.
 *
 * Rule database: The RegEx device accepts regular expressions and converts them
 * into a compiled rule database that can then be used to scan data.
 * Compilation allows the device to analyze the given pattern(s) and
 * pre-determine how to scan for these patterns in an optimized fashion that
 * would be far too expensive to compute at run-time. A rule database contains
 * a set of rules that compiled in device specific binary form.
 *
 * Match ID or Rule ID: A unique identifier provided at the time of rule
 * creation for the application to identify the rule upon match.
 *
 * Group ID: Group of rules can be grouped under one group ID to enable
 * rule isolation and effective pattern matching. A unique group identifier
 * provided at the time of rule creation for the application to identify the
 * rule upon match.
 *
 * Scan: A pattern matching request through *enqueue* API.
 *
 * It may possible that a given RegEx device may not support all the features
 * of PCRE. The application may probe unsupported features through
 * struct rte_regexdev_info::pcre_unsup_flags
 *
 * By default, all the functions of the RegEx Device API exported by a PMD
 * are lock-free functions which assume to not be invoked in parallel on
 * different logical cores to work on the same target object. For instance,
 * the dequeue function of a PMD cannot be invoked in parallel on two logical
 * cores to operates on same RegEx queue pair. Of course, this function
 * can be invoked in parallel by different logical core on different queue pair.
 * It is the responsibility of the upper level application to enforce this rule.
 *
 * In all functions of the RegEx API, the RegEx device is
 * designated by an integer >= 0 named the device identifier *dev_id*
 *
 * At the RegEx driver level, RegEx devices are represented by a generic
 * data structure of type *rte_regexdev*.
 *
 * RegEx devices are dynamically registered during the PCI/SoC device probing
 * phase performed at EAL initialization time.
 * When a RegEx device is being probed, a *rte_regexdev* structure and
 * a new device identifier are allocated for that device. Then, the
 * regexdev_init() function supplied by the RegEx driver matching the probed
 * device is invoked to properly initialize the device.
 *
 * The role of the device init function consists of resetting the hardware or
 * software RegEx driver implementations.
 *
 * If the device init operation is successful, the correspondence between
 * the device identifier assigned to the new device and its associated
 * *rte_regexdev* structure is effectively registered.
 * Otherwise, both the *rte_regexdev* structure and the device identifier are
 * freed.
 *
 * The functions exported by the application RegEx API to setup a device
 * designated by its device identifier must be invoked in the following order:
 *     - rte_regexdev_configure()
 *     - rte_regexdev_queue_pair_setup()
 *     - rte_regexdev_start()
 *
 * Then, the application can invoke, in any order, the functions
 * exported by the RegEx API to enqueue pattern matching job, dequeue pattern
 * matching response, get the stats, update the rule database,
 * get/set device attributes and so on
 *
 * If the application wants to change the configuration (i.e. call
 * rte_regexdev_configure() or rte_regexdev_queue_pair_setup()), it must call
 * rte_regexdev_stop() first to stop the device and then do the reconfiguration
 * before calling rte_regexdev_start() again. The enqueue and dequeue
 * functions should not be invoked when the device is stopped.
 *
 * Finally, an application can close a RegEx device by invoking the
 * rte_regexdev_close() function.
 *
 * Each function of the application RegEx API invokes a specific function
 * of the PMD that controls the target device designated by its device
 * identifier.
 *
 * For this purpose, all device-specific functions of a RegEx driver are
 * supplied through a set of pointers contained in a generic structure of type
 * *regexdev_ops*.
 * The address of the *regexdev_ops* structure is stored in the *rte_regexdev*
 * structure by the device init function of the RegEx driver, which is
 * invoked during the PCI/SoC device probing phase, as explained earlier.
 *
 * In other words, each function of the RegEx API simply retrieves the
 * *rte_regexdev* structure associated with the device identifier and
 * performs an indirect invocation of the corresponding driver function
 * supplied in the *regexdev_ops* structure of the *rte_regexdev* structure.
 *
 * For performance reasons, the address of the fast-path functions of the
 * RegEx driver is not contained in the *regexdev_ops* structure.
 * Instead, they are directly stored at the beginning of the *rte_regexdev*
 * structure to avoid an extra indirect memory access during their invocation.
 *
 * RTE RegEx device drivers do not use interrupts for enqueue or dequeue
 * operation. Instead, RegEx drivers export Poll-Mode enqueue and dequeue
 * functions to applications.
 *
 * The *enqueue* operation submits a burst of RegEx pattern matching request
 * to the RegEx device and the *dequeue* operation gets a burst of pattern
 * matching response for the ones submitted through *enqueue* operation.
 *
 * Typical application utilisation of the RegEx device API will follow the
 * following programming flow.
 *
 * - rte_regexdev_configure()
 * - rte_regexdev_queue_pair_setup()
 * - rte_regexdev_rule_db_update() Needs to invoke if precompiled rule database
 *   not provided in rte_regexdev_config::rule_db for rte_regexdev_configure()
 *   and/or application needs to update rule database.
 * - rte_regexdev_rule_db_compile_activate() Needs to invoke if
 *   rte_regexdev_rule_db_update function was used.
 * - Create or reuse exiting mempool for *rte_regex_ops* objects.
 * - rte_regexdev_start()
 * - rte_regexdev_enqueue_burst()
 * - rte_regexdev_dequeue_burst()
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_config.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_mbuf.h>
#include <rte_memory.h>

#define RTE_REGEXDEV_NAME_MAX_LEN RTE_DEV_NAME_MAX_LEN

extern int rte_regexdev_logtype;

#define RTE_REGEXDEV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, rte_regexdev_logtype, "" __VA_ARGS__)

/* Macros to check for valid port */
#define RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, retval) do { \
	if (!rte_regexdev_is_valid_dev(dev_id)) { \
		RTE_REGEXDEV_LOG(ERR, "Invalid dev_id=%u\n", dev_id); \
		return retval; \
	} \
} while (0)

#define RTE_REGEXDEV_VALID_DEV_ID_OR_RET(dev_id) do { \
	if (!rte_regexdev_is_valid_dev(dev_id)) { \
		RTE_REGEXDEV_LOG(ERR, "Invalid dev_id=%u\n", dev_id); \
		return; \
	} \
} while (0)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Check if dev_id is ready.
 *
 * @param dev_id
 *   The dev identifier of the RegEx device.
 *
 * @return
 *   - 0 if device state is not in ready state.
 *   - 1 if device state is ready state.
 */
__rte_experimental
int rte_regexdev_is_valid_dev(uint16_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the total number of RegEx devices that have been successfully
 * initialised.
 *
 * @return
 *   The total number of usable RegEx devices.
 */
__rte_experimental
uint8_t
rte_regexdev_count(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get the device identifier for the named RegEx device.
 *
 * @param name
 *   RegEx device name to select the RegEx device identifier.
 *
 * @return
 *   Returns RegEx device identifier on success.
 *   - <0: Failure to find named RegEx device.
 */
__rte_experimental
int
rte_regexdev_get_dev_id(const char *name);

/* Enumerates RegEx device capabilities */
#define RTE_REGEXDEV_CAPA_RUNTIME_COMPILATION_F (1ULL << 0)
/**< RegEx device does support compiling the rules at runtime unlike
 * loading only the pre-built rule database using
 * struct rte_regexdev_config::rule_db in rte_regexdev_configure()
 *
 * @see struct rte_regexdev_config::rule_db, rte_regexdev_configure()
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_CAPA_SUPP_PCRE_START_ANCHOR_F (1ULL << 1)
/**< RegEx device support PCRE Anchor to start of match flag.
 * Example RegEx is `/\Gfoo\d/`. Here `\G` asserts position at the end of the
 * previous match or the start of the string for the first match.
 * This position will change each time the RegEx is applied to the subject
 * string. If the RegEx is applied to `foo1foo2Zfoo3` the first two matches will
 * be successful for `foo1foo2` and fail for `Zfoo3`.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_CAPA_SUPP_PCRE_ATOMIC_GROUPING_F (1ULL << 2)
/**< RegEx device support PCRE Atomic grouping.
 * Atomic groups are represented by `(?>)`. An atomic group is a group that,
 * when the RegEx engine exits from it, automatically throws away all
 * backtracking positions remembered by any tokens inside the group.
 * Example RegEx is `a(?>bc|b)c` if the given patterns are `abc` and `abcc` then
 * `a(bc|b)c` matches both where as `a(?>bc|b)c` matches only abcc because
 * atomic groups don't allow backtracking back to `b`.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_BACKTRACKING_CTRL_F (1ULL << 3)
/**< RegEx device support PCRE backtracking control verbs.
 * Some examples of backtracking verbs are (*COMMIT), (*ACCEPT), (*FAIL),
 * (*SKIP), (*PRUNE).
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_CALLOUTS_F (1ULL << 4)
/**< RegEx device support PCRE callouts.
 * PCRE supports calling external function in between matches by using `(?C)`.
 * Example RegEx `ABC(?C)D` if a given patter is `ABCD` then the RegEx engine
 * will parse ABC perform a userdefined callout and return a successful match at
 * D.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_BACKREFERENCE_F (1ULL << 5)
/**< RegEx device support PCRE backreference.
 * Example RegEx is `(\2ABC|(GHI))+` `\2` matches the same text as most recently
 * matched by the 2nd capturing group i.e. `GHI`.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_GREEDY_F (1ULL << 6)
/**< RegEx device support PCRE Greedy mode.
 * For example if the RegEx is `AB\d*?` then `*?` represents zero or unlimited
 * matches. In greedy mode the pattern `AB12345` will be matched completely
 * where as the ungreedy mode `AB` will be returned as the match.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_MATCH_ALL_F (1ULL << 7)
/**< RegEx device support match all mode.
 * For example if the RegEx is `AB\d*?` then `*?` represents zero or unlimited
 * matches. In match all mode the pattern `AB12345` will return 6 matches.
 * AB, AB1, AB12, AB123, AB1234, AB12345.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_LOOKAROUND_ASRT_F (1ULL << 8)
/**< RegEx device support PCRE Lookaround assertions
 * (Zero-width assertions). Example RegEx is `[a-z]+\d+(?=!{3,})` if
 * the given pattern is `dwad1234!` the RegEx engine doesn't report any matches
 * because the assert `(?=!{3,})` fails. The pattern `dwad123!!!` would return a
 * successful match.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_MATCH_POINT_RST_F (1ULL << 9)
/**< RegEx device doesn't support PCRE match point reset directive.
 * Example RegEx is `[a-z]+\K\d+` if the pattern is `dwad123`
 * then even though the entire pattern matches only `123`
 * is reported as a match.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_NEWLINE_CONVENTIONS_F (1ULL << 10)
/**< RegEx support PCRE newline convention.
 * Newline conventions are represented as follows:
 * (*CR)        carriage return
 * (*LF)        linefeed
 * (*CRLF)      carriage return, followed by linefeed
 * (*ANYCRLF)   any of the three above
 * (*ANY)       all Unicode newline sequences
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_NEWLINE_SEQ_F (1ULL << 11)
/**< RegEx device support PCRE newline sequence.
 * The escape sequence `\R` will match any newline sequence.
 * It is equivalent to: `(?>\r\n|\n|\x0b|\f|\r|\x85)`.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_POSSESSIVE_QUALIFIERS_F (1ULL << 12)
/**< RegEx device support PCRE possessive qualifiers.
 * Example RegEx possessive qualifiers `*+`, `++`, `?+`, `{m,n}+`.
 * Possessive quantifier repeats the token as many times as possible and it does
 * not give up matches as the engine backtracks. With a possessive quantifier,
 * the deal is all or nothing.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_SUBROUTINE_REFERENCES_F (1ULL << 13)
/**< RegEx device support PCRE Subroutine references.
 * PCRE Subroutine references allow for sub patterns to be assessed
 * as part of the RegEx. Example RegEx is `(foo|fuzz)\g<1>+bar` matches the
 * pattern `foofoofuzzfoofuzzbar`.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_UTF_8_F (1ULL << 14)
/**< RegEx device support UTF-8 character encoding.
 *
 * @see struct rte_regexdev_info::pcre_unsup_flags
 */

#define RTE_REGEXDEV_SUPP_PCRE_UTF_16_F (1ULL << 15)
/**< RegEx device support UTF-16 character encoding.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_UTF_32_F (1ULL << 16)
/**< RegEx device support UTF-32 character encoding.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_WORD_BOUNDARY_F (1ULL << 17)
/**< RegEx device support word boundaries.
 * The meta character `\b` represents word boundary anchor.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_PCRE_FORWARD_REFERENCES_F (1ULL << 18)
/**< RegEx device support Forward references.
 * Forward references allow you to use a back reference to a group that appears
 * later in the RegEx. Example RegEx is `(\3ABC|(DEF|(GHI)))+` matches the
 * following string `GHIGHIABCDEF`.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_MATCH_AS_END_F (1ULL << 19)
/**< RegEx device support match as end.
 * Match as end means that the match result holds the end offset of the
 * detected match. No len value is set.
 * If the device doesn't support this feature it means the match
 * result holds the starting position of match and the length of the match.
 *
 * @see struct rte_regexdev_info::regexdev_capa
 */

#define RTE_REGEXDEV_SUPP_CROSS_BUFFER_F (1ULL << 20)
/**< RegEx device support cross buffer match.
 * Cross buffer matching means that the match can be detected even if the
 * string was started in previous buffer.
 * In case the device is configured as RTE_REGEXDEV_CFG_MATCH_AS_END
 * the end offset will be relative for the first packet.
 * For example RegEx is ABC the first buffer is xxxx second buffer yyyA and
 * the last buffer BCzz.
 * In case the match as end is configured the end offset will be 10.
 *
 * @see RTE_REGEXDEV_CFG_MATCH_AS_END_F
 * @see RTE_REGEXDEV_CFG_CROSS_BUFFER_SCAN_F
 * @see RTE_REGEX_OPS_RSP_PMI_SOJ_F
 * @see RTE_REGEX_OPS_RSP_PMI_EOJ_F
 */

#define RTE_REGEXDEV_SUPP_MATCH_ALL_F (1ULL << 21)
/**< RegEx device support match all.
 * Match all means that the RegEx engine will return all possible matches.
 * For example, assume the RegEx is `A+b`, given the input AAAb the
 * returned matches will be: Ab, AAb and AAAb.
 *
 * @see RTE_REGEXDEV_CFG_MATCH_ALL_F
 */

#define RTE_REGEXDEV_CAPA_QUEUE_PAIR_OOS_F (1ULL << 22)
/**< RegEx device supports out of order scan.
 * Out of order scan means the response of a specific job can be returned as
 * soon as it is ready even if previous jobs on the same queue didn't complete.
 *
 * @see RTE_REGEX_QUEUE_PAIR_CFG_OOS_F
 * @see struct rte_regexdev_info::regexdev_capa
 */

/* Enumerates PCRE rule flags */
#define RTE_REGEX_PCRE_RULE_ALLOW_EMPTY_F (1ULL << 0)
/**< When this flag is set, the pattern that can match against an empty string,
 * such as `.*` are allowed.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_ANCHORED_F (1ULL << 1)
/**< When this flag is set, the pattern is forced to be "anchored", that is, it
 * is constrained to match only at the first matching point in the string that
 * is being searched. Similar to `^` and represented by `\A`.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_CASELESS_F (1ULL << 2)
/**< When this flag is set, letters in the pattern match both upper and lower
 * case letters in the subject.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_DOTALL_F (1ULL << 3)
/**< When this flag is set, a dot metacharacter in the pattern matches any
 * character, including one that indicates a newline.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_DUPNAMES_F (1ULL << 4)
/**< When this flag is set, names used to identify capture groups need not be
 * unique.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_EXTENDED_F (1ULL << 5)
/**< When this flag is set, most white space characters in the pattern are
 * totally ignored except when escaped or inside a character class.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_MATCH_UNSET_BACKREF_F (1ULL << 6)
/**< When this flag is set, a backreference to an unset capture group matches an
 * empty string.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_MULTILINE_F (1ULL << 7)
/**< When this flag  is set, the `^` and `$` constructs match immediately
 * following or immediately before internal newlines in the subject string,
 * respectively, as well as at the very start and end.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_NO_AUTO_CAPTURE_F (1ULL << 8)
/**< When this Flag is set, it disables the use of numbered capturing
 * parentheses in the pattern. References to capture groups (backreferences or
 * recursion/subroutine calls) may only refer to named groups, though the
 * reference can be by name or by number.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_UCP_F (1ULL << 9)
/**< By default, only ASCII characters are recognized, When this flag is set,
 * Unicode properties are used instead to classify characters.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_UNGREEDY_F (1ULL << 10)
/**< When this flag is set, the "greediness" of the quantifiers is inverted
 * so that they are not greedy by default, but become greedy if followed by
 * `?`.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_UTF_F (1ULL << 11)
/**< When this flag is set, RegEx engine has to regard both the pattern and the
 * subject strings that are subsequently processed as strings of UTF characters
 * instead of single-code-unit strings.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

#define RTE_REGEX_PCRE_RULE_NEVER_BACKSLASH_C_F (1ULL << 12)
/**< This flag locks out the use of `\C` in the pattern that is being compiled.
 * This escape matches one data unit, even in UTF mode which can cause
 * unpredictable behavior in UTF-8 or UTF-16 modes, because it may leave the
 * current matching point in the mi:set hlsearchddle of a multi-code-unit
 * character.
 *
 * @see struct rte_regexdev_info::rule_flags
 * @see struct rte_regexdev_rule::rule_flags
 */

/**
 * RegEx device information
 */
struct rte_regexdev_info {
	const char *driver_name; /**< RegEx driver name. */
	struct rte_device *dev;	/**< Device information. */
	uint16_t max_matches;
	/**< Maximum matches per scan supported by this device. */
	uint16_t max_queue_pairs;
	/**< Maximum queue pairs supported by this device. */
	uint16_t max_payload_size;
	/**< Maximum payload size for a pattern match request or scan.
	 * @see RTE_REGEXDEV_CFG_CROSS_BUFFER_SCAN_F
	 */
	uint32_t max_rules_per_group;
	/**< Maximum rules supported per group by this device. */
	uint16_t max_groups;
	/**< Maximum groups supported by this device. */
	uint32_t regexdev_capa;
	/**< RegEx device capabilities. @see RTE_REGEXDEV_CAPA_* */
	uint64_t rule_flags;
	/**< Supported compiler rule flags.
	 * @see RTE_REGEX_PCRE_RULE_*, struct rte_regexdev_rule::rule_flags
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the contextual information of a RegEx device.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param[out] dev_info
 *   A pointer to a structure of type *rte_regexdev_info* to be filled with the
 *   contextual information of the device.
 *
 * @return
 *   - 0: Success, driver updates the contextual information of the RegEx device
 *   - <0: Error code returned by the driver info get function.
 */
__rte_experimental
int
rte_regexdev_info_get(uint8_t dev_id, struct rte_regexdev_info *dev_info);

/* Enumerates RegEx device configuration flags */
#define RTE_REGEXDEV_CFG_CROSS_BUFFER_SCAN_F (1ULL << 0)
/**< Cross buffer scan refers to the ability to be able to detect
 * matches that occur across buffer boundaries, where the buffers are related
 * to each other in some way. Enable this flag when to scan payload size
 * greater than struct rte_regexdev_info::max_payload_size and/or
 * matches can present across scan buffer boundaries.
 *
 * @see struct rte_regexdev_info::max_payload_size
 * @see struct rte_regexdev_config::dev_cfg_flags, rte_regexdev_configure()
 * @see RTE_REGEX_OPS_RSP_PMI_SOJ_F
 * @see RTE_REGEX_OPS_RSP_PMI_EOJ_F
 */

#define RTE_REGEXDEV_CFG_MATCH_AS_END_F (1ULL << 1)
/**< Match as end is the ability to return the result as ending offset.
 * When this flag is set, the result for each match will hold the ending
 * offset of the match in end_offset.
 * If this flag is not set, then the match result will hold the starting offset
 * in start_offset, and the length of the match in len.
 *
 * @see RTE_REGEXDEV_SUPP_MATCH_AS_END_F
 */

#define RTE_REGEXDEV_CFG_MATCH_ALL_F (1ULL << 2)
/**< Match all is the ability to return all possible results.
 *
 * @see RTE_REGEXDEV_SUPP_MATCH_ALL_F
 */

/** RegEx device configuration structure */
struct rte_regexdev_config {
	uint16_t nb_max_matches;
	/**< Maximum matches per scan configured on this device.
	 * This value cannot exceed the *max_matches*
	 * which previously provided in rte_regexdev_info_get().
	 * The value 0 is allowed, in which case, value 1 used.
	 * @see struct rte_regexdev_info::max_matches
	 */
	uint16_t nb_queue_pairs;
	/**< Number of RegEx queue pairs to configure on this device.
	 * This value cannot exceed the *max_queue_pairs* which previously
	 * provided in rte_regexdev_info_get().
	 * @see struct rte_regexdev_info::max_queue_pairs
	 */
	uint32_t nb_rules_per_group;
	/**< Number of rules per group to configure on this device.
	 * This value cannot exceed the *max_rules_per_group*
	 * which previously provided in rte_regexdev_info_get().
	 * The value 0 is allowed, in which case,
	 * struct rte_regexdev_info::max_rules_per_group used.
	 * @see struct rte_regexdev_info::max_rules_per_group
	 */
	uint16_t nb_groups;
	/**< Number of groups to configure on this device.
	 * This value cannot exceed the *max_groups*
	 * which previously provided in rte_regexdev_info_get().
	 * @see struct rte_regexdev_info::max_groups
	 */
	const char *rule_db;
	/**< Import initial set of prebuilt rule database on this device.
	 * The value NULL is allowed, in which case, the device will not
	 * be configured prebuilt rule database. Application may use
	 * rte_regexdev_rule_db_update() or rte_regexdev_rule_db_import() API
	 * to update or import rule database after the
	 * rte_regexdev_configure().
	 * @see rte_regexdev_rule_db_update(), rte_regexdev_rule_db_import()
	 */
	uint32_t rule_db_len;
	/**< Length of *rule_db* buffer. */
	uint32_t dev_cfg_flags;
	/**< RegEx device configuration flags, See RTE_REGEXDEV_CFG_*  */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Configure a RegEx device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * The caller may use rte_regexdev_info_get() to get the capability of each
 * resources available for this regex device.
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param cfg
 *   The RegEx device configuration structure.
 *
 * @return
 *   - 0: Success, device configured. Otherwise negative errno is returned.
 */
__rte_experimental
int
rte_regexdev_configure(uint8_t dev_id, const struct rte_regexdev_config *cfg);

/* Enumerates RegEx queue pair configuration flags */
#define RTE_REGEX_QUEUE_PAIR_CFG_OOS_F (1ULL << 0)
/**< Out of order scan, If not set, a scan must retire after previously issued
 * in-order scans to this queue pair. If set, this scan can be retired as soon
 * as device returns completion. Application should not set out of order scan
 * flag if it needs to maintain the ingress order of scan request.
 *
 * @see struct rte_regexdev_qp_conf::qp_conf_flags
 * @see rte_regexdev_queue_pair_setup()
 */

struct rte_regex_ops;
typedef void (*regexdev_stop_flush_t)(uint8_t dev_id, uint16_t qp_id,
				      struct rte_regex_ops *op);
/**< Callback function called during rte_regexdev_stop(), invoked once per
 * flushed RegEx op.
 */

/** RegEx queue pair configuration structure */
struct rte_regexdev_qp_conf {
	uint32_t qp_conf_flags;
	/**< Queue pair config flags, See RTE_REGEX_QUEUE_PAIR_CFG_* */
	uint16_t nb_desc;
	/**< The number of descriptors to allocate for this queue pair. */
	regexdev_stop_flush_t cb;
	/**< Callback function called during rte_regexdev_stop(), invoked
	 * once per flushed regex op. Value NULL is allowed, in which case
	 * callback will not be invoked. This function can be used to properly
	 * dispose of outstanding regex ops from response queue,
	 * for example ops containing memory pointers.
	 * @see rte_regexdev_stop()
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Allocate and set up a RegEx queue pair for a RegEx device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_pair_id
 *   The index of the RegEx queue pair to setup. The value must be in the range
 *   [0, nb_queue_pairs - 1] previously supplied to rte_regexdev_configure().
 * @param qp_conf
 *   The pointer to the configuration data to be used for the RegEx queue pair.
 *   NULL value is allowed, in which case default configuration	used.
 *
 * @return
 *   0 on success. Otherwise negative errno is returned.
 */
__rte_experimental
int
rte_regexdev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
			      const struct rte_regexdev_qp_conf *qp_conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Start a RegEx device.
 *
 * The device start step is the last one and consists of setting the RegEx
 * queues to start accepting the pattern matching scan requests.
 *
 * On success, all basic functions exported by the API (RegEx enqueue,
 * RegEx dequeue and so on) can be invoked.
 *
 * @param dev_id
 *   RegEx device identifier.
 *
 * @return
 *   0 on success. Otherwise negative errno is returned.
 */
__rte_experimental
int
rte_regexdev_start(uint8_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Stop a RegEx device.
 *
 * Stop a RegEx device. The device can be restarted with a call to
 * rte_regexdev_start().
 *
 * This function causes all queued response regex ops to be drained in the
 * response queue. While draining ops out of the device,
 * struct rte_regexdev_qp_conf::cb will be invoked for each ops.
 *
 * @param dev_id
 *   RegEx device identifier.
 *
 * @return
 *   0 on success. Otherwise negative errno is returned.
 */
__rte_experimental
int
rte_regexdev_stop(uint8_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Close a RegEx device. The device cannot be restarted!
 *
 * @param dev_id
 *   RegEx device identifier
 *
 * @return
 *   0 on success. Otherwise negative errno is returned.
 */
__rte_experimental
int
rte_regexdev_close(uint8_t dev_id);

/* Device get/set attributes */

/** Enumerates RegEx device attribute identifier */
enum rte_regexdev_attr_id {
	RTE_REGEXDEV_ATTR_SOCKET_ID,
	/**< The NUMA socket id to which the device is connected or
	 * a default of zero if the socket could not be determined.
	 * datatype: *int*
	 * operation: *get*
	 */
	RTE_REGEXDEV_ATTR_MAX_MATCHES,
	/**< Maximum number of matches per scan.
	 * datatype: *uint8_t*
	 * operation: *get* and *set*
	 * @see RTE_REGEX_OPS_RSP_MAX_MATCH_F
	 */
	RTE_REGEXDEV_ATTR_MAX_SCAN_TIMEOUT,
	/**< Upper bound scan time in ns.
	 * datatype: *uint16_t*
	 * operation: *get* and *set*
	 * @see RTE_REGEX_OPS_RSP_MAX_SCAN_TIMEOUT_F
	 */
	RTE_REGEXDEV_ATTR_MAX_PREFIX,
	/**< Maximum number of prefix detected per scan.
	 * This would be useful for denial of service detection.
	 * datatype: *uint16_t*
	 * operation: *get* and *set*
	 * @see RTE_REGEX_OPS_RSP_MAX_PREFIX_F
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Get an attribute from a RegEx device.
 *
 * @param dev_id
 *   RegEx device identifier.
 * @param attr_id
 *   The attribute ID to retrieve.
 * @param attr_value
 *   A pointer that will be filled in with the attribute
 *   value if successful.
 *
 * @return
 *   - 0: Successfully retrieved attribute value.
 *   - -EINVAL: Invalid device or  *attr_id* provided, or *attr_value* is NULL.
 *   - -ENOTSUP: if the device doesn't support specific *attr_id*.
 */
__rte_experimental
int
rte_regexdev_attr_get(uint8_t dev_id, enum rte_regexdev_attr_id attr_id,
		      void *attr_value);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Set an attribute to a RegEx device.
 *
 * @param dev_id
 *   RegEx device identifier.
 * @param attr_id
 *   The attribute ID to retrieve.
 * @param attr_value
 *   Pointer that will be filled in with the attribute value
 *   by the application.
 *
 * @return
 *   - 0: Successfully applied the attribute value.
 *   - -EINVAL: Invalid device or  *attr_id* provided, or *attr_value* is NULL.
 *   - -ENOTSUP: if the device doesn't support specific *attr_id*.
 */
__rte_experimental
int
rte_regexdev_attr_set(uint8_t dev_id, enum rte_regexdev_attr_id attr_id,
		      const void *attr_value);

/* Rule related APIs */
/** Enumerates RegEx rule operation. */
enum rte_regexdev_rule_op {
	RTE_REGEX_RULE_OP_ADD,
	/**< Add RegEx rule to rule database. */
	RTE_REGEX_RULE_OP_REMOVE
	/**< Remove RegEx rule from rule database. */
};

/** Structure to hold a RegEx rule attributes. */
struct rte_regexdev_rule {
	enum rte_regexdev_rule_op op;
	/**< OP type of the rule either a OP_ADD or OP_DELETE. */
	uint16_t group_id;
	/**< Group identifier to which the rule belongs to. */
	uint32_t rule_id;
	/**< Rule identifier which is returned on successful match. */
	const char *pcre_rule;
	/**< Buffer to hold the PCRE rule. */
	uint16_t pcre_rule_len;
	/**< Length of the PCRE rule. */
	uint64_t rule_flags;
	/* PCRE rule flags. Supported device specific PCRE rules enumerated
	 * in struct rte_regexdev_info::rule_flags. For successful rule
	 * database update, application needs to provide only supported
	 * rule flags.
	 * @See RTE_REGEX_PCRE_RULE_*, struct rte_regexdev_info::rule_flags
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Update the local rule set.
 * This functions only modify the rule set in memory.
 * In order for the changes to take effect, the function
 * rte_regexdev_rule_db_compile_active must be called.
 *
 * @param dev_id
 *   RegEx device identifier.
 * @param rules
 *   Points to an array of *nb_rules* objects of type *rte_regexdev_rule*
 *   structure which contain the regex rules attributes to be updated
 *   in rule database.
 * @param nb_rules
 *   The number of PCRE rules to update the rule database.
 *
 * @return
 *   The number of regex rules actually updated on the regex device's rule
 *   database. The return value can be less than the value of the *nb_rules*
 *   parameter when the regex devices fails to update the rule database or
 *   if invalid parameters are specified in a *rte_regexdev_rule*.
 *   If the return value is less than *nb_rules*, the remaining PCRE rules
 *   at the end of *rules* are not consumed and the caller has to take
 *   care of them and rte_errno is set accordingly.
 *   Possible errno values include:
 *   - -EINVAL:  Invalid device ID or rules is NULL
 *   - -ENOTSUP: The last processed rule is not supported on this device.
 *   - -ENOSPC: No space available in rule database.
 *
 * @see rte_regexdev_rule_db_import(), rte_regexdev_rule_db_export(),
 *   rte_regexdev_rule_db_compile_activate()
 */
__rte_experimental
int
rte_regexdev_rule_db_update(uint8_t dev_id,
			    const struct rte_regexdev_rule *rules,
			    uint32_t nb_rules);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Compile local rule set and burn the complied result to the
 * RegEx device.
 *
 * @param dev_id
 *   RegEx device identifier.
 *
 * @return
 *   0 on success, otherwise negative errno.
 *
 * @see rte_regexdev_rule_db_import(), rte_regexdev_rule_db_export(),
 *   rte_regexdev_rule_db_update()
 */
__rte_experimental
int
rte_regexdev_rule_db_compile_activate(uint8_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Import a prebuilt rule database from a buffer to a RegEx device.
 *
 * @param dev_id
 *   RegEx device identifier.
 * @param rule_db
 *   Points to prebuilt rule database.
 * @param rule_db_len
 *   Length of the rule database.
 *
 * @return
 *   - 0: Successfully updated the prebuilt rule database.
 *   - -EINVAL:  Invalid device ID or rule_db is NULL
 *   - -ENOTSUP: Rule database import is not supported on this device.
 *   - -ENOSPC: No space available in rule database.
 *
 * @see rte_regexdev_rule_db_update(), rte_regexdev_rule_db_export()
 */
__rte_experimental
int
rte_regexdev_rule_db_import(uint8_t dev_id, const char *rule_db,
			    uint32_t rule_db_len);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Export the prebuilt rule database from a RegEx device to the buffer.
 *
 * @param dev_id
 *   RegEx device identifier.
 * @param[out] rule_db
 *   Block of memory to insert the rule database. Must be at least size in
 *   capacity. If set to NULL, function returns required capacity.
 *
 * @return
 *   - 0: Successfully exported the prebuilt rule database.
 *   - size: If rule_db set to NULL then required capacity for *rule_db*
 *   - -EINVAL:  Invalid device ID
 *   - -ENOTSUP: Rule database export is not supported on this device.
 *
 * @see rte_regexdev_rule_db_update(), rte_regexdev_rule_db_import()
 */
__rte_experimental
int
rte_regexdev_rule_db_export(uint8_t dev_id, char *rule_db);

/* Extended statistics */
/** Maximum name length for extended statistics counters */
#define RTE_REGEXDEV_XSTATS_NAME_SIZE 64

/**
 * A name-key lookup element for extended statistics.
 *
 * This structure is used to map between names and ID numbers
 * for extended RegEx device statistics.
 */
struct rte_regexdev_xstats_map {
	uint16_t id;
	/**< xstat identifier */
	char name[RTE_REGEXDEV_XSTATS_NAME_SIZE];
	/**< xstat name */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve names of extended statistics of a regex device.
 *
 * @param dev_id
 *   The identifier of the regex device.
 * @param[out] xstats_map
 *   Block of memory to insert id and names into. Must be at least size in
 *   capacity. If set to NULL, function returns required capacity.
 * @return
 *   - Positive value on success:
 *        -The return value is the number of entries filled in the stats map.
 *        -If xstats_map set to NULL then required capacity for xstats_map.
 *   - Negative value on error:
 *      -ENODEV for invalid *dev_id*
 *      -ENOTSUP if the device doesn't support this function.
 */
__rte_experimental
int
rte_regexdev_xstats_names_get(uint8_t dev_id,
			      struct rte_regexdev_xstats_map *xstats_map);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve extended statistics of an regex device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param ids
 *   The id numbers of the stats to get. The ids can be got from the stat
 *   position in the stat list from rte_regexdev_xstats_names_get(), or
 *   by using rte_regexdev_xstats_by_name_get().
 * @param values
 *   The values for each stats request by ID.
 * @param nb_values
 *   The number of stats requested.
 * @return
 *   - Positive value: number of stat entries filled into the values array
 *   - Negative value on error:
 *      -ENODEV for invalid *dev_id*
 *      -ENOTSUP if the device doesn't support this function.
 */
__rte_experimental
int
rte_regexdev_xstats_get(uint8_t dev_id, const uint16_t *ids,
			uint64_t *values, uint16_t nb_values);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Retrieve the value of a single stat by requesting it by name.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param name
 *   The stat name to retrieve.
 * @param id
 *   If non-NULL, the numerical id of the stat will be returned, so that further
 *   requests for the stat can be got using rte_regexdev_xstats_get, which will
 *   be faster as it doesn't need to scan a list of names for the stat.
 * @param[out] value
 *   Must be non-NULL, retrieved xstat value will be stored in this address.
 *
 * @return
 *   - 0: Successfully retrieved xstat value.
 *   - -EINVAL: invalid parameters
 *   - -ENOTSUP: if not supported.
 */
__rte_experimental
int
rte_regexdev_xstats_by_name_get(uint8_t dev_id, const char *name,
				uint16_t *id, uint64_t *value);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Reset the values of the xstats of the selected component in the device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param ids
 *   Selects specific statistics to be reset. When NULL, all statistics will be
 *   reset. If non-NULL, must point to array of at least *nb_ids* size.
 * @param nb_ids
 *   The number of ids available from the *ids* array. Ignored when ids is NULL.
 *
 * @return
 *   - 0: Successfully reset the statistics to zero.
 *   - -EINVAL: invalid parameters.
 *   - -ENOTSUP: if not supported.
 */
__rte_experimental
int
rte_regexdev_xstats_reset(uint8_t dev_id, const uint16_t *ids,
			  uint16_t nb_ids);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Trigger the RegEx device self test.
 *
 * @param dev_id
 *   The identifier of the device.
 * @return
 *   - 0: Selftest successful.
 *   - -ENOTSUP if the device doesn't support selftest.
 *   - other values < 0 on failure.
 */
__rte_experimental
int
rte_regexdev_selftest(uint8_t dev_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Dump internal information about *dev_id* to the FILE* provided in *f*.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param f
 *   A pointer to a file for output.
 *
 * @return
 *   0 on success, negative errno on failure.
 */
__rte_experimental
int
rte_regexdev_dump(uint8_t dev_id, FILE *f);

/* Fast path APIs */

/**
 * The generic *rte_regexdev_match* structure to hold the RegEx match
 * attributes.
 * @see struct rte_regex_ops::matches
 */
struct rte_regexdev_match {
	RTE_STD_C11
	union {
		uint64_t u64;
		struct {
			uint32_t rule_id:20;
			/**< Rule identifier to which the pattern matched.
			 * @see struct rte_regexdev_rule::rule_id
			 */
			uint32_t group_id:12;
			/**< Group identifier of the rule which the pattern
			 * matched. @see struct rte_regexdev_rule::group_id
			 */
			uint16_t start_offset;
			/**< Starting Byte Position for matched rule. */
			RTE_STD_C11
			union {
				uint16_t len;
				/**< Length of match in bytes */
				uint16_t end_offset;
				/**< The end offset of the match. In case
				 * MATCH_AS_END configuration is enabled.
				 * @see RTE_REGEXDEV_CFG_MATCH_AS_END
				 */
			};
		};
	};
};

/* Enumerates RegEx request flags. */
#define RTE_REGEX_OPS_REQ_GROUP_ID0_VALID_F (1 << 0)
/**< Set when struct rte_regexdev_rule::group_id0 is valid. */

#define RTE_REGEX_OPS_REQ_GROUP_ID1_VALID_F (1 << 1)
/**< Set when struct rte_regexdev_rule::group_id1 is valid. */

#define RTE_REGEX_OPS_REQ_GROUP_ID2_VALID_F (1 << 2)
/**< Set when struct rte_regexdev_rule::group_id2 is valid. */

#define RTE_REGEX_OPS_REQ_GROUP_ID3_VALID_F (1 << 3)
/**< Set when struct rte_regexdev_rule::group_id3 is valid. */

#define RTE_REGEX_OPS_REQ_STOP_ON_MATCH_F (1 << 4)
/**< The RegEx engine will stop scanning and return the first match. */

#define RTE_REGEX_OPS_REQ_MATCH_HIGH_PRIORITY_F (1 << 5)
/**< In High Priority mode a maximum of one match will be returned per scan to
 * reduce the post-processing required by the application. The match with the
 * lowest Rule id, lowest start pointer and lowest match length will be
 * returned.
 *
 * @see struct rte_regex_ops::nb_actual_matches
 * @see struct rte_regex_ops::nb_matches
 */


/* Enumerates RegEx response flags. */
#define RTE_REGEX_OPS_RSP_PMI_SOJ_F (1 << 0)
/**< Indicates that the RegEx device has encountered a partial match at the
 * start of scan in the given buffer.
 *
 * @see RTE_REGEXDEV_CFG_CROSS_BUFFER_SCAN_F
 */

#define RTE_REGEX_OPS_RSP_PMI_EOJ_F (1 << 1)
/**< Indicates that the RegEx device has encountered a partial match at the
 * end of scan in the given buffer.
 *
 * @see RTE_REGEXDEV_CFG_CROSS_BUFFER_SCAN_F
 */

#define RTE_REGEX_OPS_RSP_MAX_SCAN_TIMEOUT_F (1 << 2)
/**< Indicates that the RegEx device has exceeded the max timeout while
 * scanning the given buffer.
 *
 * @see RTE_REGEXDEV_ATTR_MAX_SCAN_TIMEOUT
 */

#define RTE_REGEX_OPS_RSP_MAX_MATCH_F (1 << 3)
/**< Indicates that the RegEx device has exceeded the max matches while
 * scanning the given buffer.
 *
 * @see RTE_REGEXDEV_ATTR_MAX_MATCHES
 */

#define RTE_REGEX_OPS_RSP_MAX_PREFIX_F (1 << 4)
/**< Indicates that the RegEx device has reached the max allowed prefix length
 * while scanning the given buffer.
 *
 * @see RTE_REGEXDEV_ATTR_MAX_PREFIX
 */

#define RTE_REGEX_OPS_RSP_RESOURCE_LIMIT_REACHED_F (1 << 4)
/**< Indicates that the RegEx device has reached the max allowed resource
 * allowed while scanning the given buffer.
 */

/**
 * The generic *rte_regex_ops* structure to hold the RegEx attributes
 * for enqueue and dequeue operation.
 */
struct rte_regex_ops {
	/* W0 */
	uint16_t req_flags;
	/**< Request flags for the RegEx ops.
	 * @see RTE_REGEX_OPS_REQ_*
	 */
	uint16_t rsp_flags;
	/**< Response flags for the RegEx ops.
	 * @see RTE_REGEX_OPS_RSP_*
	 */
	uint16_t nb_actual_matches;
	/**< The total number of actual matches detected by the Regex device.*/
	uint16_t nb_matches;
	/**< The total number of matches returned by the RegEx device for this
	 * scan. The size of *rte_regex_ops::matches* zero length array will be
	 * this value.
	 *
	 * @see struct rte_regex_ops::matches, struct rte_regexdev_match
	 */

	/* W1 */
	struct rte_mbuf *mbuf; /**< source mbuf, to search in. */

	/* W2 */
	uint16_t group_id0;
	/**< First group_id to match the rule against. At minimum one group
	 * should be valid. Behaviour is undefined non of the groups are valid.
	 *
	 * @see RTE_REGEX_OPS_REQ_GROUP_ID0_VALID_F
	 */
	uint16_t group_id1;
	/**< Second group_id to match the rule against.
	 *
	 * @see RTE_REGEX_OPS_REQ_GROUP_ID1_VALID_F
	 */
	uint16_t group_id2;
	/**< Third group_id to match the rule against.
	 *
	 * @see RTE_REGEX_OPS_REQ_GROUP_ID2_VALID_F
	 */
	uint16_t group_id3;
	/**< Forth group_id to match the rule against.
	 *
	 * @see RTE_REGEX_OPS_REQ_GROUP_ID3_VALID_F
	 */

	/* W3 */
	RTE_STD_C11
	union {
		uint64_t user_id;
		/**< Application specific opaque value. An application may use
		 * this field to hold application specific value to share
		 * between dequeue and enqueue operation.
		 * Implementation should not modify this field.
		 */
		void *user_ptr;
		/**< Pointer representation of *user_id* */
	};

	/* W4 */
	RTE_STD_C11
	union {
		uint64_t cross_buf_id;
		/**< ID used by the RegEx device in order to support cross
		 * packet detection.
		 * This ID is returned from the RegEx device on the dequeue
		 * function. The application must send it back when calling
		 * enqueue with the following packet.
		 */
		void *cross_buf_ptr;
		/**< Pointer representation of *corss_buf_id* */
	};

	/* W5 */
	struct rte_regexdev_match matches[];
	/**< Zero length array to hold the match tuples.
	 * The struct rte_regex_ops::nb_matches value holds the number of
	 * elements in this array.
	 *
	 * @see struct rte_regex_ops::nb_matches
	 */
};

#include "rte_regexdev_core.h"

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Enqueue a burst of scan request on a RegEx device.
 *
 * The rte_regexdev_enqueue_burst() function is invoked to place
 * regex operations on the queue *qp_id* of the device designated by
 * its *dev_id*.
 *
 * The *nb_ops* parameter is the number of operations to process which are
 * supplied in the *ops* array of *rte_regexdev_op* structures.
 *
 * The rte_regexdev_enqueue_burst() function returns the number of
 * operations it actually enqueued for processing. A return value equal to
 * *nb_ops* means that all packets have been enqueued.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param qp_id
 *   The index of the queue pair which packets are to be enqueued for
 *   processing. The value must be in the range [0, nb_queue_pairs - 1]
 *   previously supplied to rte_regexdev_configure().
 * @param ops
 *   The address of an array of *nb_ops* pointers to *rte_regexdev_op*
 *   structures which contain the regex operations to be processed.
 * @param nb_ops
 *   The number of operations to process.
 *
 * @return
 *   The number of operations actually enqueued on the regex device. The return
 *   value can be less than the value of the *nb_ops* parameter when the
 *   regex devices queue is full or if invalid parameters are specified in
 *   a *rte_regexdev_op*. If the return value is less than *nb_ops*, the
 *   remaining ops at the end of *ops* are not consumed and the caller has
 *   to take care of them.
 */
__rte_experimental
static inline uint16_t
rte_regexdev_enqueue_burst(uint8_t dev_id, uint16_t qp_id,
			   struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct rte_regexdev *dev = &rte_regex_devices[dev_id];
#ifdef RTE_LIBRTE_REGEXDEV_DEBUG
	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->enqueue, -ENOTSUP);
	if (qp_id >= dev->data->dev_conf.nb_queue_pairs) {
		RTE_REGEXDEV_LOG(ERR, "Invalid queue %d\n", qp_id);
		return -EINVAL;
	}
#endif
	return (*dev->enqueue)(dev, qp_id, ops, nb_ops);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Dequeue a burst of scan response from a queue on the RegEx device.
 * The dequeued operation are stored in *rte_regexdev_op* structures
 * whose pointers are supplied in the *ops* array.
 *
 * The rte_regexdev_dequeue_burst() function returns the number of ops
 * actually dequeued, which is the number of *rte_regexdev_op* data structures
 * effectively supplied into the *ops* array.
 *
 * A return value equal to *nb_ops* indicates that the queue contained
 * at least *nb_ops* operations, and this is likely to signify that other
 * processed operations remain in the devices output queue. Applications
 * implementing a "retrieve as many processed operations as possible" policy
 * can check this specific case and keep invoking the
 * rte_regexdev_dequeue_burst() function until a value less than
 * *nb_ops* is returned.
 *
 * The rte_regexdev_dequeue_burst() function does not provide any error
 * notification to avoid the corresponding overhead.
 *
 * @param dev_id
 *   The RegEx device identifier
 * @param qp_id
 *   The index of the queue pair from which to retrieve processed packets.
 *   The value must be in the range [0, nb_queue_pairs - 1] previously
 *   supplied to rte_regexdev_configure().
 * @param ops
 *   The address of an array of pointers to *rte_regexdev_op* structures
 *   that must be large enough to store *nb_ops* pointers in it.
 * @param nb_ops
 *   The maximum number of operations to dequeue.
 *
 * @return
 *   The number of operations actually dequeued, which is the number
 *   of pointers to *rte_regexdev_op* structures effectively supplied to the
 *   *ops* array. If the return value is less than *nb_ops*, the remaining
 *   ops at the end of *ops* are not consumed and the caller has to take care
 *   of them.
 */
__rte_experimental
static inline uint16_t
rte_regexdev_dequeue_burst(uint8_t dev_id, uint16_t qp_id,
			   struct rte_regex_ops **ops, uint16_t nb_ops)
{
	struct rte_regexdev *dev = &rte_regex_devices[dev_id];
#ifdef RTE_LIBRTE_REGEXDEV_DEBUG
	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dequeue, -ENOTSUP);
	if (qp_id >= dev->data->dev_conf.nb_queue_pairs) {
		RTE_REGEXDEV_LOG(ERR, "Invalid queue %d\n", qp_id);
		return -EINVAL;
	}
#endif
	return (*dev->dequeue)(dev, qp_id, ops, nb_ops);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_REGEXDEV_H_ */
