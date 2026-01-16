..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

.. submitting_patches:

Contributing Code to DPDK
=========================

This document provides guidelines for submitting code to DPDK.

The DPDK development process is loosely based on the Linux Kernel development model.
Review the Linux kernel's guide:
`How to Get Your Change Into the Linux Kernel <https://www.kernel.org/doc/html/latest/process/submitting-patches.html>`_.
Many of DPDK's submission guidelines draw from the kernel process,
and the rationale behind them is often explained in greater depth there.


The DPDK Development Process
----------------------------

The DPDK development process includes the following key elements:

* The project maintains code in a public Git repository.
* Developers submit patches via a public mailing list.
* A hierarchical structure assigns maintainers to different components.
* The community reviews patches publicly on the mailing list.
* Maintainers merge reviewed patches into the repository.
* Contributors should send patches to the target repository or sub-tree (see below for details).

The mailing list for DPDK development is `dev@dpdk.org <https://mails.dpdk.org/archives/dev/>`_.
Contributors must `register for the mailing list <https://mails.dpdk.org/listinfo/dev>`_
to submit patches.
Register for DPDK `Patchwork <https://patches.dpdk.org/project/dpdk/list/>`_ as well.

If you use GitHub, pushing to a branch triggers GitHub Actions,
which builds your changes and runs unit tests and ABI checks.

Contributing to DPDK requires a basic understanding of the Git version control system.
For more information, refer to the `Pro Git Book <http://www.git-scm.com/book/>`_.

Source License
--------------

DPDK uses the Open Source BSD-3-Clause license for its core libraries and drivers.
Kernel components use the GPL-2.0 license.
To identify licenses in source files, DPDK follows the SPDX standard
developed by the Linux Foundation `SPDX project <http://spdx.org/>`_.

Use the SPDX tag on the first line of the file.
For *#!* scripts, place the SPDX tag on the second line.

For BSD-3-Clause:

``SPDX-License-Identifier: BSD-3-Clause``

For dual-licensing with BSD-3-Clause and GPL-2.0 (e.g., shared kernel/user-space code):

``SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0``

Refer to ``licenses/README`` for more details.

Maintainers and Sub-trees
-------------------------

The DPDK maintenance hierarchy divides into a main repository ``dpdk`` and sub-repositories ``dpdk-next-*``.

Maintainers exist for the trees and for components within the tree.

The ``MAINTAINERS`` file lists trees and maintainers. For example::

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

The ``MAINTAINERS`` file contains additional details.

Component maintainers are responsible for:

* Reviewing patches related to their component or delegating review to others.
  Ideally, complete reviews within one week of mailing list submission.
* Acknowledging patches by adding an ``acked-by`` tag to those deemed ready for merging.
* Responding to questions regarding the component and offering guidance when needed.

Add or remove maintainers by submitting a patch to the ``MAINTAINERS`` file.
New maintainers should have demonstrated consistent contributions or reviews to the component area.
An established contributor must confirm their addition with an ``ack``.
A single component may have multiple maintainers if needed.

Tree Maintainer Responsibilities:

Tree maintainers ensure the overall quality and integrity of their tree.
Their duties include:

* Conducting additional reviews, compilation checks, or other tests as needed.
* Committing patches that component maintainers or other contributors have sufficiently reviewed.
* Ensuring timely review of submitted patches.
* Preparing the tree for integration.
* Appointing a designated backup maintainer and coordinating handovers when unavailable.

Maintainer Changes:

Add or remove tree maintainers by submitting a patch to the MAINTAINERS file.
Proposals must justify the creation of a new sub-tree
and demonstrate significant contribution to the relevant area.
An existing tree maintainer must ack the proposal.

Escalate disputes regarding trees or maintainers to the Technical Board.

Backup Maintainers:

* Choose the main tree's backup maintainer from among existing sub-tree maintainers.
* Choose a sub-tree's backup from the component maintainers within that sub-tree.


Getting the Source Code
-----------------------

Clone the source code using either of the following:

main repository::

    git clone git://dpdk.org/dpdk
    git clone https://dpdk.org/git/dpdk

sub-repositories (`list <https://git.dpdk.org/next>`_)::

    git clone git://dpdk.org/next/dpdk-next-*
    git clone https://dpdk.org/git/next/dpdk-next-*

Make Changes
------------

After cloning the DPDK repository, make your planned changes
while following these key guidelines and requirements:

* Follow the :doc:`coding_style` guidelines.

* If you are a new contributor, or if your mail address changed,
  you may update the ``.mailmap`` file.
  Otherwise a maintainer will add the new name or address.
  Keeping this file up-to-date helps when someone wants to contact you
  about the changes you contributed.

* If you add new files or directories, add your name to the ``MAINTAINERS`` file.

PMD Submissions
~~~~~~~~~~~~~~~

* Prepare initial submissions of new PMDs against the corresponding repo.

  * For example, prepare initial submission of a new network PMD
    against the dpdk-next-net repo.

  * Likewise, prepare initial submission of a new crypto or compression PMD
    against the dpdk-next-crypto repo.

  * For other PMDs and more info, refer to the ``MAINTAINERS`` file.

* Export new external functions.
  See the :doc:`abi_policy` and :doc:`abi_versioning` guides.

API and ABI Guidelines
~~~~~~~~~~~~~~~~~~~~~~

* Use any new API function in the ``/app`` test directory.

* When introducing a new device API, at least one driver should implement it.

Documentation and Release Notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Important changes require an addition to the release notes in ``doc/guides/rel_notes/``.
  See the :ref:`release notes guidelines <style_release_notes_guidelines>`.

* Test compilation with different targets, compilers, and options;
  see :ref:`contrib_check_compilation`.

* Do not break compilation between commits with forward dependencies in a patch set.
  Each commit should compile on its own to allow ``git bisect`` and continuous integration testing.

* Add tests to the ``app/test`` unit test framework where possible.

* Add documentation, if relevant, in the form of Doxygen comments or a User Guide in RST format.
  See the :doc:`documentation`.

* Update code and related documentation atomically in the same patch.

Commit your changes to your local repo after making them.

Keep small changes that do not require specific explanations in a single patch.
Separate larger changes that require different explanations into logical patches in a patch set.
Consider whether the change could be applied without dependencies as a backport
when deciding whether to split a patch.

Run ``git log`` on similar files as a guide to how patches should be structured.


Commit Messages: Subject Line
-----------------------------

The first, summary, line of the git commit message becomes the subject line of the patch email.
Here are some guidelines for the summary line:

* The summary line must capture the area and the impact of the change.

* The summary line should be around 50 characters.

* The summary line should be lowercase apart from acronyms.

* Prefix with the component name (use git log to check existing components).
  For example::

     ixgbe: fix offload config option name

     config: increase max queues per port

* Use the imperative of the verb (like instructions to the code base).

* Do not add a period/full stop to the subject line or you will end up with two in the patch name: ``dpdk_description..patch``.

The actual email subject line should be prefixed by ``[PATCH]`` and the version, if greater than v1,
for example: ``PATCH v2``.
``git send-email`` or ``git format-patch`` generally adds this prefix; see below.

If you are submitting an RFC draft of a feature, use ``[RFC]`` instead of ``[PATCH]``.
An RFC patch does not have to be complete.
It is intended as a way of getting early feedback.


Commit Messages: Body
---------------------

Here are guidelines for the body of a commit message:

* The body of the message should describe the issue being fixed or the feature being added.
  Provide enough information to allow a reviewer to understand the purpose of the patch.

* When the change is obvious, the body can be blank apart from the signoff.

* The commit message must end with a ``Signed-off-by:`` line, added using::

      git commit --signoff # or -s

  The
  `Developer's Certificate of Origin <https://www.kernel.org/doc/html/latest/process/submitting-patches.html#developer-s-certificate-of-origin-1-1>`_
  section of the Linux kernel guidelines explains the purpose of the signoff.

  .. Note::

     All developers must read and understand the
     Developer's Certificate of Origin section of the documentation prior
     to applying the signoff and submitting a patch.

* The signoff must be a real name and not an alias or nickname.
  More than one signoff is allowed.

* Wrap the text of the commit message at 72 characters.

* When fixing a regression, reference the id of the commit
  which introduced the bug, and put the original author of that commit on CC.
  Generate the required lines using the following git alias, which prints
  the commit SHA and the author of the original code::

     git config alias.fixline "log -1 --abbrev=12 --format='Fixes: %h (\"%s\")%nCc: %ae'"

  Add the output of ``git fixline <SHA>`` to the commit message::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Fixes: abcdefgh1234 ("doc: add some parameter")

     Signed-off-by: Alex Smith <alex.smith@example.com>
     ---
     Cc: author@example.com

* When fixing an error or warning, include the error message and instructions on how to reproduce it.

* Use correct capitalization, punctuation, and spelling.

In addition to the ``Signed-off-by:`` name, commit messages can also have tags
for who reported, suggested, tested, and reviewed the patch being posted.
Please refer to the `Tested, Acked and Reviewed by`_ section.

Patch Fix Related Issues
~~~~~~~~~~~~~~~~~~~~~~~~

`Coverity <https://scan.coverity.com/projects/dpdk-data-plane-development-kit>`_
is a tool for static code analysis.
It is used as a cloud-based service to scan the DPDK source code
and alert developers of any potential defects in the source code.
When fixing an issue found by Coverity, the patch must contain a Coverity issue ID
in the body of the commit message. For example::


     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Coverity issue: 12345
     Fixes: abcdefgh1234 ("doc: add some parameter")

     Signed-off-by: Alex Smith <alex.smith@example.com>
     ---
     Cc: author@example.com


`Bugzilla <https://bugs.dpdk.org>`_
is a bug- or issue-tracking system.
Bug-tracking systems allow individual or groups of developers
to effectively track outstanding problems with their product.
When fixing an issue raised in Bugzilla, the patch must contain
a Bugzilla issue ID in the body of the commit message.
For example::

    doc: fix some parameter description

    Update the docs, fixing description of some parameter.

    Bugzilla ID: 12345
    Fixes: abcdefgh1234 ("doc: add some parameter")

    Signed-off-by: Alex Smith <alex.smith@example.com>
    ---
    Cc: author@example.com

Patch for Stable Releases
~~~~~~~~~~~~~~~~~~~~~~~~~

CC all fix patches to the main branch that are candidates for backporting
to the `stable@dpdk.org <https://mails.dpdk.org/listinfo/stable>`_
mailing list.
In the commit message body, insert ``Cc: stable@dpdk.org`` as follows::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Fixes: abcdefgh1234 ("doc: add some parameter")
     Cc: stable@dpdk.org

     Signed-off-by: Alex Smith <alex.smith@example.com>

For further information on stable contribution, see
:doc:`Stable Contribution Guide <stable>`.

Patch Dependencies
~~~~~~~~~~~~~~~~~~

Sometimes a patch or patch set depends on another one.
To help the maintainers and automation tasks, document this dependency in the commit log or cover letter
with the following syntax:

``Depends-on: series-NNNNN ("Title of the series")`` or ``Depends-on: patch-NNNNN ("Title of the patch")``

Where ``NNNNN`` is the patchwork ID for the patch or series::

     doc: fix some parameter description

     Update the docs, fixing description of some parameter.

     Signed-off-by: Alex Smith <alex.smith@example.com>
     ---
     Depends-on: series-10000 ("Title of the series")

Tag order
~~~~~~~~~

A pattern indicates how certain tags should relate to each other.

Example of proper tag sequence::

     Coverity issue:
     Bugzilla ID:
     Fixes:
     Cc:

     Reported-by:
     Suggested-by:
     Signed-off-by:
     Acked-by:
     Reviewed-by:
     Tested-by:

An empty line separates the first and second tag sections.

While ``Signed-off-by:`` is obligatory and must exist in each commit,
all other tags are optional.
Any tag, as long as it is in proper location relative to other adjacent tags (if present),
may occur multiple times.

Place tags after the first occurrence of ``Signed-off-by:``
in chronological order.


Creating Patches
----------------

You can send patches directly from git,
but for new contributors it is recommended to generate the patches with ``git format-patch``
and then, when everything looks okay and the patches have been checked,
send them with ``git send-email``.

Here are some examples of using ``git format-patch`` to generate patches:

.. code-block:: console

   # Generate a patch from the last commit.
   git format-patch -1

   # Generate a patch from the last 3 commits.
   git format-patch -3

   # Generate the patches in a directory.
   git format-patch -3 -o ~/patch/

   # Add a cover letter to explain a patch set.
   git format-patch -3 -o ~/patch/ --cover-letter

   # Add a prefix with a version number.
   git format-patch -3 -o ~/patch/ -v 2


Cover letters are useful for explaining a patch set
and help generate logical threading to the patches.
Put smaller notes inline in the patch after the ``---`` separator, for example::

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

Version 2 and later of a patch set should also include a short log of the changes
so the reviewer knows what has changed.
Add this to the cover letter or the annotations.
For example::

   ---
   v3:
   * Fixed issued with version.map.

   v2:
   * Added i40e support.
   * Renamed ethdev functions from rte_eth_ieee15888_*() to rte_eth_timesync_*()
     since 802.1AS can be supported through the same interfaces.


Checking the Patches
--------------------

Check patches for formatting and syntax issues
using the ``checkpatches.sh`` script in the ``devtools`` directory of the DPDK repo.
This uses the Linux kernel development tool ``checkpatch.pl``,
which can be obtained by cloning, and periodically updating, the Linux kernel sources.

Set the path to the original Linux script in the environment variable ``DPDK_CHECKPATCH_PATH``.

Spell checking of commonly misspelled words is enabled
by default if installed in ``/usr/share/codespell/dictionary.txt``.
Specify a different dictionary path
in the environment variable ``DPDK_CHECKPATCH_CODESPELL``.

A DPDK script builds an adjusted dictionary
from the multiple codespell dictionaries::

   git clone https://github.com/codespell-project/codespell.git
   devtools/build-dict.sh codespell/ > codespell-dpdk.txt

The development tools load environment variables
from the following files, in order of preference::

   .develconfig
   ~/.config/dpdk/devel.config
   /etc/dpdk/devel.config.

Once the environment variable is set, run the script as follows::

   devtools/checkpatches.sh ~/patch/

The script usage is::

   checkpatches.sh [-h] [-q] [-v] [-nX|-r range|patch1 [patch2] ...]

Then check the git logs using the ``check-git-log.sh`` script.

The script usage is::

   check-git-log.sh [-h] [-nX|-r range]

For both scripts, the -n option specifies a number of commits from HEAD,
and the -r option specifies a ``git log`` range.

Additionally, when contributing to the DTS tool, check patches using
the ``dts-check-format.sh`` script in the ``devtools`` directory of the DPDK repo.
Running the script requires extra :ref:`Python dependencies <dts_deps>`.


.. _contrib_ai_assisted_review:

AI-Assisted Patch Review
------------------------

Contributors may optionally use the ``review-patch.py`` script
to get an AI-assisted review of patches before submitting them to the mailing list.
The script checks patches against the DPDK coding standards
and contribution guidelines documented in ``AGENTS.md``.

The script supports multiple AI providers
(Anthropic Claude, OpenAI ChatGPT, xAI Grok, Google Gemini).
An API key for the chosen provider must be set
in the corresponding environment variable (see ``--list-providers``).

Basic usage::

   # Review a single patch (default provider: Anthropic Claude)
   devtools/ai/review-patch.py my-patch.patch

   # Use a different provider
   devtools/ai/review-patch.py -p openai my-patch.patch

   # Review for an LTS branch (enables stricter rules)
   devtools/ai/review-patch.py -r 24.11 my-patch.patch

   # List available providers and their API key variables
   devtools/ai/review-patch.py --list-providers

For a patch series in an mbox file,
the ``--split-patches`` option reviews each patch individually::

   devtools/ai/review-patch.py --split-patches series.mbox

   # Review only a range of patches
   devtools/ai/review-patch.py --split-patches --patch-range 1-5 series.mbox

When reviewing for a Long Term Stable (LTS) release,
use the ``-r`` option with the target version.
Any DPDK release with minor version ``.11`` (e.g., 23.11, 24.11)
is automatically recognized as LTS,
and the script will enforce stricter rules: bug fixes only, no new features or API.

Output can be formatted as plain text (default), Markdown, HTML, or JSON::

   devtools/ai/review-patch.py -f markdown -o review.md my-patch.patch

The review guidelines in ``AGENTS.md`` focus on correctness bug detection
and other DPDK-specific requirements.
Commit message formatting and SPDX/copyright compliance
are checked by ``checkpatches.sh`` and are not duplicated in the AI review.

.. note::

   Always verify AI suggestions before acting on them.


.. _contrib_check_compilation:

Checking Compilation
--------------------

Test compilation of patches with the ``devtools/test-meson-builds.sh`` script.

The script internally checks for dependencies, then builds for several
combinations of compilation configuration.
By default, each build goes in a subfolder of the current working directory.
However, to place the builds in a different location,
set the environment variable ``DPDK_BUILD_TEST_DIR`` to that desired location.
For example, setting ``DPDK_BUILD_TEST_DIR=__builds`` puts all builds
in a single subfolder called "__builds" created in the current directory.
Setting ``DPDK_BUILD_TEST_DIR`` to an absolute directory path (e.g., ``/tmp``) is also supported.


.. _contrib_integrated_abi_check:

Checking ABI compatibility
--------------------------

By default, ABI compatibility checks are disabled.

To enable them, select a reference version via the environment
variable ``DPDK_ABI_REF_VERSION``. Contributors should ordinarily reference the
git tag of the most recent release of DPDK in ``DPDK_ABI_REF_VERSION``.

The ``devtools/test-meson-builds.sh`` script then builds this reference version
in a temporary directory and stores the results in a subfolder of the current
working directory.
Set the environment variable ``DPDK_ABI_REF_DIR`` to place the results in a different location.

Sample::

   DPDK_ABI_REF_VERSION=v19.11 DPDK_ABI_REF_DIR=/tmp ./devtools/test-meson-builds.sh


Sending Patches
---------------

Send patches to the mailing list using ``git send-email``.
Configure an external SMTP with something like the following::

   [sendemail]
       smtpuser = name@domain.com
       smtpserver = smtp.domain.com
       smtpserverport = 465
       smtpencryption = ssl

See the `Git send-email <https://git-scm.com/docs/git-send-email>`_ documentation for more details.

Send patches to ``dev@dpdk.org``.
If the patches change existing files, send them TO the maintainer(s) and CC ``dev@dpdk.org``.
Find the appropriate maintainer in the ``MAINTAINERS`` file::

   git send-email --to maintainer@some.org --cc dev@dpdk.org 000*.patch

The script ``get-maintainer.sh`` can select maintainers automatically::

  git send-email --to-cmd ./devtools/get-maintainer.sh --cc dev@dpdk.org 000*.patch

Send new additions without a maintainer::

   git send-email --to dev@dpdk.org 000*.patch

Test the emails by sending them to yourself or with the ``--dry-run`` option.

If the patch relates to a previous email thread,
add it to the same thread using the Message ID::

   git send-email --to dev@dpdk.org --in-reply-to <1234-foo@bar.com> 000*.patch

Find the Message ID in the raw text of emails or at the top of each Patchwork patch,
`for example <https://patches.dpdk.org/patch/7646/>`_.
Shallow threading (``--thread --no-chain-reply-to``) is preferred for a patch series.

Once submitted, your patches appear on the mailing list and in Patchwork.

Experienced committers may send patches directly with ``git send-email`` without the ``git format-patch`` step.
The options ``--annotate`` and ``confirm = always`` are recommended for checking patches before sending.


Backporting patches for Stable Releases
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes a maintainer or contributor wishes, or can be asked, to send a patch
for a stable release rather than mainline.
In this case, send the patch(es) to ``stable@dpdk.org``,
not to ``dev@dpdk.org``.

Given that multiple stable releases are maintained simultaneously,
specify exactly which branch(es) the patch is for
using ``git send-email --subject-prefix='PATCH 16.11' ...``
and also optionally in the cover letter or in the annotation.


The Review Process
------------------

The community reviews patches, relying on the experience and
collaboration of the members to double-check each other's work. There are a
number of ways to indicate that you have checked a patch on the mailing list.


Tested, Acked and Reviewed by
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To indicate that you have interacted with a patch on the mailing list,
respond to the patch in an email with one of the following tags:

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

``Reviewed-by:`` is a strong statement_ that the patch is in an appropriate state
for merging without any remaining serious technical issues. Reviews from
community members who understand the subject area
and perform thorough reviews increase the likelihood of the patch getting merged.

``Acked-by:`` records that the person named was not directly involved in preparing the patch
but wishes to signify and record their acceptance and approval of it.

``Tested-by:`` indicates that the person named has successfully tested the patch
(in some environment).

``Reported-by:`` acknowledges the person who found or reported the bug.

``Suggested-by:`` indicates that the named person suggested the patch idea.


Frequency and volume of patches
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Allow at least 24 hours between posting patch revisions.
This ensures reviewers from different geographical regions have time to provide feedback.
Additionally, do not wait too long (i.e., weeks) between revisions
as this makes it harder for reviewers and maintainers
to recall the context of the previous posting.
If you have not received any feedback within a week,
it is appropriate to send a ping to the mailing list.

Do not post new revisions without addressing all feedback.
Make sure that all outstanding items have been addressed
before posting a new revision for review
(this should involve replying to all the feedback).
Do not post a new version of a patch while there is ongoing discussion
unless a reviewer has specifically requested it.

Do not post your patches to the list in lieu of running tests.
**YOU MUST ENSURE** that your patches are ready by testing them locally
before posting to the mailing list.
Testing locally should involve, at a minimum,
running compilation with debug and release flags, and invoking the unit tests.
Your changes are expected to pass on an x86/x86-64 Linux system.
The infrastructure running the tests is a shared resource among all developers on the project,
and many frequent reposts will result in delays for all developers.
We do our best to include CI and self-test infrastructure
that developers can use individually.

For details on running the unit tests, see :doc:`unit_test`.
It is also recommended to run the :doc:`DTS </tools/dts>` comprehensive tests.
Finally, you can also push to a branch on the GitHub service
to trigger a comprehensive set of compile and unit test runs.

Keep all patch sets to a reasonable length.
Too many or too large patches and series can quickly become difficult to review reasonably.
Appropriately split patches and series into groups of digestible logical changes.


Steps to getting your patch merged
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The more work you put into the previous steps the easier it will be to get a
patch accepted. The general cycle for patch review and acceptance is:

#. Submit the patch.

#. Check the automatic test reports in the coming hours.

#. Wait for review comments. While you are waiting, review some other patches.

#. Fix the review comments and submit a ``v n+1`` patch set::

      git format-patch -3 -v 2

#. Update Patchwork to mark your previous patches as "Superseded".

#. If the relevant maintainer(s) or other developers deem the patch suitable for merging,
   they will ``ack`` the patch with an email that includes something like::

      Acked-by: Alex Smith <alex.smith@example.com>

   **Note**: When acking patches, remove as much of the text of the patch email as possible.
   It is generally best to delete everything after the ``Signed-off-by:`` line.

#. Having the patch ``Reviewed-by:`` and/or ``Tested-by:`` will also help the patch to be accepted.

#. If the patch is deemed unsuitable based on being out of scope
   or conflicting with existing functionality,
   it may receive a ``nack``.
   In this case you will need to make a more convincing technical argument in favor of your patches.

#. In addition, a patch will not be accepted
   if it does not address comments from a previous version with fixes or valid arguments.

#. Maintainers are responsible for ensuring that patches are reviewed
   and for providing an ``ack`` or ``nack`` of those patches as appropriate.

#. Once the relevant maintainer has acked a patch,
   reviewers may still comment on it for a further two weeks.
   After that time, the patch should be merged into the relevant git tree for the next release.
   Additional notes and restrictions:

   * Maintainers should ack patches at least two days before the release merge deadline,
     in order to make that release.
   * For patches acked with less than two weeks to go to the merge deadline, all additional
     comments should be made no later than two days before the merge deadline.
   * After the appropriate time for additional feedback has passed, if the patch has not yet
     been merged to the relevant tree by the committer, it should be treated as though it had,
     in that any additional changes needed to it must be addressed by a follow-on patch, rather
     than rework of the original.
   * Trivial patches may be merged sooner than described above at the tree committer's
     discretion.


Milestones definition
---------------------

Each DPDK release has milestones that help everyone converge to the release date.
The following is a list of these milestones together with
concrete definitions and expectations for a typical release cycle.
An average cycle lasts 3 months and has 4 release candidates in the last month.
Test reports are expected after each release candidate.
The number and expectations of release candidates might vary slightly.
The schedule is updated in the `roadmap <https://core.dpdk.org/roadmap/#dates>`_.

.. note::
   Sooner is always better. Deadlines are not ideal dates.

   Integration is never guaranteed but everyone can help.

Roadmap
~~~~~~~

* Announce new features in libraries, drivers, applications, and examples.
* Publish before the previous release.

Proposal Deadline
~~~~~~~~~~~~~~~~~

* Send an RFC (Request For Comments) or a complete patch of new features.
* Early RFC gives time for design review before complete implementation.
* Include at least the API changes in libraries and applications.
* Library code should be quite complete at the deadline.
* Nice to have: driver implementation, example code, and documentation.

rc1
~~~

* Priority: libraries. No library feature should be accepted after -rc1.
* Implement API changes or additions in libraries.
* The API must include Doxygen documentation
  and be part of the relevant .rst files (library-specific and release notes).
* Use the API in a test application (``/app``).
* At least one PMD should implement the API.
  It may be a draft sent in a separate series.
* Send the above to the mailing list at least 2 weeks before -rc1
  to give time for review and maintainer approval.
* If no review after 10 days, send a reminder.
* Nice to have: example code (``/examples``)

rc2
~~~

* Priority: drivers. No driver feature should be accepted after -rc2.
* A driver change must include documentation
  in the relevant .rst files (driver-specific and release notes).
* Send driver changes to the mailing list before -rc1 is released.

rc3
~~~

* Priority: applications. No application feature should be accepted after -rc3.
* New functionality that does not depend on library updates
  can be integrated as part of -rc3.
* The application change must include documentation in the relevant .rst files
  (application-specific and release notes if significant).
* Library and driver cleanup is allowed.
* Small driver reworks.

rc4
~~~

* Documentation updates.
* Critical bug fixes only.

.. note::
   Integrate bug fixes as early as possible at any stage.
