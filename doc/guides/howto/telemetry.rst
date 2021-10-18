..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.


DPDK Telemetry User Guide
=========================

The Telemetry library provides users with the ability to query DPDK for
telemetry information, currently including information such as ethdev stats,
ethdev port list, and eal parameters.


Telemetry Interface
-------------------

The :doc:`../prog_guide/telemetry_lib` opens a socket with path
*<runtime_directory>/dpdk_telemetry.<version>*. The version represents the
telemetry version, the latest is v2. For example, a client would connect to a
socket with path  */var/run/dpdk/\*/dpdk_telemetry.v2* (when the primary process
is run by a root user).


Telemetry Initialization
------------------------

The library is enabled by default, however an EAL flag to enable the library
exists, to provide backward compatibility for the previous telemetry library
interface::

  --telemetry

A flag exists to disable Telemetry also::

  --no-telemetry


Running Telemetry
-----------------

The following steps show how to run an application with telemetry support,
and query information using the telemetry client python script.

#. Launch testpmd as the primary application with telemetry::

      ./app/dpdk-testpmd

#. Launch the telemetry client script::

      ./usertools/dpdk-telemetry.py

#. When connected, the script displays the following, waiting for user input::

     Connecting to /var/run/dpdk/rte/dpdk_telemetry.v2
     {"version": "DPDK 20.05.0-rc2", "pid": 60285, "max_output_len": 16384}
     -->

#. The user can now input commands to send across the socket, and receive the
   response. Some available commands are shown below.

   * List all commands::

       --> /
       {"/": ["/", "/eal/app_params", "/eal/params", "/ethdev/list",
       "/ethdev/link_status", "/ethdev/xstats", "/help", "/info"]}

   * Get the list of ethdev ports::

       --> /ethdev/list
       {"/ethdev/list": [0, 1]}

   .. Note::

      For commands that expect a parameter, use "," to separate the command
      and parameter. See examples below.

   * Get extended statistics for an ethdev port::

       --> /ethdev/xstats,0
       {"/ethdev/xstats": {"rx_good_packets": 0, "tx_good_packets": 0,
       "rx_good_bytes": 0, "tx_good_bytes": 0, "rx_missed_errors": 0,
       ...
       "tx_priority7_xon_to_xoff_packets": 0}}

   * Get the help text for a command. This will indicate what parameters are
     required. Pass the command as a parameter::

       --> /help,/ethdev/xstats
       {"/help": {"/ethdev/xstats": "Returns the extended stats for a port.
       Parameters: int port_id"}}


Connecting to Different DPDK Processes
--------------------------------------

When multiple DPDK process instances are running on a system, the user will
naturally wish to be able to select the instance to which the connection is
being made. The method to select the instance depends on how the individual
instances are run:

* For DPDK processes run using a non-default file-prefix,
  i.e. using the `--file-prefix` EAL option flag,
  the file-prefix for the process should be passed via the `-f` or `--file-prefix` script flag.

  For example, to connect to testpmd run as::

     $ ./build/app/dpdk-testpmd -l 2,3 --file-prefix="tpmd"

  One would use the telemetry script command::

     $ ./usertools/dpdk-telemetry -f "tpmd"

  To list all running telemetry-enabled file-prefixes, the ``-l`` or ``--list`` flags can be used::

     $ ./usertools/dpdk-telemetry -l

* For the case where multiple processes are run using the `--in-memory` EAL flag,
  but no `--file-prefix` flag, or the same `--file-prefix` flag,
  those processes will all share the same runtime directory.
  In this case,
  each process after the first will add an increasing count suffix to the telemetry socket name,
  with each one taking the first available free socket name.
  This suffix count can be passed to the telemetry script using the `-i` or `--instance` flag.

  For example, if the following two applications are run in separate terminals::

     $ ./build/app/dpdk-testpmd -l 2,3 --in-memory    # will use socket "dpdk_telemetry.v2"

     $ ./build/app/test/dpdk-test -l 4,5 --in-memory  # will use "dpdk_telemetry.v2:1"

  The following telemetry script commands would allow one to connect to each binary::

     $ ./usertools/dpdk-telemetry.py       # will connect to testpmd

     $ ./usertools/dpdk-telemetry.py -i 1  # will connect to test binary
