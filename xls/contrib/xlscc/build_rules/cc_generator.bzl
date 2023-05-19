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

"""Provides fuzz test functionality for XLScc"""

def xls_int_fuzz_binaries(name, seed_start, seed_count):
    """Generate fuzz test binaries for a range of seeds and the result comparison binary

    Args:
          name: descriptive name of the fuzz test
          seed_start: start seed of test to generate
          seed_count: number of tests to generate
    """
    all_outputs = []
    for x in range(seed_count):
        seed = seed_start + x
        srcfile = "{}_{}.cc".format(name, str(seed))

        native.genrule(
            name = "fuzzfiles_{}_{}".format(name, str(seed)),
            outs = [srcfile],
            cmd = "./$(location cc_generate_test) -seed=" + str(seed) + " -cc_filepath=$(OUTS)",
            tools = ["cc_generate_test"],
        )

        native.cc_binary(
            name = "{}_{}".format(name, str(seed)),
            srcs = [srcfile],
            deps = ["@com_github_hlslibs_ac_types//:ac_int"],
        )
        all_outputs.append("{}_{}".format(name, str(seed)))
        all_outputs.append(srcfile)

    native.filegroup(
        name = "{}_group".format(name),
        data = all_outputs,
    )

    all_outputs.extend([
        ":synth_only_headers",
        ":xlscc",
        "@com_github_hlslibs_ac_types//:ac_types_as_data",
    ])

    native.cc_test(
        name = "cc_fuzz_tester",
        testonly = 1,
        srcs = ["cc_fuzz_tester.cc"],
        data = all_outputs,
        args = [
            "--seed={}".format(seed_start),
            "--sample_count={}".format(seed_count),
        ],
        deps = [
            ":cc_generator",
            ":unit_test",
            "@com_google_absl//absl/container:inlined_vector",
            "@com_google_absl//absl/flags:flag",
            "@com_google_absl//absl/status:statusor",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/strings:str_format",
            "//xls/codegen:combinational_generator",
            "//xls/codegen:module_signature",
            "//xls/common:init_xls",
            "//xls/common:subprocess",
            "//xls/common/file:filesystem",
            "//xls/common/file:get_runfile_path",
            "//xls/common/file:temp_directory",
            "//xls/common/logging",
            "//xls/common/status:matchers",
            "//xls/common/status:status_macros",
            "//xls/interpreter:ir_interpreter",
            "//xls/ir",
            "//xls/ir:events",
            "//xls/ir:function_builder",
            "//xls/ir:ir_test_base",
            "//xls/ir:value",
            "//xls/passes:standard_pipeline",
            "//xls/simulation:module_simulator",
            "//xls/simulation:verilog_simulators",
        ],
    )
