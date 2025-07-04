.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Marvell.

Adding a new library
====================

Process for approval in principle
---------------------------------

Rationale
~~~~~~~~~

Adding a new library to DPDK with proper RFC and then full patch-sets is a significant work.
In order to save effort, developers should get an early approval in principle,
or an early feedback in case the library is not suitable for various reasons.

Process
~~~~~~~

#. When a contributor would like to add a new library to DPDK code base,
   the contributor must send the following items to DPDK mailing list
   for Technical Board approval-in-principle.

   * Purpose of the library.
   * Scope of work: outline the various additional tasks planned for this library,
     such as developing new test applications, adding new drivers,
     and updating existing applications.
   * Expected usage models of the library.
   * Any licensing constraints.
   * Justification for adding to DPDK.
   * Any other implementations of the same functionality in other libraries/projects
     and how this version differs.
   * Public API specification header file as RFC.

     * Optional and good to have.
     * Technical Board may additionally request this specification collateral
       if needed to get more clarity on scope and purpose.

   * Any new library dependencies to DPDK.

#. Technical Board to schedule discussion on this in upcoming Technical Board meeting
   along with author.
   Based on the Technical Board schedule and/or author availability,
   Technical Board may need a maximum of **five** Technical Board meeting slots.

#. Based on mailing list and Technical Board meeting discussions,
   Technical Board to vote and share the decision in the mailing list.
   The decision outcome can be any of the following:

   * Approved in principle
   * Not approved
   * Further information needed

#. Once the Technical Board approves the library in principle,
   it is safe to start working on the implementation.
   However, the patches will need to meet the usual quality criteria
   in order to be effectively accepted.
