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

Profile Your Application
========================

The following sections describe methods of profiling DPDK applications on
different architectures.


Profiling on x86
----------------

Intel processors provide performance counters to monitor events.
Some tools provided by Intel, such as Intel® VTune™ Amplifier, can be used
to profile and benchmark an application.
See the *VTune Performance Analyzer Essentials* publication from Intel Press for more information.

For a DPDK application, this can be done in a Linux* application environment only.

The main situations that should be monitored through event counters are:

*   Cache misses

*   Branch mis-predicts

*   DTLB misses

*   Long latency instructions and exceptions

Refer to the
`Intel Performance Analysis Guide <http://software.intel.com/sites/products/collateral/hpc/vtune/performance_analysis_guide.pdf>`_
for details about application profiling.


Empty cycles tracing
~~~~~~~~~~~~~~~~~~~~

Iterations that yielded no RX packets (empty cycles, wasted iterations) can
be analyzed using VTune Amplifier. This profiling employs the
`Instrumentation and Tracing Technology (ITT) API
<https://software.intel.com/en-us/node/544195>`_
feature of VTune Amplifier and requires only reconfiguring the DPDK library,
no changes in a DPDK application are needed.

To trace wasted iterations on RX queues, first reconfigure DPDK with
``CONFIG_RTE_ETHDEV_RXTX_CALLBACKS`` and
``CONFIG_RTE_ETHDEV_PROFILE_ITT_WASTED_RX_ITERATIONS`` enabled.

Then rebuild DPDK, specifying paths to the ITT header and library, which can
be found in any VTune Amplifier distribution in the *include* and *lib*
directories respectively:

.. code-block:: console

    make EXTRA_CFLAGS=-I<path to ittnotify.h> \
         EXTRA_LDLIBS="-L<path to libittnotify.a> -littnotify"

Finally, to see wasted iterations in your performance analysis results,
select the *"Analyze user tasks, events, and counters"* checkbox in the
*"Analysis Type"* tab when configuring analysis via VTune Amplifier GUI.
Alternatively, when running VTune Amplifier via command line, specify
``-knob enable-user-tasks=true`` option.

Collected regions of wasted iterations will be marked on VTune Amplifier's
timeline as ITT tasks. These ITT tasks have predefined names, containing
Ethernet device and RX queue identifiers.


Profiling on ARM64
------------------

Using Linux perf
~~~~~~~~~~~~~~~~

The ARM64 architecture provide performance counters to monitor events.  The
Linux ``perf`` tool can be used to profile and benchmark an application.  In
addition to the standard events, ``perf`` can be used to profile arm64
specific PMU (Performance Monitor Unit) events through raw events (``-e``
``-rXX``).

For more derails refer to the
`ARM64 specific PMU events enumeration <http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.100095_0002_04_en/way1382543438508.html>`_.


High-resolution cycle counter
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The default ``cntvct_el0`` based ``rte_rdtsc()`` provides a portable means to
get a wall clock counter in user space. Typically it runs at <= 100MHz.

The alternative method to enable ``rte_rdtsc()`` for a high resolution wall
clock counter is through the armv8 PMU subsystem. The PMU cycle counter runs
at CPU frequency. However, access to the PMU cycle counter from user space is
not enabled by default in the arm64 linux kernel. It is possible to enable
cycle counter for user space access by configuring the PMU from the privileged
mode (kernel space).

By default the ``rte_rdtsc()`` implementation uses a portable ``cntvct_el0``
scheme.  Application can choose the PMU based implementation with
``CONFIG_RTE_ARM_EAL_RDTSC_USE_PMU``.

The example below shows the steps to configure the PMU based cycle counter on
an armv8 machine.

.. code-block:: console

    git clone https://github.com/jerinjacobk/armv8_pmu_cycle_counter_el0
    cd armv8_pmu_cycle_counter_el0
    make
    sudo insmod pmu_el0_cycle_counter.ko
    cd $DPDK_DIR
    make config T=arm64-armv8a-linuxapp-gcc
    echo "CONFIG_RTE_ARM_EAL_RDTSC_USE_PMU=y" >> build/.config
    make

.. warning::

   The PMU based scheme is useful for high accuracy performance profiling with
   ``rte_rdtsc()``. However, this method can not be used in conjunction with
   Linux userspace profiling tools like ``perf`` as this scheme alters the PMU
   registers state.
