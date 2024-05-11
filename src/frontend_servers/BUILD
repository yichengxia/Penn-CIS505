load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//bazel:defs.bzl", "LINKOPTS")

cc_library(
    name = "request_handler",
    srcs = ["request_handler.cc"],
    hdrs = ["request_handler.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src/frontend_servers:util",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
        "//src/webmail:mailhelper",
    ],
)

cc_library(
    name = "admin_server",
    srcs = ["admin_server.cc"],
    deps = [
        "//src/frontend_servers:util",
    	"//src:kv-client",
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-exceptions",
        "//src:utils",
        "@com_github_nlohmann_json//:json",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)

cc_library(
    name = "frontend_server",
    srcs = ["frontend_server.cc"],
    hdrs = ["frontend_server.h"],
    deps = [
    	"//src/frontend_servers:request_handler",
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-client",
        "//src:kv-exceptions",
        "//src:utils",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "//src:kv-master-client",
    ],
    linkopts = LINKOPTS + ["-lpthread -lresolv"],
)
