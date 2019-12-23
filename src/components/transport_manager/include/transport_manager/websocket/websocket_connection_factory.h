/*
 * \file websocket_connection_factory.h
 * \brief WebSocketConnectionFactory class header file.
 *
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

#pragma once

#include "transport_manager/transport_adapter/server_connection_factory.h"

namespace transport_manager {
namespace transport_adapter {

class TransportAdapterController;

/**
 * @brief Create connections.
 */
class WebSocketConnectionFactory : public ServerConnectionFactory {
 public:
  /**
   * @brief Constructor.
   *
   * @param controller Pointer to the device adapter controller.
   */
  explicit WebSocketConnectionFactory(TransportAdapterController* controller);

  /**
   * @brief Start web engine server connection factory.
   *
   * @return Information about possible reason of error during initialization.
   */
  virtual TransportAdapter::Error Init();

  /**
   * @brief Create web engine boost websocket connection..
   *
   * @param device_uid device unique identifier.
   * @param app_handle Handle of application.
   *
   * @return Error information about possible reason of failure.
   */
  virtual TransportAdapter::Error CreateConnection(
      const DeviceUID& device_uid, const ApplicationHandle& app_handle);

  /**
   * @brief
   */
  virtual void Terminate();

  /**
   * @brief Check for initialization.
   *
   * @return true - initialized.
   * false - not initialized.
   */
  virtual bool IsInitialised() const;

  /**
   * @brief Destructor.
   */
  virtual ~WebSocketConnectionFactory();

 private:
  TransportAdapterController* controller_;
};

}  // namespace transport_adapter
}  // namespace transport_manager
