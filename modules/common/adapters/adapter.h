/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 */

#ifndef MODULES_ADAPTERS_ADAPTER_H_
#define MODULES_ADAPTERS_ADAPTER_H_

#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "glog/logging.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

#include "modules/common/adapters/adapter_gflags.h"
#include "modules/common/proto/header.pb.h"
#include "modules/common/time/time.h"
#include "modules/common/util/file.h"
#include "modules/common/util/string_util.h"
#include "modules/common/util/util.h"

#include "sensor_msgs/CompressedImage.h"
#include "sensor_msgs/PointCloud2.h"

/**
 * @namespace apollo::common::adapter
 * @brief apollo::common::adapter
 */
namespace apollo {
namespace common {
namespace adapter {

// Borrowed from C++ 14.
template <bool B, class T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

/**
 * @class Adapter
 * @brief this class serves as the interface and a layer of
 * abstraction for Apollo modules to interact with various I/O (e.g.
 * ROS). The adapter will also store history data, so that other
 * Apollo modules can get access to both the current and the past data
 * without having to handle communication protocols directly.
 *
 * \par
 * Each \class Adapter instance only works with one single topic and
 * its corresponding data type.
 *
 * \par
 * Under the hood, a queue is used to store the current and historical
 * messages. In most cases, the underlying data type is a proto, though
 * this is not necessary.
 *
 * \note
 * Adapter::Observe() is thread-safe, but calling it from
 * multiple threads may introduce unexpected behavior. Adapter is
 * thread-safe w.r.t. data access and update.
 */
template <typename D>
class Adapter {
 public:
  /// The user can use Adapter::DataType to get the type of the
  /// underlying data.
  typedef D DataType;

  typedef typename std::list<std::shared_ptr<D>>::const_iterator Iterator;
  typedef typename std::function<void(const D&)> Callback;

  /**
   * @brief Construct the \class Adapter object.
   * @param adapter_name the name of the adapter. It is used to log
   * error messages when something bad happens, to help people get an
   * idea which adapter goes wrong.
   * @param topic_name the topic that the adapter listens to.
   * @param message_num the number of historical messages that the
   * adapter stores. Older messages will be removed upon calls to
   * Adapter::OnReceive().
   */
  Adapter(const std::string& adapter_name, const std::string& topic_name,
          size_t message_num, const std::string& dump_dir = "/tmp")
      : topic_name_(topic_name),
        message_num_(message_num),
        enable_dump_(FLAGS_enable_adapter_dump),
        dump_path_(dump_dir + "/" + adapter_name) {
    if (HasSequenceNumber<D>()) {
      if (!apollo::common::util::EnsureDirectory(dump_path_)) {
        AERROR << "Cannot enable dumping for '" << adapter_name
               << "' adapter because the path " << dump_path_
               << " cannot be created or is not a directory.";
        enable_dump_ = false;
      } else if (!apollo::common::util::RemoveAllFiles(dump_path_)) {
        AERROR << "Cannot enable dumping for '" << adapter_name
               << "' adapter because the path " << dump_path_
               << " contains files that cannot be removed.";
        enable_dump_ = false;
      }
    } else {
      enable_dump_ = false;
    }
  }

  /**
   * @brief returns the topic name that this adapter listens to.
   */
  const std::string& topic_name() const {
    return topic_name_;
  }

  /**
   * @brief reads the proto message from the file, and push it into
   * the adapter's data queue.
   * @param message_file the path to the file that contains a (usually
   * proto) message of DataType.
   */
  template <class T = D>
  bool FeedFile(const std::string& message_file) {
    return FeedFile(message_file, IdentifierType<T>());
  }

  /**
   * @brief push (a copy of) the input data into the data queue of
   * the adapter.
   * @param data the input data.
   */
  void FeedData(const D& data) {
    EnqueueData(data);
  }

  /**
   * @brief the callback that will be invoked whenever a new
   * message is received.
   * @param message the newly received message.
   */
  void OnReceive(const D& message) {
    UpdateDelay(message);
    EnqueueData(message);
    FireCallbacks(message);
  }

  /**
   * @brief copy the data_queue_ into the observing queue to create a
   * view of data up to the call time for the user.
   */
  void Observe() {
    std::lock_guard<std::mutex> lock(mutex_);
    observed_queue_ = data_queue_;
  }

  /**
   * @brief returns TRUE if the observing queue is empty.
   */
  bool Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return observed_queue_.empty();
  }

  /**
   * @brief returns TRUE if the adapter has received any message.
   */
  bool HasReceived() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !data_queue_.empty();
  }

  /**
   * @brief returns the most recent message in the observing queue.
   *
   * /note
   * Please call Empty() to make sure that there is data in the
   * queue before calling GetOldestObserved().
   */
  const D& GetLatestObserved() const {
    std::lock_guard<std::mutex> lock(mutex_);
    DCHECK(!observed_queue_.empty())
        << "The view of data queue is empty. No data is received yet or you "
           "forgot to call Observe()"
        << ":" << topic_name_;
    return *observed_queue_.front();
  }

  /**
   * @brief returns the oldest message in the observing queue.
   *
   * /note
   * Please call Empty() to make sure that there is data in the
   * queue before calling GetOldestObserved().
   */
  const D& GetOldestObserved() const {
    std::lock_guard<std::mutex> lock(mutex_);
    DCHECK(!observed_queue_.empty())
        << "The view of data queue is empty. No data is received yet or you "
           "forgot to call Observe().";
    return *observed_queue_.back();
  }

  /**
   * @brief returns an iterator representing the head of the observing
   * queue. The caller can use it to iterate over the observed data
   * from the head. The API also supports range based for loop.
   */
  Iterator begin() const {
    return observed_queue_.begin();
  }

  /**
   * @brief returns an iterator representing the tail of the observing
   * queue. The caller can use it to iterate over the observed data
   * from the head. The API also supports range based for loop.
   */
  Iterator end() const {
    return observed_queue_.end();
  }

  /**
   * @brief registers the provided callback function to the adapter,
   * so that the callback function will be called once right after the
   * message hits the adapter.
   * @param callback the callback with signature void(const D &).
   */
  void AddCallback(Callback callback) {
    receive_callbacks_.push_back(callback);
  }

  /**
   * @brief Pops out the latest added callback.
   * @return false if there's no callback to pop out, true otherwise.
   */
  bool PopCallback() {
    if (receive_callbacks_.empty()) {
      return false;
    }
    receive_callbacks_.pop_back();
    return true;
  }

  /**
   * @brief fills the fields module_name, timestamp_sec and
   * sequence_num in the header.
   */
  void FillHeader(const std::string& module_name, D* data) {
    static_assert(std::is_base_of<google::protobuf::Message, D>::value,
                  "Can only fill header to proto messages!");
    auto* header = data->mutable_header();
    double timestamp = apollo::common::time::Clock::NowInSecond();
    header->set_module_name(module_name);
    header->set_timestamp_sec(timestamp);
    header->set_sequence_num(++seq_num_);
  }

  uint32_t GetSeqNum() const {
    return seq_num_;
  }

  void SetLatestPublished(const D& data) {
    latest_published_data_.reset(new D(data));
  }

  const D* GetLatestPublished() {
    return latest_published_data_.get();
  }

  /**
   * @brief Gets message delay in milliseconds.
   */
  double GetDelayInMs() {
    return delay_ms_;
  }

  /**
   * @brief Clear the data received so far.
   */
  void ClearData() {
    // Lock the queue.
    std::lock_guard<std::mutex> lock(mutex_);
    data_queue_.clear();
    observed_queue_.clear();
  }

  /**
   * @brief Dumps the latest received data to file.
   */
  bool DumpLatestMessage() {
    if (!Empty()) {
      D msg = GetLatestObserved();
      return DumpMessage<D>(msg);
    }

    AWARN << "Unable to dump message with topic " << topic_name_
          << ". No message received.";
    return false;
  }

 private:
  template <typename T>
  struct IdentifierType {};

  template <class T>
  bool FeedFile(const std::string& message_file, IdentifierType<T>) {
    D data;
    if (!apollo::common::util::GetProtoFromFile(message_file, &data)) {
      AERROR << "Unable to parse input pb file " << message_file;
      return false;
    }
    FeedData(data);
    return true;
  }
  bool FeedFile(const std::string& message_file,
                IdentifierType<::sensor_msgs::PointCloud2>) {
    return false;
  }
  bool FeedFile(const std::string& message_file,
                IdentifierType<::sensor_msgs::CompressedImage>) {
    return false;
  }

  // HasSequenceNumber returns false for non-proto-message data types.
  template <typename InputMessageType>
  static bool HasSequenceNumber(
      enable_if_t<
          !std::is_base_of<google::protobuf::Message, InputMessageType>::value,
          InputMessageType>* message = nullptr) {
    return false;
  }

  // HasSequenceNumber returns true if the data type is proto and has
  // header.sequence_num.
  template <typename InputMessageType>
  static bool HasSequenceNumber(
      enable_if_t<
          std::is_base_of<google::protobuf::Message, InputMessageType>::value,
          InputMessageType>* message = nullptr) {
    using gpf = google::protobuf::FieldDescriptor;
    InputMessageType sample;
    auto descriptor = sample.GetDescriptor();
    auto header_descriptor = descriptor->FindFieldByName("header");
    if (header_descriptor == nullptr ||
        header_descriptor->cpp_type() != gpf::CPPTYPE_MESSAGE) {
      return false;
    }

    auto sequence_num_descriptor =
        header_descriptor->message_type()->FindFieldByName("sequence_num");
    return sequence_num_descriptor != nullptr &&
           sequence_num_descriptor->cpp_type() == gpf::CPPTYPE_UINT32;
  }

  // DumpMessage does nothing for non proto message data type.
  template <typename InputMessageType>
  bool DumpMessage(const enable_if_t<!std::is_base_of<google::protobuf::Message,
                                                      InputMessageType>::value,
                                     InputMessageType>& message) {
    return true;
  }

  // DumpMessage dumps the message to a file to
  // /tmp/<adapter_name>/<name>.pb.txt, where the message is in ASCII
  // mode and <name> is the .header().sequence_num() of the message.
  template <typename InputMessageType>
  bool DumpMessage(const enable_if_t<std::is_base_of<google::protobuf::Message,
                                                     InputMessageType>::value,
                                     InputMessageType>& message) {
    using google::protobuf::Message;
    auto descriptor = message.GetDescriptor();
    auto header_descriptor = descriptor->FindFieldByName("header");
    if (header_descriptor == nullptr) {
      ADEBUG << "Fail to find header field in pb.";
      return false;
    }
    const Message& header = message.GetReflection()->GetMessage(
        *static_cast<const Message*>(&message), header_descriptor);
    auto seq_num_descriptor =
        header.GetDescriptor()->FindFieldByName("sequence_num");
    if (seq_num_descriptor == nullptr) {
      ADEBUG << "Fail to find sequence_num field in pb.";
      return false;
    }
    uint32_t sequence_num =
        header.GetReflection()->GetUInt32(header, seq_num_descriptor);
    return util::SetProtoToASCIIFile(
        message, util::StrCat(dump_path_, "/", sequence_num, ".pb.txt"));
  }

  /**
   * @brief proactively invokes the callbacks one by one registered with the
   * specified data.
   * @param data the specified data.
   */
  void FireCallbacks(const D& data) {
    for (const auto& callback : receive_callbacks_) {
      callback(data);
    }
  }

  /**
   * @brief push the shared-pointer-guarded data to the data queue of
   * the adapter.
   */
  void EnqueueData(const D& data) {
    if (enable_dump_) {
      DumpMessage<D>(data);
    }

    // Don't try to copy data and enqueue if the message_num is 0
    if (message_num_ == 0) {
      return;
    }

    // Lock the queue.
    std::lock_guard<std::mutex> lock(mutex_);
    if (data_queue_.size() + 1 > message_num_) {
      data_queue_.pop_back();
    }
    data_queue_.push_front(std::make_shared<D>(data));
  }

  /**
   * @brief Calculates message delay based on message type.
   */
  double CalculateDelayInMs(const D& new_msg, const D& last_msg) {
    return MessageDelay<D>::Get(new_msg, last_msg);
  }

  /**
   * @brief Updates the message delay upon receiving a new message.
   */
  void UpdateDelay(const D& new_msg) {
    if (!data_queue_.empty()) {
      std::lock_guard<std::mutex> lock(mutex_);
      delay_ms_ = CalculateDelayInMs(new_msg, *data_queue_.front());
    }
  }

  /// A few partial template specialzations to get message delays for different
  /// message types.
  template <class T, class Enable = void>
  struct MessageDelay {
    static double Get(const T& new_msg, const T& last_msg) {
      return 0.0;
    }
  };

  template <class T>
  struct MessageDelay<
      T, enable_if_t<std::is_base_of<google::protobuf::Message, T>::value>> {
    static double Get(const T& new_msg, const T& last_msg) {
      using google::protobuf::Message;

      return (ExtractTimeStampFromMsg(static_cast<const Message&>(new_msg)) -
              ExtractTimeStampFromMsg(static_cast<const Message&>(last_msg))) *
             1000.0;
    }

    static double ExtractTimeStampFromMsg(
        const google::protobuf::Message& message) {
      using gpf = google::protobuf::FieldDescriptor;

      auto descriptor = message.GetDescriptor();
      auto header_descriptor = descriptor->FindFieldByName("header");

      if (header_descriptor == nullptr ||
          header_descriptor->cpp_type() != gpf::CPPTYPE_MESSAGE) {
        return 0.0;
      }

      auto timestamp_sec_descriptor =
          header_descriptor->message_type()->FindFieldByName("timestamp_sec");
      if (timestamp_sec_descriptor == nullptr ||
          timestamp_sec_descriptor->cpp_type() != gpf::CPPTYPE_DOUBLE) {
        return 0.0;
      }

      const auto& header =
          message.GetReflection()->GetMessage(message, header_descriptor);
      return header.GetReflection()->GetDouble(header,
                                               timestamp_sec_descriptor);
    }
  };

  template <class Enable>
  struct MessageDelay<sensor_msgs::PointCloud2, Enable> {
    static double Get(const sensor_msgs::PointCloud2& new_msg,
                      const sensor_msgs::PointCloud2& last_msg) {
      return (new_msg.header.stamp - last_msg.header.stamp).sec * 1000.0;
    }
  };

  /// The topic name that the adapter listens to.
  std::string topic_name_;

  /// The maximum size of data_queue_ and observed_queue_
  size_t message_num_ = 0;

  /// The received data. Its size is no more than message_num_
  std::list<std::shared_ptr<D>> data_queue_;

  /// It is the snapshot of the data queue. The snapshot is taken when
  /// Observe() is called.
  std::list<std::shared_ptr<D>> observed_queue_;

  /// User defined function when receiving a message
  std::vector<Callback> receive_callbacks_;

  /// The mutex guarding data_queue_ and observed_queue_
  mutable std::mutex mutex_;

  /// Whether dumping is enabled.
  bool enable_dump_ = false;

  /// The directory of dumped files.
  std::string dump_path_;

  /// The monotonically increasing sequence number of the message to
  /// be published.
  uint32_t seq_num_ = 0;

  /// The most recenct published data.
  std::unique_ptr<D> latest_published_data_;

  /// The interval between receiving two consecutive messages.
  double delay_ms_ = std::numeric_limits<double>::quiet_NaN();
};

}  // namespace adapter
}  // namespace common
}  // namespace apollo

#endif  // MODULES_ADAPTERS_ADAPTER_H_
