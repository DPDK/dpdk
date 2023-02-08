..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

L3 Forwarding with Power Management Sample Application
======================================================

Introduction
------------

The L3 Forwarding with Power Management application is an example of power-aware packet processing using the DPDK.
The application is based on existing L3 Forwarding sample application,
with the power management algorithms to control the P-states and
C-states of the Intel processor via a power management library.

Overview
--------

The application demonstrates the use of the Power libraries in the DPDK to implement packet forwarding.
The initialization and run-time paths are very similar to those of the :doc:`l3_forward`.
The main difference from the L3 Forwarding sample application is that this application introduces power-aware optimization algorithms
by leveraging the Power library to control P-state and C-state of processor based on packet load.

The DPDK includes poll-mode drivers to configure Intel NIC devices and their receive (Rx) and transmit (Tx) queues.
The design principle of this PMD is to access the Rx and Tx descriptors directly without any interrupts to quickly receive,
process and deliver packets in the user space.

In general, the DPDK executes an endless packet processing loop on dedicated IA cores that include the following steps:

*   Retrieve input packets through the PMD to poll Rx queue

*   Process each received packet or provide received packets to other processing cores through software queues

*   Send pending output packets to Tx queue through the PMD

In this way, the PMD achieves better performance than a traditional interrupt-mode driver,
at the cost of keeping cores active and running at the highest frequency,
hence consuming the maximum power all the time.
However, during the period of processing light network traffic,
which happens regularly in communication infrastructure systems due to well-known "tidal effect",
the PMD is still busy waiting for network packets, which wastes a lot of power.

Processor performance states (P-states) are the capability of an Intel processor
to switch between different supported operating frequencies and voltages.
If configured correctly, according to system workload, this feature provides power savings.
CPUFreq is the infrastructure provided by the Linux* kernel to control the processor performance state capability.
CPUFreq supports a user space governor that enables setting frequency via manipulating the virtual file device from a user space application.
The Power library in the DPDK provides a set of APIs for manipulating a virtual file device to allow user space application
to set the CPUFreq governor and set the frequency of specific cores.

This application includes a P-state power management algorithm to generate a frequency hint to be sent to CPUFreq.
The algorithm uses the number of received and available Rx packets on recent polls to make a heuristic decision to scale frequency up/down.
Specifically, some thresholds are checked to see whether a specific core running a DPDK polling thread needs to increase frequency
a step up based on the near to full trend of polled Rx queues.
Also, it decreases frequency a step if packet processed per loop is far less than the expected threshold
or the thread's sleeping time exceeds a threshold.

C-States are also known as sleep states.
They allow software to put an Intel core into a low power idle state from which it is possible to exit via an event, such as an interrupt.
However, there is a tradeoff between the power consumed in the idle state and the time required to wake up from the idle state (exit latency).
Therefore, as you go into deeper C-states, the power consumed is lower but the exit latency is increased. Each C-state has a target residency.
It is essential that when entering into a C-state, the core remains in this C-state for at least as long as the target residency in order
to fully realize the benefits of entering the C-state.
CPUIdle is the infrastructure provide by the Linux kernel to control the processor C-state capability.
Unlike CPUFreq, CPUIdle does not provide a mechanism that allows the application to change C-state.
It actually has its own heuristic algorithms in kernel space to select target C-state to enter by executing privileged instructions like HLT and MWAIT,
based on the speculative sleep duration of the core.
In this application, we introduce a heuristic algorithm that allows packet processing cores to sleep for a short period
if there is no Rx packet received on recent polls.
In this way, CPUIdle automatically forces the corresponding cores to enter deeper C-states
instead of always running to the C0 state waiting for packets.

.. note::

    To fully demonstrate the power saving capability of using C-states,
    it is recommended to enable deeper C3 and C6 states in the BIOS during system boot up.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``l3fwd-power`` sub-directory.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l3fwd_power [EAL options] -- -p PORTMASK [-P]  --config(port,queue,lcore)[,(port,queue,lcore)] [--max-pkt-len PKTLEN] [--no-numa]

where,

*   -p PORTMASK: Hexadecimal bitmask of ports to configure

*   -P: Sets all ports to promiscuous mode so that packets are accepted regardless of the packet's Ethernet MAC destination address.
    Without this option, only packets with the Ethernet MAC destination address set to the Ethernet address of the port are accepted.

*   -u: optional, sets uncore min/max frequency to minimum value.

*   -U: optional, sets uncore min/max frequency to maximum value.

*   -i (frequency index): optional, sets uncore frequency to frequency index value, by setting min and max values to be the same.

*   --config (port,queue,lcore)[,(port,queue,lcore)]: determines which queues from which ports are mapped to which cores.

*   --max-pkt-len: optional, maximum packet length in decimal (64-9600)

*   --no-numa: optional, disables numa awareness

*   --telemetry:  Telemetry mode.

*   --pmd-mgmt: PMD power management mode.

*   --max-empty-polls : Number of empty polls to wait before entering sleep state. Applies to --pmd-mgmt mode only.

*   --pause-duration: Set the duration of the pause callback (microseconds). Applies to --pmd-mgmt mode only.

*   --scale-freq-min: Set minimum frequency for scaling. Applies to --pmd-mgmt mode only.

*   --scale-freq-max: Set maximum frequency for scaling. Applies to --pmd-mgmt mode only.

See :doc:`l3_forward` for details.
The L3fwd-power example reuses the L3fwd command line options.

Explanation
-----------

The following sections provide some explanation of the sample application code.
As mentioned in the overview section,
the initialization and run-time paths are identical to those of the L3 forwarding application.
The following sections describe aspects that are specific to the L3 Forwarding with Power Management sample application.

Power Library Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Power library is initialized in the main routine.
It changes the P-state governor to userspace for specific cores that are under control.
The Timer library is also initialized and several timers are created later on,
responsible for checking if it needs to scale down frequency at run time by checking CPU utilization statistics.

.. note::

    Only the power management related initialization is shown.

.. literalinclude:: ../../../examples/l3fwd-power/main.c
    :language: c
    :start-after: Power library initialized in the main routine. 8<
    :end-before: >8 End of power library initialization.

Monitoring Loads of Rx Queues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In general, the polling nature of the DPDK prevents the OS power management subsystem from knowing
if the network load is actually heavy or light.
In this sample, sampling network load work is done by monitoring received and
available descriptors on NIC Rx queues in recent polls.
Based on the number of returned and available Rx descriptors,
this example implements algorithms to generate frequency scaling hints and speculative sleep duration,
and use them to control P-state and C-state of processors via the power management library.
Frequency (P-state) control and sleep state (C-state) control work individually for each logical core,
and the combination of them contributes to a power efficient packet processing solution when serving light network loads.

The rte_eth_rx_burst() function and the newly-added rte_eth_rx_queue_count() function are used in the endless packet processing loop
to return the number of received and available Rx descriptors.
And those numbers of specific queue are passed to P-state and C-state heuristic algorithms
to generate hints based on recent network load trends.

.. note::

    Only power control related code is shown.

.. literalinclude:: ../../../examples/l3fwd-power/main.c
    :language: c
    :start-after: Main processing loop. 8<
    :end-before: >8 End of main processing loop.

P-State Heuristic Algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The power_freq_scaleup_heuristic() function is responsible for generating a frequency hint for the specified logical core
according to available descriptor number returned from rte_eth_rx_queue_count().
On every poll for new packets, the length of available descriptor on an Rx queue is evaluated,
and the algorithm used for frequency hinting is as follows:

*   If the size of available descriptors exceeds 96, the maximum frequency is hinted.

*   If the size of available descriptors exceeds 64, a trend counter is incremented by 100.

*   If the length of the ring exceeds 32, the trend counter is incremented by 1.

*   When the trend counter reached 10000 the frequency hint is changed to the next higher frequency.

.. note::

    The assumption is that the Rx queue size is 128 and the thresholds specified above
    must be adjusted accordingly based on actual hardware Rx queue size,
    which are configured via the rte_eth_rx_queue_setup() function.

In general, a thread needs to poll packets from multiple Rx queues.
Most likely, different queue have different load, so they would return different frequency hints.
The algorithm evaluates all the hints and then scales up frequency in an aggressive manner
by scaling up to highest frequency as long as one Rx queue requires.
In this way, we can minimize any negative performance impact.

On the other hand, frequency scaling down is controlled in the timer callback function.
Specifically, if the sleep times of a logical core indicate that it is sleeping more than 25% of the sampling period,
or if the average packet per iteration is less than expectation, the frequency is decreased by one step.

C-State Heuristic Algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Whenever recent rte_eth_rx_burst() polls return 5 consecutive zero packets,
an idle counter begins incrementing for each successive zero poll.
At the same time, the function power_idle_heuristic() is called to generate speculative sleep duration
in order to force logical to enter deeper sleeping C-state.
There is no way to control C- state directly, and the CPUIdle subsystem in OS is intelligent enough
to select C-state to enter based on actual sleep period time of giving logical core.
The algorithm has the following sleeping behavior depending on the idle counter:

*   If idle count less than 100, the counter value is used as a microsecond sleep value through rte_delay_us()
    which execute pause instructions to avoid costly context switch but saving power at the same time.

*   If idle count is between 100 and 999, a fixed sleep interval of 100 μs is used.
    A 100 μs sleep interval allows the core to enter the C1 state while keeping a fast response time in case new traffic arrives.

*   If idle count is greater than 1000, a fixed sleep value of 1 ms is used until the next timer expiration is used.
    This allows the core to enter the C3/C6 states.

.. note::

    The thresholds specified above need to be adjusted for different Intel processors and traffic profiles.

If a thread polls multiple Rx queues and different queue returns different sleep duration values,
the algorithm controls the sleep time in a conservative manner by sleeping for the least possible time
in order to avoid a potential performance impact.

Telemetry Mode
--------------

The telemetry mode support for ``l3fwd-power`` is a standalone mode, in this mode
``l3fwd-power`` does simple l3fwding along with calculating empty polls, full polls,
and busy percentage for each forwarding core. The aggregation of these
values of all cores is reported as application level telemetry to metric
library for every 500ms from the main core.

The busy percentage is calculated by recording the poll_count
and when the count reaches a defined value the total
cycles it took is measured and compared with minimum and maximum
reference cycles and accordingly busy rate is set  to either 0% or
50% or 100%.

.. code-block:: console

        ./<build_dir>/examples/dpdk-l3fwd-power --telemetry -l 1-3 -- -p 0x0f --config="(0,0,2),(0,1,3)" --telemetry

The new stats ``empty_poll`` , ``full_poll`` and ``busy_percent`` can be viewed by running the script
``/usertools/dpdk-telemetry-client.py`` and selecting the menu option ``Send for global Metrics``.

PMD power management Mode
-------------------------

The PMD power management  mode support for ``l3fwd-power`` is a standalone mode.
In this mode, ``l3fwd-power`` does simple l3fwding
along with enabling the power saving scheme on specific port/queue/lcore.
Main purpose for this mode is to demonstrate
how to use the PMD power management API.

.. code-block:: console

        ./build/examples/dpdk-l3fwd-power -l 1-3 --  --pmd-mgmt -p 0x0f --config="(0,0,2),(0,1,3)"

PMD Power Management Mode
-------------------------

There is also a traffic-aware operating mode that,
instead of using explicit power management,
will use automatic PMD power management.
This mode is limited to one queue per core,
and has three available power management schemes:

``monitor``
  This will use ``rte_power_monitor()`` function to enter
  a power-optimized state (subject to platform support).

``pause``
  This will use ``rte_power_pause()`` or ``rte_pause()``
  to avoid busy looping when there is no traffic.

``scale``
  This will use frequency scaling routines
  available in the ``librte_power`` library.
  The reaction time of the scale mode is longer
  than the pause and monitor mode.

See :doc:`Power Management<../prog_guide/power_man>` chapter
in the DPDK Programmer's Guide for more details on PMD power management.

.. code-block:: console

        ./<build_dir>/examples/dpdk-l3fwd-power -l 1-3 -- -p 0x0f --config="(0,0,2),(0,1,3)" --pmd-mgmt=scale

Setting Uncore Values
---------------------

Uncore frequency can be adjusted through manipulating related sysfs entries
to adjust the minimum and maximum uncore values.
This will be set for each package and die on the SKU.
The driver for enabling this is available from kernel version 5.6 and above.
Three options are available for setting uncore frequency:

``-u``
  This will set uncore minimum and maximum frequencies to minimum possible value.

``-U``
  This will set uncore minimum and maximum frequencies to maximum possible value.

``-i``
  This will allow you to set the specific uncore frequency index that you want,
  by setting the uncore frequency to a frequency pointed by index.
  Frequency index's are set 100MHz apart from maximum to minimum.
  Frequency index values are in descending order,
  i.e., index 0 is maximum frequency index.

.. code-block:: console

   dpdk-l3fwd-power -l 1-3 -- -p 0x0f --config="(0,0,2),(0,1,3)" -i 1
