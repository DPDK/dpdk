..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

EAL parameters
==============

This document contains a list of all EAL parameters. These parameters can be
used by any DPDK application running on Linux.

Common EAL parameters
---------------------

The following EAL parameters are common to all platforms supported by DPDK.

.. include:: eal_args.include.rst

Linux-specific EAL parameters
-----------------------------

In addition to common EAL parameters, there are also Linux-specific EAL
parameters.

Device-related options
~~~~~~~~~~~~~~~~~~~~~~

*   ``--create-uio-dev``

    Create ``/dev/uioX`` files for devices bound to igb_uio kernel driver
    (usually done by the igb_uio driver itself).

*   ``--vmware-tsc-map``

    Use VMware TSC map instead of native RDTSC.

*   ``--no-hpet``

    Do not use the HPET timer.

*   ``--vfio-intr <legacy|msi|msix>``

    Use specified interrupt mode for devices bound to VFIO kernel driver.

*   ``--vfio-vf-token <uuid>``

    Use specified VF token for devices bound to VFIO kernel driver.

Multiprocessing-related options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*   ``--file-prefix <prefix name>``

    Use a different shared data file prefix for a DPDK process. This option
    allows running multiple independent DPDK primary/secondary processes under
    different prefixes.

Memory-related options
~~~~~~~~~~~~~~~~~~~~~~

*   ``--legacy-mem``

    Use legacy DPDK memory allocation mode.

*   ``--socket-mem <amounts of memory per socket>``

    Preallocate specified amounts of memory per socket. The parameter is a
    comma-separated list of values. For example::

        --socket-mem 1024,2048

    This will allocate 1 gigabyte of memory on socket 0, and 2048 megabytes of
    memory on socket 1.

*   ``--socket-limit <amounts of memory per socket>``

    Place a per-socket upper limit on memory use (non-legacy memory mode only).
    0 will disable the limit for a particular socket.

*   ``--single-file-segments``

    Create fewer files in hugetlbfs (non-legacy mode only).

*   ``--huge-dir <path to hugetlbfs directory>``

    Use specified hugetlbfs directory instead of autodetected ones. This can be
    a sub-directory within a hugetlbfs mountpoint.

*   ``--huge-unlink``

    Unlink hugepage files after creating them (implies no secondary process
    support).

*   ``--match-allocations``

    Free hugepages back to system exactly as they were originally allocated.

Other options
~~~~~~~~~~~~~

*   ``--syslog <syslog facility>``

    Set syslog facility. Valid syslog facilities are::

        auth
        cron
        daemon
        ftp
        kern
        lpr
        mail
        news
        syslog
        user
        uucp
        local0
        local1
        local2
        local3
        local4
        local5
        local6
        local7
