..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Introduction
============

This document contains instructions for installing and configuring the Data
Plane Development Kit (DPDK) software. The document describes how to compile
and run a DPDK application in a Windows* OS application environment, without
going deeply into detail.

\*Other names and brands may be claimed as the property of others.

Limitations
===========

DPDK for Windows is currently a work in progress. Not all DPDK source files
compile. Support is being added in pieces so as to limit the overall scope
of any individual patch series. The goal is to be able to run any DPDK
application natively on Windows.

The :doc:`../contributing/abi_policy` does not apply to the Windows build,
as function versioning is not supported on Windows,
therefore minor ABI versions may be incompatible.
