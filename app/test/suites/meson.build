# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation

# some perf tests (eg: memcpy perf autotest) take a very long
# to complete, so timeout to 10 minutes
timeout_seconds = 600
timeout_seconds_fast = 10

test_no_huge_args = ['--no-huge', '-m', '2048']
has_hugepage = run_command(has_hugepages_cmd, check: true).stdout().strip() != '0'
message('hugepage availability: @0@'.format(has_hugepage))

# process source files to determine the different unit test suites
# - fast_tests
# - perf_tests
# - driver_tests
test_suites = run_command(get_test_suites_cmd, autotest_sources,
         check: true).stdout().strip().split()
foreach suite:test_suites
    # simple cases - tests without parameters or special handling
    suite = suite.split('=')
    suite_name = suite[0]
    suite_tests = suite[1].split(',')
    if suite_name != 'fast-tests'
        # simple cases - tests without parameters or special handling
        foreach t: suite_tests
            test(t, dpdk_test,
                    env: ['DPDK_TEST=' + t],
                    timeout: timeout_seconds,
                    is_parallel: false,
                    suite: suite_name)
        endforeach
    else
        # special fast-test handling here
        foreach t: suite_tests
            params = t.split(':')
            test_name = params[0]
            nohuge = params[1] == 'true'
            asan = params[2] == 'true'

            test_args = []
            if nohuge
                test_args += test_no_huge_args
            elif not has_hugepage
                continue  #skip this tests
            endif
            if not asan and (get_option('b_sanitize') == 'address'
                    or get_option('b_sanitize') == 'address,undefined')
                continue  # skip this test
            endif

            if get_option('default_library') == 'shared'
                test_args += ['-d', dpdk_drivers_build_dir]
            endif

            test(test_name, dpdk_test,
                args : test_args,
                env: ['DPDK_TEST=' + test_name],
                timeout : timeout_seconds_fast,
                is_parallel : false,
                suite : 'fast-tests')
            if not is_windows and test_name == 'trace_autotest'
                test_args += ['--trace=.*']
                test_args += ['--trace-dir=@0@'.format(meson.current_build_dir())]
                test(test_name + '_with_traces', dpdk_test,
                    args : test_args,
                    env: ['DPDK_TEST=' + test_name],
                    timeout : timeout_seconds_fast,
                    is_parallel : false,
                    suite : 'fast-tests')
            endif
        endforeach
    endif
endforeach
