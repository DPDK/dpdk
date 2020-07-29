.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 Mellanox Technologies, Ltd

dpdk-test-regex Tool
====================

The ``dpdk-test-regex`` tool is a Data Plane Development Kit (DPDK)
application that allows functional testing and performance measurement for
the RegEx PMDs.
The test supports only one core and one PMD.
It is based on precompiled rule file, and an input file, both of them can
be selected using command-line options.

In general case, each PMD has its own rule file.

The test outputs the following data:

* Performance, in gigabit per second.

* Matching results (rule id, position, length), for each job.

* Matching results in absolute location (rule id, position , length),
  relative to the start of the input data.


Limitations
~~~~~~~~~~~

* Only one queue is supported.

* Supports only precompiled rules.


Application Options
~~~~~~~~~~~~~~~~~~~

``--rules NAME``
  precompiled rule file

``--data NAME``
  data file to use

``--nb_jobs N``
  number of jobs to use

``--perf N``
  only outputs the performance data

``--nb_iter N``
  number of iteration to run

``--help``
  print application options


Running the Tool
----------------

**Step 1: Compile a rule file**

In order for the RegEx to work it must have a precompiled rule file.
to generate this file there is a need to use a RegEx compiler that matches the
RegEx PMD.

**Step 2: Generate a data file**

The data file, will be used as a source data for the RegEx to work on.

**Step 3: Run the tool**

The tool has a number of command line options. Here is the sample command line::

   ./dpdk-test-regex -w 83:00.0 -- --rules rule_file.rof2 --data data_file.txt --job 100
