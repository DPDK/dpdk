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
generate_doxygen = py3 + files('generate_doxygen.py')
generate_examples = py3 + files('generate_examples.py')

htmldir = join_paths(get_option('datadir'), 'doc', 'dpdk')

# due to the following bug: https://github.com/mesonbuild/meson/issues/4107
# if install is set to true it will override build_by_default and it will
# cause the target to always be built. If install were to be always set to
# false it would be impossible to install the docs.
# So use a configure option for now.
example = custom_target('examples.dox',
        output: 'examples.dox',
        command: [generate_examples, join_paths(dpdk_source_root, 'examples'), '@OUTPUT@'],
        depfile: 'examples.dox.d',
        install: get_option('enable_docs'),
        install_dir: htmldir,
        build_by_default: get_option('enable_docs'))

# set up common Doxygen configuration
cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
cdata.set('API_EXAMPLES', join_paths(dpdk_build_root, 'doc', 'api', 'examples.dox'))
cdata.set('OUTPUT', join_paths(dpdk_build_root, 'doc', 'api'))
cdata.set('TOPDIR', dpdk_source_root)
cdata.set('STRIP_FROM_PATH', ' '.join([dpdk_source_root, join_paths(dpdk_build_root, 'doc', 'api')]))
cdata.set('WARN_AS_ERROR', 'NO')
if get_option('werror')
    cdata.set('WARN_AS_ERROR', 'YES')
endif

# configure HTML Doxygen run
html_cdata = configuration_data()
html_cdata.merge_from(cdata)
html_cdata.set('GENERATE_HTML', 'YES')
html_cdata.set('GENERATE_MAN', 'NO')
html_cdata.set('FULL_PATH_NAMES', 'YES')

doxy_html_conf = configure_file(input: 'doxy-api.conf.in',
        output: 'doxy-api-html.conf',
        configuration: html_cdata)

# configure manpage Doxygen run
man_cdata = configuration_data()
man_cdata.merge_from(cdata)
man_cdata.set('GENERATE_HTML', 'NO')
man_cdata.set('GENERATE_MAN', 'YES')
# for manpages, have the pages only titled with the header name,
# rather than the full path to the header
man_cdata.set('FULL_PATH_NAMES', 'NO')

doxy_man_conf = configure_file(input: 'doxy-api.conf.in',
        output: 'doxy-api-man.conf',
        configuration: man_cdata)

# do Doxygen runs
doxy_html_build = custom_target('doxygen-html',
        depends: example,
        depend_files: 'doxy-api-index.md',
        input: doxy_html_conf,
        output: 'html',
        depfile: 'html.d',
        command: [generate_doxygen, '@OUTPUT@', doxygen, '@INPUT@'],
        install: get_option('enable_docs'),
        install_dir: htmldir,
        build_by_default: get_option('enable_docs'))

doc_targets += doxy_html_build
doc_target_names += 'Doxygen_API(HTML)'

doxy_man_build = custom_target('doxygen-man',
        depends: example,
        depend_files: 'doxy-api-index.md',
        input: doxy_man_conf,
        output: 'man',
        depfile: 'man.d',
        command: [generate_doxygen, '@OUTPUT@', doxygen, '@INPUT@'],
        install: get_option('enable_docs'),
        install_dir: get_option('datadir'),
        build_by_default: get_option('enable_docs'))

doc_targets += doxy_man_build
doc_target_names += 'Doxygen_API(Manpage)'

# refresh the manpage database on install
# if DPDK manpages are installed to a staging directory, not in MANPATH, this has no effect
mandb = find_program('mandb', required: false)
if mandb.found() and get_option('enable_docs') and meson.version().version_compare('>=0.55.0')
    meson.add_install_script(mandb)
endif
