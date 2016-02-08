..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Introduction
============

This document contains instructions for installing and configuring the Data Plane Development Kit (DPDK) software.
It is designed to get customers up and running quickly.
The document describes how to compile and run a DPDK application in a Linux application (linuxapp) environment,
without going deeply into detail.

Documentation Roadmap
---------------------

The following is a list of DPDK documents in the suggested reading order:

*   Release Notes: Provides release-specific information, including supported features, limitations, fixed issues, known issues and so on.
    Also, provides the answers to frequently asked questions in FAQ format.

*   Getting Started Guide (this document): Describes how to install and configure the DPDK; designed to get users up and running quickly with the software.

*   Programmer's Guide: Describes:

    *   The software architecture and how to use it (through examples), specifically in a Linux application (linuxapp) environment

    *   The content of the DPDK, the build system (including the commands that can be used in the root DPDK Makefile to build the development kit and
        an application) and guidelines for porting an application

    *   Optimizations used in the software and those that should be considered for new development

    A glossary of terms is also provided.

*   API Reference: Provides detailed information about DPDK functions, data structures and other programming constructs.

*   Sample Applications User Guide: Describes a set of sample applications.
    Each chapter describes a sample application that showcases specific functionality and provides instructions on how to compile, run and use the sample application.
