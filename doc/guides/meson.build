# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

sphinx = find_program('sphinx-build', required: get_option('enable_docs'))

if sphinx.found()
	htmldir = join_paths('share', 'doc', 'dpdk')
	html_guides_build = custom_target('html_guides_build',
		input: meson.current_source_dir(),
		output: 'guides',
		command: [sphinx, '-b', 'html',
			'-d', meson.current_build_dir() + '/.doctrees',
			'@INPUT@', meson.current_build_dir() + '/guides'],
		build_by_default: get_option('enable_docs'),
		install: get_option('enable_docs'),
		install_dir: htmldir)

	doc_targets += html_guides_build
	doc_target_names += 'HTML_Guides'

	# sphinx leaves a .buildinfo in the target directory, which we don't
	# want to install. Note that sh -c has to be used, otherwise the
	# env var does not get expanded if calling rm/install directly.
	meson.add_install_script('sh', '-c',
		'rm -f $MESON_INSTALL_DESTDIR_PREFIX/share/doc/dpdk/guides/.buildinfo')
	meson.add_install_script('sh', '-c',
		'install -D -m0644 $MESON_SOURCE_ROOT/doc/guides/custom.css $MESON_INSTALL_DESTDIR_PREFIX/share/doc/dpdk/guides/_static/css/custom.css')
endif
