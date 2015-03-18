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
from sphinx.highlighting import PygmentsBridge
from pygments.formatters.latex import LatexFormatter

project = 'DPDK'

html_show_copyright = False

version = subprocess.check_output(['make', '-sRrC', '../../', 'showversion']).decode('utf-8')
release = version

master_doc = 'index'

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
\usepackage{DejaVuSansMono}
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
