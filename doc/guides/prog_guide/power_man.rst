..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Power Management
================

The DPDK Power Management feature allows users space applications to save power
by dynamically adjusting CPU frequency or entering into different C-States.

*   Adjusting the CPU frequency dynamically according to the utilization of RX queue.

*   Entering into different deeper C-States according to the adaptive algorithms to speculate
    brief periods of time suspending the application if no packets are received.

The interfaces for adjusting the operating CPU frequency are in the power management library.
C-State control is implemented in applications according to the different use cases.

CPU Frequency Scaling
---------------------

The Linux kernel provides a cpufreq module for CPU frequency scaling for each lcore.
For example, for cpuX, /sys/devices/system/cpu/cpuX/cpufreq/ has the following sys files for frequency scaling:

*   affected_cpus

*   bios_limit

*   cpuinfo_cur_freq

*   cpuinfo_max_freq

*   cpuinfo_min_freq

*   cpuinfo_transition_latency

*   related_cpus

*   scaling_available_frequencies

*   scaling_available_governors

*   scaling_cur_freq

*   scaling_driver

*   scaling_governor

*   scaling_max_freq

*   scaling_min_freq

*   scaling_setspeed

In the DPDK, scaling_governor is configured in user space.
Then, a user space application can prompt the kernel by writing scaling_setspeed to adjust the CPU frequency
according to the strategies defined by the user space application.

Core-load Throttling through C-States
-------------------------------------

Core state can be altered by speculative sleeps whenever the specified lcore has nothing to do.
In the DPDK, if no packet is received after polling,
speculative sleeps can be triggered according the strategies defined by the user space application.

Per-core Turbo Boost
--------------------

Individual cores can be allowed to enter a Turbo Boost state on a per-core
basis. This is achieved by enabling Turbo Boost Technology in the BIOS, then
looping through the relevant cores and enabling/disabling Turbo Boost on each
core.

Use of Power Library in a Hyper-Threaded Environment
----------------------------------------------------

In the case where the power library is in use on a system with Hyper-Threading enabled,
the frequency on the physical core is set to the highest frequency of the Hyper-Thread siblings.
So even though an application may request a scale down, the core frequency will
remain at the highest frequency until all Hyper-Threads on that core request a scale down.

API Overview of the Power Library
---------------------------------

The main methods exported by power library are for CPU frequency scaling and include the following:

*   **Freq up**: Prompt the kernel to scale up the frequency of the specific lcore.

*   **Freq down**: Prompt the kernel to scale down the frequency of the specific lcore.

*   **Freq max**: Prompt the kernel to scale up the frequency of the specific lcore to the maximum.

*   **Freq min**: Prompt the kernel to scale down the frequency of the specific lcore to the minimum.

*   **Get available freqs**: Read the available frequencies of the specific lcore from the sys file.

*   **Freq get**: Get the current frequency of the specific lcore.

*   **Freq set**: Prompt the kernel to set the frequency for the specific lcore.

*   **Enable turbo**: Prompt the kernel to enable Turbo Boost for the specific lcore.

*   **Disable turbo**: Prompt the kernel to disable Turbo Boost for the specific lcore.

User Cases
----------

The power management mechanism is used to save power when performing L3 forwarding.


Empty Poll API
--------------

Abstract
~~~~~~~~

For packet processing workloads such as DPDK polling is continuous.
This means CPU cores always show 100% busy independent of how much work
those cores are doing. It is critical to accurately determine how busy
a core is hugely important for the following reasons:

        * No indication of overload conditions
        * User does not know how much real load is on a system, resulting
          in wasted energy as no power management is utilized

Compared to the original l3fwd-power design, instead of going to sleep
after detecting an empty poll, the new mechanism just lowers the core frequency.
As a result, the application does not stop polling the device, which leads
to improved handling of bursts of traffic.

When the system become busy, the empty poll mechanism can also increase the core
frequency (including turbo) to do best effort for intensive traffic. This gives
us more flexible and balanced traffic awareness over the standard l3fwd-power
application.


Proposed Solution
~~~~~~~~~~~~~~~~~
The proposed solution focuses on how many times empty polls are executed.
The less the number of empty polls, means current core is busy with processing
workload, therefore, the higher frequency is needed. The high empty poll number
indicates the current core not doing any real work therefore, we can lower the
frequency to safe power.

In the current implementation, each core has 1 empty-poll counter which assume
1 core is dedicated to 1 queue. This will need to be expanded in the future to
support multiple queues per core.

Power state definition:
^^^^^^^^^^^^^^^^^^^^^^^

* LOW:  Not currently used, reserved for future use.

* MED:  the frequency is used to process modest traffic workload.

* HIGH: the frequency is used to process busy traffic workload.

There are two phases to establish the power management system:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
* Training phase. This phase is used to measure the optimal frequency
  change thresholds for a given system. The thresholds will differ from
  system to system due to differences in processor micro-architecture,
  cache and device configurations.
  In this phase, the user must ensure that no traffic can enter the
  system so that counts can be measured for empty polls at low, medium
  and high frequencies. Each frequency is measured for two seconds.
  Once the training phase is complete, the threshold numbers are
  displayed, and normal mode resumes, and traffic can be allowed into
  the system. These threshold number can be used on the command line
  when starting the application in normal mode to avoid re-training
  every time.

* Normal phase. Every 10ms the run-time counters are compared
  to the supplied threshold values, and the decision will be made
  whether to move to a different power state (by adjusting the
  frequency).

API Overview for Empty Poll Power Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* **State Init**: initialize the power management system.

* **State Free**: free the resource hold by power management system.

* **Update Empty Poll Counter**: update the empty poll counter.

* **Update Valid Poll Counter**: update the valid poll counter.

* **Set the Frequency Index**: update the power state/frequency mapping.

* **Detect empty poll state change**: empty poll state change detection algorithm then take action.

User Cases
----------
The mechanism can applied to any device which is based on polling. e.g. NIC, FPGA.

Ethernet PMD Power Management API
---------------------------------

Abstract
~~~~~~~~

Existing power management mechanisms require developers to change application
design or change code to make use of it. The PMD power management API provides a
convenient alternative by utilizing Ethernet PMD RX callbacks, and triggering
power saving whenever empty poll count reaches a certain number.

* Monitor
   This power saving scheme will put the CPU into optimized power state and
   monitor the Ethernet PMD RX descriptor address, waking the CPU up whenever
   there's new traffic. Support for this scheme may not be available on all
   platforms, and further limitations may apply (see below).

* Pause
   This power saving scheme will avoid busy polling by either entering
   power-optimized sleep state with ``rte_power_pause()`` function, or, if it's
   not supported by the underlying platform, use ``rte_pause()``.

* Frequency scaling
   This power saving scheme will use ``librte_power`` library functionality to
   scale the core frequency up/down depending on traffic volume.
   The reaction time of the frequency scaling mode is longer
   than the pause and monitor mode.

The "monitor" mode is only supported in the following configurations and scenarios:

* On Linux* x86_64, `rte_power_monitor()` requires WAITPKG instruction set being
  supported by the CPU, while `rte_power_monitor_multi()` requires WAITPKG and
  RTM instruction sets being supported by the CPU. RTM instruction set may also
  require booting the Linux with `tsx=on` command line parameter. Please refer
  to your platform documentation for further information.

* If ``rte_cpu_get_intrinsics_support()`` function indicates that
  ``rte_power_monitor_multi()`` function is supported by the platform, then
  monitoring multiple Ethernet Rx queues for traffic will be supported.

* If ``rte_cpu_get_intrinsics_support()`` function indicates that only
  ``rte_power_monitor()`` is supported by the platform, then monitoring will be
  limited to a mapping of 1 core 1 queue (thus, each Rx queue will have to be
  monitored from a different lcore).

* If ``rte_cpu_get_intrinsics_support()`` function indicates that neither of the
  two monitoring functions are supported, then monitor mode will not be supported.

* Not all Ethernet drivers support monitoring, even if the underlying
  platform may support the necessary CPU instructions. Please refer to
  :doc:`../nics/overview` for more information.


API Overview for Ethernet PMD Power Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Queue Enable**: Enable specific power scheme for certain queue/port/core.

* **Queue Disable**: Disable power scheme for certain queue/port/core.

References
----------

*   The :doc:`../sample_app_ug/l3_forward_power_man`
    chapter in the :doc:`../sample_app_ug/index` section.

*   The :doc:`../sample_app_ug/vm_power_management`
    chapter in the :doc:`../sample_app_ug/index` section.

*   The :doc:`../nics/overview` chapter in the :doc:`../nics/index` section
