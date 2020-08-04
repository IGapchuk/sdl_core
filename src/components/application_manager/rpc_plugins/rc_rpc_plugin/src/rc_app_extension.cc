/*
 Copyright (c) 2018, Ford Motor Company
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following
 disclaimer in the documentation and/or other materials provided with the
 distribution.

 Neither the name of the Ford Motor Company nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

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

#include "rc_rpc_plugin/rc_app_extension.h"
#include <algorithm>
#include "rc_rpc_plugin/rc_module_constants.h"
#include "smart_objects/smart_object.h"
#include "utils/logger.h"

CREATE_LOGGERPTR_GLOBAL(logger_, "RCAppExtension")

namespace rc_rpc_plugin {
RCAppExtension::RCAppExtension(application_manager::AppExtensionUID uid,
                               application_manager::Application& application)
    : AppExtension(uid), application_(application) {}

void RCAppExtension::SubscribeToInteriorVehicleData(const ModuleUid& module) {
  subscribed_interior_vehicle_data_.insert(module);
  UpdateHash();
}

void RCAppExtension::UnsubscribeFromInteriorVehicleData(
    const ModuleUid& module) {
  subscribed_interior_vehicle_data_.erase(module);
  UpdateHash();
}

void RCAppExtension::UnsubscribeFromInteriorVehicleDataOfType(
    const std::string& module_type) {
  bool unsubscribed = false;
  for (auto& item : subscribed_interior_vehicle_data_) {
    if (module_type == item.first) {
      subscribed_interior_vehicle_data_.erase(item);
      unsubscribed = true;
    }
  }

  if (unsubscribed) {
    // If didin't unsubscribe from some module type, we don't need to update
    // application hash
    UpdateHash();
  }
}

void RCAppExtension::UnsubscribeFromInteriorVehicleData() {
  subscribed_interior_vehicle_data_.clear();
  UpdateHash();
}

bool RCAppExtension::IsSubscribedToInteriorVehicleDataOfType(
    const std::string& module_type) {
  auto it = std::find_if(subscribed_interior_vehicle_data_.begin(),
                         subscribed_interior_vehicle_data_.end(),
                         [&module_type](ModuleUid module) -> bool {
                           return module_type == module.first;
                         });

  return (it != subscribed_interior_vehicle_data_.end());
}

bool RCAppExtension::IsSubscribedToInteriorVehicleData(
    const ModuleUid& module) {
  std::set<ModuleUid>::iterator it =
      subscribed_interior_vehicle_data_.find(module);

  return (it != subscribed_interior_vehicle_data_.end());
}

void RCAppExtension::SaveResumptionData(
    smart_objects::SmartObject& resumption_data) {
  LOG4CXX_AUTO_TRACE(logger_);

  if (subscribed_interior_vehicle_data_.empty()) {
    LOG4CXX_DEBUG(logger_, "Subscribed modules data is absent");
    return;
  }

  resumption_data[message_params::kModuleData] =
      smart_objects::SmartObject(smart_objects::SmartType_Array);
  auto& module_data = resumption_data[message_params::kModuleData];

  uint32_t index = 0;
  for (const auto& module_uid : subscribed_interior_vehicle_data_) {
    LOG4CXX_DEBUG(logger_,
                  "Save module: [" << module_uid.first << ":"
                                   << module_uid.second << "]");
    auto module_info_so =
        smart_objects::SmartObject(smart_objects::SmartType_Map);
    module_info_so[message_params::kModuleType] = module_uid.first;
    module_info_so[message_params::kModuleId] = module_uid.second;
    module_data[index++] = module_info_so;
  }
}

void RCAppExtension::ProcessResumption(
    const smart_objects::SmartObject& saved_app,
    resumption::Subscriber subscriber) {}

void RCAppExtension::RevertResumption(
    const smart_objects::SmartObject& subscriptions) {}

std::set<ModuleUid> RCAppExtension::InteriorVehicleDataSubscriptions() const {
  return subscribed_interior_vehicle_data_;
}

Grid RCAppExtension::GetUserLocation() const {
  return user_location_;
}

void RCAppExtension::SetUserLocation(
    const ns_smart_device_link::ns_smart_objects::SmartObject& user_location) {
  const auto grid = user_location[strings::kGrid];
  const int32_t col = grid[strings::kCol].asInt();
  const int32_t row = grid[strings::kRow].asInt();
  const int32_t level = grid[strings::kLevel].asInt();
  const int32_t colspan = grid[strings::kColspan].asInt();
  const int32_t rowspan = grid[strings::kRowspan].asInt();
  const int32_t levelspan = grid[strings::kLevelspan].asInt();
  user_location_ = Grid(col, row, level, colspan, rowspan, levelspan);
}

void RCAppExtension::SetUserLocation(const Grid& grid) {
  user_location_ = grid;
}

void RCAppExtension::UpdateHash() {
  application_.UpdateHash();
}

RCAppExtension::~RCAppExtension() {}
}  //  namespace rc_rpc_plugin
