..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

Hello World Sample Application
==============================

The Hello World sample application is an example of the simplest DPDK application that can be written.
The application simply prints an "helloworld" message on every enabled lcore.

Compiling the Application
-------------------------

#.  Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk
        cd ${RTE_SDK}/examples/helloworld

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started* Guide for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

To run the example in a linuxapp environment:

.. code-block:: console

    $ ./build/helloworld -l 0-3 -n 4

Refer to *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of code.

EAL Initialization
~~~~~~~~~~~~~~~~~~

The first task is to initialize the Environment Abstraction Layer (EAL).
This is done in the main() function using the following code:

.. code-block:: c

    int

    main(int argc, char **argv)

    {
        ret = rte_eal_init(argc, argv);
        if (ret < 0)
            rte_panic("Cannot init EAL\n");

This call finishes the initialization process that was started before main() is called (in case of a Linuxapp environment).
The argc and argv arguments are provided to the rte_eal_init() function.
The value returned is the number of parsed arguments.

Starting Application Unit Lcores
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once the EAL is initialized, the application is ready to launch a function on an lcore.
In this example, lcore_hello() is called on every available lcore.
The following is the definition of the function:

.. code-block:: c

    static int
    lcore_hello( attribute ((unused)) void *arg)
    {
        unsigned lcore_id;

        lcore_id = rte_lcore_id();
        printf("hello from core %u\n", lcore_id);
        return 0;
    }

The code that launches the function on each lcore is as follows:

.. code-block:: c

    /* call lcore_hello() on every slave lcore */

    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
       rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
    }

    /* call it on master lcore too */

    lcore_hello(NULL);

The following code is equivalent and simpler:

.. code-block:: c

    rte_eal_mp_remote_launch(lcore_hello, NULL, CALL_MASTER);

Refer to the *DPDK API Reference* for detailed information on the rte_eal_mp_remote_launch() function.
