..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

EAL parameters
==============

This document contains a list of all EAL parameters. These parameters can be
used by any DPDK application running on FreeBSD.

Common EAL parameters
---------------------

The following EAL parameters are common to all platforms supported by DPDK.

.. include:: ../linux_gsg/eal_args.include.rst

FreeBSD-specific EAL parameters
-------------------------------

There are currently no FreeBSD-specific EAL command-line parameters available.

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
