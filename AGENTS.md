<!-- SPDX-License-Identifier: BSD-3-Clause -->
<!-- Copyright(c) 2026 Stephen Hemminger -->

# AGENTS.md - DPDK Code Review Guidelines for AI Tools

## CRITICAL INSTRUCTION - READ FIRST

This document has two categories of review rules with different confidence thresholds:

### 1. Correctness Bugs -- HIGHEST PRIORITY (report at >=50% confidence)

**Always report potential correctness bugs.** These are the most valuable findings.
When in doubt, report them with a note about your confidence level.
A possible use-after-free or resource leak is worth mentioning even if you are not certain.

Correctness bugs include:
- Use-after-free (accessing memory after `free`/`rte_free`)
- Resource leaks on error paths (memory, file descriptors, locks)
- Double-free or double-close
- NULL pointer dereference
- Buffer overflows or out-of-bounds access
- Uninitialized variable use in a reachable code path
- Race conditions (unsynchronized shared state)
- `volatile` used instead of atomic operations for inter-thread shared variables
- Missing error checks on functions that can fail
- Error paths that skip cleanup (goto labels, missing free/close)
- Incorrect error propagation (wrong return value, lost errno)
- Logic errors in conditionals (wrong operator, inverted test)
- Integer overflow/truncation in size calculations
- Missing bounds checks on user-supplied sizes or indices
- Statistics accumulation using `=` instead of `+=`
- Integer multiply without widening cast losing upper bits (16x16, 32x32, etc.)
- Unbounded descriptor chain traversal on guest/API-supplied data
- `1 << n` on 64-bit bitmask (must use `1ULL << n` or `RTE_BIT64()`)
- Left shift of narrow unsigned (`uint8_t`/`uint16_t`) used as 64-bit value (sign extension via implicit `int` promotion)
- Variable assigned then overwritten before being read (dead store)
- Same variable used as loop counter in nested loops
- `rte_mbuf_raw_free_bulk()` called on mbufs that may originate from different mempools (Tx burst, ring dequeue)
- MTU confused with frame length (MTU is L3 payload; frame length = MTU + L2 overhead)
- Using `dev_conf.rxmode.mtu` after configure instead of `dev->data->mtu`
- Hardcoded Ethernet overhead instead of per-device calculation
- MTU set without enabling `RTE_ETH_RX_OFFLOAD_SCATTER` when frame size exceeds mbuf data room
- `mtu_set` callback rejects valid MTU when scatter Rx is already enabled
- Rx queue setup silently drops oversized packets instead of enabling scatter or returning an error
- Rx function selection ignores `scattered_rx` flag or MTU-vs-mbuf-size check

**Do NOT self-censor correctness bugs.** If you identify a code path
where a resource could leak or memory could be used after free, report it.
Do not talk yourself out of it.

### 2. Style, Process, and Formatting -- suppress false positives

**NEVER list a style/process item under "Errors" or "Warnings" if you conclude it is correct.**

Before outputting any style, formatting, or process error/warning, verify it is actually wrong.
If your analysis concludes with phrases like "there's no issue here", "which is fine",
"appears correct", "is acceptable", or "this is actually correct" --
then DO NOT INCLUDE IT IN YOUR OUTPUT AT ALL.
Delete it.
Omit it entirely.

This suppression rule applies to: naming conventions, code style, and process compliance.
It does NOT apply to correctness bugs listed above.
(SPDX/copyright format and commit message formatting are handled by checkpatch
and are excluded from AI review entirely.)

---

This document provides guidelines for AI-powered code review tools
when reviewing contributions to the Data Plane Development Kit (DPDK).
It is derived from the official DPDK contributor guidelines and validation scripts.

## Overview

DPDK follows a development process modeled on the Linux Kernel.
All patches are reviewed publicly on the mailing list before being merged.
AI review tools should verify compliance with the standards outlined below.

## Review Philosophy

**Correctness bugs are the primary goal of AI review.** Style and formatting checks are secondary.
A review that catches a use-after-free but misses a style nit is far more valuable
than one that catches every style issue but misses the bug.

**BEFORE OUTPUTTING YOUR REVIEW**: Re-read each item.
- For correctness bugs: keep them.
  If you have reasonable doubt that a code path is safe, report it.
- For style/process items: if ANY item contains phrases like "is fine", "no issue",
  "appears correct", "is acceptable", "actually correct" -- DELETE THAT ITEM.
  Do not include it.

### Correctness review guidelines
- Trace error paths: for every function that allocates a resource or acquires a lock,
  verify that ALL error paths after that point release it
- Check every `goto error` and early `return`: does it clean up everything allocated so far?
- Look for use-after-free: after `free(p)`, is `p` accessed again?
- Check that error codes are propagated, not silently dropped
- Report at >=50% confidence; note uncertainty if appropriate
- It is better to report a potential bug that turns out to be safe than to miss a real bug

### Style and process review guidelines
- Only comment on style/process issues when you have HIGH CONFIDENCE (>80%) that an issue exists
- Be concise: one sentence per comment when possible
- Focus on actionable feedback, not observations
- When reviewing text, only comment on clarity issues if the text is genuinely confusing
  or could lead to errors.
- Do NOT comment on copyright years, SPDX format, or copyright holders -
  not subject to AI review
- Do NOT report an issue then contradict yourself -
  if something is acceptable, do not mention it at all
- Do NOT include items in Errors/Warnings that you then say are "acceptable" or "correct"
- Do NOT mention things that are correct or "not an issue" - only report actual problems
- Do NOT speculate about contributor circumstances (employment, company policies, etc.)
- Before adding any style item to your review, ask: "Is this actually wrong?"
  If no, omit it entirely.
- NEVER write "(Correction: ...)" - if you need to correct yourself, simply omit the item entirely
- Do NOT add vague suggestions like "should be verified" or "should be checked" -
  either it's wrong or don't mention it
- Do NOT flag something as an Error then say "which is correct" in the same item
- Do NOT say "no issue here" or "this is actually correct" -
  if there's no issue, do not include it in your review
- Do NOT analyze cross-patch dependencies or compilation order -
  you cannot reliably determine this from patch review
- Do NOT claim a patch "would cause compilation failure" based on symbols used
  in other patches in the series
- Review each patch individually for its own correctness;
  assume the patch author ordered them correctly
- When reviewing a patch series, OMIT patches that have no issues.
  Do not include a patch in your output just to say "no issues found"
  or to summarize what the patch does.
  Only include patches where you have actual findings to report.

## Priority Areas (Review These)

### Security & Safety
- Unsafe code blocks without justification
- Command injection risks (shell commands, user input)
- Path traversal vulnerabilities
- Credential exposure or hard coded secrets
- Missing input validation on external data
- Improper error handling that could leak sensitive info

### Correctness Issues
- Logic errors that could cause panics or incorrect behavior
- Buffer overflows
- Race conditions
- **`volatile` for inter-thread synchronization**:
  `volatile` does not provide atomicity or memory ordering between threads.
  Use `rte_atomic_load_explicit()`/`rte_atomic_store_explicit()` with appropriate `rte_memory_order_*` instead.
  See the Shared Variable Access section under Forbidden Tokens for details.
- Resource leaks (files, connections, memory)
- Off-by-one errors or boundary conditions
- Incorrect error propagation
- **Use-after-free** (any access to memory after it has been freed)
- **Error path resource leaks**:
  For every allocation or fd open,
  trace each error path (`goto`, early `return`, conditional) to verify the resource is released.
  Common patterns to check:
  - `malloc`/`rte_malloc` followed by a failure that does `return -1` instead of `goto cleanup`
  - `open()`/`socket()` fd not closed on a later error
  - Lock acquired but not released on an error branch
  - Partially initialized structure where early fields are allocated
    but later allocation fails without freeing the early ones
- **Double-free / double-close**:
  resource freed in both a normal path and an error path,
  or fd closed but not set to -1 allowing a second close
- **Missing error checks**:
  functions that can fail (malloc, open, ioctl, etc.) whose return value is not checked
- **Do NOT flag unchecked return values from functions
  that always succeed on Linux.** Some POSIX functions are documented to always return zero on Linux
  when called with valid arguments.
  Flagging unchecked returns from these is a false positive.
  The list includes:
  - `pthread_mutex_init()`, `pthread_cond_init()`
  - `pthread_cond_signal()`, `pthread_cond_broadcast()`, `pthread_cond_wait()`
  - `pthread_condattr_init()`, `pthread_condattr_destroy()`
  - `pthread_attr_init()`, `pthread_attr_destroy()`
- Changes to API without release notes
- Changes to ABI on non-LTS release
- Usage of deprecated API when replacements exist
- Overly defensive code that adds unnecessary checks
- Unnecessary comments that just restate what the code already shows (remove them)
- **Process-shared synchronization errors** (pthread mutexes in shared memory without `PTHREAD_PROCESS_SHARED`)
- **Statistics accumulation using `=` instead of `+=`**:
  When accumulating statistics (counters, byte totals, packet counts),
  using `=` overwrites the running total with only the latest value.
  This silently produces wrong results.
  ```c
  /* BAD - overwrites instead of accumulating */
  stats->rx_packets = nb_rx;
  stats->rx_bytes = total_bytes;

  /* GOOD - accumulates over time */
  stats->rx_packets += nb_rx;
  stats->rx_bytes += total_bytes;
  ```
  Note: `=` is correct for gauge-type values (e.g., queue depth, link status)
  and for initial assignment.
  Only flag when the context is clearly incremental accumulation
  (loop bodies, per-burst counters, callback tallies).
- **Integer multiply without widening cast**:
  When multiplying integers to produce a result wider than the operands (sizes, offsets, byte counts),
  the multiplication is performed at the operand width
  and the upper bits are silently lost before the assignment.
  This applies to any narrowing scenario: 16x16 assigned to a 32-bit variable,
  32x32 assigned to a 64-bit variable, etc.
  ```c
  /* BAD - 32x32 overflows before widening to 64 */
  uint64_t total_size = num_entries * entry_size;  /* both are uint32_t */
  size_t offset = ring->idx * ring->desc_size;     /* 32x32 -> truncated */

  /* BAD - 16x16 overflows before widening to 32 */
  uint32_t byte_count = pkt_len * nb_segs;         /* both are uint16_t */

  /* GOOD - widen before multiply */
  uint64_t total_size = (uint64_t)num_entries * entry_size;
  size_t offset = (size_t)ring->idx * ring->desc_size;
  uint32_t byte_count = (uint32_t)pkt_len * nb_segs;
  ```
- **Unbounded descriptor chain traversal**:
  When walking a chain of descriptors (virtio, DMA, NIC Rx/Tx rings) where the chain length
  or next-index comes from guest memory or an untrusted API caller,
  the traversal MUST have a bounds check or loop counter to prevent infinite loops
  or out-of-bounds access from malicious/corrupt data.
  ```c
  /* BAD - guest controls desc[idx].next with no bound */
  while (desc[idx].flags & VRING_DESC_F_NEXT) {
      idx = desc[idx].next;          /* guest-supplied, unbounded */
      process(desc[idx]);
  }

  /* GOOD - cap iterations to descriptor ring size */
  for (i = 0; i < ring_size; i++) {
      if (!(desc[idx].flags & VRING_DESC_F_NEXT))
          break;
      idx = desc[idx].next;
      if (idx >= ring_size)          /* bounds check */
          return -EINVAL;
      process(desc[idx]);
  }
  ```
  This applies to any chain/linked-list traversal where indices
  or pointers originate from untrusted input (guest VMs, user-space callers, network packets).
- **Bitmask shift using `1` instead of `1ULL` on 64-bit masks**:
  The literal `1` is `int` (32 bits).
  Shifting it by 32 or more is undefined behavior; shifting it by less than 32
  but assigning to a `uint64_t` silently zeroes the upper 32 bits.
  Use `1ULL << n`, `UINT64_C(1) << n`, or the DPDK `RTE_BIT64(n)` macro.
  ```c
  /* BAD - 1 is int, UB if n >= 32, wrong if result used as uint64_t */
  uint64_t mask = 1 << bit_pos;
  if (features & (1 << VIRTIO_NET_F_MRG_RXBUF))  /* bit 15 OK, bit 32+ UB */

  /* GOOD */
  uint64_t mask = UINT64_C(1) << bit_pos;
  uint64_t mask = 1ULL << bit_pos;
  uint64_t mask = RTE_BIT64(bit_pos);        /* preferred in DPDK */
  if (features & RTE_BIT64(VIRTIO_NET_F_MRG_RXBUF))
  ```
  Note: `1U << n` is acceptable when the mask is known to be 32-bit
  (e.g., `uint32_t` register fields with `n < 32`).
  Only flag when the result is stored in, compared against,
  or returned as a 64-bit type, or when `n` could be >= 32.
- **Left shift of narrow unsigned type sign-extends to 64-bit**:
  When a `uint8_t` or `uint16_t` value is left-shifted,
  C integer promotion converts it to `int` (signed 32-bit) before the shift.
  If the result has bit 31 set, implicit conversion to `uint64_t`, `size_t`,
  or use in pointer arithmetic sign-extends the upper 32 bits to all-1s,
  producing a wrong address or value.
  This is Coverity SIGN_EXTENSION.
  The fix is to cast the narrow operand to an unsigned type
  at least as wide as the target before shifting.
  ```c
  /* BAD - uint16_t promotes to signed int, bit 31 may set,
   * then sign-extends when converted to 64-bit for pointer math */
  uint16_t idx = get_index();
  void *addr = base + (idx << wqebb_shift);   /* SIGN_EXTENSION */
  uint64_t off = (uint64_t)(idx << shift);    /* too late: shift already in int */

  /* BAD - uint8_t shift with result used as size_t */
  uint8_t page_order = get_order();
  size_t size = page_order << PAGE_SHIFT;     /* promotes to int first */

  /* GOOD - cast before shift */
  void *addr = base + ((uint64_t)idx << wqebb_shift);
  uint64_t off = (uint64_t)idx << shift;
  size_t size = (size_t)page_order << PAGE_SHIFT;

  /* GOOD - intermediate unsigned variable */
  uint32_t offset = (uint32_t)idx << wqebb_shift;  /* OK if result fits 32 bits */
  ```
  Note: This is distinct from the `1 << n` pattern (where the literal `1` is the problem)
  and from the integer-multiply pattern (where the operation is `*` not `<<`).
  The mechanism is the same C integer promotion rule,
  but the code patterns and Coverity checker names differ.
  Only flag when the shift result is used in a context wider than 32 bits (64-bit assignment,
  pointer arithmetic, function argument expecting `uint64_t`/`size_t`).
  A shift whose result is stored in a `uint32_t` or narrower variable is not affected.
- **Variable overwrite before read (dead store)**:
  A variable is assigned a value that is unconditionally overwritten before it is ever read.
  This usually indicates a logic error (wrong variable name, missing `if`, copy-paste mistake)
  or at minimum is dead code.
  ```c
  /* BAD - first assignment is never read */
  ret = validate_input(cfg);
  ret = apply_config(cfg);     /* overwrites without checking first ret */
  if (ret != 0)
      return ret;

  /* GOOD - check each return value */
  ret = validate_input(cfg);
  if (ret != 0)
      return ret;
  ret = apply_config(cfg);
  if (ret != 0)
      return ret;
  ```
  Do NOT flag cases where the initial value is intentionally a default that may
  or may not be overwritten (e.g., `int ret = 0;` followed by a conditional assignment).
  Only flag unconditional overwrites where the first value can never be observed.
- **Shared loop counter in nested loops**:
  Using the same variable as the loop counter in both an outer
  and inner loop causes the outer loop to malfunction because the inner loop modifies its counter.
  ```c
  /* BAD - inner loop clobbers outer loop counter */
  int i;
  for (i = 0; i < nb_queues; i++) {
      setup_queue(i);
      for (i = 0; i < nb_descs; i++)    /* BUG: reuses i */
          init_desc(i);
  }

  /* GOOD - distinct loop counters */
  for (int i = 0; i < nb_queues; i++) {
      setup_queue(i);
      for (int j = 0; j < nb_descs; j++)
          init_desc(j);
  }
  ```
- **`rte_mbuf_raw_free_bulk()` on mixed-pool mbuf arrays**:
  Tx burst functions and ring/queue dequeue paths receive mbufs
  that may originate from different mempools (applications are free to send mbufs from any pool).
  `rte_mbuf_raw_free_bulk()` takes an explicit mempool parameter
  and calls `rte_mempool_put_bulk()` directly --
  ALL mbufs in the array must come from that single pool.
  If mbufs come from different pools, they are returned to the wrong pool,
  corrupting pool accounting and causing hard-to-debug failures.
  Note: `rte_pktmbuf_free_bulk()` is safe for mixed pools --
  it batches mbufs by pool internally and flushes whenever the pool changes.
  ```c
  /* BAD - assumes all mbufs are from the same pool */
  /* (in tx_burst completion or ring dequeue error path) */
  rte_mbuf_raw_free_bulk(mp, mbufs, nb_mbufs);

  /* GOOD - rte_pktmbuf_free_bulk handles mixed pools correctly */
  rte_pktmbuf_free_bulk(mbufs, nb_mbufs);

  /* GOOD - free individually (each mbuf returned to its own pool) */
  for (i = 0; i < nb_mbufs; i++)
      rte_pktmbuf_free(mbufs[i]);
  ```
  This applies to any path that frees mbufs submitted by the application:
  Tx completion, Tx error cleanup, and ring/queue drain paths.
  `rte_mbuf_raw_free_bulk()` is an optimization for the fast-free case (`RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE`)
  where the application guarantees all mbufs come from a single pool with refcnt=1.
- **MTU confused with Ethernet frame length**:
  Maximum Transmission Unit (MTU) is the maximum L3 payload size
  (e.g., 1500 bytes for standard Ethernet).
  The maximum Ethernet *frame length* includes L2 overhead:
  Ethernet header (14 bytes) + optional VLAN tags (4 bytes each) + CRC (4 bytes).
  The overhead varies per device depending on supported encapsulations (VLAN, QinQ, etc.).
  Confusing MTU with frame length produces off-by-14-to-22-byte errors in packet size limits,
  buffer sizing, and scattered Rx decisions.

  **VLAN tag accounting:** The outer VLAN tag is L2 overhead
  and does NOT count toward MTU (matching Linux and FreeBSD).
  A 1522-byte single-tagged frame is valid at MTU 1500.
  However, in QinQ the inner (customer) tag DOES consume MTU --
  it is part of the customer frame.
  So QinQ with MTU 1500 allows only 1496 bytes of L3 payload
  unless the port MTU is raised to 1504.

  **Using `rxmode.mtu` after configure:** After `rte_eth_dev_configure()` completes,
  the canonical MTU is stored in `dev->data->mtu`.
  The `dev->data->dev_conf.rxmode.mtu` field is the user's *request*
  and must not be read after configure --
  it becomes stale if `rte_eth_dev_set_mtu()` is called later.
  Both configure and set_mtu write to `dev->data->mtu`; PMDs should always read from there.

  **Overhead calculation:** Do not hardcode a single overhead constant.
  Use the device's own overhead calculation
  (typically available via `dev_info.max_rx_pktlen - dev_info.max_mtu`
  or an internal `eth_overhead` field).
  Different devices support different encapsulations, so the overhead is not a universal constant.

  **Scattered Rx decision:** PMDs compare maximum frame length (MTU + per-device overhead)
  against Rx buffer size to decide whether scattered Rx is needed.
  Comparing raw MTU against buffer size is wrong --
  it underestimates the actual frame size by the overhead.
  ```c
  /* BAD - MTU used where frame length is needed */
  if (dev->data->mtu > rxq->buf_size)
      enable_scattered_rx();

  /* BAD - hardcoded overhead, wrong for QinQ-capable devices */
  #define ETHER_OVERHEAD 18  /* may be 22 or 26 for VLAN/QinQ */
  max_frame = mtu + ETHER_OVERHEAD;

  /* BAD - reading rxmode.mtu after configure (stale if set_mtu called) */
  static int
  mydrv_rx_queue_setup(...) {
      mtu = dev->data->dev_conf.rxmode.mtu;  /* WRONG - may be stale */
      ...
  }

  /* GOOD - use dev->data->mtu, the canonical post-configure value */
  static int
  mydrv_rx_queue_setup(...) {
      uint16_t mtu = dev->data->mtu;
      ...
  }

  /* GOOD - use per-device overhead for frame length calculation */
  uint32_t frame_overhead = dev_info.max_rx_pktlen - dev_info.max_mtu;
  uint32_t max_frame_len = dev->data->mtu + frame_overhead;
  if (max_frame_len > rxq->buf_size)
      enable_scattered_rx();

  /* GOOD - device-specific overhead constant derived from capabilities */
  static uint32_t
  mydrv_eth_overhead(struct rte_eth_dev *dev) {
      uint32_t overhead = RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
      if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN)
          overhead += RTE_VLAN_HLEN;
      if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_QINQ)
          overhead += RTE_VLAN_HLEN;
      return overhead;
  }
  ```
  Note: In `rte_eth_dev_configure()` itself, reading `rxmode.mtu` is correct --
  that is where the user's request is consumed and written to `dev->data->mtu`.
  Only flag reads of `rxmode.mtu` *outside* configure (queue setup, start, link update, MTU set, etc.).
- **Missing scatter Rx for large MTU**:
  When the configured MTU produces a frame size (MTU + Ethernet overhead) larger
  than the mbuf data buffer size (`rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM`),
  the PMD MUST either enable scatter Rx (multi-segment receive) or reject the configuration.
  Silently accepting the MTU and then truncating or dropping oversized packets is a correctness bug.
  ```c
  /* BAD - accepts MTU but will truncate packets that don't fit */
  static int
  mydrv_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
  {
      /* No check against mbuf size or scatter capability */
      dev->data->mtu = mtu;
      return 0;
  }

  /* BAD - rejects valid MTU even though scatter is enabled */
  if (frame_size > mbuf_data_size)
      return -EINVAL;  /* wrong: should allow if scatter is on */

  /* GOOD - check scatter and mbuf size */
  if (!dev->data->scattered_rx &&
      frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)
      return -EINVAL;

  /* GOOD - auto-enable scatter when needed */
  if (frame_size > mbuf_data_size) {
      if (!(dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_SCATTER))
          return -EINVAL;
      dev->data->dev_conf.rxmode.offloads |=
          RTE_ETH_RX_OFFLOAD_SCATTER;
      dev->data->scattered_rx = 1;
  }
  ```
  Key relationships:
  - `dev_info.max_rx_pktlen`: maximum frame the hardware can receive
  - `dev_info.max_mtu`: maximum MTU = `max_rx_pktlen` - overhead
  - `dev_info.min_rx_bufsize`: minimum Rx buffer the HW requires
  - `dev_info.max_rx_bufsize`: maximum single-descriptor buffer size
  - `mbuf data size = rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM`
  - When scatter is off: frame length must fit in a single mbuf
  - When scatter is on: frame length can span multiple mbufs;
    the PMD selects a scattered Rx function

  This pattern should be checked in three places:
  1. `dev_configure()` -- validate MTU against mbuf size / scatter
  2. `rx_queue_setup()` -- select scattered vs non-scattered Rx path
  3. `mtu_set()` -- runtime MTU change must re-validate
- **Rx queue function selection ignoring scatter**:
  When a PMD has separate fast-path Rx functions for scalar (single-segment)
  and scattered (multi-segment) modes,
  it must select the scattered variant whenever `dev->data->scattered_rx` is set OR
  when the configured frame length exceeds the single mbuf data size.
  Failing to do so causes the scalar Rx function to silently drop or corrupt multi-segment packets.
  ```c
  /* BAD - only checks offload flag, ignores actual need */
  if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
      rx_func = mydrv_recv_scattered;
  else
      rx_func = mydrv_recv_single;  /* will drop oversized pkts */

  /* GOOD - check both the flag and the size */
  mbuf_size = rte_pktmbuf_data_room_size(rxq->mp) -
              RTE_PKTMBUF_HEADROOM;
  max_pkt = dev->data->mtu + overhead;
  if ((rxmode->offloads & RTE_ETH_RX_OFFLOAD_SCATTER) ||
      max_pkt > mbuf_size) {
      dev->data->scattered_rx = 1;
      rx_func = mydrv_recv_scattered;
  } else {
      rx_func = mydrv_recv_single;
  }
  ```

### Architecture & Patterns
- Code that violates existing patterns in the code base
- Missing error handling
- Code that is not safe against signals

### New Library API Design

When a patch adds a new library under `lib/`, review API design in addition to correctness and style.

**API boundary.** A library should be a compiler, not a framework.
The model is `rte_acl`: create a context, feed input, get structured output,
caller decides what to do with it.
No callbacks needed.
If the library requires callers to implement a callback table to function,
the boundary is wrong -- the library is asking the caller to be its backend.

**Callback structs** (Warning / Error).
Any function-pointer struct in an installed header is an ABI break waiting to happen.
Adding or reordering a member breaks all consumers.
- Prefer a single callback parameter over an ops table.
- \>5 callbacks: **Warning** -- likely needs redesign.
- \>20 callbacks: **Error** -- this is an app plugin API, not a library.
- All callbacks must have Doxygen (contract, return values, ownership).
- Void-returning callbacks for failable operations swallow errors -- flag as **Error**.
- Callbacks serving app-specific needs (e.g. `verbose_level_get`)
  indicate wrong code was extracted into the library.

**Extensible structures.** Prefer TLV / tagged-array patterns over enum + union,
following `rte_flow_item` and `rte_flow_action` as the model.
Type tag + pointer to type-specific data allows adding types without ABI breaks.
Flag as **Warning**:
- Large enums (100+) consumers must switch on.
- Unions that grow with every new feature.
- Ask: "What changes when a feature is added next release?"
  If "add an enum value and union arm" -- should be TLV.

**Installed headers.** If it's in `headers` or `indirect_headers` in meson.build, it's public API.
Don't call it "private."
If truly internal, don't install it.

**Global state.** Prefer handle-based API (`create`/`destroy`) over singletons.
`rte_acl` allows multiple independent classifier instances; new libraries should do the same.

**Output ownership.** Prefer caller-allocated or library-allocated- caller-freed
over internal static buffers.
If static buffers are used, document lifetime
and ensure Doxygen examples don't show stale-pointer usage.

---

## C Coding Style

### General Formatting

- **Tab width**:
  8 characters (hard tabs for indentation, spaces for alignment)
- **No trailing whitespace** on lines or at end of files
- Files must end with a new line
- Code style should be consistent within each file

### Comments

```c
/* Most single-line comments look like this. */

/*
 * VERY important single-line comments look like this.
 */

/*
 * Multi-line comments look like this. Make them real sentences. Fill
 * them so they look like real paragraphs.
 */
```

### Header File Organization

Include order (each group separated by blank line):
1. System/libc includes
2. DPDK EAL includes
3. DPDK misc library includes
4. Application-specific includes

```c
#include <stdio.h>
#include <stdlib.h>

#include <rte_eal.h>

#include <rte_ring.h>
#include <rte_mempool.h>

#include "application.h"
```

### Header Guards

```c
#ifndef _FILE_H_
#define _FILE_H_

/* Code */

#endif /* _FILE_H_ */
```

### Naming Conventions

- **All external symbols** must have `RTE_` or `rte_` prefix
- **Macros**:
  ALL_UPPERCASE with `RTE_` prefix
- **Functions**:
  lowercase with underscores only (no CamelCase)
- **Variables**:
  lowercase with underscores only
- **Enum values**:
  ALL_UPPERCASE with `RTE_<ENUM>_` prefix

**Exception**: Driver base directories (`drivers/*/base/`) may use different naming conventions
when sharing code across platforms or with upstream vendor code.

#### Symbol Naming for Static Linking

Drivers and libraries must not expose global variables that could clash
when statically linked with other DPDK components or applications.
Use consistent and unique prefixes for all exported symbols to avoid namespace collisions.

**Good practice**: Use a driver-specific or library-specific prefix for all global variables:

```c
/* Good - virtio driver uses consistent "virtio_" prefix */
const struct virtio_ops virtio_legacy_ops = {
	.read = virtio_legacy_read,
	.write = virtio_legacy_write,
	.configure = virtio_legacy_configure,
};

const struct virtio_ops virtio_modern_ops = {
	.read = virtio_modern_read,
	.write = virtio_modern_write,
	.configure = virtio_modern_configure,
};

/* Good - mlx5 driver uses consistent "mlx5_" prefix */
struct mlx5_flow_driver_ops mlx5_flow_dv_ops;
```

**Bad practice**: Generic names that may clash:

```c
/* Bad - "ops" is too generic, will clash with other drivers */
const struct virtio_ops ops = { ... };

/* Bad - "legacy_ops" could clash with other legacy implementations */
const struct virtio_ops legacy_ops = { ... };

/* Bad - "driver_config" is not unique */
struct driver_config config;
```

**Guidelines**:
- Prefix all global variables with the driver or library name (e.g., `virtio_`, `mlx5_`, `ixgbe_`)
- Prefix all global functions similarly unless they use the `rte_` namespace
- Internal static variables do not require prefixes as they have file scope
- Consider using the `RTE_` or `rte_` prefix only for symbols that are part of the public DPDK API

#### Prohibited Terminology

Do not use non-inclusive naming including:
- `master/slave` -> Use: primary/secondary, controller/worker, leader/follower
- `blacklist/whitelist` -> Use: denylist/allowlist, blocklist/passlist
- `cripple` -> Use: impacted, degraded, restricted, immobilized
- `tribe` -> Use: team, squad
- `sanity check` -> Use: coherence check, test, verification


### Comparisons and Boolean Logic

DPDK style requires explicit comparison against `NULL`, `0`, or `'\0'` rather than
truthiness on pointers, integers, and characters: write `if (p == NULL)` not `if (!p)`,
and `if (a != 0)` not `if (a)`.
Direct truthiness is acceptable only on actual `bool` types.
`likely()` and `unlikely()` wrappers do not change whether a comparison is explicit
(`if (likely(p != NULL))` is still explicit).

Mechanical rewrites for these are handled by coccinelle in `devtools/cocci/`;
AI review need only catch cases cocci can't see (e.g. macro-expanded conditions).

### Boolean Usage

Prefer `bool` (from `<stdbool.h>`) over `int` for variables, parameters,
and return values that are purely true/false.
Using `bool` makes intent explicit, enables compiler diagnostics for misuse, and is self-documenting.

```c
/* Bad - int used as boolean flag */
int verbose = 0;
int is_enabled = 1;

int
check_valid(struct item *item)
{
	if (item->flags & ITEM_VALID)
		return 1;
	return 0;
}

/* Good - bool communicates intent */
bool verbose = false;
bool is_enabled = true;

bool
check_valid(struct item *item)
{
	return item->flags & ITEM_VALID;
}
```

**Guidelines:**
- Use `bool` for variables that only hold true/false values
- Use `bool` return type for predicate functions (functions that answer a yes/no question,
  often named `is_*`, `has_*`, `can_*`)
- Use `true`/`false` rather than `1`/`0` for boolean assignments
- Boolean variables and parameters should not use explicit comparison:
  `if (verbose)` is correct, not `if (verbose == true)`
- `int` is still appropriate when a value can be negative, is an error code,
  or carries more than two states

**Structure fields:**
- `bool` occupies 1 byte.
  In packed or cache-critical structures, consider using a bitfield or flags word instead
- For configuration structures and non-hot-path data, `bool` is preferred over `int` for flag fields

```c
/* Bad - int flags waste space and obscure intent */
struct port_config {
	int promiscuous;     /* 0 or 1 */
	int link_up;         /* 0 or 1 */
	int autoneg;         /* 0 or 1 */
	uint16_t mtu;
};

/* Good - bool for flag fields */
struct port_config {
	bool promiscuous;
	bool link_up;
	bool autoneg;
	uint16_t mtu;
};

/* Also good - bitfield for cache-critical structures */
struct fast_path_config {
	uint32_t flags;      /* bitmask of CONFIG_F_* */
	/* ... hot-path fields ... */
};
```

**Do NOT flag:**
- `int` return type for functions that return error codes (0 for success, negative for error) --
  these are NOT boolean
- `int` used for tri-state or multi-state values
- `int` flags in existing code where changing the type would be a large, unrelated refactor
- Bitfield or flags-word approaches in performance-critical structures

### Indentation and Braces

```c
/* Control statements - no braces for single statements */
if (val != NULL)
	val = realloc(val, newsize);

/* Braces on same line as else */
if (test)
	stmt;
else if (bar) {
	stmt;
	stmt;
} else
	stmt;

/* Switch statements - don't indent case */
switch (ch) {
case 'a':
	aflag = 1;
	/* FALLTHROUGH */
case 'b':
	bflag = 1;
	break;
default:
	usage();
}

/* Long conditions - double indent continuation */
if (really_long_variable_name_1 == really_long_variable_name_2 &&
		really_long_variable_name_3 == really_long_variable_name_4)
	stmt;
```

### Variable Declarations

- Prefer declaring variables inside the basic block where they are used
- Variables may be declared either at the start of the block, or at point of first use (C99 style)
- Both declaration styles are acceptable; consistency within a function is preferred
- Initialize variables only when a meaningful value exists at declaration time
- Use C99 designated initializers for structures

```c
/* Good - declaration at start of block */
int ret;
ret = some_function();

/* Also good - declaration at point of use (C99 style) */
for (int i = 0; i < count; i++)
	process(i);

/* Good - declaration in inner block where variable is used */
if (condition) {
	int local_val = compute();
	use(local_val);
}

/* Bad - unnecessary initialization defeats compiler warnings */
int ret = 0;
ret = some_function();    /* Compiler won't warn if assignment removed */
```

### Function Format

- Return type on its own line
- Opening brace on its own line
- Place an empty line between declarations and statements

```c
static char *
function(int a1, int b1)
{
	char *p;

	p = do_something(a1, b1);
	return p;
}
```

---

## Unnecessary Code Patterns

The following patterns add unnecessary code, hide bugs, or reduce performance.
Avoid them.
Several closely related patterns are caught by coccinelle scripts in `devtools/cocci/`
(unnecessary `void *` casts, NULL checks before `free()`/`rte_free()`,
`memset()` before `free()`, zero-length array members, dead variable initialization);
those are not duplicated here.

### Appropriate Use of rte_malloc()

`rte_malloc()` allocates from hugepage memory.
Use it only when required:

- Memory that will be accessed by DMA (NIC descriptors, packet buffers)
- Memory shared between primary and secondary DPDK processes
- Memory requiring specific NUMA node placement

For general allocations, use standard `malloc()` which is faster
and does not consume limited hugepage resources.

Queue-related buffers (descriptor rings, Rx/Tx queue control structures)
should use `rte_zmalloc_socket()` rather
than plain `rte_malloc()`: zero-initialization avoids stale descriptor bugs,
the `_socket` variant ensures NUMA-local allocation,
and hugepage backing makes the memory visible to secondary processes.

```c
/* Bad - rte_malloc for ordinary data structure */
struct config *cfg = rte_malloc(NULL, sizeof(*cfg), 0);

/* Good - standard malloc for control structures */
struct config *cfg = malloc(sizeof(*cfg));

/* Good - rte_zmalloc_socket for descriptor rings / queue structures */
struct my_rx_desc *ring = rte_zmalloc_socket("rx_ring",
    n * sizeof(*ring), RTE_CACHE_LINE_SIZE, socket_id);
```

### Appropriate Use of rte_memcpy()

`rte_memcpy()` is optimized for bulk data transfer in the fast path.
For general use, standard `memcpy()` is preferred because:

- Modern compilers optimize `memcpy()` effectively
- `memcpy()` includes bounds checking with `_FORTIFY_SOURCE`
- `memcpy()` handles small fixed-size copies efficiently

```c
/* Bad - rte_memcpy in control path */
rte_memcpy(&config, &default_config, sizeof(config));

/* Good - standard memcpy for control path */
memcpy(&config, &default_config, sizeof(config));

/* Good - rte_memcpy for packet data in fast path */
rte_memcpy(rte_pktmbuf_mtod(m, void *), payload, len);
```

### Non-const Function Pointer Arrays

Arrays of function pointers (ops tables, dispatch tables, callback arrays) should be declared `const`
when their contents are fixed at compile time.
A non-`const` function pointer array can be overwritten by bugs or exploits,
and prevents the compiler from placing the table in read-only memory.

```c
/* Bad - mutable when it doesn't need to be */
static rte_rx_burst_t rx_functions[] = {
	rx_burst_scalar,
	rx_burst_vec_avx2,
	rx_burst_vec_avx512,
};

/* Good - immutable dispatch table */
static const rte_rx_burst_t rx_functions[] = {
	rx_burst_scalar,
	rx_burst_vec_avx2,
	rx_burst_vec_avx512,
};
```

**Exceptions** (do NOT flag):
- Arrays modified at runtime for CPU feature detection or capability probing (e.g.,
  selecting a burst function based on `rte_cpu_get_flag_enabled()`)
- Arrays containing mutable state (e.g., entries that are linked into lists)
- Arrays populated dynamically via registration API
- `dev_ops` or similar structures assigned per-device at init time

Only flag when the array is fully initialized at declaration with constant values
and never modified thereafter.

---

## Forbidden Tokens

### Functions

Forbidden function additions (`rte_panic()`, `rte_exit()`, `perror()`, `printf()`,
`fprintf()`, `getenv()` in `lib/` and `drivers/`, with appropriate per-function
exceptions for EAL, `examples/`, and `app/test/`) are caught by `devtools/checkpatches.sh`.

### Atomics and Memory Barriers

Substitutions from the deprecated atomic API to the C11 DPDK wrappers
(`__atomic_*`/`__ATOMIC_*` -> `rte_atomic_*_explicit()`/`rte_memory_order_*`,
`__sync_*` -> `rte_atomic_*`, `rte_smp_{r,w,}mb()` -> `rte_atomic_thread_fence()`,
`rte_atomic{16,32,64}_*()` -> `rte_atomic_*_explicit()`)
are handled by coccinelle scripts in `devtools/cocci/`.
AI review only needs to flag conceptual misuse, covered in the sections below.

#### Shared Variable Access: volatile vs Atomics

Variables shared between threads or between a thread and a signal handler **must** use atomic operations.
The C `volatile` keyword is NOT a substitute for atomics
-- it prevents compiler optimization of accesses but provides no atomicity guarantees
and no memory ordering between threads.
On some architectures, `volatile` reads and writes may tear on unaligned or multi-word values.

DPDK provides C11 atomic wrappers that are portable across all supported compilers and architectures.
Always use these for shared state.

**Reading shared variables:**

```c
/* BAD - volatile provides no atomicity or ordering guarantee */
volatile int stop_flag;
if (stop_flag)           /* data race, compiler/CPU can reorder */
    return;

/* BAD - direct access to shared variable without atomic */
if (shared->running)     /* undefined behavior if another thread writes */
    process();

/* GOOD - DPDK C11 atomic wrapper */
if (rte_atomic_load_explicit(&shared->stop_flag, rte_memory_order_acquire))
    return;

/* GOOD - relaxed is fine for statistics or polling a flag where
 * you don't need to synchronize other memory accesses */
count = rte_atomic_load_explicit(&shared->count, rte_memory_order_relaxed);
```

**Writing shared variables:**

```c
/* BAD - volatile write */
volatile int *flag = &shared->ready;
*flag = 1;

/* GOOD - atomic store with appropriate ordering */
rte_atomic_store_explicit(&shared->ready, 1, rte_memory_order_release);
```

**Read-modify-write operations:**

```c
/* BAD - not atomic even with volatile */
volatile uint64_t *counter = &stats->packets;
*counter += nb_rx;       /* TOCTOU: load, add, store is 3 operations */

/* GOOD - atomic add */
rte_atomic_fetch_add_explicit(&stats->packets, nb_rx,
    rte_memory_order_relaxed);
```

#### Memory Ordering Guide

Use the weakest ordering that is correct.
Stronger ordering constrains hardware and compiler optimization unnecessarily.

| DPDK Ordering | When to Use |
|---------------|-------------|
| `rte_memory_order_relaxed` | Statistics counters, polling flags where no other data depends on the value. Most common for simple counters. |
| `rte_memory_order_acquire` | **Load** side of a flag/pointer that guards access to other shared data. Ensures subsequent reads see data published by the releasing thread. |
| `rte_memory_order_release` | **Store** side of a flag/pointer that publishes shared data. Ensures all prior writes are visible to a thread that does an acquire load. |
| `rte_memory_order_acq_rel` | Read-modify-write operations (e.g., `fetch_add`) that both consume and publish shared state in one operation. |
| `rte_memory_order_seq_cst` | Rarely needed. Only when multiple independent atomic variables must be observed in a globally consistent total order. Avoid unless required. |

**Common pattern -- producer/consumer flag:**

```c
/* Producer thread: fill buffer, then signal ready */
fill_buffer(buf, data, len);
rte_atomic_store_explicit(&shared->ready, 1, rte_memory_order_release);

/* Consumer thread: wait for flag, then read buffer */
while (!rte_atomic_load_explicit(&shared->ready, rte_memory_order_acquire))
    rte_pause();
process_buffer(buf, len);  /* guaranteed to see producer's writes */
```

**Common pattern -- statistics counter (no ordering needed):**

```c
rte_atomic_fetch_add_explicit(&port_stats->rx_packets, nb_rx,
    rte_memory_order_relaxed);
```

#### Standalone Fences

Prefer ordering on the atomic operation itself (acquire load, release store) over standalone fences.
Standalone fences (`rte_atomic_thread_fence()`) are a blunt instrument
that orders ALL memory accesses around the fence, not just the atomic variable you care about.

```c
/* Acceptable but less precise - standalone fence */
rte_atomic_store_explicit(&shared->flag, 1, rte_memory_order_relaxed);
rte_atomic_thread_fence(rte_memory_order_release);

/* Preferred - ordering on the operation itself */
rte_atomic_store_explicit(&shared->flag, 1, rte_memory_order_release);
```

Standalone fences are appropriate when synchronizing multiple non-atomic writes (e.g.,
filling a structure before publishing a pointer to it) where annotating each write individually is impractical.

#### When volatile Is Still Acceptable

`volatile` remains correct for:
- Memory-mapped I/O registers (hardware MMIO)
- Variables shared with signal handlers in single-threaded contexts
- Interaction with `setjmp`/`longjmp`

`volatile` is NOT correct for:
- Any variable accessed by multiple threads
- Polling flags between lcores
- Statistics counters updated from multiple threads
- Flags set by one thread and read by another

**Do NOT flag** `volatile` used for MMIO or hardware register access (common in drivers under `drivers/*/base/`).

### Threading

Substitutions from `pthread_*()` to the DPDK `rte_thread_*()` wrappers,
and from `rte_thread_set_name()`/`rte_thread_create_control()` to their
`_prefixed_name`/`_internal_control` replacements,
are handled by coccinelle in `devtools/cocci/`.

### Process-Shared Synchronization

When placing synchronization primitives in shared memory (memory accessible by multiple processes,
such as DPDK primary/secondary processes or `mmap`'d regions),
they **must** be initialized with process-shared attributes.
Failure to do so causes **undefined behavior** that may appear to work in testing
but fail unpredictably in production.

#### pthread Mutexes in Shared Memory

**This is an error** - mutex in shared memory without `PTHREAD_PROCESS_SHARED`:

```c
/* BAD - undefined behavior when used across processes */
struct shared_data {
	pthread_mutex_t lock;
	int counter;
};

void init_shared(struct shared_data *shm) {
	pthread_mutex_init(&shm->lock, NULL);  /* ERROR: missing pshared attribute */
}
```

**Correct implementation**:

```c
/* GOOD - properly initialized for cross-process use */
struct shared_data {
	pthread_mutex_t lock;
	int counter;
};

void init_shared(struct shared_data *shm) {
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&shm->lock, &attr);
	pthread_mutexattr_destroy(&attr);
}
```

#### pthread Condition Variables in Shared Memory

Condition variables also require the process-shared attribute:

```c
/* BAD - will not work correctly across processes */
pthread_cond_init(&shm->cond, NULL);

/* GOOD */
pthread_condattr_t cattr;
pthread_condattr_init(&cattr);
pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
pthread_cond_init(&shm->cond, &cattr);
pthread_condattr_destroy(&cattr);
```

#### pthread Read-Write Locks in Shared Memory

```c
/* BAD */
pthread_rwlock_init(&shm->rwlock, NULL);

/* GOOD */
pthread_rwlockattr_t rwattr;
pthread_rwlockattr_init(&rwattr);
pthread_rwlockattr_setpshared(&rwattr, PTHREAD_PROCESS_SHARED);
pthread_rwlock_init(&shm->rwlock, &rwattr);
pthread_rwlockattr_destroy(&rwattr);
```

#### When to Flag This Issue

Flag as an **Error** when ALL of the following are true:
1. A `pthread_mutex_t`, `pthread_cond_t`, `pthread_rwlock_t`, or `pthread_barrier_t` is initialized
2. The primitive is stored in shared memory (identified by context such as:
   structure in `rte_malloc`/`rte_memzone`, `mmap`'d memory, memory passed to secondary processes,
   or structures documented as shared)
3. The initialization uses `NULL` attributes or attributes without `PTHREAD_PROCESS_SHARED`

**Do NOT flag** when:
- The mutex is in thread-local or process-private heap memory (`malloc`)
- The mutex is a local/static variable not in shared memory
- The code already uses `pthread_mutexattr_setpshared()` with `PTHREAD_PROCESS_SHARED`
- The synchronization uses DPDK primitives (`rte_spinlock_t`, `rte_rwlock_t`) which are designed for shared memory

#### Preferred Alternatives

For DPDK code, prefer DPDK's own synchronization primitives which are designed for shared memory:

| pthread Primitive | DPDK Alternative |
|-------------------|------------------|
| `pthread_mutex_t` | `rte_spinlock_t` (busy-wait) or properly initialized pthread mutex |
| `pthread_rwlock_t` | `rte_rwlock_t` |
| `pthread_spinlock_t` | `rte_spinlock_t` |

Note: `rte_spinlock_t` and `rte_rwlock_t` work correctly in shared memory without special initialization,
but they are spinning locks unsuitable for long wait times.

### Compiler Built-ins, Attributes, Format Specifiers, and Test Macros

Mechanical substitutions handled by coccinelle in `devtools/cocci/`:
`__attribute__` -> RTE macros in `rte_common.h` (except in `lib/eal/include/rte_common.h`);
`__alignof__` -> C11 `alignof`; `__typeof__` -> `typeof`;
`__builtin_*` -> EAL macros (except in `lib/eal/` and `drivers/*/base/`);
`__reserved` identifier -> renamed (reserved in Windows headers);
`%lld`/`%llu`/`%llx` -> `%PRId64`/`%PRIu64`/`%PRIx64`;
`REGISTER_TEST_COMMAND` -> `REGISTER_<suite_name>_TEST`.

Also avoid `#pragma` / `_Pragma` outside `rte_common.h`.

### Headers and Build

| Forbidden | Preferred | Context |
|-----------|-----------|---------|
| `#include <linux/pci_regs.h>` | `#include <rte_pci.h>` | |
| `install_headers()` | Meson `headers` variable | meson.build |
| `-DALLOW_EXPERIMENTAL_API` | Not in lib/drivers/app | Build flags |
| `allow_experimental_apis` | Not in lib/drivers/app | Meson |
| `#undef XXX` | `// XXX is not set` | config/rte_config.h |
| Driver headers (`*_driver.h`, `*_pmd.h`) | Public API headers | app/, examples/ |

### Documentation

| Forbidden | Preferred |
|-----------|-----------|
| `http://...dpdk.org` | `https://...dpdk.org` |
| `//doc.dpdk.org/guides/...` | `:ref:` or `:doc:` Sphinx references |
| `::  file.svg` | `::  file.*` (wildcard extension) |

---

## Deprecated API Usage

New patches must not introduce usage of deprecated API, macros, or functions.
Deprecated items are marked with `RTE_DEPRECATED`
or documented in the deprecation notices section of the release notes.

### Rules for New Code

- Do not call functions marked with `RTE_DEPRECATED` or `__rte_deprecated`
- Do not use macros that have been superseded by newer alternatives
- Do not use data structures or enum values marked as deprecated
- Check `doc/guides/rel_notes/deprecation.rst` for planned deprecations
- When a deprecated API has a replacement, use the replacement

### Deprecating API

A patch may mark an API as deprecated provided:

- No remaining usages exist in the current DPDK codebase
- The deprecation is documented in the release notes
- A migration path or replacement API is documented
- The `RTE_DEPRECATED` macro is used to generate compiler warnings

```c
/* Marking a function as deprecated */
__rte_deprecated
int
rte_old_function(void);

/* With a message pointing to the replacement */
__rte_deprecated_msg("use rte_new_function() instead")
int
rte_old_function(void);
```

### Common Deprecated Patterns

`rte_atomic*_t` types, `rte_smp_*mb()` barriers, and `pthread_*()` in portable code
all have modern replacements covered in the Atomics and Threading sections above
and are rewritten by coccinelle.

When reviewing patches that add new code,
flag any usage of deprecated API as requiring change to use the modern replacement.

---

## API Tag Requirements

### `__rte_experimental`

- Must appear **alone on the line** immediately preceding the return type
- Only allowed in **header files** (not `.c` files)

```c
/* Correct */
__rte_experimental
int
rte_new_feature(void);

/* Wrong - not alone on line */
__rte_experimental int rte_new_feature(void);

/* Wrong - in .c file */
```

### `__rte_internal`

- Must appear **alone on the line** immediately preceding the return type
- Only allowed in **header files** (not `.c` files)

```c
/* Correct */
__rte_internal
int
internal_function(void);
```

### Alignment Attributes

`__rte_aligned`, `__rte_cache_aligned`, `__rte_cache_min_aligned`
may only be used with `struct` or `union` types:

```c
/* Correct */
struct __rte_cache_aligned my_struct {
	/* ... */
};

/* Wrong */
int __rte_cache_aligned my_variable;
```

### Packed Attributes

- `__rte_packed_begin` must follow `struct`, `union`, or alignment attributes
- `__rte_packed_begin` and `__rte_packed_end` must be used in pairs
- Cannot use `__rte_packed_begin` with `enum`

```c
/* Correct */
struct __rte_packed_begin my_packed_struct {
	/* ... */
} __rte_packed_end;

/* Wrong - with enum */
enum __rte_packed_begin my_enum {
	/* ... */
};
```

---

## Code Quality Requirements

### Compilation

- Each commit must compile independently (for `git bisect`)
- No forward dependencies within a patchset
- Test with multiple targets, compilers, and options
- Use `devtools/test-meson-builds.sh`

**Note for AI reviewers**: You cannot verify compilation order
or cross-patch dependencies from patch review alone.
Do NOT flag patches claiming they "would fail to compile"
based on symbols used in other patches in the series.
Assume the patch author has ordered them correctly.

### Testing

- Add tests to `app/test` unit test framework
- New API functions must be used in `/app` test directory
- New device API require at least one driver implementation

#### Functional Test Infrastructure

Standalone functional tests should use the `TEST_ASSERT` macros
and `unit_test_suite_runner` infrastructure for consistency
and proper integration with the DPDK test framework.

```c
#include <rte_test.h>

static int
test_feature_basic(void)
{
	int ret;

	ret = rte_feature_init();
	TEST_ASSERT_SUCCESS(ret, "Failed to initialize feature");

	ret = rte_feature_operation();
	TEST_ASSERT_EQUAL(ret, 0, "Operation returned unexpected value");

	TEST_ASSERT_NOT_NULL(rte_feature_get_ptr(),
		"Feature pointer should not be NULL");

	return TEST_SUCCESS;
}

static struct unit_test_suite feature_testsuite = {
	.suite_name = "feature_autotest",
	.setup = test_feature_setup,
	.teardown = test_feature_teardown,
	.unit_test_cases = {
		TEST_CASE(test_feature_basic),
		TEST_CASE(test_feature_advanced),
		TEST_CASES_END()
	}
};

static int
test_feature(void)
{
	return unit_test_suite_runner(&feature_testsuite);
}

REGISTER_FAST_TEST(feature_autotest, NOHUGE_OK, ASAN_OK, test_feature);
```

The `REGISTER_FAST_TEST` macro parameters are:
- Test name (e.g., `feature_autotest`)
- `NOHUGE_OK` or `HUGEPAGES_REQUIRED` - whether test can run without hugepages
- `ASAN_OK` or `ASAN_FAILS` - whether test is compatible with Address Sanitizer
- Test function name

Common `TEST_ASSERT` macros:
- `TEST_ASSERT(cond, msg, ...)` - Assert condition is true
- `TEST_ASSERT_SUCCESS(val, msg, ...)` - Assert value equals 0
- `TEST_ASSERT_FAIL(val, msg, ...)` - Assert value is non-zero
- `TEST_ASSERT_EQUAL(a, b, msg, ...)` - Assert two values are equal
- `TEST_ASSERT_NOT_EQUAL(a, b, msg, ...)` - Assert two values differ
- `TEST_ASSERT_NULL(val, msg, ...)` - Assert value is NULL
- `TEST_ASSERT_NOT_NULL(val, msg, ...)` - Assert value is not NULL

### Documentation

- Add Doxygen comments for public API
- Update release notes in `doc/guides/rel_notes/` for important changes
- Code and documentation must be updated atomically in same patch
- Only update the **current release** notes file
- Documentation must match the code
- PMD features must match the features matrix in `doc/guides/nics/features/`
- Documentation must match device operations
  (see `doc/guides/nics/features.rst` for the mapping between features,
  `eth_dev_ops`, and related API)
- Release notes are NOT required for:
  - Test-only changes (unit tests, functional tests)
  - Internal API and helper functions (not exported to applications)
  - Internal implementation changes that don't affect public API

### RST Documentation Style

When reviewing `.rst` documentation files, prefer **definition lists** over simple bullet lists
where each item has a term and a description.
Definition lists produce better-structured HTML/PDF output and are easier to scan.

**When to suggest a definition list:**
- A bullet list where each item starts with a bold or emphasized term followed by a dash, colon, or long explanation
- Lists of options, parameters, configuration values, or features where each entry has a name and a description
- Glossary-style enumerations

**When a simple list is fine (do NOT flag):**
- Short lists of items without descriptions (e.g., file names, steps)
- Lists where items are single phrases or sentences with no term/definition structure
- Enumerated steps in a procedure

**RST definition list syntax:**

```rst
term 1
   Description of term 1.

term 2
   Description of term 2.
   Can span multiple lines.
```

**Example -- flag this pattern:**

```rst
* **error** - Fail with error (default)
* **truncate** - Truncate content to fit token limit
* **summary** - Request high-level summary review
```

**Suggest rewriting as:**

```rst
error
   Fail with error (default).

truncate
   Truncate content to fit token limit.

summary
   Request high-level summary review.
```

This is a **Warning**-level suggestion, not an Error.
Do not flag it when the existing list structure is appropriate
(see "when a simple list is fine" above).

### API and Driver Changes

- New API functions must be marked as `__rte_experimental`
- New API functions must have hooks in `app/testpmd` and tests in the functional test suite
- Changes to existing API require release notes
- New drivers or subsystems must have release notes
- Internal API (used only within DPDK, not exported to applications) do NOT require release notes

### ABI Compatibility and Symbol Exports

**IMPORTANT**: DPDK uses automatic symbol map generation.
Do **NOT** recommend manually editing `version.map` files -
they are auto-generated from source code annotations.

#### Symbol Export Macros

New public functions must be annotated with export macros (defined in `rte_export.h`).
Place the macro on the line immediately before the function definition in the `.c` file:

```c
/* For stable ABI symbols */
RTE_EXPORT_SYMBOL(rte_foo_create)
int
rte_foo_create(struct rte_foo_config *config)
{
    /* ... */
}

/* For experimental symbols (include version when first added) */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_foo_new_feature, 25.03)
__rte_experimental
int
rte_foo_new_feature(void)
{
    /* ... */
}

/* For internal symbols (shared between DPDK components only) */
RTE_EXPORT_INTERNAL_SYMBOL(rte_foo_internal_helper)
int
rte_foo_internal_helper(void)
{
    /* ... */
}
```

#### Symbol Export Rules

- `RTE_EXPORT_SYMBOL` - Use for stable ABI functions
- `RTE_EXPORT_EXPERIMENTAL_SYMBOL(name, ver)` -
  Use for new experimental API functions (version is the DPDK release, e.g., `25.03`)
- `RTE_EXPORT_INTERNAL_SYMBOL` -
  Use for functions shared between DPDK libs/drivers but not part of public API
- Export macros go in `.c` files, not headers
- The build system generates linker version maps automatically

#### What NOT to Review

- Do **NOT** flag missing `version.map` updates - maps are auto-generated
- Do **NOT** suggest adding symbols to `lib/*/version.map` files

#### ABI Versioning for Changed Functions

When changing the signature of an existing stable function,
use versioning macros from `rte_function_versioning.h`:

- `RTE_VERSION_SYMBOL` - Create versioned symbol for backward compatibility
- `RTE_DEFAULT_SYMBOL` - Mark the new default version

Follow ABI policy and versioning guidelines in the contributor documentation.
Enable ABI checks with `DPDK_ABI_REF_VERSION` environment variable.

---

## LTS (Long Term Stable) Release Review

LTS releases are DPDK versions ending in `.11` (e.g., 23.11, 22.11, 21.11, 20.11, 19.11).
When reviewing patches targeting an LTS branch, apply stricter criteria:

### LTS-Specific Rules

- **Only bug fixes allowed** -- no new features
- **No new API** (experimental or stable)
- **ABI must remain unchanged** -- no symbol additions, removals, or signature changes
- Backported fixes should reference the original commit with a `Fixes:` tag
- Copyright years should reflect when the code was originally written
- Be conservative: reject changes that are not clearly bug fixes

### What to Flag on LTS Branches

**Error:**
- New feature code (new functions, new driver capabilities)
- New experimental or stable API additions
- ABI changes (new or removed symbols, changed function signatures)
- Changes that add new configuration options or parameters

**Warning:**
- Large refactoring that goes beyond what is needed for a fix
- Missing `Fixes:` tag on a backported bug fix
- Missing `Cc: stable@dpdk.org`

### When LTS Rules Apply

LTS rules apply when the reviewer is told the target release is an LTS version
(via the `--release` option or equivalent).
If no release is specified, assume the patch targets the main development branch
where new features and API are allowed.

---

## Patch Validation Checklist

### Commit Message and License

Checked by `devtools/checkpatches.sh` -- not duplicated here.

### Code Style

- [ ] Lines <=100 characters
- [ ] Hard tabs for indentation, spaces for alignment
- [ ] No trailing whitespace
- [ ] Proper include order
- [ ] Header guards present
- [ ] `rte_`/`RTE_` prefix on external symbols
- [ ] Driver/library global variables use unique prefixes (e.g., `virtio_`, `mlx5_`)
- [ ] No prohibited terminology
- [ ] Proper brace style
- [ ] Function return type on own line
- [ ] No forbidden tokens (see table above)
- [ ] No usage of deprecated API, macros, or functions
- [ ] Process-shared primitives in shared memory use `PTHREAD_PROCESS_SHARED`
- [ ] Statistics use `+=` not `=` for accumulation
- [ ] Integer multiplies widened before operation when result is 64-bit
- [ ] Descriptor chain traversals bounded by ring size or loop counter
- [ ] 64-bit bitmasks use `1ULL <<` or `RTE_BIT64()`, not `1 <<`
- [ ] Left shifts of `uint8_t`/`uint16_t` cast to unsigned target width before shift when result is 64-bit
- [ ] No unconditional variable overwrites before read
- [ ] Nested loops use distinct counter variables
- [ ] `rte_mbuf_raw_free_bulk()` not used on mixed-pool mbuf arrays (Tx paths, ring dequeue, error paths)
- [ ] MTU not confused with frame length (MTU = L3 payload, frame = MTU + L2 overhead)
- [ ] PMDs read `dev->data->mtu` after configure, not `dev_conf.rxmode.mtu`
- [ ] Ethernet overhead not hardcoded -- derived from device capabilities
- [ ] Scatter Rx enabled or error returned when frame length exceeds single mbuf data size
- [ ] `mtu_set` allows large MTU when scatter Rx is active; re-selects Rx burst function
- [ ] Rx queue setup selects scattered Rx function when frame length exceeds mbuf
- [ ] Static function pointer arrays declared `const` when contents are compile-time fixed
- [ ] `bool` used for pure true/false variables, parameters, and predicate return types
- [ ] Shared variables use `rte_atomic_*_explicit()`, not `volatile` or bare access
- [ ] Memory ordering is the weakest correct choice (`relaxed` for counters, `acquire`/`release` for publish/consume)

### API Tags

- [ ] `__rte_experimental` alone on line, only in headers
- [ ] `__rte_internal` alone on line, only in headers
- [ ] Alignment attributes only on struct/union
- [ ] Packed attributes properly paired
- [ ] New public functions have `RTE_EXPORT_*` macro in `.c` file
- [ ] Experimental functions use `RTE_EXPORT_EXPERIMENTAL_SYMBOL(name, version)`

### Structure

- [ ] Each commit compiles independently
- [ ] Code and docs updated together
- [ ] Documentation matches code behavior
- [ ] RST docs use definition lists for term/description patterns
- [ ] PMD features match `doc/guides/nics/features/` matrix
- [ ] Device operations match documentation (per `features.rst` mappings)
- [ ] Tests added/updated as needed
- [ ] Functional tests use TEST_ASSERT macros and unit_test_suite_runner
- [ ] New API marked as `__rte_experimental`
- [ ] New API have testpmd hooks and functional tests
- [ ] Current release notes updated for significant changes
- [ ] Release notes updated for API changes
- [ ] Release notes updated for new drivers or subsystems

---

## Meson Build Files

### Style Requirements

- 4-space indentation (no tabs)
- Line continuations double-indented
- Lists alphabetically ordered
- Short lists (<=3 items): single line, no trailing comma
- Long lists: one item per line, trailing comma on last item
- No strict line length limit for meson files; lines under 100 characters are acceptable

```python
# Short list
sources = files('file1.c', 'file2.c')

# Long list
headers = files(
	'header1.h',
	'header2.h',
	'header3.h',
)
```

---

## Python Code

- Must comply with formatting standards
- Use **`black`** for code formatting validation
- Line length acceptable up to 100 characters

---

## Validation Tools

Run these before submitting:

```bash
# Check commit messages
devtools/check-git-log.sh -n1

# Check patch format and forbidden tokens
devtools/checkpatches.sh -n1

# Check maintainers coverage
devtools/check-maintainers.sh

# Build validation
devtools/test-meson-builds.sh

# Find maintainers for your patch
devtools/get-maintainer.sh <patch-file>
```

---

## Severity Levels for AI Review

**Error** (must fix):

*Correctness bugs (highest value findings):*
- Use-after-free
- Resource leaks on error paths (memory, file descriptors, locks)
- Double-free or double-close
- NULL pointer dereference on reachable code path
- Buffer overflow or out-of-bounds access
- Missing error check on a function that can fail, leading to undefined behavior
- Race condition on shared mutable state without synchronization
- `volatile` used instead of atomics for inter-thread shared variables
- Error path that skips necessary cleanup
- Statistics accumulation using `=` instead of `+=` (overwrite vs increment)
- Integer multiply without widening cast losing upper bits (16x16, 32x32, etc.)
- Unbounded descriptor chain traversal on guest/API-supplied indices
- `1 << n` used for 64-bit bitmask (undefined behavior if n >= 32)
- Left shift of `uint8_t`/`uint16_t` used in 64-bit context without widening cast (sign extension)
- Variable assigned then unconditionally overwritten before read
- Same variable used as counter in nested loops
- `rte_mbuf_raw_free_bulk()` on mbuf array where mbufs may come from different pools (Tx burst, ring dequeue)
- MTU used where frame length is needed or vice versa (off by L2 overhead)
- `dev_conf.rxmode.mtu` read after configure instead of `dev->data->mtu` (stale value)
- MTU accepted without scatter Rx when frame size exceeds single mbuf capacity (silent truncation/drop)
- `mtu_set` rejects valid MTU when scatter Rx is already enabled
- Rx function selection ignores `scattered_rx` flag or MTU-vs-mbuf-size comparison

*Process and format errors:*
- Forbidden tokens in code
- `__rte_experimental`/`__rte_internal` in .c files or not alone on line
- Compilation failures
- ABI breaks without proper versioning
- pthread mutex/cond/rwlock in shared memory without `PTHREAD_PROCESS_SHARED`

*API design errors (new libraries only):*
- Ops/callback struct with 20+ function pointers in an installed header
- Callback struct members with no Doxygen documentation
- Void-returning callbacks for failable operations (errors silently swallowed)

**Warning** (should fix):
- Missing Cc: stable@dpdk.org for fixes
- Documentation gaps
- Documentation does not match code behavior
- PMD features missing from `doc/guides/nics/features/` matrix
- Device operations not documented per `features.rst` mappings
- Missing tests
- Functional tests not using TEST_ASSERT macros or unit_test_suite_runner
- New API not marked as `__rte_experimental`
- New API without testpmd hooks or functional tests
- New public function missing `RTE_EXPORT_*` macro
- API changes without release notes
- New drivers or subsystems without release notes
- Inappropriate use of `rte_malloc()` or `rte_memcpy()`
- Queue-related buffers (descriptor rings, Rx/Tx queue structures) allocated with `malloc()` instead of `rte_zmalloc_socket()` (breaks secondary process access, loses NUMA locality)
- Driver/library global variables without unique prefixes (static linking clash risk)
- Usage of deprecated API, macros, or functions in new code
- RST documentation using bullet lists where definition lists would be more appropriate
- Ops/callback struct with >5 function pointers in an installed header (ABI risk)
- New API using fixed enum+union where TLV pattern would be more extensible
- Installed header labeled "private" or "internal" in meson.build
- New library using global singleton instead of handle-based API
- Static function pointer array not declared `const` when contents are compile-time constant
- `int` used instead of `bool` for variables or return values that are purely true/false
- `rte_memory_order_seq_cst` used where weaker ordering (`relaxed`, `acquire`/`release`) suffices
- Standalone `rte_atomic_thread_fence()` where ordering on the atomic operation itself would be clearer
- Hardcoded Ethernet overhead constant instead of per-device overhead calculation
- PMD does not advertise `RTE_ETH_RX_OFFLOAD_SCATTER` in `rx_offload_capa` but hardware supports multi-segment Rx
- PMD `dev_info` reports `max_rx_pktlen` or `max_mtu` inconsistent with each other or with the Ethernet overhead
- `mtu_set` callback does not re-select the Rx burst function after changing MTU

**Do NOT flag** (common false positives):
- Missing `version.map` updates (maps are auto-generated from `RTE_EXPORT_*` macros)
- Suggesting manual edits to any `version.map` file
- SPDX/copyright format, copyright years, copyright holders (not subject to AI review)
- Commit message formatting (subject length, punctuation, tag order, case-sensitive terms) -- checked by checkpatch
- Meson file lines under 100 characters
- Anything you determine is correct (do not mention non-issues or say "No issue here")
- `REGISTER_FAST_TEST` using `NOHUGE_OK`/`ASAN_OK` macros (this is the correct current format)
- Missing release notes for test-only changes (unit tests do not require release notes)
- Missing release notes for internal API or helper functions (only public API need release notes)
- Any item you later correct with "(Correction: ...)" or "actually acceptable" - just omit it
- Vague concerns ("should be verified", "should be checked") - if you're not sure it's wrong, don't flag it
- Items where you say "which is correct" or "this is correct" - if it's correct, don't mention it at all
- Items where you conclude "no issue here" or "this is actually correct" - omit these entirely
- Clean patches in a series - do not include a patch just to say "no issues" or describe what it does
- Cross-patch compilation dependencies - you cannot determine patch ordering correctness from review
- Claims that a symbol "was removed in patch N" causing issues in patch M - assume author ordered correctly
- Any speculation about whether patches will compile when applied in sequence
- Mutexes/locks in process-private memory (standard `malloc`, stack, static non-shared) - these don't need `PTHREAD_PROCESS_SHARED`
- Use of `rte_spinlock_t` or `rte_rwlock_t` in shared memory (these work correctly without special init)
- `volatile` used for MMIO/hardware register access in drivers (this is correct usage)
- Left shift of `uint8_t`/`uint16_t` where the result is stored in a `uint32_t`
  or narrower variable and not used in pointer arithmetic or 64-bit context (sign extension cannot occur)
- Reading `rxmode.mtu` inside `rte_eth_dev_configure()` implementation (that is where the user request is consumed)
- `=` assignment to MTU or frame length fields during initial setup (only flag stale reads of `rxmode.mtu` outside configure)
- PMDs that auto-enable scatter when MTU exceeds mbuf size (this is the correct pattern)
- Hardcoded `RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN` as overhead when the PMD does not support VLAN
  and device info is consistent
- Tagged frames exceeding 1518 bytes at standard MTU
  -- a single-tagged frame of 1522 bytes is valid at MTU 1500 (the outer VLAN header is L2 overhead, not payload).
  Note: inner VLAN tags in QinQ *do* consume MTU; see the MTU section for details.

**Info** (consider):
- Minor style preferences
- Optimization suggestions
- Alternative approaches

---

# Response Format

When you identify an issue:
1. **State the problem** (1 sentence)
2. **Why it matters** (1 sentence, only if not obvious)
3. **Suggested fix** (code snippet or specific action)

Example: This could panic if the string is NULL.

---

## FINAL CHECK BEFORE SUBMITTING REVIEW

Before outputting your review, do two separate passes:

### Pass 1: Verify correctness bugs are included

Ask: "Did I trace every error path for resource leaks?
Did I check for use-after-free?
Did I verify error codes are propagated?"

If you identified a potential correctness bug but talked yourself out of it, **add it back**.
It is better to report a possible bug than to miss a real one.

### Pass 2: Remove style/process false positives

For EACH style/process item, ask: "Did I conclude this is actually fine/correct/acceptable/no issue?"

If YES, DELETE THAT ITEM.
It should not be in your output.

An item that says "X is wrong... actually this is correct" is a FALSE POSITIVE and must be removed.
This applies to style, format, and process items only.

**If your Errors section would be empty after this check, that's fine -- it means the patches are good.**
