/*
 Copyright (c) 2019, Ford Motor Company, Livio
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following
 disclaimer in the documentation and/or other materials provided with the
 distribution.

 Neither the name of the the copyright holders nor the names of their
 contributors may be used to endorse or promote products derived from this
 software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SRC_COMPONENTS_APPLICATION_MANAGER_RPC_PLUGINS_APP_SERVICE_RPC_PLUGIN_INCLUDE_APP_SERVICE_RPC_PLUGIN_COMMANDS_MOBILE_POI_SEARCH_REQUEST_TO_MOBILE_H
#define SRC_COMPONENTS_APPLICATION_MANAGER_RPC_PLUGINS_APP_SERVICE_RPC_PLUGIN_INCLUDE_APP_SERVICE_RPC_PLUGIN_COMMANDS_MOBILE_POI_SEARCH_REQUEST_TO_MOBILE_H

#include "application_manager/commands/command_request_to_mobile.h"

namespace app_service_rpc_plugin {
namespace app_mngr = application_manager;
namespace commands {
class POISearchRequestToMobile
    : public app_mngr::commands::CommandRequestToMobile {
 public:
  /**
   * @brief POISearchRequestToMobile class constructor
   *
   * @param message reference to RequestMessage instance
   * @param application_manager reference to Application Manager instance
   * @param rpc_srvice
   * @param hmi_capabilities
   * @param policy_handler
   */
  POISearchRequestToMobile(const app_mngr::commands::MessageSharedPtr& message,
                           app_mngr::ApplicationManager& application_manager,
                           app_mngr::rpc_service::RPCService& rpc_service,
                           app_mngr::HMICapabilities& hmi_capabilities,
                           policy::PolicyHandlerInterface& policy_handle);

  /**
   * @brief Execute command
   **/
  void Run() OVERRIDE;

  /**
   * @brief POISearchRequestToMobile class destructor
   **/
  ~POISearchRequestToMobile();
};
}  // namespace commands
}  // namespace app_service_rpc_plugin

#endif  // SRC_COMPONENTS_APPLICATION_MANAGER_RPC_PLUGINS_APP_SERVICE_RPC_PLUGIN_INCLUDE_APP_SERVICE_RPC_PLUGIN_COMMANDS_MOBILE_POI_SEARCH_REQUEST_TO_MOBILE_H
