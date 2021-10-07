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


Creating Callback Functions
---------------------------


Function Type
~~~~~~~~~~~~~

When creating a callback function in a library/app, it must be of the following type:

.. code-block:: c

   typedef int (*telemetry_cb)(const char *cmd, const char *params,
           struct rte_tel_data *info);

An example callback function is shown below:

.. code-block:: c

   static int
   handle_example_cmd(const char *cmd __rte_unused, const char *params __rte_unused,
           struct rte_tel_data *d)

For more detail on the callback function parameters, please refer to the
`definition in the API doc
<https://doc.dpdk.org/api/rte__telemetry_8h.html#a41dc74d561442bb6184ee6dd1f9b5bcc>`_

**Example Callback**

This callback is an example of handling multiple commands in one callback,
and also shows the use of params which holds a port ID. The ``params`` input needs
to be validated and converted to the required integer type for port ID. The ``cmd``
parameter is then used in a comparison to decide which command was requested,
which will decide what port information should fill the ``rte_tel_data`` structure.

.. code-block:: c

   int
   handle_cmd_request(const char *cmd, const char *params,
         struct rte_tel_data *d)
   {
      int port_id, used = 0;

      if (params == NULL || strlen(params) == 0 || !isdigit(*params))
         return -1;

      port_id = atoi(params);
      if (!rte_eth_dev_is_valid_port(port_id))
         return -1;

      if (strcmp(cmd, "/cmd_1") == 0)
         /* Build up port data requested for command 1 */
      else
         /* Build up port data requested for command 2 */

       return used;
   }


Formatting Data
~~~~~~~~~~~~~~~

The callback function provided by the library must format its telemetry
information in the required data format. The Telemetry library provides a data
utilities API to build up the data structure with the required information.
The telemetry library is then responsible for formatting the data structure
into a JSON response before sending to the client.


Array Data
^^^^^^^^^^

Some data will need to be formatted in a list structure. For example, if a
callback needs to return five integer values in the data response, it can be
constructed using the following functions to build up the list:

.. code-block:: c

   rte_tel_data_start_array(d, RTE_TEL_INT_VAL);
       for(i = 0; i < 5; i++)
           rte_tel_data_add_array_int(d, i);

The resulting response to the client shows the list data provided above
by the handler function in the library/app, placed in a JSON reply by telemetry::

    {"/example_lib/five_ints": [0, 1, 2, 3, 4]}


Dictionary Data
^^^^^^^^^^^^^^^

For data that needs to be structured in a dictionary with key/value pairs,
the data utilities API can also be used. For example, some information about
a brownie recipe is constructed in the callback function shown below:

.. code-block:: c

   rte_tel_data_start_dict(d);
   rte_tel_data_add_dict_string(d, "Recipe", "Brownies");
   rte_tel_data_add_dict_int(d, "Prep time (mins)", 25);
   rte_tel_data_add_dict_int(d, "Cooking time (mins)", 30);
   rte_tel_data_add_dict_int(d, "Serves", 16);

The resulting response to the client shows the key/value data provided above
by the handler function in telemetry, placed in a JSON reply by telemetry::

    {"/example_lib/brownie_recipe": {"Recipe": "Brownies", "Prep time (mins)": 25,
      "Cooking time (mins)": 30, "Serves": 16}}


String Data
^^^^^^^^^^^

Telemetry also supports single string data.
The data utilities API can again be used for this, see the example below.

.. code-block:: c

   rte_tel_data_string(d, "This is an example string");

Giving the following response to the client::

    {"/example_lib/string_example": "This is an example string"}

For more information on the range of data functions available in the API,
please refer to the `API doc <https://doc.dpdk.org/api/rte__telemetry_8h.html>`_


Registering Commands
--------------------

Libraries and applications must register commands to make their information
available via the Telemetry library. This involves providing a string command
in the required format ("/library/command"), the callback function that
will handle formatting the information when required, and help text for the
command. An example command being registered is shown below:

.. code-block:: c

    rte_telemetry_register_cmd("/example_lib/string_example", handle_string,
            "Returns an example string. Takes no parameters");


Using Commands
--------------

To use commands, with a DPDK app running (e.g. testpmd), use the
``dpdk-telemetry.py`` script.
For details on its use, see the :doc:`../howto/telemetry`.
