..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Compiling the DPDK Target from Source
=====================================

System Requirements
-------------------

Building the DPDK and its applications requires one of the following
environments:

* The Clang-LLVM C compiler and Microsoft MSVC linker.
* The MinGW-w64 toolchain (either native or cross).

The Meson Build system is used to prepare the sources for compilation
with the Ninja backend.
The installation of these tools is covered in this section.


Option 1. Clang-LLVM C Compiler and Microsoft MSVC Linker
---------------------------------------------------------

Install the Compiler
~~~~~~~~~~~~~~~~~~~~

Download and install the clang compiler from
`LLVM website <http://releases.llvm.org/download.html>`_.
For example, Clang-LLVM direct download link::

	http://releases.llvm.org/7.0.1/LLVM-7.0.1-win64.exe


Install the Linker
~~~~~~~~~~~~~~~~~~

Download and install the Build Tools for Visual Studio to link and build the
files on windows,
from `Microsoft website <https://visualstudio.microsoft.com/downloads>`_.
When installing build tools, select the "Visual C++ build tools" option
and ensure the Windows SDK is selected.


Option 2. MinGW-w64 Toolchain
-----------------------------

Obtain the latest version from
`MinGW-w64 website <http://mingw-w64.org/doku.php/download>`_.
On Windows, install to a folder without spaces in its name, like ``C:\MinGW``.
This path is assumed for the rest of this guide.

Version 4.0.4 for Ubuntu 16.04 cannot be used due to a
`MinGW-w64 bug <https://sourceforge.net/p/mingw-w64/bugs/562/>`_.


Install the Build System
------------------------

Download and install the build system from
`Meson website <http://mesonbuild.com/Getting-meson.html>`_.
A good option to choose is the MSI installer for both meson and ninja together::

	http://mesonbuild.com/Getting-meson.html#installing-meson-and-ninja-with-the-msi-installer%22

Recommended version is either Meson 0.47.1 (baseline) or the latest release.

Install the Backend
-------------------

If using Ninja, download and install the backend from
`Ninja website <https://ninja-build.org/>`_ or
install along with the meson build system.

Build the code
--------------

The build environment is setup to build the EAL and the helloworld example by
default.

Option 1. Native Build on Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When using Clang-LLVM, specifying the compiler might be required to complete
the meson command:

.. code-block:: console

    set CC=clang

When using MinGW-w64, it is sufficient to have toolchain executables in PATH:

.. code-block:: console

    set PATH=C:\MinGW\mingw64\bin;%PATH%

To compile the examples, the flag ``-Dexamples`` is required.

.. code-block:: console

    cd C:\Users\me\dpdk
    meson -Dexamples=helloworld build
    ninja -C build


Run the helloworld example
==========================

Navigate to the examples in the build directory and run `dpdk-helloworld.exe`.

.. code-block:: console

    cd C:\Users\me\dpdk\build\examples
    dpdk-helloworld.exe
    hello from core 1
    hello from core 3
    hello from core 0
    hello from core 2

Note for MinGW-w64: applications are linked to ``libwinpthread-1.dll``
by default. To run the example, either add toolchain executables directory
to the PATH or copy the library to the working directory.
Alternatively, static linking may be used (mind the LGPLv2.1 license).
