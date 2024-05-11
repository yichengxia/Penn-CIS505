load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# ==== gRPC ===
http_archive(
    name = "com_github_grpc_grpc",
    urls = [
        "https://github.com/grpc/grpc/archive/358bfb581feeda5bf17dd3b96da1074d84a6ef8d.tar.gz",
    ],
    strip_prefix = "grpc-358bfb581feeda5bf17dd3b96da1074d84a6ef8d",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
grpc_extra_deps()

# ==== openSSL ====
new_local_repository(
    name = "openssl",
    path = "/usr/include/openssl",
    build_file_content = """
package(default_visibility = ["//visibility:public"])
cc_library(
    name = "headers",
    hdrs = glob(["**/*.h"]),
    includes = ["."],
)
"""
)

# ==== {fmt} ====
git_repository(
    name = "fmt",
    commit = "a33701196adfad74917046096bf5a2aa0ab0bb50",
    remote = "https://github.com/fmtlib/fmt",
    patch_cmds = [
        "mv support/bazel/.bazelrc .bazelrc",
        "mv support/bazel/.bazelversion .bazelversion",
        "mv support/bazel/BUILD.bazel BUILD.bazel",
        "mv support/bazel/WORKSPACE.bazel WORKSPACE.bazel",
    ],
    patch_cmds_win = [
        "Move-Item -Path support/bazel/.bazelrc -Destination .bazelrc",
        "Move-Item -Path support/bazel/.bazelversion -Destination .bazelversion",
        "Move-Item -Path support/bazel/BUILD.bazel -Destination BUILD.bazel",
        "Move-Item -Path support/bazel/WORKSPACE.bazel -Destination WORKSPACE.bazel",
    ],
)

# ==== json ====
http_archive(
    name = "com_github_nlohmann_json",
    build_file = "//bazel:json.BUILD",
    strip_prefix = "json-3.11.2",
    urls = ["https://github.com/nlohmann/json/archive/v3.11.2.tar.gz"],
    sha256 = "d69f9deb6a75e2580465c6c4c5111b89c4dc2fa94e3a85fcd2ffcd9a143d9273",
)
