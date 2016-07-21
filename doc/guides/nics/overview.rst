..  BSD LICENSE
    Copyright 2016 6WIND S.A.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of 6WIND S.A. nor the names of its
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

Overview of Networking Drivers
==============================

The networking drivers may be classified in two categories:

- physical for real devices
- virtual for emulated devices

Some physical devices may be shaped through a virtual layer as for
SR-IOV.
The interface seen in the virtual environment is a VF (Virtual Function).

The ethdev layer exposes an API to use the networking functions
of these devices.
The bottom half part of ethdev is implemented by the drivers.
Thus some features may not be implemented.

There are more differences between drivers regarding some internal properties,
portability or even documentation availability.
Most of these differences are summarized below.

.. _table_net_pmd_features:

.. raw:: html

   <style>
      table#id1 th {
         font-size: 80%;
         white-space: pre-wrap;
         text-align: center;
         vertical-align: top;
         padding: 2px;
      }
      table#id1 th:first-child {
         vertical-align: bottom;
      }
      table#id1 td {
         font-size: 70%;
         padding: 1px;
      }
      table#id1 td:first-child {
         padding-left: 1em;
      }
   </style>

.. table:: Features availability in networking drivers

   ==================== = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
   Feature              a b b b b c e e e i i i i i i i i i i f f f f m m m n n p q q r s t v v v v x
                        f n n n o x 1 n n 4 4 4 4 g g x x x x m m m m l l p f u c e e i z h h i i m e
                        p x x x n g 0 a i 0 0 0 0 b b g g g g 1 1 1 1 x x i p l a d d n e u o r r x n
                        a 2 2 t d b 0   c e e e e   v b b b b 0 0 0 0 4 5 p   l p e e g d n s t t n v
                        c x x   i e 0       . v v   f e e e e k k k k     e         v   a d t i i e i
                        k   v   n           . f f       . v v   . v v               f   t e   o o t r
                        e   f   g           .   .       . f f   . f f                   a r     . 3 t
                        t                   v   v       v   v   v   v                   2 x     v
                                            e   e       e   e   e   e                           e
                                            c   c       c   c   c   c                           c
   ==================== = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
   Speed capabilities
   Link status            Y Y Y   Y Y   Y Y Y     Y   Y Y Y Y         Y Y         Y Y   Y Y Y Y Y
   Link status event      Y Y       Y     Y Y     Y   Y Y             Y Y         Y Y     Y Y
   Queue status event                                                                       Y
   Rx interrupt                     Y     Y Y Y Y Y Y Y Y Y Y Y Y Y Y
   Queue start/stop           Y   Y   Y Y Y Y Y Y     Y Y     Y Y Y Y Y Y               Y Y   Y Y
   MTU update                     Y Y Y P         Y   Y Y Y Y         Y Y         Y Y     Y
   Jumbo frame                    Y Y Y Y Y Y Y Y Y   Y Y Y Y Y Y Y Y Y Y       Y Y Y     Y
   Scattered Rx                   Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y               Y Y   Y
   LRO                                                Y Y Y Y
   TSO                            Y   Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y
   Promiscuous mode       Y Y Y   Y Y   Y Y Y Y Y Y Y Y Y     Y Y     Y Y         Y Y   Y Y   Y Y
   Allmulticast mode              Y Y     Y Y Y Y Y Y Y Y Y Y Y Y     Y Y         Y Y   Y Y   Y Y
   Unicast MAC filter     Y Y Y     Y   Y Y Y Y Y Y Y Y Y Y Y Y Y     Y Y         Y Y         Y Y
   Multicast MAC filter   Y Y Y         Y Y Y Y Y             Y Y     Y Y         Y Y         Y Y
   RSS hash                       Y   Y Y Y Y Y Y Y   Y Y Y Y Y Y Y Y Y Y         Y Y     Y
   RSS key update                     Y   Y Y Y Y Y   Y Y Y Y Y Y Y Y   Y         Y Y     Y
   RSS reta update            Y       Y   Y Y Y Y Y   Y Y Y Y Y Y Y Y   Y         Y Y     Y
   VMDq                                   Y Y     Y   Y Y     Y Y
   SR-IOV                   Y         Y   Y Y     Y   Y Y             Y Y           Y     Y
   DCB                                    Y Y     Y   Y Y
   VLAN filter                      Y   Y Y Y Y Y Y Y Y Y Y Y Y Y     Y Y         Y Y         Y Y
   Ethertype filter                       Y Y     Y   Y Y
   N-tuple filter                                 Y   Y Y
   SYN filter                                     Y   Y Y
   Tunnel filter                          Y Y         Y Y
   Flexible filter                                Y
   Hash filter                            Y Y Y Y
   Flow director                          Y Y         Y Y               Y
   Flow control                   Y Y     Y Y     Y   Y Y                         Y Y
   Rate limitation                                    Y Y
   Traffic mirroring                      Y Y         Y Y
   CRC offload                    Y Y Y Y Y   Y   Y Y Y   Y   Y Y Y Y   Y         Y Y     Y
   VLAN offload                   Y Y Y Y Y   Y   Y Y Y   Y   Y Y Y Y   Y         Y Y     P
   QinQ offload                     Y     Y   Y   Y Y Y   Y
   L3 checksum offload            Y Y Y Y Y   Y   Y Y Y   Y   Y Y Y Y Y Y                 Y
   L4 checksum offload            Y Y Y Y Y   Y   Y Y Y   Y   Y Y Y Y Y Y                 Y
   Inner L3 checksum                  Y   Y   Y       Y   Y           Y
   Inner L4 checksum                  Y   Y   Y       Y   Y           Y
   Packet type parsing            Y     Y Y   Y   Y Y Y   Y   Y Y Y Y Y Y         Y Y     Y
   Timesync                               Y Y     Y   Y Y
   Basic stats            Y Y Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y       Y Y Y   Y Y Y Y Y
   Extended stats         Y Y Y       Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y             Y Y   Y   Y
   Stats per queue                Y                   Y Y     Y Y Y Y Y Y         Y Y   Y Y   Y Y
   EEPROM dump                    Y               Y   Y Y
   Registers dump                 Y               Y Y Y Y Y Y                             Y
   Multiprocess aware                     Y Y Y Y     Y Y Y Y Y Y Y Y Y Y       Y Y Y     Y
   BSD nic_uio                    Y Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y                         Y Y
   Linux UIO              Y Y Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y             Y Y         Y Y
   Linux VFIO                     Y Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y                     Y   Y Y
   Other kdrv                                                         Y Y               Y
   ARMv7                                                                        Y             Y Y
   ARMv8                                              Y Y Y Y                   Y         Y   Y Y
   Power8                                                             Y Y       Y
   TILE-Gx                                                                      Y
   x86-32                         Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y       Y           Y Y Y
   x86-64                 Y Y Y   Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y       Y Y Y   Y   Y Y Y
   Usage doc              Y Y     Y     Y                             Y Y       Y Y Y   Y Y   Y
   Design doc
   Perf doc
   ==================== = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

.. Note::

   Features marked with "P" are partially supported. Refer to the appropriate
   NIC guide in the following sections for details.
