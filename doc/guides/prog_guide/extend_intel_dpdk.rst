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

Extending the DPDK
=========================

This chapter describes how a developer can extend the DPDK to provide a new library,
a new target, or support a new target.

Example: Adding a New Library libfoo
------------------------------------

To add a new library to the DPDK, proceed as follows:

#.  Add a new configuration option:

   .. code-block:: bash

        for f in config/\*; do \
            echo CONFIG_RTE_LIBFOO=y >> $f; done

#.  Create a new directory with sources:

   .. code-block:: console

        mkdir ${RTE_SDK}/lib/libfoo
        touch ${RTE_SDK}/lib/libfoo/foo.c
        touch ${RTE_SDK}/lib/libfoo/foo.h

#.  Add a foo() function in libfoo.

    Definition is in foo.c:

    .. code-block:: c

        void foo(void)
        {
        }

    Declaration is in foo.h:

    .. code-block:: c

        extern void foo(void);


#.  Update lib/Makefile:

    .. code-block:: console

        vi ${RTE_SDK}/lib/Makefile
        # add:
        # DIRS-$(CONFIG_RTE_LIBFOO) += libfoo

#.  Create a new Makefile for this library, for example, derived from mempool Makefile:

    .. code-block:: console

        cp ${RTE_SDK}/lib/librte_mempool/Makefile ${RTE_SDK}/lib/libfoo/

        vi ${RTE_SDK}/lib/libfoo/Makefile
        # replace:
        # librte_mempool -> libfoo
        # rte_mempool -> foo


#.  Update mk/DPDK.app.mk, and add -lfoo in LDLIBS variable when the option is enabled.
    This will automatically add this flag when linking a DPDK application.


#.  Build the DPDK with the new library (we only show a specific target here):

    .. code-block:: console

        cd ${RTE_SDK}
        make config T=x86_64-native-linuxapp-gcc
        make


#.  Check that the library is installed:

    .. code-block:: console

        ls build/lib
        ls build/include

Example: Using libfoo in the Test Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The test application is used to validate all functionality of the DPDK.
Once you have added a library, a new test case should be added in the test application.

*   A new test_foo.c file should be added, that includes foo.h and calls the foo() function from test_foo().
    When the test passes, the test_foo() function should return 0.

*   Makefile, test.h and commands.c must be updated also, to handle the new test case.

*   Test report generation: autotest.py is a script that is used to generate the test report that is available in the
    ${RTE_SDK}/doc/rst/test_report/autotests directory. This script must be updated also.
    If libfoo is in a new test family, the links in ${RTE_SDK}/doc/rst/test_report/test_report.rst must be updated.

*   Build the DPDK with the updated test application (we only show a specific target here):


    .. code-block:: console

        cd ${RTE_SDK}
        make config T=x86_64-native-linuxapp-gcc
        make
