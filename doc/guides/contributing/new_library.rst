.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Marvell.

Adding a new library
====================

Process for approval in principle
---------------------------------

Rationale
~~~~~~~~~

Adding a new library to DPDK with proper RFC and then full patch-sets requires significant effort.
To save effort, developers should seek early approval in principle,
or early feedback if the library is unsuitable for various reasons.

Process
~~~~~~~

#. When a contributor would like to add a new library to the DPDK code base,
   the contributor must send the following items to the DPDK mailing list
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

#. Technical Board schedules discussion on this in an upcoming Technical Board meeting
   along with the author.
   Based on the Technical Board schedule and/or author availability,
   Technical Board may need a maximum of **five** Technical Board meeting slots.

#. Based on mailing list and Technical Board meeting discussions,
   Technical Board votes and shares the decision on the mailing list.
   The decision outcome can be any of the following:

   * Approved in principle
   * Not approved
   * Further information needed

#. Once the Technical Board approves the library in principle,
   it is safe to start working on the implementation.
   However, the patches must meet the usual quality criteria
   to be effectively accepted.
