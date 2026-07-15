..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.


DPDK Telemetry User Guide
=========================

The Telemetry library provides users with the ability to query DPDK for
telemetry information, currently including information such as ethdev stats,
ethdev port list, and eal parameters.


Telemetry Interface
-------------------

The :doc:`/prog_guide/telemetry_lib` opens a socket with path
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

   * Get the help text for a command.
     This will indicate what parameters are required.
     Use the ``help`` keyword followed by the command or keyword of interest,
     for example::

       --> help FOREACH
       FOREACH usage:
          FOREACH /<list_cmd> /<iter_cmd> .<field> [.<field> ...]
          FOREACH <var> /<list_cmd> /<iter_cmd_with_$var> .<field> [.<field> ...]

       Examples:
          FOREACH /ethdev/list /ethdev/stats .opackets
          ...

       --> help /ethdev/xstats
       {"/help": {"/ethdev/xstats": "Returns the extended stats for a port. Parameters: int port_id"}}

   ..  note::

       For commands starting with ``/`` that are telemetry endpoints,
       the help text can also be obtained by sending the ``/help`` command to the telemetry socket.
       In this case, the parameter must be separated by a comma, not a space.
       For example::

          --> /help,/ethdev/xstats
          {"/help": {"/ethdev/xstats": "Returns the extended stats for a port. Parameters: int port_id"}}

   * Run a compound query using ``FOREACH``.

     The ``FOREACH`` command runs a list command, iterates each returned item,
     runs a second command for each item, and emits combined JSON output.

     Start with the simplest form (no loop variable)::

        FOREACH /<list_cmd> /<iter_cmd> .<field> [.<field> ...]

     To include numbered output, use a loop variable::

        FOREACH <var> /<list_cmd> /<iter_cmd_with_$var> .<field> [.<field> ...]

     Notes:

     - Field selectors are whitespace-separated tokens, each starting with ``.``.
     - In no-variable mode, the iter command is called as ``/<iter_cmd>,<item>``.
     - In loop-variable mode, use ``$<var>`` in the iter command
       where the item value should be substituted.

     Examples::

        --> FOREACH /ethdev/list /ethdev/stats .opackets
        [0, 0]

        --> FOREACH /ethdev/list /ethdev/stats .ipackets .opackets
        [{"ipackets": 0, "opackets": 0}, {"ipackets": 0, "opackets": 0}]

        --> FOREACH i /ethdev/list /ethdev/info,$i .name
        [{"i": 0, "name": "0000:16:00.0"}, {"i": 1, "name": "0000:16:00.1"}]

        --> FOREACH i /ethdev/list /ethdev/stats,$i .ipackets .opackets
        [{"i": 0, "ipackets": 0, "opackets": 0}, {"i": 1, "ipackets": 0, "opackets": 0}]

     Output behavior:

     - Without loop variable and one field: returns an array of values.
     - Without loop variable and multiple fields: returns an array of objects
       containing named value fields.
     - With loop variable: returns an array of objects
       containing the loop variable field and requested value fields.

   * Use command aliases.

     The telemetry script can load aliases at startup from::

        $XDG_CONFIG_HOME/dpdk/telemetry_aliases

     If ``XDG_CONFIG_HOME`` is not set, ``$HOME/.config/dpdk/telemetry_aliases`` is used.
     A custom path can also be provided via the ``--alias-file`` script flag.

     Each alias entry must be in ``alias=command`` format.
     Empty lines and lines starting with ``#`` are ignored.

     Example alias file::

        # Basic shortcuts
        ls=/ethdev/list
        names=FOREACH i /ethdev/list /ethdev/info,$i .name
        q=quit

     Alias behavior is intentionally similar to shell aliases:

     - The first token of the entered input is checked for an alias match.
     - If matched, that first token is replaced with its expansion.
     - Alias expansion is recursive (aliases can expand to other aliases).
     - Expansion has a safety limit to prevent infinite loops.

     Examples::

        --> ls
        {"/ethdev/list": [0, 1]}

        --> names
        [{"i": 0, "name": "0000:16:00.0"}, {"i": 1, "name": "0000:16:00.1"}]

        --> q
        # exits the client


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
