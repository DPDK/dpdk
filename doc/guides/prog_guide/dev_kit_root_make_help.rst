..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

.. _Development_Kit_Root_Makefile_Help:

Development Kit Root Makefile Help
==================================

The DPDK provides a root level Makefile with targets for configuration, building, cleaning, testing, installation and others.
These targets are explained in the following sections.

Configuration Targets
---------------------

The configuration target requires the name of the target, which is specified using T=mytarget and it is mandatory.
The list of available targets are in $(RTE_SDK)/config (remove the defconfig _ prefix).

Configuration targets also support the specification of the name of the output directory, using O=mybuilddir.
This is an optional parameter, the default output directory is build.

*   Config

    This will create a build directory, and generates a configuration from a template.
    A Makefile is also created in the new build directory.

    Example:

    .. code-block:: console

        make config O=mybuild T=x86_64-native-linux-gcc

Build Targets
-------------

Build targets support the optional specification of the name of the output directory, using O=mybuilddir.
The default output directory is build.

*   all, build or just make

    Build the DPDK in the output directory previously created by a make config.

    Example:

    .. code-block:: console

        make O=mybuild

*   clean

    Clean all objects created using make build.

    Example:

    .. code-block:: console

        make clean O=mybuild

*   %_sub

    Build a subdirectory only, without managing dependencies on other directories.

    Example:

    .. code-block:: console

        make lib/librte_eal_sub O=mybuild

*   %_clean

    Clean a subdirectory only.

    Example:

    .. code-block:: console

        make lib/librte_eal_clean O=mybuild

Install Targets
---------------

*   Install

    The list of available targets are in $(RTE_SDK)/config (remove the defconfig\_ prefix).

    The GNU standards variables may be used:
    http://gnu.org/prep/standards/html_node/Directory-Variables.html and
    http://gnu.org/prep/standards/html_node/DESTDIR.html

    Example:

    .. code-block:: console

        make install DESTDIR=myinstall prefix=/usr

Test Targets
------------

*   test

    Launch automatic tests for a build directory specified using O=mybuilddir.
    It is optional, the default output directory is build.

    Example:

    .. code-block:: console

        make test O=mybuild

Documentation Targets
---------------------

*   doc

    Generate the documentation (API and guides).

*   doc-api-html

    Generate the Doxygen API documentation in html.

*   doc-guides-html

    Generate the guides documentation in html.

*   doc-guides-pdf

    Generate the guides documentation in pdf.

Misc Targets
------------

*   help

    Show a quick help.

Other Useful Command-line Variables
-----------------------------------

The following variables can be specified on the command line:

*   V=

    Enable verbose build (show full compilation command line, and some intermediate commands).

*   D=

    Enable dependency debugging. This provides some useful information about why a target is built or not.

*   EXTRA_CFLAGS=, EXTRA_LDFLAGS=, EXTRA_LDLIBS=, EXTRA_ASFLAGS=, EXTRA_CPPFLAGS=

    Append specific compilation, link or asm flags.

*   CROSS=

    Specify a cross toolchain header that will prefix all gcc/binutils applications. This only works when using gcc.

Make in a Build Directory
-------------------------

All targets described above are called from the SDK root $(RTE_SDK).
It is possible to run the same Makefile targets inside the build directory.
For instance, the following command:

.. code-block:: console

    cd $(RTE_SDK)
    make config O=mybuild T=x86_64-native-linux-gcc
    make O=mybuild

is equivalent to:

.. code-block:: console

    cd $(RTE_SDK)
    make config O=mybuild T=x86_64-native-linux-gcc
    cd mybuild

    # no need to specify O= now
    make

Compiling for Debug
-------------------

To compile the DPDK and sample applications with debugging information included and the optimization level set to 0,
the EXTRA_CFLAGS environment variable should be set before compiling as follows:

.. code-block:: console

    export EXTRA_CFLAGS='-O0 -g'
