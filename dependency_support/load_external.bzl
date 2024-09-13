# Copyright 2020 The XLS Authors
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

"""Provides helper that loads external repositories with third-party code."""

# TODO(https://github.com/google/xls/issues/931): all the remaining toplevel projects we need should move to MODULE.bazel,
# somewhat dependent on what becomes available in https://registry.bazel.build/.
# Eventual goal that none of this is needed anymore and the file can be removed.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//dependency_support/boost:workspace.bzl", repo_boost = "repo")
load("//dependency_support/llvm:workspace.bzl", repo_llvm = "repo")
load("//dependency_support/rules_hdl:workspace.bzl", repo_rules_hdl = "repo")

def load_external_repositories():
    """Loads external repositories with third-party code."""

    # Note: there are more direct dependencies than are explicitly listed here.
    #
    # By letting direct dependencies be satisfied by transitive WORKSPACE
    # setups we let the transitive dependency versions "float" to satisfy their
    # package requirements, so that we can bump our dependency versions here
    # more easily without debugging/resolving unnecessary conflicts.
    #
    # This situation will change when XLS moves to bzlmod. See
    # https://github.com/google/xls/issues/865 and
    # https://github.com/google/xls/issues/931#issue-1667228764 for more
    # information / background.

    repo_boost()
    repo_llvm()
    repo_rules_hdl()

    # Release on 2024-09-13, current as of 2024-10-01
    http_archive(
        name = "bazel_features",
        sha256 = "bdc12fcbe6076180d835c9dd5b3685d509966191760a0eb10b276025fcb76158",
        strip_prefix = "bazel_features-1.17.0",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.17.0/bazel_features-v1.17.0.tar.gz",
    )

    http_archive(
        name = "rules_proto",
        sha256 = "6fb6767d1bef535310547e03247f7518b03487740c11b6c6adb7952033fe1295",
        strip_prefix = "rules_proto-6.0.2",
        urls = [
            "https://github.com/bazelbuild/rules_proto/releases/download/6.0.2/rules_proto-6.0.2.tar.gz",
        ],
    )

    # Released on 2024-09-24, current as of 2024-10-01
    http_archive(
        name = "rules_python",
        sha256 = "ca77768989a7f311186a29747e3e95c936a41dffac779aff6b443db22290d913",
        strip_prefix = "rules_python-0.36.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.36.0/rules_python-0.36.0.tar.gz",
    )

    http_archive(
        name = "z3",
        urls = ["https://github.com/Z3Prover/z3/archive/z3-4.12.2.tar.gz"],
        sha256 = "9f58f3710bd2094085951a75791550f547903d75fe7e2fcb373c5f03fc761b8f",
        strip_prefix = "z3-z3-4.12.2",
        build_file = "//dependency_support/z3:bundled.BUILD.bazel",
    )

    # Release 2024-02-23, current as of 2024-06-26
    http_archive(
        name = "io_bazel_rules_closure",
        sha256 = "70ef2b4da987bf0d266e663d7c251eac509ff70dd65bba02d41d1e86e840a569",
        strip_prefix = "rules_closure-0.13.0",
        urls = [
            "https://github.com/bazelbuild/rules_closure/archive/0.13.0.tar.gz",
        ],
    )

    # Commit from 2024-02-22, current as of 2024-06-26
    http_archive(
        name = "linenoise",
        sha256 = "839ed407fe0dfa5fd7dd103abfc695dee72fea2840df8d4250ad42b0e64839e8",
        strip_prefix = "linenoise-d895173d679be70bcd8b23041fff3e458e1a3506",
        urls = ["https://github.com/antirez/linenoise/archive/d895173d679be70bcd8b23041fff3e458e1a3506.tar.gz"],
        build_file = "//dependency_support/linenoise:bundled.BUILD.bazel",
    )

    # Needed by fuzztest. Release 2024-05-21, current as of 2024-06-26
    http_archive(
        name = "snappy",
        sha256 = "736aeb64d86566d2236ddffa2865ee5d7a82d26c9016b36218fcc27ea4f09f86",
        build_file = "@com_google_riegeli//third_party:snappy.BUILD",
        strip_prefix = "snappy-1.2.1",
        urls = ["https://github.com/google/snappy/archive/1.2.1.tar.gz"],
    )

    # Needed by fuzztest. Release 2023-08-31, current as of 2024-06-26
    http_archive(
        name = "org_brotli",
        sha256 = "e720a6ca29428b803f4ad165371771f5398faba397edf6778837a18599ea13ff",
        strip_prefix = "brotli-1.1.0",
        urls = ["https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz"],
    )

    # Needed by fuzztest. Commit from 2024-04-18, current as of 2024-06-26
    http_archive(
        name = "highwayhash",
        build_file = "@com_google_riegeli//third_party:highwayhash.BUILD",
        sha256 = "d564c621618ef734e0ae68545f59526e97dfe4912612f80b2b8b9b31b9bb02b5",
        strip_prefix = "highwayhash-f8381f3331d9c56a9792f9b4a35f61c41108c39e",
        urls = ["https://github.com/google/highwayhash/archive/f8381f3331d9c56a9792f9b4a35f61c41108c39e.tar.gz"],
    )

    # Updated to head on 2024-03-14
    FUZZTEST_COMMIT = "393ae75c0fca5f9892e73969da5d6bce453ad318"
    http_archive(
        name = "com_google_fuzztest",
        strip_prefix = "fuzztest-" + FUZZTEST_COMMIT,
        url = "https://github.com/google/fuzztest/archive/" + FUZZTEST_COMMIT + ".zip",
        sha256 = "a0558ceb617d78ee93d7e6b62930b4aeebc02f1e5817d4d0dae53699f6f6c352",
        patch_args = ["-p1", "-R"],  # reverse patch until we upgrade bazel to 7.1; see: bazelbuild/bazel#19233.
        patches = ["//dependency_support/com_google_fuzztest:e317d5277e34948ae7048cb5e48309e0288e8df3.patch"],
    )

    # Used by xlscc. Tagged 2024-02-16 (note: release is lagging tag), current as of 2024-06-26
    http_archive(
        name = "com_github_hlslibs_ac_types",
        urls = ["https://github.com/hlslibs/ac_types/archive/refs/tags/4.8.0.tar.gz"],
        sha256 = "238197203f8c6254a1d6ac6884e89e6f4c060bffb7473d336df4a1fb53ba7fab",
        strip_prefix = "ac_types-4.8.0",
        build_file = "//dependency_support/com_github_hlslibs_ac_types:bundled.BUILD.bazel",
    )

    # Released 2024-09-13, current as of 2024-10-16.
    ORTOOLS_VERSION = "9.11"
    http_archive(
        name = "com_google_ortools",
        urls = ["https://github.com/google/or-tools/archive/refs/tags/v{tag}.tar.gz".format(tag = ORTOOLS_VERSION)],
        integrity = "sha256-9qC9W58wWKoagUt5jbXTk8Meycu2EDSGcomXtJqxJ7w=",
        strip_prefix = "or-tools-" + ORTOOLS_VERSION,
    )

    # Tagged 2024-11-23, current as of 2024-11-25
    VERIBLE_TAG = "v0.0-3858-g660d1664"
    http_archive(
        name = "verible",
        sha256 = "89bba2f840bacc9cb9145e7e40ae70d30657cd874425ecee589bc04e623803f3",
        strip_prefix = "verible-" + VERIBLE_TAG.lstrip("v"),
        urls = ["https://github.com/chipsalliance/verible/archive/refs/tags/" + VERIBLE_TAG + ".tar.gz"],
    )

    # Used in C++ tests of the ZSTD Module
    # Transitive dependency of fuzztest (required by riegeli in fuzztest workspace)
    # Version fdfb2aff released on 2024-07-31
    # https://github.com/facebook/zstd/commit/fdfb2aff39dc498372d8c9e5f2330b692fea9794
    # Updated 2024-08-08
    # This does exist already in https://registry.bazel.build/ - NB: when moving to MODULE.bazel,
    # we probably need to use our local bundled.BUILD.bazel as it provides the decodecorpus.
    http_archive(
        name = "zstd",
        sha256 = "9ace5a1b3c477048c6e034fe88d2abb5d1402ced199cae8e9eef32fdc32204df",
        strip_prefix = "zstd-fdfb2aff39dc498372d8c9e5f2330b692fea9794",
        urls = ["https://github.com/facebook/zstd/archive/fdfb2aff39dc498372d8c9e5f2330b692fea9794.zip"],
        build_file = "//dependency_support/com_github_facebook_zstd:bundled.BUILD.bazel",
    )
