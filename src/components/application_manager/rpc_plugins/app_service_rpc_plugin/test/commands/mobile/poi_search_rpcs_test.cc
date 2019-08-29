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
#include "app_service_rpc_plugin/commands/hmi/as_poi_search_request_to_hmi.h"
#include "app_service_rpc_plugin/commands/hmi/as_poi_search_response_from_hmi.h"
#include "app_service_rpc_plugin/commands/hmi/as_poi_search_response_to_hmi.h"
#include "app_service_rpc_plugin/commands/mobile/poi_search_request_from_mobile.h"
#include "app_service_rpc_plugin/commands/mobile/poi_search_request_to_mobile.h"
#include "app_service_rpc_plugin/commands/mobile/poi_search_response_from_mobile.h"
#include "app_service_rpc_plugin/commands/mobile/poi_search_response_to_mobile.h"
#include "resumption/last_state_impl.h"

#include "application_manager/commands/command_impl.h"
#include "application_manager/commands/commands_test.h"

#include "application_manager/mock_app_service_manager.h"
#include "application_manager/mock_application.h"
#include "application_manager/mock_event_dispatcher.h"
#include "protocol_handler/mock_protocol_handler.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace am = application_manager;

using am::commands::MessageSharedPtr;
using test::components::application_manager_test::MockApplication;
using test::components::application_manager_test::MockAppServiceManager;
using test::components::commands_test::CommandsTest;
using test::components::commands_test::CommandsTestMocks;
using test::components::commands_test::HMIResultCodeIs;
using test::components::commands_test::MobileResultCodeIs;
using test::components::commands_test::MockMessageHelper;
using test::components::event_engine_test::MockEventDispatcher;
using test::components::protocol_handler_test::MockProtocolHandler;

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgReferee;

namespace {
const uint32_t kAppId1 = 0u;
const uint32_t kConnectionKey1 = 1u;
const uint32_t kAppId2 = 2u;
const uint32_t kConnectionKey2 = 3u;
const uint32_t kMobileCorrelationID = 4u;
const std::string kServiceType = "POINTS_OF_INTEREST";
const std::string kServiceID = "service_id";
const protocol_handler::MajorProtocolVersion kProtocolVersion =
    protocol_handler::MajorProtocolVersion::PROTOCOL_VERSION_3;

enum class MessageSource { MOBILE, HMI };

smart_objects::SmartObject CreateServiceRecord() {
  smart_objects::SmartObject record(smart_objects::SmartType::SmartType_Map);

  record[am::strings::service_manifest][am::strings::service_type] =
      kServiceType;
  record[am::strings::service_id] = kServiceID;
  return record;
}

MessageSharedPtr CreateBaseMessage(const MessageSource message_source) {
  using namespace am;
  MessageSharedPtr message =
      CommandsTest<CommandsTestMocks::kIsNice>::CreateMessage();

  (*message)[strings::msg_params] =
      smart_objects::SmartObject(smart_objects::SmartType::SmartType_Map);
  switch (message_source) {
    case MessageSource::MOBILE: {
      (*message)[strings::params][strings::function_id] =
          mobile_apis::FunctionID::POISearchID;
      (*message)[strings::msg_params][strings::result_code] =
          mobile_apis::Result::SUCCESS;
    } break;
    case MessageSource::HMI: {
      (*message)[am::strings::params][am::strings::function_id] =
          hmi_apis::FunctionID::AppService_POISearch;
      (*message)[strings::msg_params][strings::result_code] =
          hmi_apis::Common_Result::SUCCESS;
    }
  }
  return message;
}

}  // namespace

namespace app_service_plugin_test {
namespace poi_search_service_test {
using namespace app_service_rpc_plugin;

// Unit tests for POISearch request from Mobile

class POISearchRequestFromMobileTest
    : public CommandsTest<CommandsTestMocks::kIsNice> {
 public:
  POISearchRequestFromMobileTest()
      : app_service_record_(CreateServiceRecord())
      , mock_app_(std::make_shared<NiceMock<MockApplication> >())
      , last_state_("app_storage_folder", "app_info_storage")
      , mock_app_service_manager_(app_mngr_, last_state_) {}

 protected:
  void SetUp() OVERRIDE {
    ON_CALL(app_mngr_, GetAppServiceManager())
        .WillByDefault(ReturnRef(mock_app_service_manager_));
    ON_CALL(app_mngr_, protocol_handler())
        .WillByDefault(ReturnRef(mock_protocol_handler_));
    ON_CALL(app_mngr_, event_dispatcher())
        .WillByDefault(ReturnRef(mock_event_dispatcher_));
  }

  std::vector<am::AppService> app_services_;
  smart_objects::SmartObject app_service_record_;
  std::shared_ptr<MockApplication> mock_app_;
  resumption::LastStateImpl last_state_;
  MockAppServiceManager mock_app_service_manager_;
  MockProtocolHandler mock_protocol_handler_;
  NiceMock<MockEventDispatcher> mock_event_dispatcher_;
};

TEST_F(POISearchRequestFromMobileTest,
       HMIProvider_SendRequestFromMobileToHMI_ReceiveResponse) {
  auto request_from_mobile = CreateBaseMessage(MessageSource::MOBILE);

  auto command =
      CreateCommand<commands::POISearchRequestFromMobile>(request_from_mobile);

  am::AppService hmi_service;
  hmi_service.connection_key = kConnectionKey1;
  hmi_service.mobile_service = false;
  hmi_service.record = app_service_record_;

  app_services_.push_back(hmi_service);

  EXPECT_CALL(mock_app_service_manager_, GetActiveServices())
      .WillOnce(Return(app_services_));

  EXPECT_CALL(mock_app_service_manager_,
              GetProviderByType(kServiceType, true, _, _))
      .WillOnce(SetArgReferee<3>(true));

  EXPECT_CALL(mock_rpc_service_,
              ManageHMICommand(
                  _, am::commands::Command::CommandSource::SOURCE_SDL_TO_HMI));

  // Send request from Mobile consumer to HMI provider
  ASSERT_TRUE(command->Init());
  command->Run();

  auto response_from_hmi_to_mobile = CreateBaseMessage(MessageSource::HMI);
  (*response_from_hmi_to_mobile)[am::strings::params][am::hmi_response::code] =
      hmi_apis::Common_Result::SUCCESS;

  am::event_engine::Event event(hmi_apis::FunctionID::AppService_POISearch);
  event.set_smart_object(*response_from_hmi_to_mobile);

  EXPECT_CALL(mock_rpc_service_, ManageMobileCommand(_, _));

  // Receive response from HMI provider and forward it to Mobile consumer
  command->on_event(event);
}

TEST_F(POISearchRequestFromMobileTest,
       MobileProvider_SendRequestFromMobileToMobile_ReceiveResponse) {
  auto request_from_mobile = CreateBaseMessage(MessageSource::MOBILE);

  auto command =
      CreateCommand<commands::POISearchRequestFromMobile>(request_from_mobile);

  am::AppService mobile_service;
  mobile_service.connection_key = kConnectionKey1;
  mobile_service.mobile_service = true;
  mobile_service.record = app_service_record_;

  app_services_.push_back(mobile_service);

  EXPECT_CALL(mock_app_service_manager_, GetActiveServices())
      .WillOnce(Return(app_services_));

  EXPECT_CALL(mock_app_service_manager_,
              GetProviderByType(kServiceType, true, _, _))
      .WillOnce(DoAll(SetArgReferee<3>(true), SetArgReferee<2>(mock_app_)));

  EXPECT_CALL(mock_rpc_service_, ManageMobileCommand(_, _));

  // Send request from Mobile consumer to Mobile provider
  ASSERT_TRUE(command->Init());
  command->Run();

  auto response_from_mobile_to_mobile =
      CreateBaseMessage(MessageSource::MOBILE);
  (*response_from_mobile_to_mobile)[am::strings::msg_params] =
      smart_objects::SmartObject(smart_objects::SmartType::SmartType_Map);
  (*response_from_mobile_to_mobile)[am::strings::params]
                                   [am::hmi_response::code] =
                                       hmi_apis::Common_Result::SUCCESS;

  am::event_engine::MobileEvent event(mobile_apis::FunctionID::POISearchID);
  event.set_smart_object(*response_from_mobile_to_mobile);

  EXPECT_CALL(mock_rpc_service_, ManageMobileCommand(_, _));

  // Receive response from Mobile provider and forward it to Mobile consumer
  command->on_event(event);
}

// Unit tests for POISearch request from HMI

class ASPOISearchRequestFromHMI
    : public CommandsTest<CommandsTestMocks::kIsNice> {
 public:
  ASPOISearchRequestFromHMI()
      : app_service_record_(CreateServiceRecord())
      , mock_app_(std::make_shared<NiceMock<MockApplication> >())
      , last_state_("app_storage_folder", "app_info_storage")
      , mock_app_service_manager_(app_mngr_, last_state_) {}

 protected:
  void SetUp() OVERRIDE {
    ON_CALL(app_mngr_, GetAppServiceManager())
        .WillByDefault(ReturnRef(mock_app_service_manager_));
    ON_CALL(app_mngr_, protocol_handler())
        .WillByDefault(ReturnRef(mock_protocol_handler_));
    ON_CALL(app_mngr_, event_dispatcher())
        .WillByDefault(ReturnRef(mock_event_dispatcher_));
    ON_CALL(app_mngr_, GetNextMobileCorrelationID())
        .WillByDefault(Return(kMobileCorrelationID));
    ON_CALL(*mock_app_, protocol_version())
        .WillByDefault(Return(kProtocolVersion));
    ON_CALL(mock_message_helper_, MobileToHMIResult(_))
        .WillByDefault(Return(hmi_apis::Common_Result::SUCCESS));
  }

  std::vector<am::AppService> app_services_;
  smart_objects::SmartObject app_service_record_;
  std::shared_ptr<MockApplication> mock_app_;
  resumption::LastStateImpl last_state_;
  MockAppServiceManager mock_app_service_manager_;
  MockProtocolHandler mock_protocol_handler_;
  NiceMock<MockEventDispatcher> mock_event_dispatcher_;
};

TEST_F(ASPOISearchRequestFromHMI,
       MobileProvider_SendRequestFromHMIToMobile_ReceiveResponse) {
  auto request_from_hmi = CreateBaseMessage(MessageSource::HMI);

  auto command =
      CreateCommand<commands::ASPOISearchRequestFromHMI>(request_from_hmi);

  am::AppService mobile_service;
  mobile_service.connection_key = kConnectionKey1;
  mobile_service.mobile_service = true;
  mobile_service.record = app_service_record_;

  app_services_.push_back(mobile_service);

  EXPECT_CALL(mock_app_service_manager_, GetActiveServices())
      .WillOnce(Return(app_services_));

  EXPECT_CALL(mock_app_service_manager_,
              GetProviderByType(kServiceType, false, _, _))
      .WillOnce(DoAll(SetArgReferee<3>(false), SetArgReferee<2>(mock_app_)));

  EXPECT_CALL(mock_rpc_service_, ManageMobileCommand(_, _));

  // Send request from HMI consumer to Mobile provider
  ASSERT_TRUE(command->Init());
  command->Run();

  auto response_from_mobile_to_hmi = CreateBaseMessage(MessageSource::MOBILE);
  (*response_from_mobile_to_hmi)[am::strings::msg_params] =
      smart_objects::SmartObject(smart_objects::SmartType::SmartType_Map);
  (*response_from_mobile_to_hmi)[am::strings::params][am::hmi_response::code] =
      hmi_apis::Common_Result::SUCCESS;

  am::event_engine::MobileEvent event(mobile_apis::FunctionID::POISearchID);
  event.set_smart_object(*response_from_mobile_to_hmi);

  EXPECT_CALL(mock_rpc_service_, ManageHMICommand(_, _));

  // Receive response from Mobile provider and forward it to HMI consumer
  command->on_event(event);
}

}  // namespace poi_search_service_test
}  // namespace app_service_plugin_test
