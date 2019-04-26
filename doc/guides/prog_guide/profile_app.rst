..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

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


Profiling with VTune
~~~~~~~~~~~~~~~~~~~~

To allow VTune attaching to the DPDK application, reconfigure and recompile
the DPDK with ``CONFIG_RTE_ETHDEV_RXTX_CALLBACKS`` and
``CONFIG_RTE_ETHDEV_PROFILE_WITH_VTUNE`` enabled.


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
clock counter is through the ARMv8 PMU subsystem. The PMU cycle counter runs
at CPU frequency. However, access to the PMU cycle counter from user space is
not enabled by default in the arm64 linux kernel. It is possible to enable
cycle counter for user space access by configuring the PMU from the privileged
mode (kernel space).

By default the ``rte_rdtsc()`` implementation uses a portable ``cntvct_el0``
scheme.  Application can choose the PMU based implementation with
``CONFIG_RTE_ARM_EAL_RDTSC_USE_PMU``.

The example below shows the steps to configure the PMU based cycle counter on
an ARMv8 machine.

.. code-block:: console

    git clone https://github.com/jerinjacobk/armv8_pmu_cycle_counter_el0
    cd armv8_pmu_cycle_counter_el0
    make
    sudo insmod pmu_el0_cycle_counter.ko
    cd $DPDK_DIR
    make config T=arm64-armv8a-linux-gcc
    echo "CONFIG_RTE_ARM_EAL_RDTSC_USE_PMU=y" >> build/.config
    make

.. warning::

   The PMU based scheme is useful for high accuracy performance profiling with
   ``rte_rdtsc()``. However, this method can not be used in conjunction with
   Linux userspace profiling tools like ``perf`` as this scheme alters the PMU
   registers state.
