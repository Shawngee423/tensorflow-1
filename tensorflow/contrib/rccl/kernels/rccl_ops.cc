/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#if TENSORFLOW_USE_ROCM

#include <unordered_map>
#include <vector>

#include "external/rccl_archive/inc/rccl.h"
#include "tensorflow/contrib/rccl/kernels/rccl_manager.h"
#include "tensorflow/core/framework/op_kernel.h"

namespace tensorflow {

// Base class for all communicator ops that use rccl.
//
// About memory management and stream syncing:
// 1. The rccl communicator has a stream for each rank.
// 2. For input tensors to the communicator, the compute stream is passed to the
//    RcclManager which will do a needed
//    communicator_stream.ThenWaitFor(input_tensor_stream).
// 3. The done_callback of the async kernel is not called by the
//    RcclManager until after the communicator kernel is complete. This
//    is enough to a) keep the input tensor data valid for the lifetime of the
//    collective; and b) ensure the data in the output tensor is available
//    when the async op kernel's done callback is called.
class RcclAsyncOpBase : public AsyncOpKernel {
 public:
  explicit RcclAsyncOpBase(OpKernelConstruction* c) : AsyncOpKernel(c) {
    OP_REQUIRES_OK(c, c->GetAttr("num_devices", &num_devices_));
    OP_REQUIRES_OK(c, c->GetAttr("shared_name", &collective_prefix_));
  }

  string GetCollectiveKey(OpKernelContext* c) {
    return strings::StrCat(collective_prefix_, ";", c->step_id(), ";",
                           c->frame_iter().frame_id, ":",
                           c->frame_iter().iter_id);
  }

  int num_devices() const { return num_devices_; }

 private:
  int num_devices_;
  string collective_prefix_;

  TF_DISALLOW_COPY_AND_ASSIGN(RcclAsyncOpBase);
};

// To execute a single all-reduce, this kernel is called once for each of the
// <k> devices in the communicator.
class RcclAllReduceOpKernel : public RcclAsyncOpBase {
 public:
  explicit RcclAllReduceOpKernel(OpKernelConstruction* c) : RcclAsyncOpBase(c) {
    string reduction;
    OP_REQUIRES_OK(c, c->GetAttr("reduction", &reduction));
    if (reduction == "min") {
      reduction_op_ = rcclMin;
    } else if (reduction == "max") {
      reduction_op_ = rcclMax;
    } else if (reduction == "sum") {
      reduction_op_ = rcclSum;
    } else if (reduction == "prod") {
      reduction_op_ = rcclProd;
    } else {
      OP_REQUIRES_OK(c,
                     errors::InvalidArgument("Invalid reduction: ", reduction));
    }
  }

  void ComputeAsync(OpKernelContext* c, DoneCallback done) override {
    const Tensor* in_t = &c->input(0);
    Tensor* out_t;
    OP_REQUIRES_OK_ASYNC(c, c->allocate_output(0, in_t->shape(), &out_t), done);

    auto actual_done = [c, done](Status s) {
      OP_REQUIRES_OK_ASYNC(c, s, done);
      done();
    };

    auto* compute_stream = c->op_device_context()->stream();
    auto* gpu_info = c->device()->tensorflow_gpu_device_info();
    RcclManager::instance()->AddToAllReduce(
        num_devices(), GetCollectiveKey(c), reduction_op_,
        compute_stream->parent(), gpu_info->gpu_id, gpu_info->event_mgr,
        compute_stream, in_t, out_t, actual_done);
  }

 private:
  rcclRedOp_t reduction_op_;
};

REGISTER_KERNEL_BUILDER(Name("RcclAllReduce").Device(DEVICE_GPU),
                        RcclAllReduceOpKernel);

class RcclBroadcastSendKernel : public RcclAsyncOpBase {
 public:
  explicit RcclBroadcastSendKernel(OpKernelConstruction* c)
      : RcclAsyncOpBase(c) {}

  void ComputeAsync(OpKernelContext* c, DoneCallback done) override {
    auto actual_done = [c, done](Status s) {
      OP_REQUIRES_OK_ASYNC(c, s, done);
      done();
    };

    auto* compute_stream = c->op_device_context()->stream();
    auto* gpu_info = c->device()->tensorflow_gpu_device_info();
    RcclManager::instance()->AddBroadcastSend(
        num_devices(), GetCollectiveKey(c), compute_stream->parent(),
        gpu_info->gpu_id, gpu_info->event_mgr, compute_stream, &c->input(0),
        std::move(actual_done));
  }
};
REGISTER_KERNEL_BUILDER(Name("RcclBroadcastSend").Device(DEVICE_GPU),
                        RcclBroadcastSendKernel);

class RcclBroadcastRecvKernel : public RcclAsyncOpBase {
 public:
  explicit RcclBroadcastRecvKernel(OpKernelConstruction* c)
      : RcclAsyncOpBase(c) {}

  void ComputeAsync(OpKernelContext* c, DoneCallback done) override {
    const Tensor& shape_t = c->input(0);
    TensorShape shape;
    OP_REQUIRES_OK_ASYNC(
        c, TensorShapeUtils::MakeShape(shape_t.vec<int64>(), &shape), done);
    Tensor* out_t;
    OP_REQUIRES_OK_ASYNC(c, c->allocate_output(0, shape, &out_t), done);

    auto actual_done = [c, done](Status s) {
      OP_REQUIRES_OK_ASYNC(c, s, done);
      done();
    };

    auto* compute_stream = c->op_device_context()->stream();
    auto* gpu_info = c->device()->tensorflow_gpu_device_info();
    RcclManager::instance()->AddBroadcastRecv(
        num_devices(), GetCollectiveKey(c), compute_stream->parent(),
        gpu_info->gpu_id, gpu_info->event_mgr, compute_stream, out_t,
        std::move(actual_done));
  }
};
REGISTER_KERNEL_BUILDER(
    Name("RcclBroadcastRecv").Device(DEVICE_GPU).HostMemory("shape"),
    RcclBroadcastRecvKernel);

}  // namespace tensorflow

#endif  // TENSORFLOW_USE_ROCM
