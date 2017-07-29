..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

.. _linux_gsg_compiling_dpdk:

Compiling the DPDK Target from Source
=====================================

.. note::

    Parts of this process can also be done using the setup script described in
    the :ref:`linux_setup_script` section of this document.

Install the DPDK and Browse Sources
-----------------------------------

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

Installation of DPDK Target Environments
----------------------------------------

The format of a DPDK target is::

    ARCH-MACHINE-EXECENV-TOOLCHAIN

where:

* ``ARCH`` can be:  ``i686``, ``x86_64``, ``ppc_64``

* ``MACHINE`` can be:  ``native``, ``power8``

* ``EXECENV`` can be:  ``linuxapp``,  ``bsdapp``

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

    make install T=x86_64-native-linuxapp-icc

To compile a 32-bit build using gcc, the make command should be:

.. code-block:: console

    make install T=i686-native-linuxapp-gcc

To prepare a target without building it, for example, if the configuration changes need to be made before compilation,
use the ``make config T=<target>`` command:

.. code-block:: console

    make config T=x86_64-native-linuxapp-gcc

.. warning::

    Any kernel modules to be used, e.g. ``igb_uio``, ``kni``, must be compiled with the
    same kernel as the one running on the target.
    If the DPDK is not being built on the target machine,
    the ``RTE_KERNELDIR`` environment variable should be used to point the compilation at a copy of the kernel version to be used on the target machine.

Once the target environment is created, the user may move to the target environment directory and continue to make code changes and re-compile.
The user may also make modifications to the compile-time DPDK configuration by editing the .config file in the build directory.
(This is a build-local copy of the defconfig file from the top- level config directory).

.. code-block:: console

    cd x86_64-native-linuxapp-gcc
    vi .config
    make

In addition, the make clean command can be used to remove any existing compiled files for a subsequent full, clean rebuild of the code.

Browsing the Installed DPDK Environment Target
----------------------------------------------

Once a target is created it contains all libraries, including poll-mode drivers, and header files for the DPDK environment that are required to build customer applications.
In addition, the test and testpmd applications are built under the build/app directory, which may be used for testing.
A kmod  directory is also present that contains kernel modules which may be loaded if needed.
