..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Packet Classification and Access Control
========================================

The DPDK provides an Access Control library that gives the ability
to classify an input packet based on a set of classification rules.

The ACL library is used to perform an N-tuple search over a set of rules with multiple categories
and find the best match (highest priority) for each category.
The library API provides the following basic operations:

*   Create a new Access Control (AC) context.

*   Add rules into the context.

*   For all rules in the context, build the runtime structures necessary to perform packet classification.

*   Perform input packet classifications.

*   Destroy an AC context and its runtime structures and free the associated memory.

Overview
--------

Rule definition
~~~~~~~~~~~~~~~

The current implementation allows the user for each AC context to specify its own rule (set of fields)
over which packet classification will be performed.
Though there are few restrictions on the rule fields layout:

*  First field in the rule definition has to be one byte long.
*  All subsequent fields has to be grouped into sets of 4 consecutive bytes.

This is done mainly for performance reasons - search function processes the first input byte as part of the flow setup and then the inner loop of the search function is unrolled to process four input bytes at a time.

To define each field inside an AC rule, the following structure is used:

.. code-block:: c

    struct rte_acl_field_def {
        uint8_t type;         /*< type - ACL_FIELD_TYPE. */
        uint8_t size;         /*< size of field 1,2,4, or 8. */
        uint8_t field_index;  /*< index of field inside the rule. */
        uint8_t input_index;  /*< 0-N input index. */
        uint32_t offset;      /*< offset to start of field. */
    };

*   type
    The field type is one of three choices:

    *   _MASK - for fields such as IP addresses that have a value and a mask defining the number of relevant bits.

    *   _RANGE - for fields such as ports that have a lower and upper value for the field.

    *   _BITMASK - for fields such as protocol identifiers that have a value and a bit mask.

*   size
    The size parameter defines the length of the field in bytes. Allowable values are 1, 2, 4, or 8 bytes.
    Note that due to the grouping of input bytes, 1 or 2 byte fields must be defined as consecutive fields
    that make up 4 consecutive input bytes.
    Also, it is best to define fields of 8 or more bytes as 4 byte fields so that
    the build processes can eliminate fields that are all wild.

*   field_index
    A zero-based value that represents the position of the field inside the rule; 0 to N-1 for N fields.

*   input_index
    As mentioned above, all input fields, except the very first one, must be in groups of 4 consecutive bytes.
    The input index specifies to which input group that field belongs to.

*   offset
    The offset field defines the offset for the field.
    This is the offset from the beginning of the buffer parameter for the search.

For example, to define classification for the following IPv4 5-tuple structure:

.. code-block:: c

    struct ipv4_5tuple {
        uint8_t proto;
        uint32_t ip_src;
        uint32_t ip_dst;
        uint16_t port_src;
        uint16_t port_dst;
    };

The following array of field definitions can be used:

.. code-block:: c

    struct rte_acl_field_def ipv4_defs[5] = {
        /* first input field - always one byte long. */
        {
            .type = RTE_ACL_FIELD_TYPE_BITMASK,
            .size = sizeof (uint8_t),
            .field_index = 0,
            .input_index = 0,
            .offset = offsetof (struct ipv4_5tuple, proto),
        },

        /* next input field (IPv4 source address) - 4 consecutive bytes. */
        {
            .type = RTE_ACL_FIELD_TYPE_MASK,
            .size = sizeof (uint32_t),
            .field_index = 1,
            .input_index = 1,
           .offset = offsetof (struct ipv4_5tuple, ip_src),
        },

        /* next input field (IPv4 destination address) - 4 consecutive bytes. */
        {
            .type = RTE_ACL_FIELD_TYPE_MASK,
            .size = sizeof (uint32_t),
            .field_index = 2,
            .input_index = 2,
           .offset = offsetof (struct ipv4_5tuple, ip_dst),
        },

        /*
         * Next 2 fields (src & dst ports) form 4 consecutive bytes.
         * They share the same input index.
         */
        {
            .type = RTE_ACL_FIELD_TYPE_RANGE,
            .size = sizeof (uint16_t),
            .field_index = 3,
            .input_index = 3,
            .offset = offsetof (struct ipv4_5tuple, port_src),
        },

        {
            .type = RTE_ACL_FIELD_TYPE_RANGE,
            .size = sizeof (uint16_t),
            .field_index = 4,
            .input_index = 3,
            .offset = offsetof (struct ipv4_5tuple, port_dst),
        },
    };

A typical example of such an IPv4 5-tuple rule is a follows:

::

    source addr/mask  destination addr/mask  source ports dest ports protocol/mask
    192.168.1.0/24    192.168.2.31/32        0:65535      1234:1234  17/0xff

Any IPv4 packets with protocol ID 17 (UDP), source address 192.168.1.[0-255], destination address 192.168.2.31,
source port [0-65535] and destination port 1234 matches the above rule.

To define classification for the IPv6 2-tuple: <protocol, IPv6 source address> over the following IPv6 header structure:

.. code-block:: c

    struct struct ipv6_hdr {
        uint32_t vtc_flow;     /* IP version, traffic class & flow label. */
        uint16_t payload_len;  /* IP packet length - includes sizeof(ip_header). */
        uint8_t proto;         /* Protocol, next header. */
        uint8_t hop_limits;    /* Hop limits. */
        uint8_t src_addr[16];  /* IP address of source host. */
        uint8_t dst_addr[16];  /* IP address of destination host(s). */
    } __attribute__((__packed__));

The following array of field definitions can be used:

.. code-block:: c

    struct struct rte_acl_field_def ipv6_2tuple_defs[5] = {
        {
            .type = RTE_ACL_FIELD_TYPE_BITMASK,
            .size = sizeof (uint8_t),
            .field_index = 0,
            .input_index = 0,
            .offset = offsetof (struct ipv6_hdr, proto),
        },

        {
            .type = RTE_ACL_FIELD_TYPE_MASK,
            .size = sizeof (uint32_t),
            .field_index = 1,
            .input_index = 1,
            .offset = offsetof (struct ipv6_hdr, src_addr[0]),
        },

        {
            .type = RTE_ACL_FIELD_TYPE_MASK,
            .size = sizeof (uint32_t),
            .field_index = 2,
            .input_index = 2,
            .offset = offsetof (struct ipv6_hdr, src_addr[4]),
        },

        {
            .type = RTE_ACL_FIELD_TYPE_MASK,
            .size = sizeof (uint32_t),
            .field_index = 3,
            .input_index = 3,
           .offset = offsetof (struct ipv6_hdr, src_addr[8]),
        },

        {
           .type = RTE_ACL_FIELD_TYPE_MASK,
           .size = sizeof (uint32_t),
           .field_index = 4,
           .input_index = 4,
           .offset = offsetof (struct ipv6_hdr, src_addr[12]),
        },
    };

A typical example of such an IPv6 2-tuple rule is a follows:

::

    source addr/mask                              protocol/mask
    2001:db8:1234:0000:0000:0000:0000:0000/48     6/0xff

Any IPv6 packets with protocol ID 6 (TCP), and source address inside the range
[2001:db8:1234:0000:0000:0000:0000:0000 - 2001:db8:1234:ffff:ffff:ffff:ffff:ffff] matches the above rule.

When creating a set of rules, for each rule, additional information must be supplied also:

*   **priority**: A weight to measure the priority of the rules (higher is better).
    If the input tuple matches more than one rule, then the rule with the higher priority is returned.
    Note that if the input tuple matches more than one rule and these rules have equal priority,
    it is undefined which rule is returned as a match.
    It is recommended to assign a unique priority for each rule.

*   **category_mask**: Each rule uses a bit mask value to select the relevant category(s) for the rule.
    When a lookup is performed, the result for each category is returned.
    This effectively provides a "parallel lookup" by enabling a single search to return multiple results if,
    for example, there were four different sets of ACL rules, one for access control, one for routing, and so on.
    Each set could be assigned its own category and by combining them into a single database,
    one lookup returns a result for each of the four sets.

*   **userdata**: A user-defined field that could be any value except zero.
    For each category, a successful match returns the userdata field of the highest priority matched rule.

.. note::

    When adding new rules into an ACL context, all fields must be in host byte order (LSB).
    When the search is performed for an input tuple, all fields in that tuple must be in network byte order (MSB).

RT memory size limit
~~~~~~~~~~~~~~~~~~~~

Build phase (rte_acl_build()) creates for a given set of rules internal structure for further run-time traversal.
With current implementation it is a set of multi-bit tries (with stride == 8).
Depending on the rules set, that could consume significant amount of memory.
In attempt to conserve some space ACL build process tries to split the given
rule-set into several non-intersecting subsets and construct a separate trie
for each of them.
Depending on the rule-set, it might reduce RT memory requirements but might
increase classification time.
There is a possibility at build-time to specify maximum memory limit for internal RT structures for given AC context.
It could be done via **max_size** field of the **rte_acl_config** strucure.
Setting it to the value greater than zero, instructs rte_acl_build() to:

*   attempt to minimise number of tries in the RT table, but
*   make sure that size of RT table wouldn't exceed given value.

Setting it to zero makes rte_acl_build() to use the default behaviour:
try to minimise size of the RT structures, but doesn't expose any hard limit on it.

That gives the user the ability to decisions about performance/space trade-off.
For example:

.. code-block:: c

    struct rte_acl_ctx * acx;
    struct rte_acl_config cfg;
    int ret;

    /*
     * assuming that acx points to already created and
     * populated with rules AC context and cfg filled properly.
     */

     /* try to build AC context, with RT strcutures less then 8MB. */
     cfg.max_size = 0x800000;
     ret = rte_acl_build(acx, &cfg);

     /*
      * RT strcutures can't fit into 8MB for given context.
      * Try to build without exposing any hard limit.
      */
     if (ret == -ERANGE) {
        cfg.max_size = 0;
        ret = rte_acl_build(acx, &cfg);
     }



Classification methods
~~~~~~~~~~~~~~~~~~~~~~

After rte_acl_build() over given AC context has finished successfully, it can be used to perform classification - search for a rule with highest priority over the input data.
There are several implementations of classify algorithm:

*   **RTE_ACL_CLASSIFY_SCALAR**: generic implementation, doesn't require any specific HW support.

*   **RTE_ACL_CLASSIFY_SSE**: vector implementation, can process up to 8 flows in parallel. Requires SSE 4.1 support.

*   **RTE_ACL_CLASSIFY_AVX2**: vector implementation, can process up to 16 flows in parallel. Requires AVX2 support.

It is purely a runtime decision which method to choose, there is no build-time difference.
All implementations operates over the same internal RT structures and use similar principles. The main difference is that vector implementations can manually exploit IA SIMD instructions and process several input data flows in parallel.
At startup ACL library determines the highest available classify method for the given platform and sets it as default one. Though the user has an ability to override the default classifier function for a given ACL context or perform particular search using non-default classify method. In that case it is user responsibility to make sure that given platform supports selected classify implementation.

Application Programming Interface (API) Usage
---------------------------------------------

.. note::

    For more details about the Access Control API, please refer to the *DPDK API Reference*.

The following example demonstrates IPv4, 5-tuple classification for rules defined above
with multiple categories in more detail.

Classify with Multiple Categories
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    struct rte_acl_ctx * acx;
    struct rte_acl_config cfg;
    int ret;

    /* define a structure for the rule with up to 5 fields. */

    RTE_ACL_RULE_DEF(acl_ipv4_rule, RTE_DIM(ipv4_defs));

    /* AC context creation parameters. */

    struct rte_acl_param prm = {
        .name = "ACL_example",
        .socket_id = SOCKET_ID_ANY,
        .rule_size = RTE_ACL_RULE_SZ(RTE_DIM(ipv4_defs)),

        /* number of fields per rule. */

        .max_rule_num = 8, /* maximum number of rules in the AC context. */
    };

    struct acl_ipv4_rule acl_rules[] = {

        /* matches all packets traveling to 192.168.0.0/16, applies for categories: 0,1 */
        {
            .data = {.userdata = 1, .category_mask = 3, .priority = 1},

            /* destination IPv4 */
            .field[2] = {.value.u32 = IPv4(192,168,0,0),. mask_range.u32 = 16,},

            /* source port */
            .field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},

            /* destination port */
           .field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
        },

        /* matches all packets traveling to 192.168.1.0/24, applies for categories: 0 */
        {
            .data = {.userdata = 2, .category_mask = 1, .priority = 2},

            /* destination IPv4 */
            .field[2] = {.value.u32 = IPv4(192,168,1,0),. mask_range.u32 = 24,},

            /* source port */
            .field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},

            /* destination port */
            .field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
        },

        /* matches all packets traveling from 10.1.1.1, applies for categories: 1 */
        {
            .data = {.userdata = 3, .category_mask = 2, .priority = 3},

            /* source IPv4 */
            .field[1] = {.value.u32 = IPv4(10,1,1,1),. mask_range.u32 = 32,},

            /* source port */
            .field[3] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},

            /* destination port */
            .field[4] = {.value.u16 = 0, .mask_range.u16 = 0xffff,},
        },

    };


    /* create an empty AC context  */

    if ((acx = rte_acl_create(&prm)) == NULL) {

        /* handle context create failure. */

    }

    /* add rules to the context */

    ret = rte_acl_add_rules(acx, acl_rules, RTE_DIM(acl_rules));
    if (ret != 0) {
       /* handle error at adding ACL rules. */
    }

    /* prepare AC build config. */

    cfg.num_categories = 2;
    cfg.num_fields = RTE_DIM(ipv4_defs);

    memcpy(cfg.defs, ipv4_defs, sizeof (ipv4_defs));

    /* build the runtime structures for added rules, with 2 categories. */

    ret = rte_acl_build(acx, &cfg);
    if (ret != 0) {
       /* handle error at build runtime structures for ACL context. */
    }

For a tuple with source IP address: 10.1.1.1 and destination IP address: 192.168.1.15,
once the following lines are executed:

.. code-block:: c

    uint32_t results[4]; /* make classify for 4 categories. */

    rte_acl_classify(acx, data, results, 1, 4);

then the results[] array contains:

.. code-block:: c

    results[4] = {2, 3, 0, 0};

*   For category 0, both rules 1 and 2 match, but rule 2 has higher priority,
    therefore results[0] contains the userdata for rule 2.

*   For category 1, both rules 1 and 3 match, but rule 3 has higher priority,
    therefore results[1] contains the userdata for rule 3.

*   For categories 2 and 3, there are no matches, so results[2] and results[3] contain zero,
    which indicates that no matches were found for those categories.

For a tuple with source IP address: 192.168.1.1 and destination IP address: 192.168.2.11,
once the following lines are executed:

.. code-block:: c

    uint32_t results[4]; /* make classify by 4 categories. */

    rte_acl_classify(acx, data, results, 1, 4);

the results[] array contains:

.. code-block:: c

    results[4] = {1, 1, 0, 0};

*   For categories 0 and 1, only rule 1 matches.

*   For categories 2 and 3, there are no matches.

For a tuple with source IP address: 10.1.1.1 and destination IP address: 201.212.111.12,
once the following lines are executed:

.. code-block:: c

    uint32_t results[4]; /* make classify by 4 categories. */
    rte_acl_classify(acx, data, results, 1, 4);

the results[] array contains:

.. code-block:: c

    results[4] = {0, 3, 0, 0};

*   For category 1, only rule 3 matches.

*   For categories 0, 2 and 3, there are no matches.
