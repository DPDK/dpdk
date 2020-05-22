..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Intel Corporation.

Telemetry Library
=================

The Telemetry library provides an interface to retrieve information from a
variety of DPDK libraries. The library provides this information via socket
connection, taking requests from a connected client and replying with the JSON
response containing the requested telemetry information.

Telemetry is enabled to run by default when running a DPDK application, and the
telemetry information from enabled libraries is made available. Libraries are
responsible for registering their own commands, and providing the callback
function that will format the library specific stats into the correct data
format, when requested.


Registering Commands
--------------------

Libraries and applications must register commands to make their information
available via the Telemetry library. This involves providing a string command
in the required format ("/library/command"), the callback function that
will handle formatting the information when required, and help text for the
command. An example showing ethdev commands being registered is shown below:

.. code-block:: c

    rte_telemetry_register_cmd("/ethdev/list", handle_port_list,
            "Returns list of available ethdev ports. Takes no parameters");
    rte_telemetry_register_cmd("/ethdev/xstats", handle_port_xstats,
            "Returns the extended stats for a port. Parameters: int port_id");
    rte_telemetry_register_cmd("/ethdev/link_status", handle_port_link_status,
            "Returns the link status for a port. Parameters: int port_id");


Formatting JSON response
------------------------

The callback function provided by the library must format its telemetry
information in the required data format. The Telemetry library provides a data
utilities API to build up the response. For example, the ethdev library provides a
list of available ethdev ports in a formatted data response, constructed using the
following functions to build up the list:

.. code-block:: c

    rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
        RTE_ETH_FOREACH_DEV(port_id)
            rte_tel_data_add_array_int(d, port_id);

The data structure is then formatted into a JSON response before sending.
The resulting response shows the port list data provided above by the handler
function in ethdev, placed in a JSON reply by telemetry:

.. code-block:: console

    {"/ethdev/list": [0, 1]}

For more information on the range of data functions available in the API,
please refer to the docs.
