..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

User Cases
----------

The power management mechanism is used to save power when performing L3 forwarding.

References
----------

*   l3fwd-power: The sample application in DPDK that performs L3 forwarding with power management.

*   The "L3 Forwarding with Power Management Sample Application" chapter in the *DPDK Sample Application's User Guide*.
