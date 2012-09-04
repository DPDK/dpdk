#!/usr/bin/env python

#   BSD LICENSE
# 
#   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
#  version: DPDK.L.1.2.3-3

import sys, re
import numpy as np
import matplotlib
matplotlib.use('Agg') # we don't want to use X11
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

INT = "([-+]?[0-9][0-9]*)"

class RingTest:
    l = []

    def __init__(self):
        pass

    # sort a test case list
    def sort(self, x, y):
        for t in [ "enq_core", "deq_core", "enq_bulk", "deq_bulk", "rate" ]:
            if x[t] > y[t]:
                return 1
            if x[t] < y[t]:
                return -1
        return 0

    # add a test case
    def add(self, **args):
        self.l.append(args)

    # get an ordered list matching parameters
    # ex: r.get(enq_core=1, deq_core=1)
    def get(self, **args):
        retlist = []
        for t in self.l:
            add_it = 1
            for a in args:
                if args[a] != t[a]:
                    add_it = 0
                    break
            if add_it:
                retlist.append(t)
        retlist.sort(cmp=self.sort)
        return retlist

    # return an ordered list of all values for this param or param list
    # ex: r.get_value_list("enq_core")
    def get_value_list(self, param):
        retlist = []
        if type(param) is not list:
            param = [param]
        for t in self.l:
            entry = []
            for p in param:
                entry.append(t[p])
            if len(entry) == 1:
                entry = entry[0]
            else:
                entry = tuple(entry)
            if not entry in retlist:
                retlist.append(entry)
        retlist.sort()
        return retlist

# read the file and return a RingTest object containing all data
def read_data_from_file(filename):

    ring_test = RingTest()

    # parse the file: it produces a list of dict containing the data for
    # each test case (each dict in the list corresponds to a line)
    f = open(filename)
    while True:
        l = f.readline()

        if l == "":
            break

        regexp  = "ring_autotest "
        regexp += "e/d_core=%s,%s e/d_bulk=%s,%s "%(INT, INT, INT, INT)
        regexp += "sp=%s sc=%s "%(INT, INT)
        regexp += "rate_persec=%s"%(INT)
        m = re.match(regexp, l)
        if m == None:
            continue

        ring_test.add(enq_core = int(m.groups()[0]),
                      deq_core = int(m.groups()[1]),
                      enq_bulk = int(m.groups()[2]),
                      deq_bulk = int(m.groups()[3]),
                      sp = int(m.groups()[4]),
                      sc = int(m.groups()[5]),
                      rate = int(m.groups()[6]))

    f.close()
    return ring_test

def millions(x, pos):
    return '%1.1fM' % (x*1e-6)

# graph one, with specific parameters -> generate a .svg file
def graph_one(str, ring_test, enq_core, deq_core, sp, sc):
    filename = "ring_%d_%d"%(enq_core, deq_core)
    if sp:
        sp_str = "sp"
    else:
        sp_str = "mp"
    if sc:
        sc_str = "sc"
    else:
        sc_str = "mc"
    filename += "_%s_%s.svg"%(sp_str, sc_str)


    enq_bulk_list = ring_test.get_value_list("enq_bulk")
    N_enq_bulk = len(enq_bulk_list)
    enq_names = map(lambda x:"enq=%d"%x, enq_bulk_list)

    deq_bulk_list = ring_test.get_value_list("deq_bulk")
    N_deq_bulk = len(deq_bulk_list)
    deq_names = map(lambda x:"deq=%d"%x, deq_bulk_list)

    N = N_enq_bulk * (N_deq_bulk + 1)
    rates = []

    colors = []
    for enq_bulk in ring_test.get_value_list("enq_bulk"):
        col = 0.
        for deq_bulk in ring_test.get_value_list("deq_bulk"):
            col += 0.9 / len(ring_test.get_value_list("deq_bulk"))
            r = ring_test.get(enq_core=enq_core, deq_core=deq_core,
                              enq_bulk=enq_bulk, deq_bulk=deq_bulk,
                              sp=sp, sc=sc)
            r = r[0]["rate"]
            rates.append(r)
            colors.append((1. - col, 0.2, col, 1.)) # rgba

        rates.append(0)
        colors.append((0.,0.,0.,0.))

    ind = np.arange(N)  # the x locations for the groups
    width = 1           # the width of the bars: can also be len(x) sequence


    formatter = FuncFormatter(millions)
    fig = plt.figure()
    p = plt.bar(ind, tuple(rates), width, color=tuple(colors))
    fig.axes[0].yaxis.set_major_formatter(formatter)

    plt.ylabel('Obj/sec')
    #plt.ylim(0, 400000000.)
    plt.title("Ring autotest \"%s\"\nenq core(s)=%d, deq core(s)=%d, %s, %s"\
                  %(str, enq_core, deq_core, sp_str, sc_str))
    ind_names = np.arange(N_enq_bulk) * (N_deq_bulk+1) + (N_deq_bulk+1) / 2
    plt.xticks(ind_names, tuple(enq_names))
    plt.legend(tuple([p[i] for i in range(N_deq_bulk)]), tuple(deq_names),
               loc="upper left")
    plt.savefig(filename)

if len(sys.argv) != 3:
    print "usage: graph_ring.py file title"
    sys.exit(1)

ring_test = read_data_from_file(sys.argv[1])

for enq_core, deq_core, sp, sc in \
        ring_test.get_value_list(["enq_core", "deq_core", "sp", "sc"]):
    graph_one(sys.argv[2], ring_test, enq_core, deq_core, sp, sc)
