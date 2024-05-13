load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("//bazel:defs.bzl", "LINKOPTS")

package(default_visibility = ["//visibility:public"])

cc_grpc_library(
    name = "kv_grpc",
    srcs = ["protos/kv.proto"],
    deps = []
)

cc_grpc_library(
    name = "kvmaster_grpc",
    srcs = ["protos/kvmaster.proto"],
    deps = []
)

cc_library(
    name = "utils",
    srcs = ["common/common_util.cc"],
    hdrs = ["common/common_util.h"],
)

cc_library(
    name = "kv-client",
    srcs = ["kv/storage_client.cc"],
    hdrs = ["kv/storage_client.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@openssl//:headers",
        ":kv_grpc",
        ":kvmaster_grpc",
        ":kv-exceptions",
        ":utils",
    ],
    linkopts = ["-L/usr/local/opt/openssl/lib -lssl -lcrypto"],
)

cc_library(
    name = "kv-master-client",
    srcs = ["kv/storage_client_coordinator.cc"],
    hdrs = ["kv/storage_client_coordinator.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        ":kv_grpc",
        ":kv-exceptions",
        ":utils",
    ],
)

cc_library(
    name = "kv-exceptions",
    hdrs = ["kv/err_except.h"],
    srcs = ["kv/err_except.cc"],
    deps = ["@com_github_grpc_grpc//:grpc++"],
)

cc_library(
    name = "kvheader",
    hdrs = ["kv/KVClass.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@fmt",
        ":kv_grpc",
        ":kvmaster_grpc",
        ":kv-exceptions",
        ":utils",
    ],
)

cc_library(
    name = "kvmaster-header",
    hdrs = ["kv_coordinator.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        ":kvmaster_grpc",
        ":kv-master-client",
        ":utils",
    ],
)

# executables
cc_binary(
    name = "kv_server",
    srcs = ["kv_server.cc"],
    deps = [
        "kvheader",
    ],
    linkopts = LINKOPTS,
)

cc_binary(
    name = "kv_coordinator",
    srcs = ["kv_coordinator.cc"],
    deps = [
        "kvmaster-header",
    ],
    linkopts = LINKOPTS,
)

