/*
 * Copyright (c) 2019, Ford Motor Company
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

#include "transport_manager/websocket/websocket_connection_factory.h"
#include "transport_manager/transport_adapter/transport_adapter_controller.h"
#include "transport_manager/websocket/websocket_connection.h"
#include "transport_manager/websocket/websocket_device.h"

#include "utils/logger.h"

namespace transport_manager {
namespace transport_adapter {

CREATE_LOGGERPTR_GLOBAL(logger_, "WebSocketConnectionFactory")

WebSocketConnectionFactory::WebSocketConnectionFactory(
    TransportAdapterController* controller)
    : controller_(controller) {}

TransportAdapter::Error WebSocketConnectionFactory::Init() {
  return TransportAdapter::OK;
}

TransportAdapter::Error WebSocketConnectionFactory::CreateConnection(
    const DeviceUID& device_uid, const ApplicationHandle& app_handle) {
  LOG4CXX_AUTO_TRACE(logger_);
  LOG4CXX_DEBUG(
      logger_,
      "DeviceUID: " << &device_uid << ", ApplicationHandle: " << &app_handle);

  DeviceSptr device = controller_->FindDevice(device_uid);
  if (device.use_count() == 0) {
    LOG4CXX_ERROR(logger_, "device " << device_uid << " not found");
    LOG4CXX_TRACE(logger_,
                  "exit with TransportAdapter::BAD_PARAM. Condition: "
                  "device.use_count() == 0");
    return TransportAdapter::BAD_PARAM;
  }

  boost::asio::io_context ioc;
  WebSocketDevice* websocket_device =
      static_cast<WebSocketDevice*>(device.get());
  if (websocket_device->IsSecure()) {
    ssl::context ctx(ssl::context::sslv23);  // temporarily for compilation only
    tcp::socket secure_socket(ioc);
    secure_socket.assign(websocket_device->GetProtocol(), app_handle);
    auto connection =
        std::make_shared<WebSocketConnection<WebSocketSecureSession<> > >(
            websocket_device->unique_device_id(),
            app_handle,
            std::move(secure_socket),
            ctx,
            controller_);

    controller_->ConnectionCreated(
        connection, device->unique_device_id(), app_handle);

    controller_->ConnectDone(device->unique_device_id(), app_handle);

    connection->Run();

  } else {
    tcp::socket socket(ioc);
    socket.assign(websocket_device->GetProtocol(), app_handle);
    auto connection =
        std::make_shared<WebSocketConnection<WebSocketSession<> > >(
            device->unique_device_id(),
            app_handle,
            std::move(socket),
            controller_);

    controller_->ConnectionCreated(
        connection, device->unique_device_id(), app_handle);

    controller_->ConnectDone(device->unique_device_id(), app_handle);

    connection->Run();
  }

  return TransportAdapter::OK;
}

void WebSocketConnectionFactory::Terminate() {}

bool WebSocketConnectionFactory::IsInitialised() const {
  return true;
}

WebSocketConnectionFactory::~WebSocketConnectionFactory() {}

}  // namespace transport_adapter
}  // namespace transport_manager
