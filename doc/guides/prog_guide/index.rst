..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2017 Intel Corporation.

Programmer's Guide
==================

Foundation Principles
---------------------

.. toctree::
    :maxdepth: 1
    :numbered:

    intro
    overview
    source_org
    glossary


Memory Management
-----------------

.. toctree::
    :maxdepth: 1
    :numbered:

    lcore_var
    mempool_lib
    mbuf_lib
    multi_proc_support


CPU Management
--------------

.. toctree::
    :maxdepth: 1
    :numbered:

    env_abstraction_layer
    power_man
    thread_safety
    service_cores


CPU Packet Processing
---------------------

.. toctree::
    :maxdepth: 1
    :numbered:

    toeplitz_hash_lib
    hash_lib
    member_lib
    ip_fragment_reassembly_lib
    generic_receive_offload_lib
    generic_segmentation_offload_lib
    packet_classif_access_ctrl
    packet_distrib_lib
    efd_lib
    reorder_lib
    lpm_lib
    lpm6_lib
    rib_lib
    fib_lib


Device Libraries
----------------

.. toctree::
    :maxdepth: 1
    :numbered:

    ethdev/index
    link_bonding_poll_mode_drv_lib
    vhost_lib
    cryptodev_lib
    rte_security
    compressdev
    regexdev
    bbdev
    mldev
    dmadev
    gpudev
    rawdev
    eventdev/index


Protocol Processing Libraries
-----------------------------

.. toctree::
    :maxdepth: 1
    :numbered:

    pdcp_lib
    ipsec_lib


High-Level Libraries
--------------------

.. toctree::
    :maxdepth: 1
    :numbered:

    packet_framework
    graph_lib


Utility Libraries
-----------------

.. toctree::
    :maxdepth: 1
    :numbered:

    argparse_lib
    cmdline
    ptr_compress_lib
    timer_lib
    rcu_lib
    ring_lib
    stack_lib
    log_lib
    metrics_lib
    telemetry_lib
    pdump_lib
    pcapng_lib
    bpf_lib
    trace_lib


Howto Guides
-------------

.. toctree::
    :maxdepth: 1
    :numbered:

    build-sdk-meson
    meson_ut
    build_app


Tips & Tricks
-------------

.. toctree::
    :maxdepth: 1
    :numbered:

    perf_opt_guidelines
    writing_efficient_code
    lto
    profile_app
    asan
