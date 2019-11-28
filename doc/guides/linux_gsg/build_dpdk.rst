..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

.. _linux_gsg_compiling_dpdk:

Compiling the DPDK Target from Source
=====================================

.. note::

    Parts of this process can also be done using the setup script described in
    the :ref:`linux_setup_script` section of this document.

Uncompress DPDK and Browse Sources
----------------------------------

First, uncompress the archive and move to the uncompressed DPDK source directory:

.. code-block:: console

    tar xJf dpdk-<version>.tar.xz
    cd dpdk-<version>

The DPDK is composed of several directories:

*   lib: Source code of DPDK libraries

*   drivers: Source code of DPDK poll-mode drivers

*   app: Source code of DPDK applications (automatic tests)

*   examples: Source code of DPDK application examples

*   config, buildtools, mk: Framework-related makefiles, scripts and configuration

Compiling and Installing DPDK System-wide
-----------------------------------------

DPDK can be configured, built and installed on your system using the tools
``meson`` and ``ninja``.

.. note::

  The older makefile-based build system used in older DPDK releases is
  still present and its use is described in section
  `Installation of DPDK Target Environment using Make`_.

DPDK Configuration
~~~~~~~~~~~~~~~~~~

To configure a DPDK build use:

.. code-block:: console

     meson <options> build

where "build" is the desired output build directory, and "<options>" can be
empty or one of a number of meson or DPDK-specific build options, described
later in this section. The configuration process will finish with a summary
of what DPDK libraries and drivers are to be built and installed, and for
each item disabled, a reason why that is the case. This information can be
used, for example, to identify any missing required packages for a driver.

Once configured, to build and then install DPDK system-wide use:

.. code-block:: console

        cd build
        ninja
        ninja install
        ldconfig

The last two commands above generally need to be run as root,
with the `ninja install` step copying the built objects to their final system-wide locations,
and the last step causing the dynamic loader `ld.so` to update its cache to take account of the new objects.

.. note::

   On some linux distributions, such as Fedora or Redhat, paths in `/usr/local` are
   not in the default paths for the loader. Therefore, on these
   distributions, `/usr/local/lib` and `/usr/local/lib64` should be added
   to a file in `/etc/ld.so.conf.d/` before running `ldconfig`.


Adjusting Build Options
~~~~~~~~~~~~~~~~~~~~~~~

DPDK has a number of options that can be adjusted as part of the build configuration process.
These options can be listed by running ``meson configure`` inside a configured build folder.
Many of these options come from the "meson" tool itself and can be seen documented on the
`Meson Website <https://mesonbuild.com/Builtin-options.html>`_.

For example, to change the build-type from the default, "debugoptimized",
to a regular "debug" build, you can either:

* pass ``-Dbuildtype=debug`` or ``--buildtype=debug`` to meson when configuring the build folder initially

* run ``meson configure -Dbuildtype=debug`` inside the build folder after the initial meson run.

Other options are specific to the DPDK project but can be adjusted similarly.
To set the "max_lcores" value to 256, for example, you can either:

* pass ``-Dmax_lcores=256`` to meson when configuring the build folder initially

* run ``meson configure -Dmax_lcores=256`` inside the build folder after the initial meson run.

Some of the DPDK sample applications in the `examples` directory can be
automatically built as part of a meson build too.
To do so, pass a comma-separated list of the examples to build to the
`-Dexamples` meson option as below::

  meson -Dexamples=l2fwd,l3fwd build

As with other meson options, this can also be set post-initial-config using `meson configure` in the build directory.
There is also a special value "all" to request that all example applications whose
dependencies are met on the current system are built.
When `-Dexamples=all` is set as a meson option, meson will check each example application to see if it can be built,
and add all which can be built to the list of tasks in the ninja build configuration file.

Building Applications Using Installed DPDK
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When installed system-wide, DPDK provides a pkg-config file ``libdpdk.pc`` for applications to query as part of their build.
It's recommended that the pkg-config file be used, rather than hard-coding the parameters (cflags/ldflags)
for DPDK into the application build process.

An example of how to query and use the pkg-config file can be found in the ``Makefile`` of each of the example applications included with DPDK.
A simplified example snippet is shown below, where the target binary name has been stored in the variable ``$(APP)``
and the sources for that build are stored in ``$(SRCS-y)``.

.. code-block:: makefile

        PKGCONF = pkg-config

        CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
        LDFLAGS += $(shell $(PKGCONF) --libs libdpdk)

        $(APP): $(SRCS-y) Makefile
                $(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS)

.. note::

   Unlike with the older make build system, the meson system is not
   designed to be used directly from a build directory. Instead it is
   recommended that it be installed either system-wide or to a known
   location in the user's home directory. The install location can be set
   using the `--prefix` meson option (default: `/usr/local`).

an equivalent build recipe for a simple DPDK application using meson as a
build system is shown below:

.. code-block:: python

   project('dpdk-app', 'c')

   dpdk = dependency('libdpdk')
   sources = files('main.c')
   executable('dpdk-app', sources, dependencies: dpdk)


Installation of DPDK Target Environment using Make
--------------------------------------------------

.. note::

   The building of DPDK using make will be deprecated in a future release. It
   is therefore recommended that DPDK installation is done using meson and
   ninja as described above.

The format of a DPDK target is::

    ARCH-MACHINE-EXECENV-TOOLCHAIN

where:

* ``ARCH`` can be:  ``i686``, ``x86_64``, ``ppc_64``, ``arm64``

* ``MACHINE`` can be:  ``native``, ``power8``, ``armv8a``

* ``EXECENV`` can be:  ``linux``,  ``freebsd``

* ``TOOLCHAIN`` can be:  ``gcc``,  ``icc``

The targets to be installed depend on the 32-bit and/or 64-bit packages and compilers installed on the host.
Available targets can be found in the DPDK/config directory.
The defconfig\_ prefix should not be used.

.. note::

    Configuration files are provided with the ``RTE_MACHINE`` optimization level set.
    Within the configuration files, the ``RTE_MACHINE`` configuration value is set to native,
    which means that the compiled software is tuned for the platform on which it is built.
    For more information on this setting, and its possible values, see the *DPDK Programmers Guide*.

When using the Intel® C++ Compiler (icc), one of the following commands should be invoked for 64-bit or 32-bit use respectively.
Notice that the shell scripts update the ``$PATH`` variable and therefore should not be performed in the same session.
Also, verify the compiler's installation directory since the path may be different:

.. code-block:: console

    source /opt/intel/bin/iccvars.sh intel64
    source /opt/intel/bin/iccvars.sh ia32

To install and make targets, use the ``make install T=<target>`` command in the top-level DPDK directory.

For example, to compile a 64-bit target using icc, run:

.. code-block:: console

    make install T=x86_64-native-linux-icc

To compile a 32-bit build using gcc, the make command should be:

.. code-block:: console

    make install T=i686-native-linux-gcc

To prepare a target without building it, for example, if the configuration changes need to be made before compilation,
use the ``make config T=<target>`` command:

.. code-block:: console

    make config T=x86_64-native-linux-gcc

.. warning::

    Any kernel modules to be used, e.g. ``igb_uio``, ``kni``, must be compiled with the
    same kernel as the one running on the target.
    If the DPDK is not being built on the target machine,
    the ``RTE_KERNELDIR`` environment variable should be used to point the compilation at a copy of the kernel version to be used on the target machine.

Once the target environment is created, the user may move to the target environment directory and continue to make code changes and re-compile.
The user may also make modifications to the compile-time DPDK configuration by editing the .config file in the build directory.
(This is a build-local copy of the defconfig file from the top- level config directory).

.. code-block:: console

    cd x86_64-native-linux-gcc
    vi .config
    make

In addition, the make clean command can be used to remove any existing compiled files for a subsequent full, clean rebuild of the code.

Browsing the Installed DPDK Environment Target
----------------------------------------------

Once a target is created it contains all libraries, including poll-mode drivers, and header files for the DPDK environment that are required to build customer applications.
In addition, the test and testpmd applications are built under the build/app directory, which may be used for testing.
A kmod  directory is also present that contains kernel modules which may be loaded if needed.
