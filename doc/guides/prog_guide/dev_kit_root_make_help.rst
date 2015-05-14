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

        make config O=mybuild T=x86_64-native-linuxapp-gcc

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

    Build the DPDK binary.
    Actually, this builds each supported target in a separate directory.
    The name of each directory is the name of the target.
    The name of the targets to install can be optionally specified using T=mytarget.
    The target name can contain wildcard \* characters.
    The list of available targets are in $(RTE_SDK)/config (remove the defconfig\_ prefix).

    Example:

    .. code-block:: console

        make install T=x86_64-*

*   Uninstall

    Remove installed target directories.

Test Targets
------------

*   test

    Launch automatic tests for a build directory specified using O=mybuilddir.
    It is optional, the default output directory is build.

    Example:

    .. code-block:: console

        make test O=mybuild

*   testall

    Launch automatic tests for all installed target directories (after a make install).
    The name of the targets to test can be optionally specified using T=mytarget.
    The target name can contain wildcard (\*) characters.
    The list of available targets are in $(RTE_SDK)/config (remove the defconfig\_  prefix).

    Examples:

    .. code-block:: console

        make testall, make testall T=x86_64-*

Documentation Targets
---------------------

*   doc

    Generate the Doxygen documentation (API, html and pdf).

*   doc-api-html

    Generate the Doxygen API documentation in html.

*   doc-guides-html

    Generate the guides documentation in html.

*   doc-guides-pdf

    Generate the guides documentation in pdf.


Deps Targets
------------

*   depdirs

    This target is implicitly called by make config.
    Typically, there is no need for a user to call it,
    except if DEPDIRS-y variables have been updated in Makefiles.
    It will generate the file  $(RTE_OUTPUT)/.depdirs.

    Example:

    .. code-block:: console

        make depdirs O=mybuild

*   depgraph

    This command generates a dot graph of dependencies.
    It can be displayed to debug circular dependency issues, or just to understand the dependencies.

    Example:

    .. code-block:: console

        make depgraph O=mybuild > /tmp/graph.dot && dotty /tmp/ graph.dot

Misc Targets
------------

*   help

    Show this help.

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
    make config O=mybuild T=x86_64-native-linuxapp-gcc
    make O=mybuild

is equivalent to:

.. code-block:: console

    cd $(RTE_SDK)
    make config O=mybuild T=x86_64-native-linuxapp-gcc
    cd mybuild

    # no need to specify O= now
    make

Compiling for Debug
-------------------

To compile the DPDK and sample applications with debugging information included and the optimization level set to 0,
the EXTRA_CFLAGS environment variable should be set before compiling as follows:

.. code-block:: console

    export EXTRA_CFLAGS='-O0 -g'

The DPDK and any user or sample applications can then be compiled in the usual way.
For example:

.. code-block:: console

    make install T=x86_64-native-linuxapp-gcc make -C examples/<theapp>
