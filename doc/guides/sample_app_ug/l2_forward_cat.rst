..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

L2 Forwarding Sample Application with Cache Allocation Technology (CAT)
=======================================================================

Basic Forwarding sample application is a simple *skeleton* example of
a forwarding application. It has been extended to make use of CAT via extended
command line options and linking against the libpqos library.

It is intended as a demonstration of the basic components of a DPDK forwarding
application and use of the libpqos library to program CAT.
For more detailed implementations see the L2 and L3 forwarding
sample applications.

CAT and Code Data Prioritization (CDP) features allow management of the CPU's
last level cache. CAT introduces classes of service (COS) that are essentially
bitmasks. In current CAT implementations, a bit in a COS bitmask corresponds to
one cache way in last level cache.
A CPU core is always assigned to one of the CAT classes.
By programming CPU core assignment and COS bitmasks, applications can be given
exclusive, shared, or mixed access to the CPU's last level cache.
CDP extends CAT so that there are two bitmasks per COS,
one for data and one for code.
The number of classes and number of valid bits in a COS bitmask is CPU model
specific and COS bitmasks need to be contiguous. Sample code calls this bitmask
``cbm`` or capacity bitmask.
By default, after reset, all CPU cores are assigned to COS 0 and all classes
are programmed to allow fill into all cache ways.
CDP is off by default.

For more information about CAT please see:

* https://github.com/01org/intel-cmt-cat

White paper demonstrating example use case:

* `Increasing Platform Determinism with Platform Quality of Service for the Data Plane Development Kit <http://www.intel.com/content/www/us/en/communications/increasing-platform-determinism-pqos-dpdk-white-paper.html>`_

Compiling the Application
-------------------------
.. note::

    Requires ``libpqos`` from Intel's
    `intel-cmt-cat software package <https://github.com/01org/intel-cmt-cat>`_
    hosted on GitHub repository. For installation notes, please see ``README`` file.

    GIT:

    * https://github.com/01org/intel-cmt-cat


#. To compile the application export the path to PQoS lib
   and the DPDK source tree and go to the example directory:

   .. code-block:: console

       export PQOS_INSTALL_PATH=/path/to/libpqos


To compile the sample application see :doc:`compiling`.

The application is located in the ``l2fwd-cat`` sub-directory.


Running the Application
-----------------------

To run the example in a ``linux`` environment and enable CAT on cpus 0-2:

.. code-block:: console

    ./build/l2fwd-cat -l 1 -n 4 -- --l3ca="0x3@(0-2)"

or to enable CAT and CDP on cpus 1,3:

.. code-block:: console

    ./build/l2fwd-cat -l 1 -n 4 -- --l3ca="(0x00C00,0x00300)@(1,3)"

If CDP is not supported it will fail with following error message:

.. code-block:: console

    PQOS: CDP requested but not supported.
    PQOS: Requested CAT configuration is not valid!
    PQOS: Shutting down PQoS library...
    EAL: Error - exiting with code: 1
      Cause: PQOS: L3CA init failed!

The option to enable CAT is:

* ``--l3ca='<common_cbm@cpus>[,<(code_cbm,data_cbm)@cpus>...]'``:

  where ``cbm`` stands for capacity bitmask and must be expressed in
  hexadecimal form.

  ``common_cbm`` is a single mask, for a CDP enabled system, a group of two
  masks (``code_cbm`` and ``data_cbm``) is used.

  ``(`` and ``)`` are necessary if it's a group.

  ``cpus`` could be a single digit/range or a group and must be expressed in
  decimal form.

  ``(`` and ``)`` are necessary if it's a group.

  e.g. ``--l3ca='0x00F00@(1,3),0x0FF00@(4-6),0xF0000@7'``

  * cpus 1 and 3 share its 4 ways with cpus 4, 5 and 6;

  * cpus 4, 5 and 6 share half (4 out of 8 ways) of its L3 with cpus 1 and 3;

  * cpus 4, 5 and 6 have exclusive access to 4 out of 8 ways;

  * cpu 7 has exclusive access to all of its 4 ways;

  e.g. ``--l3ca='(0x00C00,0x00300)@(1,3)'`` for CDP enabled system

  * cpus 1 and 3 have access to 2 ways for code and 2 ways for data, code and
    data ways are not overlapping.


Refer to *DPDK Getting Started Guide* for general information on running
applications and the Environment Abstraction Layer (EAL) options.


To reset or list CAT configuration and control CDP please use ``pqos`` tool
from Intel's
`intel-cmt-cat software package <https://github.com/01org/intel-cmt-cat>`_.

To enabled or disable CDP:

.. code-block:: console

    sudo ./pqos -S cdp-on

    sudo ./pqos -S cdp-off

to reset CAT configuration:

.. code-block:: console

    sudo ./pqos -R

to list CAT config:

.. code-block:: console

    sudo ./pqos -s

For more info about ``pqos`` tool please see its man page or
`intel-cmt-cat wiki <https://github.com/01org/intel-cmt-cat/wiki>`_.


Explanation
-----------

The following sections provide an explanation of the main components of the
code.

All DPDK library functions used in the sample code are prefixed with ``rte_``
and are explained in detail in the *DPDK API Documentation*.


The Main Function
~~~~~~~~~~~~~~~~~

The ``main()`` function performs the initialization and calls the execution
threads for each lcore.

The first task is to initialize the Environment Abstraction Layer (EAL).  The
``argc`` and ``argv`` arguments are provided to the ``rte_eal_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

The next task is to initialize the PQoS library and configure CAT. The
``argc`` and ``argv`` arguments are provided to the ``cat_init()``
function. The value returned is the number of parsed arguments:

.. code-block:: c

    int ret = cat_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "PQOS: L3CA init failed!\n");

``cat_init()`` is a wrapper function which parses the command, validates
the requested parameters and configures CAT accordingly.

Parsing of command line arguments is done in ``parse_args(...)``.
libpqos is then initialized with the ``pqos_init(...)`` call. Next, libpqos is
queried for system CPU information and L3CA capabilities via
``pqos_cap_get(...)`` and ``pqos_cap_get_type(..., PQOS_CAP_TYPE_L3CA, ...)``
calls. When all capability and topology information is collected, the requested
CAT configuration is validated. A check is then performed (on per socket basis)
for a sufficient number of un-associated COS. COS are selected and
configured via the ``pqos_l3ca_set(...)`` call. Finally, COS are associated to
relevant CPUs via ``pqos_l3ca_assoc_set(...)`` calls.

``atexit(...)`` is used to register ``cat_exit(...)`` to be called on
a clean exit. ``cat_exit(...)`` performs a simple CAT clean-up, by associating
COS 0 to all involved CPUs via ``pqos_l3ca_assoc_set(...)`` calls.
