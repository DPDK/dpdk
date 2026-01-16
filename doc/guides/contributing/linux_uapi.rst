.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2024 Red Hat, Inc.

Linux uAPI header files
=======================

Rationale
---------

The build system may run a different kernel version than the deployment target.
To support features unavailable in the build system's kernel, import Linux kernel uAPI headers.

For example, if the build system runs upstream Kernel v5.19,
but you need to build a VDUSE application
that uses the VDUSE_IOTLB_GET_INFO ioctl introduced in Kernel v6.0,
importing the relevant uAPI headers enables this.

The Linux kernel's syscall license exception permits the inclusion of unmodified uAPI header files in such cases.

`Linux Kernel licence exception regarding syscalls
<https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/LICENSES/exceptions/Linux-syscall-note>`_
enables importing unmodified Linux Kernel uAPI header files.

Importing or updating an uAPI header file
-----------------------------------------

To ensure that imported uAPI headers are unmodified
and sourced from an official Linux kernel release,
a helper script is provided and must be used.
Below is an example to import ``linux/vduse.h`` file from Linux ``v6.10``:

.. code-block:: console

   devtools/linux-uapi.sh -i linux/vduse.h -u v6.10

Once imported, commit header files without any modifications.
Note that all imported headers are updated to match the specified kernel version.

Perform updates to a newer released version only when necessary,
using the same helper script.

.. code-block:: console

   devtools/linux-uapi.sh -u v6.10

The commit message should reflect why updating the headers is necessary.

Once committed, check that headers are valid using the same Linux uAPI script
with the check option:

.. code-block:: console

   devtools/linux-uapi.sh -c

Note that all the linux-uapi.sh script options can be combined. For example:

.. code-block:: console

   devtools/linux-uapi.sh -i linux/virtio_net.h -u v6.10 -c

Header inclusion into library or driver
---------------------------------------

Libraries or drivers that rely on imported uAPI headers
must explicitly include the relevant header
using the ``uapi/`` prefix in their C source files.

This inclusion must be done before any header external to DPDK is included,
to prevent inclusion of the system uAPI header in any of those external headers.

For example, to include VDUSE uAPI:

.. code-block:: c

   #include <uapi/linux/vduse.h>

   #include <stdint.h>
   ...
