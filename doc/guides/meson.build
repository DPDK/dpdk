# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sphinx = find_program('sphinx-build', required: get_option('enable_docs'))

if not sphinx.found()
	subdir_done()
endif

htmldir = join_paths(get_option('datadir'), 'doc', 'dpdk')
html_guides = custom_target('html_guides',
	input: files('index.rst'),
	output: 'html',
	command: [sphinx_wrapper, sphinx, meson.current_source_dir(), meson.current_build_dir()],
	depfile: '.html.d',
	build_by_default: get_option('enable_docs'),
	install: get_option('enable_docs'),
	install_dir: htmldir)

install_data(files('custom.css'),
			install_dir: join_paths(htmldir,'_static', 'css'))

doc_targets += html_guides
doc_target_names += 'HTML_Guides'
