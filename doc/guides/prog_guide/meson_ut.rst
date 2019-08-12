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

The meson command to list all available tests::

    $ meson test --list

Test cases are run serially by default for better stability.

Arguments of ``test()`` that can be provided in meson.build are as below:

* ``is_parallel`` is used to run test case either in parallel or non-parallel mode.
* ``timeout`` is used to specify the timeout of test case.
* ``args`` is used to specify test specific parameters.
* ``env`` is used to specify test specific environment parameters.


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
