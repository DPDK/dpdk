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

import sys, re
import numpy as np
import matplotlib
matplotlib.use('Agg') # we don't want to use X11
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

INT = "([-+]?[0-9][0-9]*)"

class MempoolTest:
    l = []

    def __init__(self):
        pass

    # sort a test case list
    def sort(self, x, y):
        for t in [ "cache", "cores", "n_get_bulk", "n_put_bulk",
                   "n_keep", "rate" ]:
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

# read the file and return a MempoolTest object containing all data
def read_data_from_file(filename):

    mempool_test = MempoolTest()

    # parse the file: it produces a list of dict containing the data for
    # each test case (each dict in the list corresponds to a line)
    f = open(filename)
    while True:
        l = f.readline()

        if l == "":
            break

        regexp  = "mempool_autotest "
        regexp += "cache=%s cores=%s "%(INT, INT)
        regexp += "n_get_bulk=%s n_put_bulk=%s "%(INT, INT)
        regexp += "n_keep=%s rate_persec=%s"%(INT, INT)
        m = re.match(regexp, l)
        if m == None:
            continue

        mempool_test.add(cache = int(m.groups()[0]),
                         cores = int(m.groups()[1]),
                         n_get_bulk = int(m.groups()[2]),
                         n_put_bulk = int(m.groups()[3]),
                         n_keep = int(m.groups()[4]),
                         rate = int(m.groups()[5]))

    f.close()
    return mempool_test

def millions(x, pos):
    return '%1.1fM' % (x*1e-6)

# graph one, with specific parameters -> generate a .svg file
def graph_one(str, mempool_test, cache, cores, n_keep):
    filename = "mempool_%d_%d_%d.svg"%(cache, cores, n_keep)

    n_get_bulk_list = mempool_test.get_value_list("n_get_bulk")
    N_n_get_bulk = len(n_get_bulk_list)
    get_names = map(lambda x:"get=%d"%x, n_get_bulk_list)

    n_put_bulk_list = mempool_test.get_value_list("n_put_bulk")
    N_n_put_bulk = len(n_put_bulk_list)
    put_names = map(lambda x:"put=%d"%x, n_put_bulk_list)

    N = N_n_get_bulk * (N_n_put_bulk + 1)
    rates = []

    colors = []
    for n_get_bulk in mempool_test.get_value_list("n_get_bulk"):
        col = 0.
        for n_put_bulk in mempool_test.get_value_list("n_put_bulk"):
            col += 0.9 / len(mempool_test.get_value_list("n_put_bulk"))
            r = mempool_test.get(cache=cache, cores=cores,
                                 n_get_bulk=n_get_bulk,
                                 n_put_bulk=n_put_bulk, n_keep=n_keep)
            if len(r) != 0:
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
    title  = "Mempool autotest \"%s\"\n"%(str)
    title += "cache=%d, core(s)=%d, n_keep=%d"%(cache, cores, n_keep)
    plt.title(title)
    ind_names = np.arange(N_n_get_bulk) * (N_n_put_bulk+1) + (N_n_put_bulk+1) / 2
    plt.xticks(ind_names, tuple(get_names))
    plt.legend(tuple([p[i] for i in range(N_n_put_bulk)]), tuple(put_names),
               loc="upper left")
    plt.savefig(filename)

if len(sys.argv) != 3:
    print "usage: graph_mempool.py file title"
    sys.exit(1)

mempool_test = read_data_from_file(sys.argv[1])

for cache, cores, n_keep in mempool_test.get_value_list(["cache", "cores",
                                                         "n_keep"]):
    graph_one(sys.argv[2], mempool_test, cache, cores, n_keep)
