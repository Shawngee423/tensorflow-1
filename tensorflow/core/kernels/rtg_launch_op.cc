/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifdef TENSORFLOW_USE_ROCM
#include "tensorflow/core/kernels/rtg_launch_op.h"

#include "tensorflow/core/common_runtime/dma_helper.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"
#include "tensorflow/core/util/stream_executor_util.h"

namespace gpu = perftools::gputools;

namespace tensorflow {

RTGLaunchOp::RTGLaunchOp(OpKernelConstruction* ctx) : OpKernel(ctx) {
  const NameAttrList* func;
  OP_REQUIRES_OK(ctx, ctx->GetAttr("function", &func));
  function_ = *func;
}

void RTGLaunchOp::Compute(OpKernelContext* ctx) {
  VLOG(1) << "RTGLaunchOp::Compute ";
  gpu::Stream* stream =
      ctx->op_device_context() ? ctx->op_device_context()->stream() : nullptr;

    // Execute the computation.
    VLOG(2) << "Executing computation.";

#if 0    
    auto start_time = env->NowMicros();
    auto run_result = executable->Run(arg_ptrs, run_options);
    OP_REQUIRES(ctx, run_result.ok(), run_result.status());
    output = std::move(run_result.ValueOrDie());
    auto elapsed = env->NowMicros() - start_time;
    VLOG(2) << "Elapsed time: " << elapsed << "us";

    // Computation output should always be a tuple.
    if (VLOG_IS_ON(2)) {
      VLOG(2) << "Result tuple shape: " << output->shape().DebugString();
    }

  // Copy XLA results to the OpOutputList.
  int output_num = 0;
  for (int i = 0; i < ctx->num_outputs(); ++i) {
    if (kernel->outputs[i].is_constant) {
      // Output is a constant
      const Tensor& const_tensor = kernel->outputs[i].constant_value;
      const size_t total_bytes = const_tensor.TotalBytes();
      if (stream && total_bytes > 0) {
        // Copy host -> device. (Empty tensors don't have backing buffers.)
        VLOG(1) << "Constant output tensor on device";
        Tensor* output_tensor;
        TF_CHECK_OK(
            ctx->allocate_output(i, const_tensor.shape(), &output_tensor));

        const void* src_ptr = DMAHelper::base(&const_tensor);
        void* dst_ptr = DMAHelper::base(output_tensor);
        gpu::DeviceMemoryBase gpu_dst_ptr(dst_ptr, total_bytes);
        stream->ThenMemcpy(&gpu_dst_ptr, src_ptr, total_bytes);
      } else {
        // No copy required.
        ctx->set_output(i, const_tensor);
      }
    } else {
      const TensorShape& shape = kernel->outputs[i].shape;
      VLOG(2) << "Retval " << i << " shape " << shape.DebugString();

      gpu::DeviceMemoryBase buffer;
      if (output_is_tuple) {
        buffer = output->buffer({output_num});
      } else {
        CHECK_EQ(0, output_num);
        buffer = output->buffer({});
      }
      Tensor output_tensor;
      // Looks up the owning Tensor by buffer address.
      OP_REQUIRES_OK(ctx, xla_allocator.MakeTensorFromBuffer(
                              buffer, ctx->expected_output_dtype(i), shape,
                              &output_tensor));
      ctx->set_output(i, output_tensor);
      ++output_num;
    }

    if (VLOG_IS_ON(3)) {
      VLOG(3) << ctx->mutable_output(i)->DebugString();
    }
  }
#endif
  VLOG(1) << "Done";
}

RTGLaunchOp::~RTGLaunchOp() {
  VLOG(1) << "RTGLaunchOp destroyed";
}

REGISTER_KERNEL_BUILDER(Name("RTGLaunchOp").Device(DEVICE_GPU), RTGLaunchOp);

}  // namespace tensorflow

#endif // TENSORFLOW_USE_ROCM
