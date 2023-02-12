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

#ifndef XLS_DSLX_MAKE_STRUCT_FORMAT_DESCRIPTOR_H_
#define XLS_DSLX_MAKE_STRUCT_FORMAT_DESCRIPTOR_H_

#include "xls/dslx/concrete_type.h"
#include "xls/dslx/struct_format_descriptor.h"

namespace xls::dslx {

// Converts a struct type, as determined e.g. in type inference, into a format
// descriptor that can be used in formatting a struct in e.g. trace output.
std::unique_ptr<StructFormatDescriptor> MakeStructFormatDescriptor(
    const StructType& struct_type);

}  // namespace xls::dslx

#endif  // XLS_DSLX_MAKE_STRUCT_FORMAT_DESCRIPTOR_H_
