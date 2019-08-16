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

#include "app_service_rpc_plugin/commands/hmi/as_poi_search_request_from_hmi.h"

#include "application_manager/app_service_manager.h"
#include "application_manager/message_helper.h"
#include "interfaces/MOBILE_API.h"

namespace app_service_rpc_plugin {
using namespace application_manager;
namespace commands {
ASPOISearchRequestFromHMI::ASPOISearchRequestFromHMI(
    const app_mngr::commands::MessageSharedPtr& message,
    app_mngr::ApplicationManager& application_manager,
    app_mngr::rpc_service::RPCService& rpc_service,
    app_mngr::HMICapabilities& hmi_capabilities,
    policy::PolicyHandlerInterface& policy_handle)
    : RequestFromHMI(message,
                     application_manager,
                     rpc_service,
                     hmi_capabilities,
                     policy_handle)
    , service_type_("POINTS_OF_INTEREST") {}

void ASPOISearchRequestFromHMI::Run() {
  LOG4CXX_AUTO_TRACE(logger_);

  if (!IsServiceExists(service_type_)) {
    LOG4CXX_DEBUG(logger_, "Service [" << service_type_ << "] does not exist");
    SendErrorResponse(correlation_id(),
                      hmi_apis::FunctionID::AppService_POISearch,
                      hmi_apis::Common_Result::REJECTED,
                      "The requested service type does not exist");
    return;
  }

  SendRequestToMobile();
}

void ASPOISearchRequestFromHMI::on_event(
    const event_engine::MobileEvent& event) {
  LOG4CXX_AUTO_TRACE(logger_);

  smart_objects::SmartObject event_message(event.smart_object());

  auto& msg_params = event_message[strings::msg_params];

  mobile_apis::Result::eType mobile_result =
      static_cast<mobile_apis::Result::eType>(
          msg_params[strings::result_code].asInt());
  hmi_apis::Common_Result::eType result =
      MessageHelper::MobileToHMIResult(mobile_result);

  SendResponse(true,
               correlation_id(),
               hmi_apis::FunctionID::AppService_POISearch,
               result,
               &msg_params,
               application_manager::commands::Command::SOURCE_SDL_TO_HMI);
}

ASPOISearchRequestFromHMI::~ASPOISearchRequestFromHMI() {}

bool ASPOISearchRequestFromHMI::IsServiceExists(
    const std::string& service_type) {
  auto active_services =
      application_manager_.GetAppServiceManager().GetActiveServices();

  for (const auto& service : active_services) {
    if (service
            .record[app_service_rpc_plugin::strings::service_manifest]
                   [app_mngr::strings::service_type]
            .asString() == service_type) {
      return true;
    }
  }

  return false;
}

void ASPOISearchRequestFromHMI::SendRequestToMobile() {
  ApplicationSharedPtr app;
  bool hmi_service = false;

  application_manager_.GetAppServiceManager().GetProviderByType(
      service_type_, false, app, hmi_service);

  if (!app) {
    LOG4CXX_DEBUG(
        logger_,
        "Application with app_id:" << connection_key() << " is absent");
    return;
  }

  const auto& msg_params = (*message_)[app_mngr::strings::msg_params];

  SendMobileRequest(
      mobile_apis::FunctionID::POISearchID, app, &msg_params, true);
}
}  // namespace commands
}  // namespace app_service_rpc_plugin
