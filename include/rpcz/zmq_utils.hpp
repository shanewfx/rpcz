// Copyright 2011 Google Inc. All Rights Reserved.
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
//
// Author: nadavs@google.com <Nadav Samet>

#ifndef RPCZ_ZMQ_UTILS_H
#define RPCZ_ZMQ_UTILS_H

#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include "zmq.hpp"
#include "rpcz/macros.hpp"

namespace zmq {
class socket_t;
class message_t;
}

namespace rpcz {

class message_iterator {
 public:
  explicit message_iterator(zmq::socket_t& socket) :
      socket_(socket), has_more_(true) {}

  message_iterator(const message_iterator& other) :
      socket_(other.socket_),
      has_more_(other.has_more_) {}

  ~message_iterator() {
    while (has_more()) next();
  }

  inline bool has_more() { return has_more_; }

  inline zmq::message_t& next() {
    socket_.recv(&message_, 0);
    has_more_ = message_.more();
    return message_;
  }

 private:
  zmq::socket_t& socket_;
  zmq::message_t message_;
  bool has_more_;

  message_iterator& operator=(const message_iterator&);
};

class message_vector {
 public:
  zmq::message_t& operator[](int index) {
    return *data_[index];
  }

  size_t size() const { return data_.size(); }

  // transfers points in the the range [from, to) from the other
  // message_vector to the beginning of this messsage vector.
  void transfer(size_t from, size_t to, message_vector& other) {
    data_.insert(data_.begin(),
                 std::make_move_iterator(other.data_.begin() + from),
                 std::make_move_iterator(other.data_.begin() + to));
    other.data_.erase(other.data_.begin() + from, other.data_.begin() + to);
  }

  void push_back(zmq::message_t* msg) { data_.emplace_back(msg); }

  void erase_first() { data_.erase(data_.begin()); }

  zmq::message_t* release(int index) {
    return data_[index].release(); }

 private:
  typedef std::vector<std::unique_ptr<zmq::message_t> > DataType;

  DataType data_;
};

bool read_message_to_vector(zmq::socket_t* socket,
                         message_vector* data);

bool read_message_to_vector(zmq::socket_t* socket,
                         message_vector* routes,
                         message_vector* data);

void write_vector_to_socket(zmq::socket_t* socket,
                         message_vector& data,
                         int flags=0);

void write_vectors_to_socket(zmq::socket_t* socket,
                          message_vector& routes,
                          message_vector& data);

std::string message_to_string(zmq::message_t& msg);

zmq::message_t* string_to_message(const std::string& str);

bool send_empty_message(zmq::socket_t* socket,
                      int flags=0);

bool send_string(zmq::socket_t* socket,
                const std::string& str,
                int flags=0);

bool send_uint64(zmq::socket_t* socket,
                uint64 value,
                int flags=0);

bool forward_message(zmq::socket_t &socket_in,
                    zmq::socket_t &socket_out);

template<typename T, typename Message>
inline T& interpret_message(Message& msg) {
  assert(msg.size() == sizeof(T));
  T &t = *static_cast<T*>(msg.data());
  return t;
}

template<typename T>
inline bool send_pointer(zmq::socket_t* socket, T* ptr, int flags=0) {
  zmq::message_t msg(sizeof(T*));
  memcpy(msg.data(), &ptr, sizeof(T*));
  return socket->send(msg, flags);
}

namespace internal {
template<typename T>
void delete_t(void* data, void* hint) {
  delete (T*)data;
}
}

template<typename T>
inline bool send_object(zmq::socket_t* socket, const T& object, int flags=0) {
  T* clone = new T(object);
  zmq::message_t msg(clone, sizeof(*clone), &rpcz::internal::delete_t<T>);
  return socket->send(msg, flags);
}

inline bool send_char(zmq::socket_t* socket, char ch, int flags=0) {
  zmq::message_t msg(1);
  *(char*)msg.data() = ch;
  return socket->send(msg, flags);
}

void log_message_vector(message_vector& vector);

inline void forward_messages(message_iterator& iter, zmq::socket_t& socket) {
  while (iter.has_more()) {
    zmq::message_t& msg = iter.next();
    socket.send(msg, iter.has_more() ? ZMQ_SNDMORE : 0);
  }
}
}  // namespace rpcz
#endif
