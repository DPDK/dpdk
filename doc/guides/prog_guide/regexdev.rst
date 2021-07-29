.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

RegEx Device Library
====================

The RegEx library provides a RegEx device framework for management and
provisioning of hardware and software RegEx poll mode drivers, defining generic
APIs which support a number of different RegEx operations.


Design Principles
-----------------

The RegEx library follows the same basic principles as those used in DPDK's
Ethernet Device framework and the Crypto framework. The RegEx framework provides
a generic Crypto device framework which supports both physical (hardware)
and virtual (software) RegEx devices as well as a generic RegEx API which allows
RegEx devices to be managed and configured and supports RegEx operations to be
provisioned on RegEx poll mode driver.


Device Management
-----------------

Device Creation
~~~~~~~~~~~~~~~

Physical RegEx devices are discovered during the PCI probe/enumeration of the
EAL function which is executed at DPDK initialization, based on
their PCI device identifier, each unique PCI BDF (bus/bridge, device,
function). Specific physical ReEx devices, like other physical devices in DPDK
can be listed using the EAL command line options.


Device Identification
~~~~~~~~~~~~~~~~~~~~~

Each device, whether virtual or physical is uniquely designated by two
identifiers:

- A unique device index used to designate the RegEx device in all functions
  exported by the regexdev API.

- A device name used to designate the RegEx device in console messages, for
  administration or debugging purposes.


Device Configuration
~~~~~~~~~~~~~~~~~~~~

The configuration of each RegEx device includes the following operations:

- Allocation of resources, including hardware resources if a physical device.
- Resetting the device into a well-known default state.
- Initialization of statistics counters.

The rte_regexdev_configure API is used to configure a RegEx device.

.. code-block:: c

   int rte_regexdev_configure(uint8_t dev_id,
                              const struct rte_regexdev_config *cfg);

The ``rte_regexdev_config`` structure is used to pass the configuration
parameters for the RegEx device for example  number of queue pairs, number of
groups, max number of matches and so on.

.. code-block:: c

   struct rte_regexdev_config {
        uint16_t nb_max_matches;
        /**< Maximum matches per scan configured on this device.
         * This value cannot exceed the *max_matches*
         * which previously provided in rte_regexdev_info_get().
         * The value 0 is allowed, in which case, value 1 used.
         * @see struct rte_regexdev_info::max_matches
         */
        uint16_t nb_queue_pairs;
        /**< Number of RegEx queue pairs to configure on this device.
         * This value cannot exceed the *max_queue_pairs* which previously
         * provided in rte_regexdev_info_get().
         * @see struct rte_regexdev_info::max_queue_pairs
         */
        uint32_t nb_rules_per_group;
        /**< Number of rules per group to configure on this device.
         * This value cannot exceed the *max_rules_per_group*
         * which previously provided in rte_regexdev_info_get().
         * The value 0 is allowed, in which case,
         * struct rte_regexdev_info::max_rules_per_group used.
         * @see struct rte_regexdev_info::max_rules_per_group
         */
        uint16_t nb_groups;
        /**< Number of groups to configure on this device.
         * This value cannot exceed the *max_groups*
         * which previously provided in rte_regexdev_info_get().
         * @see struct rte_regexdev_info::max_groups
         */
        const char *rule_db;
        /**< Import initial set of prebuilt rule database on this device.
         * The value NULL is allowed, in which case, the device will not
         * be configured prebuilt rule database. Application may use
         * rte_regexdev_rule_db_update() or rte_regexdev_rule_db_import() API
         * to update or import rule database after the
         * rte_regexdev_configure().
         * @see rte_regexdev_rule_db_update(), rte_regexdev_rule_db_import()
         */
        uint32_t rule_db_len;
        /**< Length of *rule_db* buffer. */
        uint32_t dev_cfg_flags;
        /**< RegEx device configuration flags, See RTE_REGEXDEV_CFG_*  */
    };


Configuration of Rules Database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each Regex device should be configured with the rule database.
There are two modes of setting the rule database, online or offline.
The online mode means, that the rule database in being compiled by the
RegEx PMD while in the offline mode the rule database is compiled by external
compiler, and is being loaded to the PMD as a buffer.
The configuration mode is depended on the PMD capabilities.

Online rule configuration is done using the following API functions:
``rte_regexdev_rule_db_update`` which add / remove rules from the rules
precompiled list, and ``rte_regexdev_rule_db_compile_activate``
which compile the rules and loads them to the RegEx HW.

Offline rule configuration can be done by adding a pointer to the compiled
rule database in the configuration step, or by using
``rte_regexdev_rule_db_import`` API.


Configuration of Queue Pairs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each RegEx device can be configured with number of queue pairs.
Each queue pair is configured using ``rte_regexdev_queue_pair_setup``


Logical Cores, Memory and Queues Pair Relationships
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Multiple logical cores should never share the same queue pair for enqueuing
operations or dequeuing operations on the same RegEx device since this would
require global locks and hinder performance.


Device Features and Capabilities
---------------------------------

RegEx devices may support different feature set.
In order to get the supported PMD feature ``rte_regexdev_info_get``
API which return the info of the device and it's supported features.


Enqueue / Dequeue Burst APIs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The burst enqueue API uses a RegEx device identifier and a queue pair
identifier to specify the device queue pair to schedule the processing on.
The ``nb_ops`` parameter is the number of operations to process which are
supplied in the ``ops`` array of ``rte_regex_ops`` structures.
The enqueue function returns the number of operations it actually enqueued for
processing, a return value equal to ``nb_ops`` means that all packets have been
enqueued.

Data pointed in each op, should not be released until the dequeue of for that
op.

The dequeue API uses the same format as the enqueue API of processed but
the ``nb_ops`` and ``ops`` parameters are now used to specify the max processed
operations the user wishes to retrieve and the location in which to store them.
The API call returns the actual number of processed operations returned, this
can never be larger than ``nb_ops``.
