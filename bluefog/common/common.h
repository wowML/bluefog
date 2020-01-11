#ifndef BLUEFOG_COMMON_H
#define BLUEFOG_COMMON_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace bluefog {
namespace common {

// Device ID used for CPU.
#define CPU_DEVICE_ID (-1)

// List of supported frameworks.
enum class Framework { TENSORFLOW, PYTORCH };

enum class StatusType {
  OK,
  UNKNOWN_ERROR,
  PRECONDITION_ERROR,
  ABORTED,
  INVALID_ARGUMENT,
  IN_PROGRESS
};

enum class DeviceType { CPU, GPU };

enum class Communicator {
  GLOBAL = 0,
  LOCAL = 1,
  CROSS = 2,
  GRAPH = 3,
};

enum class DataType {
  BLUEFOG_UINT8 = 0,
  BLUEFOG_INT8 = 1,
  BLUEFOG_UINT16 = 2,
  BLUEFOG_INT16 = 3,
  BLUEFOG_INT32 = 4,
  BLUEFOG_INT64 = 5,
  BLUEFOG_FLOAT16 = 6,
  BLUEFOG_FLOAT32 = 7,
  BLUEFOG_FLOAT64 = 8,
  BLUEFOG_BOOL = 9,
  BLUEFOG_BYTE = 10,
};

enum class MPIOpsType {
  UNKNOWN = 0,
  ALLREDUCE = 1,
  ALLGATHER = 2,
  BROADCAST = 3,
  NEIGHBOR_ALLREDUCE = 4,
  NEIGHBOR_ALLGATHER = 5,
  WIN_PUT = 6,
  WIN_GET = 7,
  WIN_ACCUMULATE = 8,
};

inline std::string CommunicatorName(Communicator comm) {
  switch (comm) {
    case Communicator::GLOBAL:
      return "global";
    case Communicator::LOCAL:
      return "local";
    case Communicator::CROSS:
      return "cross";
    case Communicator::GRAPH:
      return "graph";
    default:
      return "<unknown>";
  }
}

const std::string& DataType_Name(DataType value);

class Status {
 public:
  Status();
  static Status OK();
  static Status UnknownError(std::string message);
  static Status PreconditionError(std::string message);
  static Status Aborted(std::string message);
  static Status InvalidArgument(std::string message);
  static Status InProgress();
  bool ok() const;
  bool in_progress() const;
  StatusType type() const;
  const std::string& reason() const;

 private:
  StatusType type_ = StatusType::OK;
  std::string reason_ = "";
  Status(StatusType type, std::string reason);
};

// Common error status
const Status NOT_INITIALIZED_ERROR = Status::PreconditionError(
    "Bluefog has not been initialized; use bf.init().");

const Status SHUT_DOWN_ERROR = Status::UnknownError(
    "Bluefog has been shut down. This was caused by an exception on one of the "
    "ranks or an attempt to allreduce, allgather or broadcast a tensor after "
    "one of the ranks finished execution. If the shutdown was caused by an "
    "exception, you should see the exception in the log before the first "
    "shutdown message.");

const Status DUPLICATE_NAME_ERROR = Status::InvalidArgument(
    "Requested to allreduce, allgather, or broadcast a tensor with the same "
    "name as another tensor that is currently being processed.  If you want "
    "to request another tensor, use a different tensor name.");

class TensorShape {
 public:
  void AddDim(int64_t dim);
  void AppendShape(TensorShape& other);

  const std::string DebugString() const;
  int dims() const;
  int64_t dim_size(int idx) const;
  int64_t num_elements() const;
  const std::vector<int64_t>& to_vector() const;

  inline bool operator==(const TensorShape& rhs) const {
    return shape_ == rhs.shape_;
  }

  inline bool operator!=(const TensorShape& rhs) const {
    return shape_ != rhs.shape_;
  }

 private:
  std::vector<int64_t> shape_;
};

class Tensor {
 public:
  virtual const DataType dtype() const = 0;
  virtual const TensorShape shape() const = 0;
  virtual const void* data() const = 0;
  virtual int64_t size() const = 0;
  virtual ~Tensor() = default;
};

// ReadyEvent is used to inform the CPU when GPU is used.
class ReadyEvent {
public:
  virtual bool Ready() const = 0;
  virtual ~ReadyEvent() = default;
};

class OpContext;

class PersistentBuffer {
 public:
  virtual const void* AccessData(std::shared_ptr<OpContext> context) const = 0;
  virtual ~PersistentBuffer() = default;
};

class OpContext {
 public:
  // These allocators are fully synchronous, unlike TensorFlow counterparts.
  virtual Status AllocatePersistent(
      int64_t size, std::shared_ptr<PersistentBuffer>* tensor) = 0;
  virtual Status AllocateOutput(TensorShape shape,
                                std::shared_ptr<Tensor>* tensor) = 0;
  virtual Status AllocateZeros(int64_t num_elements, DataType dtype,
                               std::shared_ptr<Tensor>* tensor) = 0;
  virtual Framework framework() const = 0;
  virtual ~OpContext() = default;
};

// A callback to call after the communication completes. Since the
// allreduce and allgather ops are asynchronous, this callback is what resumes
// computation after the reduction is completed.
using StatusCallback = std::function<void(const Status&)>;

// Table storing Tensors to be reduced, keyed by unique name.
// This table contains everything necessary to do the reduction.
struct TensorTableEntry {
  // Name of the tensor.
  std::string tensor_name;
  // Input tensor.
  std::shared_ptr<Tensor> tensor;
  // Pre-allocated output tensor.
  std::shared_ptr<Tensor> output;
  // Operation context, used for allocation.
  std::shared_ptr<OpContext> context;
  // Type of MPI operations, such as ALLREDUCE, BROADCAST etc.
  MPIOpsType mpi_ops_type;
  // Root rank for broadcast operation.
  int root_rank = -1;
  // GPU to do reduction on, or CPU_DEVICE_ID in case of CPU.
  int device = CPU_DEVICE_ID;
  // Source and destination of ranks used in get and put ops.
  std::vector<int> dst_ranks = {};
  std::vector<int> src_ranks = {};
  // A callback to call with the status.
  StatusCallback callback;
};
using TensorTable = std::unordered_map<std::string, TensorTableEntry>;

}  // namespace common
}  // namespace bluefog

#endif  // BLUEFOG_COMMON_H