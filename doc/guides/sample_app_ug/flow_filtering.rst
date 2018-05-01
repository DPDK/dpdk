..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 Mellanox Technologies, Ltd

Basic RTE Flow Filtering Sample Application
===========================================

The Basic RTE flow filtering sample application is a simple example of a
creating a RTE flow rule.

It is intended as a demonstration of the basic components RTE flow rules.


Compiling the Application
-------------------------

To compile the application export the path to the DPDK source tree and go to
the example directory:

.. code-block:: console

    export RTE_SDK=/path/to/rte_sdk

    cd ${RTE_SDK}/examples/flow_filtering

Set the target, for example:

.. code-block:: console

    export RTE_TARGET=x86_64-native-linuxapp-gcc

See the *DPDK Getting Started* Guide for possible ``RTE_TARGET`` values.

Build the application as follows:

.. code-block:: console

    make


Running the Application
-----------------------

To run the example in a ``linuxapp`` environment:

.. code-block:: console

    ./build/flow -l 1 -n 1

Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.


Explanation
-----------

The example is build from 2 main files,
``main.c`` which holds the example logic and ``flow_blocks.c`` that holds the
implementation for building the flow rule.

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function located in ``main.c`` file performs the initialization
and runs the main loop function.

The first task is to initialize the Environment Abstraction Layer (EAL).  The
``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");


The ``main()`` also allocates a mempool to hold the mbufs (Message Buffers)
used by the application:

.. code-block:: c

   mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 4096, 128, 0,
                                            RTE_MBUF_DEFAULT_BUF_SIZE,
                                            rte_socket_id());

Mbufs are the packet buffer structure used by DPDK. They are explained in
detail in the "Mbuf Library" section of the *DPDK Programmer's Guide*.

The ``main()`` function also initializes all the ports using the user defined
``init_port()`` function which is explained in the next section:

.. code-block:: c

   init_port();

Once the initialization is complete, we set the flow rule using the
following code:

.. code-block:: c

   /* create flow for send packet with */
   flow = generate_ipv4_flow(port_id, selected_queue,
                                SRC_IP, EMPTY_MASK,
                                DEST_IP, FULL_MASK, &error);
   if (!flow) {
          printf("Flow can't be created %d message: %s\n",
                       error.type,
                       error.message ? error.message : "(no stated reason)");
          rte_exit(EXIT_FAILURE, "error in creating flow");
   }

In the last part the application is ready to launch the
``main_loop()`` function. Which is explained below.


.. code-block:: c

   main_loop();

The Port Initialization  Function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The main functional part of the port initialization used in the flow filtering
application is shown below:

.. code-block:: c

   init_port(void)
   {
           int ret;
           uint16_t i;
           struct rte_eth_conf port_conf = {
                   .rxmode = {
                           .split_hdr_size = 0,
                           .ignore_offload_bitfield = 1,
                           .offloads = DEV_RX_OFFLOAD_CRC_STRIP,
                           },
                   .txmode = {
                           .offloads =
                                   DEV_TX_OFFLOAD_VLAN_INSERT |
                                   DEV_TX_OFFLOAD_IPV4_CKSUM  |
                                   DEV_TX_OFFLOAD_UDP_CKSUM   |
                                   DEV_TX_OFFLOAD_TCP_CKSUM   |
                                   DEV_TX_OFFLOAD_SCTP_CKSUM  |
                                   DEV_TX_OFFLOAD_TCP_TSO,
                   },
           };
           struct rte_eth_txconf txq_conf;
           struct rte_eth_rxconf rxq_conf;
           struct rte_eth_dev_info dev_info;

           printf(":: initializing port: %d\n", port_id);
           ret = rte_eth_dev_configure(port_id,
                   nr_queues, nr_queues, &port_conf);
           if (ret < 0) {
                   rte_exit(EXIT_FAILURE,
                           ":: cannot configure device: err=%d, port=%u\n",
                           ret, port_id);
           }

           rte_eth_dev_info_get(port_id, &dev_info);
           rxq_conf = dev_info.default_rxconf;
           rxq_conf.offloads = port_conf.rxmode.offloads;
           /* only set Rx queues: something we care only so far */
           for (i = 0; i < nr_queues; i++) {
                   ret = rte_eth_rx_queue_setup(port_id, i, 512,
                           rte_eth_dev_socket_id(port_id),
                           &rxq_conf,
                           mbuf_pool);
                   if (ret < 0) {
                            rte_exit(EXIT_FAILURE,
                                    ":: Rx queue setup failed: err=%d, port=%u\n",
                                    ret, port_id);
                   }
           }

           txq_conf = dev_info.default_txconf;
           txq_conf.offloads = port_conf.txmode.offloads;

           for (i = 0; i < nr_queues; i++) {
                   ret = rte_eth_tx_queue_setup(port_id, i, 512,
                           rte_eth_dev_socket_id(port_id),
                           &txq_conf);
                   if (ret < 0) {
                           rte_exit(EXIT_FAILURE,
                                   ":: Tx queue setup failed: err=%d, port=%u\n",
                                   ret, port_id);
                   }
          }

           rte_eth_promiscuous_enable(port_id);
           ret = rte_eth_dev_start(port_id);
           if (ret < 0) {
                   rte_exit(EXIT_FAILURE,
                           "rte_eth_dev_start:err=%d, port=%u\n",
                           ret, port_id);
           }

           assert_link_status();

           printf(":: initializing port: %d done\n", port_id);
   }

The Ethernet port is configured with default settings using the
``rte_eth_dev_configure()`` function and the ``port_conf_default`` struct:

.. code-block:: c

   struct rte_eth_conf port_conf = {
           .rxmode = {
                   .split_hdr_size = 0,
                   .ignore_offload_bitfield = 1,
                   .offloads = DEV_RX_OFFLOAD_CRC_STRIP,
                   },
           .txmode = {
                   .offloads =
                           DEV_TX_OFFLOAD_VLAN_INSERT |
                           DEV_TX_OFFLOAD_IPV4_CKSUM  |
                           DEV_TX_OFFLOAD_UDP_CKSUM   |
                           DEV_TX_OFFLOAD_TCP_CKSUM   |
                           DEV_TX_OFFLOAD_SCTP_CKSUM  |
                           DEV_TX_OFFLOAD_TCP_TSO,
                   },
           };

   ret = rte_eth_dev_configure(port_id, nr_queues, nr_queues, &port_conf);
   if (ret < 0) {
        rte_exit(EXIT_FAILURE,
                 ":: cannot configure device: err=%d, port=%u\n",
                 ret, port_id);
   }
   rte_eth_dev_info_get(port_id, &dev_info);
   rxq_conf = dev_info.default_rxconf;
   rxq_conf.offloads = port_conf.rxmode.offloads;

For this example we are configuring number of rx and tx queues that are connected
to a single port.

.. code-block:: c

   for (i = 0; i < nr_queues; i++) {
          ret = rte_eth_rx_queue_setup(port_id, i, 512,
                                       rte_eth_dev_socket_id(port_id),
                                       &rxq_conf,
                                       mbuf_pool);
          if (ret < 0) {
                  rte_exit(EXIT_FAILURE,
                          ":: Rx queue setup failed: err=%d, port=%u\n",
                          ret, port_id);
          }
   }

   for (i = 0; i < nr_queues; i++) {
          ret = rte_eth_tx_queue_setup(port_id, i, 512,
                                       rte_eth_dev_socket_id(port_id),
                                       &txq_conf);
          if (ret < 0) {
                  rte_exit(EXIT_FAILURE,
                           ":: Tx queue setup failed: err=%d, port=%u\n",
                           ret, port_id);
          }
   }

In the next step we create and apply the flow rule. which is to send packets
with destination ip equals to 192.168.1.1 to queue number 1. The detail
explanation of the ``generate_ipv4_flow()`` appears later in this document:

.. code-block:: c

   flow = generate_ipv4_flow(port_id, selected_queue,
                             SRC_IP, EMPTY_MASK,
                             DEST_IP, FULL_MASK, &error);

We are setting the RX port to promiscuous mode:

.. code-block:: c

   rte_eth_promiscuous_enable(port_id);

The last step is to start the port.

.. code-block:: c

   ret = rte_eth_dev_start(port_id);
   if (ret < 0)  {
        rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err%d, port=%u\n",
                        ret, port_id);
   }


The main_loop function
~~~~~~~~~~~~~~~~~~~~~~

As we saw above the ``main()`` function calls an application function to handle
the main loop. For the flow filtering application the main_loop function
looks like the following:

.. code-block:: c

   static void
   main_loop(void)
   {
           struct rte_mbuf *mbufs[32];
           struct ether_hdr *eth_hdr;
           uint16_t nb_rx;
           uint16_t i;
           uint16_t j;

           while (!force_quit) {
                   for (i = 0; i < nr_queues; i++) {
                           nb_rx = rte_eth_rx_burst(port_id,
                                                   i, mbufs, 32);
                           if (nb_rx) {
                                   for (j = 0; j < nb_rx; j++) {
                                           struct rte_mbuf *m = mbufs[j];

                                           eth_hdr = rte_pktmbuf_mtod(m,
                                                        struct ether_hdr *);
                                           print_ether_addr("src=",
                                                        &eth_hdr->s_addr);
                                           print_ether_addr(" - dst=",
                                                        &eth_hdr->d_addr);
                                           printf(" - queue=0x%x",
                                                           (unsigned int)i);
                                           printf("\n");
                                           rte_pktmbuf_free(m);
                                   }
                           }
                   }
           }
           /* closing and releasing resources */
           rte_flow_flush(port_id, &error);
           rte_eth_dev_stop(port_id);
           rte_eth_dev_close(port_id);
   }

The main work of the application is reading the packets from all
queues and printing for each packet the destination queue:

.. code-block:: c

    while (!force_quit) {
        for (i = 0; i < nr_queues; i++) {
                   nb_rx = rte_eth_rx_burst(port_id, i, mbufs, 32);
                if (nb_rx) {
                        for (j = 0; j < nb_rx; j++) {
                             struct rte_mbuf *m = mbufs[j];
                             eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
                             print_ether_addr("src=", &eth_hdr->s_addr);
                             print_ether_addr(" - dst=", &eth_hdr->d_addr);
                             printf(" - queue=0x%x", (unsigned int)i);
                             printf("\n");
                             rte_pktmbuf_free(m);
                        }
                }
           }
    }


The forwarding loop can be interrupted and the application closed using
``Ctrl-C``. Which results in closing the port and the device using
``rte_eth_dev_stop`` and ``rte_eth_dev_close``

The generate_ipv4_flow function
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The generate_ipv4_rule function is responsible for creating the flow rule.
This function is located in the ``flow_blocks.c`` file.

.. code-block:: c

   static struct rte_flow *
   generate_ipv4_flow(uint8_t port_id, uint16_t rx_q,
                   uint32_t src_ip, uint32_t src_mask,
                   uint32_t dest_ip, uint32_t dest_mask,
                   struct rte_flow_error *error)
   {
           struct rte_flow_attr attr;
           struct rte_flow_item pattern[MAX_PATTERN_NUM];
           struct rte_flow_action action[MAX_PATTERN_NUM];
           struct rte_flow *flow = NULL;
           struct rte_flow_action_queue queue = { .index = rx_q };
           struct rte_flow_item_eth eth_spec;
           struct rte_flow_item_eth eth_mask;
           struct rte_flow_item_vlan vlan_spec;
           struct rte_flow_item_vlan vlan_mask;
           struct rte_flow_item_ipv4 ip_spec;
           struct rte_flow_item_ipv4 ip_mask;

           memset(pattern, 0, sizeof(pattern));
           memset(action, 0, sizeof(action));

           /*
            * set the rule attribute.
            * in this case only ingress packets will be checked.
            */
           memset(&attr, 0, sizeof(struct rte_flow_attr));
           attr.ingress = 1;

           /*
            * create the action sequence.
            * one action only,  move packet to queue
            */

           action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
           action[0].conf = &queue;
           action[1].type = RTE_FLOW_ACTION_TYPE_END;

           /*
            * set the first level of the pattern (eth).
            * since in this example we just want to get the
            * ipv4 we set this level to allow all.
            */
           memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
           memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
           eth_spec.type = 0;
           eth_mask.type = 0;
           pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
           pattern[0].spec = &eth_spec;
           pattern[0].mask = &eth_mask;

           /*
            * setting the second level of the pattern (vlan).
            * since in this example we just want to get the
            * ipv4 we also set this level to allow all.
            */
           memset(&vlan_spec, 0, sizeof(struct rte_flow_item_vlan));
           memset(&vlan_mask, 0, sizeof(struct rte_flow_item_vlan));
           pattern[1].type = RTE_FLOW_ITEM_TYPE_VLAN;
           pattern[1].spec = &vlan_spec;
           pattern[1].mask = &vlan_mask;

           /*
            * setting the third level of the pattern (ip).
            * in this example this is the level we care about
            * so we set it according to the parameters.
            */
           memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
           memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
           ip_spec.hdr.dst_addr = htonl(dest_ip);
           ip_mask.hdr.dst_addr = dest_mask;
           ip_spec.hdr.src_addr = htonl(src_ip);
           ip_mask.hdr.src_addr = src_mask;
           pattern[2].type = RTE_FLOW_ITEM_TYPE_IPV4;
           pattern[2].spec = &ip_spec;
           pattern[2].mask = &ip_mask;

           /* the final level must be always type end */
           pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

           int res = rte_flow_validate(port_id, &attr, pattern, action, error);
           if(!res)
               flow = rte_flow_create(port_id, &attr, pattern, action, error);

           return flow;
   }

The first part of the function is declaring the structures that will be used.

.. code-block:: c

   struct rte_flow_attr attr;
   struct rte_flow_item pattern[MAX_PATTERN_NUM];
   struct rte_flow_action action[MAX_PATTERN_NUM];
   struct rte_flow *flow;
   struct rte_flow_error error;
   struct rte_flow_action_queue queue = { .index = rx_q };
   struct rte_flow_item_eth eth_spec;
   struct rte_flow_item_eth eth_mask;
   struct rte_flow_item_vlan vlan_spec;
   struct rte_flow_item_vlan vlan_mask;
   struct rte_flow_item_ipv4 ip_spec;
   struct rte_flow_item_ipv4 ip_mask;

The following part create the flow attributes, in our case ingress.

.. code-block:: c

   memset(&attr, 0, sizeof(struct rte_flow_attr));
   attr.ingress = 1;

The third part defines the action to be taken when a packet matches
the rule. In this case send the packet to queue.

.. code-block:: c

   action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
   action[0].conf = &queue;
   action[1].type = RTE_FLOW_ACTION_TYPE_END;

The forth part is responsible for creating the pattern and is build from
number of step. In each step we build one level of the pattern starting with
the lowest one.

Setting the first level of the pattern ETH:

.. code-block:: c

   memset(&eth_spec, 0, sizeof(struct rte_flow_item_eth));
   memset(&eth_mask, 0, sizeof(struct rte_flow_item_eth));
   eth_spec.type = 0;
   eth_mask.type = 0;
   pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
   pattern[0].spec = &eth_spec;
   pattern[0].mask = &eth_mask;

Setting the second level of the pattern VLAN:

.. code-block:: c

   memset(&vlan_spec, 0, sizeof(struct rte_flow_item_vlan));
   memset(&vlan_mask, 0, sizeof(struct rte_flow_item_vlan));
   pattern[1].type = RTE_FLOW_ITEM_TYPE_VLAN;
   pattern[1].spec = &vlan_spec;
   pattern[1].mask = &vlan_mask;

Setting the third level ip:

.. code-block:: c

   memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
   memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
   ip_spec.hdr.dst_addr = htonl(dest_ip);
   ip_mask.hdr.dst_addr = dest_mask;
   ip_spec.hdr.src_addr = htonl(src_ip);
   ip_mask.hdr.src_addr = src_mask;
   pattern[2].type = RTE_FLOW_ITEM_TYPE_IPV4;
   pattern[2].spec = &ip_spec;
   pattern[2].mask = &ip_mask;

Closing the pattern part.

.. code-block:: c

   pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

The last part of the function is to validate the rule and create it.

.. code-block:: c

   int res = rte_flow_validate(port_id, &attr, pattern, action, &error);
   if (!res)
        flow = rte_flow_create(port_id, &attr, pattern, action, &error);

