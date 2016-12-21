#!/usr/bin/env python

#   BSD LICENSE
#
#   Copyright(c) 2016 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
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

#
# This script creates a visual representation for a configuration file used by
# the DPDK ip_pipeline application.
#
# The input configuration file is translated to an output file in DOT syntax,
# which is then used to create the image file using graphviz
# (www.graphviz.org).
#

from __future__ import print_function
import argparse
import re
import os

#
# Command to generate the image file
#
DOT_COMMAND = 'dot -Gsize=20,30 -Tpng %s > %s'

#
# Layout of generated DOT file
#
DOT_INTRO = \
    '#\n# Command to generate image file:\n# \t%s\n#\n\n'
DOT_GRAPH_BEGIN = \
    'digraph g {\n  graph [ splines = true rankdir = "LR" ]\n'
DOT_NODE_LINK_RX = \
    '  "%s RX" [ shape = box style = filled fillcolor = yellowgreen ]\n'
DOT_NODE_LINK_TX = \
    '  "%s TX" [ shape = box style = filled fillcolor = yellowgreen ]\n'
DOT_NODE_KNI_RX = \
    '  "%s RX" [ shape = box style = filled fillcolor = orange ]\n'
DOT_NODE_KNI_TX = \
    '  "%s TX" [ shape = box style = filled fillcolor = orange ]\n'
DOT_NODE_TAP_RX = \
    '  "%s RX" [ shape = box style = filled fillcolor = gold ]\n'
DOT_NODE_TAP_TX = \
    '  "%s TX" [ shape = box style = filled fillcolor = gold ]\n'
DOT_NODE_SOURCE = \
    '  "%s" [ shape = box style = filled fillcolor = darkgreen ]\n'
DOT_NODE_SINK = \
    '  "%s" [ shape = box style = filled fillcolor = peachpuff ]\n'
DOT_NODE_PIPELINE = \
    '  "%s" [ shape = box style = filled fillcolor = royalblue ]\n'
DOT_EDGE_PKTQ = \
    '  "%s" -> "%s" [ label = "%s" color = gray ]\n'
DOT_GRAPH_END = \
    '}\n'

# Relationships between the graph nodes and the graph edges:
#
# Edge ID | Edge Label | Writer Node | Reader Node   | Dependencies
# --------+------------+-------------+---------------+--------------
# RXQx.y  | RXQx.y     | LINKx       | PIPELINEz     | LINKx
# TXQx.y  | TXQx.y     | PIPELINEz   | LINKx         | LINKx
# SWQx    | SWQx       | PIPELINEy   | PIPELINEz     | -
# TMx     | TMx        | PIPELINEy   | PIPELINEz     | LINKx
# KNIx RX | KNIx       | KNIx RX     | PIPELINEy     | KNIx, LINKx
# KNIx TX | KNIx       | PIPELINEy   | KNIx TX       | KNIx, LINKx
# TAPx RX | TAPx       | TAPx RX     | PIPELINEy     | TAPx
# TAPx TX | TAPx       | PIPELINEy   | TAPx TX       | TAPx
# SOURCEx | SOURCEx    | SOURCEx     | PIPELINEy     | SOURCEx
# SINKx   | SINKx      | PIPELINEy   | SINKx         | SINKx


#
# Parse the input configuration file to detect the graph nodes and edges
#
def process_config_file(cfgfile):
    edges = {}
    links = set()
    knis = set()
    taps = set()
    sources = set()
    sinks = set()
    pipelines = set()
    pipeline = ''

    dotfile = cfgfile + '.txt'
    imgfile = cfgfile + '.png'

    #
    # Read configuration file
    #
    lines = open(cfgfile, 'r')
    for line in lines:
        # Remove any leading and trailing white space characters
        line = line.strip()

        # Remove any comment at end of line
        line, sep, tail = line.partition(';')

        # Look for next "PIPELINE" section
        match = re.search(r'\[(PIPELINE\d+)\]', line)
        if match:
            pipeline = match.group(1)
            continue

        # Look for next "pktq_in" section entry
        match = re.search(r'pktq_in\s*=\s*(.+)', line)
        if match:
            pipelines.add(pipeline)
            for q in re.findall('\S+', match.group(1)):
                match_rxq = re.search(r'^RXQ(\d+)\.\d+$', q)
                match_swq = re.search(r'^SWQ\d+$', q)
                match_tm = re.search(r'^TM(\d+)$', q)
                match_kni = re.search(r'^KNI(\d+)$', q)
                match_tap = re.search(r'^TAP\d+$', q)
                match_source = re.search(r'^SOURCE\d+$', q)

                # Set ID for the current packet queue (graph edge)
                q_id = ''
                if match_rxq or match_swq or match_tm or match_source:
                    q_id = q
                elif match_kni or match_tap:
                    q_id = q + ' RX'
                else:
                    print('Error: Unrecognized pktq_in element "%s"' % q)
                    return

                # Add current packet queue to the set of graph edges
                if q_id not in edges:
                    edges[q_id] = {}
                if 'label' not in edges[q_id]:
                    edges[q_id]['label'] = q
                if 'readers' not in edges[q_id]:
                    edges[q_id]['readers'] = []
                if 'writers' not in edges[q_id]:
                    edges[q_id]['writers'] = []

                # Add reader for the new edge
                edges[q_id]['readers'].append(pipeline)

                # Check for RXQ
                if match_rxq:
                    link = 'LINK' + str(match_rxq.group(1))
                    edges[q_id]['writers'].append(link + ' RX')
                    links.add(link)
                    continue

                # Check for SWQ
                if match_swq:
                    continue

                # Check for TM
                if match_tm:
                    link = 'LINK' + str(match_tm.group(1))
                    links.add(link)
                    continue

                # Check for KNI
                if match_kni:
                    link = 'LINK' + str(match_kni.group(1))
                    edges[q_id]['writers'].append(q_id)
                    knis.add(q)
                    links.add(link)
                    continue

                # Check for TAP
                if match_tap:
                    edges[q_id]['writers'].append(q_id)
                    taps.add(q)
                    continue

                # Check for SOURCE
                if match_source:
                    edges[q_id]['writers'].append(q)
                    sources.add(q)
                    continue

                continue

        # Look for next "pktq_out" section entry
        match = re.search(r'pktq_out\s*=\s*(.+)', line)
        if match:
            for q in re.findall('\S+', match.group(1)):
                match_txq = re.search(r'^TXQ(\d+)\.\d+$', q)
                match_swq = re.search(r'^SWQ\d+$', q)
                match_tm = re.search(r'^TM(\d+)$', q)
                match_kni = re.search(r'^KNI(\d+)$', q)
                match_tap = re.search(r'^TAP(\d+)$', q)
                match_sink = re.search(r'^SINK(\d+)$', q)

                # Set ID for the current packet queue (graph edge)
                q_id = ''
                if match_txq or match_swq or match_tm or match_sink:
                    q_id = q
                elif match_kni or match_tap:
                    q_id = q + ' TX'
                else:
                    print('Error: Unrecognized pktq_out element "%s"' % q)
                    return

                # Add current packet queue to the set of graph edges
                if q_id not in edges:
                    edges[q_id] = {}
                if 'label' not in edges[q_id]:
                    edges[q_id]['label'] = q
                if 'readers' not in edges[q_id]:
                    edges[q_id]['readers'] = []
                if 'writers' not in edges[q_id]:
                    edges[q_id]['writers'] = []

                # Add writer for the new edge
                edges[q_id]['writers'].append(pipeline)

                # Check for TXQ
                if match_txq:
                    link = 'LINK' + str(match_txq.group(1))
                    edges[q_id]['readers'].append(link + ' TX')
                    links.add(link)
                    continue

                # Check for SWQ
                if match_swq:
                    continue

                # Check for TM
                if match_tm:
                    link = 'LINK' + str(match_tm.group(1))
                    links.add(link)
                    continue

                # Check for KNI
                if match_kni:
                    link = 'LINK' + str(match_kni.group(1))
                    edges[q_id]['readers'].append(q_id)
                    knis.add(q)
                    links.add(link)
                    continue

                # Check for TAP
                if match_tap:
                    edges[q_id]['readers'].append(q_id)
                    taps.add(q)
                    continue

                # Check for SINK
                if match_sink:
                    edges[q_id]['readers'].append(q)
                    sinks.add(q)
                    continue

                continue

    #
    # Write DOT file
    #
    print('Creating DOT file "%s" ...' % dotfile)
    dot_cmd = DOT_COMMAND % (dotfile, imgfile)
    file = open(dotfile, 'w')
    file.write(DOT_INTRO % dot_cmd)
    file.write(DOT_GRAPH_BEGIN)

    # Write the graph nodes to the DOT file
    for l in sorted(links):
        file.write(DOT_NODE_LINK_RX % l)
        file.write(DOT_NODE_LINK_TX % l)
    for k in sorted(knis):
        file.write(DOT_NODE_KNI_RX % k)
        file.write(DOT_NODE_KNI_TX % k)
    for t in sorted(taps):
        file.write(DOT_NODE_TAP_RX % t)
        file.write(DOT_NODE_TAP_TX % t)
    for s in sorted(sources):
        file.write(DOT_NODE_SOURCE % s)
    for s in sorted(sinks):
        file.write(DOT_NODE_SINK % s)
    for p in sorted(pipelines):
        file.write(DOT_NODE_PIPELINE % p)

    # Write the graph edges to the DOT file
    for q in sorted(edges.keys()):
        rw = edges[q]
        if 'writers' not in rw:
            print('Error: "%s" has no writer' % q)
            return
        if 'readers' not in rw:
            print('Error: "%s" has no reader' % q)
            return
        for w in rw['writers']:
            for r in rw['readers']:
                file.write(DOT_EDGE_PKTQ % (w, r, rw['label']))

    file.write(DOT_GRAPH_END)
    file.close()

    #
    # Execute the DOT command to create the image file
    #
    print('Creating image file "%s" ...' % imgfile)
    if os.system('which dot > /dev/null'):
        print('Error: Unable to locate "dot" executable.'
              'Please install the "graphviz" package (www.graphviz.org).')
        return

    os.system(dot_cmd)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Create diagram for IP '
                                                 'pipeline configuration '
                                                 'file.')

    parser.add_argument(
        '-f',
        '--file',
        help='input configuration file (e.g. "ip_pipeline.cfg")',
        required=True)

    args = parser.parse_args()

    process_config_file(args.file)
