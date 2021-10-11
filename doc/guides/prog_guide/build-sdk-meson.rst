..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Installing DPDK Using the meson build system
============================================

Summary
--------
For many platforms, compiling and installing DPDK should work using the
following set of commands::

	meson build
	cd build
	ninja
	ninja install

This will compile DPDK in the ``build`` subdirectory, and then install the
resulting libraries, drivers and header files onto the system - generally
in /usr/local. A package-config file, ``libdpdk.pc``,  for DPDK will also
be installed to allow ease of compiling and linking with applications.

After installation, to use DPDK, the necessary CFLAG and LDFLAG variables
can be got from pkg-config::

	pkg-config --cflags libdpdk
	pkg-config --libs libdpdk

More detail on each of these steps can be got from the following sections.


Getting the Tools
------------------

The ``meson`` tool is used to configure a DPDK build. On most Linux
distributions this can be got using the local package management system,
e.g. ``dnf install meson`` or ``apt-get install meson``. If meson is not
available as a suitable package, it can also be installed using the Python
3 ``pip`` tool, e.g. ``pip3 install meson``. Version 0.49.2 of meson is
required - if the version packaged is too old, the latest version is
generally available from "pip".

The other dependency for building is the ``ninja`` tool, which acts similar
to make and performs the actual build using information provided by meson.
Installing meson will, in many cases, also install ninja, but, if not
already installed, it too is generally packaged by most Linux distributions.
If not available as a package, it can be downloaded as source or binary from
https://ninja-build.org/

It is best advised to go over the following links for the complete dependencies:

* :doc:`Linux <../linux_gsg/sys_reqs>`
* :doc:`FreeBSD <../freebsd_gsg/build_dpdk>`
* :doc:`Windows <../windows_gsg/build_dpdk>`


Configuring the Build
----------------------

To configure a build, run the meson tool, passing the path to the directory
to be used for the build e.g. ``meson build``, as shown above. If calling
meson from somewhere other than the root directory of the DPDK project the
path to the root directory should be passed as the first parameter, and the
build path as the second. For example, to build DPDK in /tmp/dpdk-build::

	user@host:/tmp$ meson ~user/dpdk dpdk-build

Meson will then configure the build based on settings in the project's
meson.build files, and by checking the build environment for e.g. compiler
properties or the presence of dependencies, such as libpcap, or openssl
libcrypto libraries. Once done, meson writes a ``build.ninja`` file in the
build directory to be used to do the build itself when ninja is called.

Tuning of the build is possible, both as part of the original meson call,
or subsequently using ``meson configure`` command (``mesonconf`` in some
older versions). Some options, such as ``buildtype``, or ``werror`` are
built into meson, while others, such as ``max_lcores``, or the list of
examples to build, are DPDK-specific. To have a list of all options
available run ``meson configure`` in the build directory.

Examples of adjusting the defaults when doing initial meson configuration.
Project-specific options are passed used -Doption=value::

	meson --werror werrorbuild  # build with warnings as errors

	meson --buildtype=debug debugbuild  # build for debugging

	meson -Dexamples=l3fwd,l2fwd fwdbuild  # build some examples as
					# part of the normal DPDK build

	meson -Dmax_lcores=8 smallbuild  # scale build for smaller systems

	meson -Denable_docs=true fullbuild  # build and install docs

	meson -Dcpu_instruction_set=generic  # use builder-independent baseline -march

	meson -Ddisable_drivers=event/*,net/tap  # disable tap driver and all
					# eventdev PMDs for a smaller build

	meson -Denable_trace_fp=true tracebuild # build with fast path traces
					# enabled

Examples of setting some of the same options using meson configure::

	meson configure -Dwerror=true

	meson configure -Dbuildtype=debug

	meson configure -Dexamples=l3fwd,l2fwd

	meson configure -Dmax_lcores=8

	meson configure -Denable_trace_fp=true

.. note::

        once meson has been run to configure a build in a directory, it
        cannot be run again on the same directory. Instead ``meson configure``
        should be used to change the build settings within the directory, and when
        ``ninja`` is called to do the build itself, it will trigger the necessary
        re-scan from meson.

.. note::

   cpu_instruction_set=generic uses an instruction set that works on
   all supported architectures regardless of the capabilities of the machine
   where the build is happening.

.. note::

   cpu_instruction_set is not used in Arm builds, as setting the instruction set
   without other parameters leads to inferior builds.
   The way to tailor Arm builds is to build for a SoC using -Dplatform=<SoC>.

As well as those settings taken from ``meson configure``, other options
such as the compiler to use can be passed via environment variables. For
example::

	CC=clang meson clang-build

.. note::

        for more comprehensive overriding of compilers or other environment
        settings, the tools for cross-compilation may be considered. However, for
        basic overriding of the compiler etc., the above form works as expected.


Performing the Build
---------------------

Use ``ninja`` to perform the actual build inside the build folder
previously configured. In most cases no arguments are necessary.

Ninja accepts a number of flags which are similar to make. For example, to
call ninja from outside the build folder, you can use ``ninja -C build``.
Ninja also runs parallel builds by default, but you can limit this using
the ``-j`` flag, e.g. ``ninja -j1 -v`` to do the build one step at a time,
printing each command on a new line as it runs.


Installing the Compiled Files
------------------------------

Use ``ninja install`` to install the required DPDK files onto the system.
The install prefix defaults to ``/usr/local`` but can be used as with other
options above. The environment variable ``DESTDIR`` can be used to adjust
the root directory for the install, for example when packaging.

With the base install directory, the individual directories for libraries
and headers are configurable. By default, the following will be the
installed layout::

	headers -> /usr/local/include
	libraries -> /usr/local/lib64
	drivers -> /usr/local/lib64/dpdk/drivers
	libdpdk.pc -> /usr/local/lib64/pkgconfig

For the drivers, these will also be symbolically linked into the library
install directory, so that ld.so can find them in cases where one driver may
depend on another, e.g. a NIC PMD depending upon the PCI bus driver. Within
the EAL, the default search path for drivers will be set to the configured
driver install path, so dynamically-linked applications can be run without
having to pass in ``-d /path/to/driver`` options for standard drivers.


Cross Compiling DPDK
--------------------

To cross-compile DPDK on a desired target machine we can use the following
command::

	meson cross-build --cross-file <target_machine_configuration>

For example if the target machine is arm64 we can use the following
command::

        meson arm-build --cross-file config/arm/arm64_armv8_linux_gcc

where config/arm/arm64_armv8_linux_gcc contains settings for the compilers
and other build tools to be used, as well as characteristics of the target
machine.

Using the DPDK within an Application
-------------------------------------

To compile and link against DPDK within an application, pkg-config should
be used to query the correct parameters. Examples of this are given in the
makefiles for the example applications included with DPDK. They demonstrate
how to link either against the DPDK shared libraries, or against the static
versions of the same.

From examples/helloworld/Makefile::

	PC_FILE := $(shell pkg-config --path libdpdk)
	CFLAGS += -O3 $(shell pkg-config --cflags libdpdk)
	LDFLAGS_SHARED = $(shell pkg-config --libs libdpdk)
	LDFLAGS_STATIC = $(shell pkg-config --static --libs libdpdk)

	build/$(APP)-shared: $(SRCS-y) Makefile $(PC_FILE) | build
		$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

	build/$(APP)-static: $(SRCS-y) Makefile $(PC_FILE) | build
		$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

	build:
		@mkdir -p $@
