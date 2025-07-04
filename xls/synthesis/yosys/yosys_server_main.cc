// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "grpcpp/security/server_credentials.h"
#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"
#include "grpcpp/server_context.h"
#include "xls/common/file/filesystem.h"
#include "xls/common/init_xls.h"
#include "xls/synthesis/credentials.h"
#include "xls/synthesis/yosys/yosys_synthesis_service.h"

static constexpr std::string_view kUsage =
    R"( Launches a XLS synthesis server which uses Yosys for synthesis
and optionally Nextpnr for place-and-route *OR* OpenSTA for static timing.

Invocation:

  yosys_server --yosys_path=PATH --port=N
     [--synthesis_target=DEV --nextpnr_path=PATH]
     *or*
     [--synthesis_libraries=LIB --sta_path=STAPATH --sta_libraries=STALIBS]
     *or*
     [--synthesis_only (--synthesis_target=DEV | --synthesis_libraries=LIB)]
)";

ABSL_FLAG(int32_t, port, 10000, "Port to listen on.");
ABSL_FLAG(std::string, yosys_path, "", "The path to the yosys binary.");
ABSL_FLAG(std::string, nextpnr_path, "", "The path to the nextpnr binary.");
ABSL_FLAG(std::string, synthesis_target, "",
          "The FPGA backend to target for synthesis; e.g. ice40, ecp5.");
ABSL_FLAG(bool, save_temps, false, "Do not delete temporary files.");
ABSL_FLAG(bool, synthesis_only, false,
          "Perform synthesis but not place / route / timing.");
ABSL_FLAG(bool, return_netlist, true,
          "Return the netlist generated by synthesis.");
ABSL_FLAG(std::string, sta_path, "", "The path to the sta binary.");
ABSL_FLAG(std::string, synthesis_libraries, "",
          "The technology library file to target for synthesis; *.lib");
ABSL_FLAG(
    std::string, sta_libraries, "",
    "The technology library/libraries file to target for STA; *.lib * lib.gz");
ABSL_FLAG(std::string, default_driver_cell, "",
          "The default driver cell to use during synthesis.");
ABSL_FLAG(std::string, default_load, "",
          "The default load cell to use during synthesis.");

namespace xls {
namespace synthesis {
namespace {

void RealMain() {
  std::string yosys_path = absl::GetFlag(FLAGS_yosys_path);
  std::string nextpnr_path = absl::GetFlag(FLAGS_nextpnr_path);
  std::string synthesis_target = absl::GetFlag(FLAGS_synthesis_target);
  std::string sta_path = absl::GetFlag(FLAGS_sta_path);
  std::string synthesis_libraries = absl::GetFlag(FLAGS_synthesis_libraries);
  std::string sta_libraries = absl::GetFlag(FLAGS_sta_libraries);
  const bool synthesis_only = absl::GetFlag(FLAGS_synthesis_only);

  // Check flags -- common
  QCHECK_OK(FileExists(yosys_path)) << "Valid --yosys_path must be provided";
  QCHECK(synthesis_target.empty() || synthesis_libraries.empty())
      << "\nBoth --synthesis_target and --synthesis_libraries were provided. "
      << "Use one or the other.";
  QCHECK(!(synthesis_target.empty() && synthesis_libraries.empty()))
      << "\nMust specify either --synthesis_target or --synthesis_libraries.\n";

  // Check backend flags -- 'synthesis_target' determines mode
  if (!synthesis_only) {
    if (!synthesis_target.empty()) {
      std::string msg =
          "\n--synthesis_target was provided, so using Nextpnr back end.\n";
      QCHECK_OK(FileExists(nextpnr_path))
          << msg << "Valid --nextpnr_path must be provided.";
    } else {
      std::string msg =
          "\n--synthesis_target not provided, so targeting OpenSTA.\n";
      QCHECK(!synthesis_libraries.empty())
          << msg << "--synthesis_libraries must be provided";
      QCHECK_OK(FileExists(sta_path))
          << msg << "Valid --sta_path must be provided.";
      QCHECK(!sta_libraries.empty())
          << msg << "--sta_libraries must be provided";
    }
  }

  int port = absl::GetFlag(FLAGS_port);
  std::string server_address = absl::StrCat("0.0.0.0:", port);
  YosysSynthesisServiceImpl service(
      yosys_path, nextpnr_path, synthesis_target, sta_path, synthesis_libraries,
      sta_libraries, absl::GetFlag(FLAGS_default_driver_cell),
      absl::GetFlag(FLAGS_default_load), absl::GetFlag(FLAGS_save_temps),
      absl::GetFlag(FLAGS_return_netlist), synthesis_only);

  ::grpc::ServerBuilder builder;
  std::shared_ptr<::grpc::ServerCredentials> creds = GetServerCredentials();
  builder.AddListeningPort(server_address, creds);
  builder.RegisterService(&service);
  std::unique_ptr<::grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "Serving on port: " << port;
  LOG(INFO) << "synthesis_target: " << synthesis_target;
  server->Wait();
}

}  // namespace
}  // namespace synthesis
}  // namespace xls

int main(int argc, char** argv) {
  xls::InitXls(kUsage, argc, argv);

  xls::synthesis::RealMain();

  return EXIT_SUCCESS;
}
