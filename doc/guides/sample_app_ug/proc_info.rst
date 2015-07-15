
..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.
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


proc_info Application
========================

The proc_info application is a Data Plane Development Kit (DPDK) application
that runs as a DPDK secondary process and is capable of retrieving port
statistics, resetting port statistics and printing DPDK memory information.
This application extends the original functionality that was supported by
dump_cfg.

Running the Application
-----------------------
The application has a number of command line options:

.. code-block:: console

   ./$(RTE_TARGET)/app/proc_info -- -m | [-p PORTMASK] [--stats | --xstats |
   --stats-reset | --xstats-reset]

Parameters
~~~~~~~~~~
**-p PORTMASK**: Hexadecimal bitmask of ports to configure.

**--stats**
The stats parameter controls the printing of generic port statistics. If no
port mask is specified stats are printed for all DPDK ports.

**--xstats**
The stats parameter controls the printing of extended port statistics. If no
port mask is specified xstats are printed for all DPDK ports.

**--stats-reset**
The stats-reset parameter controls the resetting of generic port statistics. If
no port mask is specified, the generic stats are reset for all DPDK ports.

**--xstats-reset**
The xstats-reset parameter controls the resetting of extended port statistics.
If no port mask is specified xstats are reset for all DPDK ports.

**-m**: Print DPDK memory information.
