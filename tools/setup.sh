#! /bin/bash

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
# Run with "source /path/to/setup.sh"
#

#
# Change to DPDK directory ( <this-script's-dir>/.. ), and export it as RTE_SDK
#
cd $(dirname ${BASH_SOURCE[0]})/..
export RTE_SDK=$PWD
echo "------------------------------------------------------------------------------"
echo " RTE_SDK exported as $RTE_SDK"
echo "------------------------------------------------------------------------------"

#
# Application EAL parameters for setting memory options (amount/channels/ranks).
#
EAL_PARAMS='-n 4'

#
# Sets QUIT variable so script will finish.
#
quit()
{
	QUIT=$1
}

#
# Sets up envronment variables for ICC.
#
setup_icc()
{
	DEFAULT_PATH=/opt/intel/bin/iccvars.sh
	param=$1
	shpath=`which iccvars.sh 2> /dev/null`
	if [ $? -eq 0 ] ; then
		echo "Loading iccvars.sh from $shpath for $param"
		source $shpath $param
	elif [ -f $DEFAULT_PATH ] ; then
		echo "Loading iccvars.sh from $DEFAULT_PATH for $param"
		source $DEFAULT_PATH $param
	else
		echo "## ERROR: cannot find 'iccvars.sh' script to set up ICC."
		echo "##     To fix, please add the directory that contains"
		echo "##     iccvars.sh  to your 'PATH' environment variable."
		quit
	fi
}

#
# Sets RTE_TARGET and does a "make install".
#
setup_target()
{
	option=$1
	export RTE_TARGET=${TARGETS[option]}

	compiler=${RTE_TARGET##*-}
	if [ "$compiler" == "icc" ] ; then
		platform=${RTE_TARGET%%-*}
		if [ "$platform" == "x86_64" ] ; then
			setup_icc intel64
		else
			setup_icc ia32
		fi
	fi
	if [ "$QUIT" == "0" ] ; then
		make install T=${RTE_TARGET}
	fi
	echo "------------------------------------------------------------------------------"
	echo " RTE_TARGET exported as $RTE_TARGET"
	echo "------------------------------------------------------------------------------"
}

#
# Uninstall all targets.
#
uninstall_targets()
{
	make uninstall
}

#
# Creates hugepage filesystem.
#
create_mnt_huge()
{
	echo "Creating /mnt/huge and mounting as hugetlbfs"
	sudo mkdir -p /mnt/huge

	grep -s '/mnt/huge' /proc/mounts > /dev/null
	if [ $? -ne 0 ] ; then
		sudo mount -t hugetlbfs nodev /mnt/huge
	fi
}

#
# Removes hugepage filesystem.
#
remove_mnt_huge()
{
	echo "Unmounting /mnt/huge and removing directory"
	grep -s '/mnt/huge' /proc/mounts > /dev/null
	if [ $? -eq 0 ] ; then
		sudo umount /mnt/huge
	fi

	if [ -d /mnt/huge ] ; then
		sudo rm -R /mnt/huge
	fi
}

#
# Unloads igb_uio.ko.
#
remove_igb_uio_module()
{
	echo "Unloading any existing DPDK UIO module"
	/sbin/lsmod | grep -s igb_uio > /dev/null
	if [ $? -eq 0 ] ; then
		sudo /sbin/rmmod igb_uio
	fi
}

#
# Loads new igb_uio.ko (and uio module if needed).
#
load_igb_uio_module()
{
	if [ ! -f $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko ];then
		echo "## ERROR: Target does not have the DPDK UIO Kernel Module."
		echo "       To fix, please try to rebuild target."
		return
	fi

	remove_igb_uio_module

	/sbin/lsmod | grep -s uio > /dev/null
	if [ $? -ne 0 ] ; then
		if [ -f /lib/modules/$(uname -r)/kernel/drivers/uio/uio.ko ] ; then
			echo "Loading uio module"
			sudo /sbin/modprobe uio
		fi
	fi

	# UIO may be compiled into kernel, so it may not be an error if it can't
	# be loaded.

	echo "Loading DPDK UIO module"
	sudo /sbin/insmod $RTE_SDK/$RTE_TARGET/kmod/igb_uio.ko
	if [ $? -ne 0 ] ; then
		echo "## ERROR: Could not load kmod/igb_uio.ko."
		quit
	fi
}

#
# Removes all reserved hugepages.
#
clear_huge_pages()
{
	echo > .echo_tmp
	for d in /sys/devices/system/node/node? ; do
		echo "echo 0 > $d/hugepages/hugepages-2048kB/nr_hugepages" >> .echo_tmp
	done
	echo "Removing currently reserved hugepages"
	sudo sh .echo_tmp
	rm -f .echo_tmp

	remove_mnt_huge
}

#
# Creates hugepages.
#
set_non_numa_pages()
{
	clear_huge_pages

	echo ""
	echo "  Input the number of 2MB pages"
	echo "  Example: to have 128MB of hugepages available, enter '64' to"
	echo "  reserve 64 * 2MB pages"
	echo -n "Number of pages: "
	read Pages

	echo "echo $Pages > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages" > .echo_tmp

	echo "Reserving hugepages"
	sudo sh .echo_tmp
	rm -f .echo_tmp

	create_mnt_huge
}

#
# Creates hugepages on specific NUMA nodes.
#
set_numa_pages()
{
	clear_huge_pages

	echo ""
	echo "  Input the number of 2MB pages for each node"
	echo "  Example: to have 128MB of hugepages available per node,"
	echo "  enter '64' to reserve 64 * 2MB pages on each node"

	echo > .echo_tmp
	for d in /sys/devices/system/node/node? ; do
		node=$(basename $d)
		echo -n "Number of pages for $node: "
		read Pages
		echo "echo $Pages > $d/hugepages/hugepages-2048kB/nr_hugepages" >> .echo_tmp
	done
	echo "Reserving hugepages"
	sudo sh .echo_tmp
	rm -f .echo_tmp

	create_mnt_huge
}

#
# Run unit test application.
#
run_test_app()
{
	echo ""
	echo "  Enter hex bitmask of cores to execute test app on"
	echo "  Example: to execute app on cores 0 to 7, enter 0xff"
	echo -n "bitmask: "
	read Bitmask
	echo "Launching app"
	sudo ${RTE_TARGET}/app/test -c $Bitmask $EAL_PARAMS
}

#
# Run unit testpmd application.
#
run_testpmd_app()
{
	echo ""
	echo "  Enter hex bitmask of cores to execute testpmd app on"
	echo "  Example: to execute app on cores 0 to 7, enter 0xff"
	echo -n "bitmask: "
	read Bitmask
	echo "Launching app"
	sudo ${RTE_TARGET}/app/testpmd -c $Bitmask $EAL_PARAMS -- -i
}

#
# Print hugepage information.
#
grep_meminfo()
{
	grep -i huge /proc/meminfo
}

#
# List all hugepage file references
#
ls_mnt_huge()
{
	ls -lh /mnt/huge
}

#
# Options for building a target. Note that this step MUST be first as it sets
# up TARGETS[] starting from 1, and this is accessed in setup_target using the
# user entered option.
#
step1_func()
{
	TITLE="Select the DPDK environment to build"
	CONFIG_NUM=1
	for cfg in config/defconfig_* ; do
		cfg=${cfg/config\/defconfig_/}
		TEXT[$CONFIG_NUM]="$cfg"
		TARGETS[$CONFIG_NUM]=$cfg
		FUNC[$CONFIG_NUM]="setup_target"
		let "CONFIG_NUM+=1"
	done
}

#
# Options for setting up environment.
#
step2_func()
{
	TITLE="Setup linuxapp environment"

	TEXT[1]="Insert IGB UIO module"
	FUNC[1]="load_igb_uio_module"

	TEXT[2]="Setup hugepage mappings for non-NUMA systems"
	FUNC[2]="set_non_numa_pages"

	TEXT[3]="Setup hugepage mappings for NUMA systems"
	FUNC[3]="set_numa_pages"
}

#
# Options for running applications.
#
step3_func()
{
	TITLE="Run test application for linuxapp environment"

	TEXT[1]="Run test application (\$RTE_TARGET/app/test)"
	FUNC[1]="run_test_app"

	TEXT[2]="Run testpmd application in interactive mode (\$RTE_TARGET/app/testpmd)"
	FUNC[2]="run_testpmd_app"
}

#
# Other options
#
step4_func()
{
	TITLE="Other tools"

	TEXT[1]="List hugepage info from /proc/meminfo"
	FUNC[1]="grep_meminfo"

	TEXT[2]="List hugepage files in /mnt/huge"
	FUNC[2]="ls_mnt_huge"
}

#
# Options for cleaning up the system
#
step5_func()
{
	TITLE="Uninstall and system cleanup"

	TEXT[1]="Uninstall all targets"
	FUNC[1]="uninstall_targets"

	TEXT[2]="Remove IGB UIO module"
	FUNC[2]="remove_igb_uio_module"

	TEXT[3]="Remove hugepage mappings"
	FUNC[3]="clear_huge_pages"
}

STEPS[1]="step1_func"
STEPS[2]="step2_func"
STEPS[3]="step3_func"
STEPS[4]="step4_func"
STEPS[5]="step5_func"

QUIT=0

while [ "$QUIT" == "0" ]; do
	OPTION_NUM=1

	for s in $(seq ${#STEPS[@]}) ; do
		${STEPS[s]}

		echo "----------------------------------------------------------"
		echo " Step $s: ${TITLE}"
		echo "----------------------------------------------------------"

		for i in $(seq ${#TEXT[@]}) ; do
			echo "[$OPTION_NUM] ${TEXT[i]}"
			OPTIONS[$OPTION_NUM]=${FUNC[i]}
			let "OPTION_NUM+=1"
		done

		# Clear TEXT and FUNC arrays before next step
		unset TEXT
		unset FUNC

		echo ""
	done

	echo "[$OPTION_NUM] Exit Script"
	OPTIONS[$OPTION_NUM]="quit"
	echo ""
	echo -n "Option: "
	read our_entry
	echo ""
	${OPTIONS[our_entry]} ${our_entry}
	echo
	echo -n "Press enter to continue ..."; read
done
