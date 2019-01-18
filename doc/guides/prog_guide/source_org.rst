..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

**Part 2: Development Environment**

Source Organization
===================

This section describes the organization of sources in the DPDK framework.

Makefiles and Config
--------------------

.. note::

    In the following descriptions,
    ``RTE_SDK`` is the environment variable that points to the base directory into which the tarball was extracted.
    See
    :ref:`Useful_Variables_Provided_by_the_Build_System`
    for descriptions of other variables.

Makefiles that are provided by the DPDK libraries and applications are located in ``$(RTE_SDK)/mk``.

Config templates are located in ``$(RTE_SDK)/config``. The templates describe the options that are enabled for each target.
The config file also contains items that can be enabled and disabled for many of the DPDK libraries,
including debug options.
The user should look at the config file and become familiar with these options.
The config file is also used to create a header file, which will be located in the new build directory.

Libraries
---------

Libraries are located in subdirectories of ``$(RTE_SDK)/lib``.
By convention a library refers to any code that provides an API to an application.
Typically, it generates an archive file (``.a``), but a kernel module would also go in the same directory.

Drivers
-------

Drivers are special libraries which provide poll-mode driver implementations for
devices: either hardware devices or pseudo/virtual devices. They are contained
in the *drivers* subdirectory, classified by type, and each compiles to a
library with the format ``librte_pmd_X.a`` where ``X`` is the driver name.

.. note::

   Several of the ``driver/net`` directories contain a ``base``
   sub-directory. The ``base`` directory generally contains code the shouldn't
   be modified directly by the user. Any enhancements should be done via the
   ``X_osdep.c`` and/or ``X_osdep.h`` files in that directory. Refer to the
   local README in the base directories for driver specific instructions.


Applications
------------

Applications are source files that contain a ``main()`` function.
They are located in the ``$(RTE_SDK)/app`` and ``$(RTE_SDK)/examples`` directories.

The app directory contains sample applications that are used to test DPDK (such as autotests)
or the Poll Mode Drivers (test-pmd).

The examples directory contains :doc:`Sample applications<../sample_app_ug/index>` that show how libraries can be used.
