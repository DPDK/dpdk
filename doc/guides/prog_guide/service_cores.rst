..  BSD LICENSE
    Copyright(c) 2017 Intel Corporation. All rights reserved.
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

Service Cores
=============

DPDK has a concept known as service cores, which enables a dynamic way of
performing work on DPDK lcores. Service core support is built into the EAL, and
an API is provided to optionally allow applications to control how the service
cores are used at runtime.

The service cores concept is built up out of services (components of DPDK that
require CPU cycles to operate) and service cores (DPDK lcores, tasked with
running services). The power of the service core concept is that the mapping
between service cores and services can be configured to abstract away the
difference between platforms and environments.

For example, the Eventdev has hardware and software PMDs. Of these the software
PMD requires an lcore to perform the scheduling operations, while the hardware
PMD does not. With service cores, the application would not directly notice
that the scheduling is done in software.

For detailed information about the service core API, please refer to the docs.

Service Core Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are two methods to having service cores in a DPDK application, either by
using the service coremask, or by dynamically adding cores using the API.
The simpler of the two is to pass the `-s` coremask argument to EAL, which will
take any cores available in the main DPDK coremask, an if the bits are also set
in the service coremask the cores become service-cores instead of DPDK
application lcores.

Enabling Services on Cores
~~~~~~~~~~~~~~~~~~~~~~~~~~

Each registered service can be individually mapped to a service core, or set of
service cores. Enabling a service on a particular core means that the lcore in
question will run the service. Disabling that core on the service stops the
lcore in question from running the service.

Using this method, it is possible to assign specific workloads to each
service core, and map N workloads to M number of service cores. Each service
lcore loops over the services that are enabled for that core, and invokes the
function to run the service.

Service Core Statistics
~~~~~~~~~~~~~~~~~~~~~~~

The service core library is capable of collecting runtime statistics like number
of calls to a specific service, and number of cycles used by the service. The
cycle count collection is dynamically configurable, allowing any application to
profile the services running on the system at any time.
