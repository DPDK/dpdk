load("@rules_foreign_cc//foreign_cc:configure.bzl", "configure_make")

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:private"],
)

configure_make(
    name = "libnuma",
    # bazel will throw error: use of undeclared identifier 'redacted' because bazel sets __DATE__ to "redacted".
    # https://github.com/bazel-contrib/rules_foreign_cc/issues/239
    configure_options = ["CFLAGS='-Dredacted=\"redacted\"'"],
    lib_source = ":srcs",
    out_headers_only = True,
    visibility = ["//visibility:public"],
    alwayslink = True,
)
