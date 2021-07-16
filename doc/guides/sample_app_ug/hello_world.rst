..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Hello World Sample Application
==============================

The Hello World sample application is an example of the simplest DPDK application that can be written.
The application simply prints an "helloworld" message on every enabled lcore.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``helloworld`` sub-directory.

Running the Application
-----------------------

To run the example in a linux environment:

.. code-block:: console

    $ ./<build_dir>/examples/dpdk-helloworld -l 0-3 -n 4

Refer to *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of code.

EAL Initialization
~~~~~~~~~~~~~~~~~~

The first task is to initialize the Environment Abstraction Layer (EAL).
This is done in the main() function using the following code:

.. literalinclude:: ../../../examples/helloworld/main.c
    :language: c
    :start-after: Initialization of Environment Abstraction Layer (EAL). 8<
    :end-before: >8 End of initialization of Environment Abstraction Layer

This call finishes the initialization process that was started before main() is called (in case of a Linux environment).
The argc and argv arguments are provided to the rte_eal_init() function.
The value returned is the number of parsed arguments.

Starting Application Unit Lcores
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once the EAL is initialized, the application is ready to launch a function on an lcore.
In this example, lcore_hello() is called on every available lcore.
The following is the definition of the function:

.. literalinclude:: ../../../examples/helloworld/main.c
    :language: c
    :start-after: Launch a function on lcore. 8<
    :end-before: >8 End of launching function on lcore.

The code that launches the function on each lcore is as follows:

.. literalinclude:: ../../../examples/helloworld/main.c
    :language: c
    :start-after: Launches the function on each lcore. 8<
    :end-before: >8 End of launching the function on each lcore.
    :dedent: 1

The following code is equivalent and simpler:

.. literalinclude:: ../../../examples/helloworld/main.c
    :language: c
    :start-after: Simpler equivalent. 8<
    :end-before: >8 End of simpler equivalent.
    :dedent: 2

Refer to the *DPDK API Reference* for detailed information on the rte_eal_mp_remote_launch() function.
