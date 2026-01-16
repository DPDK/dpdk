.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

DPDK Unit Testing Guidelines
============================

This document outlines the guidelines for running and adding new
tests to the in-tree DPDK test suites.

The DPDK test suite model is loosely based on the xUnit model,
where tests are grouped into test suites, and suites are run by runners.
For a basic overview, see the Wikipedia article on `xUnit
<https://en.wikipedia.org/wiki/XUnit>`_.


Background
----------

The in-tree testing infrastructure for DPDK consists of
multiple applications and support tools.
The primary tools are the ``dpdk-test`` application
and the ``meson test`` infrastructure.
These two are the primary ways users interact with the DPDK testing infrastructure.

Some confusion exists regarding test suite and test case separation
between ``dpdk-test`` and ``meson test``.
Both have a concept of test suite and test case.
In both, the concept is similar.
A test suite is a group of test cases,
and a test case represents the steps needed to test a particular set of code.
Where needed, they will be disambiguated by the word Meson
to denote a Meson test suite / case.


Running a test
--------------

DPDK tests run via the main test runner, the ``dpdk-test`` app.
The ``dpdk-test`` app is a command-line interface that facilitates
running various tests or test suites.

There are three modes of operation.
The first mode is an interactive command shell
that allows launching specific test suites.
This is the default operating mode of ``dpdk-test`` and can be invoked by::

   $ ./build/app/test/dpdk-test --dpdk-options-here
   EAL: Detected 4 lcore(s)
   EAL: Detected 1 NUMA nodes
   EAL: Static memory layout is selected, amount of reserved memory...
   EAL: Multi-process socket /run/user/26934/dpdk/rte/mp_socket
   EAL: Selected IOVA mode 'VA'
   EAL: Probing VFIO support...
   EAL: PCI device 0000:00:1f.6 on NUMA socket -1
   EAL:   Invalid NUMA socket, default to 0
   EAL:   probe driver: 8086:15d7 net_e1000_em
   APP: HPET is not enabled, using TSC as default timer
   RTE>>

At the prompt, type the name of the test suite you wish to run
and it will execute.

The second form is useful for a scripting environment
and is used by the DPDK Meson build system.
Invoke this mode by
assigning a specific test suite name to the environment variable ``DPDK_TEST``
before invoking the ``dpdk-test`` command, such as::

   $ DPDK_TEST=version_autotest ./build/app/test/dpdk-test --dpdk-options-here
   EAL: Detected 4 lcore(s)
   EAL: Detected 1 NUMA nodes
   EAL: Static memory layout is selected, amount of reserved memory can be...
   EAL: Multi-process socket /run/user/26934/dpdk/rte/mp_socket
   EAL: Selected IOVA mode 'VA'
   EAL: Probing VFIO support...
   EAL: PCI device 0000:00:1f.6 on NUMA socket -1
   EAL:   Invalid NUMA socket, default to 0
   EAL:   probe driver: 8086:15d7 net_e1000_em
   APP: HPET is not enabled, using TSC as default timer
   RTE>>version_autotest
   Version string: 'DPDK 20.02.0-rc0'
   Test OK
   RTE>>$

The above shows running a specific test suite.
On success, the return code will be 0;
otherwise it will be set to some error value (such as 255).

The third form is an alternative
to providing the test suite name in an environment variable.
The unit test app can accept test suite names via command line arguments::

   $ ./build/app/test/dpdk-test --dpdk-options-here version_autotest version_autotest
   EAL: Detected 8 lcore(s)
   EAL: Detected 1 NUMA nodes
   EAL: Static memory layout is selected, amount of reserved memory can be...
   EAL: Detected static linkage of DPDK
   EAL: Multi-process socket /run/user/26934/dpdk/rte/mp_socket
   EAL: Selected IOVA mode 'VA'
   APP: HPET is not enabled, using TSC as default timer
   RTE>>version_autotest
   Version string: 'DPDK 21.08.0-rc0'
   Test OK
   RTE>>version_autotest
   Version string: 'DPDK 21.08.0-rc0'
   Test OK
   RTE>>

The primary benefit here is specifying multiple test names,
which is not possible with the ``DPDK_TEST`` environment variable.

Additionally, you can specify additional test parameters
via the ``DPDK_TEST_PARAMS`` argument,
in case some tests need additional configuration.
This is not currently used in the Meson test suites.


Running test cases via Meson
----------------------------

To allow developers to quickly execute all the standard internal tests
without needing to remember or look up each test suite name,
the build system includes a standard way of executing the Meson test suites.
After building via ``ninja``, the ``meson test`` command
with no arguments executes the Meson test suites.

There are a number of pre-configured Meson test suites.
The first is the **fast** test suite, which is the largest group of test cases.
These are the bulk of the unit tests to validate functional blocks.
The second is the **perf** tests.
These test suites can take longer to run and do performance evaluations.
The third is the **driver** test suite,
which is mostly for special hardware related testing (such as crypto devices).
The fourth, and currently the last, suite is the **debug** suite.
These tests mostly dump system information.

Select the Meson test suites by adding the ``--suite`` option
to the ``meson test`` command.
Ex: ``meson test --suite fast-tests``::

   $ meson test -C build --suite fast-tests
   ninja: Entering directory `/home/aconole/git/dpdk/build'
   [2543/2543] Linking target app/test/dpdk-test.
   1/60 DPDK:fast-tests / acl_autotest          OK       3.17 s
   2/60 DPDK:fast-tests / bitops_autotest       OK       0.22 s
   3/60 DPDK:fast-tests / byteorder_autotest    OK       0.22 s
   4/60 DPDK:fast-tests / cmdline_autotest      OK       0.28 s
   5/60 DPDK:fast-tests / common_autotest       OK       0.57 s
   6/60 DPDK:fast-tests / cpuflags_autotest     OK       0.27 s
   ...

The ``meson test`` command can also execute individual Meson test cases
via the command line by adding the test names as an argument::

   $ meson test -C build version_autotest
   ninja: Entering directory `/home/aconole/git/dpdk/build'
   [2543/2543] Linking target app/test/dpdk-test.
   1/1 DPDK:fast-tests / version_autotest OK             0.17s
   ...

Note that Meson must know these test cases
for the ``meson test`` command to run them.
Simply adding a new test to the ``dpdk-test`` application is not enough.
See the section `Adding a suite or test case to Meson`_ for more details.


Adding tests to dpdk-test application
-------------------------------------

Add unit tests to the system
whenever introducing new functionality to DPDK
or resolving a bug.
This helps the DPDK project catch regressions as they occur.

The DPDK test application supports two layers of tests:
   #. *test cases* which are individual tests
   #. *test suites* which are groups of test cases

To add a new test suite to the DPDK test application,
create a new test file for that suite
(ex: see *app/test/test_version.c* for the ``version_autotest`` test suite).
There are two important functions for interacting with the test harness:

   ``REGISTER_<MESON_SUITE>_TEST(command_name, function_to_execute)``
      Registers a test command with the name `command_name`
      and which runs the function `function_to_execute` when `command_name` is invoked.
      This macro automatically adds the test to the Meson test suite `<MESON_SUITE>`.
      Examples: ``REGISTER_DRIVER_TEST``, or ``REGISTER_PERF_TEST``.
      **NOTE:** The ``REGISTER_FAST_TEST`` macro is slightly different,
      in that it takes two additional parameters before the function name:
      the hugepage requirement (``NOHUGE_OK`` if the test can run without hugepages,
      or ``NOHUGE_SKIP`` if hugepages are required),
      and Address Sanitizer compatibility (``ASAN_OK`` if the test can run with ASan enabled,
      or ``ASAN_SKIP`` if it cannot).

   ``unit_test_suite_runner(struct unit_test_suite *)``
      Returns a runner for a full test suite object,
      which contains a test suite name, setup, tear down,
      a pointer to a list of sub-testsuites,
      and vector of unit test cases.

Each test suite has a setup and tear down function
that runs at the beginning and end of the test suite execution.
Each unit test has a similar function for test case setup and tear down.

Each test suite may use a nested list of sub-testsuites,
which the ``unit_test_suite_runner`` iterates.
This support allows for better granularity when designing test suites.
The sub-testsuites list can also be used in parallel with the vector of test cases;
in this case, the test cases run first, then each sub-testsuite executes.
To see an example of a test suite using sub-testsuites,
see *app/test/test_cryptodev.c*.

Example of a test suite with a single test case:

.. code-block:: c
   :linenos:

   #include "test.h"

   static int testsuite_setup(void) { return TEST_SUCCESS; }
   static void testsuite_teardown(void) { }

   static int ut_setup(void) { return TEST_SUCCESS; }
   static void ut_teardown(void) { }

   static int test_case_first(void) { return TEST_SUCCESS; }

   static struct unit_test_suite example_testsuite = {
          .suite_name = "EXAMPLE TEST SUITE",
          .setup = testsuite_setup,
          .teardown = testsuite_teardown,
          .unit_test_cases = {
               TEST_CASE_ST(ut_setup, ut_teardown, test_case_first),

               TEST_CASES_END(), /**< NULL terminate unit test array */
          },
   };

   static int example_tests()
   {
       return unit_test_suite_runner(&example_testsuite);
   }

   REGISTER_PERF_TEST(example_autotest, example_tests);

The above code block is a small example
that can be used to create a complete test suite with a test case.

Add sub-testsuites to the ``.unit_test_suites`` element
of the unit test suite structure, for example:

.. code-block:: c
   :linenos:

   static int testsuite_setup(void) { return TEST_SUCCESS; }
   static void testsuite_teardown(void) { }

   static int ut_setup(void) { return TEST_SUCCESS; }
   static void ut_teardown(void) { }

   static int test_case_first(void) { return TEST_SUCCESS; }

   static struct unit_test_suite example_parent_testsuite = {
          .suite_name = "EXAMPLE PARENT TEST SUITE",
          .setup = testsuite_setup,
          .teardown = testsuite_teardown,
          .unit_test_cases = {TEST_CASES_END()}
   };

   static int sub_testsuite_setup(void) { return TEST_SUCCESS; }
   static void sub_testsuite_teardown(void) { }

   static struct unit_test_suite example_sub_testsuite = {
          .suite_name = "EXAMPLE SUB TEST SUITE",
          .setup = sub_testsuite_setup,
          .teardown = sub_testsuite_teardown,
          .unit_test_cases = {
               TEST_CASE_ST(ut_setup, ut_teardown, test_case_first),

               TEST_CASES_END(), /**< NULL terminate unit test array */
          },
   };

   static struct unit_test_suite end_testsuite = {
          .suite_name = NULL,
          .setup = NULL,
          .teardown = NULL,
          .unit_test_suites = NULL
   };

   static int example_tests()
   {
       uint8_t ret, i = 0;
       struct unit_test_suite *sub_suites[] = {
              &example_sub_testsuite,
              &end_testsuite /**< NULL test suite to indicate end of list */
        };

       example_parent_testsuite.unit_test_suites =
               malloc(sizeof(struct unit_test_suite *) * RTE_DIM(sub_suites));

       for (i = 0; i < RTE_DIM(sub_suites); i++)
           example_parent_testsuite.unit_test_suites[i] = sub_suites[i];

       ret = unit_test_suite_runner(&example_parent_testsuite);
       free(example_parent_testsuite.unit_test_suites);

       return ret;
   }

   REGISTER_FAST_TEST(example_autotest, NOHUGE_OK, ASAN_OK, example_tests);


Designing a test
----------------

Test cases have multiple ways of indicating an error has occurred,
in order to reflect failure state back to the runner.
Using the various methods of indicating errors can assist
in not only validating the requisite functionality is working,
but also to help debug when a change in environment or code
has caused things to go wrong.

The first way to indicate a generic error is
by returning a test result failure, using the ``TEST_FAILED`` error code.
This is the most basic way of indicating that an error
has occurred in a test routine.
It is not very informative to the user, so use it in cases
where the test has catastrophically failed.

The preferred method of indicating an error is
via the ``RTE_TEST_ASSERT`` family of macros,
which immediately return ``TEST_FAILED`` error condition,
but also log details about the failure.
The basic form is:

.. code-block:: c

   RTE_TEST_ASSERT(cond, msg, ...)

In the above macro, *cond* is the condition to evaluate to **true**.
Any generic condition can go here.
The *msg* parameter is a message to display if *cond* evaluates to **false**.
Some specialized macros already exist.
See ``lib/librte_eal/include/rte_test.h`` for a list of defined test assertions.

Sometimes it is important to indicate that a test needs to be skipped,
either because the environment cannot support running the test,
or because some requisite functionality is unavailable.
The test suite supports returning a result of ``TEST_SKIPPED``
during test case setup, or during test case execution
to indicate that the preconditions of the test are not available.
Example::

   $ meson test -C build --suite fast-tests
   ninja: Entering directory `/home/aconole/git/dpdk/build'
   [2543/2543] Linking target app/test/dpdk-test.
   1/60 DPDK:fast-tests / acl_autotest          OK       3.17 s
   2/60 DPDK:fast-tests / bitops_autotest       OK       0.22 s
   3/60 DPDK:fast-tests / byteorder_autotest    OK       0.22 s
   ...
   46/60 DPDK:fast-tests / ipsec_autotest       SKIP     0.22 s
   ...


Checking code coverage
----------------------

The Meson build system supports generating a code coverage report
via the ``-Db_coverage=true`` option,
in conjunction with a package like **lcov**,
to generate an HTML code coverage report.
Example::

   $ meson setup build -Db_coverage=true
   $ meson test -C build --suite fast-tests
   $ ninja coverage-html -C build

The above generates an HTML report
in the ``build/meson-logs/coveragereport/`` directory
that can be explored for detailed code covered information.
Use this to assist in test development.


Adding a suite or test case to Meson
------------------------------------

Adding to one of the Meson test suites involves using the appropriate macro
to register the test in dpdk-test, as described above.
For example,
defining the test command using ``REGISTER_PERF_TEST`` automatically
adds the test to the perf-test meson suite.
Once added, the new test runs
as part of the appropriate class (fast, perf, driver, etc.).

Confirm that a test is known to Meson
by using the ``--list`` option::

   $ meson test -C build --list
   DPDK:fast-tests / acl_autotest
   DPDK:fast-tests / bitops_autotest
   ...

Some of these test suites run during continuous integration tests,
making regression checking automatic for new patches submitted to the project.

.. note::

   The old ``REGISTER_TEST_COMMAND`` macro
   to add a command without adding it to a meson test suite is deprecated.
   All new tests must be added to a test suite
   using the appropriate ``REGISTER_<SUITE>_TEST`` macro.

Running cryptodev tests
-----------------------

When running cryptodev tests, create any required virtual device
via EAL arguments, as this is not done automatically by the test::

   $ ./build/app/test/dpdk-test --vdev crypto_aesni_mb
   $ meson test -C build --suite driver-tests \
                --test-args="--vdev crypto_aesni_mb"

.. note::

   The ``cryptodev_scheduler_autotest`` is the only exception.
   This vdev is created automatically by the test app,
   as it requires a more complex setup than other vdevs.
