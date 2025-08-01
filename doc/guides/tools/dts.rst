..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
    Copyright(c) 2024 Arm Limited
    Copyright(c) 2025 University of New Hampshire

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
  This is the node where we run DTS and from which DTS connects to other
  nodes.

System under test
  The system which runs a DPDK application on relevant
  hardware (NIC, accelerator cards, etc) and from which the DPDK behavior is
  observed for tests.

System under test node
  A node where at least one SUT is present.

Traffic generator
  Node that sends traffic to the SUT, which can be hardware or software-based.

Traffic generator node
  A node where at least one TG is present.


DTS Environment
---------------

DTS is written entirely in Python using a variety of dependencies.
DTS uses Poetry as its Python dependency management.
Python build/development and runtime environments are the same and DTS development environment,
DTS runtime environment or just plain DTS environment are used interchangeably.

.. _dts_deps:

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
   The recommended Poetry version is at least 1.8.2.

#. **Running DTS with Poetry**

   Once you have Poetry along with the proper Python version all set up, it's just a matter
   of installing dependencies via Poetry and running main.py:

   .. code-block:: console

      poetry install
      poetry run ./main.py

#. **SSH Connection**

   DTS uses the Fabric Python library for SSH connections between DTS environment
   and the other hosts.
   The authentication method used is pubkey authentication.
   Fabric tries to use a passed key/certificate,
   then any key it can with through an SSH agent,
   then any "id_rsa", "id_dsa" or "id_ecdsa" key discoverable in ``~/.ssh/``
   (with any matching OpenSSH-style certificates).
   DTS doesn't pass any keys, so Fabric tries to use the other two methods.


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
     Configuration of hugepages via DTS allows only for allocation of 2MB hugepages,
     as doing so prevents accidental/over allocation of hugepage sizes
     not recommended during runtime due to contiguous memory space requirements.
     Thus, if you require hugepage sizes not equal to 2MB,
     then this configuration must be done outside of the DTS framework.

   * System under test configuration

     It's possible to use the hugepage configuration already present on the SUT.
     If you wish to do so, don't specify the hugepage configuration in the DTS config file.

#. **User with administrator privileges**

.. _sut_admin_user:

   DTS needs administrator privileges to run DPDK applications (such as testpmd) on the SUT.
   The SUT user must be able run commands in privileged mode without asking for password.
   On most Linux distributions, it's a matter of setting up passwordless sudo:

   #. Run ``sudo visudo`` and check that it contains ``%sudo	ALL=(ALL:ALL) NOPASSWD:ALL``.

   #. Add the SUT user to the sudo group with:

   .. code-block:: console

      sudo usermod -aG sudo <sut_user>

#. **SR-IOV**

   Before configuring virtual functions, SR-IOV support must be enabled in the system BIOS/UEFI:

   #. Reboot the system and enter BIOS/UEFI settings.
   #. Locate the SR-IOV option (often under PCIe or Advanced settings).
   #. Set SR-IOV to **Enabled** and save changes.

   For Mellanox environments, the following additional setup steps are required:

   #. **Install mstflint tools** (if not already installed):

      .. code-block:: bash

         sudo apt install mstflint        # On Debian/Ubuntu
         sudo yum install mstflint        # On RHEL/CentOS

   #. **Start the MST service**:

      .. code-block:: bash

         sudo mst start

   #. **List Mellanox devices**:

      .. code-block:: bash

         sudo mst status

      This will output paths such as ``/dev/mst/mt4121_pciconf0``
      and possibly additional functions (e.g., ``pciconf0.1``).

   #. **Enable SR-IOV and configure number of VFs**:

      .. code-block:: bash

         sudo mlxconfig -d /dev/mst/mt4121_pciconf0 set SRIOV_EN=1 NUM_OF_VFS=8
         sudo mlxconfig -d /dev/mst/mt4121_pciconf0.1 set SRIOV_EN=1 NUM_OF_VFS=8

      Replace the device names with those matching your setup (from ``mst status``).
      The number of VFs can be adjusted as needed.

   #. **Reboot the system**:

      .. code-block:: bash

         sudo reboot now


Setting up Traffic Generator Node
---------------------------------

These need to be set up on a Traffic Generator Node:

#. **Traffic generator dependencies**

   The traffic generator running on the traffic generator node must be installed beforehand.
   For Scapy traffic generator, only a few Python libraries need to be installed:

   .. code-block:: console

      sudo apt install python3-pip
      sudo pip install --upgrade pip
      sudo pip install scapy==2.5.0

#. **Hardware dependencies**

   The traffic generators, like DPDK, need a proper driver and firmware.
   The Scapy traffic generator doesn't have strict requirements - the drivers that come
   with most OS distributions will be satisfactory.


#. **User with administrator privileges**

   Similarly to the System Under Test, traffic generators need administrator privileges
   to be able to use the devices.
   Refer to the `System Under Test section <sut_admin_user>` for details.


Running DTS
-----------

DTS needs to know which nodes to connect to and what hardware to use on those nodes.
Once that's configured, either a DPDK source code tarball or tree folder
need to be supplied whether these are on your DTS host machine or the SUT node.
DTS can accept a pre-compiled build placed in a subdirectory,
or it will compile DPDK on the SUT node,
and then run the tests with the newly built binaries.


Configuring DTS
~~~~~~~~~~~~~~~

DTS configuration is split into nodes and a test run,
and must respect the model definitions
as documented in the DTS API docs under the ``config`` page.
The root of the configuration is represented by the ``Configuration`` model.
By default, DTS will try to use the ``dts/test_run.example.yaml``
:ref:`config file <test_run_configuration_example>`,
and ``dts/nodes.example.yaml``
:ref:`config file <nodes_configuration_example>`
which are templates that illustrate what can be configured in DTS.

The user must have :ref:`administrator privileges <sut_admin_user>`
which don't require password authentication.


DTS Execution
~~~~~~~~~~~~~

DTS is run with ``main.py`` located in the ``dts`` directory using the ``poetry run`` command:

.. code-block:: console

   (dts-py3.10) $ ./main.py --help
   usage: main.py [-h] [--test-run-config-file FILE_PATH] [--nodes-config-file FILE_PATH] [--tests-config-file FILE_PATH]
                  [--output-dir DIR_PATH] [-t SECONDS] [-v] [--dpdk-tree DIR_PATH | --tarball FILE_PATH] [--remote-source]
                  [--precompiled-build-dir DIR_NAME] [--compile-timeout SECONDS] [--test-suite TEST_SUITE [TEST_CASES ...]]
                  [--re-run N_TIMES] [--random-seed NUMBER]

   Run DPDK test suites. All options may be specified with the environment variables provided in brackets. Command line arguments have higher
   priority.

   options:
     -h, --help            show this help message and exit
     --test-run-config-file FILE_PATH
                           [DTS_TEST_RUN_CFG_FILE] The configuration file that describes the test cases and DPDK build options. (default: test-run.conf.yaml)
     --nodes-config-file FILE_PATH
                           [DTS_NODES_CFG_FILE] The configuration file that describes the SUT and TG nodes. (default: nodes.conf.yaml)
     --tests-config-file FILE_PATH
                           [DTS_TESTS_CFG_FILE] Configuration file used to override variable values inside specific test suites. (default: None)
     --output-dir DIR_PATH, --output DIR_PATH
                           [DTS_OUTPUT_DIR] Output directory where DTS logs and results are saved. (default: output)
     -t SECONDS, --timeout SECONDS
                           [DTS_TIMEOUT] The default timeout for all DTS operations except for compiling DPDK. (default: 15)
     -v, --verbose         [DTS_VERBOSE] Specify to enable verbose output, logging all messages to the console. (default: False)
     --compile-timeout SECONDS
                           [DTS_COMPILE_TIMEOUT] The timeout for compiling DPDK. (default: 1200)
     --test-suite TEST_SUITE [TEST_CASES ...]
                           [DTS_TEST_SUITES] A list containing a test suite with test cases. The first parameter is the test suite name, and
                           the rest are test case names, which are optional. May be specified multiple times. To specify multiple test suites
                           in the environment variable, join the lists with a comma. Examples: --test-suite suite case case --test-suite
                           suite case ... | DTS_TEST_SUITES='suite case case, suite case, ...' | --test-suite suite --test-suite suite case
                           ... | DTS_TEST_SUITES='suite, suite case, ...' (default: [])
     --re-run N_TIMES, --re_run N_TIMES
                           [DTS_RERUN] Re-run each test case the specified number of times if a test failure occurs. (default: 0)
     --random-seed NUMBER  [DTS_RANDOM_SEED] The seed to use with the pseudo-random generator. If not specified, the configuration value is
                           used instead. If that's also not specified, a random seed is generated. (default: None)

   DPDK Build Options:
     Arguments in this group (and subgroup) will be applied to a DPDKLocation when the DPDK tree, tarball or revision will be provided,
     other arguments like remote source and build dir are optional. A DPDKLocation from settings are used instead of from config if
     construct successful.

     --dpdk-tree DIR_PATH  [DTS_DPDK_TREE] The path to the DPDK source tree directory to test. Cannot be used in conjunction with --tarball.
                           (default: None)
     --tarball FILE_PATH, --snapshot FILE_PATH
                           [DTS_DPDK_TARBALL] The path to the DPDK source tarball to test. DPDK must be contained in a folder with the same
                           name as the tarball file. Cannot be used in conjunction with --dpdk-tree. (default: None)
     --remote-source       [DTS_REMOTE_SOURCE] Set this option if either the DPDK source tree or tarball to be used are located on the SUT
                           node. Can only be used with --dpdk-tree or --tarball. (default: False)
     --precompiled-build-dir DIR_NAME
                           [DTS_PRECOMPILED_BUILD_DIR] Define the subdirectory under the DPDK tree root directory or tarball where the pre-
                           compiled binaries are located. (default: None)


The brackets contain the names of environment variables that set the same thing.
The minimum DTS needs is a config file and a pre-built DPDK
or DPDK sources location which can be specified in said config file
or on the command line or environment variables.


DTS Results
~~~~~~~~~~~

Results are stored in the output dir by default
which be changed with the ``--output-dir`` command line argument.
The results contain basic statistics of passed/failed test cases and DPDK version.


Contributing to DTS
-------------------

There are two areas of contribution: The DTS framework and DTS test suites.

The framework contains the logic needed to run test cases, such as connecting to nodes,
running DPDK applications and collecting results.

The test cases call APIs from the framework to test their scenarios.
Adding test cases may require adding code to the framework as well.


Framework Coding Guidelines
~~~~~~~~~~~~~~~~~~~~~~~~~~~

When contributing code to the DTS framework, follow existing conventions to ensure consistency.
The :ref:`DTS developer tools <dts_dev_tools>` will flag basic issues.
Also, be sure to :ref:`build the API documentation <building_api_docs>` to catch any problems during the build.

The API documentation is a helpful reference during development.
It can be viewed in the code directly or generated using the :ref:`API docs build steps <building_api_docs>`.
If you add new files or change the directory structure, update the corresponding sources in ``doc/api/dts``.

Code must be documented with docstrings that follow the
`Google style <https://google.github.io/styleguide/pyguide.html#38-comments-and-docstrings>`_.
Additional references:

* `Sphinx Google style example <https://www.sphinx-doc.org/en/master/usage/extensions/example_google.html>`_
* `PEP 257 <https://peps.python.org/pep-0257/>`_

Docstring and Attribute Guidelines

* Document ``__init__()`` separately from the class docstring.
* If an abstract method simply implements a superclass definition without changes, refer to that superclass in the docstring.
* Document instance variables in the class docstring under an ``Attributes:`` section.
* For ``@dataclass`` classes, document instance-level attributes in ``Attributes:``, as they are generated from the class fields.
* Document class variables and Pydantic fields using ``#:``,
   placed above the type-annotated line. Descriptions may be omitted if the meaning is clear.
* Apply ``#:`` to ``Enum`` and ``TypedDict`` fields as well, so that autogenerated documentation includes their values.
* When referring to a parameter in a docstring, omit articles and enclose the parameter in single backticks,
   consistent with the `Python documentation style <https://docs.python.org/3/index.html>`_.
* Use double backticks for literal values.

Example::

   def foo(greet: bool) -> None:
       """Demonstrates single and double backticks.

       `greet` controls whether ``Hello World`` is printed.

       Args:
           greet: Whether to print the ``Hello World`` message.
       """
       if greet:
           print("Hello World")

The maximum line length for docstrings must match that of the code.


How To Write a Test Suite
-------------------------

All test suites are classes that inherit from TestSuite, defined in dts/framework/test_suite.py. A typical suite contains:

Test Cases

   Test cases are defined as methods and must be decorated appropriately.
   Use the @func_test and/or @perf_test decorators from TestSuite above each test case method.
   For example:

   Example::

      @func_test
      def test_basic_link(self):
         """your testcase docstring here"""
         # your testcase code here

   Functional test cases should use the @func_test decorator, and performance test cases should use @perf_test.
   A test suite may include any number of functional and/or performance test cases.
   Each suite should focus on testing a single feature (one feature = one test suite).

Setup and Teardown Hooks

   Setup and teardown methods can be defined at both the suite and test case levels.

   Suite-level:

   * set_up_suite() — runs once before any test cases in the suite

   * tear_down_suite() — runs once after all test cases have completed

   Case-level:

   * set_up_test_case() — runs before each individual test case

   * tear_down_test_case() — runs after each individual test case

   These methods are optional. If not implemented, the framework will simply skip them.

   The TestSuite class provides a variety of methods for setup, teardown, and test logic.
   Test suites often use DPDK applications (e.g., testpmd) in interactive mode and interact with them via shell instances.

Leveraging the DTS framework in writing testsuites:

   One should avoid directly importing DTS framework code to their
   testsuites where possible. Instead, for performing common processes
   required in testsuites, one should use (or add to) the list of methods
   provided in the Testsuite class (the base class of all testsuites). For
   instance, for sending a list of packets, one should work through the packet
   transmitting function already made available in the TestSuite class,
   instead of directly importing the DTS traffic generator class and using
   that class in one's testsuite implementation. It is also acceptable to
   import and instantiate classes for various DPDK applications. For instance,
   writing a testsuite for a simple packet forwarding operation would involve
   importing the DTS TestPmdShell class, instantiating TestPmdShell, calling
   TestPmdShell's start() method, and then sending traffic via one of the
   traffic transmitting functions exposed in the Testsuite class.

Test Case Verification

   Use the verify method to assert conditions and record test results.
   This should typically be called at the end of each test case.
   Example: self.verify(link_up, "Link should be up after configuration.")

Other Methods

   All test suite code should follow the project's coding standards.
   Only test cases, setup/teardown hooks, and verification methods are treated specially by the framework.
   Additional private methods may be added as needed in your testsuite implementation.


.. _dts_dev_tools:

DTS Developer Tools
-------------------

There are two tools used in DTS to help with code checking, style and formatting:

ruff:
  - Linter and formatter (replaces flake8, pylint, isort, etc.)
  - Compatible with Black

mypy:
  - Performs static type checking

These two tools are all used in ``devtools/dts-check-format.sh``,
the DTS code check and format script.
Refer to the script for usage: ``devtools/dts-check-format.sh -h``.


.. _building_api_docs:

Building DTS API docs
---------------------

The documentation is built using the standard DPDK build system.
See :doc:`../linux_gsg/build_dpdk` for more details on compiling DPDK with meson.

After executing the meson command, build the documentation with:

.. code-block:: console

   ninja -C build doc

The output is generated in ``build/doc/api/dts/html``.

.. note::

   Make sure to fix any Sphinx warnings when adding or updating docstrings.

.. _configuration_example:

Configuration Example
---------------------

The following example configuration files sets up two nodes:

* ``SUT1`` which is already setup with the DPDK build requirements and any other
  required for execution;
* ``TG1`` which already has Scapy installed in the system.

And they both have two network ports which are physically connected to each other.

.. note::
   This example assumes that you have setup SSH keys in both the system under test
   and traffic generator nodes.

.. _test_run_configuration_example:

``dts/test_run.example.yaml``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. literalinclude:: ../../../dts/test_run.example.yaml
   :language: yaml
   :start-at: # Define

.. _nodes_configuration_example:


``dts/nodes.example.yaml``
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. literalinclude:: ../../../dts/nodes.example.yaml
   :language: yaml
   :start-at: # Define

Additionally, an example configuration file is provided
to demonstrate custom test suite configuration:

.. note::
   You do not need to supply configurations for all test suites,
   and not all test suites will support or need additional configuration.

.. _tests_config_example:

``dts/tests_config.example.yaml``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. literalinclude:: ../../../dts/tests_config.example.yaml
   :language: yaml
   :start-at: # Define
