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

#include "app_service_rpc_plugin/commands/mobile/poi_search_request_from_mobile.h"
#include "app_service_rpc_plugin/app_service_app_extension.h"
#include "application_manager/app_service_manager.h"
#include "application_manager/message_helper.h"

namespace app_service_rpc_plugin {
using namespace application_manager;
namespace commands {

POISearchRequestFromMobile::POISearchRequestFromMobile(
    const application_manager::commands::MessageSharedPtr& message,
    application_manager::ApplicationManager& application_manager,
    application_manager::rpc_service::RPCService& rpc_service,
    application_manager::HMICapabilities& hmi_capabilities,
    policy::PolicyHandlerInterface& policy_handle)
    : app_mngr::commands::CommandRequestImpl(message,
                                             application_manager,
                                             rpc_service,
                                             hmi_capabilities,
                                             policy_handle) {}

void POISearchRequestFromMobile::Run() {
  LOG4CXX_AUTO_TRACE(logger_);

  const std::string service_type = "POINTS_OF_INTEREST";

  if (!IsServiceExists(service_type)) {
    LOG4CXX_DEBUG(logger_, "Service [" << service_type << "] does not exist");
    SendResponse(false,
                 mobile_apis::Result::REJECTED,
                 "The requested service type does not exist");
    return;
  }

  SendRequest();
}

void POISearchRequestFromMobile::on_event(
    const app_mngr::event_engine::MobileEvent& event) {
  using namespace app_mngr;
  const smart_objects::SmartObject& event_message = event.smart_object();

  auto msg_params = event_message[strings::msg_params];

  mobile_apis::Result::eType result = static_cast<mobile_apis::Result::eType>(
      msg_params[strings::result_code].asInt());
  bool success = IsMobileResultSuccess(result);

  SendResponse(success, result, nullptr, &msg_params);
}

void POISearchRequestFromMobile::on_event(
    const app_mngr::event_engine::Event& event) {
  using namespace app_mngr;

  const smart_objects::SmartObject& event_message = event.smart_object();

  auto msg_params = event_message[strings::msg_params];

  hmi_apis::Common_Result::eType hmi_result =
      static_cast<hmi_apis::Common_Result::eType>(
          event_message[strings::params][hmi_response::code].asInt());

  mobile_apis::Result::eType result =
      MessageHelper::HMIToMobileResult(hmi_result);

  bool success = PrepareResultForMobileResponse(
      hmi_result, HmiInterfaces::HMI_INTERFACE_AppService);

  SendResponse(success, result, nullptr, &msg_params);
}

POISearchRequestFromMobile::~POISearchRequestFromMobile() {}

bool POISearchRequestFromMobile::IsServiceExists(
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

void POISearchRequestFromMobile::SendRequest() {
  ApplicationSharedPtr app;

  const std::string service_type = "POINTS_OF_INTEREST";
  bool hmi_service = false;

  application_manager_.GetAppServiceManager().GetProviderByType(
      service_type, true, app, hmi_service);

  const auto& msg_params = (*message_)[app_mngr::strings::msg_params];

  if (hmi_service) {
    SendHMIRequest(
        hmi_apis::FunctionID::AppService_POISearch, &msg_params, true);
    return;
  }

  if (!app) {
    LOG4CXX_DEBUG(
        logger_,
        "Application with app_id:" << connection_key() << " is absent");
    return;
  }

  SendMobileRequest(mobile_api::FunctionID::POISearchID, message_, true);
}

}  // namespace commands
}  // namespace app_service_rpc_plugin
