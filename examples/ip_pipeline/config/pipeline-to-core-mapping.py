#! /usr/bin/python2

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
# This script maps the set of pipelines identified (MASTER pipelines are
# ignored) from the input configuration file to the set of cores
# provided as input argument and creates configuration files for each of
# the mapping combinations.
#

from __future__ import print_function
import sys
import errno
import os
import re
import array
import itertools
import re
import argparse
from collections import namedtuple

# default values
enable_stage0_traceout = 1
enable_stage1_traceout = 1
enable_stage2_traceout = 1

enable_stage1_fileout = 1
enable_stage2_fileout = 1

Constants = namedtuple('Constants', ['MAX_CORES', 'MAX_PIPELINES'])
constants = Constants(16, 64)

# pattern for physical core
pattern_phycore = '^(s|S)\d(c|C)[1-9][0-9]*$'
reg_phycore = re.compile(pattern_phycore)


def popcount(mask):
    return bin(mask).count("1")


def len2mask(length):
    if (length == 0):
        return 0

    if (length > 64):
        sys.exit('error: len2mask - length %i > 64. exiting' % length)

    return int('1' * length, 2)


def bitstring_write(n, n_bits):
    tmpstr = ""
    if (n_bits > 64):
        return

    i = n_bits - 1
    while (i >= 0):
        cond = (n & (1 << i))
        if (cond):
            print('1', end='')
            tmpstr += '1'
        else:
            print('0', end='')
            tmpstr += '0'
        i -= 1
    return tmpstr


class Cores0:

    def __init__(self):
        self.n_pipelines = 0


class Cores1:

    def __init__(self):
        self.pipelines = 0
        self.n_pipelines = 0


class Cores2:

    def __init__(self):
        self.pipelines = 0
        self.n_pipelines = 0
        self.counter = 0
        self.counter_max = 0
        self.bitpos = array.array(
            "L", itertools.repeat(0, constants.MAX_PIPELINES))


class Context0:

    def __init__(self):
        self.cores = [Cores0() for i in range(0, constants.MAX_CORES)]
        self.n_cores = 0
        self.n_pipelines = 0
        self.n_pipelines0 = 0
        self.pos = 0
        self.file_comment = ""
        self.ctx1 = None
        self.ctx2 = None

    def stage0_print(self):
        print('printing Context0 obj')
        print('c0.cores(n_pipelines) = [ ', end='')
        for cores_count in range(0, constants.MAX_CORES):
            print(self.cores[cores_count].n_pipelines, end=' ')
        print(']')
        print('c0.n_cores = %d' % self.n_cores)
        print('c0.n_pipelines = %d' % self.n_pipelines)
        print('c0.n_pipelines0 = %d' % self.n_pipelines0)
        print('c0.pos = %d' % self.pos)
        print('c0.file_comment = %s' % self.file_comment)
        if (self.ctx1 is not None):
            print('c0.ctx1 = ', end='')
            print(repr(self.ctx1))
        else:
            print('c0.ctx1 = None')

        if (self.ctx2 is not None):
            print('c0.ctx2 = ', end='')
            print(repr(self.ctx2))
        else:
            print('c0.ctx2 = None')

    def stage0_init(self, num_cores, num_pipelines, ctx1, ctx2):
        self.n_cores = num_cores
        self.n_pipelines = num_pipelines
        self.ctx1 = ctx1
        self.ctx2 = ctx2

    def stage0_process(self):
        # stage0 init
        self.cores[0].n_pipelines = self.n_pipelines
        self.n_pipelines0 = 0
        self.pos = 1

        while True:
            # go forward
            while True:
                if ((self.pos < self.n_cores) and (self.n_pipelines0 > 0)):
                    self.cores[self.pos].n_pipelines = min(
                        self.cores[self.pos - 1].n_pipelines,
                        self.n_pipelines0)
                    self.n_pipelines0 -= self.cores[self.pos].n_pipelines
                    self.pos += 1
                else:
                    break

            # check solution
            if (self.n_pipelines0 == 0):
                self.stage0_log()
                self.ctx1.stage1_init(self, self.ctx2)  # self is object c0
                self.ctx1.stage1_process()

            # go backward
            while True:
                if (self.pos == 0):
                    return

                self.pos -= 1
                if ((self.cores[self.pos].n_pipelines > 1) and
                        (self.pos != (self.n_cores - 1))):
                    break

                self.n_pipelines0 += self.cores[self.pos].n_pipelines
                self.cores[self.pos].n_pipelines = 0

            # rearm
            self.cores[self.pos].n_pipelines -= 1
            self.n_pipelines0 += 1
            self.pos += 1

    def stage0_log(self):
        tmp_file_comment = ""
        if(enable_stage0_traceout != 1):
            return

        print('STAGE0: ', end='')
        tmp_file_comment += 'STAGE0: '
        for cores_count in range(0, self.n_cores):
            print('C%d = %d\t'
                  % (cores_count,
                      self.cores[cores_count].n_pipelines), end='')
            tmp_file_comment += "C{} = {}\t".format(
                cores_count, self.cores[cores_count].n_pipelines)
        # end for
        print('')
        self.ctx1.stage0_file_comment = tmp_file_comment
        self.ctx2.stage0_file_comment = tmp_file_comment


class Context1:
    _fileTrace = None

    def __init__(self):
        self.cores = [Cores1() for i in range(constants.MAX_CORES)]
        self.n_cores = 0
        self.n_pipelines = 0
        self.pos = 0
        self.stage0_file_comment = ""
        self.stage1_file_comment = ""

        self.ctx2 = None
        self.arr_pipelines2cores = []

    def stage1_reset(self):
        for i in range(constants.MAX_CORES):
            self.cores[i].pipelines = 0
            self.cores[i].n_pipelines = 0

        self.n_cores = 0
        self.n_pipelines = 0
        self.pos = 0
        self.ctx2 = None
        # clear list
        del self.arr_pipelines2cores[:]

    def stage1_print(self):
        print('printing Context1 obj')
        print('ctx1.cores(pipelines,n_pipelines) = [ ', end='')
        for cores_count in range(0, constants.MAX_CORES):
            print('(%d,%d)' % (self.cores[cores_count].pipelines,
                               self.cores[cores_count].n_pipelines), end=' ')
        print(']')
        print('ctx1.n_cores = %d' % self.n_cores)
        print('ctx1.n_pipelines = %d' % self.n_pipelines)
        print('ctx1.pos = %d' % self.pos)
        print('ctx1.stage0_file_comment = %s' % self.stage0_file_comment)
        print('ctx1.stage1_file_comment = %s' % self.stage1_file_comment)
        if (self.ctx2 is not None):
            print('ctx1.ctx2 = ', end='')
            print(self.ctx2)
        else:
            print('ctx1.ctx2 = None')

    def stage1_init(self, c0, ctx2):
        self.stage1_reset()
        self.n_cores = 0
        while (c0.cores[self.n_cores].n_pipelines > 0):
            self.n_cores += 1

        self.n_pipelines = c0.n_pipelines
        self.ctx2 = ctx2

        self.arr_pipelines2cores = [0] * self.n_pipelines

        i = 0
        while (i < self.n_cores):
            self.cores[i].n_pipelines = c0.cores[i].n_pipelines
            i += 1

    def stage1_process(self):
        pipelines_max = len2mask(self.n_pipelines)
        while True:
            pos = 0
            overlap = 0

            if (self.cores[self.pos].pipelines == pipelines_max):
                if (self.pos == 0):
                    return

                self.cores[self.pos].pipelines = 0
                self.pos -= 1
                continue

            self.cores[self.pos].pipelines += 1
            if (popcount(self.cores[self.pos].pipelines) !=
                    self.cores[self.pos].n_pipelines):
                continue

            overlap = 0
            pos = 0
            while (pos < self.pos):
                if ((self.cores[self.pos].pipelines) &
                        (self.cores[pos].pipelines)):
                    overlap = 1
                    break
                pos += 1

            if (overlap):
                continue

            if ((self.pos > 0) and
                ((self.cores[self.pos].n_pipelines) ==
                    (self.cores[self.pos - 1].n_pipelines)) and
                    ((self.cores[self.pos].pipelines) <
                        (self.cores[self.pos - 1].pipelines))):
                continue

            if (self.pos == self.n_cores - 1):
                self.stage1_log()
                self.ctx2.stage2_init(self)
                self.ctx2.stage2_process()

                if (self.pos == 0):
                    return

                self.cores[self.pos].pipelines = 0
                self.pos -= 1
                continue

            self.pos += 1

    def stage1_log(self):
        tmp_file_comment = ""
        if(enable_stage1_traceout == 1):
            print('STAGE1: ', end='')
            tmp_file_comment += 'STAGE1: '
            i = 0
            while (i < self.n_cores):
                print('C%d = [' % i, end='')
                tmp_file_comment += "C{} = [".format(i)

                j = self.n_pipelines - 1
                while (j >= 0):
                    cond = ((self.cores[i].pipelines) & (1 << j))
                    if (cond):
                        print('1', end='')
                        tmp_file_comment += '1'
                    else:
                        print('0', end='')
                        tmp_file_comment += '0'
                    j -= 1

                print(']\t', end='')
                tmp_file_comment += ']\t'
                i += 1

            print('\n', end='')
            self.stage1_file_comment = tmp_file_comment
            self.ctx2.stage1_file_comment = tmp_file_comment

        # check if file traceing is enabled
        if(enable_stage1_fileout != 1):
            return

        # spit out the combination to file
        self.stage1_process_file()

    def stage1_updateCoresInBuf(self, nPipeline, sCore):
        rePipeline = self._fileTrace.arr_pipelines[nPipeline]
        rePipeline = rePipeline.replace("[", "\[").replace("]", "\]")
        reCore = 'core\s*=\s*((\d*)|(((s|S)\d)?(c|C)[1-9][0-9]*)).*\n'
        sSubs = 'core = ' + sCore + '\n'

        reg_pipeline = re.compile(rePipeline)
        search_match = reg_pipeline.search(self._fileTrace.in_buf)

        if(search_match):
            pos = search_match.start()
            substr1 = self._fileTrace.in_buf[:pos]
            substr2 = self._fileTrace.in_buf[pos:]
            substr2 = re.sub(reCore, sSubs, substr2, 1)
            self._fileTrace.in_buf = substr1 + substr2

    def stage1_process_file(self):
        outFileName = os.path.join(self._fileTrace.out_path,
                                   self._fileTrace.prefix_outfile)
        outFileName += "_{}CoReS".format(self.n_cores)

        i = 0  # represents core number
        while (i < self.n_cores):
            j = self.n_pipelines - 1
            pipeline_idx = 0
            while(j >= 0):
                cond = ((self.cores[i].pipelines) & (1 << j))
                if (cond):
                    # update the pipelines array to match the core
                    # only in case of cond match
                    self.arr_pipelines2cores[
                        pipeline_idx] = fileTrace.in_physical_cores[i]

                j -= 1
                pipeline_idx += 1

            i += 1

        # update the in_buf as per the arr_pipelines2cores
        for pipeline_idx in range(len(self.arr_pipelines2cores)):
            outFileName += "_{}".format(self.arr_pipelines2cores[pipeline_idx])
            self.stage1_updateCoresInBuf(
                pipeline_idx, self.arr_pipelines2cores[pipeline_idx])

        # by now the in_buf is all set to be written to file
        outFileName += self._fileTrace.suffix_outfile
        outputFile = open(outFileName, "w")

        # write out the comments
        strTruncated = ("", "(Truncated)")[self._fileTrace.ncores_truncated]
        outputFile.write(
            "; =============== Pipeline-to-Core Mapping ================\n"
            "; Generated from file {}\n"
            "; Input pipelines = {}\n"
            "; Input cores = {}\n"
            "; N_PIPELINES = {} N_CORES = {} {} hyper_thread = {}\n"
            .format(
                self._fileTrace.in_file_namepath,
                fileTrace.arr_pipelines,
                fileTrace.in_physical_cores,
                self._fileTrace.n_pipelines,
                self._fileTrace.n_cores,
                strTruncated,
                self._fileTrace.hyper_thread))

        outputFile.write(
            "; {stg0cmt}\n"
            "; {stg1cmt}\n"
            "; ========================================================\n"
            "; \n"
            .format(
                stg0cmt=self.stage0_file_comment,
                stg1cmt=self.stage1_file_comment))

        # write buffer contents
        outputFile.write(self._fileTrace.in_buf)
        outputFile.flush()
        outputFile.close()


class Context2:
    _fileTrace = None

    def __init__(self):
        self.cores = [Cores2() for i in range(constants.MAX_CORES)]
        self.n_cores = 0
        self.n_pipelines = 0
        self.pos = 0
        self.stage0_file_comment = ""
        self.stage1_file_comment = ""
        self.stage2_file_comment = ""

        # each array entry is a pipeline mapped to core stored as string
        # pipeline ranging from 1 to n, however stored in zero based array
        self.arr2_pipelines2cores = []

    def stage2_print(self):
        print('printing Context2 obj')
        print('ctx2.cores(pipelines, n_pipelines, counter, counter_max) =')
        for cores_count in range(0, constants.MAX_CORES):
            print('core[%d] = (%d,%d,%d,%d)' % (
                cores_count,
                self.cores[cores_count].pipelines,
                self.cores[cores_count].n_pipelines,
                self.cores[cores_count].counter,
                self.cores[cores_count].counter_max))

            print('ctx2.n_cores = %d' % self.n_cores, end='')
            print('ctx2.n_pipelines = %d' % self.n_pipelines, end='')
            print('ctx2.pos = %d' % self.pos)
            print('ctx2.stage0_file_comment = %s' %
                  self.self.stage0_file_comment)
            print('ctx2.stage1_file_comment = %s' %
                  self.self.stage1_file_comment)
            print('ctx2.stage2_file_comment = %s' %
                  self.self.stage2_file_comment)

    def stage2_reset(self):
        for i in range(0, constants.MAX_CORES):
            self.cores[i].pipelines = 0
            self.cores[i].n_pipelines = 0
            self.cores[i].counter = 0
            self.cores[i].counter_max = 0

            for idx in range(0, constants.MAX_PIPELINES):
                self.cores[i].bitpos[idx] = 0

        self.n_cores = 0
        self.n_pipelines = 0
        self.pos = 0
        # clear list
        del self.arr2_pipelines2cores[:]

    def bitpos_load(self, coreidx):
        i = j = 0
        while (i < self.n_pipelines):
            if ((self.cores[coreidx].pipelines) &
                    (1 << i)):
                self.cores[coreidx].bitpos[j] = i
                j += 1
            i += 1
        self.cores[coreidx].n_pipelines = j

    def bitpos_apply(self, in_buf, pos, n_pos):
        out = 0
        for i in range(0, n_pos):
            out |= (in_buf & (1 << i)) << (pos[i] - i)

        return out

    def stage2_init(self, ctx1):
        self.stage2_reset()
        self.n_cores = ctx1.n_cores
        self.n_pipelines = ctx1.n_pipelines

        self.arr2_pipelines2cores = [''] * self.n_pipelines

        core_idx = 0
        while (core_idx < self.n_cores):
            self.cores[core_idx].pipelines = ctx1.cores[core_idx].pipelines

            self.bitpos_load(core_idx)
            core_idx += 1

    def stage2_log(self):
        tmp_file_comment = ""
        if(enable_stage2_traceout == 1):
            print('STAGE2: ', end='')
            tmp_file_comment += 'STAGE2: '

            for i in range(0, self.n_cores):
                mask = len2mask(self.cores[i].n_pipelines)
                pipelines_ht0 = self.bitpos_apply(
                    (~self.cores[i].counter) & mask,
                    self.cores[i].bitpos,
                    self.cores[i].n_pipelines)

                pipelines_ht1 = self.bitpos_apply(
                    self.cores[i].counter,
                    self.cores[i].bitpos,
                    self.cores[i].n_pipelines)

                print('C%dHT0 = [' % i, end='')
                tmp_file_comment += "C{}HT0 = [".format(i)
                tmp_file_comment += bitstring_write(
                    pipelines_ht0, self.n_pipelines)

                print(']\tC%dHT1 = [' % i, end='')
                tmp_file_comment += "]\tC{}HT1 = [".format(i)
                tmp_file_comment += bitstring_write(
                    pipelines_ht1, self.n_pipelines)
                print(']\t', end='')
                tmp_file_comment += ']\t'

            print('')
            self.stage2_file_comment = tmp_file_comment

        # check if file traceing is enabled
        if(enable_stage2_fileout != 1):
            return
        # spit out the combination to file
        self.stage2_process_file()

    def stage2_updateCoresInBuf(self, nPipeline, sCore):
        rePipeline = self._fileTrace.arr_pipelines[nPipeline]
        rePipeline = rePipeline.replace("[", "\[").replace("]", "\]")
        reCore = 'core\s*=\s*((\d*)|(((s|S)\d)?(c|C)[1-9][0-9]*)).*\n'
        sSubs = 'core = ' + sCore + '\n'

        reg_pipeline = re.compile(rePipeline)
        search_match = reg_pipeline.search(self._fileTrace.in_buf)

        if(search_match):
            pos = search_match.start()
            substr1 = self._fileTrace.in_buf[:pos]
            substr2 = self._fileTrace.in_buf[pos:]
            substr2 = re.sub(reCore, sSubs, substr2, 1)
            self._fileTrace.in_buf = substr1 + substr2

    def pipelines2cores(self, n, n_bits, nCore, bHT):
        if (n_bits > 64):
            return

        i = n_bits - 1
        pipeline_idx = 0
        while (i >= 0):
            cond = (n & (1 << i))
            if (cond):
                # update the pipelines array to match the core
                # only in case of cond match
                # PIPELINE0 and core 0 are reserved
                if(bHT):
                    tmpCore = fileTrace.in_physical_cores[nCore] + 'h'
                    self.arr2_pipelines2cores[pipeline_idx] = tmpCore
                else:
                    self.arr2_pipelines2cores[pipeline_idx] = \
                        fileTrace.in_physical_cores[nCore]

            i -= 1
            pipeline_idx += 1

    def stage2_process_file(self):
        outFileName = os.path.join(self._fileTrace.out_path,
                                   self._fileTrace.prefix_outfile)
        outFileName += "_{}CoReS".format(self.n_cores)

        for i in range(0, self.n_cores):
            mask = len2mask(self.cores[i].n_pipelines)
            pipelines_ht0 = self.bitpos_apply((~self.cores[i].counter) & mask,
                                              self.cores[i].bitpos,
                                              self.cores[i].n_pipelines)

            pipelines_ht1 = self.bitpos_apply(self.cores[i].counter,
                                              self.cores[i].bitpos,
                                              self.cores[i].n_pipelines)

            # update pipelines to core mapping
            self.pipelines2cores(pipelines_ht0, self.n_pipelines, i, False)
            self.pipelines2cores(pipelines_ht1, self.n_pipelines, i, True)

        # update the in_buf as per the arr_pipelines2cores
        for pipeline_idx in range(len(self.arr2_pipelines2cores)):
            outFileName += "_{}".format(
                self.arr2_pipelines2cores[pipeline_idx])
            self.stage2_updateCoresInBuf(
                pipeline_idx, self.arr2_pipelines2cores[pipeline_idx])

        # by now the in_buf is all set to be written to file
        outFileName += self._fileTrace.suffix_outfile
        outputFile = open(outFileName, "w")

        # write the file comments
        strTruncated = ("", "(Truncated)")[self._fileTrace.ncores_truncated]
        outputFile.write(
            "; =============== Pipeline-to-Core Mapping ================\n"
            "; Generated from file {}\n"
            "; Input pipelines = {}\n"
            "; Input cores = {}\n"
            "; N_PIPELINES = {}  N_CORES = {} {} hyper_thread = {} \n"
            .format(
                self._fileTrace.in_file_namepath,
                fileTrace.arr_pipelines,
                fileTrace.in_physical_cores,
                self._fileTrace.n_pipelines,
                self._fileTrace.n_cores,
                strTruncated,
                self._fileTrace.hyper_thread))

        outputFile.write(
            "; {stg0cmt}\n"
            "; {stg1cmt}\n"
            "; {stg2cmt}\n"
            "; ========================================================\n"
            "; \n"
            .format(
                stg0cmt=self.stage0_file_comment,
                stg1cmt=self.stage1_file_comment,
                stg2cmt=self.stage2_file_comment))

        # write the buffer contents
        outputFile.write(self._fileTrace.in_buf)
        outputFile.flush()
        outputFile.close()

    def stage2_process(self):
        i = 0
        while(i < self.n_cores):
            self.cores[i].counter_max = len2mask(
                self.cores[i].n_pipelines - 1)
            i += 1

        self.pos = self.n_cores - 1
        while True:
            if (self.pos == self.n_cores - 1):
                self.stage2_log()

            if (self.cores[self.pos].counter ==
                    self.cores[self.pos].counter_max):
                if (self.pos == 0):
                    return

                self.cores[self.pos].counter = 0
                self.pos -= 1
                continue

            self.cores[self.pos].counter += 1
            if(self.pos < self.n_cores - 1):
                self.pos += 1


class FileTrace:

    def __init__(self, filenamepath):
        self.in_file_namepath = os.path.abspath(filenamepath)
        self.in_filename = os.path.basename(self.in_file_namepath)
        self.in_path = os.path.dirname(self.in_file_namepath)

        filenamesplit = self.in_filename.split('.')
        self.prefix_outfile = filenamesplit[0]
        self.suffix_outfile = ".cfg"

        # output folder:  in the same folder as input file
        # create new folder in the name of input file
        self.out_path = os.path.join(
            os.path.abspath(os.path.dirname(__file__)),
            self.prefix_outfile)

        try:
            os.makedirs(self.out_path)
        except OSError as excep:
            if excep.errno == errno.EEXIST and os.path.isdir(self.out_path):
                pass
            else:
                raise

        self.in_buf = None
        self.arr_pipelines = []  # holds the positions of search

        self.max_cores = 15
        self.max_pipelines = 15

        self.in_physical_cores = None
        self.hyper_thread = None

        # save the num of pipelines determined from input file
        self.n_pipelines = 0
        # save the num of cores input (or the truncated value)
        self.n_cores = 0
        self.ncores_truncated = False

    def print_TraceFile(self):
        print("self.in_file_namepath = ", self.in_file_namepath)
        print("self.in_filename = ", self.in_filename)
        print("self.in_path = ", self.in_path)
        print("self.out_path = ", self.out_path)
        print("self.prefix_outfile = ", self.prefix_outfile)
        print("self.suffix_outfile = ", self.suffix_outfile)
        print("self.in_buf = ", self.in_buf)
        print("self.arr_pipelines =", self.arr_pipelines)
        print("self.in_physical_cores", self.in_physical_cores)
        print("self.hyper_thread", self.hyper_thread)


def process(n_cores, n_pipelines, fileTrace):
    '''process and map pipelines, cores.'''
    if (n_cores == 0):
        sys.exit('N_CORES is 0, exiting')

    if (n_pipelines == 0):
        sys.exit('N_PIPELINES is 0, exiting')

    if (n_cores > n_pipelines):
        print('\nToo many cores, truncating N_CORES to N_PIPELINES')
        n_cores = n_pipelines
        fileTrace.ncores_truncated = True

    fileTrace.n_pipelines = n_pipelines
    fileTrace.n_cores = n_cores

    strTruncated = ("", "(Truncated)")[fileTrace.ncores_truncated]
    print("N_PIPELINES = {}, N_CORES = {} {}"
          .format(n_pipelines, n_cores, strTruncated))
    print("---------------------------------------------------------------")

    ctx0_inst = Context0()
    ctx1_inst = Context1()
    ctx2_inst = Context2()

    # initialize the class variables
    ctx1_inst._fileTrace = fileTrace
    ctx2_inst._fileTrace = fileTrace

    ctx0_inst.stage0_init(n_cores, n_pipelines, ctx1_inst, ctx2_inst)
    ctx0_inst.stage0_process()


def validate_core(core):
    match = reg_phycore.match(core)
    if(match):
        return True
    else:
        return False


def validate_phycores(phy_cores):
    '''validate physical cores, check if unique.'''
    # eat up whitespaces
    phy_cores = phy_cores.strip().split(',')

    # check if the core list is unique
    if(len(phy_cores) != len(set(phy_cores))):
        print('list of physical cores has duplicates')
        return None

    for core in phy_cores:
        if not validate_core(core):
            print('invalid physical core specified.')
            return None
    return phy_cores


def scanconfigfile(fileTrace):
    '''scan input file for pipelines, validate then process.'''
    # open file
    filetoscan = open(fileTrace.in_file_namepath, 'r')
    fileTrace.in_buf = filetoscan.read()

    # reset iterator on open file
    filetoscan.seek(0)

    # scan input file for pipelines
    # master pipelines to be ignored
    pattern_pipeline = r'\[PIPELINE\d*\]'
    pattern_mastertype = r'type\s*=\s*MASTER'

    pending_pipeline = False
    for line in filetoscan:
        match_pipeline = re.search(pattern_pipeline, line)
        match_type = re.search('type\s*=', line)
        match_mastertype = re.search(pattern_mastertype, line)

        if(match_pipeline):
            sPipeline = line[match_pipeline.start():match_pipeline.end()]
            pending_pipeline = True
        elif(match_type):
            # found a type definition...
            if(match_mastertype is None):
                # and this is not a master pipeline...
                if(pending_pipeline):
                    # add it to the list of pipelines to be mapped
                    fileTrace.arr_pipelines.append(sPipeline)
                    pending_pipeline = False
            else:
                # and this is a master pipeline...
                # ignore the current and move on to next
                sPipeline = ""
                pending_pipeline = False
    filetoscan.close()

    # validate if pipelines are unique
    if(len(fileTrace.arr_pipelines) != len(set(fileTrace.arr_pipelines))):
        sys.exit('Error: duplicate pipelines in input file')

    num_pipelines = len(fileTrace.arr_pipelines)
    num_cores = len(fileTrace.in_physical_cores)

    print("-------------------Pipeline-to-core mapping--------------------")
    print("Input pipelines = {}\nInput cores = {}"
          .format(fileTrace.arr_pipelines, fileTrace.in_physical_cores))

    # input configuration file validations goes here
    if (num_cores > fileTrace.max_cores):
        sys.exit('Error: number of cores specified > max_cores (%d)' %
                 fileTrace.max_cores)

    if (num_pipelines > fileTrace.max_pipelines):
        sys.exit('Error: number of pipelines in input \
                cfg file > max_pipelines (%d)' % fileTrace.max_pipelines)

    # call process to generate pipeline-to-core mapping, trace and log
    process(num_cores, num_pipelines, fileTrace)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='mappipelines')

    reqNamedGrp = parser.add_argument_group('required named args')
    reqNamedGrp.add_argument(
        '-i',
        '--input-file',
        type=argparse.FileType('r'),
        help='Input config file',
        required=True)

    reqNamedGrp.add_argument(
        '-pc',
        '--physical-cores',
        type=validate_phycores,
        help='''Enter available CPU cores in
                format:\"<core>,<core>,...\"
                where each core format: \"s<SOCKETID>c<COREID>\"
                where SOCKETID={0..9}, COREID={1-99}''',
        required=True)

    # add optional arguments
    parser.add_argument(
        '-ht',
        '--hyper-thread',
        help='enable/disable hyper threading. default is ON',
        default='ON',
        choices=['ON', 'OFF'])

    parser.add_argument(
        '-nO',
        '--no-output-file',
        help='''disable output config file generation.
                Output file generation is enabled by default''',
        action="store_true")

    args = parser.parse_args()

    if(args.physical_cores is None):
        parser.error("invalid physical_cores specified")

    # create object of FileTrace and initialise
    fileTrace = FileTrace(args.input_file.name)
    fileTrace.in_physical_cores = args.physical_cores
    fileTrace.hyper_thread = args.hyper_thread

    if(fileTrace.hyper_thread == 'OFF'):
        print("!!!!disabling stage2 HT!!!!")
        enable_stage2_traceout = 0
        enable_stage2_fileout = 0
    elif(fileTrace.hyper_thread == 'ON'):
        print("!!!!HT enabled. disabling stage1 file generation.!!!!")
        enable_stage1_fileout = 0

    if(args.no_output_file is True):
        print("!!!!disabling stage1 and stage2 fileout!!!!")
        enable_stage1_fileout = 0
        enable_stage2_fileout = 0

    scanconfigfile(fileTrace)
