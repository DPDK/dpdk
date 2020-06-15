..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Dmitry Kozlyuk

Running DPDK Applications
=========================

Grant *Lock pages in memory* Privilege
--------------------------------------

Use of hugepages ("large pages" in Windows terminology) requires
``SeLockMemoryPrivilege`` for the user running an application.

1. Open *Local Security Policy* snap-in, either:

   * Control Panel / Computer Management / Local Security Policy;
   * or Win+R, type ``secpol``, press Enter.

2. Open *Local Policies / User Rights Assignment / Lock pages in memory.*

3. Add desired users or groups to the list of grantees.

4. Privilege is applied upon next logon. In particular, if privilege has been
   granted to current user, a logoff is required before it is available.

See `Large-Page Support`_ in MSDN for details.

.. _Large-Page Support: https://docs.microsoft.com/en-us/windows/win32/memory/large-page-support


Run the ``helloworld`` Example
------------------------------

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
