/*
Copyright (c) 2019 Livio, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

* Neither the name of SmartDeviceLink Consortium, Inc. nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "transport_manager/websocket/websocket_connection.h"
#include <unistd.h>
#include "transport_manager/transport_adapter/transport_adapter_controller.h"

namespace transport_manager {
namespace transport_adapter {

using namespace boost::beast::websocket;

template <>
WebSocketConnection<tcp::socket>::WebSocketConnection(
    const DeviceUID& device_uid,
    const ApplicationHandle& app_handle,
    boost::asio::ip::tcp::socket socket,
    TransportAdapterController* controller)
    : device_uid_(device_uid)
    , app_handle_(app_handle)
    , ws_(std::move(socket))
    , strand_(ws_.get_executor())
    , controller_(controller)
    , shutdown_(false)
    , thread_delegate_(new LoopThreadDelegate(&message_queue_, this))
    , thread_(threads::CreateThread("WS Async Send", thread_delegate_)) {
  thread_->start(threads::ThreadOptions());
}

template <>
WebSocketConnection<ssl::stream<tcp::socket&> >::WebSocketConnection(
    const DeviceUID& device_uid,
    const ApplicationHandle& app_handle,
    boost::asio::ip::tcp::socket socket,
    ssl::context& ctx,
    TransportAdapterController* controller)
    : device_uid_(device_uid)
    , app_handle_(app_handle)
    , socket_(new tcp::socket(std::move(socket)))
    , ws_(*socket_.get(), ctx)
    , strand_(ws_.get_executor())
    , controller_(controller)
    , shutdown_(false)
    , thread_delegate_(new LoopThreadDelegate(&message_queue_, this))
    , thread_(threads::CreateThread("WS Async Send", thread_delegate_)) {
  thread_->start(threads::ThreadOptions());
}

template <class Layer>
WebSocketConnection<Layer>::~WebSocketConnection() {}

template <class Layer>
void WebSocketConnection<Layer>::Accept() {
  ws_.async_accept(
      boost::asio::bind_executor(strand_,
                                 std::bind(&WebSocketConnection::Recv,
                                           this->shared_from_this(),
                                           std::placeholders::_1)));
}

template <class Layer>
void WebSocketConnection<Layer>::Shutdown() {
  shutdown_ = true;
  thread_delegate_->SetShutdown();
  thread_->join();
  delete thread_delegate_;
  threads::DeleteThread(thread_);
}

template <class Layer>
bool WebSocketConnection<Layer>::IsShuttingDown() {
  return shutdown_;
}

template <class Layer>
void WebSocketConnection<Layer>::Recv(boost::system::error_code ec) {
  LOG4CXX_AUTO_TRACE(wsc_logger_);
  if (shutdown_) {
    return;
  }

  if (ec) {
    auto str_err = "ErrorMessage: " + ec.message();
    LOG4CXX_ERROR(wsc_logger_, str_err);
    shutdown_ = true;
    thread_delegate_->SetShutdown();
    // controller_->deleteController(this);
    return;
  }

  ws_.async_read(
      buffer_,
      boost::asio::bind_executor(strand_,
                                 std::bind(&WebSocketConnection::Read,
                                           this->shared_from_this(),
                                           std::placeholders::_1,
                                           std::placeholders::_2)));
}

template <class Layer>
TransportAdapter::Error WebSocketConnection<Layer>::SendData(
    ::protocol_handler::RawMessagePtr message) {
  if (shutdown_) {
    return TransportAdapter::BAD_STATE;
  }

  message_queue_.push(message);

  return TransportAdapter::OK;
}

template <class Layer>
void WebSocketConnection<Layer>::Read(boost::system::error_code ec,
                                      std::size_t bytes_transferred) {
  LOG4CXX_AUTO_TRACE(wsc_logger_);
  boost::ignore_unused(bytes_transferred);
  if (ec) {
    auto str_err = "ErrorMessage: " + ec.message();
    LOG4CXX_ERROR(wsc_logger_, str_err);
    shutdown_ = true;
    thread_delegate_->SetShutdown();
    // controller_->deleteController(this);
    buffer_.consume(buffer_.size());
    return;
  }

  auto size = (ssize_t)buffer_.size();
  const auto data = boost::asio::buffer_cast<const uint8_t*>(
      boost::beast::buffers_front(buffer_.data()));

  LOG4CXX_DEBUG(
      wsc_logger_,
      "Read Msg: " << boost::beast::buffers_to_string(buffer_.data()));

  ::protocol_handler::RawMessagePtr frame(
      new protocol_handler::RawMessage(0, 0, data, size, false));

  controller_->DataReceiveDone(device_uid_, app_handle_, frame);

  buffer_.consume(buffer_.size());
  Recv(ec);
}

template <class Layer>
WebSocketConnection<Layer>::LoopThreadDelegate::LoopThreadDelegate(
    MessageQueue<Message, AsyncQueue>* message_queue,
    WebSocketConnection* handler)
    : message_queue_(*message_queue), handler_(*handler), shutdown_(false) {}

template <class Layer>
void WebSocketConnection<Layer>::LoopThreadDelegate::threadMain() {
  while (!message_queue_.IsShuttingDown() && !shutdown_) {
    DrainQueue();
    message_queue_.wait();
  }
  DrainQueue();
}

template <class Layer>
void WebSocketConnection<Layer>::LoopThreadDelegate::exitThreadMain() {
  shutdown_ = true;
  if (!message_queue_.IsShuttingDown()) {
    message_queue_.Shutdown();
  }
}

template <class Layer>
void WebSocketConnection<Layer>::LoopThreadDelegate::DrainQueue() {
  Message message_ptr;
  while (!shutdown_ && message_queue_.pop(message_ptr)) {
    boost::system::error_code ec;
    handler_.ws_.write(
        boost::asio::buffer(message_ptr->data(), message_ptr->data_size()), ec);
    if (ec) {
      LOG4CXX_ERROR(wsc_logger_,
                    "A system error has occurred: " << ec.message());
    }
  }
}

template <class Layer>
void WebSocketConnection<Layer>::LoopThreadDelegate::SetShutdown() {
  shutdown_ = true;
  if (!message_queue_.IsShuttingDown()) {
    message_queue_.Shutdown();
  }
}

}  // namespace transport_adapter
}  // namespace transport_manager
