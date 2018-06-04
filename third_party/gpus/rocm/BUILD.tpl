licenses(["restricted"])  # MPL2, portions GPL v3, LGPL v3, BSD-like

package(default_visibility = ["//visibility:public"])

config_setting(
    name = "using_hipcc",
    values = {
        "define": "using_rocm_hipcc=true",
    },
)

config_setting(
    name = "darwin",
    values = {"cpu": "darwin"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "freebsd",
    values = {"cpu": "freebsd"},
    visibility = ["//visibility:public"],
)

cc_library(
    name = "rocm_headers",
    hdrs = [
        "rocm/rocm_config.h",
        %{rocm_headers}
    ],
    includes = [
        ".",
        "rocm/include",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "rocmrt_static",
    srcs = ["rocm/lib/%{rocmrt_static_lib}"],
    includes = [
        ".",
        "rocm/include",
    ],
    linkopts = select({
        ":freebsd": [],
        "//conditions:default": ["-ldl"],
    }) + [
        "-lpthread",
        %{rocmrt_static_linkopt}
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "rocmrt",
    srcs = ["rocm/lib/%{rocmrt_lib}"],
    data = ["rocm/lib/%{rocmrt_lib}"],
    includes = [
        ".",
        "rocm/include",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "hipblas",
    srcs = ["rocm/lib/%{hipblas_lib}"],
    data = ["rocm/lib/%{hipblas_lib}"],
    includes = [
        ".",
        "rocm/include",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "rocfft",
    srcs = ["rocm/lib/%{rocfft_lib}"],
    data = ["rocm/lib/%{rocfft_lib}"],
    includes = [
        ".",
        "rocm/include",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "hiprand",
    srcs = ["rocm/lib/%{hiprand_lib}"],
    data = ["rocm/lib/%{hiprand_lib}"],
    includes = [
        ".",
        "rocm/include",
        "rocm/include/rocrand",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "miopen",
    srcs = ["rocm/lib/%{miopen_lib}"],
    data = ["rocm/lib/%{miopen_lib}"],
    includes = [
        ".",
        "rocm/include",
    ],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "rocm",
    visibility = ["//visibility:public"],
    deps = [
        ":rocm_headers",
        ":rocmrt",
        ":hipblas",
        ":rocfft",
        ":hiprand",
        ":miopen",
    ],
)

%{rocm_include_genrules}
