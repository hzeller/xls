# Copyright 2023 The XLS Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# DSLX formatting library components.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//xls:xls_internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "pretty_print",
    srcs = ["pretty_print.cc"],
    hdrs = ["pretty_print.h"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "//xls/common:strong_int",
        "//xls/common:visitor",
        "//xls/common/logging",
    ],
)

cc_test(
    name = "pretty_print_test",
    srcs = ["pretty_print_test.cc"],
    deps = [
        ":pretty_print",
        "//xls/common:xls_gunit",
        "//xls/common:xls_gunit_main",
    ],
)