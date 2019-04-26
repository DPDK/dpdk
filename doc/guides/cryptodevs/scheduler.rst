..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Cryptodev Scheduler Poll Mode Driver Library
============================================

Scheduler PMD is a software crypto PMD, which has the capabilities of
attaching hardware and/or software cryptodevs, and distributes ingress
crypto ops among them in a certain manner.

.. figure:: img/scheduler-overview.*

   Cryptodev Scheduler Overview


The Cryptodev Scheduler PMD library (**librte_pmd_crypto_scheduler**) acts as
a software crypto PMD and shares the same API provided by librte_cryptodev.
The PMD supports attaching multiple crypto PMDs, software or hardware, as
slaves, and distributes the crypto workload to them with certain behavior.
The behaviors are categorizes as different "modes". Basically, a scheduling
mode defines certain actions for scheduling crypto ops to its slaves.

The librte_pmd_crypto_scheduler library exports a C API which provides an API
for attaching/detaching slaves, set/get scheduling modes, and enable/disable
crypto ops reordering.

Limitations
-----------

* Sessionless crypto operation is not supported
* OOP crypto operation is not supported when the crypto op reordering feature
  is enabled.


Installation
------------

To build DPDK with CRYTPO_SCHEDULER_PMD the user is required to set
CONFIG_RTE_LIBRTE_PMD_CRYPTO_SCHEDULER=y in config/common_base, and
recompile DPDK


Initialization
--------------

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_scheduler") within the application.

* Use --vdev="crypto_scheduler" in the EAL options, which will call
  rte_vdev_init() internally.


The following parameters (all optional) can be provided in the previous
two calls:

* socket_id: Specify the socket where the memory for the device is going
  to be allocated (by default, socket_id will be the socket where the core
  that is creating the PMD is running on).

* max_nb_sessions: Specify the maximum number of sessions that can be
  created. This value may be overwritten internally if there are too
  many devices are attached.

* slave: If a cryptodev has been initialized with specific name, it can be
  attached to the scheduler using this parameter, simply filling the name
  here. Multiple cryptodevs can be attached initially by presenting this
  parameter multiple times.

* mode: Specify the scheduling mode of the PMD. The supported scheduling
  mode parameter values are specified in the "Cryptodev Scheduler Modes
  Overview" section.

* mode_param: Specify the mode-specific parameter. Some scheduling modes
  may be initialized with specific parameters other than the default ones,
  such as the **threshold** packet size of **packet-size-distr** mode. This
  parameter fulfills the purpose.

* ordering: Specify the status of the crypto operations ordering feature.
  The value of this parameter can be "enable" or "disable". This feature
  is disabled by default.

Example:

.. code-block:: console

    ... --vdev "crypto_aesni_mb0,name=aesni_mb_1" --vdev "crypto_aesni_mb1,name=aesni_mb_2" --vdev "crypto_scheduler,slave=aesni_mb_1,slave=aesni_mb_2" ...

.. note::

    * The scheduler cryptodev cannot be started unless the scheduling mode
      is set and at least one slave is attached. Also, to configure the
      scheduler in the run-time, like attach/detach slave(s), change
      scheduling mode, or enable/disable crypto op ordering, one should stop
      the scheduler first, otherwise an error will be returned.

    * The crypto op reordering feature requires using the userdata field of
      every mbuf to be processed to store temporary data. By the end of
      processing, the field is set to pointing to NULL, any previously
      stored value of this field will be lost.


Cryptodev Scheduler Modes Overview
----------------------------------

Currently the Crypto Scheduler PMD library supports following modes of
operation:

*   **CDEV_SCHED_MODE_ROUNDROBIN:**

   *Initialization mode parameter*: **round-robin**

   Round-robin mode, which distributes the enqueued burst of crypto ops
   among its slaves in a round-robin manner. This mode may help to fill
   the throughput gap between the physical core and the existing cryptodevs
   to increase the overall performance.

*   **CDEV_SCHED_MODE_PKT_SIZE_DISTR:**

   *Initialization mode parameter*: **packet-size-distr**

   Packet-size based distribution mode, which works with 2 slaves, the primary
   slave and the secondary slave, and distributes the enqueued crypto
   operations to them based on their data lengths. A crypto operation will be
   distributed to the primary slave if its data length is equal to or bigger
   than the designated threshold, otherwise it will be handled by the secondary
   slave.

   A typical usecase in this mode is with the QAT cryptodev as the primary and
   a software cryptodev as the secondary slave. This may help applications to
   process additional crypto workload than what the QAT cryptodev can handle on
   its own, by making use of the available CPU cycles to deal with smaller
   crypto workloads.

   The threshold is set to 128 bytes by default. It can be updated by calling
   function **rte_cryptodev_scheduler_option_set**. The parameter of
   **option_type** must be **CDEV_SCHED_OPTION_THRESHOLD** and **option** should
   point to a rte_cryptodev_scheduler_threshold_option structure filled with
   appropriate threshold value. Please NOTE this threshold has be a power-of-2
   unsigned integer. It is possible to use **mode_param** initialization
   parameter to achieve the same purpose. For example:

   ... --vdev "crypto_scheduler,mode=packet-size-distr,mode_param=threshold:512" ...

   The above parameter will overwrite the threshold value to 512.

*   **CDEV_SCHED_MODE_FAILOVER:**

   *Initialization mode parameter*: **fail-over**

   Fail-over mode, which works with 2 slaves, the primary slave and the
   secondary slave. In this mode, the scheduler will enqueue the incoming
   crypto operation burst to the primary slave. When one or more crypto
   operations fail to be enqueued, then they will be enqueued to the secondary
   slave.

*   **CDEV_SCHED_MODE_MULTICORE:**

   *Initialization mode parameter*: **multi-core**

   Multi-core mode, which distributes the workload with several (up to eight)
   worker cores. The enqueued bursts are distributed among the worker cores in a
   round-robin manner. If scheduler cannot enqueue entire burst to the same worker,
   it will enqueue the remaining operations to the next available worker.
   For pure small packet size (64 bytes) traffic however the multi-core mode is not
   an optimal solution, as it doesn't give significant per-core performance improvement.
   For mixed traffic (IMIX) the optimal number of worker cores is around 2-3.
   For large packets (1.5 kbytes) scheduler shows linear scaling in performance
   up to eight cores.
   Each worker uses its own slave cryptodev. Only software cryptodevs
   are supported. Only the same type of cryptodevs should be used concurrently.

   The multi-core mode uses one extra parameter:

   * corelist: Semicolon-separated list of logical cores to be used as workers.
     The number of worker cores should be equal to the number of slave cryptodevs.
     These cores should be present in EAL core list parameter and
     should not be used by the application or any other process.

   Example:
    ... --vdev "crypto_aesni_mb1,name=aesni_mb_1" --vdev "crypto_aesni_mb_pmd2,name=aesni_mb_2" \
    --vdev "crypto_scheduler,slave=aesni_mb_1,slave=aesni_mb_2,mode=multi-core,corelist=23;24" ...
