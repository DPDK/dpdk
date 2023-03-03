..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2022-2023 PANTHEON.tech s.r.o.

DPDK Test Suite
===============

The DPDK Test Suite, abbreviated DTS, is a Python test framework with test suites
implementing functional and performance tests used to test DPDK.


DTS Terminology
---------------

DTS node
   A generic description of any host/server DTS connects to.

DTS runtime environment
   An environment containing Python with packages needed to run DTS.

DTS runtime environment node
  A node where at least one DTS runtime environment is present.
  This is the node where we run DTS and from which DTS connects to other nodes.

System under test
  An SUT is the combination of DPDK and the hardware we're testing
  in conjunction with DPDK (NICs, crypto and other devices).

System under test node
  A node where at least one SUT is present.

Traffic generator
  A TG is either software or hardware capable of sending packets.

Traffic generator node
  A node where at least one TG is present.
  In case of hardware traffic generators, the TG and the node are literally the same.


In most cases, interchangeably referring to a runtime environment, SUT, TG or the node
they're running on (e.g. using SUT and SUT node interchangeably) doesn't cause confusion.
There could theoretically be more than of these running on the same node and in that case
it's useful to have stricter definitions.
An example would be two different traffic generators (such as Trex and Scapy)
running on the same node.
A different example would be a node containing both a DTS runtime environment
and a traffic generator, in which case it's both a DTS runtime environment node and a TG node.


DTS Environment
---------------

DTS is written entirely in Python using a variety of dependencies.
DTS uses Poetry as its Python dependency management.
Python build/development and runtime environments are the same and DTS development environment,
DTS runtime environment or just plain DTS environment are used interchangeably.


Setting up DTS environment
~~~~~~~~~~~~~~~~~~~~~~~~~~

#. **Python Version**

   The Python Version required by DTS is specified in ``dts/pyproject.toml`` in the
   **[tool.poetry.dependencies]** section:

   .. literalinclude:: ../../../dts/pyproject.toml
      :language: cfg
      :start-at: [tool.poetry.dependencies]
      :end-at: python

   The Python dependency manager DTS uses, Poetry, doesn't install Python, so you may need
   to satisfy this requirement by other means if your Python is not up-to-date.
   A tool such as `Pyenv <https://github.com/pyenv/pyenv>`_ is a good way to get Python,
   though not the only one.

#. **Poetry**

   The typical style of python dependency management, pip with ``requirements.txt``,
   has a few issues.
   The advantages of Poetry include specifying what Python version is required and forcing you
   to specify versions, enforced by a lockfile, both of which help prevent broken dependencies.
   Another benefit is the usage of ``pyproject.toml``, which has become the standard config file
   for python projects, improving project organization.
   To install Poetry, visit their `doc pages <https://python-poetry.org/docs/>`_.

#. **Getting a Poetry shell**

   Once you have Poetry along with the proper Python version all set up, it's just a matter
   of installing dependencies via Poetry and using the virtual environment Poetry provides:

   .. code-block:: console

      poetry install
      poetry shell

#. **SSH Connection**

   DTS uses Python pexpect for SSH connections between DTS environment and the other hosts.
   The pexpect implementation is a wrapper around the ssh command in the DTS environment.
   This means it'll use the SSH agent providing the ssh command and its keys.


Setting up System Under Test
----------------------------

There are two areas that need to be set up on a System Under Test:

#. **DPDK dependencies**

   DPDK will be built and run on the SUT.
   Consult the Getting Started guides for the list of dependencies for each distribution.

#. **Hardware dependencies**

   Any hardware DPDK uses needs a proper driver
   and most OS distributions provide those, but the version may not be satisfactory.
   It's up to each user to install the driver they're interested in testing.
   The hardware also may also need firmware upgrades, which is also left at user discretion.

#. **Hugepages**

   There are two ways to configure hugepages:

   * DTS configuration

     You may specify the optional hugepage configuration in the DTS config file.
     If you do, DTS will take care of configuring hugepages,
     overwriting your current SUT hugepage configuration.

   * System under test configuration

     It's possible to use the hugepage configuration already present on the SUT.
     If you wish to do so, don't specify the hugepage configuration in the DTS config file.


Running DTS
-----------

DTS needs to know which nodes to connect to and what hardware to use on those nodes.
Once that's configured, DTS needs a DPDK tarball and it's ready to run.

Configuring DTS
~~~~~~~~~~~~~~~

DTS configuration is split into nodes and executions and build targets within executions.
By default, DTS will try to use the ``dts/conf.yaml`` config file,
which is a template that illustrates what can be configured in DTS:

  .. literalinclude:: ../../../dts/conf.yaml
     :language: yaml
     :start-at: executions:


The user must be root or any other user with prompt starting with ``#``.
The other fields are mostly self-explanatory
and documented in more detail in ``dts/framework/config/conf_yaml_schema.json``.

DTS Execution
~~~~~~~~~~~~~

DTS is run with ``main.py`` located in the ``dts`` directory after entering Poetry shell::

   usage: main.py [-h] [--config-file CONFIG_FILE] [--output-dir OUTPUT_DIR] [-t TIMEOUT]
                  [-v VERBOSE] [-s SKIP_SETUP] [--tarball TARBALL]
                  [--compile-timeout COMPILE_TIMEOUT] [--test-cases TEST_CASES]
                  [--re-run RE_RUN]

   Run DPDK test suites. All options may be specified with the environment variables provided in
   brackets. Command line arguments have higher priority.

   options:
     -h, --help            show this help message and exit
     --config-file CONFIG_FILE
                           [DTS_CFG_FILE] configuration file that describes the test cases, SUTs
                           and targets. (default: conf.yaml)
     --output-dir OUTPUT_DIR, --output OUTPUT_DIR
                           [DTS_OUTPUT_DIR] Output directory where dts logs and results are
                           saved. (default: output)
     -t TIMEOUT, --timeout TIMEOUT
                           [DTS_TIMEOUT] The default timeout for all DTS operations except for
                           compiling DPDK. (default: 15)
     -v VERBOSE, --verbose VERBOSE
                           [DTS_VERBOSE] Set to 'Y' to enable verbose output, logging all
                           messages to the console. (default: N)
     -s SKIP_SETUP, --skip-setup SKIP_SETUP
                           [DTS_SKIP_SETUP] Set to 'Y' to skip all setup steps on SUT and TG
                           nodes. (default: N)
     --tarball TARBALL, --snapshot TARBALL
                           [DTS_DPDK_TARBALL] Path to DPDK source code tarball which will be
                           used in testing. (default: dpdk.tar.xz)
     --compile-timeout COMPILE_TIMEOUT
                           [DTS_COMPILE_TIMEOUT] The timeout for compiling DPDK. (default: 1200)
     --test-cases TEST_CASES
                           [DTS_TESTCASES] Comma-separated list of test cases to execute.
                           Unknown test cases will be silently ignored. (default: )
     --re-run RE_RUN, --re_run RE_RUN
                           [DTS_RERUN] Re-run each test case the specified amount of times if a
                           test failure occurs (default: 0)


The brackets contain the names of environment variables that set the same thing.
The minimum DTS needs is a config file and a DPDK tarball.
You may pass those to DTS using the command line arguments or use the default paths.


DTS Results
~~~~~~~~~~~

Results are stored in the output dir by default
which be changed with the ``--output-dir`` command line argument.
The results contain basic statistics of passed/failed test cases and DPDK version.


How To Write a Test Suite
-------------------------

All test suites inherit from ``TestSuite`` defined in ``dts/framework/test_suite.py``.
There are four types of methods that comprise a test suite:

#. **Test cases**

   | Test cases are methods that start with a particular prefix.
   | Functional test cases start with ``test_``, e.g. ``test_hello_world_single_core``.
   | Performance test cases start with ``test_perf_``, e.g. ``test_perf_nic_single_core``.
   | A test suite may have any number of functional and/or performance test cases.
     However, these test cases must test the same feature,
     following the rule of one feature = one test suite.
     Test cases for one feature don't need to be grouped in just one test suite, though.
     If the feature requires many testing scenarios to cover,
     the test cases would be better off spread over multiple test suites
     so that each test suite doesn't take too long to execute.

#. **Setup and Teardown methods**

   | There are setup and teardown methods for the whole test suite and each individual test case.
   | Methods ``set_up_suite`` and ``tear_down_suite`` will be executed
     before any and after all test cases have been executed, respectively.
   | Methods ``set_up_test_case`` and ``tear_down_test_case`` will be executed
     before and after each test case, respectively.
   | These methods don't need to be implemented if there's no need for them in a test suite.
     In that case, nothing will happen when they're is executed.

#. **Test case verification**

   Test case verification should be done with the ``verify`` method, which records the result.
   The method should be called at the end of each test case.

#. **Other methods**

   Of course, all test suite code should adhere to coding standards.
   Only the above methods will be treated specially and any other methods may be defined
   (which should be mostly private methods needed by each particular test suite).
   Any specific features (such as NIC configuration) required by a test suite
   should be implemented in the ``SutNode`` class (and the underlying classes that ``SutNode`` uses)
   and used by the test suite via the ``sut_node`` field.


DTS Developer Tools
-------------------

There are three tools used in DTS to help with code checking, style and formatting:

* `isort <https://pycqa.github.io/isort/>`_

  Alphabetically sorts python imports within blocks.

* `black <https://github.com/psf/black>`_

  Does most of the actual formatting (whitespaces, comments, line length etc.)
  and works similarly to clang-format.

* `pylama <https://github.com/klen/pylama>`_

  Runs a collection of python linters and aggregates output.
  It will run these tools over the repository:

  .. literalinclude:: ../../../dts/pyproject.toml
     :language: cfg
     :start-after: [tool.pylama]
     :end-at: linters

These three tools are all used in ``devtools/dts-check-format.sh``,
the DTS code check and format script.
Refer to the script for usage: ``devtools/dts-check-format.sh -h``.
