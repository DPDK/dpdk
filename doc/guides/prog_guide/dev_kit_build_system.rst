..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

.. _Development_Kit_Build_System:

Development Kit Build System
============================

The DPDK requires a build system for compilation activities and so on.
This section describes the constraints and the mechanisms used in the DPDK framework.

There are two use-cases for the framework:

*   Compilation of the DPDK libraries and sample applications;
    the framework generates specific binary libraries,
    include files and sample applications

*   Compilation of an external application or library, using an installed binary DPDK

Building the Development Kit Binary
-----------------------------------

The following provides details on how to build the DPDK binary.

Build Directory Concept
~~~~~~~~~~~~~~~~~~~~~~~

After installation, a build directory structure is created.
Each build directory contains include files, libraries, and applications:

.. code-block:: console

    ~/DPDK$ ls
    app                               MAINTAINERS
    config                            Makefile
    COPYRIGHT                         mk
    doc                               scripts
    examples                          lib
    tools                             x86_64-native-linuxapp-gcc
    x86_64-native-linuxapp-icc        i686-native-linuxapp-gcc
    i686-native-linuxapp-icc

    ...
    ~/DEV/DPDK$ ls i686-native-linuxapp-gcc

    app build buildtools include kmod lib Makefile


    ~/DEV/DPDK$ ls i686-native-linuxapp-gcc/app/
    cmdline_test   dump_cfg     test     testpmd
    cmdline_test.map      dump_cfg.map   test.map
	    testpmd.map


    ~/DEV/DPDK$ ls i686-native-linuxapp-gcc/lib/

    libethdev.a  librte_hash.a  librte_mbuf.a librte_pmd_ixgbe.a

    librte_cmdline.a librte_lpm.a librte_mempool.a librte_ring.a

    librte_eal.a librte_pmd_e1000.a librte_timer.a


    ~/DEV/DPDK$ ls i686-native-linuxapp-gcc/include/
    arch                       rte_cpuflags.h       rte_memcpy.h
    cmdline_cirbuf.h           rte_cycles.h         rte_memory.h
    cmdline.h                  rte_debug.h          rte_mempool.h
    cmdline_parse_etheraddr.h  rte_eal.h            rte_memzone.h
    cmdline_parse.h            rte_errno.h          rte_pci_dev_ids.h
    cmdline_parse_ipaddr.h     rte_ethdev.h         rte_pci.h
    cmdline_parse_num.h        rte_ether.h          rte_per_lcore.h
    cmdline_parse_portlist.h   rte_fbk_hash.h       rte_prefetch.h
    cmdline_parse_string.h     rte_hash_crc.h       rte_random.h
    cmdline_rdline.h           rte_hash.h           rte_ring.h
    cmdline_socket.h           rte_interrupts.h     rte_rwlock.h
    cmdline_vt100.h            rte_ip.h             rte_sctp.h
    exec-env                   rte_jhash.h          rte_spinlock.h
    rte_alarm.h                rte_launch.h         rte_string_fns.h
    rte_atomic.h               rte_lcore.h          rte_tailq.h
    rte_branch_prediction.h    rte_log.h            rte_tcp.h
    rte_byteorder.h            rte_lpm.h            rte_timer.h
    rte_common.h               rte_malloc.h         rte_udp.h
    rte_config.h               rte_mbuf.h


A build directory is specific to a configuration that includes architecture + execution environment + toolchain.
It is possible to have several build directories sharing the same sources with different configurations.

For instance, to create a new build directory called my_sdk_build_dir using the default configuration template config/defconfig_x86_64-linuxapp,
we use:

.. code-block:: console

    cd ${RTE_SDK}
    make config T=x86_64-native-linuxapp-gcc O=my_sdk_build_dir

This creates a new my_sdk_build_dir directory. After that, we can compile by doing:

.. code-block:: console

    cd my_sdk_build_dir
    make

which is equivalent to:

.. code-block:: console

    make O=my_sdk_build_dir

The content of the my_sdk_build_dir is then:

::

    -- .config                         # used configuration

    -- Makefile                        # wrapper that calls head Makefile
                                       # with $PWD as build directory


        -- build                              #All temporary files used during build
        +--app                                # process, including . o, .d, and .cmd files.
            |  +-- test                       # For libraries, we have the .a file.
            |  +-- test.o                     # For applications, we have the elf file.
            |  `-- ...
            +-- lib
                +-- librte_eal
                |   `-- ...
                +-- librte_mempool
                |  +--  mempool-file1.o
                |  +--  .mempool-file1.o.cmd
                |  +--  .mempool-file1.o.d
                |  +--   mempool-file2.o
                |  +--  .mempool-file2.o.cmd
                |  +--  .mempool-file2.o.d
                |  `--  mempool.a
                `-- ...

    -- include                # All include files installed by libraries
        +-- librte_mempool.h  # and applications are located in this
        +-- rte_eal.h         # directory. The installed files can depend
        +-- rte_spinlock.h    # on configuration if needed (environment,
        +-- rte_atomic.h      # architecture, ..)
        `-- \*.h ...

    -- lib                    # all compiled libraries are copied in this
        +-- librte_eal.a      # directory
        +-- librte_mempool.a
        `-- \*.a ...

    -- app                    # All compiled applications are installed
    + --test                  # here. It includes the binary in elf format

Refer to
:ref:`Development Kit Root Makefile Help <Development_Kit_Root_Makefile_Help>`
for details about make commands that can be used from the root of DPDK.

Building External Applications
------------------------------

Since DPDK is in essence a development kit, the first objective of end users will be to create an application using this SDK.
To compile an application, the user must set the RTE_SDK and RTE_TARGET environment variables.

.. code-block:: console

    export RTE_SDK=/opt/DPDK
    export RTE_TARGET=x86_64-native-linuxapp-gcc
    cd /path/to/my_app

For a new application, the user must create their own Makefile that includes some .mk files, such as
${RTE_SDK}/mk/rte.vars.mk, and ${RTE_SDK}/mk/ rte.app.mk.
This is described in
:ref:`Building Your Own Application <Building_Your_Own_Application>`.

Depending on the chosen target (architecture, machine, executive environment, toolchain) defined in the Makefile or as an environment variable,
the applications and libraries will compile using the appropriate .h files and will link with the appropriate .a files.
These files are located in ${RTE_SDK}/arch-machine-execenv-toolchain, which is referenced internally by ${RTE_BIN_SDK}.

To compile their application, the user just has to call make.
The compilation result will be located in /path/to/my_app/build directory.

Sample applications are provided in the examples directory.

.. _Makefile_Description:

Makefile Description
--------------------

General Rules For DPDK Makefiles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the DPDK, Makefiles always follow the same scheme:

#. Include $(RTE_SDK)/mk/rte.vars.mk at the beginning.

#. Define specific variables for RTE build system.

#. Include a specific $(RTE_SDK)/mk/rte.XYZ.mk, where XYZ can be app, lib, extapp, extlib, obj, gnuconfigure,
   and so on, depending on what kind of object you want to build.
   :ref:`See Makefile Types <Makefile_Types>` below.

#. Include user-defined rules and variables.

   The following is a very simple example of an external application Makefile:

   ..  code-block:: make

        include $(RTE_SDK)/mk/rte.vars.mk

        # binary name
        APP = helloworld

        # all source are stored in SRCS-y
        SRCS-y := main.c

        CFLAGS += -O3
        CFLAGS += $(WERROR_FLAGS)

        include $(RTE_SDK)/mk/rte.extapp.mk

.. _Makefile_Types:

Makefile Types
~~~~~~~~~~~~~~

Depending on the .mk file which is included at the end of the user Makefile, the Makefile will have a different role.
Note that it is not possible to build a library and an application in the same Makefile.
For that, the user must create two separate Makefiles, possibly in two different directories.

In any case, the rte.vars.mk file must be included in the user Makefile as soon as possible.

Application
^^^^^^^^^^^

These Makefiles generate a binary application.

*   rte.app.mk: Application in the development kit framework

*   rte.extapp.mk: External application

*   rte.hostapp.mk: prerequisite tool to build dpdk

Library
^^^^^^^

Generate a .a library.

*   rte.lib.mk: Library in the development kit framework

*   rte.extlib.mk: external library

*   rte.hostlib.mk: host library in the development kit framework

Install
^^^^^^^

*   rte.install.mk: Does not build anything, it is only used to create links or copy files to the installation directory.
    This is useful for including files in the development kit framework.

Kernel Module
^^^^^^^^^^^^^

*   rte.module.mk: Build a kernel module in the development kit framework.

Objects
^^^^^^^

*   rte.obj.mk: Object aggregation (merge several .o in one) in the development kit framework.

*   rte.extobj.mk: Object aggregation (merge several .o in one) outside the development kit framework.

Misc
^^^^

*   rte.doc.mk: Documentation in the development kit framework

*   rte.gnuconfigure.mk: Build an application that is configure-based.

*   rte.subdir.mk: Build several directories in the development kit framework.

.. _Internally_Generated_Build_Tools:

Internally Generated Build Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``app/dpdk-pmdinfogen``


``dpdk-pmdinfogen`` scans an object (.o) file for various well known symbol names.
These well known symbol names are defined by various macros and used to export
important information about hardware support and usage for pmd files.  For
instance the macro:

.. code-block:: c

   PMD_REGISTER_DRIVER(drv, name)

Creates the following symbol:

.. code-block:: c

   static char this_pmd_name0[] __attribute__((used)) = "<name>";


Which ``dpdk-pmdinfogen`` scans for.  Using this information other relevant
bits of data can be exported from the object file and used to produce a
hardware support description, that ``dpdk-pmdinfogen`` then encodes into a
json formatted string in the following format:

.. code-block:: c

   static char <name_pmd_string>="PMD_INFO_STRING=\"{'name' : '<name>', ...}\"";


These strings can then be searched for by external tools to determine the
hardware support of a given library or application.


.. _Useful_Variables_Provided_by_the_Build_System:

Useful Variables Provided by the Build System
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*   RTE_SDK: The absolute path to the DPDK sources.
    When compiling the development kit, this variable is automatically set by the framework.
    It has to be defined by the user as an environment variable if compiling an external application.

*   RTE_SRCDIR: The path to the root of the sources. When compiling the development kit, RTE_SRCDIR = RTE_SDK.
    When compiling an external application, the variable points to the root of external application sources.

*   RTE_OUTPUT: The path to which output files are written.
    Typically, it is $(RTE_SRCDIR)/build, but it can be overridden by the O= option in the make command line.

*   RTE_TARGET: A string identifying the target for which we are building.
    The format is arch-machine-execenv-toolchain.
    When compiling the SDK, the target is deduced by the build system from the configuration (.config).
    When building an external application, it must be specified by the user in the Makefile or as an environment variable.

*   RTE_SDK_BIN: References $(RTE_SDK)/$(RTE_TARGET).

*   RTE_ARCH: Defines the architecture (i686, x86_64).
    It is the same value as CONFIG_RTE_ARCH  but without the double-quotes around the string.

*   RTE_MACHINE: Defines the machine.
    It is the same value as CONFIG_RTE_MACHINE but without the double-quotes around the string.

*   RTE_TOOLCHAIN: Defines the toolchain (gcc , icc).
    It is the same value as CONFIG_RTE_TOOLCHAIN but without the double-quotes around the string.

*   RTE_EXEC_ENV: Defines the executive environment (linuxapp).
    It is the same value as CONFIG_RTE_EXEC_ENV but without the double-quotes around the string.

*   RTE_KERNELDIR: This variable contains the absolute path to the kernel sources that will be used to compile the kernel modules.
    The kernel headers must be the same as the ones that will be used on the target machine (the machine that will run the application).
    By default, the variable is set to /lib/modules/$(shell uname -r)/build,
    which is correct when the target machine is also the build machine.

*   RTE_DEVEL_BUILD: Stricter options (stop on warning). It defaults to y in a git tree.

Variables that Can be Set/Overridden in a Makefile Only
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*   VPATH: The path list that the build system will search for sources. By default, RTE_SRCDIR will be included in VPATH.

*   CFLAGS: Flags to use for C compilation. The user should use +=  to append data in this variable.

*   LDFLAGS: Flags to use for linking. The user should use +=  to append data in this variable.

*   ASFLAGS: Flags to use for assembly. The user should use +=  to append data in this variable.

*   CPPFLAGS: Flags to use to give flags to C preprocessor (only useful when assembling .S files).
    The user should use += to append data in this variable.

*   LDLIBS: In an application, the list of libraries to link with (for example, -L  /path/to/libfoo -lfoo ).
    The user should use  +=  to append data in this variable.

*   SRC-y: A list of source files (.c, .S, or .o  if the source is a binary) in case of application, library or object Makefiles.
    The sources must be available from VPATH.

*   INSTALL-y-$(INSTPATH): A list of files to be installed in  $(INSTPATH).
    The files must be available from VPATH and will be copied in $(RTE_OUTPUT)/$(INSTPATH). Can be used in almost any RTE Makefile.

*   SYMLINK-y-$(INSTPATH): A list of files to be installed in $(INSTPATH).
    The files must be available from VPATH and will be linked (symbolically) in  $(RTE_OUTPUT)/$(INSTPATH).
    This variable can be used in almost any DPDK Makefile.

*   PREBUILD: A list of prerequisite actions to be taken before building. The user should use +=  to append data in this variable.

*   POSTBUILD: A list of actions to be taken after the main build. The user should use += to append data in this variable.

*   PREINSTALL: A list of prerequisite actions to be taken before installing. The user should use += to append data in this variable.

*   POSTINSTALL: A list of actions to be taken after installing. The user should use += to append data in this variable.

*   PRECLEAN: A list of prerequisite actions to be taken before cleaning. The user should use += to append data in this variable.

*   POSTCLEAN: A list of actions to be taken after cleaning. The user should use += to append data in this variable.

*   DEPDIR-y: Only used in the development kit framework to specify if the build of the current directory depends on build of another one.
    This is needed to support parallel builds correctly.

Variables that can be Set/Overridden by the User on the Command Line Only
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Some variables can be used to configure the build system behavior. They are documented in
:ref:`Development Kit Root Makefile Help <Development_Kit_Root_Makefile_Help>` and
:ref:`External Application/Library Makefile Help <External_Application/Library_Makefile_Help>`

    *   WERROR_CFLAGS: By default, this is set to a specific value that depends on the compiler.
        Users are encouraged to use this variable as follows:

            CFLAGS += $(WERROR_CFLAGS)

This avoids the use of different cases depending on the compiler (icc or gcc).
Also, this variable can be overridden from the command line, which allows bypassing of the flags for testing purposes.

Variables that Can be Set/Overridden by the User in a Makefile or Command Line
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*   CFLAGS_my_file.o: Specific flags to add for C compilation of my_file.c.

*   LDFLAGS_my_app: Specific flags to add when linking my_app.

*   EXTRA_CFLAGS: The content of this variable is appended after CFLAGS when compiling.

*   EXTRA_LDFLAGS: The content of this variable is appended after LDFLAGS when linking.

*   EXTRA_LDLIBS: The content of this variable is appended after LDLIBS when linking.

*   EXTRA_ASFLAGS: The content of this variable is appended after ASFLAGS when assembling.

*   EXTRA_CPPFLAGS: The content of this variable is appended after CPPFLAGS when using a C preprocessor on assembly files.
