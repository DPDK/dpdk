load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def cpp_deps():
    git_repository(
        name = "libnuma",
        remote = "https://github.com/numactl/numactl",
        patch_cmds = [
            "./autogen.sh",
        ],
        build_file = "//bazel:numactl.BUILD",
        commit = "b34da01b0017946a1a77057c247cfc6d231a0215",
    )

def _cpp_deps_extension_impl(_):
    cpp_deps()

cpp_deps_extension = module_extension(
    implementation = _cpp_deps_extension_impl,
)
