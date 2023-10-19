.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Marvell.

dpdk-graph Application
======================

The ``dpdk-graph`` tool is a Data Plane Development Kit (DPDK)
application that allows exercising various graph use cases.
This application has a generic framework to add new graph based use cases
to verify functionality.
Each use case is defined as a ``<usecase>.cli`` file.
Based on the input file, application creates a graph to cater the use case.

Also this application framework can be used by other graph-based applications.


Running the Application
-----------------------

The application has a number of command line options
which can be provided in following syntax:

.. code-block:: console

   dpdk-graph [EAL Options] -- [application options]

EAL Options
~~~~~~~~~~~

Following are the EAL command-line options that can be used in conjunction
with the ``dpdk-graph`` application.
See the DPDK Getting Started Guides for more information on these options.

``-c <COREMASK>`` or ``-l <CORELIST>``

   Set the hexadecimal bit mask of the cores to run on.
   The CORELIST is a list of cores to be used.

Application Options
~~~~~~~~~~~~~~~~~~~

Following are the application command-line options:

``-s``

   Script name with absolute path which specifies the use case.
   It is a mandatory parameter which will be used
   to create desired graph for a given use case.

``--help``

   Dumps application usage.


Supported CLI commands
----------------------

This section provides details on commands which can be used in ``<usecase>.cli``
file to express the requested use case configuration.

.. table:: Exposed CLIs
   :widths: auto

   +--------------------------------------+-----------------------------------+---------+----------+
   |               Command                |             Description           | Dynamic | Optional |
   +======================================+===================================+=========+==========+
   |                                      |                                   |         |          |
   +--------------------------------------+-----------------------------------+---------+----------+


Runtime configuration
---------------------


Created graph for use case
--------------------------

On the successful execution of ``<usecase>.cli`` file, corresponding graph will be created.
This section mentions the created graph for each use case.
