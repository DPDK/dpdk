..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2022 PANTHEON.tech s.r.o.

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
--------------------------

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
