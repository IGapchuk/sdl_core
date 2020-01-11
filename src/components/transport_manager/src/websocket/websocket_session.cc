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

#include "transport_manager/websocket/websocket_session.h"
#include <unistd.h>
#include "transport_manager/transport_adapter/transport_adapter_controller.h"

namespace transport_manager {
namespace transport_adapter {

using namespace boost::beast::websocket;

template <>
WebSocketSession<tcp::socket&>::WebSocketSession(
    boost::asio::ip::tcp::socket socket, DataReceiveCallback dataReceive)
    : socket_(std::move(socket))
    , ws_(socket_)
    , strand_(ws_.get_executor())
    , dataReceive_(dataReceive) {
  ws_.binary(true);
}

template <>
WebSocketSession<ssl::stream<tcp::socket&> >::WebSocketSession(
    boost::asio::ip::tcp::socket socket,
    ssl::context& ctx,
    DataReceiveCallback dataReceive)
    : socket_(std::move(socket))
    , ws_(socket_, ctx)
    , strand_(ws_.get_executor())
    , dataReceive_(dataReceive) {
  ws_.binary(true);
}

template <typename ExecutorType>
WebSocketSession<ExecutorType>::~WebSocketSession() {}

template <typename ExecutorType>
void WebSocketSession<ExecutorType>::AsyncAccept() {
  ws_.async_accept(
      boost::asio::bind_executor(strand_,
                                 std::bind(&WebSocketSession::AsyncRead,
                                           this->shared_from_this(),
                                           std::placeholders::_1)));
}

template <typename ExecutorType>
void WebSocketSession<ExecutorType>::AsyncRead(boost::system::error_code ec) {
  LOG4CXX_AUTO_TRACE(ws_logger_);
  if (ec) {
    auto str_err = "ErrorMessage: " + ec.message();
    LOG4CXX_ERROR(ws_logger_, str_err);
    // shutdown_ = true;
    // thread_delegate_->SetShutdown();
    return;
  }

  ws_.async_read(buffer_,
                 boost::asio::bind_executor(strand_,
                                            std::bind(&WebSocketSession::Read,
                                                      this->shared_from_this(),
                                                      std::placeholders::_1,
                                                      std::placeholders::_2)));
}

template <typename ExecutorType>
TransportAdapter::Error WebSocketSession<ExecutorType>::WriteDown(
    ::protocol_handler::RawMessagePtr message) {
  boost::system::error_code ec;
  ws_.write(boost::asio::buffer(message->data(), message->data_size()), ec);

  if (ec) {
    LOG4CXX_ERROR(ws_logger_, "A system error has occurred: " << ec.message());
    return TransportAdapter::FAIL;
  }

  return TransportAdapter::OK;
}

template <typename ExecutorType>
void WebSocketSession<ExecutorType>::Read(boost::system::error_code ec,
                                          std::size_t bytes_transferred) {
  LOG4CXX_AUTO_TRACE(ws_logger_);
  boost::ignore_unused(bytes_transferred);
  if (ec) {
    LOG4CXX_ERROR(ws_logger_, ec.message());
    buffer_.consume(buffer_.size());
    return;
  }

  auto size = (ssize_t)buffer_.size();
  const auto data = boost::asio::buffer_cast<const uint8_t*>(
      boost::beast::buffers_front(buffer_.data()));

  LOG4CXX_DEBUG(ws_logger_,
                "Msg: " << boost::beast::buffers_to_string(buffer_.data())
                        << " Size: " << size;);

  ::protocol_handler::RawMessagePtr frame(
      new protocol_handler::RawMessage(0, 0, data, size, false));

  dataReceive_(frame);

  buffer_.consume(buffer_.size());
  AsyncRead(ec);
}

template <typename ExecutorType>
bool WebSocketSession<ExecutorType>::Shutdown() {
  LOG4CXX_AUTO_TRACE(ws_logger_);
  boost::system::error_code ec;
  socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
  buffer_.consume(buffer_.size());
  if (ec) {
    LOG4CXX_ERROR(ws_logger_, ec.message());
    return false;
  }
  return true;
}

template class WebSocketSession<tcp::socket&>;
template class WebSocketSession<ssl::stream<tcp::socket&> >;

}  // namespace transport_adapter
}  // namespace transport_manager
