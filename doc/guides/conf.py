#   BSD LICENSE
#   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#   * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
#   * Neither the name of Intel Corporation nor the names of its
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import subprocess
from docutils import nodes
from distutils.version import LooseVersion
from sphinx import __version__ as sphinx_version
from sphinx.highlighting import PygmentsBridge
from pygments.formatters.latex import LatexFormatter

project = 'Data Plane Development Kit'

if LooseVersion(sphinx_version) >= LooseVersion('1.3.1'):
    html_theme = "sphinx_rtd_theme"
html_logo = '../logo/DPDK_logo_vertical_rev_small.png'
latex_logo = '../logo/DPDK_logo_horizontal_tag.png'
html_add_permalinks = ""
html_show_copyright = False
highlight_language = 'none'

version = subprocess.check_output(['make', '-sRrC', '../../', 'showversion']).decode('utf-8').rstrip()
release = version

master_doc = 'index'

# Figures, tables and code-blocks automatically numbered if they have caption
numfig = True

latex_documents = [
    ('index',
     'doc.tex',
     '',
     '',
     'manual')
]

# Latex directives to be included directly in the latex/pdf docs.
latex_preamble = r"""
\usepackage[utf8]{inputenc}
\usepackage[T1]{fontenc}
\usepackage{helvet}
\renewcommand{\familydefault}{\sfdefault}
\RecustomVerbatimEnvironment{Verbatim}{Verbatim}{xleftmargin=5mm}
"""

# Configuration for the latex/pdf docs.
latex_elements = {
    'papersize': 'a4paper',
    'pointsize': '11pt',
    # remove blank pages
    'classoptions': ',openany,oneside',
    'babel': '\\usepackage[english]{babel}',
    # customize Latex formatting
    'preamble': latex_preamble
}

# Override the default Latex formatter in order to modify the
# code/verbatim blocks.
class CustomLatexFormatter(LatexFormatter):
    def __init__(self, **options):
        super(CustomLatexFormatter, self).__init__(**options)
        # Use the second smallest font size for code/verbatim blocks.
        self.verboptions = r'formatcom=\footnotesize'

# Replace the default latex formatter.
PygmentsBridge.latex_formatter = CustomLatexFormatter

######## :numref: fallback ########
# The following hook functions add some simple handling for the :numref:
# directive for Sphinx versions prior to 1.3.1. The functions replace the
# :numref: reference with a link to the target (for all Sphinx doc types).
# It doesn't try to label figures/tables.

def numref_role(reftype, rawtext, text, lineno, inliner):
    """
    Add a Sphinx role to handle numref references. Note, we can't convert
    the link here because the doctree isn't build and the target information
    isn't available.
    """
    # Add an identifier to distinguish numref from other references.
    newnode = nodes.reference('',
                              '',
                              refuri='_local_numref_#%s' % text,
                              internal=True)
    return [newnode], []

def process_numref(app, doctree, from_docname):
    """
    Process the numref nodes once the doctree has been built and prior to
    writing the files. The processing involves replacing the numref with a
    link plus text to indicate if it is a Figure or Table link.
    """

    # Iterate over the reference nodes in the doctree.
    for node in doctree.traverse(nodes.reference):
        target = node.get('refuri', '')

        # Look for numref nodes.
        if target.startswith('_local_numref_#'):
            target = target.replace('_local_numref_#', '')

            # Get the target label and link information from the Sphinx env.
            data = app.builder.env.domains['std'].data
            docname, label, _ = data['labels'].get(target, ('', '', ''))
            relative_url = app.builder.get_relative_uri(from_docname, docname)

            # Add a text label to the link.
            if target.startswith('figure'):
                caption = 'Figure'
            elif target.startswith('table'):
                caption = 'Table'
            else:
                caption = 'Link'

            # New reference node with the updated link information.
            newnode = nodes.reference('',
                                      caption,
                                      refuri='%s#%s' % (relative_url, label),
                                      internal=True)
            node.replace_self(newnode)

def setup(app):
    if LooseVersion(sphinx_version) < LooseVersion('1.3.1'):
        print('Upgrade sphinx to version >= 1.3.1 for '
              'improved Figure/Table number handling.')
        # Add a role to handle :numref: references.
        app.add_role('numref', numref_role)
        # Process the numref references once the doctree has been created.
        app.connect('doctree-resolved', process_numref)
