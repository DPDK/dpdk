..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

* Call rte_eal_vdev_init("crpyto_scheduler") within the application.

* Use --vdev="crpyto_scheduler" in the EAL options, which will call
  rte_eal_vdev_init() internally.


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

Example:

.. code-block:: console

    ... --vdev "crypto_aesni_mb_pmd,name=aesni_mb_1" --vdev "crypto_aesni_mb_pmd,name=aesni_mb_2" --vdev "crypto_scheduler_pmd,slave=aesni_mb_1,slave=aesni_mb_2" ...

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

   Round-robin mode, which distributes the enqueued burst of crypto ops
   among its slaves in a round-robin manner. This mode may help to fill
   the throughput gap between the physical core and the existing cryptodevs
   to increase the overall performance.
