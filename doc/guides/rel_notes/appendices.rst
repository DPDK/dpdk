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

Appendix A  Intel®  DPDK License Overview
=========================================


The following describes the various licenses used by the Intel® Data Plane Development Kit (Intel® DPDK).
The purpose of the Intel® DPDK is to prove the abilities of the Intel® architecture processors and to provide users with a strong set of examples, libraries and proof points.
By placing the majority of this software under the BSD License, users may choose to use the Intel® as is, parts of it, or just the ideas for their programs.
All code may be modified by the user to suit their project needs and requirements.

.. note::

    The license in each source file takes precedence over this document, and should be used as the definitive license for that file.
    All users should seek their legal team's guidance with respect to the licensing used by the Intel® DPDK.



The following table lists those files (or libraries) that are not under a BSD License. In some cases, these files are part of the standard Intel® DPDK release package,
and in other cases may be a separate package that requires a separate download to be added to the Intel® DPDK. This document spells out those cases where possible.

The sections following the table provide the various licenses used. Please note that copyright notices may change overtime.
It is the responsibility of all users to understand these licenses and seek their legal team's guidance.

The use of the GPLv2 License is confined to files in kernel loadable modules.

The use of the Dual BSD/LGPLv2 License and Dual BSD/GPL License allows use with either free/open source software or with proprietary software in userspace.


+---------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+--------------------------------------+
| File                                              | Description                                                                                                          | License                              |
|                                                   |                                                                                                                      |                                      |
+===================================================+======================================================================================================================+======================================+
| igb_uio.c                                         | **1st Released**                                                                                                     | GPLv2 License Information            |
|                                                   | : 1.0                                                                                                                |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Location**                                                                                                         |                                      |
|                                                   | :                                                                                                                    |                                      |
|                                                   | DPDK/lib/librte_eal/linuxapp/igb_uio/                                                                                |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Description**                                                                                                      |                                      |
|                                                   | : This file is used for a kernel loadable module which is responsible for unbinding NICs from the Linux kernel       |                                      |
|                                                   | and binding them to the Intel® DPDK                                                                                  |                                      |
|                                                   |                                                                                                                      |                                      |
+---------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+--------------------------------------+
| kni_dev.h kni_ethtool.c kni_misc.c kni_net.c      | **1st Released**                                                                                                     | GPLv2 License Information            |
|                                                   | : 1.3                                                                                                                |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Location**                                                                                                         |                                      |
|                                                   | : DPDK/lib/librte_eal/linuxapp/kni/                                                                                  |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Description**                                                                                                      |                                      |
|                                                   | : The KNI kernel loadable module is a standard net driver which allows Intel DPDK Linux userspace applications       |                                      |
|                                                   | to exchange packets/data with the Linux kernel.                                                                      |                                      |
|                                                   |                                                                                                                      |                                      |
+---------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+--------------------------------------+
| rte_kni_common.h                                  | **1st Released**                                                                                                     | Dual BSD/LGPLv2 License Information  |
|                                                   | : 1.3                                                                                                                |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Location**                                                                                                         |                                      |
|                                                   | : DPDK/lib/librte_eal/linuxapp/eal/include/exec-env/                                                                 |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Description**                                                                                                      |                                      |
|                                                   | : The KNI header files is utilized by both the Intel DPDK userspace application and the KNI kernel loadable module   |                                      |
|                                                   |                                                                                                                      |                                      |
+---------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+--------------------------------------+
| All files under this directory are for the        | **1st Released**                                                                                                     | GPLv2 License Information            |
| ethtool functionality.                            | : 1.3                                                                                                                |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Location**                                                                                                         |                                      |
|                                                   | : DPDK/lib/librte_eal/linuxapp/kni/ethtool                                                                           |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Description**                                                                                                      |                                      |
|                                                   | : The igb and ixgbe drivers for the Ethtool function available with the KNI kernel loadable.                         |                                      |
|                                                   |                                                                                                                      |                                      |
+---------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+--------------------------------------+
| rte_pci_dev_ids.h                                 | **1st Released**                                                                                                     | Dual BSD/GPL License Information     |
|                                                   | : 1.3                                                                                                                |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Location**                                                                                                         |                                      |
|                                                   | : DPDK/lib/librte_eal/common/include                                                                                 |                                      |
|                                                   |                                                                                                                      |                                      |
|                                                   | **Description**                                                                                                      |                                      |
|                                                   | : Contains the PCI device ids for all devices.                                                                       |                                      |
|                                                   |                                                                                                                      |                                      |
+---------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+--------------------------------------+

Added rte_dom0_mm.h file for Dual BSD/LGPLv2 License information:

rte_dom0_mm.h 1st Released 1.6.0

Location: DPDK/lib.librte_eal/eal/include/exec-env/

Description: The Dom0 header file is utilized by both the Intel® DPDK userspace application and the Dom0 kernel loadable module

Added dom0_mm_misc.h and dom0_mm_misc.c file for Dual BSD/GPLv2 License Information:

dom0_mm_dev.h 1st Released 1.6.0

dom0_mm_dev.c Location: DPDK/lib.librte_eal/linuxapp/xen_dom0

Description: The Dom0 memory management kernel loadable module is a misc device driver which is used to facilitate allocating and mapping via an **IOCTL** (allocation) and **MMAP** (mapping).


A.1 BSD License
---------------

.. code-block:: c

    /*-
     *        BSD LICENSE
     *
     *        Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
     *        All rights reserved.
     *
     *        Redistribution and use in source and binary forms, with or without
     *        modification, are permitted provided that the following conditions
     *        are met:
     *
     *        * Redistributions of source code must retain the above copyright
     *        notice, this list of conditions and the following disclaimer.
     *        * Redistributions in binary form must reproduce the above copyright
     *        notice, this list of conditions and the following disclaimer in
     *        the documentation and/or other materials provided with the
     *        distribution.
     *        * Neither the name of Intel Corporation nor the names of its
     *        contributors may be used to endorse or promote products derived
     *        from this software without specific prior written permission.
     *
     *        THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
     *        "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
     *        LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
     *        A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
     *        OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
     *        SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
     *        LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
     *        DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
     *        THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
     *        (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
     *        OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
     *
     */

A.2 GPLv2 License Information
-----------------------------

.. code-block:: c

    /*-
     *
     *        Copyright (c) 2010-2012, Intel Corporation
     *
     *        This program is free software; you can redistribute it and/or
     *        modify it under the terms of the GNU General Public License
     *        as published by the Free Software Foundation; either version 2
     *        of the License, or (at your option) any later version.
     *
     *        This program is distributed in the hope that it will be useful,
     *        but WITHOUT ANY WARRANTY; without even the implied warranty of
     *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
     *        GNU General Public License for more details.
     *
     *        You should have received a copy of the GNU General Public License
     *        along with this program; if not, write to the Free Software
     *        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
     *
..

    \*        GNU GPL V2: `http://www.gnu.org/licenses/old-licenses/gpl-2.0.html <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>`_

.. code-block:: c

     *
     */

A.3 Dual BSD/LGPLv2 License Information
---------------------------------------

.. code-block:: c

    /*
    This file is provided under a dual BSD/LGPLv2 license. When using
    or redistributing this file, you may do so under either license.

    GNU LESSER GENERAL PUBLIC LICENSE

    Copyright(c) 2007,2008,2009 Intel Corporation. All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2.1 of the GNU Lesser General Public License
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
    PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.


    Contact Information: Intel Corporation

    BSD LICENSE

    Copyright(c) 2010-2012 Intel Corporation.All rights reserved. All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are
    permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this list of
    conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice, this list
    of conditions and the following disclaimer in the documentation and/or other
    materials provided with the distribution.
    Neither the name of Intel Corporation nor the names of its contributors may be used
    to endorse or promote products derived from this software without specific prior
    written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
    SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
    TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
    DAMAGE.
    */


A.4 Dual BSD/GPL License Information
------------------------------------

.. code-block:: c

    /*-
     *       This file is provided under a dual BSD/GPLv2 license. When using or
     *       redistributing this file, you may do so under either license.
     *
     *       GPL LICENSE SUMMARY
     *
     *       Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
     *
     *       This program is free software; you can redistribute it and/or modify
     *       it under the terms of version 2 of the GNU General Public License as
     *       published by the Free Software Foundation.
     *
     *       This program is distributed in the hope that it will be useful, but
     *       WITHOUT ANY WARRANTY; without even the implied warranty of
     *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
     *       General Public License for more details.
     *
     *       You should have received a copy of the GNU General Public License
     *       along with this program; if not, write to the Free Software
     *       Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
     *       The full GNU General Public License is included in this distribution
     *       in the file called LICENSE.GPL.
     *
     *       Contact Information:
     *       Intel Corporation
     *
     *       BSD LICENSE
     *
     *       Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
     *       All rights reserved.
     *
     *       Redistribution and use in source and binary forms, with or without
     *       modification, are permitted provided that the following conditions
     *       are met:
     *
     *       * Redistributions of source code must retain the above copyright
     *       notice, this list of conditions and the following disclaimer.
     *       * Redistributions in binary form must reproduce the above copyright
     *       notice, this list of conditions and the following disclaimer in
     *       the documentation and/or other materials provided with the
     *       distribution.
     *       * Neither the name of Intel Corporation nor the names of its
     *       contributors may be used to endorse or promote products derived
     *       from this software without specific prior written permission.
     *
     *       THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
     *       "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
     *       LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
     *       A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
     *       OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
     *       SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
     *       LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
     *       DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
     *       THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
     *       (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
     *       OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
     *
     */
