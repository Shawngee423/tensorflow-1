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
#ifndef THIRD_PARTY_TENSORFLOW_CORE_KERNELS_RCCL_COMMUNICATOR_H_
#define THIRD_PARTY_TENSORFLOW_CORE_KERNELS_RCCL_COMMUNICATOR_H_

#if TENSORFLOW_USE_ROCM

#include <unordered_map>
#include <vector>

#include "external/rccl_archive/inc/rccl.h"
#include "tensorflow/core/common_runtime/gpu/gpu_event_mgr.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/stream_executor.h"

namespace tensorflow {

// The communicator is used to make the asynchronous communicator calls and to
// manage the per-device streams used for communication.
//
// See rccl_ops.cc for example usage, including description of memory
// management and stream synchronization.
class RcclManager {
 public:
  typedef std::function<void(Status)> DoneCallback;
  RcclManager();
  ~RcclManager();

  static RcclManager* instance();

  // Add one participant to an all-reduce, sending in data from <in_t> and
  // receiving the result of the all-reduce in <out_t>.  The device for this
  // participant is managed by <executor>, and its events are polled by
  // <event_mgr>.
  //
  // This is an asynchronous call. When <done_callback> is called, <out_t> has
  // been set to the all-reduce result (note: the stream may not yet have been
  // synced).
  //
  // <tensor_stream> is the stream that should be waited on to ensure <in_t>'s
  // data is available on the GPU for the communication stream to access. It
  // is also the stream that will use the produced data; <done_callback> is
  // not called until the next kernel launched on <stream> would see the data.
  void AddToAllReduce(int num_devices, const string& key,
                      rcclRedOp_t reduction_op,
                      perftools::gputools::StreamExecutor* executor,
                      int gpu_device_id, EventMgr* event_mgr,
                      perftools::gputools::Stream* tensor_stream,
                      const Tensor* in_t, Tensor* out_t,
                      const DoneCallback& done_callback);

  // AddBroadcastSend and AddBroadcastRecv combine to sent data from one sender
  // to all receivers.
  void AddBroadcastSend(int num_devices, const string& key,
                        perftools::gputools::StreamExecutor* executor,
                        int gpu_device_id, EventMgr* event_mgr,
                        perftools::gputools::Stream* tensor_stream,
                        const Tensor* in_t, DoneCallback done_callback);
  void AddBroadcastRecv(int num_devices, const string& key,
                        perftools::gputools::StreamExecutor* executor,
                        int gpu_device_id, EventMgr* event_mgr,
                        perftools::gputools::Stream* tensor_stream,
                        Tensor* out_t, DoneCallback done_callback);

 private:
  enum CollectiveType {
    kAllReduce = 1,
    kBroadcast = 2,
  };
  struct Collective;
  struct Communicator;
  struct CommunicatorMember;
  struct RcclStream;
  struct Participant;

  Communicator* GetCommunicator(Collective* collective);

  void AddParticipant(int num_devices, const string& key,
                      std::unique_ptr<Participant> participant,
                      DataType data_type, CollectiveType collective_type,
                      rcclRedOp_t reduction_op);

  // Run <collective>.  This calls takes ownership of <collective>.
  void RunCollective(const string& key, Collective* collective);
  void LoopKernelLaunches(RcclStream* stream);

  mutex mu_;

  // Maps key to collectives currently being assembled or run.
  std::unordered_map<string, std::unique_ptr<Collective>> collectives_
      GUARDED_BY(mu_);

  // Maps a device to the communication streams that make up its collective.
  // This is used to share the stream across different communicators that
  // include the same device.
  std::map<perftools::gputools::StreamExecutor*,
           std::vector<std::unique_ptr<RcclStream>>>
      device_to_comm_streams_ GUARDED_BY(mu_);

  std::vector<std::unique_ptr<Communicator>> communicators_;

  TF_DISALLOW_COPY_AND_ASSIGN(RcclManager);
};

}  // namespace tensorflow

#endif  // TENSORFLOW_USE_ROCM

#endif  // THIRD_PARTY_TENSORFLOW_CORE_KERNELS_RCCL_COMMUNICATOR_H_
