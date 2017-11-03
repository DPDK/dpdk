..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
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

Flow Classification Library
===========================

DPDK provides a Flow Classification library that provides the ability
to classify an input packet by matching it against a set of Flow rules.

The initial implementation supports counting of IPv4 5-tuple packets which match
a particular Flow rule only.

Please refer to the
:doc:`./rte_flow`
for more information.

The Flow Classification library uses the ``librte_table`` API for managing Flow
rules and matching packets against the Flow rules.
The library is table agnostic and can use the following tables:
``Access Control List``, ``Hash`` and ``Longest Prefix Match(LPM)``.
The ``Access Control List`` table is used in the initial implementation.

Please refer to the
:doc:`./packet_framework`
for more information.on ``librte_table``.

DPDK provides an Access Control List library that provides the ability to
classify an input packet based on a set of classification rules.

Please refer to the
:doc:`./packet_classif_access_ctrl`
library for more information on ``librte_acl``.

There is also a Flow Classify sample application which demonstrates the use of
the Flow Classification Library API's.

Please refer to the
:doc:`../sample_app_ug/flow_classify`
for more information on the ``flow_classify`` sample application.

Overview
--------

The library has the following API's

.. code-block:: c

    /**
     * Flow classifier create
     *
     * @param params
     *   Parameters for flow classifier creation
     * @return
     *   Handle to flow classifier instance on success or NULL otherwise
     */
    struct rte_flow_classifier *
    rte_flow_classifier_create(struct rte_flow_classifier_params *params);

    /**
     * Flow classifier free
     *
     * @param cls
     *   Handle to flow classifier instance
     * @return
     *   0 on success, error code otherwise
     */
    int
    rte_flow_classifier_free(struct rte_flow_classifier *cls);

    /**
     * Flow classify table create
     *
     * @param cls
     *   Handle to flow classifier instance
     * @param params
     *   Parameters for flow_classify table creation
     * @param table_id
     *   Table ID. Valid only within the scope of table IDs of the current
     *   classifier. Only returned after a successful invocation.
     * @return
     *   0 on success, error code otherwise
     */
    int
    rte_flow_classify_table_create(struct rte_flow_classifier *cls,
           struct rte_flow_classify_table_params *params,
           uint32_t *table_id);

    /**
     * Add a flow classify rule to the flow_classifier table.
     *
     * @param[in] cls
     *   Flow classifier handle
     * @param[in] table_id
     *   id of table
     * @param[in] attr
     *   Flow rule attributes
     * @param[in] pattern
     *   Pattern specification (list terminated by the END pattern item).
     * @param[in] actions
     *   Associated actions (list terminated by the END pattern item).
     * @param[out] error
     *   Perform verbose error reporting if not NULL. Structure
     *   initialised in case of error only.
     * @return
     *   A valid handle in case of success, NULL otherwise.
     */
    struct rte_flow_classify_rule *
    rte_flow_classify_table_entry_add(struct rte_flow_classifier *cls,
            uint32_t table_id,
            const struct rte_flow_attr *attr,
            const struct rte_flow_item pattern[],
            const struct rte_flow_action actions[],
            struct rte_flow_error *error);

    /**
     * Delete a flow classify rule from the flow_classifier table.
     *
     * @param[in] cls
     *   Flow classifier handle
     * @param[in] table_id
     *   id of table
     * @param[in] rule
     *   Flow classify rule
     * @return
     *   0 on success, error code otherwise.
     */
    int
    rte_flow_classify_table_entry_delete(struct rte_flow_classifier *cls,
            uint32_t table_id,
            struct rte_flow_classify_rule *rule);

    /**
     * Query flow classifier for given rule.
     *
     * @param[in] cls
     *   Flow classifier handle
     * @param[in] table_id
     *   id of table
     * @param[in] pkts
     *   Pointer to packets to process
     * @param[in] nb_pkts
     *   Number of packets to process
     * @param[in] rule
     *   Flow classify rule
     * @param[in] stats
     *   Flow classify stats
     *
     * @return
     *   0 on success, error code otherwise.
     */
    int
    rte_flow_classifier_query(struct rte_flow_classifier *cls,
            uint32_t table_id,
            struct rte_mbuf **pkts,
            const uint16_t nb_pkts,
            struct rte_flow_classify_rule *rule,
            struct rte_flow_classify_stats *stats);

Classifier creation
~~~~~~~~~~~~~~~~~~~

The application creates the ``Classifier`` using the
``rte_flow_classifier_create`` API.
The ``rte_flow_classify_params`` structure must be initialised by the
application before calling the API.

.. code-block:: c

    struct rte_flow_classifier_params {
        /** flow classifier name */
        const char *name;

        /** CPU socket ID where memory for the flow classifier and its */
        /** elements (tables) should be allocated */
        int socket_id;

        /** Table type */
        enum rte_flow_classify_table_type type;
    };

The ``Classifier`` has the following internal structures:

.. code-block:: c

    struct rte_table {
        /* Input parameters */
        struct rte_table_ops ops;
        uint32_t entry_size;
        enum rte_flow_classify_table_type type;

        /* Handle to the low-level table object */
        void *h_table;
    };

    #define RTE_FLOW_CLASSIFIER_MAX_NAME_SZ 256

    struct rte_flow_classifier {
        /* Input parameters */
        char name[RTE_FLOW_CLASSIFIER_MAX_NAME_SZ];
        int socket_id;
        enum rte_flow_classify_table_type type;

        /* Internal tables */
        struct rte_table tables[RTE_FLOW_CLASSIFY_TABLE_MAX];
        uint32_t num_tables;
        uint16_t nb_pkts;
        struct rte_flow_classify_table_entry
            *entries[RTE_PORT_IN_BURST_SIZE_MAX];
    } __rte_cache_aligned;

Adding a table to the Classifier
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The application adds a table to the ``Classifier`` using the
``rte_flow_classify_table_create`` API.
The ``rte_flow_classify_table_params`` structure must be initialised by the
application before calling the API.

.. code-block:: c

    struct rte_flow_classify_table_params {
        /** Table operations (specific to each table type) */
        struct rte_table_ops *ops;

        /** Opaque param to be passed to the table create operation */
        void *arg_create;

        /** Memory size to be reserved per classifier object entry for */
        /** storing meta data */
        uint32_t table_metadata_size;
     };

To create an ACL table the ``rte_table_acl_params`` structure must be
initialised and assigned to ``arg_create`` in the
``rte_flow_classify_table_params`` structure.

.. code-block:: c

    struct rte_table_acl_params {
        /** Name */
        const char *name;

        /** Maximum number of ACL rules in the table */
        uint32_t n_rules;

        /** Number of fields in the ACL rule specification */
        uint32_t n_rule_fields;

        /** Format specification of the fields of the ACL rule */
        struct rte_acl_field_def field_format[RTE_ACL_MAX_FIELDS];
    };

The fields for the ACL rule must also be initialised by the application.

An ACL table can be added to the ``Classifier`` for each ACL rule, for example
another table could be added for the IPv6 5-tuple rule.

Flow Parsing
~~~~~~~~~~~~

The library currently supports three IPv4 5-tuple flow patterns, for UDP, TCP
and SCTP.

.. code-block:: c

    /* Pattern for IPv4 5-tuple UDP filter */
    static enum rte_flow_item_type pattern_ntuple_1[] = {
        RTE_FLOW_ITEM_TYPE_ETH,
        RTE_FLOW_ITEM_TYPE_IPV4,
        RTE_FLOW_ITEM_TYPE_UDP,
        RTE_FLOW_ITEM_TYPE_END,
    };

    /* Pattern for IPv4 5-tuple TCP filter */
    static enum rte_flow_item_type pattern_ntuple_2[] = {
        RTE_FLOW_ITEM_TYPE_ETH,
        RTE_FLOW_ITEM_TYPE_IPV4,
        RTE_FLOW_ITEM_TYPE_TCP,
        RTE_FLOW_ITEM_TYPE_END,
    };

    /* Pattern for IPv4 5-tuple SCTP filter */
    static enum rte_flow_item_type pattern_ntuple_3[] = {
        RTE_FLOW_ITEM_TYPE_ETH,
        RTE_FLOW_ITEM_TYPE_IPV4,
        RTE_FLOW_ITEM_TYPE_SCTP,
        RTE_FLOW_ITEM_TYPE_END,
    };

The internal function ``flow_classify_parse_flow`` parses the
IPv4 5-tuple pattern, attributes and actions and returns the 5-tuple data in the
``rte_eth_ntuple_filter`` structure.

.. code-block:: c

    static int
    flow_classify_parse_flow(
                   const struct rte_flow_attr *attr,
                   const struct rte_flow_item pattern[],
                   const struct rte_flow_action actions[],
                   struct rte_flow_error *error)

Adding Flow Rules
~~~~~~~~~~~~~~~~~

The ``rte_flow_classify_table_entry_add`` API creates an
``rte_flow_classify`` object which contains the flow_classify id and type, the
action, a union of add and delete keys and a union of rules.
It uses the ``flow_classify_parse_flow`` internal function for parsing the
flow parameters.
The 5-tuple ACL key data is obtained from the ``rte_eth_ntuple_filter``
structure populated by the ``classify_parse_ntuple_filter`` function which
parses the Flow rule.

.. code-block:: c

    struct acl_keys {
        struct rte_table_acl_rule_add_params key_add; /* add key */
        struct rte_table_acl_rule_delete_params	key_del; /* delete key */
    };

    struct classify_rules {
        enum rte_flow_classify_rule_type type;
        union {
            struct rte_flow_classify_ipv4_5tuple ipv4_5tuple;
        } u;
    };

    struct rte_flow_classify {
        uint32_t id;  /* unique ID of classify object */
        struct rte_flow_action action; /* action when match found */
	struct classify_rules rules; /* union of rules */
        union {
            struct acl_keys key;
        } u;
        int key_found; /* rule key found in table */
        void *entry; /* pointer to buffer to hold rule meta data */
        void *entry_ptr; /* handle to the table entry for rule meta data */
    };

It then calls the ``table[table_id].ops.f_add`` API to add the rule to the ACL
table.

Deleting Flow Rules
~~~~~~~~~~~~~~~~~~~

The ``rte_flow_classify_table_entry_delete`` API calls the
``table[table_id].ops.f_delete`` API to delete a rule from the ACL table.

Packet Matching
~~~~~~~~~~~~~~~

The ``rte_flow_classifier_query`` API is used to find packets which match a
given flow Flow rule in the table.
This API calls the flow_classify_run internal function which calls the
``table[table_id].ops.f_lookup`` API to see if any packets in a burst match any
of the Flow rules in the table.
The meta data for the highest priority rule matched for each packet is returned
in the entries array in the ``rte_flow_classify`` object.
The internal function ``action_apply`` implements the ``Count`` action which is
used to return data which matches a particular Flow rule.

The rte_flow_classifier_query API uses the following structures to return data
to the application.

.. code-block:: c

    /** IPv4 5-tuple data */
    struct rte_flow_classify_ipv4_5tuple {
        uint32_t dst_ip;         /**< Destination IP address in big endian. */
        uint32_t dst_ip_mask;    /**< Mask of destination IP address. */
        uint32_t src_ip;         /**< Source IP address in big endian. */
        uint32_t src_ip_mask;    /**< Mask of destination IP address. */
        uint16_t dst_port;       /**< Destination port in big endian. */
        uint16_t dst_port_mask;  /**< Mask of destination port. */
        uint16_t src_port;       /**< Source Port in big endian. */
        uint16_t src_port_mask;  /**< Mask of source port. */
        uint8_t proto;           /**< L4 protocol. */
        uint8_t proto_mask;      /**< Mask of L4 protocol. */
    };

    /**
     * Flow stats
     *
     * For the count action, stats can be returned by the query API.
     *
     * Storage for stats is provided by the application.
     *
     *
     */
    struct rte_flow_classify_stats {
        void *stats;
    };

    struct rte_flow_classify_5tuple_stats {
        /** count of packets that match IPv4 5tuple pattern */
        uint64_t counter1;
        /** IPv4 5tuple data */
        struct rte_flow_classify_ipv4_5tuple ipv4_5tuple;
    };
