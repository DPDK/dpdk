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

extra_sphinx_args = ['-E', '-c', join_paths(doc_source_dir, 'guides')]

# DTS docs are generated inside main Doxygen docs build directory
# DTS docs relies on main docs to install it so does not define install dir
dts_api_html = custom_target('dts_api_html',
        depends: doxy_html_build,
        output: 'html',
        command: [sphinx_wrapper, sphinx, meson.project_version(),
            meson.current_source_dir(), join_paths(api_build_dir, 'html', 'dts'), extra_sphinx_args],
        build_by_default: get_option('enable_docs'))

doc_targets += dts_api_html
doc_target_names += 'DTS_API_HTML'
