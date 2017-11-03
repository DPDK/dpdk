#   BSD LICENSE
#
#   Copyright(c) 2017 Intel Corporation.
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


# process all libraries equally, as far as possible
# "core" libs first, then others alphebetically as far as possible
# NOTE: for speed of meson runs, the dependencies in the subdirectories
# sometimes skip deps that would be implied by others, e.g. if mempool is
# given as a dep, no need to mention ring. This is especially true for the
# core libs which are widely reused, so their deps are kept to a minimum.
libraries = [ 'compat', # just a header, used for versioning
	'eal', 'ring', 'mempool', 'mbuf', 'net', 'ether', 'pci', # core
	'metrics', # bitrate/latency stats depends on this
	'hash',    # efd depends on this
	'kvargs',  # cryptodev depends on this
	'acl', 'bbdev', 'bitratestats', 'cfgfile',
	'cmdline', 'cryptodev',
	'distributor', 'efd', 'eventdev',
	'gro', 'gso', 'ip_frag', 'jobstats',
	'kni', 'latencystats', 'lpm', 'member',
	'meter', 'power', 'pdump',
	'reorder', 'sched', 'security', 'timer', 'vhost',
	# add pkt framework libs which use other libs from above
	'port', 'table', 'pipeline',
	# flow_classify lib depends on pkt framework table lib
	'flow_classify']

foreach l:libraries
	build = true
	name = l
	version = 1
	allow_experimental_apis = false
	sources = []
	headers = []
	includes = []
	cflags = []
	objs = [] # other object files to link against, used e.g. for
	          # instruction-set optimized versions of code

	# use "deps" for internal DPDK dependencies, and "ext_deps" for
	# external package/library requirements
	ext_deps = []
	deps = ['eal']   # eal is standard dependency except for itself
	if l == 'eal'
		deps = []
	endif

	dir_name = 'librte_' + l
	subdir(dir_name)

	if build
		dpdk_conf.set('RTE_LIBRTE_' + name.to_upper(), 1)
		install_headers(headers)

		libname = 'rte_' + name
		includes += include_directories(dir_name)

		if sources.length() == 0
			# if no C files, just set a dependency on header path
			dep = declare_dependency(include_directories: includes)
		else
			dep_objs = ext_deps
			foreach d:deps
				dep_objs += [get_variable('dep_rte_' + d)]
			endforeach

			if allow_experimental_apis
				cflags += '-DALLOW_EXPERIMENTAL_API'
			endif

			if get_option('per_library_versions')
				lib_version = '@0@.1'.format(version)
				so_version = '@0@'.format(version)
			else
				prj_ver = meson.project_version().split('.')
				lib_version = '@0@.@1@'.format(
						prj_ver.get(0), prj_ver.get(1))
				so_version = lib_version
			endif

			version_map = '@0@/@1@/rte_@2@_version.map'.format(
					meson.current_source_dir(), dir_name, name)
			lib = library(libname,
					sources,
					objects: objs,
					c_args: cflags,
					dependencies: dep_objs,
					include_directories: includes,
					link_args: '-Wl,--version-script=' + version_map,
					link_depends: version_map,
					version: lib_version,
					soversion: so_version,
					install: true)
			dep = declare_dependency(link_with: lib,
					include_directories: includes,
					dependencies: dep_objs)

			dpdk_libraries = [lib] + dpdk_libraries
		endif # sources.length() > 0

		set_variable('dep_' + libname, dep)
	endif # if build
endforeach
