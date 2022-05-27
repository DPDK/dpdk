..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Source Organization
===================

This section describes the organization of sources in the DPDK framework.

Libraries
---------

Libraries are located in subdirectories of ``dpdk/lib``.
By convention a library refers to any code that provides an API to an application.
Typically, it generates an archive file (``.a``), but a kernel module would also go in the same directory.

Drivers
-------

Drivers are special libraries which provide poll-mode driver implementations for
devices: either hardware devices or pseudo/virtual devices. They are contained
in the *drivers* subdirectory, classified by type, and each compiles to a
library with the format ``librte_X_Y.a`` where ``X`` is the device class
name and ``Y`` is the driver name.

.. note::

   Several of the ``driver/net`` directories contain a ``base``
   sub-directory. The ``base`` directory generally contains code the shouldn't
   be modified directly by the user. Any enhancements should be done via the
   ``X_osdep.c`` and/or ``X_osdep.h`` files in that directory. Refer to the
   local README in the base directories for driver specific instructions.


Applications
------------

Applications are source files that contain a ``main()`` function.
They are located in the ``dpdk/app`` and ``dpdk/examples`` directories.

The app directory contains sample applications that are used to test DPDK (such as autotests)
or the Poll Mode Drivers (test-pmd).

The examples directory contains :doc:`Sample applications<../sample_app_ug/index>` that show how libraries can be used.
