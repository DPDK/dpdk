..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2015 Intel Corporation.

.. _linux_gsg_compiling_dpdk:

Compiling the DPDK Target from Source
=====================================

Uncompress DPDK and Browse Sources
----------------------------------

First, uncompress the archive and move to the uncompressed DPDK source directory:

.. code-block:: console

    tar xJf dpdk-<version>.tar.xz
    cd dpdk-<version>

The DPDK is composed of several directories, including:

*   doc: DPDK Documentation

*   license: DPDK license information

*   lib: Source code of DPDK libraries

*   drivers: Source code of DPDK poll-mode drivers

*   app: Source code of DPDK applications (automatic tests)

*   examples: Source code of DPDK application examples

*   config, buildtools: Framework-related scripts and configuration

*   usertools: Utility scripts for end-users of DPDK applications

*   devtools: Scripts for use by DPDK developers

*   kernel: Kernel modules needed for some operating systems


Compiling and Installing DPDK System-wide
-----------------------------------------

DPDK can be configured, built and installed on your system using the tools
``meson`` and ``ninja``.


DPDK Configuration
~~~~~~~~~~~~~~~~~~

To configure a DPDK build use:

.. code-block:: console

     meson setup <options> build

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
        meson install
        ldconfig

The last two commands above generally need to be run as root,
with the `meson install` step copying the built objects to their final system-wide locations,
and the last step causing the dynamic loader `ld.so` to update its cache to take account of the new objects.

.. note::

   On some linux distributions, such as Fedora or Redhat, paths in `/usr/local` are
   not in the default paths for the loader. Therefore, on these
   distributions, `/usr/local/lib` and `/usr/local/lib64` should be added
   to a file in `/etc/ld.so.conf.d/` before running `ldconfig`.

.. _adjusting_build_options:

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
The "platform" option specifies a set a configuration parameters that will be used.
The valid values are:

* ``-Dplatform=native`` will tailor the configuration to the build machine.

* ``-Dplatform=generic`` will use configuration that works on all machines
  of the same architecture as the build machine.

* ``-Dplatform=<SoC>`` will use configuration optimized for a particular SoC.
  Consult the "socs" dictionary in ``config/arm/meson.build`` to see which
  SoCs are supported.

The instruction set will be set automatically by default according to these rules:

* ``-Dplatform=native`` sets ``cpu_instruction_set`` to ``native``,
  which configures ``-march`` (x86_64), ``-mcpu`` (ppc), ``-mtune`` (ppc) to ``native``.

* ``-Dplatform=generic`` sets ``cpu_instruction_set`` to ``generic``,
  which configures ``-march`` (x86_64), ``-mcpu`` (ppc), ``-mtune`` (ppc) to
  a common minimal baseline needed for DPDK.

To override what instruction set will be used, set the ``cpu_instruction_set``
parameter to the instruction set of your choice (such as ``corei7``, ``power8``, etc.).

``cpu_instruction_set`` is not used in Arm builds, as setting the instruction set
without other parameters leads to inferior builds. The way to tailor Arm builds
is to build for a SoC using ``-Dplatform=<SoC>`` mentioned above.

The values determined by the ``platform`` parameter may be overwritten.
For example, to set the ``max_lcores`` value to 256, you can either:

* pass ``-Dmax_lcores=256`` to meson when configuring the build folder initially

* run ``meson configure -Dmax_lcores=256`` inside the build folder after the initial meson run.

Some of the DPDK sample applications in the `examples` directory can be
automatically built as part of a meson build too.
To do so, pass a comma-separated list of the examples to build to the
`-Dexamples` meson option as below::

  meson setup -Dexamples=l2fwd,l3fwd build

As with other meson options, this can also be set post-initial-config using `meson configure` in the build directory.
There is also a special value "all" to request that all example applications whose
dependencies are met on the current system are built.
When `-Dexamples=all` is set as a meson option, meson will check each example application to see if it can be built,
and add all which can be built to the list of tasks in the ninja build configuration file.


Building 32-bit DPDK on 64-bit Systems
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To build a 32-bit copy of DPDK on a 64-bit OS,
the ``-m32`` flag should be passed to the compiler and linker
to force the generation of 32-bit objects and binaries.
This can be done either by setting ``CFLAGS`` and ``LDFLAGS`` in the environment,
or by passing the value to meson using ``-Dc_args=-m32`` and ``-Dc_link_args=-m32``.
For correctly identifying and using any dependency packages,
the ``pkg-config`` tool must also be configured
to look in the appropriate directory for .pc files for 32-bit libraries.
This is done by setting ``PKG_CONFIG_LIBDIR`` to the appropriate path.

The following meson command can be used on RHEL/Fedora systems to configure a 32-bit build,
assuming the relevant 32-bit development packages, such as a 32-bit libc, are installed::

  PKG_CONFIG_LIBDIR=/usr/lib/pkgconfig \
      meson setup -Dc_args='-m32' -Dc_link_args='-m32' build

For Debian/Ubuntu systems, the equivalent command is::

  PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig \
      meson setup -Dc_args='-m32' -Dc_link_args='-m32' build

Once the build directory has been configured,
DPDK can be compiled using ``ninja`` as described above.


.. _building_app_using_installed_dpdk:

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

   Unlike with the make build system present in older DPDK releases,
   the meson system is not
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
