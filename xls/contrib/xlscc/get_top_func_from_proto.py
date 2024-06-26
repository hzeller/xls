# Copyright 2021 The XLS Authors
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
"""Return the entry point from XLScc-generated metadata.

XLScc-generated XLSIR needs #pragma hls_top denoting the entry point.  That
entry point is expected by several other tools.  get_top_func_from_proto
extracts that entry point from the metadata protobuf blob and prints it to
stdout.

Input: protocol buffer generated by XLScc.
"""

from typing import Sequence

from absl import app

from xls.contrib.xlscc import metadata_output_pb2


def main(argv: Sequence[str]) -> None:
  if len(argv) != 2:
    raise app.UsageError('Expecting a protocol buffer.')
  m = metadata_output_pb2.MetadataOutput()
  with open(argv[1], 'rb') as reader:
    m.ParseFromString(reader.read())
  if (
      m
      and m.top_func_proto
      and m.top_func_proto.name
      and m.top_func_proto.name.name
  ):
    print(m.top_func_proto.name.name)
  else:
    raise app.UsageError(
        'Input {} does not appear to be XLScc metadata.'.format(argv[1])
    )


if __name__ == '__main__':
  app.run(main)
