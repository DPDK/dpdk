.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 Red Hat, Inc.

Linux uAPI header files
=======================

Rationale
---------

The system a DPDK library or driver is built on is not necessarily running the
same Kernel version than the system that will run it.
Importing Linux Kernel uAPI headers enable to build features that are not
supported yet by the build system.

For example, the build system runs upstream Kernel v5.19 and we would like to
build a VDUSE application that will use VDUSE_IOTLB_GET_INFO ioctl() introduced
in Linux Kernel v6.0.

`Linux Kernel licence exception regarding syscalls
<https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/LICENSES/exceptions/Linux-syscall-note>`_
enable importing unmodified Linux Kernel uAPI header files.

Importing or updating an uAPI header file
-----------------------------------------

In order to ensure the imported uAPI headers are both unmodified and from a
released version of the linux Kernel, a helper script is made available and
MUST be used.
Below is an example to import ``linux/vduse.h`` file from Linux ``v6.10``:

.. code-block:: console

   devtools/linux-uapi.sh -i linux/vduse.h -u v6.10

Once imported, the header files should be committed without any other change.
Note that all the imported headers will be updated to the requested version.

Updating imported headers to a newer released version should only be done on
a need basis, it can be achieved using the same script:

.. code-block:: console

   devtools/linux-uapi.sh -u v6.10

The commit message should reflect why updating the headers is necessary.

Once committed, user can check headers are valid by using the same Linux
uAPI script using the check option:

.. code-block:: console

   devtools/linux-uapi.sh -c

Note that all the linux-uapi.sh script options can be combined. For example:

.. code-block:: console

   devtools/linux-uapi.sh -i linux/virtio_net.h -u v6.10 -c

Header inclusion into library or driver
---------------------------------------

The library or driver willing to make use of imported uAPI headers needs to
explicitly include the header file with ``uapi/`` prefix in C files.
For example to include VDUSE uAPI:

.. code-block:: c

   #include <uapi/linux/vduse.h>

