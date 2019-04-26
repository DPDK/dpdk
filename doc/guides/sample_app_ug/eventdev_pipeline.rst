..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Eventdev Pipeline Sample Application
====================================

The eventdev pipeline sample application is a sample app that demonstrates
the usage of the eventdev API using the software PMD. It shows how an
application can configure a pipeline and assign a set of worker cores to
perform the processing required.

The application has a range of command line arguments allowing it to be
configured for various numbers worker cores, stages,queue depths and cycles per
stage of work. This is useful for performance testing as well as quickly testing
a particular pipeline configuration.


Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``examples`` sub-directory.



Running the Application
-----------------------

The application has a lot of command line options. This allows specification of
the eventdev PMD to use, and a number of attributes of the processing pipeline
options.

An example eventdev pipeline running with the software eventdev PMD using
these settings is shown below:

 * ``-r1``: core mask 0x1 for RX
 * ``-t1``: core mask 0x1 for TX
 * ``-e4``: core mask 0x4 for the software scheduler
 * ``-w FF00``: core mask for worker cores, 8 cores from 8th to 16th
 * ``-s4``: 4 atomic stages
 * ``-n0``: process infinite packets (run forever)
 * ``-c32``: worker dequeue depth of 32
 * ``-W1000``: do 1000 cycles of work per packet in each stage
 * ``-D``: dump statistics on exit

.. code-block:: console

    ./build/eventdev_pipeline --vdev event_sw0 -- -r1 -t1 -e4 -w FF00 -s4 -n0 -c32 -W1000 -D

The application has some sanity checking built-in, so if there is a function
(e.g.; the RX core) which doesn't have a cpu core mask assigned, the application
will print an error message:

.. code-block:: console

  Core part of pipeline was not assigned any cores. This will stall the
  pipeline, please check core masks (use -h for details on setting core masks):
          rx: 0
          tx: 1

Configuration of the eventdev is covered in detail in the programmers guide,
see the Event Device Library section.


Observing the Application
-------------------------

At runtime the eventdev pipeline application prints out a summary of the
configuration, and some runtime statistics like packets per second. On exit the
worker statistics are printed, along with a full dump of the PMD statistics if
required. The following sections show sample output for each of the output
types.

Configuration
~~~~~~~~~~~~~

This provides an overview of the pipeline,
scheduling type at each stage, and parameters to options such as how many
flows to use and what eventdev PMD is in use. See the following sample output
for details:

.. code-block:: console

  Config:
        ports: 2
        workers: 8
        packets: 0
        priorities: 1
        Queue-prio: 0
        qid0 type: atomic
        Cores available: 44
        Cores used: 10
        Eventdev 0: event_sw
  Stages:
        Stage 0, Type Atomic    Priority = 128
        Stage 1, Type Atomic    Priority = 128
        Stage 2, Type Atomic    Priority = 128
        Stage 3, Type Atomic    Priority = 128

Runtime
~~~~~~~

At runtime, the statistics of the consumer are printed, stating the number of
packets received, runtime in milliseconds, average mpps, and current mpps.

.. code-block:: console

  # consumer RX= xxxxxxx, time yyyy ms, avg z.zzz mpps [current w.www mpps]

Shutdown
~~~~~~~~

At shutdown, the application prints the number of packets received and
transmitted, and an overview of the distribution of work across worker cores.

.. code-block:: console

        Signal 2 received, preparing to exit...
          worker 12 thread done. RX=4966581 TX=4966581
          worker 13 thread done. RX=4963329 TX=4963329
          worker 14 thread done. RX=4953614 TX=4953614
          worker 0 thread done. RX=0 TX=0
          worker 11 thread done. RX=4970549 TX=4970549
          worker 10 thread done. RX=4986391 TX=4986391
          worker 9 thread done. RX=4970528 TX=4970528
          worker 15 thread done. RX=4974087 TX=4974087
          worker 8 thread done. RX=4979908 TX=4979908
          worker 2 thread done. RX=0 TX=0

        Port Workload distribution:
        worker 0 :      12.5 % (4979876 pkts)
        worker 1 :      12.5 % (4970497 pkts)
        worker 2 :      12.5 % (4986359 pkts)
        worker 3 :      12.5 % (4970517 pkts)
        worker 4 :      12.5 % (4966566 pkts)
        worker 5 :      12.5 % (4963297 pkts)
        worker 6 :      12.5 % (4953598 pkts)
        worker 7 :      12.5 % (4974055 pkts)

To get a full dump of the state of the eventdev PMD, pass the ``-D`` flag to
this application. When the app is terminated using ``Ctrl+C``, the
``rte_event_dev_dump()`` function is called, resulting in a dump of the
statistics that the PMD provides. The statistics provided depend on the PMD
used, see the Event Device Drivers section for a list of eventdev PMDs.
