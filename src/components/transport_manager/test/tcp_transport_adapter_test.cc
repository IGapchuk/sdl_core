/*
 * Copyright (c) 2015, Ford Motor Company
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

#include "transport_manager/tcp/tcp_transport_adapter.h"
#include "gtest/gtest.h"
#include "protocol/raw_message.h"
#include "resumption/last_state_impl.h"
#include "resumption/last_state_wrapper_impl.h"
#include "transport_manager/mock_transport_manager_settings.h"
#include "transport_manager/tcp/mock_tcp_transport_adapter.h"
#include "transport_manager/transport_adapter/connection.h"
#include "transport_manager/transport_adapter/mock_connection.h"
#include "transport_manager/transport_adapter/mock_device.h"
#include "transport_manager/transport_adapter/mock_transport_adapter_listener.h"

namespace test {
namespace components {
namespace transport_manager_test {

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

using namespace ::protocol_handler;
using namespace ::transport_manager;
using namespace transport_manager::transport_adapter;

class TcpAdapterTest : public ::testing::Test {
 protected:
  TcpAdapterTest() {
    last_state_wrapper_ = std::make_shared<resumption::LastStateWrapperImpl>(
        std::make_shared<resumption::LastStateImpl>("app_storage_folder",
                                                    "app_info_storage"));
  }
  MockTransportManagerSettings transport_manager_settings;
  std::shared_ptr<resumption::LastStateWrapperImpl> last_state_wrapper_;
  const uint32_t port = 12345;
  const std::string string_port = "12345";
  std::string network_interface = "";

  void SetUp() OVERRIDE {
    EXPECT_CALL(transport_manager_settings,
                transport_manager_tcp_adapter_network_interface())
        .WillRepeatedly(ReturnRef(network_interface));
  }
};

TEST_F(TcpAdapterTest, StoreDataWithOneDeviceAndOneApplication) {
  // Prepare
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  std::string uniq_id = "unique_device_name";
  std::shared_ptr<MockTCPDevice> mockdev =
      std::make_shared<MockTCPDevice>(port, uniq_id);
  transport_adapter.AddDevice(mockdev);

  std::vector<std::string> devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(1u, devList.size());
  EXPECT_EQ(uniq_id, devList[0]);

  const int app_handle = 1;
  std::vector<int> intList = {app_handle};
  EXPECT_CALL(*mockdev, GetApplicationList()).WillOnce(Return(intList));

  ConnectionSPtr mock_connection = std::make_shared<MockConnection>();
  EXPECT_CALL(transport_adapter, FindDevice(uniq_id)).WillOnce(Return(mockdev));
  EXPECT_CALL(transport_adapter, FindEstablishedConnection(uniq_id, app_handle))
      .WillOnce(Return(mock_connection));

  EXPECT_CALL(*mockdev, GetApplicationPort(app_handle)).WillOnce(Return(port));

  transport_adapter.CallStore();

  // Check that value is saved
  resumption::LastStateAccessor accessor = last_state_wrapper_->get_accessor();
  const Json::Value dictionary = accessor.GetData().dictionary();
  const Json::Value& tcp_dict = dictionary["TransportManager"]["TcpAdapter"];

  ASSERT_TRUE(tcp_dict.isObject());
  ASSERT_FALSE(tcp_dict["devices"].isNull());
  ASSERT_FALSE(tcp_dict["devices"][0]["applications"].isNull());
  ASSERT_FALSE(tcp_dict["devices"][0]["address"].isNull());
  EXPECT_EQ(1u, tcp_dict["devices"][0]["applications"].size());
  EXPECT_EQ(string_port,
            tcp_dict["devices"][0]["applications"][0]["port"].asString());
  EXPECT_EQ(uniq_id, tcp_dict["devices"][0]["name"].asString());
}

TEST_F(TcpAdapterTest, StoreDataWithSeveralDevicesAndOneApplication) {
  // Prepare
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  const uint32_t count_dev = 10;
  std::shared_ptr<MockTCPDevice> mockdev[count_dev];
  std::string uniq_id[count_dev];
  for (uint32_t i = 0; i < count_dev; i++) {
    char numb[12];
    std::snprintf(numb, 12, "%d", i);
    uniq_id[i] = "unique_device_name" + std::string(numb);
    mockdev[i] = std::make_shared<MockTCPDevice>(port, uniq_id[i]);
    EXPECT_CALL(*(mockdev[i]), IsSameAs(_)).WillRepeatedly(Return(false));
    transport_adapter.AddDevice(mockdev[i]);
  }

  std::vector<std::string> devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(count_dev, devList.size());
  EXPECT_EQ(uniq_id[0], devList[0]);

  const int app_handle = 1;
  std::vector<int> intList = {app_handle};

  ConnectionSPtr mock_connection = std::make_shared<MockConnection>();
  for (uint32_t i = 0; i < count_dev; i++) {
    EXPECT_CALL(transport_adapter, FindDevice(uniq_id[i]))
        .WillOnce(Return(mockdev[i]));
    EXPECT_CALL(*(mockdev[i]), GetApplicationList()).WillOnce(Return(intList));

    EXPECT_CALL(transport_adapter,
                FindEstablishedConnection(uniq_id[i], app_handle))
        .WillOnce(Return(mock_connection));

    EXPECT_CALL(*(mockdev[i]), GetApplicationPort(app_handle))
        .WillOnce(Return(port));
  }
  transport_adapter.CallStore();

  // Check that values are saved
  resumption::LastStateAccessor accessor = last_state_wrapper_->get_accessor();
  const Json::Value dictionary = accessor.GetData().dictionary();
  const Json::Value& tcp_dict = dictionary["TransportManager"]["TcpAdapter"];

  ASSERT_TRUE(tcp_dict.isObject());
  ASSERT_FALSE(tcp_dict["devices"].isNull());
  for (uint32_t i = 0; i < count_dev; i++) {
    ASSERT_FALSE(tcp_dict["devices"][i]["applications"].isNull());
    ASSERT_FALSE(tcp_dict["devices"][i]["address"].isNull());
    EXPECT_EQ(1u, tcp_dict["devices"][i]["applications"].size());
    EXPECT_EQ(string_port,
              tcp_dict["devices"][i]["applications"][0]["port"].asString());
    EXPECT_EQ(uniq_id[i], tcp_dict["devices"][i]["name"].asString());
  }
}

TEST_F(TcpAdapterTest, StoreDataWithSeveralDevicesAndSeveralApplications) {
  // Prepare
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  const uint32_t count_dev = 10;

  std::shared_ptr<MockTCPDevice> mockdev[count_dev];
  std::string uniq_id[count_dev];
  for (uint32_t i = 0; i < count_dev; i++) {
    char numb[12];
    std::snprintf(numb, 12, "%d", i);
    uniq_id[i] = "unique_device_name" + std::string(numb);
    mockdev[i] = std::make_shared<MockTCPDevice>(port, uniq_id[i]);
    EXPECT_CALL(*(mockdev[i]), IsSameAs(_)).WillRepeatedly(Return(false));
    transport_adapter.AddDevice(mockdev[i]);
  }

  std::vector<std::string> devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(count_dev, devList.size());
  EXPECT_EQ(uniq_id[0], devList[0]);

  const uint32_t connection_count = 3;
  const int app_handle[connection_count] = {1, 2, 3};
  std::vector<int> intList = {app_handle[0], app_handle[1], app_handle[2]};
  const std::string ports[connection_count] = {"11111", "67890", "98765"};
  const int int_port[connection_count] = {11111, 67890, 98765};
  ConnectionSPtr mock_connection = std::make_shared<MockConnection>();
  for (uint32_t i = 0; i < count_dev; i++) {
    EXPECT_CALL(transport_adapter, FindDevice(uniq_id[i]))
        .WillOnce(Return(mockdev[i]));
    EXPECT_CALL(*(mockdev[i]), GetApplicationList()).WillOnce(Return(intList));

    for (uint32_t j = 0; j < connection_count; j++) {
      EXPECT_CALL(transport_adapter,
                  FindEstablishedConnection(uniq_id[i], app_handle[j]))
          .WillOnce(Return(mock_connection));
      EXPECT_CALL(*(mockdev[i]), GetApplicationPort(app_handle[j]))
          .WillOnce(Return(int_port[j]));
    }
  }
  transport_adapter.CallStore();

  // Check that value is saved
  resumption::LastStateAccessor accessor = last_state_wrapper_->get_accessor();
  const Json::Value dictionary = accessor.GetData().dictionary();
  const Json::Value& tcp_dict = dictionary["TransportManager"]["TcpAdapter"];

  ASSERT_TRUE(tcp_dict.isObject());
  ASSERT_FALSE(tcp_dict["devices"].isNull());
  for (uint32_t i = 0; i < count_dev; i++) {
    ASSERT_FALSE(tcp_dict["devices"][i]["applications"].isNull());
    ASSERT_FALSE(tcp_dict["devices"][i]["address"].isNull());
    for (uint32_t j = 0; j < intList.size(); j++) {
      EXPECT_EQ(ports[j],
                tcp_dict["devices"][i]["applications"][j]["port"].asString());
      EXPECT_EQ(uniq_id[i], tcp_dict["devices"][i]["name"].asString());
    }
  }
}

TEST_F(TcpAdapterTest, StoreData_ConnectionNotExist_DataNotStored) {
  // Prepare
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  std::string uniq_id = "unique_device_name";
  auto mockdev = std::make_shared<MockTCPDevice>(port, uniq_id);
  transport_adapter.AddDevice(mockdev);

  std::vector<std::string> devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(1u, devList.size());
  EXPECT_EQ(uniq_id, devList[0]);
  std::vector<int> intList = {};
  EXPECT_CALL(*mockdev, GetApplicationList()).WillOnce(Return(intList));

  EXPECT_CALL(transport_adapter, FindDevice(uniq_id)).WillOnce(Return(mockdev));
  EXPECT_CALL(transport_adapter, FindEstablishedConnection(uniq_id, _))
      .Times(0);
  EXPECT_CALL(*mockdev, GetApplicationPort(_)).Times(0);
  transport_adapter.CallStore();

  // Check that value is not saved
  resumption::LastStateAccessor accessor = last_state_wrapper_->get_accessor();
  const Json::Value dictionary = accessor.GetData().dictionary();
  const Json::Value& tcp_dict =
      dictionary["TransportManager"]["TcpAdapter"]["devices"];

  ASSERT_TRUE(tcp_dict.isNull());
}

TEST_F(TcpAdapterTest, RestoreData_DataNotStored) {
  {
    resumption::LastStateAccessor accessor =
        last_state_wrapper_->get_accessor();
    Json::Value dictionary = accessor.GetData().dictionary();
    Json::Value& tcp_dictionary = dictionary["TransportManager"]["TcpAdapter"];
    tcp_dictionary = Json::Value();
    accessor.GetMutableData().set_dictionary(dictionary);
  }
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  EXPECT_CALL(transport_adapter, Connect(_, _)).Times(0);
  EXPECT_TRUE(transport_adapter.CallRestore());
}

TEST_F(TcpAdapterTest, StoreDataWithOneDevice_RestoreData) {
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  std::string uniq_id = "unique_device_name";
  std::shared_ptr<MockTCPDevice> mockdev =
      std::make_shared<MockTCPDevice>(port, uniq_id);
  transport_adapter.AddDevice(mockdev);

  std::vector<std::string> devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(1u, devList.size());
  EXPECT_EQ(uniq_id, devList[0]);

  const int app_handle = 1;
  std::vector<int> intList = {app_handle};
  EXPECT_CALL(*mockdev, GetApplicationList()).WillOnce(Return(intList));

  ConnectionSPtr mock_connection = std::make_shared<MockConnection>();
  EXPECT_CALL(transport_adapter, FindDevice(uniq_id)).WillOnce(Return(mockdev));
  EXPECT_CALL(transport_adapter, FindEstablishedConnection(uniq_id, app_handle))
      .WillOnce(Return(mock_connection));

  EXPECT_CALL(*mockdev, GetApplicationPort(app_handle)).WillOnce(Return(port));

  transport_adapter.CallStore();

  EXPECT_CALL(transport_adapter, Connect(uniq_id, app_handle))
      .WillOnce(Return(TransportAdapter::OK));

  EXPECT_TRUE(transport_adapter.CallRestore());

  devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(1u, devList.size());
  EXPECT_EQ(uniq_id, devList[0]);
}

TEST_F(TcpAdapterTest, StoreDataWithSeveralDevices_RestoreData) {
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  const uint32_t count_dev = 10;

  std::shared_ptr<MockTCPDevice> mockdev[count_dev];
  std::string uniq_id[count_dev];
  for (uint32_t i = 0; i < count_dev; i++) {
    char numb[12];
    std::snprintf(numb, 12, "%d", i);
    uniq_id[i] = "unique_device_name" + std::string(numb);
    mockdev[i] = std::make_shared<MockTCPDevice>(port, uniq_id[i]);
    EXPECT_CALL(*(mockdev[i]), IsSameAs(_)).WillRepeatedly(Return(false));
    transport_adapter.AddDevice(mockdev[i]);
  }

  std::vector<std::string> devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(count_dev, devList.size());
  EXPECT_EQ(uniq_id[0], devList[0]);

  const int app_handle = 1;
  std::vector<int> intList = {app_handle};

  ConnectionSPtr mock_connection = std::make_shared<MockConnection>();
  for (uint32_t i = 0; i < count_dev; i++) {
    EXPECT_CALL(transport_adapter, FindDevice(uniq_id[i]))
        .WillOnce(Return(mockdev[i]));
    EXPECT_CALL(*(mockdev[i]), GetApplicationList()).WillOnce(Return(intList));

    EXPECT_CALL(transport_adapter,
                FindEstablishedConnection(uniq_id[i], app_handle))
        .WillOnce(Return(mock_connection));

    EXPECT_CALL(*(mockdev[i]), GetApplicationPort(app_handle))
        .WillOnce(Return(port));
  }
  transport_adapter.CallStore();

  for (uint32_t i = 0; i < count_dev; i++) {
    EXPECT_CALL(transport_adapter, Connect(uniq_id[i], app_handle))
        .WillOnce(Return(TransportAdapter::OK));
  }

  EXPECT_TRUE(transport_adapter.CallRestore());

  devList = transport_adapter.GetDeviceList();
  ASSERT_EQ(count_dev, devList.size());
  for (uint32_t i = 0; i < count_dev; i++) {
    EXPECT_EQ(uniq_id[i], devList[i]);
  }
}

TEST_F(TcpAdapterTest, NotifyTransportConfigUpdated) {
  MockTransportAdapterListener mock_adapter_listener;

  TcpTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);
  transport_adapter.AddListener(&mock_adapter_listener);

  TransportConfig config;
  config[tc_enabled] = std::string("true");
  config[tc_tcp_ip_address] = std::string("192.168.1.1");
  config[tc_tcp_port] = std::string("12345");

  EXPECT_CALL(mock_adapter_listener,
              OnTransportConfigUpdated(&transport_adapter))
      .Times(1);

  transport_adapter.TransportConfigUpdated(config);
}

TEST_F(TcpAdapterTest, GetTransportConfiguration) {
  MockTCPTransportAdapter transport_adapter(
      port, last_state_wrapper_, transport_manager_settings);

  TransportConfig config;
  config[tc_enabled] = std::string("true");
  config[tc_tcp_ip_address] = std::string("192.168.1.1");
  config[tc_tcp_port] = std::string("12345");

  transport_adapter.TransportConfigUpdated(config);

  EXPECT_EQ(config, transport_adapter.GetTransportConfiguration());
}

}  // namespace transport_manager_test
}  // namespace components
}  // namespace test
