#!/usr/bin/python
#/*
# *   BSD LICENSE
# *
# *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
# *   All rights reserved.
# *
# *   Redistribution and use in source and binary forms, with or without
# *   modification, are permitted provided that the following conditions
# *   are met:
# *
# *     * Redistributions of source code must retain the above copyright
# *       notice, this list of conditions and the following disclaimer.
# *     * Redistributions in binary form must reproduce the above copyright
# *       notice, this list of conditions and the following disclaimer in
# *       the documentation and/or other materials provided with the
# *       distribution.
# *     * Neither the name of Intel Corporation nor the names of its
# *       contributors may be used to endorse or promote products derived
# *       from this software without specific prior written permission.
# *
# *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# */

#####################################################################
# This script is designed to modify the call to the QEMU emulator
# to support userspace vhost when starting a guest machine through
# libvirt with vhost enabled. The steps to enable this are as follows
# and should be run as root:
#
# 1. Place this script in a libvirtd's binary search PATH ($PATH)
#    A good location would be in the same directory that the QEMU
#    binary is located
#
# 2. Ensure that the script has the same owner/group and file
#    permissions as the QEMU binary
#
# 3. Update the VM xml file using "virsh edit VM.xml"
#
#    3.a) Set the VM to use the launch script
#
#	Set the emulator path contained in the
#		<emulator><emulator/> tags
#
#	e.g replace <emulator>/usr/bin/qemu-kvm<emulator/>
#        with    <emulator>/usr/bin/qemu-wrap.py<emulator/>
#
#	 3.b) Set the VM's device's to use vhost-net offload
#
#		<interface type="network">
#	<model type="virtio"/>
#	<driver name="vhost"/>
#		<interface/>
#
# 4. Enable libvirt to access our userpace device file by adding it to
#    controllers cgroup for libvirtd using the following steps
#
#   4.a) In /etc/libvirt/qemu.conf add/edit the following lines:
#         1) cgroup_controllers = [ ... "devices", ... ]
#		  2) clear_emulator_capabilities = 0
#         3) user = "root"
#         4) group = "root"
#         5) cgroup_device_acl = [
#                "/dev/null", "/dev/full", "/dev/zero",
#                "/dev/random", "/dev/urandom",
#                "/dev/ptmx", "/dev/kvm", "/dev/kqemu",
#                "/dev/rtc", "/dev/hpet", "/dev/net/tun",
#                "/dev/<devbase-name>",
#                "/dev/hugepages",
#            ]
#
#   4.b) Disable SELinux or set to permissive mode
#
#   4.c) Mount cgroup device controller
#        "mkdir /dev/cgroup"
#        "mount -t cgroup none /dev/cgroup -o devices"
#
#   4.d) Set hugetlbfs_mount variable - ( Optional )
#        VMs using userspace vhost must use hugepage backed
#        memory. This can be enabled in the libvirt XML
#        config by adding a memory backing section to the
#        XML config e.g.
#             <memoryBacking>
#             <hugepages/>
#             </memoryBacking>
#        This memory backing section should be added after the
#        <memory> and <currentMemory> sections. This will add
#        flags "-mem-prealloc -mem-path <path>" to the QEMU
#        command line. The hugetlbfs_mount variable can be used
#        to override the default <path> passed through by libvirt.
#
#        if "-mem-prealloc" or "-mem-path <path>" are not passed
#        through and a vhost device is detected then these options will
#        be automatically added by this script. This script will detect
#        the system hugetlbfs mount point to be used for <path>. The
#        default <path> for this script can be overidden by the
#        hugetlbfs_dir variable in the configuration section of this script.
#
#
#   4.e) Restart the libvirtd system process
#        e.g. on Fedora "systemctl restart libvirtd.service"
#
#
#   4.f) Edit the Configuration Parameters section of this script
#        to point to the correct emulator location and set any
#        addition options
#
# The script modifies the libvirtd Qemu call by modifying/adding
# options based on the configuration parameters below.
# NOTE:
#     emul_path and us_vhost_path must be set
#     All other parameters are optional
#####################################################################


#############################################
# Configuration Parameters
#############################################
#Path to QEMU binary
emul_path = "/usr/local/bin/qemu-system-x86_64"

#Path to userspace vhost device file
# This filename should match the --dev-basename parameters of
# the command used to launch the userspace vhost sample application e.g.
# if the sample app lauch command is:
#    ./build/vhost-switch ..... --dev-basename usvhost
# then this variable should be set to:
#   us_vhost_path = "/dev/usvhost"
us_vhost_path = "/dev/usvhost"

#List of additional user defined emulation options. These options will
#be added to all Qemu calls
emul_opts_user = []

#List of additional user defined emulation options for vhost only.
#These options will only be added to vhost enabled guests
emul_opts_user_vhost = []

#For all VHOST enabled VMs, the VM memory is preallocated from hugetlbfs
# Set this variable to one to enable this option for all VMs
use_huge_all = 0

#Instead of autodetecting, override the hugetlbfs directory by setting
#this variable
hugetlbfs_dir = ""

#############################################


#############################################
# ****** Do Not Modify Below this Line ******
#############################################

import sys, os, subprocess
import time
import signal


#List of open userspace vhost file descriptors
fd_list = []

#additional virtio device flags when using userspace vhost
vhost_flags = [ "csum=off",
                "gso=off",
                "guest_tso4=off",
                "guest_tso6=off",
                "guest_ecn=off"
              ]

#String of the path to the Qemu process pid
qemu_pid = "/tmp/%d-qemu.pid" % os.getpid()

#############################################
# Signal haldler to kill Qemu subprocess
#############################################
def kill_qemu_process(signum, stack):
    pidfile = open(qemu_pid, 'r')
    pid = int(pidfile.read())
    os.killpg(pid, signal.SIGTERM)
    pidfile.close()


#############################################
# Find the system hugefile mount point.
# Note:
# if multiple hugetlbfs mount points exist
# then the first one found will be used
#############################################
def find_huge_mount():

    if (len(hugetlbfs_dir)):
        return hugetlbfs_dir

    huge_mount = ""

    if (os.access("/proc/mounts", os.F_OK)):
        f = open("/proc/mounts", "r")
        line = f.readline()
        while line:
            line_split = line.split(" ")
            if line_split[2] == 'hugetlbfs':
                huge_mount = line_split[1]
                break
            line = f.readline()
    else:
        print "/proc/mounts not found"
        exit (1)

    f.close
    if len(huge_mount) == 0:
        print "Failed to find hugetlbfs mount point"
        exit (1)

    return huge_mount


#############################################
# Get a userspace Vhost file descriptor
#############################################
def get_vhost_fd():

    if (os.access(us_vhost_path, os.F_OK)):
        fd = os.open( us_vhost_path, os.O_RDWR)
    else:
        print ("US-Vhost file %s not found" %us_vhost_path)
        exit (1)

    return fd


#############################################
# Check for vhostfd. if found then replace
# with our own vhost fd and append any vhost
# flags onto the end
#############################################
def modify_netdev_arg(arg):

    global fd_list
    vhost_in_use = 0
    s = ''
    new_opts = []
    netdev_opts = arg.split(",")

    for opt in netdev_opts:
        #check if vhost is used
        if "vhost" == opt[:5]:
            vhost_in_use = 1
        else:
            new_opts.append(opt)

    #if using vhost append vhost options
    if vhost_in_use == 1:
        #append vhost on option
        new_opts.append('vhost=on')
        #append vhostfd ption
        new_fd = get_vhost_fd()
        new_opts.append('vhostfd=' + str(new_fd))
        fd_list.append(new_fd)

    #concatenate all options
    for opt in new_opts:
        if len(s) > 0:
			s+=','

        s+=opt

    return s


#############################################
# Main
#############################################
def main():

    global fd_list
    global vhost_in_use
    new_args = []
    num_cmd_args = len(sys.argv)
    emul_call = ''
    mem_prealloc_set = 0
    mem_path_set = 0
    num = 0;

    #parse the parameters
    while (num < num_cmd_args):
        arg = sys.argv[num]

	#Check netdev +1 parameter for vhostfd
        if arg == '-netdev':
            num_vhost_devs = len(fd_list)
            new_args.append(arg)

            num+=1
            arg = sys.argv[num]
            mod_arg = modify_netdev_arg(arg)
            new_args.append(mod_arg)

            #append vhost flags if this is a vhost device
            # and -device is the next arg
            # i.e -device -opt1,-opt2,...,-opt3,%vhost
            if (num_vhost_devs < len(fd_list)):
                num+=1
                arg = sys.argv[num]
                if arg == '-device':
                    new_args.append(arg)
                    num+=1
                    new_arg = sys.argv[num]
                    for flag in vhost_flags:
                        new_arg = ''.join([new_arg,',',flag])
                    new_args.append(new_arg)
                else:
                    new_args.append(arg)
        elif arg == '-mem-prealloc':
            mem_prealloc_set = 1
            new_args.append(arg)
        elif arg == '-mem-path':
            mem_path_set = 1
            new_args.append(arg)

        else:
            new_args.append(arg)

        num+=1

    #Set Qemu binary location
    emul_call+=emul_path
    emul_call+=" "

    #Add prealloc mem options if using vhost and not already added
    if ((len(fd_list) > 0) and (mem_prealloc_set == 0)):
        emul_call += "-mem-prealloc "

    #Add mempath mem options if using vhost and not already added
    if ((len(fd_list) > 0) and (mem_path_set == 0)):
        #Detect and add hugetlbfs mount point
        mp = find_huge_mount()
        mp = "".join(["-mem-path ", mp])
        emul_call += mp
        emul_call += " "

    #add user options
    for opt in emul_opts_user:
        emul_call += opt
        emul_call += " "

    #Add add user vhost only options
    if len(fd_list) > 0:
        for opt in emul_opts_user_vhost:
            emul_call += opt
            emul_call += " "

    #Add updated libvirt options
    iter_args = iter(new_args)
    #skip 1st arg i.e. call to this script
    next(iter_args)
    for arg in iter_args:
        emul_call+=str(arg)
        emul_call+= " "

    emul_call += "-pidfile %s " % qemu_pid
    #Call QEMU
    process = subprocess.Popen(emul_call, shell=True, preexec_fn=os.setsid)

    for sig in [signal.SIGTERM, signal.SIGINT, signal.SIGHUP, signal.SIGQUIT]:
        signal.signal(sig, kill_qemu_process)

    process.wait()

    #Close usvhost files
    for fd in fd_list:
        os.close(fd)
    #Cleanup temporary files
    if os.access(qemu_pid, os.F_OK):
        os.remove(qemu_pid)

if __name__ == "__main__":
    main()
