..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Intel Corporation.

Running DPDK Unit Tests with Meson
==================================

This section describes how to run test cases with the DPDK meson build system.

Steps to build and install DPDK using meson can be referred
in :doc:`build-sdk-meson`

Grouping of test cases
----------------------

Test cases have been classified into four different groups.

* Fast tests.
* Performance tests.
* Driver tests.
* Tests which produce lists of objects as output, and therefore that need
  manual checking.

These tests can be run using the argument to ``meson test`` as
``--suite project_name:label``.

For example::

    $ meson test -C <build path> --suite DPDK:fast-tests

If the ``<build path>`` is current working directory,
the ``-C <build path>`` option can be skipped as below::

    $ meson test --suite DPDK:fast-tests

The project name is optional so the following is equivalent to the previous
command::

    $ meson test --suite fast-tests

If desired, additional arguments can be passed to the test run via the meson
``--test-args`` option.
For example, tests will by default run on as many available cores as is needed
for the test, starting with the lowest number core - generally core 0.
To run the fast-tests suite using only cores 8 through 16, one can use::

    $ meson test --suite fast-tests --test-args="-l 8-16"

The meson command to list all available tests::

    $ meson test --list

Test cases are run serially by default for better stability.

Arguments of ``test()`` that can be provided in meson.build are as below:

* ``is_parallel`` is used to run test case either in parallel or non-parallel mode.
* ``timeout`` is used to specify the timeout of test case.
* ``args`` is used to specify test specific parameters (see note below).
* ``env`` is used to specify test specific environment parameters.

Note: the content of meson ``--test-args`` option and the content of ``args``
are appended when invoking the DPDK test binary.
Because of this, it is recommended not to set any default coremask or memory
configuration in per test ``args`` and rather let users select what best fits
their environment. If a test can't run, then it should be skipped, as described
below.


Dealing with skipped test cases
-------------------------------

Some unit test cases have a dependency on external libraries, driver modules
or config flags, without which the test cases cannot be run. Such test cases
will be reported as skipped if they cannot run. To enable those test cases,
the user should ensure the required dependencies are met.
Below are a few possible causes why tests may be skipped:

#. Optional external libraries are not found.
#. Config flags for the dependent library are not enabled.
#. Dependent driver modules are not installed on the system.
#. Not enough processing cores. Some tests are skipped on machines with 2 or 4 cores.
