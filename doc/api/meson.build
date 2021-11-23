# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Luca Boccassi <bluca@debian.org>

doxygen = find_program('doxygen', required: get_option('enable_docs'))

if not doxygen.found()
  subdir_done()
endif

# due to the CSS customisation script, which needs to run on a file that
# is in a subdirectory that is created at build time and thus it cannot
# be an individual custom_target, we need to wrap the doxygen call in a
# script to run the CSS modification afterwards
generate_doxygen = find_program('generate_doxygen.sh')
generate_examples = find_program('generate_examples.sh')
generate_css = find_program('doxy-html-custom.sh')

htmldir = join_paths(get_option('datadir'), 'doc', 'dpdk')

# due to the following bug: https://github.com/mesonbuild/meson/issues/4107
# if install is set to true it will override build_by_default and it will
# cause the target to always be built. If install were to be always set to
# false it would be impossible to install the docs.
# So use a configure option for now.
example = custom_target('examples.dox',
	output: 'examples.dox',
	command: [generate_examples, join_paths(meson.source_root(), 'examples'), '@OUTPUT@'],
	depfile: 'examples.dox.d',
	install: get_option('enable_docs'),
	install_dir: htmldir,
	build_by_default: get_option('enable_docs'))

cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
cdata.set('API_EXAMPLES', join_paths(meson.build_root(), 'doc', 'api', 'examples.dox'))
cdata.set('OUTPUT', join_paths(meson.build_root(), 'doc', 'api'))
cdata.set('HTML_OUTPUT', 'html')
cdata.set('TOPDIR', meson.source_root())
cdata.set('STRIP_FROM_PATH', ' '.join([meson.source_root(), join_paths(meson.build_root(), 'doc', 'api')]))
cdata.set('WARN_AS_ERROR', 'NO')
if get_option('werror')
	cdata.set('WARN_AS_ERROR', 'YES')
endif

doxy_conf = configure_file(input: 'doxy-api.conf.in',
	output: 'doxy-api.conf',
	configuration: cdata)

doxy_build = custom_target('doxygen',
	depends: example,
	depend_files: 'doxy-api-index.md',
	input: doxy_conf,
	output: 'html',
	depfile: 'html.d',
	command: [generate_doxygen, '@INPUT@', '@OUTPUT@', generate_css],
	install: get_option('enable_docs'),
	install_dir: htmldir,
	build_by_default: get_option('enable_docs'))

doc_targets += doxy_build
doc_target_names += 'Doxygen_API'
