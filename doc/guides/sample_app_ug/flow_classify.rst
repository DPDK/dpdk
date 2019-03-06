..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Flow Classify Sample Application
================================

The Flow Classify sample application is based on the simple *skeleton* example
of a forwarding application.

It is intended as a demonstration of the basic components of a DPDK forwarding
application which uses the Flow Classify library API's.

Please refer to the
:doc:`../prog_guide/flow_classify_lib`
for more information.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``flow_classify`` sub-directory.

Running the Application
-----------------------

To run the example in a ``linux`` environment:

.. code-block:: console

    cd ~/dpdk/examples/flow_classify
    ./build/flow_classify -c 4 -n 4 -- --rule_ipv4="../ipv4_rules_file.txt"

Please refer to the *DPDK Getting Started Guide*, section
:doc:`../linux_gsg/build_sample_apps`
for general information on running applications and the Environment Abstraction
Layer (EAL) options.


Sample ipv4_rules_file.txt
--------------------------

.. code-block:: console

    #file format:
    #src_ip/masklen dst_ip/masklen src_port : mask dst_port : mask proto/mask priority
    #
    2.2.2.3/24 2.2.2.7/24 32 : 0xffff 33 : 0xffff 17/0xff 0
    9.9.9.3/24 9.9.9.7/24 32 : 0xffff 33 : 0xffff 17/0xff 1
    9.9.9.3/24 9.9.9.7/24 32 : 0xffff 33 : 0xffff 6/0xff 2
    9.9.8.3/24 9.9.8.7/24 32 : 0xffff 33 : 0xffff 6/0xff 3
    6.7.8.9/24 2.3.4.5/24 32 : 0x0000 33 : 0x0000 132/0xff 4

Explanation
-----------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.

ACL field definitions for the IPv4 5 tuple rule
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following field definitions are used when creating the ACL table during
initialisation of the ``Flow Classify`` application..

.. code-block:: c

     enum {
         PROTO_FIELD_IPV4,
         SRC_FIELD_IPV4,
         DST_FIELD_IPV4,
         SRCP_FIELD_IPV4,
         DSTP_FIELD_IPV4,
         NUM_FIELDS_IPV4
    };

    enum {
        PROTO_INPUT_IPV4,
        SRC_INPUT_IPV4,
        DST_INPUT_IPV4,
        SRCP_DESTP_INPUT_IPV4
    };

    static struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {
        /* first input field - always one byte long. */
        {
            .type = RTE_ACL_FIELD_TYPE_BITMASK,
            .size = sizeof(uint8_t),
            .field_index = PROTO_FIELD_IPV4,
            .input_index = PROTO_INPUT_IPV4,
            .offset = sizeof(struct ether_hdr) +
                offsetof(struct ipv4_hdr, next_proto_id),
        },
        /* next input field (IPv4 source address) - 4 consecutive bytes. */
        {
            /* rte_flow uses a bit mask for IPv4 addresses */
            .type = RTE_ACL_FIELD_TYPE_BITMASK,
            .size = sizeof(uint32_t),
            .field_index = SRC_FIELD_IPV4,
            .input_index = SRC_INPUT_IPV4,
            .offset = sizeof(struct ether_hdr) +
                offsetof(struct ipv4_hdr, src_addr),
        },
        /* next input field (IPv4 destination address) - 4 consecutive bytes. */
        {
            /* rte_flow uses a bit mask for IPv4 addresses */
            .type = RTE_ACL_FIELD_TYPE_BITMASK,
            .size = sizeof(uint32_t),
            .field_index = DST_FIELD_IPV4,
            .input_index = DST_INPUT_IPV4,
            .offset = sizeof(struct ether_hdr) +
                offsetof(struct ipv4_hdr, dst_addr),
        },
        /*
         * Next 2 fields (src & dst ports) form 4 consecutive bytes.
         * They share the same input index.
         */
	{
            /* rte_flow uses a bit mask for protocol ports */
            .type = RTE_ACL_FIELD_TYPE_BITMASK,
            .size = sizeof(uint16_t),
            .field_index = SRCP_FIELD_IPV4,
            .input_index = SRCP_DESTP_INPUT_IPV4,
            .offset = sizeof(struct ether_hdr) +
                sizeof(struct ipv4_hdr) +
                offsetof(struct tcp_hdr, src_port),
        },
        {
             /* rte_flow uses a bit mask for protocol ports */
             .type = RTE_ACL_FIELD_TYPE_BITMASK,
             .size = sizeof(uint16_t),
             .field_index = DSTP_FIELD_IPV4,
             .input_index = SRCP_DESTP_INPUT_IPV4,
             .offset = sizeof(struct ether_hdr) +
                 sizeof(struct ipv4_hdr) +
                 offsetof(struct tcp_hdr, dst_port),
        },
    };

The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).
The ``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

It then parses the flow_classify application arguments

.. code-block:: c

    ret = parse_args(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid flow_classify parameters\n");

The ``main()`` function also allocates a mempool to hold the mbufs
(Message Buffers) used by the application:

.. code-block:: c

    mbuf_pool = rte_mempool_create("MBUF_POOL",
                                   NUM_MBUFS * nb_ports,
                                   MBUF_SIZE,
                                   MBUF_CACHE_SIZE,
                                   sizeof(struct rte_pktmbuf_pool_private),
                                   rte_pktmbuf_pool_init, NULL,
                                   rte_pktmbuf_init, NULL,
                                   rte_socket_id(),
                                   0);

mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``port_init()`` function which is explained in the next section:

.. code-block:: c

    RTE_ETH_FOREACH_DEV(portid) {
        if (port_init(portid, mbuf_pool) != 0) {
            rte_exit(EXIT_FAILURE,
                     "Cannot init port %" PRIu8 "\n", portid);
        }
    }

The ``main()`` function creates the ``flow classifier object`` and adds an ``ACL
table`` to the flow classifier.

.. code-block:: c

    struct flow_classifier {
        struct rte_flow_classifier *cls;
    };

    struct flow_classifier_acl {
        struct flow_classifier cls;
    } __rte_cache_aligned;

    /* Memory allocation */
    size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct flow_classifier_acl));
    cls_app = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
    if (cls_app == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate classifier memory\n");

    cls_params.name = "flow_classifier";
    cls_params.socket_id = socket_id;

    cls_app->cls = rte_flow_classifier_create(&cls_params);
    if (cls_app->cls == NULL) {
        rte_free(cls_app);
        rte_exit(EXIT_FAILURE, "Cannot create classifier\n");
    }

    /* initialise ACL table params */
    table_acl_params.name = "table_acl_ipv4_5tuple";
    table_acl_params.n_rule_fields = RTE_DIM(ipv4_defs);
    table_acl_params.n_rules = FLOW_CLASSIFY_MAX_RULE_NUM;
    memcpy(table_acl_params.field_format, ipv4_defs, sizeof(ipv4_defs));

    /* initialise table create params */
    cls_table_params.ops = &rte_table_acl_ops,
    cls_table_params.arg_create = &table_acl_params,
    cls_table_params.type = RTE_FLOW_CLASSIFY_TABLE_ACL_IP4_5TUPLE;

    ret = rte_flow_classify_table_create(cls_app->cls, &cls_table_params);
    if (ret) {
        rte_flow_classifier_free(cls_app->cls);
        rte_free(cls);
        rte_exit(EXIT_FAILURE, "Failed to create classifier table\n");
    }

It then reads the ipv4_rules_file.txt file and initialises the parameters for
the ``rte_flow_classify_table_entry_add`` API.
This API adds a rule to the ACL table.

.. code-block:: c

    if (add_rules(parm_config.rule_ipv4_name)) {
        rte_flow_classifier_free(cls_app->cls);
        rte_free(cls_app);
        rte_exit(EXIT_FAILURE, "Failed to add rules\n");
    }

Once the initialization is complete, the application is ready to launch a
function on an lcore. In this example ``lcore_main()`` is called on a single
lcore.

.. code-block:: c

    lcore_main(cls_app);

The ``lcore_main()`` function is explained below.

The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization used in the Basic
Forwarding application is shown below:

.. code-block:: c

    static inline int
    port_init(uint8_t port, struct rte_mempool *mbuf_pool)
    {
        struct rte_eth_conf port_conf = port_conf_default;
        const uint16_t rx_rings = 1, tx_rings = 1;
        struct ether_addr addr;
        int retval;
        uint16_t q;

        /* Configure the Ethernet device. */
        retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
        if (retval != 0)
            return retval;

        /* Allocate and set up 1 RX queue per Ethernet port. */
        for (q = 0; q < rx_rings; q++) {
            retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
                    rte_eth_dev_socket_id(port), NULL, mbuf_pool);
            if (retval < 0)
                return retval;
        }

        /* Allocate and set up 1 TX queue per Ethernet port. */
        for (q = 0; q < tx_rings; q++) {
            retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
                    rte_eth_dev_socket_id(port), NULL);
            if (retval < 0)
                return retval;
        }

        /* Start the Ethernet port. */
        retval = rte_eth_dev_start(port);
        if (retval < 0)
            return retval;

        /* Display the port MAC address. */
        rte_eth_macaddr_get(port, &addr);
        printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
               " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
               port,
               addr.addr_bytes[0], addr.addr_bytes[1],
               addr.addr_bytes[2], addr.addr_bytes[3],
               addr.addr_bytes[4], addr.addr_bytes[5]);

        /* Enable RX in promiscuous mode for the Ethernet device. */
        rte_eth_promiscuous_enable(port);

        return 0;
    }

The Ethernet ports are configured with default settings using the
``rte_eth_dev_configure()`` function and the ``port_conf_default`` struct.

.. code-block:: c

    static const struct rte_eth_conf port_conf_default = {
        .rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
    };

For this example the ports are set up with 1 RX and 1 TX queue using the
``rte_eth_rx_queue_setup()`` and ``rte_eth_tx_queue_setup()`` functions.

The Ethernet port is then started:

.. code-block:: c

    retval  = rte_eth_dev_start(port);


Finally the RX port is set in promiscuous mode:

.. code-block:: c

    rte_eth_promiscuous_enable(port);

The Add Rules function
~~~~~~~~~~~~~~~~~~~~~~

The ``add_rules`` function reads the ``ipv4_rules_file.txt`` file and calls the
``add_classify_rule`` function which calls the
``rte_flow_classify_table_entry_add`` API.

.. code-block:: c

    static int
    add_rules(const char *rule_path)
    {
        FILE *fh;
        char buff[LINE_MAX];
        unsigned int i = 0;
        unsigned int total_num = 0;
        struct rte_eth_ntuple_filter ntuple_filter;

        fh = fopen(rule_path, "rb");
        if (fh == NULL)
            rte_exit(EXIT_FAILURE, "%s: Open %s failed\n", __func__,
                     rule_path);

        fseek(fh, 0, SEEK_SET);

        i = 0;
        while (fgets(buff, LINE_MAX, fh) != NULL) {
            i++;

            if (is_bypass_line(buff))
                continue;

            if (total_num >= FLOW_CLASSIFY_MAX_RULE_NUM - 1) {
                printf("\nINFO: classify rule capacity %d reached\n",
                       total_num);
                break;
            }

            if (parse_ipv4_5tuple_rule(buff, &ntuple_filter) != 0)
                rte_exit(EXIT_FAILURE,
                         "%s Line %u: parse rules error\n",
                         rule_path, i);

            if (add_classify_rule(&ntuple_filter) != 0)
                rte_exit(EXIT_FAILURE, "add rule error\n");

            total_num++;
	}

	fclose(fh);
	return 0;
    }


The Lcore Main function
~~~~~~~~~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function on the
available lcores.
The ``lcore_main`` function calls the ``rte_flow_classifier_query`` API.
For the Basic Forwarding application the ``lcore_main`` function looks like the
following:

.. code-block:: c

    /* flow classify data */
    static int num_classify_rules;
    static struct rte_flow_classify_rule *rules[MAX_NUM_CLASSIFY];
    static struct rte_flow_classify_ipv4_5tuple_stats ntuple_stats;
    static struct rte_flow_classify_stats classify_stats = {
            .stats = (void *)&ntuple_stats
    };

    static __attribute__((noreturn)) void
    lcore_main(cls_app)
    {
        uint16_t port;

        /*
         * Check that the port is on the same NUMA node as the polling thread
         * for best performance.
         */
        RTE_ETH_FOREACH_DEV(port)
            if (rte_eth_dev_socket_id(port) > 0 &&
                rte_eth_dev_socket_id(port) != (int)rte_socket_id()) {
                printf("\n\n");
                printf("WARNING: port %u is on remote NUMA node\n",
                       port);
                printf("to polling thread.\n");
                printf("Performance will not be optimal.\n");

                printf("\nCore %u forwarding packets. \n",
                       rte_lcore_id());
                printf("[Ctrl+C to quit]\n
            }

        /* Run until the application is quit or killed. */
        for (;;) {
            /*
             * Receive packets on a port and forward them on the paired
             * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
             */
            RTE_ETH_FOREACH_DEV(port) {

                /* Get burst of RX packets, from first port of pair. */
                struct rte_mbuf *bufs[BURST_SIZE];
                const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                        bufs, BURST_SIZE);

                if (unlikely(nb_rx == 0))
                    continue;

                for (i = 0; i < MAX_NUM_CLASSIFY; i++) {
                    if (rules[i]) {
                        ret = rte_flow_classifier_query(
                            cls_app->cls,
                            bufs, nb_rx, rules[i],
                            &classify_stats);
                        if (ret)
                            printf(
                                "rule [%d] query failed ret [%d]\n\n",
                                i, ret);
                        else {
                            printf(
                                "rule[%d] count=%"PRIu64"\n",
                                i, ntuple_stats.counter1);

                            printf("proto = %d\n",
                                ntuple_stats.ipv4_5tuple.proto);
                        }
                     }
                 }

                /* Send burst of TX packets, to second port of pair. */
                const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
                        bufs, nb_rx);

                /* Free any unsent packets. */
                if (unlikely(nb_tx < nb_rx)) {
                    uint16_t buf;
                    for (buf = nb_tx; buf < nb_rx; buf++)
                        rte_pktmbuf_free(bufs[buf]);
                }
            }
        }
    }

The main work of the application is done within the loop:

.. code-block:: c

        for (;;) {
            RTE_ETH_FOREACH_DEV(port) {

                /* Get burst of RX packets, from first port of pair. */
                struct rte_mbuf *bufs[BURST_SIZE];
                const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                        bufs, BURST_SIZE);

                if (unlikely(nb_rx == 0))
                    continue;

                /* Send burst of TX packets, to second port of pair. */
                const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
                        bufs, nb_rx);

                /* Free any unsent packets. */
                if (unlikely(nb_tx < nb_rx)) {
                    uint16_t buf;
                    for (buf = nb_tx; buf < nb_rx; buf++)
                        rte_pktmbuf_free(bufs[buf]);
                }
            }
        }

Packets are received in bursts on the RX ports and transmitted in bursts on
the TX ports. The ports are grouped in pairs with a simple mapping scheme
using the an XOR on the port number::

    0 -> 1
    1 -> 0

    2 -> 3
    3 -> 2

    etc.

The ``rte_eth_tx_burst()`` function frees the memory buffers of packets that
are transmitted. If packets fail to transmit, ``(nb_tx < nb_rx)``, then they
must be freed explicitly using ``rte_pktmbuf_free()``.

The forwarding loop can be interrupted and the application closed using
``Ctrl-C``.
