..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

.. stable_lts_releases:

DPDK Stable Releases and Long Term Support
==========================================

This section sets out the guidelines for the DPDK Stable Releases and the DPDK
Long Term Support releases (LTS).


Introduction
------------

The purpose of the DPDK Stable Releases is to maintain releases of DPDK with
backported fixes over an extended period of time. This provides downstream
consumers of DPDK with a stable target on which to base applications or
packages.

The Long Term Support release (LTS) is a designation applied to a Stable
Release to indicate longer term support.


Stable Releases
---------------

Any major release of DPDK can be designated as a Stable Release if a
maintainer volunteers to maintain it.

A Stable Release is used to backport fixes from an ``N`` release back to an
``N-1`` release, for example, from 16.11 to 16.07.

The duration of a stable is one complete release cycle (3 months). It can be
longer, up to 1 year, if a maintainer continues to support the stable branch,
or if users supply backported fixes, however the explicit commitment should be
for one release cycle.

The release cadence is determined by the maintainer based on the number of
bugfixes and the criticality of the bugs. Releases should be coordinated with
the validation engineers to ensure that a tagged release has been tested.


LTS Release
-----------

A stable release can be designated as an LTS release based on community
agreement and a commitment from a maintainer. The current policy is that each
year's November release will be maintained as an LTS for 2 years.

The current DPDK LTS releases are 16.11 and 17.11.

It is anticipated that there will be at least 4 releases per year of the LTS
or approximately 1 every 3 months. However, the cadence can be shorter or
longer depending on the number and criticality of the backported
fixes. Releases should be coordinated with the validation engineers to ensure
that a tagged release has been tested.


What changes should be backported
---------------------------------

Backporting should be limited to bug fixes. All patches accepted on the master
branch with a Fixes: tag should be backported to the relevant stable/LTS
branches, unless the submitter indicates otherwise. If there are exceptions,
they will be discussed on the mailing lists.

Fixes suitable for backport should have a ``Cc: stable@dpdk.org`` tag in the
commit message body as follows::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Fixes: abcdefgh1234 ("doc: add some parameter")
     Cc: stable@dpdk.org

     Signed-off-by: Alex Smith <alex.smith@example.com>


Fixes not suitable for backport should not include the ``Cc: stable@dpdk.org`` tag.

Features should not be backported to stable releases. It may be acceptable, in
limited cases, to back port features for the LTS release where:

* There is a justifiable use case (for example a new PMD).
* The change is non-invasive.
* The work of preparing the backport is done by the proposer.
* There is support within the community.


The Stable Mailing List
-----------------------

The Stable and LTS release are coordinated on the stable@dpdk.org mailing
list.

All fix patches to the master branch that are candidates for backporting
should also be CCed to the `stable@dpdk.org <http://mails.dpdk.org/listinfo/stable>`_
mailing list.


Releasing
---------

A Stable Release will be released by:

* Tagging the release with YY.MM.n (year, month, number).
* Uploading a tarball of the release to dpdk.org.
* Sending an announcement to the `announce@dpdk.org <http://mails.dpdk.org/listinfo/announce>`_
  list.

Stable releases are available on the `dpdk.org download page <http://core.dpdk.org/download/>`_.


ABI
---

The Stable Release should not be seen as a way of breaking or circumventing
the DPDK ABI policy.
