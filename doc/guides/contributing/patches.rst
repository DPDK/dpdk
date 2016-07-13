.. submitting_patches:

Contributing Code to DPDK
=========================

This document outlines the guidelines for submitting code to DPDK.

The DPDK development process is modelled (loosely) on the Linux Kernel development model so it is worth reading the
Linux kernel guide on submitting patches:
`How to Get Your Change Into the Linux Kernel <http://www.kernel.org/doc/Documentation/SubmittingPatches>`_.
The rationale for many of the DPDK guidelines is explained in greater detail in the kernel guidelines.


The DPDK Development Process
-----------------------------

The DPDK development process has the following features:

* The code is hosted in a public git repository.
* There is a mailing list where developers submit patches.
* There are maintainers for hierarchical components.
* Patches are reviewed publicly on the mailing list.
* Successfully reviewed patches are merged to the master branch of the repository.

The mailing list for DPDK development is `dev@dpdk.org <http://dpdk.org/ml/archives/dev/>`_.
Contributors will need to `register for the mailing list <http://dpdk.org/ml/listinfo/dev>`_ in order to submit patches.
It is also worth registering for the DPDK `Patchwork <http://dpdk.org/dev/patchwxispork/project/dpdk/list/>`_

The development process requires some familiarity with the ``git`` version control system.
Refer to the `Pro Git Book <http://www.git-scm.com/book/>`_ for further information.


Getting the Source Code
-----------------------

The source code can be cloned using either of the following::

    git clone git://dpdk.org/dpdk

    git clone http://dpdk.org/git/dpdk


Make your Changes
-----------------

Make your planned changes in the cloned ``dpdk`` repo. Here are some guidelines and requirements:

* Follow the :ref:`coding_style` guidelines.

* If you add new files or directories you should add your name to the ``MAINTAINERS`` file.

* New external functions should be added to the local ``version.map`` file.
  See the :doc:`Guidelines for ABI policy and versioning </contributing/versioning>`.
  New external functions should also be added in alphabetical order.

* Important changes will require an addition to the release notes in ``doc/guides/rel_notes/``.
  See the :ref:`Release Notes section of the Documentation Guidelines <doc_guidelines>` for details.

* Test the compilation works with different targets, compilers and options, see :ref:`contrib_check_compilation`.

* Don't break compilation between commits with forward dependencies in a patchset.
  Each commit should compile on its own to allow for ``git bisect`` and continuous integration testing.

* Add tests to the the ``app/test`` unit test framework where possible.

* Add documentation, if relevant, in the form of Doxygen comments or a User Guide in RST format.
  See the :ref:`Documentation Guidelines <doc_guidelines>`.

Once the changes have been made you should commit them to your local repo.

For small changes, that do not require specific explanations, it is better to keep things together in the
same patch.
Larger changes that require different explanations should be separated into logical patches in a patchset.
A good way of thinking about whether a patch should be split is to consider whether the change could be
applied without dependencies as a backport.

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
  `Developer's Certificate of Origin <http://www.kernel.org/doc/Documentation/SubmittingPatches>`_
  section of the Linux kernel guidelines.

  .. Note::

     All developers must ensure that they have read and understood the
     Developer's Certificate of Origin section of the documentation prior
     to applying the signoff and submitting a patch.

* The signoff must be a real name and not an alias or nickname.
  More than one signoff is allowed.

* The text of the commit message should be wrapped at 72 characters.

* When fixing a regression, it is a good idea to reference the id of the commit which introduced the bug.
  You can generate the required text using the following git alias::

     git config alias.fixline "log -1 --abbrev=12 --format='Fixes: %h (\"%s\")'"

  The ``Fixes:`` line can then be added to the commit message::

     doc: fix vhost sample parameter

     Update the docs to reflect removed dev-index.

     Fixes: 17b8320a3e11 ("vhost: remove index parameter")

     Signed-off-by: Alex Smith <alex.smith@example.com>

* When fixing an error or warning it is useful to add the error message and instructions on how to reproduce it.

* Use correct capitalization, punctuation and spelling.

In addition to the ``Signed-off-by:`` name the commit messages can also have one or more of the following:

* ``Reported-by:`` The reporter of the issue.
* ``Tested-by:`` The tester of the change.
* ``Reviewed-by:`` The reviewer of the change.
* ``Suggested-by:`` The person who suggested the change.
* ``Acked-by:`` When a previous version of the patch was acked and the ack is still relevant.


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

Patches should be checked for formatting and syntax issues using the ``checkpatches.sh`` script in the ``scripts``
directory of the DPDK repo.
This uses the Linux kernel development tool ``checkpatch.pl`` which  can be obtained by cloning, and periodically,
updating the Linux kernel sources.

The path to the original Linux script must be set in the environment variable ``DPDK_CHECKPATCH_PATH``.
This, and any other configuration variables required by the development tools, are loaded from the following
files, in order of preference::

   .develconfig
   ~/.config/dpdk/devel.config
   /etc/dpdk/devel.config.

Once the environment variable the script can be run as follows::

   scripts/checkpatches.sh ~/patch/

The script usage is::

   checkpatches.sh [-h] [-q] [-v] [patch1 [patch2] ...]]"

Where:

* ``-h``: help, usage.
* ``-q``: quiet. Don't output anything for files without issues.
* ``-v``: verbose.
* ``patchX``: path to one or more patches.

Then the git logs should be checked using the ``check-git-log.sh`` script.

The script usage is::

   check-git-log.sh [range]

Where the range is a ``git log`` option.


.. _contrib_check_compilation:

Checking Compilation
--------------------

Compilation of patches and changes should be tested using the the ``test-build.sh`` script in the ``scripts``
directory of the DPDK repo::

  scripts/test-build.sh x86_64-native-linuxapp-gcc+next+shared

The script usage is::

   test-build.sh [-h] [-jX] [-s] [config1 [config2] ...]]

Where:

* ``-h``: help, usage.
* ``-jX``: use X parallel jobs in "make".
* ``-s``: short test with only first config and without examples/doc.
* ``config``: default config name plus config switches delimited with a ``+`` sign.

Examples of configs are::

   x86_64-native-linuxapp-gcc
   x86_64-native-linuxapp-gcc+next+shared
   x86_64-native-linuxapp-clang+shared

The builds can be modifies via the following environmental variables:

* ``DPDK_BUILD_TEST_CONFIGS`` (target1+option1+option2 target2)
* ``DPDK_DEP_CFLAGS``
* ``DPDK_DEP_LDFLAGS``
* ``DPDK_DEP_MOFED`` (y/[n])
* ``DPDK_DEP_PCAP`` (y/[n])
* ``DPDK_NOTIFY`` (notify-send)

These can be set from the command line or in the config files shown above in the :ref:`contrib_checkpatch`.

The recommended configurations and options to test compilation prior to submitting patches are::

   x86_64-native-linuxapp-gcc+shared+next
   x86_64-native-linuxapp-clang+shared
   i686-native-linuxapp-gcc

   export DPDK_DEP_ZLIB=y
   export DPDK_DEP_PCAP=y
   export DPDK_DEP_SSL=y


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

New additions can be sent without a maintainer::

   git send-email --to dev@dpdk.org 000*.patch

You can test the emails by sending it to yourself or with the ``--dry-run`` option.

If the patch is in relation to a previous email thread you can add it to the same thread using the Message ID::

   git send-email --to dev@dpdk.org --in-reply-to <1234-foo@bar.com> 000*.patch

The Message ID can be found in the raw text of emails or at the top of each Patchwork patch,
`for example <http://dpdk.org/dev/patchwork/patch/7646/>`_.
Shallow threading (``--thread --no-chain-reply-to``) is preferred for a patch series.

Once submitted your patches will appear on the mailing list and in Patchwork.

Experienced committers may send patches directly with ``git send-email`` without the ``git format-patch`` step.
The options ``--annotate`` and ``confirm = always`` are recommended for checking patches before sending.


The Review Process
------------------

The more work you put into the previous steps the easier it will be to get a patch accepted.

The general cycle for patch review and acceptance is:

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

#. Acked patches will be merged in the current or next merge window.
