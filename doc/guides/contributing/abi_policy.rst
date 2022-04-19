..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

ABI Policy
==========

Description
-----------

This document details the management policy that ensures the long-term stability
of the DPDK ABI and API.

General Guidelines
------------------

#. Major ABI versions are declared no more frequently than yearly. Compatibility
   with the major ABI version is mandatory in subsequent releases until a
   :ref:`new major ABI version <new_abi_version>` is declared.
#. Major ABI versions are usually but not always declared aligned with a
   :ref:`LTS release <stable_lts_releases>`.
#. The ABI version is managed at a project level in DPDK, and is reflected in
   all non-experimental :ref:`library's soname <what_is_soname>`.
#. The ABI should be preserved and not changed lightly. ABI changes must follow
   the outlined :ref:`deprecation process <abi_changes>`.
#. The addition of symbols is generally not problematic. The modification of
   symbols is managed with :ref:`ABI Versioning <abi_versioning>`.
#. The removal of symbols is considered an :ref:`ABI breakage <abi_breakages>`,
   once approved these will form part of the next ABI version.
#. Libraries or APIs marked as :ref:`experimental <experimental_apis>`
   may be changed or removed without prior notice,
   as they are not considered part of an ABI version.
   The :ref:`experimental <experimental_apis>` status of an API
   is not an indefinite state.
#. Updates to the :ref:`minimum hardware requirements <hw_rqmts>`, which drop
   support for hardware which was previously supported, should be treated as an
   ABI change.

.. note::

   Please note that this policy does not currently apply to the
   :doc:`Windows build <../windows_gsg/intro>`.

What is an ABI?
~~~~~~~~~~~~~~~

An ABI (Application Binary Interface) is the set of runtime interfaces exposed
by a library. It is similar to an API (Application Programming Interface) but
is the result of compilation.  It is also effectively cloned when applications
link to dynamic libraries.  That is to say when an application is compiled to
link against dynamic libraries, it is assumed that the ABI remains constant
between the time the application is compiled/linked, and the time that it runs.
Therefore, in the case of dynamic linking, it is critical that an ABI is
preserved, or (when modified), done in such a way that the application is unable
to behave improperly or in an unexpected fashion.

.. _figure_what_is_an_abi:

.. figure:: img/what_is_an_abi.*

	    Illustration of DPDK API and ABI.


What is an ABI version?
~~~~~~~~~~~~~~~~~~~~~~~

An ABI version is an instance of a library's ABI at a specific release. Certain
releases are considered to be milestone releases, the yearly LTS release for
example. The ABI of a milestone release may be declared as a 'major ABI
version', where this ABI version is then supported for some number of subsequent
releases and is annotated in the library's :ref:`soname<what_is_soname>`.

ABI version support in subsequent releases facilitates application upgrades, by
enabling applications built against the milestone release to upgrade to
subsequent releases of a library without a rebuild.

More details on major ABI version can be found in the :ref:`ABI versioning
<major_abi_versions>` guide.

The DPDK ABI policy
-------------------

A new major ABI version is declared no more frequently than yearly, with
declarations usually aligning with a LTS release, e.g. ABI 21 for DPDK 20.11.
Compatibility with the major ABI version is then mandatory in subsequent
releases until the next major ABI version is declared, e.g. ABI 22 for DPDK
21.11.

At the declaration of a major ABI version, major version numbers encoded in
libraries' sonames are bumped to indicate the new version, with the minor
version reset to ``0``. An example would be ``librte_eal.so.21.3`` would become
``librte_eal.so.22.0``.

The ABI may then change multiple times, without warning, between the last major
ABI version increment and the HEAD label of the git tree, with the condition
that ABI compatibility with the major ABI version is preserved and therefore
sonames do not change.

Minor versions are incremented to indicate the release of a new ABI compatible
DPDK release, typically the DPDK quarterly releases. An example of this, might
be that ``librte_eal.so.21.1`` would indicate the first ABI compatible DPDK
release, following the declaration of the new major ABI version ``21``.

An ABI version is supported in all new releases until the next major ABI version
is declared. When changing the major ABI version, the release notes will detail
all ABI changes.

.. _figure_abi_stability_policy:

.. figure:: img/abi_stability_policy.*

	    Mapping of new ABI versions and ABI version compatibility to DPDK
	    releases.

.. _abi_changes:

ABI Changes
~~~~~~~~~~~

The ABI may still change after the declaration of a major ABI version, that is
new APIs may be still added or existing APIs may be modified.

.. Warning::

   Note that, this policy details the method by which the ABI may be changed,
   with due regard to preserving compatibility and observing deprecation
   notices. This process however should not be undertaken lightly, as a general
   rule ABI stability is extremely important for downstream consumers of DPDK.
   The API should only be changed for significant reasons, such as performance
   enhancements. API breakages due to changes such as reorganizing public
   structure fields for aesthetic or readability purposes should be avoided.

The requirements for changing the ABI are:

#. At least 3 acknowledgments of the need to do so must be made on the
   dpdk.org mailing list.

   - The acknowledgment of the maintainer of the component is mandatory, or if
     no maintainer is available for the component, the tree/sub-tree maintainer
     for that component must acknowledge the ABI change instead.

   - The acknowledgment of three members of the technical board, as delegates
     of the `technical board <https://core.dpdk.org/techboard/>`_ acknowledging
     the need for the ABI change, is also mandatory.

   - It is also recommended that acknowledgments from different "areas of
     interest" be sought for each deprecation, for example: from NIC vendors,
     CPU vendors, end-users, etc.

#. Backward compatibility with the major ABI version must be maintained through
   :ref:`abi_versioning`, with :ref:`forward-only <forward-only>` compatibility
   offered for any ABI changes that are indicated to be part of the next ABI
   version.

   - In situations where backward compatibility is not possible, read the
     section on :ref:`abi_breakages`.

   - No backward or forward compatibility is offered for API changes marked as
     ``experimental``, as described in the section on :ref:`Experimental APIs
     and Libraries <experimental_apis>`.

   - In situations in which an ``experimental`` symbol has been stable for some
     time. When promoting the symbol to become part of the next ABI version, the
     maintainer may choose to provide an alias to the ``experimental`` tag, so
     as not to break consuming applications.

#. If a newly proposed API functionally replaces an existing one, when the new
   API becomes non-experimental, then the old one is marked with
   ``__rte_deprecated``.

    - The deprecated API should follow the notification process to be removed,
      see  :ref:`deprecation_notices`.

    - At the declaration of the next major ABI version, those ABI changes then
      become a formal part of the new ABI and the requirement to preserve ABI
      compatibility with the last major ABI version is then dropped.

    - The responsibility for removing redundant ABI compatibility code rests
      with the original contributor of the ABI changes, failing that, then with
      the contributor's company and then finally with the maintainer.

.. _forward-only:

.. Note::

   Note that forward-only compatibility is offered for those changes made
   between major ABI versions. As a library's soname can only describe
   compatibility with the last major ABI version, until the next major ABI
   version is declared, these changes therefore cannot be resolved as a runtime
   dependency through the soname. Therefore any application wishing to make use
   of these ABI changes can only ensure that its runtime dependencies are met
   through Operating System package versioning.

.. _hw_rqmts:

.. Note::

   Updates to the minimum hardware requirements, which drop support for hardware
   which was previously supported, should be treated as an ABI change, and
   follow the relevant deprecation policy procedures as above: 3 acks, technical
   board approval and announcement at least one release in advance.

.. _abi_breakages:

ABI Breakages
~~~~~~~~~~~~~

For those ABI changes that are too significant to reasonably maintain multiple
symbol versions, there is an amended process. In these cases, ABIs may be
updated without the requirement of backward compatibility being provided. These
changes must follow the same process :ref:`described above <abi_changes>` as non-breaking
changes, however with the following additional requirements:

#. ABI breaking changes (including an alternative map file) can be included with
   deprecation notice, in wrapped way by the ``RTE_NEXT_ABI`` option, to provide
   more details about oncoming changes. ``RTE_NEXT_ABI`` wrapper will be removed
   at the declaration of the next major ABI version.

#. Once approved, and after the deprecation notice has been observed these
   changes will form part of the next declared major ABI version.

Examples of ABI Changes
~~~~~~~~~~~~~~~~~~~~~~~

The following are examples of allowable ABI changes occurring between
declarations of major ABI versions.

* DPDK 20.11 release defines the function ``rte_foo()`` ; ``rte_foo()``
  is part of the major ABI version ``21``.

* DPDK 21.02 release defines a new function ``rte_foo(uint8_t bar)``.
  This is not a problem as long as the symbol ``rte_foo@DPDK_21`` is
  preserved through :ref:`abi_versioning`.

  - The new function may be marked with the ``__rte_experimental`` tag for a
    number of releases, as described in the section :ref:`experimental_apis`.

  - Once ``rte_foo(uint8_t bar)`` becomes non-experimental, ``rte_foo()`` is
    declared as ``__rte_deprecated`` and an deprecation notice is provided.

* DPDK 20.11 is not re-released to include ``rte_foo(uint8_t bar)``, the new
  version of ``rte_foo`` only exists from DPDK 21.02 onwards as described in the
  :ref:`note on forward-only compatibility<forward-only>`.

* DPDK 21.02 release defines the experimental function ``__rte_experimental
  rte_baz()``. This function may or may not exist in the DPDK 21.05 release.

* An application ``dPacket`` wishes to use ``rte_foo(uint8_t bar)``, before the
  declaration of the DPDK ``22`` major ABI version. The application can only
  ensure its runtime dependencies are met by specifying ``DPDK (>= 21.2)`` as
  an explicit package dependency, as the soname can only indicate the
  supported major ABI version.

* At the release of DPDK 21.11, the function ``rte_foo(uint8_t bar)`` becomes
  formally part of then new major ABI version DPDK ``22`` and ``rte_foo()`` may be
  removed.

.. _deprecation_notices:

Examples of Deprecation Notices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following are some examples of ABI deprecation notices which would be
added to the Release Notes:

* The Macro ``#RTE_FOO`` is deprecated and will be removed with ABI version
  22, to be replaced with the inline function ``rte_foo()``.

* The function ``rte_mbuf_grok()`` has been updated to include a new parameter
  in version 21.2. Backwards compatibility will be maintained for this function
  until the release of the new DPDK major ABI version 22, in DPDK version
  21.11.

* The members of ``struct rte_foo`` have been reorganized in DPDK 21.02 for
  performance reasons. Existing binary applications will have backwards
  compatibility in release 21.02, while newly built binaries will need to
  reference the new structure variant ``struct rte_foo2``. Compatibility will be
  removed in release 21.11, and all applications will require updating and
  rebuilding to the new structure at that time, which will be renamed to the
  original ``struct rte_foo``.

* Significant ABI changes are planned for the ``librte_dostuff`` library. The
  upcoming release 21.02 will not contain these changes, but release 21.11 will,
  and no backwards compatibility is planned due to the extensive nature of
  these changes. Binaries using this library built prior to ABI version 22 will
  require updating and recompilation.


.. _new_abi_version:

New ABI versions
------------------

A new ABI version may be declared aligned with a given release.
The requirement to preserve compatibility with the previous major ABI version
is then dropped for the duration of this release cycle.
This is commonly known as the *ABI breakage window*,
and some amended rules apply during this cycle:

 * The requirement to preserve compatibility with the previous major ABI
   version, as described in the section :ref:`abi_changes` does not apply.
 * Contributors of compatibility preserving code in previous releases,
   are now required to remove this compatibility code,
   as described in the section :ref:`abi_changes`.
 * Symbol versioning references to the old ABI version are updated
   to reference the new ABI version,
   as described in the section :ref:`deprecating_entire_abi`.
 * Contributors of aliases to experimental in previous releases,
   as described in section :ref:`aliasing_experimental_symbols`,
   are now required to remove these aliases.
 * Finally, the *ABI breakage window* is *not* permission to circumvent
   the other aspects of the procedures to make ABI changes
   described in :ref:`abi_changes`, that is, 3 ACKs of the requirement
   to break the ABI and the observance of a deprecation notice
   are still considered mandatory.

.. _experimental_apis:

Experimental
------------

APIs
~~~~

APIs marked as ``experimental`` are not considered part of an ABI version and
may be changed or removed without prior notice. Since changes to APIs are most likely
immediately after their introduction, as users begin to take advantage of those
new APIs and start finding issues with them, new DPDK APIs will be automatically
marked as ``experimental`` to allow for a period of stabilization before they
become part of a tracked ABI version.

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

For removing the experimental tag associated with an API, deprecation notice is
not required. Though, an API should remain in experimental state for at least
one release. Thereafter, the normal process of posting patch for review to
mailing list can be followed.

After the experimental tag has been formally removed, a tree/sub-tree maintainer
may choose to offer an alias to the experimental tag so as not to break
applications using the symbol. The alias is then dropped at the declaration of
next major ABI version.

Libraries
~~~~~~~~~

Libraries marked as ``experimental`` are entirely not considered part of an ABI
version.
All functions in such libraries may be changed or removed without prior notice.

Promotion to stable
~~~~~~~~~~~~~~~~~~~

An API's ``experimental`` status should be reviewed annually,
by both the maintainer and/or the original contributor.
Ordinarily APIs marked as ``experimental`` will be promoted to the stable ABI
once a maintainer has become satisfied that the API is mature
and is unlikely to change.

In exceptional circumstances, should an API still be classified
as ``experimental`` after two years
and is without any prospect of becoming part of the stable API.
The API will then become a candidate for removal,
to avoid the accumulation of abandoned symbols.

Should an ABI change, usually due to a direct change to the API's signature,
it is reasonable for the review and expiry clocks to reset.
The promotion or removal of symbols will typically form part of a conversation
between the maintainer and the original contributor.
