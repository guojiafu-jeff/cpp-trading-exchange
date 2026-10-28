load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "order_test",
    srcs = ["order_test.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wpedantic",
    ],
    deps = [
        "//exchange/matcher:order",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "order_book_test",
    srcs = ["order_book_test.cpp"],
    copts = [
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wpedantic",
    ],
    deps = [
        "//exchange/matcher:order_book",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)