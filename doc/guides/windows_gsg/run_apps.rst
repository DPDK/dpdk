..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 Dmitry Kozlyuk

Running DPDK Applications
=========================

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
