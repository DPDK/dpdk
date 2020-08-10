..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

.. submitting_patches:

Contributing Code to DPDK
=========================

This document outlines the guidelines for submitting code to DPDK.

The DPDK development process is modeled (loosely) on the Linux Kernel development model so it is worth reading the
Linux kernel guide on submitting patches:
`How to Get Your Change Into the Linux Kernel <https://www.kernel.org/doc/html/latest/process/submitting-patches.html>`_.
The rationale for many of the DPDK guidelines is explained in greater detail in the kernel guidelines.


The DPDK Development Process
----------------------------

The DPDK development process has the following features:

* The code is hosted in a public git repository.
* There is a mailing list where developers submit patches.
* There are maintainers for hierarchical components.
* Patches are reviewed publicly on the mailing list.
* Successfully reviewed patches are merged to the repository.
* Patches should be sent to the target repository or sub-tree, see below.
* All sub-repositories are merged into main repository for ``-rc1`` and ``-rc2`` versions of the release.
* After the ``-rc2`` release all patches should target the main repository.

The mailing list for DPDK development is `dev@dpdk.org <https://mails.dpdk.org/archives/dev/>`_.
Contributors will need to `register for the mailing list <https://mails.dpdk.org/listinfo/dev>`_ in order to submit patches.
It is also worth registering for the DPDK `Patchwork <https://patches.dpdk.org/project/dpdk/list/>`_

If you are using the GitHub service, you can link your repository to
the ``travis-ci.org`` build service.  When you push patches to your GitHub
repository, the travis service will automatically build your changes.

The development process requires some familiarity with the ``git`` version control system.
Refer to the `Pro Git Book <http://www.git-scm.com/book/>`_ for further information.

Source License
--------------

The DPDK uses the Open Source BSD-3-Clause license for the core libraries and
drivers. The kernel components are GPL-2.0 licensed. DPDK uses single line
reference to Unique License Identifiers in source files as defined by the Linux
Foundation's `SPDX project <http://spdx.org/>`_.

DPDK uses first line of the file to be SPDX tag. In case of *#!* scripts, SPDX
tag can be placed in 2nd line of the file.

For example, to label a file as subject to the BSD-3-Clause license,
the following text would be used:

``SPDX-License-Identifier: BSD-3-Clause``

To label a file as dual-licensed with BSD-3-Clause and GPL-2.0 (e.g., for code
that is shared between the kernel and userspace), the following text would be
used:

``SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)``

Refer to ``licenses/README`` for more details.

Maintainers and Sub-trees
-------------------------

The DPDK maintenance hierarchy is divided into a main repository ``dpdk`` and sub-repositories ``dpdk-next-*``.

There are maintainers for the trees and for components within the tree.

Trees and maintainers are listed in the ``MAINTAINERS`` file. For example::

    Crypto Drivers
    --------------
    M: Some Name <some.name@email.com>
    T: git://dpdk.org/next/dpdk-next-crypto

    Intel AES-NI GCM PMD
    M: Some One <some.one@email.com>
    F: drivers/crypto/aesni_gcm/
    F: doc/guides/cryptodevs/aesni_gcm.rst

Where:

* ``M`` is a tree or component maintainer.
* ``T`` is a repository tree.
* ``F`` is a maintained file or directory.

Additional details are given in the ``MAINTAINERS`` file.

The role of the component maintainers is to:

* Review patches for the component or delegate the review.
  The review should be done, ideally, within 1 week of submission to the mailing list.
* Add an ``acked-by`` to patches, or patchsets, that are ready for committing to a tree.
* Reply to questions asked about the component.

Component maintainers can be added or removed by submitting a patch to the ``MAINTAINERS`` file.
Maintainers should have demonstrated a reasonable level of contributions or reviews to the component area.
The maintainer should be confirmed by an ``ack`` from an established contributor.
There can be more than one component maintainer if desired.

The role of the tree maintainers is to:

* Maintain the overall quality of their tree.
  This can entail additional review, compilation checks or other tests deemed necessary by the maintainer.
* Commit patches that have been reviewed by component maintainers and/or other contributors.
  The tree maintainer should determine if patches have been reviewed sufficiently.
* Ensure that patches are reviewed in a timely manner.
* Prepare the tree for integration.
* Ensure that there is a designated back-up maintainer and coordinate a handover for periods where the
  tree maintainer can't perform their role.

Tree maintainers can be added or removed by submitting a patch to the ``MAINTAINERS`` file.
The proposer should justify the need for a new sub-tree and should have demonstrated a sufficient level of contributions in the area or to a similar area.
The maintainer should be confirmed by an ``ack`` from an existing tree maintainer.
Disagreements on trees or maintainers can be brought to the Technical Board.

The backup maintainer for the main tree should be selected
from the existing sub-tree maintainers of the project.
The backup maintainer for a sub-tree should be selected from among the component maintainers within that sub-tree.


Getting the Source Code
-----------------------

The source code can be cloned using either of the following:

main repository::

    git clone git://dpdk.org/dpdk
    git clone https://dpdk.org/git/dpdk

sub-repositories (`list <https://git.dpdk.org/next>`_)::

    git clone git://dpdk.org/next/dpdk-next-*
    git clone https://dpdk.org/git/next/dpdk-next-*

Make your Changes
-----------------

Make your planned changes in the cloned ``dpdk`` repo. Here are some guidelines and requirements:

* Follow the :ref:`coding_style` guidelines.

* If you add new files or directories you should add your name to the ``MAINTAINERS`` file.

* Initial submission of new PMDs should be prepared against a corresponding repo.

  * Thus, for example, initial submission of a new network PMD should be
    prepared against dpdk-next-net repo.

  * Likewise, initial submission of a new crypto or compression PMD should be
    prepared against dpdk-next-crypto repo.

  * For other PMDs and more info, refer to the ``MAINTAINERS`` file.

* New external functions should be added to the local ``version.map`` file. See
  the :doc:`ABI policy <abi_policy>` and :ref:`ABI versioning <abi_versioning>`
  guides. New external functions should also be added in alphabetical order.

* Important changes will require an addition to the release notes in ``doc/guides/rel_notes/``.
  See the :ref:`Release Notes section of the Documentation Guidelines <doc_guidelines>` for details.

* Test the compilation works with different targets, compilers and options, see :ref:`contrib_check_compilation`.

* Don't break compilation between commits with forward dependencies in a patchset.
  Each commit should compile on its own to allow for ``git bisect`` and continuous integration testing.

* Add tests to the ``app/test`` unit test framework where possible.

* Add documentation, if relevant, in the form of Doxygen comments or a User Guide in RST format.
  See the :ref:`Documentation Guidelines <doc_guidelines>`.

Once the changes have been made you should commit them to your local repo.

For small changes, that do not require specific explanations, it is better to keep things together in the
same patch.
Larger changes that require different explanations should be separated into logical patches in a patchset.
A good way of thinking about whether a patch should be split is to consider whether the change could be
applied without dependencies as a backport.

It is better to keep the related documentation changes in the same patch
file as the code, rather than one big documentation patch at the end of a
patchset. This makes it easier for future maintenance and development of the
code.

As a guide to how patches should be structured run ``git log`` on similar files.


Commit Messages: Subject Line
-----------------------------

The first, summary, line of the git commit message becomes the subject line of the patch email.
Here are some guidelines for the summary line:

* The summary line must capture the area and the impact of the change.

* The summary line should be around 50 characters.

* The summary line should be lowercase apart from acronyms.

* It should be prefixed with the component name (use git log to check existing components).
  For example::

     ixgbe: fix offload config option name

     config: increase max queues per port

* Use the imperative of the verb (like instructions to the code base).

* Don't add a period/full stop to the subject line or you will end up two in the patch name: ``dpdk_description..patch``.

The actual email subject line should be prefixed by ``[PATCH]`` and the version, if greater than v1,
for example: ``PATCH v2``.
The is generally added by ``git send-email`` or ``git format-patch``, see below.

If you are submitting an RFC draft of a feature you can use ``[RFC]`` instead of ``[PATCH]``.
An RFC patch doesn't have to be complete.
It is intended as a way of getting early feedback.


Commit Messages: Body
---------------------

Here are some guidelines for the body of a commit message:

* The body of the message should describe the issue being fixed or the feature being added.
  It is important to provide enough information to allow a reviewer to understand the purpose of the patch.

* When the change is obvious the body can be blank, apart from the signoff.

* The commit message must end with a ``Signed-off-by:`` line which is added using::

      git commit --signoff # or -s

  The purpose of the signoff is explained in the
  `Developer's Certificate of Origin <https://www.kernel.org/doc/html/latest/process/submitting-patches.html#developer-s-certificate-of-origin-1-1>`_
  section of the Linux kernel guidelines.

  .. Note::

     All developers must ensure that they have read and understood the
     Developer's Certificate of Origin section of the documentation prior
     to applying the signoff and submitting a patch.

* The signoff must be a real name and not an alias or nickname.
  More than one signoff is allowed.

* The text of the commit message should be wrapped at 72 characters.

* When fixing a regression, it is required to reference the id of the commit
  which introduced the bug, and put the original author of that commit on CC.
  You can generate the required lines using the following git alias, which prints
  the commit SHA and the author of the original code::

     git config alias.fixline "log -1 --abbrev=12 --format='Fixes: %h (\"%s\")%nCc: %ae'"

  The output of ``git fixline <SHA>`` must then be added to the commit message::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Fixes: abcdefgh1234 ("doc: add some parameter")
     Cc: author@example.com

     Signed-off-by: Alex Smith <alex.smith@example.com>

* When fixing an error or warning it is useful to add the error message and instructions on how to reproduce it.

* Use correct capitalization, punctuation and spelling.

In addition to the ``Signed-off-by:`` name the commit messages can also have
tags for who reported, suggested, tested and reviewed the patch being
posted. Please refer to the `Tested, Acked and Reviewed by`_ section.

Patch Fix Related Issues
~~~~~~~~~~~~~~~~~~~~~~~~

`Coverity <https://scan.coverity.com/projects/dpdk-data-plane-development-kit>`_
is a tool for static code analysis.
It is used as a cloud-based service used to scan the DPDK source code,
and alert developers of any potential defects in the source code.
When fixing an issue found by Coverity, the patch must contain a Coverity issue ID
in the body of the commit message. For example::


     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Coverity issue: 12345
     Fixes: abcdefgh1234 ("doc: add some parameter")
     Cc: author@example.com

     Signed-off-by: Alex Smith <alex.smith@example.com>


`Bugzilla <https://bugs.dpdk.org>`_
is a bug- or issue-tracking system.
Bug-tracking systems allow individual or groups of developers
effectively to keep track of outstanding problems with their product.
When fixing an issue raised in Bugzilla, the patch must contain
a Bugzilla issue ID in the body of the commit message.
For example::

    doc: fix some parameter description

    Update the docs, fixing description of some parameter.

    Bugzilla ID: 12345
    Fixes: abcdefgh1234 ("doc: add some parameter")
    Cc: author@example.com

    Signed-off-by: Alex Smith <alex.smith@example.com>

Patch for Stable Releases
~~~~~~~~~~~~~~~~~~~~~~~~~

All fix patches to the main branch that are candidates for backporting
should also be CCed to the `stable@dpdk.org <https://mails.dpdk.org/listinfo/stable>`_
mailing list.
In the commit message body the Cc: stable@dpdk.org should be inserted as follows::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Fixes: abcdefgh1234 ("doc: add some parameter")
     Cc: stable@dpdk.org

     Signed-off-by: Alex Smith <alex.smith@example.com>

For further information on stable contribution you can go to
:doc:`Stable Contribution Guide <stable>`.

Patch Dependencies
~~~~~~~~~~~~~~~~~~

Sometimes a patch or patchset can depend on another one.
To help the maintainers and automation tasks, please document this dependency in commit log or cover letter
with the following syntax:

``Depends-on: series-NNNNN ("Title of the series")`` or ``Depends-on: patch-NNNNN ("Title of the patch")``

Where ``NNNNN`` is patchwork ID for patch or series::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Signed-off-by: Alex Smith <alex.smith@example.com>
     ---
     Depends-on: series-10000 ("Title of the series")

Creating Patches
----------------

It is possible to send patches directly from git but for new contributors it is recommended to generate the
patches with ``git format-patch`` and then when everything looks okay, and the patches have been checked, to
send them with ``git send-email``.

Here are some examples of using ``git format-patch`` to generate patches:

.. code-block:: console

   # Generate a patch from the last commit.
   git format-patch -1

   # Generate a patch from the last 3 commits.
   git format-patch -3

   # Generate the patches in a directory.
   git format-patch -3 -o ~/patch/

   # Add a cover letter to explain a patchset.
   git format-patch -3 -o ~/patch/ --cover-letter

   # Add a prefix with a version number.
   git format-patch -3 -o ~/patch/ -v 2


Cover letters are useful for explaining a patchset and help to generate a logical threading to the patches.
Smaller notes can be put inline in the patch after the ``---`` separator, for example::

   Subject: [PATCH] fm10k/base: add FM10420 device ids

   Add the device ID for Boulder Rapids and Atwood Channel to enable
   drivers to support those devices.

   Signed-off-by: Alex Smith <alex.smith@example.com>
   ---

   ADD NOTES HERE.

    drivers/net/fm10k/base/fm10k_api.c  | 6 ++++++
    drivers/net/fm10k/base/fm10k_type.h | 6 ++++++
    2 files changed, 12 insertions(+)
   ...

Version 2 and later of a patchset should also include a short log of the changes so the reviewer knows what has changed.
This can be added to the cover letter or the annotations.
For example::

   ---
   v3:
   * Fixed issued with version.map.

   v2:
   * Added i40e support.
   * Renamed ethdev functions from rte_eth_ieee15888_*() to rte_eth_timesync_*()
     since 802.1AS can be supported through the same interfaces.


.. _contrib_checkpatch:

Checking the Patches
--------------------

Patches should be checked for formatting and syntax issues using the ``checkpatches.sh`` script in the ``devtools``
directory of the DPDK repo.
This uses the Linux kernel development tool ``checkpatch.pl`` which  can be obtained by cloning, and periodically,
updating the Linux kernel sources.

The path to the original Linux script must be set in the environment variable ``DPDK_CHECKPATCH_PATH``.

Spell checking of commonly misspelled words
can be enabled by downloading the codespell dictionary::

   https://raw.githubusercontent.com/codespell-project/codespell/master/codespell_lib/data/dictionary.txt

The path to the downloaded ``dictionary.txt`` must be set
in the environment variable ``DPDK_CHECKPATCH_CODESPELL``.

Environment variables required by the development tools,
are loaded from the following files, in order of preference::

   .develconfig
   ~/.config/dpdk/devel.config
   /etc/dpdk/devel.config.

Once the environment variable is set, the script can be run as follows::

   devtools/checkpatches.sh ~/patch/

The script usage is::

   checkpatches.sh [-h] [-q] [-v] [-nX|-r range|patch1 [patch2] ...]

Then the git logs should be checked using the ``check-git-log.sh`` script.

The script usage is::

   check-git-log.sh [-h] [-nX|-r range]

For both of the above scripts, the -n option is used to specify a number of commits from HEAD,
and the -r option allows the user specify a ``git log`` range.

.. _contrib_check_compilation:

Checking Compilation
--------------------

Compilation of patches is to be tested with ``devtools/test-meson-builds.sh`` script.

The script internally checks for dependencies, then builds for several
combinations of compilation configuration.
By default, each build will be put in a subfolder of the current working directory.
However, if it is preferred to place the builds in a different location,
the environment variable ``DPDK_BUILD_TEST_DIR`` can be set to that desired location.
For example, setting ``DPDK_BUILD_TEST_DIR=__builds`` will put all builds
in a single subfolder called "__builds" created in the current directory.
Setting ``DPDK_BUILD_TEST_DIR`` to an absolute directory path e.g. ``/tmp`` is also supported.


.. _integrated_abi_check:

Checking ABI compatibility
--------------------------

By default, ABI compatibility checks are disabled.

To enable them, a reference version must be selected via the environment
variable ``DPDK_ABI_REF_VERSION``. Contributors should ordinarily reference the
git tag of the most recent release of DPDK in ``DPDK_ABI_REF_VERSION``.

The ``devtools/test-meson-builds.sh`` script then build this reference version
in a temporary directory and store the results in a subfolder of the current
working directory.
The environment variable ``DPDK_ABI_REF_DIR`` can be set so that the results go
to a different location.

Sample::

   DPDK_ABI_REF_VERSION=v19.11 DPDK_ABI_REF_DIR=/tmp ./devtools/test-meson-builds.sh


Sending Patches
---------------

Patches should be sent to the mailing list using ``git send-email``.
You can configure an external SMTP with something like the following::

   [sendemail]
       smtpuser = name@domain.com
       smtpserver = smtp.domain.com
       smtpserverport = 465
       smtpencryption = ssl

See the `Git send-email <https://git-scm.com/docs/git-send-email>`_ documentation for more details.

The patches should be sent to ``dev@dpdk.org``.
If the patches are a change to existing files then you should send them TO the maintainer(s) and CC ``dev@dpdk.org``.
The appropriate maintainer can be found in the ``MAINTAINERS`` file::

   git send-email --to maintainer@some.org --cc dev@dpdk.org 000*.patch

Script ``get-maintainer.sh`` can be used to select maintainers automatically::

  git send-email --to-cmd ./devtools/get-maintainer.sh --cc dev@dpdk.org 000*.patch

New additions can be sent without a maintainer::

   git send-email --to dev@dpdk.org 000*.patch

You can test the emails by sending it to yourself or with the ``--dry-run`` option.

If the patch is in relation to a previous email thread you can add it to the same thread using the Message ID::

   git send-email --to dev@dpdk.org --in-reply-to <1234-foo@bar.com> 000*.patch

The Message ID can be found in the raw text of emails or at the top of each Patchwork patch,
`for example <https://patches.dpdk.org/patch/7646/>`_.
Shallow threading (``--thread --no-chain-reply-to``) is preferred for a patch series.

Once submitted your patches will appear on the mailing list and in Patchwork.

Experienced committers may send patches directly with ``git send-email`` without the ``git format-patch`` step.
The options ``--annotate`` and ``confirm = always`` are recommended for checking patches before sending.


Backporting patches for Stable Releases
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes a maintainer or contributor wishes, or can be asked, to send a patch
for a stable release rather than mainline.
In this case the patch(es) should be sent to ``stable@dpdk.org``,
not to ``dev@dpdk.org``.

Given that there are multiple stable releases being maintained at the same time,
please specify exactly which branch(es) the patch is for
using ``git send-email --subject-prefix='PATCH 16.11' ...``
and also optionally in the cover letter or in the annotation.


The Review Process
------------------

Patches are reviewed by the community, relying on the experience and
collaboration of the members to double-check each other's work. There are a
number of ways to indicate that you have checked a patch on the mailing list.


Tested, Acked and Reviewed by
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To indicate that you have interacted with a patch on the mailing list you
should respond to the patch in an email with one of the following tags:

 * Reviewed-by:
 * Acked-by:
 * Tested-by:
 * Reported-by:
 * Suggested-by:

The tag should be on a separate line as follows::

   tag-here: Name Surname <email@address.com>

Each of these tags has a specific meaning. In general, the DPDK community
follows the kernel usage of the tags. A short summary of the meanings of each
tag is given here for reference:

.. _statement: https://www.kernel.org/doc/html/latest/process/submitting-patches.html#reviewer-s-statement-of-oversight

``Reviewed-by:`` is a strong statement_ that the patch is an appropriate state
for merging without any remaining serious technical issues. Reviews from
community members who are known to understand the subject area and to perform
thorough reviews will increase the likelihood of the patch getting merged.

``Acked-by:`` is a record that the person named was not directly involved in
the preparation of the patch but wishes to signify and record their acceptance
and approval of it.

``Tested-by:`` indicates that the patch has been successfully tested (in some
environment) by the person named.

``Reported-by:`` is used to acknowledge person who found or reported the bug.

``Suggested-by:`` indicates that the patch idea was suggested by the named
person.



Steps to getting your patch merged
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The more work you put into the previous steps the easier it will be to get a
patch accepted. The general cycle for patch review and acceptance is:

#. Submit the patch.

#. Check the automatic test reports in the coming hours.

#. Wait for review comments. While you are waiting review some other patches.

#. Fix the review comments and submit a ``v n+1`` patchset::

      git format-patch -3 -v 2

#. Update Patchwork to mark your previous patches as "Superseded".

#. If the patch is deemed suitable for merging by the relevant maintainer(s) or other developers they will ``ack``
   the patch with an email that includes something like::

      Acked-by: Alex Smith <alex.smith@example.com>

   **Note**: When acking patches please remove as much of the text of the patch email as possible.
   It is generally best to delete everything after the ``Signed-off-by:`` line.

#. Having the patch ``Reviewed-by:`` and/or ``Tested-by:`` will also help the patch to be accepted.

#. If the patch isn't deemed suitable based on being out of scope or conflicting with existing functionality
   it may receive a ``nack``.
   In this case you will need to make a more convincing technical argument in favor of your patches.

#. In addition a patch will not be accepted if it doesn't address comments from a previous version with fixes or
   valid arguments.

#. It is the responsibility of a maintainer to ensure that patches are reviewed and to provide an ``ack`` or
   ``nack`` of those patches as appropriate.

#. Once a patch has been acked by the relevant maintainer, reviewers may still comment on it for a further
   two weeks. After that time, the patch should be merged into the relevant git tree for the next release.
   Additional notes and restrictions:

   * Patches should be acked by a maintainer at least two days before the release merge
     deadline, in order to make that release.
   * For patches acked with less than two weeks to go to the merge deadline, all additional
     comments should be made no later than two days before the merge deadline.
   * After the appropriate time for additional feedback has passed, if the patch has not yet
     been merged to the relevant tree by the committer, it should be treated as though it had,
     in that any additional changes needed to it must be addressed by a follow-on patch, rather
     than rework of the original.
   * Trivial patches may be merged sooner than described above at the tree committer's
     discretion.
