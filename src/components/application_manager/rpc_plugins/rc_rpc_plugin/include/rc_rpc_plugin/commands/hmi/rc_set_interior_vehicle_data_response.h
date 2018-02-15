#ifndef SRC_COMPONENTS_APPLICATION_MANAGER_RPC_PLUGINS_RC_RPC_PLUGIN_INCLUDE_COMMANDS_HMI_SET_INTERIOR_VEHICLE_DATA_RESPONSE_H
#define SRC_COMPONENTS_APPLICATION_MANAGER_RPC_PLUGINS_RC_RPC_PLUGIN_INCLUDE_COMMANDS_HMI_SET_INTERIOR_VEHICLE_DATA_RESPONSE_H

#include "application_manager/commands/response_from_hmi.h"
#include "utils/macro.h"

namespace rc_rpc_plugin {
namespace app_mngr = application_manager;

namespace commands {
class RCSetInteriorVehicleDataResponse
    : public application_manager::commands::ResponseFromHMI {
 public:
  RCSetInteriorVehicleDataResponse(
      const app_mngr::commands::MessageSharedPtr& message,
      app_mngr::ApplicationManager& application_manager,
      app_mngr::rpc_service::RPCService& rpc_service,
      app_mngr::HMICapabilities& hmi_capabilities,
      policy::PolicyHandlerInterface& policy_handle);
  bool Init() OVERRIDE;
  void Run() OVERRIDE;
  void on_event(const application_manager::event_engine::Event& event);
};
}  // namespace commands
}  // namespace rc_rpc_plugin

#endif  // SRC_COMPONENTS_APPLICATION_MANAGER_RPC_PLUGINS_RC_RPC_PLUGIN_INCLUDE_COMMANDS_HMI_SET_INTERIOR_VEHICLE_DATA_RESPONSE_H