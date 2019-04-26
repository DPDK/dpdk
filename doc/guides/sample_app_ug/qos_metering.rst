..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

QoS Metering Sample Application
===============================

The QoS meter sample application is an example that demonstrates the use of DPDK to provide QoS marking and metering,
as defined by RFC2697 for Single Rate Three Color Marker (srTCM) and RFC 2698 for Two Rate Three Color Marker (trTCM) algorithm.

Overview
--------

The application uses a single thread for reading the packets from the RX port,
metering, marking them with the appropriate color (green, yellow or red) and writing them to the TX port.

A policing scheme can be applied before writing the packets to the TX port by dropping or
changing the color of the packet in a static manner depending on both the input and output colors of the packets that are processed by the meter.

The operation mode can be selected as compile time out of the following options:

*   Simple forwarding

*   srTCM color blind

*   srTCM color aware

*   srTCM color blind

*   srTCM color aware

Please refer to RFC2697 and RFC2698 for details about the srTCM and trTCM configurable parameters
(CIR, CBS and EBS for srTCM; CIR, PIR, CBS and PBS for trTCM).

The color blind modes are functionally equivalent with the color-aware modes when
all the incoming packets are colored as green.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``qos_meter`` sub-directory.

Running the Application
-----------------------

The application execution command line is as below:

.. code-block:: console

    ./qos_meter [EAL options] -- -p PORTMASK

The application is constrained to use a single core in the EAL core mask and 2 ports only in the application port mask
(first port from the port mask is used for RX and the other port in the core mask is used for TX).

Refer to *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Explanation
-----------

Selecting one of the metering modes is done with these defines:

.. code-block:: c

    #define APP_MODE_FWD   0
    #define APP_MODE_SRTCM_COLOR_BLIND  1
    #define APP_MODE_SRTCM_COLOR_AWARE  2
    #define APP_MODE_TRTCM_COLOR_BLIND  3
    #define APP_MODE_TRTCM_COLOR_AWARE  4

    #define APP_MODE  APP_MODE_SRTCM_COLOR_BLIND

To simplify debugging (for example, by using the traffic generator RX side MAC address based packet filtering feature),
the color is defined as the LSB byte of the destination MAC address.

The traffic meter parameters are configured in the application source code with following default values:

.. code-block:: c

    struct rte_meter_srtcm_params app_srtcm_params[] = {

        {.cir = 1000000 * 46, .cbs = 2048, .ebs = 2048},

    };

    struct rte_meter_trtcm_params app_trtcm_params[] = {

        {.cir = 1000000 * 46, .pir = 1500000 * 46, .cbs = 2048, .pbs = 2048},

    };

Assuming the input traffic is generated at line rate and all packets are 64 bytes Ethernet frames (IPv4 packet size of 46 bytes)
and green, the expected output traffic should be marked as shown in the following table:

.. _table_qos_metering_1:

.. table:: Output Traffic Marking

   +-------------+------------------+-------------------+----------------+
   | **Mode**    | **Green (Mpps)** | **Yellow (Mpps)** | **Red (Mpps)** |
   |             |                  |                   |                |
   +=============+==================+===================+================+
   | srTCM blind | 1                | 1                 | 12.88          |
   |             |                  |                   |                |
   +-------------+------------------+-------------------+----------------+
   | srTCM color | 1                | 1                 | 12.88          |
   |             |                  |                   |                |
   +-------------+------------------+-------------------+----------------+
   | trTCM blind | 1                | 0.5               | 13.38          |
   |             |                  |                   |                |
   +-------------+------------------+-------------------+----------------+
   | trTCM color | 1                | 0.5               | 13.38          |
   |             |                  |                   |                |
   +-------------+------------------+-------------------+----------------+
   | FWD         | 14.88            | 0                 | 0              |
   |             |                  |                   |                |
   +-------------+------------------+-------------------+----------------+

To set up the policing scheme as desired, it is necessary to modify the main.h source file,
where this policy is implemented as a static structure, as follows:

.. code-block:: c

    int policer_table[e_RTE_METER_COLORS][e_RTE_METER_COLORS] =
    {
       { GREEN, RED, RED},
       { DROP, YELLOW, RED},
       { DROP, DROP, RED}
    };

Where rows indicate the input color, columns indicate the output color,
and the value that is stored in the table indicates the action to be taken for that particular case.

There are four different actions:

*   GREEN: The packet's color is changed to green.

*   YELLOW: The packet's color is changed to yellow.

*   RED: The packet's color is changed to red.

*   DROP: The packet is dropped.

In this particular case:

*   Every packet which input and output color are the same, keeps the same color.

*   Every packet which color has improved is dropped (this particular case can't happen, so these values will not be used).

*   For the rest of the cases, the color is changed to red.

.. note::
    * In color blind mode, first row GREEN color is only valid.
    * To drop the packet, policer_table action has to be set to DROP.
