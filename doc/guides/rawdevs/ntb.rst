..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

NTB Rawdev Driver
=================

The ``ntb`` rawdev driver provides a non-transparent bridge between two
separate hosts so that they can communicate with each other. Thus, many
user cases can benefit from this, such as fault tolerance and visual
acceleration.

Build Options
-------------

- ``CONFIG_RTE_LIBRTE_PMD_NTB_RAWDEV`` (default ``y``)

   Toggle compilation of the ``ntb`` driver.

Limitation
----------

- The FIFO hasn't been introduced and will come in 19.11 release.
