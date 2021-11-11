.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

Flow Performance Tool
=====================

Application for rte_flow performance testing.
The application provides the ability to test insertion rate of specific
rte_flow rule, by stressing it to the NIC, and calculates the insertion
and deletion rates.

The application allows to configure which rule to apply through several
options of the command line.

After that the application will start producing rules with same pattern
but increasing the outer IP source address by 1 each time, thus it will
give different flow each time, and all other items will have open masks.

To assess the rule insertion rate, the flow performance tool breaks
down the entire number of flow rule operations into windows of fixed size
(defaults to 100000 flow rule operations per window, but can be configured).
Then, the flow performance tool measures the total time per window and
computes an average time across all windows.

The application also provides the ability to measure rte flow deletion rate,
in addition to memory consumption before and after the flow rules' creation.

The app supports single and multiple core performance measurements, and
support multiple cores insertion/deletion as well.


Compiling the Application
-------------------------

The ``test-flow-perf`` application is compiled as part of the main compilation
of the DPDK libraries and tools.

Refer to the DPDK Getting Started Guides for details.


Running the Application
-----------------------

EAL Command-line Options
~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :doc:`EAL parameters (Linux) <../linux_gsg/linux_eal_parameters>`
or :doc:`EAL parameters (FreeBSD) <../freebsd_gsg/freebsd_eal_parameters>` for
a list of available EAL command-line options.


Flow Performance Options
~~~~~~~~~~~~~~~~~~~~~~~~

The following are the command-line options for the flow performance application.
They must be separated from the EAL options, shown in the previous section,
with a ``--`` separator:

.. code-block:: console

	sudo ./dpdk-test-flow_perf -n 4 -a 08:00.0 -- --ingress --ether --ipv4 --queue --rules-count=1000000

The command line options are:

*	``--help``
	Display a help message and quit.

*	``--rules-count=N``
	Set the total number of flow rules to insert,
	where 1 <= N <= "number of flow rules".
	The default value is 4,000,000.

*	``--rules-batch=N``
	Set the number of flow rules to insert per iteration window,
	where 1 <= N <= "number of flow rules per iteration window".
	The default value is 100,000 flow rules per iteration window.
	For a total of --rules-count=1000000 flow rules to be inserted
	and an iteration window size of --rules-batch=100000 flow rules,
	the application will measure the insertion rate 10 times
	(i.e., once every 100000 flow rules) and then report an average
	insertion rate across the 10 measurements.

*	``--dump-iterations``
	Print rates for each iteration window.
	Default iteration window equals to the rules-batch size (i.e., 100,000).

*	``--deletion-rate``
	Enable deletion rate calculations.

*	``--dump-socket-mem``
	Dump the memory stats for each socket before the insertion and after.

*	``--enable-fwd``
	Enable packets forwarding after insertion/deletion operations.

*	``--portmask=N``
	hexadecimal bitmask of ports to be used.

*	``--cores=N``
	Set the number of needed cores to insert/delete rte_flow rules.
	Default cores count is 1.

*       ``--random-priority=N,S``
        Create flows with the priority attribute set randomly between 0 to N - 1
        and use S as seed for the pseudo-random number generator.

*	``--meter-profile-alg``
	Set the traffic metering algorithm.
	Example: meter-profile-alg=srtcmp, default algorithm is srtcm_rfc2697

*	``--unique-data``
	Flag to set using unique data for all actions that support data,
	Such as header modify and encap actions. Default is using fixed
	data for any action that support data for all flows.

*	``--rxq=N``
	Set the count of receive queues, default is 1.

*	``--txq=N``
	Set the count of send queues, default is 1.

*	``--rxd=N``
	Set the count of rxd, default is 256.

*	``--txd=N``
	Set the count of txd, default is 256.

*	``--mbuf-size=N``
	Set the size of mbuf, default size is 2048.

*	``--mbuf-cache-size=N``
	Set the size of mbuf cache, default size is 512.

*	``--total-mbuf-count=N``
	Set the count of total mbuf number, default count is 32000.

*	``--meter-profile=N1,N2,N3``
	Set the CIR, CBS and EBS parameters, default values are 1250000, 156250 and 0.

*	``--packet-mode``
	Enable packet mode for meter profile.

Attributes:

*	``--ingress``
	Set Ingress attribute to all flows attributes.

*	``--egress``
	Set Egress attribute to all flows attributes.

*	``--transfer``
	Set Transfer attribute to all flows attributes.

*	``--group=N``
	Set group for all flows, where N >= 0.
	Default group is 0.

Items:

*	``--ether``
	Add Ether item to all flows items, This item have open mask.

*	``--vlan``
	Add VLAN item to all flows items,
	This item have VLAN value defined in user_parameters.h
	under ``VNI_VALUE`` with full mask, default value = 1.
	Other fields are open mask.

*	``--ipv4``
	Add IPv4 item to all flows items,
	This item have incremental source IP, with full mask.
	Other fields are open mask.

*	``--ipv6``
	Add IPv6 item to all flows item,
	This item have incremental source IP, with full mask.
	Other fields are open mask.

*	``--tcp``
	Add TCP item to all flows items, This item have open mask.

*	``--udp``
	Add UDP item to all flows items, This item have open mask.

*	``--vxlan``
	Add VXLAN item to all flows items,
	This item have VNI value defined in user_parameters.h
	under ``VNI_VALUE`` with full mask, default value = 1.
	Other fields are open mask.

*	``--vxlan-gpe``
	Add VXLAN-GPE item to all flows items,
	This item have VNI value defined in user_parameters.h
	under ``VNI_VALUE`` with full mask, default value = 1.
	Other fields are open mask.

*	``--gre``
	Add GRE item to all flows items,
	This item have protocol value defined in user_parameters.h
	under ``GRE_PROTO`` with full mask, default protocol = 0x6558 "Ether"
	Other fields are open mask.

*	``--geneve``
	Add GENEVE item to all flows items,
	This item have VNI value defined in user_parameters.h
	under ``VNI_VALUE`` with full mask, default value = 1.
	Other fields are open mask.

*	``--gtp``
	Add GTP item to all flows items,
	This item have TEID value defined in user_parameters.h
	under ``TEID_VALUE`` with full mask, default value = 1.
	Other fields are open mask.

*	``--meta``
	Add Meta item to all flows items,
	This item have data value defined in user_parameters.h
	under ``META_DATA`` with full mask, default value = 1.
	Other fields are open mask.

*	``--tag``
	Add Tag item to all flows items,
	This item have data value defined in user_parameters.h
	under ``META_DATA`` with full mask, default value = 1.

	Also it have tag value defined in user_parameters.h
	under ``TAG_INDEX`` with full mask, default value = 0.
	Other fields are open mask.

*	``--icmpv4``
	Add icmpv4 item to all flows items, This item have open mask.

*	``--icmpv6``
	Add icmpv6 item to all flows items, This item have open mask.


Actions:

*	``--port-id``
	Add port redirection action to all flows actions.
	Port redirection destination is defined in user_parameters.h
	under PORT_ID_DST, default value = 1.

       It can also has optional parameter like --port-id=N[,M] to
       specify the destination port, the number of values should be
       the same with number of set bits in portmask.

*	``--rss``
	Add RSS action to all flows actions,
	The queues in RSS action will be all queues configured
	in the app.

*	``--queue``
	Add queue action to all flows items,
	The queue will change in round robin state for each flow.

	For example:
		The app running with 4 RX queues
		Flow #0: queue index 0
		Flow #1: queue index 1
		Flow #2: queue index 2
		Flow #3: queue index 3
		Flow #4: queue index 0
		...

*	``--jump``
	Add jump action to all flows actions.
	Jump action destination is defined in user_parameters.h
	under ``JUMP_ACTION_TABLE``, default value = 2.

*	``--mark``
	Add mark action to all flows actions.
	Mark action id is defined in user_parameters.h
	under ``MARK_ID``, default value = 1.

*	``--count``
	Add count action to all flows actions.

*	``--set-meta``
	Add set-meta action to all flows actions.
	Meta data is defined in user_parameters.h under ``META_DATA``
	with full mask, default value = 1.

*	``--set-tag``
	Add set-tag action to all flows actions.
	Meta data is defined in user_parameters.h under ``META_DATA``
	with full mask, default value = 1.

	Tag index is defined in user_parameters.h under ``TAG_INDEX``
	with full mask, default value = 0.

*	``--drop``
	Add drop action to all flows actions.

*	``--hairpin-queue=N``
	Add hairpin queue action to all flows actions.
	The queue will change in round robin state for each flow.

	For example:
		The app running with 4 RX hairpin queues and 4 normal RX queues
		Flow #0: queue index 4
		Flow #1: queue index 5
		Flow #2: queue index 6
		Flow #3: queue index 7
		Flow #4: queue index 4
		...

*	``--hairpin-rss=N``
	Add hairpin RSS action to all flows actions.
	The queues in RSS action will be all hairpin queues configured
	in the app.

*	``--set-src-mac``
	Add set source mac action to all flows actions.
	The mac to be set is random each flow.

*	``--set-dst-mac``
	Add set destination mac action to all flows actions.
	The mac to be set is random each flow.

*	``-set-src-ipv4``
	Add set source ipv4 action to all flows actions.
	The ipv4 header to be set is random each flow.

*	``--set-dst-ipv4``
	Add set destination ipv4 action to all flows actions.
	The ipv4 header to be set is random each flow.

*	``--set-src-ipv6``
	Add set source ipv6 action to all flows actions.
	The ipv6 header to be set is random each flow.

*	``--set-dst-ipv6``
	Add set destination ipv6 action to all flows actions.
	The ipv6 header to be set is random each flow.

*	``--set-src-tp``
	Add set source tp action to all flows actions.
	The tp sport header to be set is random each flow.

*	``--set-dst-tp``
	Add set destination tp action to all flows actions.
	The tp dport header to be set is random each flow.

*	``--inc-tcp-ack``
	Add increment TCP acknowledgment by one to all flows actions.

*	``--dec-tcp-ack``
	Add decrement TCP acknowledgment by one to all flows actions.

*	``--inc-tcp-seq``
	Add increment TCP sequence by one to all flows actions.

*	``--dec-tcp-seq``
	Add decrement TCP sequence by one to all flows actions.

*	``--set-ttl``
	Add set IP ttl action to all flows actions.
	The ttl value to be set is random each flow.

*	``--dec-ttl``
	Add decrement IP ttl by one to all flows actions.

*	``--set-ipv4-dscp``
	Add set IPv4 dscp action to all flows actions.
	The dscp value to be is random each flow.

*	``--set-ipv6-dscp``
	Add set IPv6 dscp action to all flows actions.
	The dscp value to be is random each flow.

*	``--flag``
	Add flag action to all flows actions.

*	``--raw-encap=<DATA>``
	Add raw encap action to all flows actions.
	Data is the data needed to be encaped, with fixed values.
	Example: raw-encap=ether,ipv4,udp,vxlan

*	``--raw-decap=<DATA>``
	Add raw decap action to all flows actions.
	Data is the data needed to be decaped, with fixed values.
	Example: raw-decap=ether,ipv4,gre

*	``--vxlan-encap``
	Add vxlan encap action to all flows actions.
	Data to encap is fixed with pattern: ether,ipv4,udp,vxlan,
	all encapped items have fixed values.

*	``--vxlan-decap``
	Add vxlan decap action to all flows actions.

*	``--policy-mtr=<str>``
	Add policy-mtr to create meter with policy and specify policy actions.
	Example: policy-mtr=rss,mark::drop

*	``--meter``
	Add meter action to all flows actions.
	Currently, 1 meter profile -> N meter rules -> N rte flows.
