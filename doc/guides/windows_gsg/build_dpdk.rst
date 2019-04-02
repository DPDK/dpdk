..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Compiling the DPDK Target from Source
=====================================

System Requirements
-------------------

The DPDK and its applications require the Clang-LLVM C compiler
and Microsoft MSVC linker.
The Meson Build system is used to prepare the sources for compilation
with the Ninja backend.
The installation of these tools is covered in this section.


Install the Compiler
--------------------

Download and install the clang compiler from
`LLVM website <http://releases.llvm.org/download.html>`_.
For example, Clang-LLVM direct download link::

	http://releases.llvm.org/7.0.1/LLVM-7.0.1-win64.exe


Install the Linker
------------------

Download and install the Build Tools for Visual Studio to link and build the
files on windows,
from `Microsoft website <https://visualstudio.microsoft.com/downloads>`_.
When installing build tools, select the "Visual C++ build tools" option
and ensure the Windows SDK is selected.


Install the Build System
------------------------

Download and install the build system from
`Meson website <http://mesonbuild.com/Getting-meson.html>`_.
A good option to choose is the MSI installer for both meson and ninja together::

	http://mesonbuild.com/Getting-meson.html#installing-meson-and-ninja-with-the-msi-installer%22

Install the Backend
-------------------

If using Ninja, download and install the backend from
`Ninja website <https://ninja-build.org/>`_ or
install along with the meson build system.

Build the code
--------------

The build environment is setup to build the EAL and the helloworld example by
default.

Using the ninja backend
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

    cd C:\Users\me\dpdk
    meson build
    cd build
    ninja

Run the helloworld example
==========================

Navigate to the build directory and run `dpdk-helloworld.exe`.

.. code-block:: console

    cd C:\Users\me\dpdk\build
    helloworld.exe
    hello from core 1
    hello from core 3
    hello from core 0
    hello from core 2
