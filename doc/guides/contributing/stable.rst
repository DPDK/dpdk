..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Stable Releases and Long Term Support
==========================================

This section outlines the guidelines for DPDK Stable Releases and Long Term Support (LTS) releases.


Introduction
------------

The purpose of DPDK Stable Releases is to maintain DPDK versions
with backported fixes over an extended period.
This allows downstream users to base applications or packages on a stable,
well-maintained version of DPDK.

The primary goal of stable releases is to fix issues without introducing regressions,
while preserving backward compatibility with the original version.

LTS (Long Term Support) is a designation given to specific stable releases
to indicate extended support duration.


Stable Releases
---------------

Any DPDK release may become a Stable Release if a maintainer volunteers to support it
and major contributors commit to validating it before each release.
This designation should be made within one month of the version's initial release.

Stable Releases are typically used to backport fixes
from an ``N`` release to an ``N-1`` release, for example, from 16.11 to 16.07.

* The standard support duration is one full release cycle (4 months).
* This may extend up to one year if the maintainer continues support
  or if users provide backported fixes.
* The maintainer determines the release cadence based on the volume and criticality of fixes.
* Coordinate releases with validation engineers to ensure proper testing before tagging.


LTS Releases
------------

A Stable Release can be promoted to an LTS release through community agreement
and a maintainer's commitment.

* The current policy designates each November release (X.11) as an LTS
  and maintains it for 3 years, contingent on community validation support.
* After release, an LTS branch is created at https://git.dpdk.org/dpdk-stable
  where bugfixes are backported.
* An LTS release may align with the declaration of a new major ABI version.
  See the :doc:`abi_policy` for more information.

Release Cadence:

* At least three LTS updates per year (roughly one every four months).
* Aligned with main DPDK releases to leverage shared validation.
* Frequency may vary depending on the urgency and volume of fixes.
* Coordinate validation with test engineers.

For a list of the currently maintained stable/LTS branches,
see the `stable roadmap <https://core.dpdk.org/roadmap/#stable>`_.

At the end of the 3-year period, a final X.11.N release is made,
after which the LTS branch is no longer maintained.


What Changes Should Be Backported
---------------------------------

Limit backports to bug fixes.

All patches accepted on the main branch with a ``Fixes:`` tag
should be backported to the relevant stable/LTS branches,
unless the submitter indicates otherwise.

Fixes suitable for backport should have a ``Cc: stable@dpdk.org`` tag in the
commit message body as follows::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Fixes: abcdefgh1234 ("doc: add some parameter")
     Cc: stable@dpdk.org

     Signed-off-by: Alex Smith <alex.smith@example.com>

Fixes not suitable for backport should not include the ``Cc: stable@dpdk.org`` tag.

New features should not be backported to stable releases.
In limited cases, a new feature may be acceptable if:

* It does not break API or ABI.
* It preserves backward compatibility.
* It targets the latest LTS release (to help with upgrade paths).
* The proposer commits to testing the feature and monitoring regressions.
* The proposer or their organization has a track record of validating stable releases.
* It clearly does not impact existing functionality.
* The change is minimally invasive and scoped.
* It affects vendor-specific components rather than common ones.
* There is a justifiable use case (a clear user need).
* There is community consensus about the backport.

Performance improvements are generally not considered fixes,
but may be considered in cases where:

* It is fixing a performance regression that occurred previously.
* An existing feature in LTS is not usable as intended without it.

APIs marked as ``experimental`` are not considered part of the ABI version
and can be changed without prior notice in mainline development.
However, in LTS releases, ``experimental`` APIs should not be changed,
as compatibility with previous releases of an LTS version is paramount.


The Stable Mailing List
-----------------------

Stable and LTS releases are coordinated on the stable@dpdk.org mailing list.

All fix patches to the main branch that are candidates for backporting
should also be CCed to the `stable@dpdk.org <https://mails.dpdk.org/listinfo/stable>`_
mailing list.


Releasing
---------

A Stable Release is made by:

* Tagging the release with YY.MM.n (year, month, number).
* Uploading a tarball of the release to dpdk.org.
* Sending an announcement to the `announce@dpdk.org <https://mails.dpdk.org/listinfo/announce>`_
  list.

Stable releases are available on the `dpdk.org download page <https://core.dpdk.org/download/>`_.
