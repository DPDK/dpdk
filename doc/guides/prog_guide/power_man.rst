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

* **Get Emptypoll Max**: Get the configured number of empty polls to wait before
  entering sleep state.

* **Set Emptypoll Max**: Set the number of empty polls to wait before entering
  sleep state.

* **Get Pause Duration**: Get the configured duration (microseconds) to be used
  in the Pause callback.

* **Set Pause Duration**: Set the duration of the pause (microseconds) used in
  the Pause mode callback.

* **Get Scaling Min Freq**: Get the configured minimum frequency (kHz) to be used
  in Frequency Scaling mode.

* **Set Scaling Min Freq**: Set the minimum frequency (kHz) to be used in Frequency
  Scaling mode.

* **Get Scaling Max Freq**: Get the configured maximum frequency (kHz) to be used
  in Frequency Scaling mode.

* **Set Scaling Max Freq**: Set the maximum frequency (kHz) to be used in Frequency
  Scaling mode.

Intel Uncore API
----------------

Abstract
~~~~~~~~

Uncore is a term used by Intel to describe the functions of a microprocessor
that are not in the core, but which must be closely connected to the core
to achieve high performance: L3 cache, on-die memory controller, etc.
Significant power savings can be achieved by reducing the uncore frequency
to its lowest value.

The Linux kernel provides the driver "intel-uncore-frequency"
to control the uncore frequency limits for x86 platform.
The driver is available from kernel version 5.6 and above.
Also CONFIG_INTEL_UNCORE_FREQ_CONTROL will need to be enabled in the kernel,
which was added in 5.6.
This manipulates the context of MSR 0x620,
which sets min/max of the uncore for the SKU.

API Overview for Intel Uncore
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Overview of each function in the Intel Uncore API,
with explanation of what they do.
Each function should not be called in the fast path.

Uncore Power Init
  Initialize uncore power, populate frequency array
  and record original min & max for die on pkg.

Uncore Power Exit
  Exit uncore power, restoring original min & max for die on pkg.

Get Uncore Power Freq
  Get current uncore freq index for die on pkg.

Set Uncore Power Freq
  Set min & max uncore freq index for die on pkg
  to specified index value (min and max will be the same).

Uncore Power Max
  Set min & max uncore freq to maximum frequency index for die on pkg
  (min and max will be the same).

Uncore Power Min
  Set min & max uncore freq to minimum frequency index for die on pkg
  (min and max will be the same).

Get Num Freqs
  Get the number of frequencies in the index array.

Get Num Pkgs
  Get the number of packages (CPU's) on the system.

Get Num Dies
  Get the number of die's on a given package.

References
----------

*   The :doc:`../sample_app_ug/l3_forward_power_man`
    chapter in the :doc:`../sample_app_ug/index` section.

*   The :doc:`../sample_app_ug/vm_power_management`
    chapter in the :doc:`../sample_app_ug/index` section.

*   The :doc:`../nics/overview` chapter in the :doc:`../nics/index` section
