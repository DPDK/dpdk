..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Compiling the DPDK Target from Source
=====================================

System Requirements
-------------------

Building the DPDK and its applications requires one of the following
environments:

* LLVM 14.0.0 (or later) and Microsoft MSVC linker.
* The MinGW-w64 10.0 (or later) toolchain (either native or cross).
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

The minimal Meson supported version is 1.5.2.


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

Open a 'Visual Studio Developer Command Prompt'.
The developer prompt will configure the environment
to select the appropriate compiler, linker and SDK paths
required to build with Visual Studio 2022.

Building DPDK applications that run on 32-bit Windows is currently not supported.
If your Visual Studio environment defaults to producing 32-bit binaries,
you can instruct the toolset to produce 64-bit binaries using "-arch" parameter.
For more details about the Developer Prompt options, look at the
`Visual Studio Developer Command Prompt and Developer PowerShell
<https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=vs-2022>`_.

.. code-block:: console

    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=amd64

Compile the code from the developer prompt.

.. code-block:: console

   cd C:\Users\me\dpdk
   meson setup -Denable_stdatomic=true build
   meson compile -C build
