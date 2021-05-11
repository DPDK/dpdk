#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

from docutils import nodes
from distutils.version import LooseVersion
from sphinx import __version__ as sphinx_version
from os import listdir
from os import environ
from os.path import basename
from os.path import dirname
from os.path import join as path_join
from sys import argv, stderr

import configparser

try:
    import sphinx_rtd_theme

    html_theme = "sphinx_rtd_theme"
except:
    print('Install the sphinx ReadTheDocs theme for improved html documentation '
          'layout: https://sphinx-rtd-theme.readthedocs.io/',
          file=stderr)
    pass

stop_on_error = ('-W' in argv)

project = 'Data Plane Development Kit'
html_logo = '../logo/DPDK_logo_vertical_rev_small.png'
if LooseVersion(sphinx_version) >= LooseVersion('3.5'):
    html_permalinks = False
else:
    html_add_permalinks = ""
html_show_copyright = False
highlight_language = 'none'

release = environ.setdefault('DPDK_VERSION', "None")
version = release

master_doc = 'index'

# Maximum feature description string length
feature_str_len = 30

# Figures, tables and code-blocks automatically numbered if they have caption
numfig = True

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


# ####### :numref: fallback ########
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


def generate_overview_table(output_filename, table_id, section, table_name, title):
    """
    Function to generate the Overview Table from the ini files that define
    the features for each driver.

    The default features for the table and their order is defined by the
    'default.ini' file.

    """
    # Default warning string.
    warning = 'Warning generate_overview_table()'

    # Get the default features and order from the 'default.ini' file.
    ini_path = path_join(dirname(output_filename), 'features')
    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(path_join(ini_path, 'default.ini'))
    default_features = config.items(section)

    # Create a dict of the valid features to validate the other ini files.
    valid_features = {}
    max_feature_length = 0
    for feature in default_features:
        key = feature[0]
        valid_features[key] = ' '
        max_feature_length = max(max_feature_length, len(key))

    # Get a list of driver ini files, excluding 'default.ini'.
    ini_files = [basename(file) for file in listdir(ini_path)
                 if file.endswith('.ini') and file != 'default.ini']
    ini_files.sort()

    # Build up a list of the table header names from the ini filenames.
    pmd_names = []
    for ini_filename in ini_files:
        name = ini_filename[:-4]
        name = name.replace('_vf', 'vf')
        pmd_names.append(name)

    # Pad the table header names.
    max_header_len = len(max(pmd_names, key=len))
    header_names = []
    for name in pmd_names:
        if '_vec' in name:
            pmd, vec = name.split('_')
            name = '{0:{fill}{align}{width}}vec'.format(pmd,
                    fill='.', align='<', width=max_header_len-3)
        else:
            name = '{0:{fill}{align}{width}}'.format(name,
                    fill=' ', align='<', width=max_header_len)
        header_names.append(name)

    # Create a dict of the defined features for each driver from the ini files.
    ini_data = {}
    for ini_filename in ini_files:
        config = configparser.ConfigParser()
        config.optionxform = str
        config.read(path_join(ini_path, ini_filename))

        # Initialize the dict with the default.ini value.
        ini_data[ini_filename] = valid_features.copy()

        # Check for a valid ini section.
        if not config.has_section(section):
            print("{}: File '{}' has no [{}] secton".format(warning,
                                                            ini_filename,
                                                            section),
                                                            file=stderr)
            if stop_on_error:
                raise Exception('Warning is treated as a failure')
            continue

        # Check for valid features names.
        for name, value in config.items(section):
            if name not in valid_features:
                print("{}: Unknown feature '{}' in '{}'".format(warning,
                                                                name,
                                                                ini_filename),
                                                                file=stderr)
                if stop_on_error:
                    raise Exception('Warning is treated as a failure')
                continue

            if value:
                # Get the first letter only.
                ini_data[ini_filename][name] = value[0]

    # Print out the RST Driver Overview table from the ini file data.
    outfile = open(output_filename, 'w')
    num_cols = len(header_names)

    print_table_css(outfile, table_id)
    print('.. table:: ' + table_name + '\n', file=outfile)
    print_table_header(outfile, num_cols, header_names, title)
    print_table_body(outfile, num_cols, ini_files, ini_data, default_features)


def print_table_header(outfile, num_cols, header_names, title):
    """ Print the RST table header. The header names are vertical. """
    print_table_divider(outfile, num_cols)

    line = ''
    for name in header_names:
        line += ' ' + name[0]

    print_table_row(outfile, title, line)

    for i in range(1, len(header_names[0])):
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
    print('   {:<{}}{}'.format(feature, feature_str_len, line), file=outfile)


def print_table_divider(outfile, num_cols):
    """ Print the table divider line. """
    line = ' '
    column_dividers = ['='] * num_cols
    line += ' '.join(column_dividers)

    feature = '=' * feature_str_len

    print_table_row(outfile, feature, line)


def print_table_css(outfile, table_id):
    template = """
.. raw:: html

   <style>
      .wy-nav-content {
         opacity: .99;
      }
      table#idx {
         cursor: default;
         overflow: hidden;
      }
      table#idx p {
         margin: 0;
         line-height: inherit;
      }
      table#idx th, table#idx td {
         text-align: center;
         border: solid 1px #ddd;
      }
      table#idx th {
         padding: 0.5em 0;
      }
      table#idx th, table#idx th p {
         font-size: 11px;
         white-space: pre-wrap;
         vertical-align: top;
         min-width: 0.9em;
      }
      table#idx col:first-child {
         width: 0;
      }
      table#idx th:first-child {
         vertical-align: bottom;
      }
      table#idx td {
         padding: 1px;
      }
      table#idx td, table#idx td p {
         font-size: 11px;
      }
      table#idx td:first-child {
         padding-left: 1em;
         text-align: left;
      }
      table#idx tr:nth-child(2n-1) td {
         background-color: rgba(210, 210, 210, 0.2);
      }
      table#idx th:not(:first-child):hover,
      table#idx td:not(:first-child):hover {
         position: relative;
      }
      table#idx th:not(:first-child):hover::after,
      table#idx td:not(:first-child):hover::after {
         content: '';
         height: 6000px;
         top: -3000px;
         width: 100%;
         left: 0;
         position: absolute;
         z-index: -1;
         background-color: #ffb;
      }
      table#idx tr:hover td {
         background-color: #ffb;
      }
   </style>
"""
    print(template.replace("idx", "id%d" % (table_id)), file=outfile)


def setup(app):
    table_file = dirname(__file__) + '/nics/overview_table.txt'
    generate_overview_table(table_file, 1,
                            'Features',
                            'Features availability in networking drivers',
                            'Feature')
    table_file = dirname(__file__) + '/cryptodevs/overview_feature_table.txt'
    generate_overview_table(table_file, 1,
                            'Features',
                            'Features availability in crypto drivers',
                            'Feature')
    table_file = dirname(__file__) + '/cryptodevs/overview_cipher_table.txt'
    generate_overview_table(table_file, 2,
                            'Cipher',
                            'Cipher algorithms in crypto drivers',
                            'Cipher algorithm')
    table_file = dirname(__file__) + '/cryptodevs/overview_auth_table.txt'
    generate_overview_table(table_file, 3,
                            'Auth',
                            'Authentication algorithms in crypto drivers',
                            'Authentication algorithm')
    table_file = dirname(__file__) + '/cryptodevs/overview_aead_table.txt'
    generate_overview_table(table_file, 4,
                            'AEAD',
                            'AEAD algorithms in crypto drivers',
                            'AEAD algorithm')
    table_file = dirname(__file__) + '/cryptodevs/overview_asym_table.txt'
    generate_overview_table(table_file, 5,
                            'Asymmetric',
                            'Asymmetric algorithms in crypto drivers',
                            'Asymmetric algorithm')
    table_file = dirname(__file__) + '/compressdevs/overview_feature_table.txt'
    generate_overview_table(table_file, 1,
                            'Features',
                            'Features availability in compression drivers',
                            'Feature')
    table_file = dirname(__file__) + '/regexdevs/overview_feature_table.txt'
    generate_overview_table(table_file, 1,
                            'Features',
                            'Features availability in regex drivers',
                            'Feature')
    table_file = dirname(__file__) + '/vdpadevs/overview_feature_table.txt'
    generate_overview_table(table_file, 1,
                            'Features',
                            'Features availability in vDPA drivers',
                            'Feature')
    table_file = dirname(__file__) + '/bbdevs/overview_feature_table.txt'
    generate_overview_table(table_file, 1,
                            'Features',
                            'Features availability in bbdev drivers',
                            'Feature')

    if LooseVersion(sphinx_version) < LooseVersion('1.3.1'):
        print('Upgrade sphinx to version >= 1.3.1 for '
              'improved Figure/Table number handling.',
              file=stderr)
        # Add a role to handle :numref: references.
        app.add_role('numref', numref_role)
        # Process the numref references once the doctree has been created.
        app.connect('doctree-resolved', process_numref)

    try:
        # New function in sphinx 1.8
        app.add_css_file('css/custom.css')
    except:
        app.add_stylesheet('css/custom.css')
