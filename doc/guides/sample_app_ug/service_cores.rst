..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Service Cores Sample Application
================================

The service cores sample application demonstrates the service cores capabilities
of DPDK. The service cores infrastructure is part of the DPDK EAL, and allows
any DPDK component to register a service. A service is a work item or task, that
requires CPU time to perform its duty.

This sample application registers 5 dummy services. These 5 services are used
to show how the service_cores API can be used to orchestrate these services to
run on different service lcores. This orchestration is done by calling the
service cores APIs, however the sample application introduces a "profile"
concept to contain the service mapping details. Note that the profile concept
is application specific, and not a part of the service cores API.


Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``service_cores`` sub-directory.

Running the Application
-----------------------

To run the example, just execute the binary. Since the application dynamically
adds service cores in the application code itself, there is no requirement to
pass a service core-mask as an EAL argument at startup time.

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-service_cores


Explanation
-----------

The following sections provide some explanation of code focusing on
registering applications from an applications point of view, and modifying the
service core counts and mappings at runtime.


Registering a Service
~~~~~~~~~~~~~~~~~~~~~

The following code section shows how to register a service as an application.
Note that the service component header must be included by the application in
order to register services: ``rte_service_component.h``, in addition
to the ordinary service cores header ``rte_service.h`` which provides
the runtime functions to add, remove and remap service cores.

.. literalinclude:: ../../../examples/service_cores/main.c
    :language: c
    :start-after: Register a service as an application. 8<
    :end-before: >8 End of registering a service as an application.
    :dedent: 2


Controlling A Service Core
~~~~~~~~~~~~~~~~~~~~~~~~~~

This section demonstrates how to add a service core. The ``rte_service.h``
header file provides the functions for dynamically adding and removing cores.
The APIs to add and remove cores use lcore IDs similar to existing DPDK
functions.

These are the functions to start a service core, and have it run a service:

.. literalinclude:: ../../../examples/service_cores/main.c
    :language: c
    :start-after: Register a service as an application. 8<
    :end-before: >8 End of registering a service as an application.
    :dedent: 2

Removing A Service Core
~~~~~~~~~~~~~~~~~~~~~~~

To remove a service core, the steps are similar to adding but in reverse order.
Note that it is not allowed to remove a service core if the service is running,
and the service-core is the only core running that service (see documentation
for ``rte_service_lcore_stop`` function for details).


Conclusion
~~~~~~~~~~

The service cores infrastructure provides DPDK with two main features. The first
is to abstract away hardware differences: the service core can CPU cycles to
a software fallback implementation, allowing the application to be abstracted
from the difference in HW / SW availability. The second feature is a flexible
method of registering functions to be run, allowing the running of the
functions to be scaled across multiple CPUs.
