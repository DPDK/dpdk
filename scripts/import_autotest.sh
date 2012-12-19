#!/bin/sh

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

#
# import autotests in documentation
# called by rte.sdktestall.mk from RTE_SDK root directory
# arguments are the list of targets
#

echo "This will overwrite current autotest results in doc/rst/test_report"
echo "and in doc/images/ directories"
echo -n "Are you sure ? [y/N] >"
read ans
if [ "$ans" != "y" -a "$ans" != "Y" ]; then
	echo "Aborted"
	exit 0
fi

rm doc/images/autotests/Makefile

for t in $*; do
	echo -------- $t
	rm -rf doc/rst/test_report/autotests/$t

	# no autotest dir, skip
	if ! ls -d $t/autotest-*/*.rst 2> /dev/null > /dev/null; then
		continue;
	fi

	for f in $t/autotest*/*.rst; do
		if [ ! -f $f ]; then
			continue
		fi
		mkdir -p doc/rst/test_report/autotests/$t
		cp $f doc/rst/test_report/autotests/$t
	done
	rm -rf doc/images/autotests/$t
	for f in $t/autotest*/*.svg; do
		if [ ! -f $f ]; then
			continue
		fi
		mkdir -p doc/images/autotests/$t
		cp $f doc/images/autotests/$t
		echo "SVG += `basename $f`" >> doc/images/autotests/$t/Makefile
	done

	if [ -f doc/images/autotests/$t/Makefile ]; then
		echo >> doc/images/autotests/$t/Makefile
		echo 'include $(RTE_SDK)/mk/rte.doc.mk' >> doc/images/autotests/$t/Makefile
	fi

	echo "DIRS += $t" >> doc/images/autotests/Makefile
done

echo 'include $(RTE_SDK)/mk/rte.doc.mk' >> doc/images/autotests/Makefile
