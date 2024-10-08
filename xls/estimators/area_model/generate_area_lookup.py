#
# Copyright 2024 The XLS Authors
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

"""Extracts area model from a text proto, constructs C++ lookup code."""

from absl import app
from absl import flags
import jinja2

from google.protobuf import text_format
from xls.common import runfiles
from xls.estimators import estimator_model
from xls.estimators import estimator_model_pb2
from xls.estimators.area_model import area_model_utils

flags.DEFINE_string(
    'model_name',
    None,
    'Name of model. Should be a short string (e.g., "asap7"). Used as the '
    'identifier when accessing the mode via xls::GetAreaEstimator.',
    required=True,
)
FLAGS = flags.FLAGS


def main(argv):
  if len(argv) > 2:
    raise app.UsageError('Too many command-line arguments.')

  with open(argv[1], 'rb') as f:
    area_model = estimator_model.EstimatorModel(
        text_format.Parse(f.read(), estimator_model_pb2.EstimatorModel())
    )

  one_bit_register_area = area_model_utils.get_one_bit_register_area(area_model)

  env = jinja2.Environment(undefined=jinja2.StrictUndefined)
  tmpl_text = runfiles.get_contents_as_text(
      'xls/estimators/area_model/generate_area_lookup.tmpl',
  )
  template = env.from_string(tmpl_text)
  rendered = template.render(
      area_model=area_model,
      name=FLAGS.model_name,
      camel_case_name=''.join(
          s.capitalize() for s in FLAGS.model_name.split('_')
      ),
      one_bit_register_area=one_bit_register_area,
  )
  print(
      '// DO NOT EDIT: this file is AUTOMATICALLY GENERATED by'
      f' generate_area_lookup.py from {argv[1]} and should not be changed.'
  )
  print(rendered)


if __name__ == '__main__':
  app.run(main)
