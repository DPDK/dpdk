.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 Yunsilicon Technology Co., Ltd

XSC Poll Mode Driver
====================

The xsc PMD (**librte_net_xsc**) provides poll mode driver support for
10/25/50/100/200 Gbps Yunsilicon metaScale Series Network Adapters.


Supported NICs
--------------

The following Yunsilicon device models are supported by the same xsc driver:

  - metaScale-200S
  - metaScale-200
  - metaScale-100Q
  - metaScale-50


Prerequisites
-------------

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>`
  to setup the basic DPDK environment.

- Learn about `Yunsilicon metaScale Series NICs
  <https://www.yunsilicon.com/#/productInformation>`_.


Limitations or Known Issues
---------------------------

32-bit architectures are not supported.

Windows and BSD are not supported yet.
