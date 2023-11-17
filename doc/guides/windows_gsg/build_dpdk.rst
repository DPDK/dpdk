..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Compiling the DPDK Target from Source
=====================================

System Requirements
-------------------

Building the DPDK and its applications requires one of the following
environments:

* LLVM 14.0.0 (or later) and Microsoft MSVC linker.
* The MinGW-w64 toolchain (either native or cross).
* Microsoft Visual Studio 2022 (any edition).

  - note Microsoft Visual Studio 2022 does not currently build enough
    of DPDK to produce a working DPDK application
    but may be used to validate that changes are portable between toolchains.

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

On Linux, i.e. for cross-compilation, install MinGW-w64 via a package manager.
Version 4.0.4 for Ubuntu 16.04 cannot be used due to a
`MinGW-w64 bug <https://sourceforge.net/p/mingw-w64/bugs/562/>`_.

On Windows, obtain the latest version installer from
`MinGW-w64 repository <https://sourceforge.net/projects/mingw-w64/files/>`_.
Any thread model (POSIX or Win32) can be chosen, DPDK does not rely on it.
Install to a folder without spaces in its name, like ``C:\MinGW``.
This path is assumed for the rest of this guide.


Option 3. Microsoft Visual Studio Toolset (MSVC)
------------------------------------------------

Install any edition of Microsoft Visual Studio 2022
from the `Visual Studio website <https://visualstudio.microsoft.com/downloads/>`_.


Install the Build System
------------------------

Download and install the build system from
`Meson website <http://mesonbuild.com/Getting-meson.html>`_.
A good option to choose is the MSI installer for both meson and ninja together::

	http://mesonbuild.com/Getting-meson.html#installing-meson-and-ninja-with-the-msi-installer%22

Required version is Meson 0.57.

Versions starting from 0.58 are unusable with LLVM toolchain
because of a `Meson issue <https://github.com/mesonbuild/meson/issues/8981>`_.


Install the Backend
-------------------

If using Ninja, download and install the backend from
`Ninja website <https://ninja-build.org/>`_ or
install along with the meson build system.

Build the code
--------------

The build environment is setup to build the EAL and the helloworld example by
default.

Option 1. Native Build on Windows using LLVM
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
    meson setup -Dexamples=helloworld build
    meson compile -C build

Option 2. Cross-Compile with MinGW-w64
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The cross-file option must be specified for Meson.
Depending on the distribution, paths in this file may need adjustments.

.. code-block:: console

    meson setup --cross-file config/x86/cross-mingw -Dexamples=helloworld build
    ninja -C build

Option 3. Native Build on Windows using MSVC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open a 'Developer PowerShell for VS 2022' prompt from the start menu.
The developer prompt will configure the environment
to select the appropriate compiler, linker and SDK paths
required to build with Visual Studio 2022.

.. code-block:: console

   cd C:\Users\me\dpdk
   meson setup -Denable_stdatomic=true build
   meson compile -C build
