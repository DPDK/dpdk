# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.

sphinx = find_program('sphinx-build', required: get_option('enable_docs'))
if not sphinx.found()
    subdir_done()
endif

python_ver_satisfied = run_command(check_dts_requirements, check: false).returncode()
if python_ver_satisfied != 0
    subdir_done()
endif

cdata.set('DTS_API_MAIN_PAGE', join_paths('..', 'dts', 'html', 'index.html'))

extra_sphinx_args = ['-E', '-c', join_paths(doc_source_dir, 'guides')]
if get_option('werror')
    extra_sphinx_args += '-W'
endif

htmldir = join_paths(get_option('datadir'), 'doc', 'dpdk', 'dts')
dts_api_html = custom_target('dts_api_html',
        output: 'html',
        command: [sphinx_wrapper, sphinx, meson.project_version(),
            meson.current_source_dir(), meson.current_build_dir(), extra_sphinx_args],
        build_by_default: get_option('enable_docs'),
        install: get_option('enable_docs'),
        install_dir: htmldir)

doc_targets += dts_api_html
doc_target_names += 'DTS_API_HTML'
