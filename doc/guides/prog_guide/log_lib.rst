..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2023 Intel Corporation.

Log Library
===========

The DPDK Log library provides the logging functionality for other DPDK libraries and drivers.
By default, in a Linux application, logs are sent to syslog and also to the console.
On FreeBSD and Windows applications, logs are sent only to the console.
However, the log function can be overridden by the user to use a different logging mechanism.

Log Levels
----------

Log messages from apps and libraries are reported with a given level of severity.
These levels, specified in ``rte_log.h`` are (from most to least important):

#. Emergency
#. Alert
#. Critical
#. Error
#. Warning
#. Notice
#. Information
#. Debug

At runtime, only messages of a configured level or above (i.e. of higher importance)
will be emitted by the application to the log output.
That level can be configured either by the application calling the relevant APIs from the logging library,
or by the user passing the ``--log-level`` parameter to the EAL via the application.

Setting Global Log Level
~~~~~~~~~~~~~~~~~~~~~~~~

To adjust the global log level for an application,
just pass a numeric level or a level name to the ``--log-level`` EAL parameter.
For example::

	/path/to/app --log-level=error

	/path/to/app --log-level=debug

	/path/to/app --log-level=5   # warning

Within an application, the log level can be similarly set using the ``rte_log_set_global_level`` API.

Setting Log Level for a Component
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In some cases, for example, for debugging purposes,
it may be desirable to increase or decrease the log level for only a specific component, or set of components.
To facilitate this, the ``--log-level`` argument also accepts an, optionally wildcarded, component name,
along with the desired level for that component.
For example::

	/path/to/app --log-level=lib.eal:crit

	/path/to/app --log-level=lib.*:warning

Within an application, the same result can be got using the ``rte_log_set_level_pattern()`` or ``rte_log_set_level_regex()`` APIs.

Using Logging APIs to Generate Log Messages
-------------------------------------------

To output log messages, ``rte_log()`` API function should be used.
As well as the log message, ``rte_log()`` takes two additional parameters:

* The log level
* The log component type

The log level is a numeric value as discussed above.
The component type is a unique id that identifies the particular DPDK component to the logging system.
To get this id, each component needs to register itself at startup,
using the macro ``RTE_LOG_REGISTER_DEFAULT``.
This macro takes two parameters, with the second being the default log level for the component.
The first parameter, called "type", the name of the "logtype", or "component type" variable used in the component.
This variable will be defined by the macro, and should be passed as the second parameter in calls to ``rte_log()``.
In general, most DPDK components define their own logging macros to simplify the calls to the log APIs.
They do this by:

* Hiding the component type parameter inside the macro so it never needs to be passed explicitly.
* Using the log-level definitions given in ``rte_log.h`` to allow short textual names to be used in
  place of the numeric log levels.

The following code is taken from ``rte_dmadev.c`` and shows the log registration,
and subsequent definition of a shortcut logging macro.
It can be used as a template for any new components using DPDK logging.

.. code:: C

	RTE_LOG_REGISTER_DEFAULT(rte_dma_logtype, INFO);
	#define RTE_DMA_LOG(level, ...) \
		rte_log(RTE_LOG_ ## level, rte_dma_logtype, RTE_FMT("dma: " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", RTE_FMT_TAIL(__VA_ARGS__,)))

.. note::

	Because the log registration macro provides the logtype variable definition,
	it should be placed near the top of the C file using it.
	If not, the logtype variable should be defined as an "extern int" near the top of the file.

	Similarly, if logging is to be done by multiple files in a component,
	only one file should register the logtype via the macro,
	and the logtype should be defined as an "extern int" in a common header file.
	Any component-specific logging macro should similarly be defined in that header.

Throughout the dmadev library, all logging calls are therefore of the form:

.. code:: C

	RTE_DMA_LOG(ERR, "Name can't be NULL");

	RTE_DMA_LOG(WARNING, "Device %d already started", dev_id);
