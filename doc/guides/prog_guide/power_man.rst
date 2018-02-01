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

References
----------

*   l3fwd-power: The sample application in DPDK that performs L3 forwarding with power management.

*   The "L3 Forwarding with Power Management Sample Application" chapter in the *DPDK Sample Application's User Guide*.
