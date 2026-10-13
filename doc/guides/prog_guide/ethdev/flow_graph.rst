.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2026 Intel Corporation

Flow Graph Parser
=================

Introduction
------------

The flow graph parser is a helper library for PMD drivers
that implements ``rte_flow`` pattern matching.
It lets a driver declare the protocol sequences it supports
as a directed graph of nodes and edges.
It then validates and extracts fields from an ``rte_flow_item`` pattern
in a single traversal.

The library is declared in ``flow_graph.h`` and built as part of ethdev.
It is an internal API, available to drivers only.
Parse diagnostics are logged at debug level using the ethdev log type.

Scope and Limitations
~~~~~~~~~~~~~~~~~~~~~

Because the parser is graph-based, it is well suited for matching *protocol stacks*.
These are sequences of protocol headers such as ``ETH / IPv4 / TCP``.
Any pattern that can be expressed as
"from protocol A, transitions to protocol B or C are allowed"
fits naturally into the graph model.

The library is **not** designed to cover every ``rte_flow`` item type.
Items that do not represent a position in a protocol stack
do not have a natural place in a protocol graph.
This includes conntrack state, meter color, and other metadata items.
Such items are best handled outside the graph, either before or after the graph parse call.

Defining a Graph
----------------

A graph consists of three parts:

1. An **enum** that assigns a numeric index to every node.
2. A **node array** (``struct flow_graph_node[]``) indexed by that enum.
3. An **edge array** (``struct flow_graph_edge[]``) also indexed by that enum,
   describing allowed transitions.

These parts are bundled together in a ``struct flow_graph``.

The running example used throughout this guide models the following protocol graph::

   START -> ETH -> [VLAN] -> (IPv4 | IPv6) -> [(TCP | UDP | SCTP)] -> END

Brackets ``[...]`` denote optional items.
Parentheses ``(...)`` denote a required choice between alternatives.
The key ideas are:

* ``ETH`` is required after ``START``.
* After ``ETH``, an optional ``VLAN`` may appear,
  but the pattern must then see an IP layer.
* After an IP layer, an optional transport layer may appear;
  it may be TCP, UDP, or SCTP, after which the pattern reaches ``END``.

Node Enum
~~~~~~~~~

Every node needs a stable index.
The first node **must** be at index ``FLOW_GRAPH_NODE_FIRST`` (which is 0).
This is the *start node*.
It is used only as a traversal anchor and must not carry callbacks.

.. code-block:: c

   enum example_node_id {
       EXAMPLE_NODE_START = FLOW_GRAPH_NODE_FIRST,
       EXAMPLE_NODE_ETH,
       EXAMPLE_NODE_VLAN,
       EXAMPLE_NODE_IPV4,
       EXAMPLE_NODE_IPV6,
       EXAMPLE_NODE_TCP,
       EXAMPLE_NODE_UDP,
       EXAMPLE_NODE_SCTP,
       EXAMPLE_NODE_END,
       /* keep last */
       EXAMPLE_NODE_MAX,
   };

Node Definitions
~~~~~~~~~~~~~~~~

Each node maps to one ``rte_flow_item_type``.
It can also carry a *validate* callback, a *process* callback, and a set of *constraints*.
The ``END`` node can also have callbacks to perform end-of-match processing.

A minimal skeleton (callbacks and constraints are added in later sections):

.. code-block:: c

   const struct flow_graph example_graph = {
       .nodes = (struct flow_graph_node[]){
           [EXAMPLE_NODE_START] = {
               .name = "START",
               /* Start node: no type, no callbacks */
           },
           [EXAMPLE_NODE_ETH] = {
               .name  = "ETH",
               .type  = RTE_FLOW_ITEM_TYPE_ETH,
           },
           [EXAMPLE_NODE_VLAN] = {
               .name  = "VLAN",
               .type  = RTE_FLOW_ITEM_TYPE_VLAN,
           },
           [EXAMPLE_NODE_IPV4] = {
               .name  = "IPV4",
               .type  = RTE_FLOW_ITEM_TYPE_IPV4,
           },
           [EXAMPLE_NODE_IPV6] = {
               .name  = "IPV6",
               .type  = RTE_FLOW_ITEM_TYPE_IPV6,
           },
           [EXAMPLE_NODE_TCP] = {
               .name  = "TCP",
               .type  = RTE_FLOW_ITEM_TYPE_TCP,
           },
           [EXAMPLE_NODE_UDP] = {
               .name  = "UDP",
               .type  = RTE_FLOW_ITEM_TYPE_UDP,
           },
           [EXAMPLE_NODE_SCTP] = {
               .name  = "SCTP",
               .type  = RTE_FLOW_ITEM_TYPE_SCTP,
           },
           [EXAMPLE_NODE_END] = {
               .name  = "END",
               .type  = RTE_FLOW_ITEM_TYPE_END,
           },
       },
   };

Edge Definitions
~~~~~~~~~~~~~~~~

Edges express which nodes may follow the current one.
Every edge list is terminated by the ``FLOW_GRAPH_NODE_EDGE_END`` sentinel.
All non-``END`` nodes **must** have an edge list.
The ``END`` node itself does not need one.

.. code-block:: c

   const struct flow_graph example_graph = {
       .nodes = (struct flow_graph_node[]){
           /* ... same nodes as above ... */
       },
       .edges = (struct flow_graph_edge[]){
           [EXAMPLE_NODE_START] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_ETH,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_ETH] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_VLAN,
                   EXAMPLE_NODE_IPV4,
                   EXAMPLE_NODE_IPV6,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_VLAN] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_IPV4,
                   EXAMPLE_NODE_IPV6,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_IPV4] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_TCP,
                   EXAMPLE_NODE_UDP,
                   EXAMPLE_NODE_SCTP,
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_IPV6] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_TCP,
                   EXAMPLE_NODE_UDP,
                   EXAMPLE_NODE_SCTP,
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_TCP] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_UDP] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_SCTP] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
       },
   };

Reading the edges back:

* From ``START``, the parser can only reach ``ETH``,
  which makes ``ETH`` required.
* From ``ETH``, the parser can reach ``VLAN``, ``IPV4``, or ``IPV6``,
  which makes ``VLAN`` optional.
* From ``IPV4`` or ``IPV6``, the parser can reach ``TCP``, ``UDP``, ``SCTP``, or ``END``,
  which makes the transport layer optional.

Assembling the Graph
~~~~~~~~~~~~~~~~~~~~

With nodes and edges defined inline,
assembling the graph is just a matter of combining the two arrays
into a single compound literal:

.. code-block:: c

   const struct flow_graph example_graph = {
       .nodes = (struct flow_graph_node[]){
           [EXAMPLE_NODE_START] = {
               .name = "START"
           },
           [EXAMPLE_NODE_ETH]   = {
               .name = "ETH",
               .type = RTE_FLOW_ITEM_TYPE_ETH
           },
           /* ... remaining nodes ... */
           [EXAMPLE_NODE_END]   = {
               .name = "END",
               .type = RTE_FLOW_ITEM_TYPE_END
           },
       },
       .edges = (struct flow_graph_edge[]){
           [EXAMPLE_NODE_START] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_ETH,
                   FLOW_GRAPH_NODE_EDGE_END
               }
           },
           /* ... remaining edges ... */
       },
   };

Callbacks
---------

The graph calls up to two callbacks on every visited node: *validate* and *process*.

Both callbacks share the same return convention.
On success they must return ``0``.
On failure they must call ``rte_flow_error_set`` to record a descriptive error and return its result.

Validate Callback
~~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef int (*flow_graph_node_validate_fn)(
       const void *ctx,
       const struct rte_flow_item *item,
       struct rte_flow_error *error);

This callback receives a **read-only** context pointer.
The canonical intent is that it should check whether
the item's spec, mask, and last values are acceptable for the driver.
On failure it returns the result of ``rte_flow_error_set``
(see the return convention above).

It is recommended to use this callback for **all checks that can reject a rule**.
This includes unsupported mask bits, conflicting field combinations, hardware limitations,
and other applicable criteria.

Process Callback
~~~~~~~~~~~~~~~~

.. code-block:: c

   typedef int (*flow_graph_node_process_fn)(
       void *ctx,
       const struct rte_flow_item *item,
       struct rte_flow_error *error);

This callback receives a **mutable** context pointer.
The canonical expectation is that it should extract the fields needed for hardware programming.
It should then store extracted data in the driver's context structure.
On the rare failure path it returns the result of ``rte_flow_error_set``
(see the return convention above).

It is recommended to use this callback for the **happy path**.
For example, it can copy addresses, ports, and protocol IDs into the driver context
so they can be programmed later.

Defining a Context Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The opaque ``ctx`` pointer passed to every callback is driver-defined.
A typical context accumulates the parsed protocol fields:

.. code-block:: c

   struct example_parsed_flow {
       /* L2 */
       struct rte_ether_addr dst_mac;
       bool has_vlan;
       uint16_t vlan_tci;

       /* L3 */
       bool is_ipv6;
       rte_be32_t ipv4_src;
       rte_be32_t ipv4_dst;
       uint8_t ipv6_src[16];
       uint8_t ipv6_dst[16];

       /* L4 */
       enum rte_flow_item_type l4_proto;
       rte_be16_t src_port;
       rte_be16_t dst_port;
   };

These fields are meant to reflect the structure used by the driver to program hardware with.

Callback Example
~~~~~~~~~~~~~~~~

Below is a validate/process pair for the IPv4 node.
The validate callback rejects unsupported mask bits.
The process callback copies addresses into the context:

.. code-block:: c

   static int
   example_validate_ipv4(const void *ctx __rte_unused,
                         const struct rte_flow_item *item,
                         struct rte_flow_error *error)
   {
       const struct rte_flow_item_ipv4 *mask = item->mask;

       /* spec is guaranteed by node constraints, mask may be NULL */
       if (mask == NULL)
           rte_flow_conv(RTE_FLOW_CONV_OP_ITEM_DEFAULT_MASK_PTR, &mask, sizeof(mask),
                   (const void *)(uintptr_t)item->type, NULL);

       if (mask->hdr.version_ihl ||
           mask->hdr.type_of_service ||
           mask->hdr.total_length ||
           mask->hdr.packet_id ||
           mask->hdr.fragment_offset ||
           mask->hdr.time_to_live ||
           mask->hdr.next_proto_id ||
           mask->hdr.hdr_checksum) {
           return rte_flow_error_set(error, EINVAL,
                   RTE_FLOW_ERROR_TYPE_ITEM, item,
                   "Only src/dst addresses supported");
       }
       return 0;
   }

   static int
   example_process_ipv4(void *ctx,
                        const struct rte_flow_item *item,
                        struct rte_flow_error *error __rte_unused)
   {
       struct example_parsed_flow *parsed = ctx;
       const struct rte_flow_item_ipv4 *spec = item->spec;

       parsed->is_ipv6 = false;
       if (spec != NULL) {
           parsed->ipv4_src = spec->hdr.src_addr;
           parsed->ipv4_dst = spec->hdr.dst_addr;
       }
       return 0;
   }

Add the callbacks to the node definition:

.. code-block:: c

   [EXAMPLE_NODE_IPV4] = {
       .name      = "IPV4",
       .type      = RTE_FLOW_ITEM_TYPE_IPV4,
       .validate  = example_validate_ipv4,
       .process   = example_process_ipv4,
   },

Node Constraints
----------------

Many nodes share common requirements
about which combination of ``spec``, ``mask``, and ``last`` pointers an item must carry.
Instead of checking these in every validate callback,
they can be declared via the ``constraints`` field.
The field uses ``flow_graph_node_expect`` flags.

Available constraint flags (may be ORed together):

``FLOW_GRAPH_NODE_EXPECT_NONE``
   Default. Any valid combination is accepted (empty, spec with optional mask,
   or spec and last with optional mask).

``FLOW_GRAPH_NODE_EXPECT_EMPTY``
   The item must have ``spec == NULL``, ``mask == NULL``,
   and ``last == NULL``.

``FLOW_GRAPH_NODE_EXPECT_SPEC``
   ``spec`` is required, ``mask`` is optional; ``last`` must be NULL.

``FLOW_GRAPH_NODE_EXPECT_RANGE``
   ``spec`` and ``last`` are required, ``mask`` is optional.

Items that are invalid per the rte_flow API (``mask`` or ``last`` without ``spec``)
are always rejected, regardless of node constraints.
When ``spec`` is set but ``mask`` is NULL, the item type's default mask applies
(see ``RTE_FLOW_CONV_OP_ITEM_DEFAULT_MASK_PTR``).

Multiple flags can be ORed together.
The item is accepted if **any one** of the flagged constraints is satisfied.
The purpose is to express the difference between "must" (i.e. accept ONLY this),
and "may" (i.e. accept this OR that).

For example, an IPv4 node that requires a spec, with or without an explicit mask:

.. code-block:: c

   [EXAMPLE_NODE_IPV4] = {
       .name        = "IPV4",
       .type        = RTE_FLOW_ITEM_TYPE_IPV4,
       .validate    = example_validate_ipv4,
       .process     = example_process_ipv4,
       .constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
   },

An Ethernet node that may appear empty (no spec/mask) or with spec+mask:

.. code-block:: c

   [EXAMPLE_NODE_ETH] = {
       .name        = "ETH",
       .type        = RTE_FLOW_ITEM_TYPE_ETH,
       .validate    = example_validate_eth,
       .process     = example_process_eth,
       .constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
                      FLOW_GRAPH_NODE_EXPECT_SPEC,
   },

Constraints are checked **before** the validate callback is invoked.

Ignoring Item Types
~~~~~~~~~~~~~~~~~~~

The ``ignore_nodes`` field on ``struct flow_graph`` is an optional complement to node constraints.
When the pattern may contain item types that are irrelevant to the driver,
list them in ``ignore_nodes``.
For example, metadata items like ``RTE_FLOW_ITEM_TYPE_MARK`` do not represent a protocol header.
The parser skips ignored items silently without advancing the current graph position:

.. code-block:: c

   const struct flow_graph example_graph = {
       /* ... nodes and edges ... */
       .ignore_nodes = (enum rte_flow_item_type[]){
           RTE_FLOW_ITEM_TYPE_MARK,
           RTE_FLOW_ITEM_TYPE_END,
       },
   };

``RTE_FLOW_ITEM_TYPE_VOID`` is always ignored regardless of this list.
Omit ``ignore_nodes`` entirely when no additional item types need to be skipped.

Calling the Parser
------------------

``flow_graph_parse`` walks the pattern against the graph:

.. code-block:: c

   int
   flow_graph_parse(const struct flow_graph *graph,
                        const struct rte_flow_item *pattern,
                        struct rte_flow_error *error,
                        void *ctx);

A typical call site looks like this:

.. code-block:: c

   struct example_parsed_flow parsed;
   int ret;

   memset(&parsed, 0, sizeof(parsed));

   ret = flow_graph_parse(&example_graph, pattern, error, &parsed);
   if (ret != 0)
       return ret;

   /* 'parsed' now contains the extracted protocol fields */

The function returns success or failure, with ``error`` populated.

Error conditions:

* **Graph is NULL** — for example, when the graph pointer itself is not provided.
* **Pattern is NULL**.
* **Unsupported transition** — when an item type has no matching edge from the current node.
    This is the primary way the graph rejects unsupported protocol sequences.
* **Constraint failure** — when the spec, mask, and last combination
    does not satisfy the node's declared constraints.
* **Validate callback failure** — when driver-specific validation rejects the item.
* **Process callback failure** — when driver-specific extraction path fails.

.. warning::

    Malformed graph tables (for example invalid node indices, missing sentinels,
    or otherwise inconsistent driver-defined graph structures)
    are considered to be a driver implementation bug.
    Graphs are trusted by default:
    driver-owned graph structures are expected to be valid and are not fully validated.

The traversal processes items in order, skipping ignored types.
After the last non-``END`` item,
the parser looks for an ``END`` node reachable from the current position.
It then visits that node and runs its callbacks, if any.
This means drivers can attach a process callback to the ``END`` node
for post-traversal finalization.

Putting It All Together
-----------------------

The complete graph definition with callbacks and constraints:

.. code-block:: c

   const struct flow_graph example_graph = {
       .nodes = (struct flow_graph_node[]){
           [EXAMPLE_NODE_START] = {
               .name = "START",
           },
           [EXAMPLE_NODE_ETH] = {
               .name        = "ETH",
               .type        = RTE_FLOW_ITEM_TYPE_ETH,
               .validate    = example_validate_eth,
               .process     = example_process_eth,
               .constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY
                            | FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_VLAN] = {
               .name        = "VLAN",
               .type        = RTE_FLOW_ITEM_TYPE_VLAN,
               .process     = example_process_vlan,
               .constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_IPV4] = {
               .name        = "IPV4",
               .type        = RTE_FLOW_ITEM_TYPE_IPV4,
               .validate    = example_validate_ipv4,
               .process     = example_process_ipv4,
               .constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_IPV6] = {
               .name        = "IPV6",
               .type        = RTE_FLOW_ITEM_TYPE_IPV6,
               .validate    = example_validate_ipv6,
               .process     = example_process_ipv6,
               .constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_TCP] = {
               .name        = "TCP",
               .type        = RTE_FLOW_ITEM_TYPE_TCP,
               .validate    = example_validate_tcp,
               .process     = example_process_tcp,
               .constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_UDP] = {
               .name        = "UDP",
               .type        = RTE_FLOW_ITEM_TYPE_UDP,
               .validate    = example_validate_udp,
               .process     = example_process_udp,
               .constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_SCTP] = {
               .name        = "SCTP",
               .type        = RTE_FLOW_ITEM_TYPE_SCTP,
               .process     = example_process_sctp,
               .constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY
                            | FLOW_GRAPH_NODE_EXPECT_SPEC,
           },
           [EXAMPLE_NODE_END] = {
               .name = "END",
               .type = RTE_FLOW_ITEM_TYPE_END,
           },
       },
       .edges = (struct flow_graph_edge[]){
           [EXAMPLE_NODE_START] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_ETH,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_ETH] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_VLAN,
                   EXAMPLE_NODE_IPV4,
                   EXAMPLE_NODE_IPV6,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_VLAN] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_IPV4,
                   EXAMPLE_NODE_IPV6,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_IPV4] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_TCP,
                   EXAMPLE_NODE_UDP,
                   EXAMPLE_NODE_SCTP,
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_IPV6] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_TCP,
                   EXAMPLE_NODE_UDP,
                   EXAMPLE_NODE_SCTP,
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_TCP] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_UDP] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [EXAMPLE_NODE_SCTP] = {
               .next = (size_t[]){
                   EXAMPLE_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
       },
   };


Tunnel Graphs and Repeated Item Types
-------------------------------------

Tunneled patterns often repeat the same ``rte_flow_item_type`` in outer and inner headers.
A simple representative example is a TCP-IPv4 over GTP-U pattern::

   ETH -> IPV4 -> UDP -> GTPU -> IPV4 -> TCP

The graph library supports this naturally.
Multiple nodes may use the same ``type`` value,
as long as they are distinct nodes in the graph and reached through different edges.

For tunnel parsing, the recommended style is to model repeated protocol types
as separate inner/outer nodes, for example ``OUTER_IPV4`` and ``INNER_IPV4``.
This makes the graph intent explicit, keeps callback logic clear,
and avoids unexpected graph paths due to loops.

.. code-block:: c

   enum tunnel_node_id {
       TUNNEL_NODE_START = FLOW_GRAPH_NODE_FIRST,
       TUNNEL_NODE_ETH,
       TUNNEL_NODE_OUTER_IPV4,
       TUNNEL_NODE_TCP,
       TUNNEL_NODE_UDP,
       TUNNEL_NODE_GTPU,
       TUNNEL_NODE_INNER_IPV4,
       TUNNEL_NODE_END,
       TUNNEL_NODE_MAX,
   };

   const struct flow_graph tunnel_graph = {
       .nodes = (struct flow_graph_node[]){
           /* Minimal topology example: callbacks and constraints are omitted. */
           [TUNNEL_NODE_START] = { .name = "START" },
           [TUNNEL_NODE_ETH] = {
               .name = "ETH",
               .type = RTE_FLOW_ITEM_TYPE_ETH,
           },
           [TUNNEL_NODE_OUTER_IPV4] = {
               .name = "OUTER_IPV4",
               .type = RTE_FLOW_ITEM_TYPE_IPV4,
           },
           [TUNNEL_NODE_UDP] = {
               .name = "UDP",
               .type = RTE_FLOW_ITEM_TYPE_UDP,
           },
           [TUNNEL_NODE_GTPU] = {
               .name = "GTPU",
               .type = RTE_FLOW_ITEM_TYPE_GTPU,
           },
           [TUNNEL_NODE_INNER_IPV4] = {
               .name = "INNER_IPV4",
               .type = RTE_FLOW_ITEM_TYPE_IPV4,
           },
           [TUNNEL_NODE_TCP] = {
               .name = "TCP",
               .type = RTE_FLOW_ITEM_TYPE_TCP,
           },
           [TUNNEL_NODE_END] = {
               .name = "END",
               .type = RTE_FLOW_ITEM_TYPE_END,
           },
       },
       .edges = (struct flow_graph_edge[]){
           [TUNNEL_NODE_START] = {
               .next = (size_t[]){
                   TUNNEL_NODE_ETH,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [TUNNEL_NODE_ETH] = {
               .next = (size_t[]){
                   TUNNEL_NODE_OUTER_IPV4,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [TUNNEL_NODE_OUTER_IPV4] = {
               .next = (size_t[]){
                   TUNNEL_NODE_UDP,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [TUNNEL_NODE_UDP] = {
               .next = (size_t[]){
                   TUNNEL_NODE_GTPU,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [TUNNEL_NODE_GTPU] = {
               .next = (size_t[]){
                   TUNNEL_NODE_INNER_IPV4,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [TUNNEL_NODE_INNER_IPV4] = {
               .next = (size_t[]){
                   TUNNEL_NODE_TCP,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
           [TUNNEL_NODE_TCP] = {
               .next = (size_t[]){
                   TUNNEL_NODE_END,
                   FLOW_GRAPH_NODE_EDGE_END,
               },
           },
       },
   };

In other words, traversal follows graph edges (node-to-node),
while node matching is done against each candidate node's ``rte_flow_item_type``.
That combination allows repeated protocol layers to be represented cleanly
with separate nodes for different parsing contexts.

Although arbitrary loops are possible in the graph,
tunnel protocol graphs are usually easier to reason about
when repeated item types are split into explicit inner/outer nodes.
It is not recommended to create loops in the graph, as these loops will be unbounded.
