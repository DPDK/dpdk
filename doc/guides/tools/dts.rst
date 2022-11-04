..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2022 PANTHEON.tech s.r.o.

DPDK Test Suite
===============

The DPDK Test Suite, abbreviated DTS, is a Python test framework with test suites
implementing functional and performance tests used to test DPDK.


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
