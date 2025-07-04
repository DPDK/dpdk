# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>
# Copyright(c) 2021 IBM Corporation

if not dpdk_conf.get('RTE_ARCH_64')
    error('Only 64-bit compiles are supported for this platform type')
endif
dpdk_conf.set('RTE_ARCH', 'ppc_64')
dpdk_conf.set('RTE_ARCH_PPC_64', 1)

# RHEL 7.x uses gcc 4.8.X which doesn't generate code for Power 9 CPUs,
# though it will detect a Power 9 CPU when the "-mcpu=native" argument
# is used, resulting in a build failure.
power9_supported = cc.has_argument('-mcpu=power9')
if not power9_supported
    cpu_instruction_set = 'power8'
    machine_args = ['-mcpu=power8', '-mtune=power8']
    dpdk_conf.set('RTE_MACHINE','power8')
endif

# Suppress the gcc warning "note: the layout of aggregates containing
# vectors with 4-byte alignment has changed in GCC 5".
if (cc.get_id() == 'gcc' and cc.version().version_compare('>=10.0') and
        cc.version().version_compare('<12.0') and cc.has_argument('-Wno-psabi'))
    add_project_arguments('-Wno-psabi', language: 'c')
endif
# If using a generic build, select the lowest common denominator
if platform == 'generic'
     cpu_instruction_set = 'power8'
# If using a native build, select the highest cpu_instruction_set supported
# by the build host. (Native is the default platform type set in meson_options.txt.)
# When a cross compile is detected, verify that the compiler/assembler supports
# the requested cpu_instruction_set in the cross-file.
elif platform == 'native'
    if meson.is_cross_build()
        # cpu_instruction_set specified in cross-compile file as "cpu"
        if host_machine.cpu() == 'power10'
            test_p10 = '''
#include <stdio.h>
#include <altivec.h>
int main() {
  __vector unsigned __int128 t, a, b;
#ifdef _ARCH_PWR10
  __asm__("vcmpequq %0,%1,%2;" : "=v" (t) : "v" (a), "v" (b) : );
#else
#error POWER10 not supported
#endif
  printf("%d", (int)t[0]);
  return 0;
}
'''
            if not cc.compiles(test_p10, args : '-mcpu=power10', name: 'Detect P10')
                  error('POWER10 requested but not supported')
            endif
        endif
    else
        # Using a POWER native build host. Assume P8 support, move to later CPUs
        # if supported by the build host and the compiler. Detect the CPU used by
        # the build host
        detect_cpu = '''
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>
int main() {
  char * platform = (char*) getauxval(AT_PLATFORM);
  if (platform) {
    if (!strncmp(platform,"power",5)) {
      printf("%s", platform+5); return 0;
    }
  }
  return 1;
}'''
        result = cc.run(detect_cpu, name : 'Detect host CPU type')
        if not result.compiled()
            error('Failed to detect CPU type')
        endif
        cpu = result.stdout().to_int()
        if cpu < 8
            error('Unsupported POWER CPU detected')
        else
            # Found supported CPU on build host, verify compiler support
            mcpu_flag = '-mcpu=power@0@'
            if cc.has_argument(mcpu_flag.format(cpu))
                # Compiler supports current detected CPU
            elif cc.has_argument(mcpu_flag.format(cpu-1))
                cpu = cpu-1
            elif cc.has_argument(mcpu_flag.format(cpu-2))
                cpu = cpu-2
            else
                error('Compiler does not support POWER@0@ platform'.format(cpu))
            endif
            if cpu >= 8
                cpu_instruction_set = 'power'+cpu.to_string()
            else
                error('Compiler does not support POWER@0@ platform'.format(cpu))
            endif
        endif
    endif  #end if not cross-build
else
    # User has explicitly chosen a platform for the build, use that
    if cc.has_argument('-mcpu=' + platform)
        cpu_instruction_set = platform
    else
        error('User selected platform ' + platform + ' not supported by compiler')
    endif
endif
machine_args = ['-mcpu=' + cpu_instruction_set, '-mtune=' + cpu_instruction_set]

# GCC versions < 11.4 may fail to build for power10, see https://bugs.dpdk.org/show_bug.cgi?id=1251.
# Explicitly specify a default used in GCC 11.4 and later to workaround the issue.
if (cpu_instruction_set == 'power10' and cc.get_id() == 'gcc' and
        cc.version().version_compare('<=11.4'))
    machine_args += '-mno-block-ops-vector-pair'
endif

dpdk_conf.set('RTE_MACHINE', cpu_instruction_set)


# Certain POWER9 systems can scale as high as 1536 LCORES, but setting such a
# high value can waste memory, cause timeouts in time limited autotests, and is
# unlikely to be used in many production situations.  Similarly, keeping the
# default 64 LCORES seems too small as most POWER9 dual socket systems will have
# at least 128 LCORES available.  Set RTE_MAX_LCORE to 128 for POWER systems as
# a compromise.
dpdk_conf.set('RTE_MAX_LCORE', 128)

# POWER systems do not allocate NUMA nodes sequentially.  A dual socket system
# will have CPUs associated with NUMA nodes 0 & 8, so ensure that the second
# NUMA node will be supported by setting RTE_MAX_NUMA_NODES to 16. High end
# systems can scale even higher with as many as 32 NUMA nodes.
dpdk_conf.set('RTE_MAX_NUMA_NODES', 16)

dpdk_conf.set('RTE_CACHE_LINE_SIZE', 128)
