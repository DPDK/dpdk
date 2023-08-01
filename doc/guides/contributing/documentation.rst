..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

.. _doc_guidelines:

DPDK Documentation Guidelines
=============================

This document outlines the guidelines for writing the DPDK Guides and API documentation in RST and Doxygen format.

It also explains the structure of the DPDK documentation and how to build it.


Structure of the Documentation
------------------------------

The DPDK source code repository contains input files to build the API documentation and User Guides.

The main directories that contain files related to documentation are shown below::

   lib
   |-- acl
   |-- cfgfile
   |-- cmdline
   |-- eal
   |   |-- ...
   ...
   doc
   |-- api
   +-- guides
       |-- freebsd_gsg
       |-- linux_gsg
       |-- prog_guide
       |-- sample_app_ug
       |-- guidelines
       |-- testpmd_app_ug
       |-- rel_notes
       |-- nics
       |-- ...


The API documentation is built from `Doxygen <http://www.doxygen.nl>`_ comments in the header files.
These files are mainly in the ``lib/*`` directories although some of the Poll Mode Drivers in ``drivers/net``
are also documented with Doxygen.

The configuration files that are used to control the Doxygen output are in the ``doc/api`` directory.

The user guides such as *The Programmers Guide* and the *FreeBSD* and *Linux Getting Started* Guides are generated
from RST markup text files using the `Sphinx <http://sphinx-doc.org>`_ Documentation Generator.

These files are included in the ``doc/guides/`` directory.
The output is controlled by the ``doc/guides/conf.py`` file.


Role of the Documentation
-------------------------

The following items outline the roles of the different parts of the documentation and when they need to be updated or
added to by the developer.

* **Release Notes**

  The Release Notes document which features have been added in the current and previous releases of DPDK and highlight
  any known issues.
  The Releases Notes also contain notifications of features that will change ABI compatibility in the next release.

  Developers should include updates to the Release Notes with patch sets that relate to any of the following sections:

  * New Features
  * Resolved Issues (see below)
  * Known Issues
  * API Changes
  * ABI Changes
  * Shared Library Versions

  Resolved Issues should only include issues from previous releases that have been resolved in the current release.
  Issues that are introduced and then fixed within a release cycle do not have to be included here.

  Refer to the Release Notes from the previous DPDK release for the correct format of each section.


* **API documentation**

  The API documentation explains how to use the public DPDK functions.
  The `API index page <https://doc.dpdk.org/api/>`_ shows the generated API documentation with related groups of functions.

  The API documentation should be updated via Doxygen comments when new functions are added.

* **Getting Started Guides**

  The Getting Started Guides show how to install and configure DPDK and how to run DPDK based applications on different OSes.

  A Getting Started Guide should be added when DPDK is ported to a new OS.

* **The Programmers Guide**

  The Programmers Guide explains how the API components of DPDK such as the EAL, Memzone, Rings and the Hash Library work.
  It also describes some of the higher level functionality such as Packet Distributor and Packet Framework.
  It also shows the build system and explains how to add applications.

  The Programmers Guide should be expanded when new functionality is added to DPDK.

* **App Guides**

  The app guides document the DPDK applications in the ``app`` directory such as ``testpmd``.

  The app guides should be updated if functionality is changed or added.

* **Sample App Guides**

  The sample app guides document the DPDK example applications in the examples directory.
  Generally they demonstrate a major feature such as L2 or L3 Forwarding, Multi Process or Power Management.
  They explain the purpose of the sample application, how to run it and step through some of the code to explain the
  major functionality.

  A new sample application should be accompanied by a new sample app guide.
  The guide for the Skeleton Forwarding app is a good starting reference.

* **Network Interface Controller Drivers**

  The NIC Drivers document explains the features of the individual Poll Mode Drivers, such as software requirements,
  configuration and initialization.

  New documentation should be added for new Poll Mode Drivers.

* **Guidelines**

  The guideline documents record community process, expectations and design directions.

  They can be extended, amended or discussed by submitting a patch and getting community approval.


Building the Documentation
--------------------------

Dependencies
~~~~~~~~~~~~

The following dependencies must be installed to build the documentation:

* Doxygen.
* Sphinx (also called python-sphinx).

`Doxygen`_ generates documentation from commented source code.
It can be installed as follows:

.. code-block:: console

   # Ubuntu/Debian.
   sudo apt-get -y install doxygen

   # Red Hat/Fedora.
   sudo dnf     -y install doxygen

`Sphinx`_ is a Python documentation tool for converting RST files to HTML.
For full support with figure and table captioning the latest version of Sphinx can be installed as follows:

.. code-block:: console

   # Ubuntu/Debian.
   sudo apt-get -y install python3-sphinx python3-sphinx-rtd-theme

   # Red Hat/Fedora.
   sudo dnf     -y install python3-sphinx python3-sphinx_rtd_theme

For further information on getting started with Sphinx see the
`Sphinx Getting Started <http://www.sphinx-doc.org/en/master/usage/quickstart.html>`_.

.. Note::

   To get full support for Figure and Table numbering it is best to install Sphinx 1.3.1 or later.


Build commands
~~~~~~~~~~~~~~

The documentation is built using the standard DPDK build system.

To build the documentation::

   ninja -C build doc

See :doc:`../linux_gsg/build_dpdk` for more detail on compiling DPDK with meson.

The output is generated in the directories ``build/doc/html/{api,guides}``.

.. Note::

   Make sure to fix any Sphinx or Doxygen warnings when adding or updating documentation.


Document Guidelines
-------------------

Here are some guidelines in relation to the style of the documentation:

* Document the obvious as well as the obscure since it won't always be obvious to the reader.
  For example an instruction like "Set up 64 2MB Hugepages" is better when followed by a sample commandline or a link to
  the appropriate section of the documentation.

* Use American English spellings throughout.
  This can be checked using the ``aspell`` utility::

       aspell --lang=en_US --check doc/guides/sample_app_ug/mydoc.rst


RST Guidelines
--------------

The RST (reStructuredText) format is a plain text markup format
that can be converted to HTML or other formats.
It is most closely associated with Python but it can be used to document any language.
It is used in DPDK to document everything apart from the API.

The Sphinx documentation contains a very useful `RST Primer <http://sphinx-doc.org/rest.html#rst-primer>`_ which is a
good place to learn the minimal set of syntax required to format a document.

The official `reStructuredText <http://docutils.sourceforge.net/rst.html>`_ website contains the specification for the
RST format and also examples of how to use it.
However, for most developers the RST Primer is a better resource.

The most common guidelines for writing RST text are detailed in the
`Documenting Python <https://docs.python.org/devguide/documenting.html>`_ guidelines.
The additional guidelines below reiterate or expand upon those guidelines.


Line Length
~~~~~~~~~~~

* Lines in sentences should be less than 80 characters and wrapped at
  words. Multiple sentences which are not separated by a blank line are joined
  automatically into paragraphs.

* Lines in literal blocks should be less than 80 characters
  since they are not wrapped by the document formatters.

  Long literal command lines can be shown wrapped with backslashes. For
  example::

     dpdk-testpmd -l 2-3 -n 4 \
             --vdev=virtio_user0,path=/dev/vhost-net,queues=2,queue_size=1024 \
             -- -i --tx-offloads=0x0000002c --enable-lro --txq=2 --rxq=2 \
             --txd=1024 --rxd=1024


Whitespace
~~~~~~~~~~

* Standard RST indentation is 3 spaces.
  Code can be indented 4 spaces, especially if it is copied from source files.

* No tabs.
  Convert tabs in embedded code to 4 or 8 spaces.

* No trailing whitespace.

* Add 2 blank lines before each section header.

* Add 1 blank line after each section header.

* Add 1 blank line between each line of a list.


Section Headers
~~~~~~~~~~~~~~~

* Section headers should use the following underline formats::

   Level 1 Heading
   ===============


   Level 2 Heading
   ---------------


   Level 3 Heading
   ~~~~~~~~~~~~~~~


   Level 4 Heading
   ^^^^^^^^^^^^^^^


* Level 4 headings should be used sparingly.

* The underlines should match the length of the text.

* In general, the heading should be less than 80 characters, for conciseness.

* As noted above:

   * Add 2 blank lines before each section header.

   * Add 1 blank line after each section header.


Lists
~~~~~

* Bullet lists should be formatted with a leading ``*`` as follows::

     * Item one.

     * Item two is a long line that is wrapped and then indented to match
       the start of the previous line.

     * One space character between the bullet and the text is preferred.

* Numbered lists can be formatted with a leading number but the preference is to use ``#.`` which will give automatic numbering.
  This is more convenient when adding or removing items::

     #. Item one.

     #. Item two is a long line that is wrapped and then indented to match
        the start of the previous line.

     #. Item three.

* Definition lists can be written with or without a bullet::

     * Item one.

       Some text about item one.

     * Item two.

       Some text about item two.

* All lists, and sub-lists, must be separated from the preceding text by a blank line.
  This is a syntax requirement.

* All list items should be separated by a blank line for readability.


Code and Literal block sections
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Inline text that is required to be rendered with a fixed width font should be enclosed in backquotes like this:
  \`\`text\`\`, so that it appears like this: ``text``.

* Fixed width, literal blocks of texts should be indented at least 3 spaces and prefixed with ``::`` like this::

     Here is some fixed width text::

        0x0001 0x0001 0x00FF 0x00FF

* It is also possible to specify an encoding for a literal block using the ``.. code-block::`` directive so that syntax
  highlighting can be applied.
  Examples of supported highlighting are::

     .. code-block:: console
     .. code-block:: c
     .. code-block:: python
     .. code-block:: diff
     .. code-block:: none

  That can be applied as follows::

      .. code-block:: c

         #include<stdio.h>

         int main() {

            printf("Hello World\n");

            return 0;
         }

  Which would be rendered as:

  .. code-block:: c

      #include<stdio.h>

      int main() {

         printf("Hello World\n");

         return 0;
      }

* Code snippets can also be included directly from the code using the ``literalinclude`` block.
  Using this block instead of a code block will ensure that the code snippets
  shown in the documentation are always up to date with the code.

  The following will include a snippet from the skeleton sample app::

      .. literalinclude:: ../../../examples/skeleton/basicfwd.c
         :language: c
         :start-after: Display the port MAC address.
         :end-before: Enable RX in promiscuous mode for the Ethernet device.
         :dedent: 1

  This would be rendered as:

  .. literalinclude:: ../../../examples/skeleton/basicfwd.c
     :language: c
     :start-after: Display the port MAC address.
     :end-before: Enable RX in promiscuous mode for the Ethernet device.
     :dedent: 1

  Specifying ``:language:`` will enable syntax highlighting for the specified language.
  ``:dedent:`` is used in this example to remove 1 leading tab from each line of the snippet.

* ``start-after`` and ``end-before`` can use any text within a given file,
  however it may be difficult to find unique text within your code to mark the
  start and end of your snippets. In these cases, it is recommended to include
  explicit tags in your code to denote these locations for documentation purposes.
  The accepted format for these comments is:

     * Before the code snippet, create a new comment which is a sentence explaining
       what the code snippet contains. The comment is terminated with a scissors ``8<``.
     * After the code snippet, create another new comment which starts with a
       scissors ``>8``, then ``End of`` and the first comment repeated.
     * The scissors should be orientated as shown to make it clear what code is being snipped.

  This can be done as follows:

  .. code-block:: c

    /* Example feature being documented. 8< */
    foo(bar);
    /* >8 End of example feature being documented. */

  ``foo(bar);`` could then be included in the docs using::

      .. literalinclude:: ../../../examples/sample_app/main.c
         :language: c
         :start-after: Example feature being documented. 8<
         :end-before: >8 End of example feature being documented.

  If a multiline comment is needed before the snippet,
  then the last line of the multiline comment should be in the same format as
  the first comment shown in the example.

* More information about the ``literalinclude`` block can be found within the
  `Sphinx Documentation <https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html?highlight=literalinclude#directive-literalinclude>`_.

* The default encoding for a literal block using the simplified ``::``
  directive is ``none``.

* Lines in literal blocks should be less than 80 characters.
  For long literal lines, try to wrap the text at sensible locations.
  For example a long command line could be documented like this and still work if copied directly from the docs::

     ./<build_dir>/app/dpdk-testpmd -l 0-2 -n3 --vdev=net_pcap0,iface=eth0    \
                               --vdev=net_pcap1,iface=eth1     \
                               -- -i --nb-cores=2 --nb-ports=2 \
                                  --total-num-mbufs=2048

* Long lines that cannot be wrapped, such as application output, should be truncated to be less than 80 characters.


Images
~~~~~~

* All images should be in SVG scalar graphics format.
  They should be true SVG XML files and should not include binary formats embedded in a SVG wrapper.

* The DPDK documentation contains some legacy images in PNG format.
  These will be converted to SVG in time.

* `Inkscape <http://inkscape.org>`_ is the recommended graphics editor for creating the images.
  Use some of the older images in ``doc/guides/prog_guide/img/`` as a template, for example ``mbuf1.svg``
  or ``ring-enqueue1.svg``.

* The SVG images should include a copyright notice, as an XML comment.

* Images in the documentation should be formatted as follows:

   * The image should be preceded by a label in the format ``.. _figure_XXXX:`` with a leading underscore and
     where ``XXXX`` is a unique descriptive name.

   * Images should be included using the ``.. figure::`` directive and the file type should be set to ``*`` (not ``.svg``).
     This allows the format of the image to be changed if required, without updating the documentation.

   * Images must have a caption as part of the ``.. figure::`` directive.

* Here is an example of the previous three guidelines::

     .. _figure_mempool:

     .. figure:: img/mempool.*

        A mempool in memory with its associated ring.

.. _mock_label:

* Images can then be linked to using the ``:numref:`` directive::

     The mempool layout is shown in :numref:`figure_mempool`.

  This would be rendered as: *The mempool layout is shown in* :ref:`Fig 6.3 <mock_label>`.

  **Note**: The ``:numref:`` directive requires Sphinx 1.3.1 or later.
  With earlier versions it will still be rendered as a link but won't have an automatically generated number.

* The caption of the image can be generated, with a link, using the ``:ref:`` directive::

     :ref:`figure_mempool`

  This would be rendered as: *A mempool in memory with its associated ring.*

Tables
~~~~~~

* RST tables should be used sparingly.
  They are hard to format and to edit, and the same information
  can usually be shown just as clearly with a definition or bullet list.

* Tables in the documentation should be formatted as follows:

   * The table should be preceded by a label in the format ``.. _table_XXXX:`` with a leading underscore and where
     ``XXXX`` is a unique descriptive name.

   * Tables should be included using the ``.. table::`` directive and must have a caption.

* Here is an example of the previous two guidelines::

     .. _table_qos_pipes:

     .. table:: Sample configuration for QOS pipes.

        +----------+----------+----------+
        | Header 1 | Header 2 | Header 3 |
        |          |          |          |
        +==========+==========+==========+
        | Text     | Text     | Text     |
        +----------+----------+----------+
        | ...      | ...      | ...      |
        +----------+----------+----------+

* Tables can be linked to using the ``:numref:`` and ``:ref:`` directives, as shown in the previous section for images.
  For example::

     The QOS configuration is shown in :numref:`table_qos_pipes`.


.. _links:

Hyperlinks
~~~~~~~~~~

* Links to external websites can be plain URLs.
  The following is rendered as https://dpdk.org::

     https://dpdk.org

* They can contain alternative text.
  The following is rendered as `Check out DPDK <https://dpdk.org>`_::

     `Check out DPDK <https://dpdk.org>`_

* An internal link can be generated by placing labels in the document with the format ``.. _label_name``.

* The following links to the top of this section: :ref:`links`::

     .. _links:

     Hyperlinks
     ~~~~~~~~~~

     * The following links to the top of this section: :ref:`links`:

.. Note::

   The label must have a leading underscore but the reference to it must omit it.
   This is a frequent cause of errors and warnings.

* The use of a label is preferred since it works across files and will still work if the header text changes.


.. _doxygen_guidelines:

Doxygen Guidelines
------------------

The DPDK API is documented using Doxygen comment annotations in the header files.
Doxygen is a very powerful tool, it is extremely configurable and with a little effort can be used to create expressive documents.
See the `Doxygen website <http://www.doxygen.nl>`_ for full details on how to use it.

The following are some guidelines for use of Doxygen in the DPDK API documentation:

* New libraries that are documented with Doxygen should be added to the Doxygen configuration file: ``doc/api/doxy-api.conf``.
  It is only required to add the directory that contains the files.
  It isn't necessary to explicitly name each file since the configuration matches all ``rte_*.h`` files in the directory.

* Use proper capitalization and punctuation in the Doxygen comments since they will become sentences in the documentation.
  This in particular applies to single line comments, which is the case the is most often forgotten.

* Use ``@`` style Doxygen commands instead of ``\`` style commands.

* Add a general description of each library at the head of the main header files:

  .. code-block:: c

      /**
       * @file
       * RTE Mempool.
       *
       * A memory pool is an allocator of fixed-size object. It is
       * identified by its name, and uses a ring to store free objects.
       * ...
       */

* Document the purpose of a function, the parameters used and the return
  value:

  .. code-block:: c

     /**
      * Try to take the lock.
      *
      * @param sl
      *   A pointer to the spinlock.
      * @return
      *   1 if the lock is successfully taken; 0 otherwise.
      */
     int rte_spinlock_trylock(rte_spinlock_t *sl);

* Doxygen supports Markdown style syntax such as bold, italics, fixed width text and lists.
  For example the second line in the ``devargs`` parameter in the previous example will be rendered as:

     The strings should be a pci address like ``0000:01:00.0`` or **virtual** device name like ``net_pcap0``.

* Use ``-`` instead of ``*`` for lists within the Doxygen comment since the latter can get confused with the comment delimiter.

* Add an empty line between the function description, the ``@params`` and ``@return`` for readability.

* Place the ``@params`` description on separate line and indent it by 2 spaces.
  (It would be better to use no indentation since this is more common and also because checkpatch complains about leading
  whitespace in comments.
  However this is the convention used in the existing DPDK code.)

* Documented functions can be linked to simply by adding ``()`` to the function name:

  .. code-block:: c

      /**
       * The functions exported by the application Ethernet API to setup
       * a device designated by its port identifier must be invoked in
       * the following order:
       *     - rte_eth_dev_configure()
       *     - rte_eth_tx_queue_setup()
       *     - rte_eth_rx_queue_setup()
       *     - rte_eth_dev_start()
       */

  In the API documentation the functions will be rendered as links, see the
  `online section of the rte_ethdev.h docs <https://doc.dpdk.org/api/rte__ethdev_8h.html>`_ that contains the above text.

* The ``@see`` keyword can be used to create a *see also* link to another file or library.
  This directive should be placed on one line at the bottom of the documentation section.

  .. code-block:: c

     /**
      * ...
      *
      * Some text that references mempools.
      *
      * @see eal_memzone.c
      */

* Doxygen supports two types of comments for documenting variables, constants and members: prefix and postfix:

  .. code-block:: c

     /** This is a prefix comment. */
     #define RTE_FOO_ERROR  0x023.

     #define RTE_BAR_ERROR  0x024. /**< This is a postfix comment. */

* Postfix comments are preferred for struct members and constants if they can be documented in the same way:

  .. code-block:: c

     struct rte_eth_stats {
         uint64_t ipackets; /**< Total number of received packets. */
         uint64_t opackets; /**< Total number of transmitted packets.*/
         uint64_t ibytes;   /**< Total number of received bytes. */
         uint64_t obytes;   /**< Total number of transmitted bytes. */
         uint64_t imissed;  /**< Total of RX missed packets. */
         uint64_t ibadcrc;  /**< Total of RX packets with CRC error. */
         uint64_t ibadlen;  /**< Total of RX packets with bad length. */
     }

  Note: postfix comments should be aligned with spaces not tabs in accordance
  with the :ref:`coding_style`.

* If a single comment type can't be used, due to line length limitations then
  prefix comments should be preferred.
  For example this section of the code contains prefix comments, postfix comments on the same line and postfix
  comments on a separate line:

  .. code-block:: c

     /** Number of elements in the elt_pa array. */
     uint32_t    pg_num __rte_cache_aligned;
     uint32_t    pg_shift;     /**< LOG2 of the physical pages. */
     uintptr_t   pg_mask;      /**< Physical page mask value. */
     uintptr_t   elt_va_start;
     /**< Virtual address of the first mempool object. */
     uintptr_t   elt_va_end;
     /**< Virtual address of the <size + 1> mempool object. */
     phys_addr_t elt_pa[1];
     /**< Array of physical page addresses for the mempool buffer. */

  This doesn't have an effect on the rendered documentation but it is confusing for the developer reading the code.
  It this case it would be clearer to use prefix comments throughout:

  .. code-block:: c

     /** Number of elements in the elt_pa array. */
     uint32_t    pg_num __rte_cache_aligned;
     /** LOG2 of the physical pages. */
     uint32_t    pg_shift;
     /** Physical page mask value. */
     uintptr_t   pg_mask;
     /** Virtual address of the first mempool object. */
     uintptr_t   elt_va_start;
     /** Virtual address of the <size + 1> mempool object. */
     uintptr_t   elt_va_end;
     /** Array of physical page addresses for the mempool buffer. */
     phys_addr_t elt_pa[1];

* Read the rendered section of the documentation that you have added for correctness, clarity and consistency
  with the surrounding text.
