..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

DPDK Telemetry API User Guide
==============================

This document describes how the Data Plane Development Kit(DPDK) Telemetry API
is used for querying port statistics from incoming traffic.

Introduction
------------

The ``librte_telemetry`` provides the functionality so that users may query
metrics from incoming port traffic. The application which initializes packet
forwarding will act as the server, sending metrics to the requesting application
which acts as the client.

In DPDK, applications are used to initialize the ``telemetry``. To view incoming
traffic on featured ports, the application should be run first (ie. after ports
are configured). Once the application is running, the service assurance agent
(for example the collectd plugin) should be run to begin querying the API.

A client connects their Service Assurance application to the DPDK application
via a UNIX socket. Once a connection is established, a client can send JSON
messages to the DPDK application requesting metrics via another UNIX client.
This request is then handled and parsed if valid. The response is then
formatted in JSON and sent back to the requesting client.

Pre-requisites
~~~~~~~~~~~~~~

* Python >= 2.5

* Jansson library for JSON serialization

Test Environment
----------------

``telemetry`` offers a range of selftests that a client can run within
the DPDK application.

Selftests are disabled by default. They can be enabled by setting the 'selftest'
variable to 1 in rte_telemetry_initial_accept().

Note: this 'hardcoded' value is temporary.

Configuration
-------------

Enable the telemetry API by modifying the following config option before
building DPDK::

        CONFIG_RTE_LIBRTE_TELEMETRY=y

Note: Meson will pick this up automatically if ``libjansson`` is available.

Running the Application
-----------------------

The following steps demonstrate how to run the ``telemetry`` API  to query all
statistics on all active ports, using the ``telemetry_client`` python script
to query.
Note: This guide assumes packet generation is applicable and the user is
testing with testpmd as a DPDK primary application to forward packets, although
any DPDK application is applicable.

#. Launch testpmd as the primary application with ``telemetry``.::

        ./app/testpmd --telemetry

#. Launch the ``telemetry`` python script with a client filepath.::

        python usertools/telemetry_client.py /var/run/some_client

   The client filepath is going to be used to setup our UNIX connection with the
   DPDK primary application, in this case ``testpmd``
   This will initialize a menu where a client can proceed to recursively query
   statistics, request statistics once or unregister the file_path, thus exiting
   the menu.

#. Send traffic to any or all available ports from a traffic generator.
   Select a query option(recursive or singular polling).
   The metrics will then be displayed on the client terminal in JSON format.

#. Once finished, unregister the client using the menu command.
