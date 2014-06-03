#!/bin/sh

#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

# script to check that dependancies are working in the framework
# must be executed from root

# do a first build
make config T=x86_64-native-linuxapp-gcc O=deptest
make -j8 O=deptest

MOD_APP_TEST1=`stat deptest/app/test | grep Modify`
MOD_APP_TEST_MEMPOOL1=`stat deptest/build/app/test/test_mempool.o | grep Modify`
MOD_LIB_MEMPOOL1=`stat deptest/lib/librte_mempool.a | grep Modify`
MOD_LIB_MBUF1=`stat deptest/lib/librte_mbuf.a | grep Modify`

echo "----- touch mempool.c, and check that deps are updated"
sleep 1
touch lib/librte_mempool/rte_mempool.c
make -j8 O=deptest

MOD_APP_TEST2=`stat deptest/app/test | grep Modify`
MOD_APP_TEST_MEMPOOL2=`stat deptest/build/app/test/test_mempool.o | grep Modify`
MOD_LIB_MEMPOOL2=`stat deptest/lib/librte_mempool.a | grep Modify`
MOD_LIB_MBUF2=`stat deptest/lib/librte_mbuf.a | grep Modify`

if [ "${MOD_APP_TEST1}" = "${MOD_APP_TEST2}" ]; then
	echo ${MOD_APP_TEST1} / ${MOD_APP_TEST2}
	echo "Bad deps on deptest/app/test"
	exit 1
fi
if [ "${MOD_APP_TEST_MEMPOOL1}" != "${MOD_APP_TEST_MEMPOOL2}" ]; then
	echo "Bad deps on deptest/build/app/test/test_mempool.o"
	exit 1
fi
if [ "${MOD_LIB_MEMPOOL1}" = "${MOD_LIB_MEMPOOL2}" ]; then
	echo "Bad deps on deptest/lib/librte_mempool.a"
	exit 1
fi
if [ "${MOD_LIB_MBUF1}" != "${MOD_LIB_MBUF2}" ]; then
	echo "Bad deps on deptest/lib/librte_mbuf.a"
	exit 1
fi

echo "----- touch mempool.h, and check that deps are updated"
sleep 1
touch lib/librte_mempool/rte_mempool.h
make -j8 O=deptest

MOD_APP_TEST3=`stat deptest/app/test | grep Modify`
MOD_APP_TEST_MEMPOOL3=`stat deptest/build/app/test/test_mempool.o | grep Modify`
MOD_LIB_MEMPOOL3=`stat deptest/lib/librte_mempool.a | grep Modify`
MOD_LIB_MBUF3=`stat deptest/lib/librte_mbuf.a | grep Modify`

if [ "${MOD_APP_TEST2}" = "${MOD_APP_TEST3}" ]; then
	echo "Bad deps on deptest/app/test"
	exit 1
fi
if [ "${MOD_APP_TEST_MEMPOOL2}" = "${MOD_APP_TEST_MEMPOOL3}" ]; then
	echo "Bad deps on deptest/build/app/test/test_mempool.o"
	exit 1
fi
if [ "${MOD_LIB_MEMPOOL2}" = "${MOD_LIB_MEMPOOL3}" ]; then
	echo "Bad deps on deptest/lib/librte_mempool.a"
	exit 1
fi
if [ "${MOD_LIB_MBUF2}" = "${MOD_LIB_MBUF3}" ]; then
	echo "Bad deps on deptest/lib/librte_mbuf.a"
	exit 1
fi


echo "----- change mempool.c's CFLAGS, and check that deps are updated"
sleep 1
make -j8 O=deptest CFLAGS_rte_mempool.o="-DDUMMY_TEST"

MOD_APP_TEST4=`stat deptest/app/test | grep Modify`
MOD_APP_TEST_MEMPOOL4=`stat deptest/build/app/test/test_mempool.o | grep Modify`
MOD_LIB_MEMPOOL4=`stat deptest/lib/librte_mempool.a | grep Modify`
MOD_LIB_MBUF4=`stat deptest/lib/librte_mbuf.a | grep Modify`

if [ "${MOD_APP_TEST3}" = "${MOD_APP_TEST4}" ]; then
	echo "Bad deps on deptest/app/test"
	exit 1
fi
if [ "${MOD_APP_TEST_MEMPOOL3}" != "${MOD_APP_TEST_MEMPOOL4}" ]; then
	echo "Bad deps on deptest/build/app/test/test_mempool.o"
	exit 1
fi
if [ "${MOD_LIB_MEMPOOL3}" = "${MOD_LIB_MEMPOOL4}" ]; then
	echo "Bad deps on deptest/lib/librte_mempool.a"
	exit 1
fi
if [ "${MOD_LIB_MBUF3}" != "${MOD_LIB_MBUF4}" ]; then
	echo "Bad deps on deptest/lib/librte_mbuf.a"
	exit 1
fi


echo "----- Deps check ok"
rm -rf deptest
exit 0
