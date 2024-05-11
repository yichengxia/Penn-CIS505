load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

package(default_visibility = ["//visibility:public"])

cc_library(
   name = "mailhelper",
   srcs = ["mailhelper.cc"],
   hdrs = ["mailhelper.h"],
)

cc_binary(
    name = "mailserver",
    srcs = ["mailserver.cc"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
        ":mailhelper",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)
