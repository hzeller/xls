// Copyright 2023 The XLS Authors
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

// Very simple language server for dslx that
//  - keeps track of open files and updates them whenever they are
//    changed in the editor (hidden under the hood).
//  - On every change, attempts to parse and send back diagnostics
//    on errors/warnings.
//
// Heavily commented below as this serves as a sample.

#include <unistd.h>

#include <filesystem>  // NOLINT
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "verible/common/lsp/json-rpc-dispatcher.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/lsp/lsp-text-buffer.h"
#include "verible/common/lsp/message-stream-splitter.h"
#include "xls/common/init_xls.h"
#include "xls/dslx/lsp/language_server_adapter.h"

ABSL_FLAG(std::string, stdlib_path, xls::kDefaultDslxStdlibPath,
          "Path to DSLX standard library files.");

static constexpr char kDslxPath[] = "DSLX_PATH";
ABSL_FLAG(std::string, dslx_path,
          getenv(kDslxPath) != nullptr ? getenv(kDslxPath) : "",
          "Additional paths to search for modules (colon delimited).");

namespace xls::dslx {
namespace {

namespace fs = std::filesystem;

using verible::lsp::BufferCollection;
using verible::lsp::DocumentSymbol;
using verible::lsp::EditTextBuffer;
using verible::lsp::InitializeResult;
using verible::lsp::JsonRpcDispatcher;
using verible::lsp::MessageStreamSplitter;

// The "initialize" method requests server capabilities.
InitializeResult InitializeServer(const nlohmann::json& params) {
  nlohmann::json capabilities;
  capabilities["textDocumentSync"] = {
      {"openClose", true},  // Want open/close events
      {"change", 2},        // Incremental updates
  };
  capabilities["documentSymbolProvider"] = true;
  capabilities["definitionProvider"] = {
      {"dynamicRegistration", false},
      {"linkSupport", true},
  };
  return InitializeResult{
      .capabilities = std::move(capabilities),
      .serverInfo =
          {
              .name = "XLS testing language server.",
              .version = "0.1",
          },
  };
}

// On text change: attempt to parse the buffer and emit diagnostics if needed.
void TextChangeHandler(const std::string& file_uri,
                       const EditTextBuffer& text_buffer,
                       verible::lsp::JsonRpcDispatcher& dispatcher,
                       LanguageServerAdapter& adapter) {
  text_buffer.RequestContent([&](std::string_view file_content) {
    adapter.Update(file_uri, file_content);
  });
  verible::lsp::PublishDiagnosticsParams params{
      .uri = file_uri,
      .diagnostics = adapter.GenerateParseDiagnostics(file_uri),
  };
  dispatcher.SendNotification("textDocument/publishDiagnostics", params);
}

absl::Status RealMain() {
  const std::string stdlib_path = absl::GetFlag(FLAGS_stdlib_path);
  const std::string dslx_path = absl::GetFlag(FLAGS_dslx_path);
  const std::vector<fs::path> dslx_paths = absl::StrSplit(dslx_path, ':');

  LspLog() << "XLS testing language server" << std::endl;
  LspLog() << "Path configuration:\n\tstdlib=" << stdlib_path << std::endl
           << "\tdsxl_path=" << dslx_path << std::endl;

  // Adapter that interfaces between dslx parsing and LSP
  LanguageServerAdapter language_server_adapter(stdlib_path, dslx_paths);

  // The dispatcher receives json rpc requests
  // (https://www.jsonrpc.org/specification) which are passed in
  // by calls to its DispatchMessage().
  // It extracts method and notification calls within, calls the
  // registered handlers, then writes the json result to its output.
  // Output is formatted as header/body to stdout.
  JsonRpcDispatcher dispatcher([](std::string_view reply) {
    // Output formatting as header/body chunk as required by LSP spec.
    std::cout << "Content-Length: " << reply.size() << "\r\n\r\n";
    std::cout << reply << std::flush;
  });

  // The input is continuous stream of (header/body)*. The stream
  // splitter separates these messages and feeds them one by one
  // to the dispatcher.
  MessageStreamSplitter stream_splitter;
  stream_splitter.SetMessageProcessor(
      [&dispatcher](std::string_view header, std::string_view body) {
        return dispatcher.DispatchMessage(body);
      });

  // -- Add request handlers reacting to json-RPC method and notification calls

  // Exchange of capabilities.
  dispatcher.AddRequestHandler("initialize", InitializeServer);

  // The client sends a request to shut down. Use that to exit our main loop.
  bool shutdown_requested = false;
  dispatcher.AddRequestHandler("shutdown",
                               [&shutdown_requested](const nlohmann::json&) {
                                 shutdown_requested = true;
                                 return nullptr;
                               });

  // The buffer collection keeps track of all the buffers opened in the editor.
  // It registers multiple notification request handlers (for open, edit,
  // remove) on the dispatcher to keep an up-to-date copy of all open editor
  // buffers.
  BufferCollection buffers(&dispatcher);

  // The text buffer collection can call a callback whenever there is a change.
  // We're using this to hook up our parser that then can send diagnostic
  // messages back.
  buffers.SetChangeListener(
      [&](const std::string& uri, const EditTextBuffer* buffer) {
        if (buffer == nullptr) {
          return;  // buffer got deleted. No interest.
        }
        TextChangeHandler(uri, *buffer, dispatcher, language_server_adapter);
      });

  dispatcher.AddRequestHandler(
      "textDocument/documentSymbol",
      [&](const verible::lsp::DocumentSymbolParams& params) {
        return language_server_adapter.GenerateDocumentSymbols(
            params.textDocument.uri);
      });

  dispatcher.AddRequestHandler(
      "textDocument/definition",
      [&](const verible::lsp::DefinitionParams& params) {
        return language_server_adapter.FindDefinitions(params.textDocument.uri,
                                                       params.position);
      });

  // Main loop. Feeding the stream-splitter that then calls the dispatcher.
  absl::Status status = absl::OkStatus();
  while (status.ok() && !shutdown_requested) {
    status = stream_splitter.PullFrom([](char* buf, int size) -> int {  //
      return static_cast<int>(read(STDIN_FILENO, buf, size));
    });
  }

  LspLog() << status << std::endl;
  return status;
}

}  // namespace
}  // namespace xls::dslx

int main(int argc, char* argv[]) {
  xls::InitXls(argv[0], argc, argv);

  XLS_QCHECK_OK(xls::dslx::RealMain());
  return EXIT_SUCCESS;
}
