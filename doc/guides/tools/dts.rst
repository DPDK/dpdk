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
   The recommended Poetry version is at least 1.5.1.

#. **Getting a Poetry shell**

   Once you have Poetry along with the proper Python version all set up, it's just a matter
   of installing dependencies via Poetry and using the virtual environment Poetry provides:

   .. code-block:: console

      poetry install --no-root
      poetry shell

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
Once that's configured, either a DPDK source code tarball or a Git revision ID
of choice needs to be supplied.
DTS will use this to compile DPDK on the SUT node
and then run the tests with the newly built binaries.


Configuring DTS
~~~~~~~~~~~~~~~

DTS configuration is split into nodes and executions and build targets within executions,
and follows a defined schema as described in `Configuration Schema`_.
By default, DTS will try to use the ``dts/conf.yaml`` :ref:`config file <configuration_schema_example>`,
which is a template that illustrates what can be configured in DTS.

The user must have :ref:`administrator privileges <sut_admin_user>`
which don't require password authentication.


DTS Execution
~~~~~~~~~~~~~

DTS is run with ``main.py`` located in the ``dts`` directory after entering Poetry shell:

.. code-block:: console

   (dts-py3.10) $ ./main.py --help
   usage: main.py [-h] [--config-file CONFIG_FILE] [--output-dir OUTPUT_DIR] [-t TIMEOUT] [-v] [-s] [--tarball TARBALL] [--compile-timeout COMPILE_TIMEOUT] [--test-cases TEST_CASES] [--re-run RE_RUN]

   Run DPDK test suites. All options may be specified with the environment variables provided in brackets. Command line arguments have higher priority.

   options:
   -h, --help            show this help message and exit
   --config-file CONFIG_FILE
                         [DTS_CFG_FILE] configuration file that describes the test cases, SUTs and targets. (default: ./conf.yaml)
   --output-dir OUTPUT_DIR, --output OUTPUT_DIR
                         [DTS_OUTPUT_DIR] Output directory where DTS logs and results are saved. (default: output)
   -t TIMEOUT, --timeout TIMEOUT
                         [DTS_TIMEOUT] The default timeout for all DTS operations except for compiling DPDK. (default: 15)
   -v, --verbose         [DTS_VERBOSE] Specify to enable verbose output, logging all messages to the console. (default: False)
   -s, --skip-setup      [DTS_SKIP_SETUP] Specify to skip all setup steps on SUT and TG nodes. (default: None)
   --tarball TARBALL, --snapshot TARBALL, --git-ref TARBALL
                         [DTS_DPDK_TARBALL] Path to DPDK source code tarball or a git commit ID, tag ID or tree ID to test. To test local changes, first commit them, then use the commit ID with this option. (default: dpdk.tar.xz)
   --compile-timeout COMPILE_TIMEOUT
                         [DTS_COMPILE_TIMEOUT] The timeout for compiling DPDK. (default: 1200)
   --test-cases TEST_CASES
                         [DTS_TESTCASES] Comma-separated list of test cases to execute. Unknown test cases will be silently ignored. (default: )
   --re-run RE_RUN, --re_run RE_RUN
                         [DTS_RERUN] Re-run each test case the specified number of times if a test failure occurs (default: 0)


The brackets contain the names of environment variables that set the same thing.
The minimum DTS needs is a config file and a DPDK tarball or git ref ID.
You may pass those to DTS using the command line arguments or use the default paths.

Example command for running DTS with the template configuration and DPDK tag v23.11:

.. code-block:: console

   (dts-py3.10) $ ./main.py --git-ref v23.11


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

When adding code to the DTS framework, pay attention to the rest of the code
and try not to divert much from it.
The :ref:`DTS developer tools <dts_dev_tools>` will issue warnings
when some of the basics are not met.

The code must be properly documented with docstrings.
The style must conform to the `Google style
<https://google.github.io/styleguide/pyguide.html#38-comments-and-docstrings>`_.
See an example of the style `here
<https://www.sphinx-doc.org/en/master/usage/extensions/example_google.html>`_.
For cases which are not covered by the Google style, refer to `PEP 257
<https://peps.python.org/pep-0257/>`_.
There are some cases which are not covered by the two style guides,
where we deviate or where some additional clarification is helpful:

   * The ``__init__()`` methods of classes are documented separately
     from the docstring of the class itself.
   * The docstrings of implemented abstract methods should refer to the superclass's definition
     if there's no deviation.
   * Instance variables/attributes should be documented in the docstring of the class
     in the ``Attributes:`` section.
   * The ``dataclass.dataclass`` decorator changes how the attributes are processed.
     The dataclass attributes which result in instance variables/attributes
     should also be recorded in the ``Attributes:`` section.
   * Class variables/attributes, on the other hand, should be documented with ``#:``
     above the type annotated line.
     The description may be omitted if the meaning is obvious.
   * The ``Enum`` and ``TypedDict`` also process the attributes in particular ways
     and should be documented with ``#:`` as well.
     This is mainly so that the autogenerated documentation contains the assigned value.
   * When referencing a parameter of a function or a method in their docstring,
     don't use any articles and put the parameter into single backticks.
     This mimics the style of `Python's documentation <https://docs.python.org/3/index.html>`_.
   * When specifying a value, use double backticks::

        def foo(greet: bool) -> None:
            """Demonstration of single and double backticks.

            `greet` controls whether ``Hello World`` is printed.

            Args:
               greet: Whether to print the ``Hello World`` message.
            """
            if greet:
               print(f"Hello World")

   * The docstring maximum line length is the same as the code maximum line length.


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
     In that case, nothing will happen when they are executed.

#. **Configuration, traffic and other logic**

   The ``TestSuite`` class contains a variety of methods for anything that
   a test suite setup, a teardown, or a test case may need to do.

   The test suites also frequently use a DPDK app, such as testpmd, in interactive mode
   and use the interactive shell instances directly.

   These are the two main ways to call the framework logic in test suites.
   If there's any functionality or logic missing from the framework,
   it should be implemented so that the test suites can use one of these two ways.

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


.. _dts_dev_tools:

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

* `mypy <https://github.com/python/mypy>`_

  Enables static typing for Python, exploiting the type hints in the source code.

These three tools are all used in ``devtools/dts-check-format.sh``,
the DTS code check and format script.
Refer to the script for usage: ``devtools/dts-check-format.sh -h``.


Configuration Schema
--------------------

Definitions
~~~~~~~~~~~

_`Node name`
   *string* – A unique identifier for a node.
   **Examples**: ``SUT1``, ``TG1``.

_`ARCH`
   *string* – The CPU architecture.
   **Supported values**: ``x86_64``, ``arm64``, ``ppc64le``.

_`CPU`
   *string* – The CPU microarchitecture. Use ``native`` for x86.
   **Supported values**: ``native``, ``armv8a``, ``dpaa2``, ``thunderx``, ``xgene1``.

_`OS`
   *string* – The operating system. **Supported values**: ``linux``.

_`Compiler`
   *string* – The compiler used for building DPDK.
   **Supported values**: ``gcc``, ``clang``, ``icc``, ``mscv``.

_`Build target`
   *mapping* – Build targets supported by DTS for building DPDK, described as:

   ==================== =================================================================
   ``arch``             See `ARCH`_
   ``os``               See `OS`_
   ``cpu``              See `CPU`_
   ``compiler``         See `Compiler`_
   ``compiler_wrapper`` *string* – Value prepended to the CC variable for the DPDK build.

                        **Example**: ``ccache``
   ==================== =================================================================

_`hugepages`
   *mapping* – hugepages described as:

   ==================== ================================================================
   ``amount``           *integer* – The amount of hugepages to configure.

                        Hugepage size will be the system default.
   ``force_first_numa`` (*optional*, defaults to ``false``) – If ``true``, it forces the

                        configuration of hugepages on the first NUMA node.
   ==================== ================================================================

_`Network port`
   *mapping* – the NIC port described as:

   ====================== =================================================================================
   ``pci``                *string* – the local PCI address of the port. **Example**: ``0000:00:08.0``
   ``os_driver_for_dpdk`` | *string* – this port's device driver when using with DPDK
                          | When setting up the SUT, DTS will bind the network device to this driver
                          | for compatibility with DPDK.

                          **Examples**: ``vfio-pci``, ``mlx5_core``
   ``os_driver``          | *string* – this port's device driver when **not** using with DPDK
                          | When tearing down the tests on the SUT, DTS will bind the network device
                          | *back* to this driver. This driver is meant to be the one that the SUT would
                          | normally use for this device, or whichever driver it is preferred to leave the
                          | device bound to after testing.
                          | This also represents the driver that is used in conjunction with the traffic
                          | generator software.

                          **Examples**: ``i40e``, ``mlx5_core``
   ``peer_node``          *string* – the name of the peer node connected to this port.
   ``peer_pci``           *string* – the PCI address of the peer node port. **Example**: ``000a:01:00.1``
   ====================== =================================================================================

_`Test suite`
   *string* – name of the test suite to run. **Examples**: ``hello_world``, ``os_udp``

_`Test target`
   *mapping* – selects specific test cases to run from a test suite. Mapping is described as follows:

   ========= ===============================================================================================
   ``suite`` See `Test suite`_
   ``cases`` (*optional*) *sequence* of *string* – list of the selected test cases in the test suite to run.

             Unknown test cases will be silently ignored.
   ========= ===============================================================================================


Properties
~~~~~~~~~~

The configuration requires listing all the execution environments and nodes
involved in the testing. These can be defined with the following mappings:

``executions``
   `sequence <https://docs.python.org/3/library/stdtypes.html#sequence-types-list-tuple-range>`_ listing
   the execution environments. Each entry is described as per the following
   `mapping <https://docs.python.org/3/library/stdtypes.html#mapping-types-dict>`_:

   +----------------------------+-------------------------------------------------------------------+
   | ``build_targets``          | *sequence* of `Build target`_                                     |
   +----------------------------+-------------------------------------------------------------------+
   | ``perf``                   | *boolean* – Enable performance testing.                           |
   +----------------------------+-------------------------------------------------------------------+
   | ``func``                   | *boolean* – Enable functional testing.                            |
   +----------------------------+-------------------------------------------------------------------+
   | ``test_suites``            | *sequence* of **one of** `Test suite`_ **or** `Test target`_      |
   +----------------------------+-------------------------------------------------------------------+
   | ``skip_smoke_tests``       | (*optional*) *boolean* – Allows you to skip smoke testing         |
   |                            | if ``true``.                                                      |
   +----------------------------+-------------------------------------------------------------------+
   | ``system_under_test_node`` | System under test node specified with:                            |
   |                            +---------------+---------------------------------------------------+
   |                            | ``node_name`` | See `Node name`_                                  |
   |                            +---------------+---------------------------------------------------+
   |                            | ``vdevs``     | (*optional*) *sequence* of *string*               |
   |                            |               |                                                   |
   |                            |               | List of virtual devices passed with the ``--vdev``|
   |                            |               | argument to DPDK. **Example**: ``crypto_openssl`` |
   +----------------------------+---------------+---------------------------------------------------+
   | ``traffic_generator_node`` | Node name for the traffic generator node.                         |
   +----------------------------+-------------------------------------------------------------------+

``nodes``
   `sequence <https://docs.python.org/3/library/stdtypes.html#sequence-types-list-tuple-range>`_ listing
   the nodes. Each entry is described as per the following
   `mapping <https://docs.python.org/3/library/stdtypes.html#mapping-types-dict>`_:

   +-----------------------+---------------------------------------------------------------------------------------+
   | ``name``              | See `Node name`_                                                                      |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``hostname``          | *string* – The network hostname or IP address of this node.                           |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``user``              | *string* – The SSH user credential to use to login to this node.                      |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``password``          | (*optional*) *string* – The SSH password credential for this node.                    |
   |                       |                                                                                       |
   |                       | **NB**: Use only as last resort. SSH keys are **strongly** preferred.                 |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``arch``              | The architecture of this node. See `ARCH`_ for supported values.                      |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``os``                | The operating system of this node. See `OS`_ for supported values.                    |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``lcores``            | | (*optional*, defaults to 1) *string* – Comma-separated list of logical              |
   |                       | | cores to use. An empty string means use all lcores.                                 |
   |                       |                                                                                       |
   |                       | **Example**: ``1,2,3,4,5,18-22``                                                      |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``use_first_core``    | (*optional*, defaults to ``false``) *boolean*                                         |
   |                       |                                                                                       |
   |                       | Indicates whether DPDK should use only the first physical core or not.                |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``memory_channels``   | (*optional*, defaults to 1) *integer*                                                 |
   |                       |                                                                                       |
   |                       | The number of the memory channels to use.                                             |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``hugepages``         | (*optional*) See `hugepages`_. If unset, hugepages won't be configured                |
   |                       |                                                                                       |
   |                       | in favour of the system configuration.                                                |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``ports``             | | *sequence* of `Network port`_ – Describe ports that are **directly** paired with    |
   |                       | | other nodes used in conjunction with this one. Both ends of the links must be       |
   |                       | | described. If there any inconsistencies DTS won't run.                              |
   |                       |                                                                                       |
   |                       | **Example**: port 1 of node ``SUT1`` is connected to port 1 of node ``TG1`` etc.      |
   +-----------------------+---------------------------------------------------------------------------------------+
   | ``traffic_generator`` | (*optional*) Traffic generator, if any, setup on this node described as:              |
   |                       +----------+----------------------------------------------------------------------------+
   |                       | ``type`` | *string* – **Supported values**: *SCAPY*                                   |
   +-----------------------+----------+----------------------------------------------------------------------------+


.. _configuration_schema_example:

Example
~~~~~~~

The following example (which can be found in ``dts/conf.yaml``) sets up two nodes:

* ``SUT1`` which is already setup with the DPDK build requirements and any other
  required for execution;
* ``TG1`` which already has Scapy installed in the system.

And they both have two network ports which are physically connected to each other.

.. note::
   This example assumes that you have setup SSH keys in both the system under test
   and traffic generator nodes.

.. literalinclude:: ../../../dts/conf.yaml
   :language: yaml
   :start-at: executions:
