// Copyright 2018 Xiaomi, Inc.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>

#include "mace/core/operator.h"

namespace mace {
namespace ops {

template <DeviceType D, class T>
class LocalResponseNormOp;

template <>
class LocalResponseNormOp<DeviceType::CPU, float> : public Operation {
 public:
  explicit LocalResponseNormOp(OpConstructContext *context)
      : Operation(context),
        depth_radius_(Operation::GetOptionalArg<int>("depth_radius", 5)),
        bias_(Operation::GetOptionalArg<float>("bias", 1.0f)),
        alpha_(Operation::GetOptionalArg<float>("alpha", 1.0f)),
        beta_(Operation::GetOptionalArg<float>("beta", 0.5f)) {}

  MaceStatus Run(OpContext *context) override {
    MACE_UNUSED(context);
    const Tensor *input = this->Input(0);

    MACE_CHECK(input->dim_size() == 4, "input must be 4-dimensional. ",
               input->dim_size());

    Tensor *output = this->Output(0);
    MACE_RETURN_IF_ERROR(output->ResizeLike(input));

    const index_t batch = input->dim(0);
    const index_t channels = input->dim(1);
    const index_t height = input->dim(2);
    const index_t width = input->dim(3);

    const float *input_ptr = input->data<float>();
    float *output_ptr = output->mutable_data<float>();

    index_t image_size = height * width;
    index_t batch_size = channels * image_size;

#pragma omp parallel for collapse(2) schedule(runtime)
    for (index_t b = 0; b < batch; ++b) {
      for (index_t c = 0; c < channels; ++c) {
        const int begin_input_c = std::max(static_cast<index_t>(0),
                                           c - depth_radius_);
        const int end_input_c = std::min(channels, c + depth_radius_ + 1);

        index_t pos = b * batch_size;
        for (index_t hw = 0; hw < height * width; ++hw, ++pos) {
          float accum = 0.f;
          for (int input_c = begin_input_c; input_c < end_input_c; ++input_c) {
            const float input_val = input_ptr[pos + input_c * image_size];
            accum += input_val * input_val;
          }
          const float multiplier = std::pow(bias_ + alpha_ * accum, -beta_);
          output_ptr[pos + c * image_size] =
              input_ptr[pos + c * image_size] * multiplier;
        }
      }
    }

    return MaceStatus::MACE_SUCCESS;
  }

 private:
  int depth_radius_;
  float bias_;
  float alpha_;
  float beta_;
};

void RegisterLocalResponseNorm(OpRegistryBase *op_registry) {
  MACE_REGISTER_OP(op_registry, "LocalResponseNorm",
                   LocalResponseNormOp, DeviceType::CPU, float);
}

}  // namespace ops
}  // namespace mace
