..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Introduction
============

This document contains instructions for installing and configuring the
Data Plane Development Kit (DPDK) software. It is designed to get customers
up and running quickly and describes how to compile and run a
DPDK application in a FreeBSD application (freebsd) environment, without going
deeply into detail.

For a comprehensive guide to installing and using FreeBSD, the following
handbook is available from the FreeBSD Documentation Project:
`FreeBSD Handbook <http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook/index.html>`_.

.. note::

   DPDK is now available as part of the FreeBSD ports collection and as a pre-built package.
   Installing via the ports collection or FreeBSD `pkg` infrastructure is now the recommended
   way to install DPDK on FreeBSD, and is documented in the next chapter, :ref:`install_from_ports`.

Documentation Roadmap
---------------------

The following is a list of DPDK documents in the suggested reading order:

*   :doc:`../rel_notes/index`: Provides release-specific information, including supported
    features, limitations, fixed issues, known issues and so on.  Also, provides the
    answers to frequently asked questions in FAQ format.

*   :doc:`../freebsd_gsg/index` (this document): Describes how to install and
    configure the DPDK; designed to get users up and running quickly with the
    software.

*   :doc:`../prog_guide/index`: Describes:

    *   The software architecture and how to use it (through examples),
        specifically in a Linux* application (linux) environment

    *   The content of the DPDK, the build system (including the commands
        that can be used to build the development kit and an application)
        and guidelines for porting an application

    *   Optimizations used in the software and those that should be considered
        for new development

    A glossary of terms is also provided.

*   `API Reference <../../../api/html/index.html>`_: Provides detailed information about DPDK functions,
    data structures and other programming constructs.

*   :doc:`../sample_app_ug/index`: Describes a set of sample applications.
    Each chapter describes a sample application that showcases specific functionality
    and provides instructions on how to compile, run and use the sample application.

*   **Driver Reference Guides**: Provides details on each driver inside a particular category.
    Separate guides exist for each of:

    * :doc:`../bbdevs/index`

    * :doc:`../compressdevs/index`

    * :doc:`../cryptodevs/index`

    * :doc:`../dmadevs/index`

    * :doc:`../eventdevs/index`

    * :doc:`../gpus/index`

    * :doc:`../mempool/index`

    * :doc:`../nics/index`

    * :doc:`../rawdevs/index`

    * :doc:`../regexdevs/index`

    * :doc:`../vdpadevs/index`
