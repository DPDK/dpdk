.. doc_guidelines:

DPDK Documentation Guidelines
=============================

This document outlines the guidelines for writing the DPDK Guides and API documentation in RST and Doxygen format.

It also explains the structure of the DPDK documentation and shows how to build the Html and PDF versions of the documents.


Structure of the Documentation
------------------------------

The DPDK source code repository contains input files to build the API documentation and User Guides.

The main directories that contain files related to documentation are shown below::

   lib
   |-- librte_acl
   |-- librte_cfgfile
   |-- librte_cmdline
   |-- librte_compat
   |-- librte_eal
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
       |-- xen
       |-- ...


The API documentation is built from `Doxygen <http://www.stack.nl/~dimitri/doxygen/>`_ comments in the header files.
These files are mainly in the ``lib/librte_*`` directories although some of the Poll Mode Drivers in ``drivers/net``
are also documented with Doxygen.

The configuration files that are used to control the Doxygen output are in the ``doc/api`` directory.

The user guides such as *The Programmers Guide* and the *FreeBSD* and *Linux Getting Started* Guides are generated
from RST markup text files using the `Sphinx <http://sphinx-doc.org/index.html>`_ Documentation Generator.

These files are included in the ``doc/guides/`` directory.
The output is controlled by the ``doc/guides/conf.py`` file.


Role of the Documentation
-------------------------

The following items outline the roles of the different parts of the documentation and when they need to be updated or
added to by the developer.

* **Release Notes**

  The Release Notes document which features have been added in the current and previous releases of DPDK and highlight
  any known issues.
  The Releases Notes also contain notifications of features that will change ABI compatibility in the next major release.

  Developers should update the Release Notes to add a short description of new or updated features.
  Developers should also update the Release Notes to add ABI announcements if necessary,
  (see :doc:`/guidelines/versioning` for details).

* **API documentation**

  The API documentation explains how to use the public DPDK functions.
  The `API index page <http://dpdk.org/doc/api/>`_ shows the generated API documentation with related groups of functions.

  The API documentation should be updated via Doxygen comments when new functions are added.

* **Getting Started Guides**

  The Getting Started Guides show how to install and configure DPDK and how to run DPDK based applications on different OSes.

  A Getting Started Guide should be added when DPDK is ported to a new OS.

* **The Programmers Guide**

  The Programmers Guide explains how the API components of DPDK such as the EAL, Memzone, Rings and the Hash Library work.
  It also explains how some higher level functionality such as Packet Distributor, Packet Framework and KNI work.
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

* TexLive (at least TexLive-core, extra Latex support and extra fonts).

* Inkscape.

`Doxygen`_ generates documentation from commented source code.
It can be installed as follows:

.. code-block:: console

   # Ubuntu/Debian.
   sudo apt-get -y install doxygen

   # Red Hat/Fedora.
   sudo yum     -y install doxygen

`Sphinx`_ is a Python documentation tool for converting RST files to Html or to PDF (via LaTeX).
It can be installed as follows:

.. code-block:: console

   # Ubuntu/Debian.
   sudo apt-get -y install python-sphinx

   # Red Hat/Fedora.
   sudo yum     -y install python-sphinx

   # Or, on any system with Python installed.
   sudo easy_install -U sphinx

For further information on getting started with Sphinx see the `Sphinx Tutorial <http://sphinx-doc.org/tutorial.html>`_.

.. Note::

   To get full support for Figure and Table numbering it is best to install Sphinx 1.3.1 or later.


`Inkscape`_ is a vector based graphics program which is used to create SVG images and also to convert SVG images to PDF images.
It can be installed as follows:

.. code-block:: console

   # Ubuntu/Debian.
   sudo apt-get -y install inkscape

   # Red Hat/Fedora.
   sudo yum     -y install inkscape

`TexLive <http://www.tug.org/texlive/>`_ is an installation package for Tex/LaTeX.
It is used to generate the PDF versions of the documentation.
The main required packages can be installed as follows:

.. code-block:: console

   # Ubuntu/Debian.
   sudo apt-get -y install texlive-latex-extra texlive-fonts-extra \
                           texlive-fonts-recommended


   # Red Hat/Fedora, selective install.
   sudo yum     -y install texlive-collection-latexextra \
                           texlive-collection-fontsextra


Build commands
~~~~~~~~~~~~~~

The documentation is built using the standard DPDK build system.
Some examples are shown below:

* Generate all the documentation targets::

     make doc

* Generate the Doxygen API documentation in Html::

     make doc-api-html

* Generate the guides documentation in Html::

     make doc-guides-html

* Generate the guides documentation in Pdf::

     make doc-guides-pdf

The output of these commands is generated in the ``build`` directory::

   build/doc
         |-- html
         |   |-- api
         |   +-- guides
         |
         +-- pdf
             +-- guides


.. Note::

   Make sure to fix any Sphinx or Doxygen warnings when adding or updating documentation.

The documentation output files can be removed as follows::

   make doc-clean


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

The RST (reStructuredText) format is a plain text markup format that can be converted to Html, PDF or other formats.
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

* The recommended style for the DPDK documentation is to put sentences on separate lines.
  This allows for easier reviewing of patches.
  Multiple sentences which are not separated by a blank line are joined automatically into paragraphs, for example::

     Here is an example sentence.
     Long sentences over the limit shown below can be wrapped onto
     a new line.
     These three sentences will be joined into the same paragraph.

     This is a new paragraph, since it is separated from the
     previous paragraph by a blank line.

  This would be rendered as follows:

     *Here is an example sentence.
     Long sentences over the limit shown below can be wrapped onto
     a new line.
     These three sentences will be joined into the same paragraph.*

     *This is a new paragraph, since it is separated from the
     previous paragraph by a blank line.*


* Long sentences should be wrapped at 120 characters +/- 10 characters. They should be wrapped at words.

* Lines in literal blocks must by less than 80 characters since they aren't wrapped by the document formatters
  and can exceed the page width in PDF documents.


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

* Section headers should use the use the following underline formats::

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

     #. Item two is a long line that is wrapped and then indented
        to match the start of the e first line.

     #. Item two is a long line that is wrapped and then indented to match
        the start of the previous line.

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


* The default encoding for a literal block using the simplified ``::``
  directive is ``none``.

* Lines in literal blocks must be less than 80 characters since they can exceed the page width when converted to PDF documentation.
  For long literal lines that exceed that limit try to wrap the text at sensible locations.
  For example a long command line could be documented like this and still work if copied directly from the docs::

     build/app/testpmd -c7 -n3 --vdev=eth_pcap0,iface=eth0     \
                               --vdev=eth_pcap1,iface=eth1     \
                               -- -i --nb-cores=2 --nb-ports=2 \
                                  --total-num-mbufs=2048

* Long lines that cannot be wrapped, such as application output, should be truncated to be less than 80 characters.


Images
~~~~~~

* All images should be in SVG scalar graphics format.
  They should be true SVG XML files and should not include binary formats embedded in a SVG wrapper.

* The DPDK documentation contains some legacy images in PNG format.
  These will be converted to SVG in time.

* `Inkscape <inkscape.org>`_ is the recommended graphics editor for creating the images.
  Use some of the older images in ``doc/guides/prog_guide/img/`` as a template, for example ``mbuf1.svg``
  or ``ring-enqueue.svg``.

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
  They are hard to format and to edit, they are often rendered incorrectly in PDF format, and the same information
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

* Tables should not include merged cells since they are not supported by the PDF renderer.


.. _links:

Hyperlinks
~~~~~~~~~~

* Links to external websites can be plain URLs.
  The following is rendered as http://dpdk.org::

     http://dpdk.org

* They can contain alternative text.
  The following is rendered as `Check out DPDK <http://dpdk.org>`_::

     `Check out DPDK <http://dpdk.org>`_

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

