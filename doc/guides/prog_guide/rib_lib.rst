..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

RIB Library
===========

The Routing Information Base (RIB) library provides a data store for routing information.
This library is intended for use in control or management plane applications.
There are more suitable libraries for use in data plane applications such as
:doc:`lpm_lib` or :doc:`fib_lib`.


Implementation details
----------------------

RIB implements a key-value store for routing information.

Routing information is represented by a prefix (key) and a next hop ID (value).
The prefix type depends on the address family. IPv4 addresses are represented by
``uint32_t`` values. IPv6 addresses are represented as ``uint8_t[16]`` values.
Next hop IDs are represented by ``uint64_t`` values.

.. note::

   The API and implementation are very similar for IPv4 ``rte_rib`` API and IPv6 ``rte_rib6``
   API, therefore only the ``rte_rib`` API will be discussed here.
   Everything within this document except for the size of the prefixes is applicable to  the
   ``rte_rib6`` API.

Internally RIB is represented as a binary tree as shown in :numref:`figure_rib_internals`:

.. _figure_rib_internals:

.. figure:: img/rib_internals.*

   RIB internals overview

The binary tree consists of two types of nodes:

* Actual Routes.

* Intermediate Nodes which are used internally to preserve the binary tree structure.


RIB API Overview
----------------

RIB has two configuration parameters:

* The maximum number of nodes.

* The size of the extension block within each node. This space is used to store
  additional user defined data.

The main methods within the ``rte_rib`` API are:

* ``rte_rib_insert()``: Add new routes.

* ``rte_rib_remove()``: Delete an existing route.

* ``rte_rib_lookup()``: Lookup an IP in the structure using longest match.

* ``rte_rib_lookup_exact()``: Lookup an IP in the structure using exact match.

* ``rte_rib_lookup_parent()``: Find a parent prefix within the structure.

* ``rte_rib_get_nxt()``: Traverse a subtree within the structure.

Given a RIB structure with the routes depicted in :numref:`figure_rib_internals`,
here are several usage examples:

* The best route for ``10.0.0.1`` can be found by calling:

.. code-block:: c

      struct rte_rib_node *route = rte_rib_lookup(rib, RTE_IPV4(10,0,0,1));

This returns an ``rte_rib_node`` pointing to the ``10.0.0.0/29`` prefix.

* To find an exact match route:

.. code-block:: c

      struct rte_rib_node *route = rte_rib_lookup_exact(rib, RTE_IPV4(10,0,0,128), 25);

This returns an ``rte_rib_node`` pointing to the ``10.0.0.128/25`` prefix.

.. code-block:: c

      struct rte_rib_node *route = rte_rib_lookup_exact(rib, RTE_IPV4(10,0,0,0), 24);

This returns ``NULL`` as no exact match can be found.

* To retrieve a group of routes under the common prefix ``10.0.0.0/24``
  (yellow triangle in :numref:`figure_rib_internals`):

.. code-block:: c

      struct rte_rib_node *route = NULL;
      do {
         route = rte_rib_get_nxt(rib, RTE_IPV4(10,0,0,0), 24, route, RTE_RIB_GET_NXT_ALL);
      } while (route != NULL)

This returns 3 ``rte_rib_node`` nodes pointing to ``10.0.0.0/29``, ``10.0.0.160/27``
and ``10.0.0.128/25``.


Extensions usage example
------------------------

Extensions can be used for a wide range of tasks.
By default, an ``rte_rib_node`` node contains only crucial information such as the prefix and
next hop ID, but it doesn't contain protocol specific information such as
metrics, administrative distance and other routing protocol information.
These examples are application specific data and the user can decide what to keep
and how it is stored within the extension memory region in each ``rte_rib_node``.

It is possible to implement a prefix independent convergence using the RIB extension feature.
If the routing daemon can provide a feasible next hop ID along with a best (active) next hop ID,
it is possible to react to a neighbour failing relatively fast.
Consider a RIB with a number of routes with different next hops (A and B) as
shown in :numref:`figure_rib_pic`. Every route can have a feasible next hop
provided by the routing daemon.

.. _figure_rib_pic:

.. figure:: img/rib_pic.*

   RIB prefix independent convergence

In case of a next hop failure, we need to replace an active failed next hop with a
feasible next hop for every corresponding route without waiting for the routing daemon
recalculation process to complete.
To achieve this we can link all existing routes with the same active next hop in a linked list,
saving the feasible next hop ID and a pointer inside the extension space of each ``rte_rib_node``.

.. code-block:: c

      struct my_route_ext {
         struct rte_rib_node *next;
         uint64_t feasible_nh;
      };

      struct rte_rib_conf conf;
      conf.ext_sz = sizeof(struct my_route_ext);
      rib = rte_rib_create("test", 0, &conf);
      ...
      /* routing daemon task */
      struct rte_rib_node *route = rte_rib_insert(rib, RTE_IPV4(192,0,2,0), 24);
      rte_rib_set_nh(route, active_nh_from_rd);
      struct my_route_ext *ext = rte_rib_get_ext(route);
      ext->feasible_nh = feasible_nh_from_rd;
      list_insert(nh_table[active_nh_from_rd].list_head, route);
      ...
      /* dataplane monitoring thread */
      /* nexthop id fail_nh fails */
      route = NULL;
      do {
         route = get_next(nh_table[fail_nh].list_head, route);
         uint32_t ip;
         uint8_t depth;
         rte_rib_get_ip(route, &ip);
         rte_rib_get_depth(route, &depth);
         ext = rte_rib_get_ext(route);
         uint64_t new_nh = ext->feasible_nh;
         /* do update to the dataplane, for example to the fib */
         rte_fib_add(fib, ip, depth, new_nh);
         /* update nexthop if necessary */
         rte_rib_set_nh(route, new_nh);
      } while (route != NULL);
