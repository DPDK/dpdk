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

from __future__ import print_function
import subprocess
from docutils import nodes
from distutils.version import LooseVersion
from sphinx import __version__ as sphinx_version
from sphinx.highlighting import PygmentsBridge
from pygments.formatters.latex import LatexFormatter
from os import listdir
from os.path import basename
from os.path import dirname
from os.path import join as path_join

try:
    # Python 2.
    import ConfigParser as configparser
except:
    # Python 3.
    import configparser


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

# Configuration for man pages
man_pages = [("testpmd_app_ug/run_app", "testpmd",
              "tests for dpdk pmds", "", 1),
             ("tools/pdump", "dpdk-pdump",
              "enable packet capture on dpdk ports", "", 1),
             ("tools/proc_info", "dpdk-procinfo",
              "access dpdk port stats and memory info", "", 1),
             ("tools/pmdinfo", "dpdk-pmdinfo",
              "dump a PMDs hardware support info", "", 1),
             ("tools/devbind", "dpdk-devbind",
              "check device status and bind/unbind them from drivers", "", 8)]

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


def generate_nic_overview_table(output_filename):
    """
    Function to generate the NIC Overview Table from the ini files that define
    the features for each NIC.

    The default features for the table and their order is defined by the
    'default.ini' file.

    """
    # Default worning string.
    warning = 'Warning generate_nic_overview_table()'

    # Get the default features and order from the 'default.ini' file.
    ini_path = path_join(dirname(output_filename), 'features')
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(path_join(ini_path, 'default.ini'))
    default_section = 'Features'
    default_features = config.items(default_section)

    # Create a dict of the valid features to validate the other ini files.
    valid_features = {}
    max_feature_length = 0
    for feature in default_features:
        key = feature[0]
        valid_features[key] = ' '
        max_feature_length = max(max_feature_length, len(key))

    # Get a list of NIC ini files, excluding 'default.ini'.
    ini_files = [basename(file) for file in listdir(ini_path)
                 if file.endswith('.ini') and file != 'default.ini']
    ini_files.sort()

    # Build up a list of the table header names from the ini filenames.
    header_names = []
    for ini_filename in ini_files:
        name = ini_filename[:-4]
        name = name.replace('_vf', 'vf')

        # Pad the table header names to match the existing format.
        if '_vec' in name:
            pmd, vec = name.split('_')
            name = '{0:{fill}{align}7}vec'.format(pmd, fill='.', align='<')
        else:
            name = '{0:{fill}{align}10}'.format(name, fill=' ', align='<')

        header_names.append(name)

    # Create a dict of the defined features for each NIC from the ini files.
    ini_data = {}
    for ini_filename in ini_files:
        config = configparser.ConfigParser()
        config.optionxform = str
        config.read(path_join(ini_path, ini_filename))

        # Initialize the dict with the default.ini value.
        ini_data[ini_filename] = valid_features.copy()

        # Check for a valid ini section.
        if not config.has_section(default_section):
            print("{}: File '{}' has no [{}] secton".format(warning,
                                                            ini_filename,
                                                            default_section))
            continue

        # Check for valid features names.
        for name, value in config.items(default_section):
            if name not in valid_features:
                print("{}: Unknown feature '{}' in '{}'".format(warning,
                                                                name,
                                                                ini_filename))
                continue

            if value is not '':
                # Get the first letter only.
                ini_data[ini_filename][name] = value[0]

    # Print out the RST NIC Overview table from the ini file data.
    outfile = open(output_filename, 'w')
    num_cols = len(header_names)

    print('.. table:: Features availability in networking drivers\n',
          file=outfile)

    print_table_header(outfile, num_cols, header_names)
    print_table_body(outfile, num_cols, ini_files, ini_data, default_features)


def print_table_header(outfile, num_cols, header_names):
    """ Print the RST table header. The header names are vertical. """
    print_table_divider(outfile, num_cols)

    line = ''
    for name in header_names:
        line += ' ' + name[0]

    print_table_row(outfile, 'Feature', line)

    for i in range(1, 10):
        line = ''
        for name in header_names:
            line += ' ' + name[i]

        print_table_row(outfile, '', line)

    print_table_divider(outfile, num_cols)


def print_table_body(outfile, num_cols, ini_files, ini_data, default_features):
    """ Print out the body of the table. Each row is a NIC feature. """

    for feature, _ in default_features:
        line = ''

        for ini_filename in ini_files:
            line += ' ' + ini_data[ini_filename][feature]

        print_table_row(outfile, feature, line)

    print_table_divider(outfile, num_cols)


def print_table_row(outfile, feature, line):
    """ Print a single row of the table with fixed formatting. """
    line = line.rstrip()
    print('   {:<20}{}'.format(feature, line), file=outfile)


def print_table_divider(outfile, num_cols):
    """ Print the table divider line. """
    line = ' '
    column_dividers = ['='] * num_cols
    line += ' '.join(column_dividers)

    feature = '=' * 20

    print_table_row(outfile, feature, line)


def setup(app):
    generate_nic_overview_table('doc/guides/nics/overview_table.txt')

    if LooseVersion(sphinx_version) < LooseVersion('1.3.1'):
        print('Upgrade sphinx to version >= 1.3.1 for '
              'improved Figure/Table number handling.')
        # Add a role to handle :numref: references.
        app.add_role('numref', numref_role)
        # Process the numref references once the doctree has been created.
        app.connect('doctree-resolved', process_numref)
