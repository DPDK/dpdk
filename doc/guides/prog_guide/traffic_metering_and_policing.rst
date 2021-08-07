..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Intel Corporation.

Traffic Metering and Policing API
=================================


Overview
--------

This is the generic API for the Quality of Service (QoS) Traffic Metering and
Policing (MTR) of Ethernet devices. This API is agnostic of the underlying HW,
SW or mixed HW-SW implementation.

The main features are:

* Part of DPDK rte_ethdev API
* Capability query API
* Metering algorithms: RFC 2697 Single Rate Three Color Marker (srTCM), RFC 2698
  and RFC 4115 Two Rate Three Color Marker (trTCM)
* Policer actions (per meter output color): recolor, drop
* Statistics (per policer output color)
* Chaining multiple meter objects

Configuration steps
-------------------

The metering and policing stage typically sits on top of flow classification,
which is why the MTR objects are enabled through a special "meter" action.

The MTR objects are created and updated in their own name space (``rte_mtr``)
within the ``librte_ethdev`` library. Whether an MTR object is private to a
flow or potentially shared by several flows has to be specified at its
creation time.

Once successfully created, an MTR object is hooked into the RX processing path
of the Ethernet device by linking it to one or several flows through the
dedicated "meter" flow action. One or several "meter" actions can be registered
for the same flow. An MTR object can only be destroyed if there are no flows
using it.

Run-time processing
-------------------

Traffic metering determines the color for the current packet (green, yellow,
red) based on the previous history for this flow as maintained by the MTR
object. The policer can do nothing, override the color the packet or drop the
packet. Statistics counters are maintained for MTR object, as configured.

The processing done for each input packet hitting an MTR object is:

* Traffic metering: The packet is assigned a color (the meter output color)
  based on the previous traffic history reflected in the current state of the
  MTR object, according to the specific traffic metering algorithm. The
  traffic metering algorithm can typically work in color aware mode, in which
  case the input packet already has an initial color (the input color), or in
  color blind mode, which is equivalent to considering all input packets
  initially colored as green.

* There is a meter policy API to manage pre-defined policies for meter.
  Any rte_flow action list can be configured per color for each policy.
  A meter object configured with a policy executes the actions per packet
  according to the packet color.

* Statistics: The set of counters maintained for each MTR object is
  configurable and subject to the implementation support. This set includes
  the number of packets and bytes dropped or passed for each output color.

API walk-through
----------------

.. _figure_rte_mtr_chaining:

.. figure:: img/rte_mtr_meter_chaining.*

   Meter components

This section will introduce the reader to the critical APIs to use
the traffic meter and policing library.

In general, the application performs the following steps to configure the
traffic meter and policing library.

#. Application gets the meter driver capabilities using ``rte_mtr_capabilities_get()``.
#. The application creates the required meter profiles by using the
   ``rte_mtr_meter_profile_add()`` API function.
#. The application creates the required meter policies by using the
   ``rte_mtr_meter_policy_add()`` API function.
#. The application creates a meter object using the ``rte_mtr_create()`` API
   function. One of the previously created meter profile
   (``struct rte_mtr_params::meter_profile_id``) and meter policy
   (``struct rte_mtr_params::meter_policy_id``) are provided as arguments
   at this step.
#. The application enables the meter object execution as part of the flow action
   processing by calling the ``rte_flow_create()`` API function with one of the
   flow action set to ``RTE_FLOW_ACTION_TYPE_METER`` and the associated
   meter object ID set to this meter object.
#. The API allows chaining the meter objects to create complex metering topology
   by the following methods.

   * Adding multiple flow actions of the type ``RTE_FLOW_ACTION_TYPE_METER`` to
     the same flow.
     Each of the meter action typically refers to a different meter object.

   * Adding one (or multiple) actions of the type ``RTE_FLOW_ACTION_TYPE_METER``
     to the list of meter actions (``struct rte_mtr_meter_policy_params::actions``)
     specified per color as show in :numref:`figure_rte_mtr_chaining`.
