# SPDX-License-Identifier: BSD-3-Clause

test_cfgfiles = files(
        'empty.ini',
        'empty_key_value.ini',
        'invalid_section.ini',
        'line_too_long.ini',
        'missing_section.ini',
        'realloc_sections.ini',
        'sample1.ini',
        'sample2.ini',
)

# generate the header file used in cfgfile test
test_cfgfile_h = custom_target('test_cfgfile',
        output: 'test_cfgfiles.h',
        input: test_cfgfiles,
        capture: true,
        command: [ header_gen_cmd, '@INPUT@'])
