..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK ABI/API policy
===================

Description
-----------

This document details some methods for handling ABI management in the DPDK.

General Guidelines
------------------

#. Whenever possible, ABI should be preserved
#. ABI/API may be changed with a deprecation process
#. The modification of symbols can generally be managed with versioning
#. Libraries or APIs marked in ``experimental`` state may change without constraint
#. New APIs will be marked as ``experimental`` for at least one release to allow
   any issues found by users of the new API to be fixed quickly
#. The addition of symbols is generally not problematic
#. The removal of symbols generally is an ABI break and requires bumping of the
   LIBABIVER macro
#. Updates to the minimum hardware requirements, which drop support for hardware which
   was previously supported, should be treated as an ABI change.

What is an ABI
~~~~~~~~~~~~~~

An ABI (Application Binary Interface) is the set of runtime interfaces exposed
by a library. It is similar to an API (Application Programming Interface) but
is the result of compilation.  It is also effectively cloned when applications
link to dynamic libraries.  That is to say when an application is compiled to
link against dynamic libraries, it is assumed that the ABI remains constant
between the time the application is compiled/linked, and the time that it runs.
Therefore, in the case of dynamic linking, it is critical that an ABI is
preserved, or (when modified), done in such a way that the application is unable
to behave improperly or in an unexpected fashion.


ABI/API Deprecation
-------------------

The DPDK ABI policy
~~~~~~~~~~~~~~~~~~~

ABI versions are set at the time of major release labeling, and the ABI may
change multiple times, without warning, between the last release label and the
HEAD label of the git tree.

ABI versions, once released, are available until such time as their
deprecation has been noted in the Release Notes for at least one major release
cycle. For example consider the case where the ABI for DPDK 2.0 has been
shipped and then a decision is made to modify it during the development of
DPDK 2.1. The decision will be recorded in the Release Notes for the DPDK 2.1
release and the modification will be made available in the DPDK 2.2 release.

ABI versions may be deprecated in whole or in part as needed by a given
update.

Some ABI changes may be too significant to reasonably maintain multiple
versions. In those cases ABI's may be updated without backward compatibility
being provided. The requirements for doing so are:

#. At least 3 acknowledgments of the need to do so must be made on the
   dpdk.org mailing list.

   - The acknowledgment of the maintainer of the component is mandatory, or if
     no maintainer is available for the component, the tree/sub-tree maintainer
     for that component must acknowledge the ABI change instead.

   - It is also recommended that acknowledgments from different "areas of
     interest" be sought for each deprecation, for example: from NIC vendors,
     CPU vendors, end-users, etc.

#. The changes (including an alternative map file) can be included with
   deprecation notice, in wrapped way by the ``RTE_NEXT_ABI`` option,
   to provide more details about oncoming changes.
   ``RTE_NEXT_ABI`` wrapper will be removed when it become the default ABI.
   More preferred way to provide this information is sending the feature
   as a separate patch and reference it in deprecation notice.

#. A full deprecation cycle, as explained above, must be made to offer
   downstream consumers sufficient warning of the change.

Note that the above process for ABI deprecation should not be undertaken
lightly. ABI stability is extremely important for downstream consumers of the
DPDK, especially when distributed in shared object form. Every effort should
be made to preserve the ABI whenever possible. The ABI should only be changed
for significant reasons, such as performance enhancements. ABI breakage due to
changes such as reorganizing public structure fields for aesthetic or
readability purposes should be avoided.

.. note::

   Updates to the minimum hardware requirements, which drop support for hardware
   which was previously supported, should be treated as an ABI change, and
   follow the relevant deprecation policy procedures as above: 3 acks and
   announcement at least one release in advance.

Examples of Deprecation Notices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following are some examples of ABI deprecation notices which would be
added to the Release Notes:

* The Macro ``#RTE_FOO`` is deprecated and will be removed with version 2.0,
  to be replaced with the inline function ``rte_foo()``.

* The function ``rte_mbuf_grok()`` has been updated to include a new parameter
  in version 2.0. Backwards compatibility will be maintained for this function
  until the release of version 2.1

* The members of ``struct rte_foo`` have been reorganized in release 2.0 for
  performance reasons. Existing binary applications will have backwards
  compatibility in release 2.0, while newly built binaries will need to
  reference the new structure variant ``struct rte_foo2``. Compatibility will
  be removed in release 2.2, and all applications will require updating and
  rebuilding to the new structure at that time, which will be renamed to the
  original ``struct rte_foo``.

* Significant ABI changes are planned for the ``librte_dostuff`` library. The
  upcoming release 2.0 will not contain these changes, but release 2.1 will,
  and no backwards compatibility is planned due to the extensive nature of
  these changes. Binaries using this library built prior to version 2.1 will
  require updating and recompilation.

New API replacing previous one
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If a new API proposed functionally replaces an existing one, when the new API
becomes non-experimental then the old one is marked with ``__rte_deprecated``.
Deprecated APIs are removed completely just after the next LTS.

Reminder that old API should follow deprecation process to be removed.


Experimental APIs
-----------------

APIs marked as ``experimental`` are not considered part of the ABI and may
change without warning at any time.  Since changes to APIs are most likely
immediately after their introduction, as users begin to take advantage of
those new APIs and start finding issues with them, new DPDK APIs will be
automatically marked as ``experimental`` to allow for a period of stabilization
before they become part of a tracked ABI.

Note that marking an API as experimental is a multi step process.
To mark an API as experimental, the symbols which are desired to be exported
must be placed in an EXPERIMENTAL version block in the corresponding libraries'
version map script.
Secondly, the corresponding prototypes of those exported functions (in the
development header files), must be marked with the ``__rte_experimental`` tag
(see ``rte_compat.h``).
The DPDK build makefiles perform a check to ensure that the map file and the
C code reflect the same list of symbols.
This check can be circumvented by defining ``ALLOW_EXPERIMENTAL_API``
during compilation in the corresponding library Makefile.

In addition to tagging the code with ``__rte_experimental``,
the doxygen markup must also contain the EXPERIMENTAL string,
and the MAINTAINERS file should note the EXPERIMENTAL libraries.

For removing the experimental tag associated with an API, deprecation notice
is not required. Though, an API should remain in experimental state for at least
one release. Thereafter, normal process of posting patch for review to mailing
list can be followed.
