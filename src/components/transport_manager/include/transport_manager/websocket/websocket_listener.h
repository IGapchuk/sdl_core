/*
 * \file websocket_listener.h
 * \brief WebSocketListener class header file.
 *
 * Copyright (c) 2019
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TRANSPORT_ADAPTER_WEBSOCKET_SERVER_LISTENER_H
#define TRANSPORT_ADAPTER_WEBSOCKET_SERVER_LISTENER_H

#include <boost/asio/thread_pool.hpp>
#include "transport_manager/transport_adapter/client_connection_listener.h"
#include "transport_manager/websocket/websocket_connection.h"

namespace transport_manager {
namespace transport_adapter {

class TransportAdapterController;

/**
 * @brief Class responsible for communication over websockets.
 */
class WebSocketListener : public ClientConnectionListener {
 public:
  /**
   * @brief Constructor.
   * @param controller Pointer to the device adapter controller.
   * @param number of threads for listen incoming connections
   */
  WebSocketListener(TransportAdapterController* controller,
                    const int num_threads = 1);

  /**
   * @brief Destructor.
   */
  ~WebSocketListener();

  TransportAdapter::Error Init() OVERRIDE;
  void Terminate() OVERRIDE;
  bool IsInitialised() const OVERRIDE {
    return true;
  }
  TransportAdapter::Error StartListening() OVERRIDE;
  TransportAdapter::Error StopListening() OVERRIDE {
    return TransportAdapter::OK;
  }
  TransportAdapter::Error SuspendListening() OVERRIDE {
    return TransportAdapter::OK;
  }
  TransportAdapter::Error ResumeListening() OVERRIDE {
    return TransportAdapter::OK;
  }

  /**
   * @brief Attempt to add provided certificate to the ssl::context
   *
   * @param cert Certificate string from policy table
   */
  void AddCertificateAuthority(std::string cert, boost::system::error_code& ec);

 protected:
  bool Run();
  bool WaitForConnection();
  bool WaitForSecureConnection();
  void StartSession(boost::system::error_code ec);
  void StartSecureSession(boost::system::error_code ec);
  void Shutdown();

 private:
  TransportAdapterController* controller_;
  boost::asio::io_context ioc_;
  boost::asio::io_context secure_ioc_;
  ssl::context ctx_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  tcp::acceptor secure_acceptor_;
  tcp::socket secure_socket_;
  boost::asio::thread_pool io_pool_;
  boost::asio::thread_pool secure_io_pool_;
  std::atomic_bool shutdown_;
  std::vector<std::shared_ptr<Connection> > mConnectionList;
  sync_primitives::Lock mConnectionListLock;
};

}  // namespace transport_adapter
}  // namespace transport_manager

#endif  // TRANSPORT_ADAPTER_WEBSOCKET_SERVER_LISTENER_H
