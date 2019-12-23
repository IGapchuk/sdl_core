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

#pragma once

#include <algorithm>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include "transport_manager/transport_adapter/connection.h"
#include "transport_manager/websocket/websocket_secure_session.h"
#include "utils/message_queue.h"
#include "utils/threads/thread.h"

using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;  // from <boost/asio/ssl.hpp>
namespace websocket =
    boost::beast::websocket;  // from <boost/beast/websocket.hpp>

using ::utils::MessageQueue;

typedef ::protocol_handler::RawMessagePtr Message;
typedef std::queue<Message> AsyncQueue;

namespace transport_manager {
namespace transport_adapter {

CREATE_LOGGERPTR_GLOBAL(wsc_logger_, "WebSocketConnection")

class TransportAdapterController;

template <class Layer = tcp::socket>
class WebSocketConnection
    : public std::enable_shared_from_this<WebSocketConnection<Layer> >,
      public Connection {
 public:
  WebSocketConnection(const DeviceUID& device_uid,
                      const ApplicationHandle& app_handle,
                      boost::asio::ip::tcp::socket socket,
                      TransportAdapterController* controller);

  WebSocketConnection(const DeviceUID& device_uid,
                      const ApplicationHandle& app_handle,
                      boost::asio::ip::tcp::socket socket,
                      ssl::context& ctx,
                      TransportAdapterController* controller);

  ~WebSocketConnection();

  void Accept();

  void Shutdown();

  bool IsShuttingDown();

  void Recv(boost::system::error_code ec);

  TransportAdapter::Error SendData(
      ::protocol_handler::RawMessagePtr message) OVERRIDE;
  TransportAdapter::Error Disconnect() OVERRIDE {
    return TransportAdapter::OK;
  }

  void Read(boost::system::error_code ec, std::size_t bytes_transferred);

 private:
  const DeviceUID device_uid_;
  const ApplicationHandle app_handle_;
  std::unique_ptr<tcp::socket> socket_;
  websocket::stream<Layer> ws_;
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  boost::beast::multi_buffer buffer_;
  TransportAdapterController* controller_;

  std::atomic_bool shutdown_;

  MessageQueue<Message, AsyncQueue> message_queue_;

  class LoopThreadDelegate : public threads::ThreadDelegate {
   public:
    LoopThreadDelegate(MessageQueue<Message, AsyncQueue>* message_queue,
                       WebSocketConnection<Layer>* handler);

    virtual void threadMain() OVERRIDE;
    virtual void exitThreadMain() OVERRIDE;

    void OnWrite();

    void SetShutdown();

   private:
    void DrainQueue();
    MessageQueue<Message, AsyncQueue>& message_queue_;
    WebSocketConnection<Layer>& handler_;
    std::atomic_bool shutdown_;
  };

  LoopThreadDelegate* thread_delegate_;
  threads::Thread* thread_;
};

template class WebSocketConnection<ssl::stream<tcp::socket&> >;
template class WebSocketConnection<tcp::socket>;

}  // namespace transport_adapter
}  // namespace transport_manager
