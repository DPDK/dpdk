..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

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
take any cores available in the main DPDK coremask, and if the bits are also set
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
