/**
* Copyright (c) 2013, Ford Motor Company
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

#include "application_manager/application_manager_impl.h"
#include "application_manager/application.h"
#include "application_manager/mobile_command_factory.h"
#include "application_manager/hmi_command_factory.h"
#include "application_manager/message_chaining.h"
#include "application_manager/audio_pass_thru_thread_impl.h"
#include "application_manager/message_helper.h"
#include "connection_handler/connection_handler_impl.h"
#include "mobile_message_handler/mobile_message_handler_impl.h"
#include "formatters/formatter_json_rpc.h"
#include "formatters/CFormatterJsonSDLRPCv2.hpp"
#include "config_profile/profile.h"
#include "utils/threads/thread.h"
#include "utils/logger.h"
#include "./from_hmh_thread_impl.h"
#include "./to_hmh_thread_impl.h"
#include "./from_mobile_thread_impl.h"
#include "./to_mobile_thread_impl.h"

namespace application_manager {

log4cxx::LoggerPtr ApplicationManagerImpl::logger_   =
  log4cxx::LoggerPtr(log4cxx::Logger::getLogger("ApplicationManager"));

namespace formatters = NsSmartDeviceLink::NsJSONHandler::Formatters;
namespace jhs = NsSmartDeviceLink::NsJSONHandler::strings;

ApplicationManagerImpl::ApplicationManagerImpl()
  : audio_pass_thru_flag_(false),
    perform_audio_thread_(NULL),
    is_distracting_driver_(false),
    is_vr_session_strated_(false),
    hmi_cooperating_(false),
    is_all_apps_allowed_(true),
    ui_language_(hmi_apis::Common_Language::INVALID_ENUM),
    vr_language_(hmi_apis::Common_Language::INVALID_ENUM),
    tts_language_(hmi_apis::Common_Language::INVALID_ENUM),
    vehicle_type_(NULL),
    hmi_handler_(NULL),
    mobile_handler_(NULL),
    connection_handler_(NULL),
    watchdog_(NULL),
    from_mobile_thread_(NULL),
    to_mobile_thread_(NULL),
    from_hmh_thread_(NULL),
    to_hmh_thread_(NULL),
    hmi_so_factory_(NULL) {
  from_mobile_thread_ = new threads::Thread(
    "application_manager::FromMobileThreadImpl",
    new FromMobileThreadImpl(this));
  if (!InitThread(from_mobile_thread_)) {
    return;
  }
  to_mobile_thread_ = new threads::Thread(
    "application_manager::ToMobileThreadImpl",
    new ToMobileThreadImpl(this));
  if (!InitThread(to_mobile_thread_)) {
    return;
  }

  to_hmh_thread_ = new threads::Thread(
    "application_manager::ToHMHThreadImpl",
    new ToHMHThreadImpl(this));
  if (!InitThread(to_hmh_thread_)) {
    return;
  }

  from_hmh_thread_ = new threads::Thread(
    "application_manager::FromHMHThreadImpl",
    new FromHMHThreadImpl(this));
  if (!InitThread(from_hmh_thread_)) {
    return;
  }
}

bool ApplicationManagerImpl::InitThread(threads::Thread* thread) {
  if (!thread) {
    LOG4CXX_ERROR(logger_, "Failed to allocate memory for thread object");
    return false;
  }
  LOG4CXX_INFO(logger_, "Starting thread with stack size " <<
               profile::Profile::instance()->thread_min_stach_size());
  if (!thread->start()) {
    /*startWithOptions(
        threads::ThreadOptions(
          profile::Profile::instance()->thread_min_stach_size()))*/
    LOG4CXX_ERROR(logger_, "Failed to start thread");
    return false;
  }
  return true;
}

ApplicationManagerImpl::~ApplicationManagerImpl() {
  message_chaining_.clear();
  if (perform_audio_thread_) {
    delete perform_audio_thread_;
  }

  if (vehicle_type_) {
    delete vehicle_type_;
  }

  if (watchdog_) {
    delete watchdog_;
  }

  if (from_mobile_thread_) {
    from_mobile_thread_->stop();
    delete from_mobile_thread_;
    from_mobile_thread_ = NULL;
  }

  if (to_hmh_thread_) {
    to_hmh_thread_->stop();
    delete to_hmh_thread_;
    to_hmh_thread_ = NULL;
  }

  if (from_hmh_thread_) {
    from_hmh_thread_->stop();
    delete from_hmh_thread_;
    from_hmh_thread_ = NULL;
  }

  if (to_mobile_thread_) {
    to_mobile_thread_->stop();
    delete to_mobile_thread_;
    to_mobile_thread_ = NULL;
  }
}

ApplicationManagerImpl* ApplicationManagerImpl::instance() {
  static ApplicationManagerImpl instance;
  return &instance;
}

Application* ApplicationManagerImpl::application(int app_id) {
  std::map<int, Application*>::iterator it = applications_.find(app_id);
  if (applications_.end() != it) {
    return it->second;
  } else {
    return NULL;
  }
}

Application* ApplicationManagerImpl::active_application() const {
  // TODO(DK) : check driver distraction
  for (std::set<Application*>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    if ((*it)->IsFullscreen()) {
      return *it;
    }
  }
  return NULL;
}

std::vector<Application*> ApplicationManagerImpl::applications_by_button(
  unsigned int button) {
  std::vector<Application*> result;
  for (std::set<Application*>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    if ((*it)->IsSubscribedToButton(static_cast<mobile_apis::ButtonName::eType>(
                                      button))) {
      result.push_back(*it);
    }
  }
  return result;
}

std::vector<Application*> ApplicationManagerImpl::applications_by_ivi(
  unsigned int vehicle_info) {
  std::vector<Application*> result;
  for (std::set<Application*>::iterator it = application_list_.begin();
       application_list_.end() != it; ++it) {
    if ((*it)->IsSubscribedToIVI(vehicle_info)) {
      result.push_back(*it);
    }
  }
  return result;
}

std::vector<Application*> ApplicationManagerImpl::applications_with_navi() {
  std::vector<Application*> result;
  for (std::set<Application*>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    if ((*it)->SupportsNavigation()) {
      result.push_back(*it);
    }
  }
  return result;
}

const std::set<connection_handler::Device>&
ApplicationManagerImpl::device_list() {
  std::set<connection_handler::Device> devices;
  return devices;
}

bool ApplicationManagerImpl::RegisterApplication(Application* application) {
  DCHECK(application);
  if (NULL == application) {
    return false;
  }

  if (false == is_all_apps_allowed_) {
    LOG4CXX_INFO(logger_,
                 "RegisterApplication: access to app's disabled by user");
    return false;
  }

  std::map<int, Application*>::iterator it = applications_.find(
        application->app_id());
  if (applications_.end() != it) {
    return false;
  }
  applications_.insert(std::pair<int, Application*>(
                         application->app_id(), application));
  application_list_.insert(application);
  return true;
}

bool ApplicationManagerImpl::UnregisterApplication(int app_id) {
  std::map<int, Application*>::iterator it = applications_.find(app_id);
  if (applications_.end() == it) {
    return false;
  }
  Application* app_to_remove = it->second;
  applications_.erase(it);
  application_list_.erase(app_to_remove);
  return true;
}

void ApplicationManagerImpl::UnregisterAllApplications(bool hmi_off) {
  applications_.clear();
  for (std::set<Application*>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    delete(*it);
  }
  application_list_.clear();

  if (hmi_off) {
    hmi_cooperating_ = false;
  }
}

bool ApplicationManagerImpl::RemoveAppDataFromHMI(Application* application) {
  return true;
}

bool ApplicationManagerImpl::LoadAppDataToHMI(Application* application) {
  return true;
}

bool ApplicationManagerImpl::ActivateApplication(Application* application) {
  return true;
}

void ApplicationManagerImpl::ConnectToDevice(unsigned int id) {
  // TODO(VS): Call function from ConnectionHandler
  if (!connection_handler_) {
    LOG4CXX_WARN(logger_, "Connection handler is not set.");
    return;
  }

  connection_handler_->ConnectToDevice(id);
}

void ApplicationManagerImpl::OnHMIStartedCooperation() {
  hmi_cooperating_ = true;
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnHMIStartedCooperation()");
  if (!connection_handler_) {
    LOG4CXX_WARN(logger_, "Connection handler is not set.");
  } else {
    connection_handler_->StartTransportManager();
  }

  smart_objects::CSmartObject* is_vr_ready = new smart_objects::CSmartObject;
  smart_objects::CSmartObject& so_to_send = *is_vr_ready;
  so_to_send[jhs::S_PARAMS][jhs::S_FUNCTION_ID] =
    hmi_apis::FunctionID::VR_IsReady;
  so_to_send[jhs::S_PARAMS][jhs::S_MESSAGE_TYPE] =
    hmi_apis::messageType::request;
  so_to_send[jhs::S_PARAMS][jhs::S_PROTOCOL_VERSION] = 2;
  so_to_send[jhs::S_PARAMS][jhs::S_PROTOCOL_TYPE] = 1;
  so_to_send[jhs::S_PARAMS][jhs::S_CORRELATION_ID] = 4444;
  so_to_send[jhs::S_MSG_PARAMS] =
    smart_objects::CSmartObject(smart_objects::SmartType_Map);

  //  ApplicationManagerImpl::hmi_so_factory_.attachSchema(so_to_send);

  ManageHMICommand(is_vr_ready);
}

bool ApplicationManagerImpl::AddMessageChain(unsigned int connection_key,
    unsigned int correlation_id, const uint64_t hmi_correlation_id,
    const smart_objects::CSmartObject* data) {

    MessageChains::iterator it =  message_chaining_.find(hmi_correlation_id);
    if (message_chaining_.end() != it) {
      it->second->IncrementCounter();
    } else {
      MessageChaining* chain =
          new MessageChaining(connection_key, correlation_id);
      if (chain) {
        if (data) {
          chain->set_data(*data);
        }
        MessageChainPtr ptr(chain);
        message_chaining_[hmi_correlation_id] = ptr;
      } else {
        LOG4CXX_ERROR(logger_, "Null-pointer message received.");
        return false;
      }
    }
    return true;
}

bool ApplicationManagerImpl::DecreaseMessageChain(
  const uint64_t hmi_correlation_id) {
  bool result = false;
  MessageChains::iterator it =
    message_chaining_.find(hmi_correlation_id);

  if (message_chaining_.end() != it) {
    (*it->second).DecrementCounter();
    if (!(*it->second).counter()) {
      result = true;
    }
    message_chaining_.erase(it);
  }

  return result;
}

MessageChaining* ApplicationManagerImpl::GetMessageChain(
  const uint64_t hmi_correlation_id) const {
  MessageChains::const_iterator it =
    message_chaining_.find(hmi_correlation_id);
  if (message_chaining_.end() != it) {
    return &(*it->second);
  }

  return NULL;
}

uint32_t ApplicationManagerImpl::GetMobilecorrelation_id(
  uint64_t correlation_id) const {
  uint32_t mobile_correlation_id = correlation_id >> 32;
  return mobile_correlation_id;
}

uint64_t ApplicationManagerImpl::GetHMIcorrelation_id(
  uint32_t correlation_id,  uint32_t connection_key) const {
  // to avoid warning: left shift count >= width of type
  uint64_t mobile_correlation_id = correlation_id;
  uint64_t conn_key = connection_key;
  uint64_t hmi_correlation_id = connection_key | (mobile_correlation_id << 32);
  return hmi_correlation_id;
}

bool ApplicationManagerImpl::audio_pass_thru_flag() const {
  return audio_pass_thru_flag_;
}

void ApplicationManagerImpl::set_audio_pass_thru_flag(bool flag) {
  audio_pass_thru_flag_ = flag;
}

void ApplicationManagerImpl::set_driver_distraction(
  bool is_distracting) {
  is_distracting_driver_ = is_distracting;
}

void ApplicationManagerImpl::set_vr_session_started(const bool& state) {
  is_vr_session_strated_ = state;
}

void ApplicationManagerImpl::set_active_ui_language(
  const hmi_apis::Common_Language::eType& language) {
  ui_language_ = language;
}

void ApplicationManagerImpl::set_active_vr_language(
  const hmi_apis::Common_Language::eType& language) {
  vr_language_ = language;
}

void ApplicationManagerImpl::set_active_tts_language(
  const hmi_apis::Common_Language::eType& language) {
  tts_language_ = language;
}

void ApplicationManagerImpl::set_vehicle_type(
  const smart_objects::CSmartObject& vehicle_type) {
  if (vehicle_type_) {
    delete vehicle_type_;
  }
  vehicle_type_ = new smart_objects::CSmartObject(vehicle_type);
}

const smart_objects::CSmartObject*
ApplicationManagerImpl::vehicle_type() const {
  return vehicle_type_;
}

void ApplicationManagerImpl::set_all_apps_allowed(const bool& allowed) {
  is_all_apps_allowed_ = allowed;
}

void ApplicationManagerImpl::StartAudioPassThruThread(int session_key,
    int correlation_id, int max_duration, int sampling_rate,
    int bits_per_sample, int audio_type) {
  AudioPassThruThreadImpl* thread_impl = new AudioPassThruThreadImpl(
    static_cast<unsigned int>(session_key),
    static_cast<unsigned int>(correlation_id),
    static_cast<unsigned int>(max_duration),
    static_cast<SamplingRate>(sampling_rate),
    static_cast<AudioCaptureQuality>(bits_per_sample),
    static_cast<AudioType>(audio_type));

  thread_impl->Init();
  perform_audio_thread_ = new threads::Thread("AudioPassThru thread",
      thread_impl);

  perform_audio_thread_->startWithOptions(
    threads::ThreadOptions(threads::Thread::kMinStackSize));
}

void ApplicationManagerImpl::StopAudioPassThruThread() {
  perform_audio_thread_->stop();
  delete perform_audio_thread_;
  perform_audio_thread_ = NULL;
}

void ApplicationManagerImpl::OnMobileMessageReceived(
  const MobileMessage& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnMobileMessageReceived");

  DCHECK(message);
  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    return;
  }

  messages_from_mobile_.push(message);
}

void ApplicationManagerImpl::onMessageReceived(
  utils::SharedPtr<application_manager::Message> message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::onMessageReceived");

  DCHECK(message);
  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    return;
  }

  messages_from_hmh_.push(message);
}

void ApplicationManagerImpl::onErrorSending(
  utils::SharedPtr<application_manager::Message> message) {
  return;
}

void ApplicationManagerImpl::OnDeviceListUpdated(
  const connection_handler::DeviceList& device_list) {
  // TODO(DK): HMI StartDeviceDiscovery response
  smart_objects::CSmartObject* update_list = new smart_objects::CSmartObject;
  smart_objects::CSmartObject& so_to_send = *update_list;
  so_to_send[jhs::S_PARAMS][jhs::S_FUNCTION_ID] =
    hmi_apis::FunctionID::BasicCommunication_OnDeviceListUpdated;
  so_to_send[jhs::S_PARAMS][jhs::S_MESSAGE_TYPE] =
    hmi_apis::messageType::notification;
  so_to_send[jhs::S_PARAMS][jhs::S_PROTOCOL_VERSION] = 2;
  so_to_send[jhs::S_PARAMS][jhs::S_PROTOCOL_TYPE] = 1;
  so_to_send[jhs::S_PARAMS][jhs::S_CORRELATION_ID] = 4435;
  smart_objects::CSmartObject* msg_params =
    MessageHelper::CreateDeviceListSO(device_list);
  if (!msg_params) {
    LOG4CXX_WARN(logger_, "Failed to create sub-smart object.");
    delete update_list;
  }
  so_to_send[jhs::S_MSG_PARAMS] = *msg_params;
  ManageHMICommand(update_list);
}

void ApplicationManagerImpl::RemoveDevice(
  const connection_handler::DeviceHandle device_handle) {
}

void ApplicationManagerImpl::OnSessionStartedCallback(
  connection_handler::DeviceHandle device_handle, int session_key,
  int first_session_key) {
}

void ApplicationManagerImpl::OnSessionEndedCallback(int session_key,
    int first_session_key) {
}

void ApplicationManagerImpl::onTimeoutExpired(request_watchdog::RequestInfo) {
}

void ApplicationManagerImpl::set_hmi_message_handler(
  hmi_message_handler::HMIMessageHandler* handler) {
  hmi_handler_ = handler;
}

void ApplicationManagerImpl::set_mobile_message_handler(
  mobile_message_handler::MobileMessageHandler* handler) {
  mobile_handler_ = handler;
}

void ApplicationManagerImpl::set_connection_handler(
  connection_handler::ConnectionHandler* handler) {
  connection_handler_ = handler;
}

void ApplicationManagerImpl::set_watchdog(
  request_watchdog::Watchdog* watchdog) {
  watchdog_ = watchdog;
}

void ApplicationManagerImpl::StartDevicesDiscovery() {
  connection_handler::ConnectionHandlerImpl::instance()->
  StartDevicesDiscovery();
}

void ApplicationManagerImpl::SendMessageToMobile(
  const utils::SharedPtr<smart_objects::CSmartObject>& message) {
  DCHECK(message);
  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    return;
  }

  if (!mobile_handler_) {
    LOG4CXX_WARN(logger_, "No Mobile Handler set");
    return;
  }

  utils::SharedPtr<Message> message_to_send(new Message);
  if (!ConvertSOtoMessage((*message), (*message_to_send))) {
    LOG4CXX_WARN(logger_,
                 "Cannot send message to Mobile: failed to create string");
    return;
  }

  messages_to_mobile_.push(message_to_send);
}

bool ApplicationManagerImpl::ManageMobileCommand(
  const utils::SharedPtr<smart_objects::CSmartObject>& message) {
  DCHECK(message);
  if (!message) {
    LOG4CXX_WARN(logger_, "Null-pointer message received.");
    return false;
  }
  CommandSharedPtr command = MobileCommandFactory::CreateCommand(message);

  if (!command) {
    LOG4CXX_WARN(logger_, "Failed to create mobile command from smart object");
    return false;
  }

  if (command->Init()) {
    command->Run();
    if (command->CleanUp()) {
      return true;
    }
  }
  return false;
}

void ApplicationManagerImpl::SendMessageToHMI(
  const utils::SharedPtr<smart_objects::CSmartObject>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::SendMessageToHMI");
  DCHECK(message);
  if (!message) {
    LOG4CXX_WARN(logger_, "Null-pointer message received.");
    return;
  }

  if (!hmi_handler_) {
    LOG4CXX_WARN(logger_, "No HMI Handler set");
    return;
  }

  utils::SharedPtr<Message> message_to_send(new Message);
  if (!message_to_send) {
    LOG4CXX_ERROR(logger_, "Null pointer");
    return;
  }

  hmi_so_factory().attachSchema(*message);
  LOG4CXX_INFO(logger_, "Attached schema to message, result if valid: "
               << message->isValid());

  if (!ConvertSOtoMessage(*message, *message_to_send)) {
    LOG4CXX_WARN(logger_,
                 "Cannot send message to HMI: failed to create string");
    return;
  }
  messages_to_hmh_.push(message_to_send);
}

bool ApplicationManagerImpl::ManageHMICommand(
  const utils::SharedPtr<smart_objects::CSmartObject>& message) {
  DCHECK(message);
  if (!message) {
    LOG4CXX_WARN(logger_, "Null-pointer message received.");
    return false;
  }
  CommandSharedPtr command = HMICommandFactory::CreateCommand(message);

  if (!command) {
    LOG4CXX_WARN(logger_, "Failed to create command from smart object");
    return false;
  }

  if (command->Init()) {
    command->Run();
    if (command->CleanUp()) {
      return true;
    }
  }
  return false;
}

void ApplicationManagerImpl::CreateHMIMatrix(HMIMatrix* matrix) {
}

void ApplicationManagerImpl::CreatePoliciesManager(PoliciesManager* managaer) {
}

bool ApplicationManagerImpl::CheckPolicies(smart_objects::CSmartObject* message,
    Application* application) {
  return true;
}

bool ApplicationManagerImpl::CheckHMIMatrix(
  smart_objects::CSmartObject* message) {
  return true;
}

bool ApplicationManagerImpl::ConvertMessageToSO(
  const Message& message, smart_objects::CSmartObject& output) {
  LOG4CXX_INFO(logger_, "Message to convert: protocol " <<
               message.protocol_version() <<
               "; json " << message.json_message());

  switch (message.protocol_version()) {
    case ProtocolVersion::kV2: {
      if (!formatters::CFormatterJsonSDLRPCv2::fromString(
            message.json_message(),
            output,
            message.function_id(),
            message.type(),
            message.correlation_id())) {
        LOG4CXX_WARN(logger_, "Failed to parse string to smart object");
        return false;
      }

      LOG4CXX_INFO(logger_, "Convertion result for sdl object is true" <<
                   " function_id " <<
                   output[jhs::S_PARAMS][jhs::S_FUNCTION_ID].asInt());
      break;
    }
    case ProtocolVersion::kHMI: {
      int result = formatters::FormatterJsonRpc::FromString <
                   hmi_apis::FunctionID::eType, hmi_apis::messageType::eType > (
                     message.json_message(),
                     output);
      LOG4CXX_INFO(logger_, "Convertion result: " <<
                   result << " function id " <<
                   output[jhs::S_PARAMS][jhs::S_FUNCTION_ID].asInt());
      hmi_so_factory().attachSchema(output);
      LOG4CXX_INFO(logger_, "Is object valid? " << output.isValid());
      break;
    }
    default:
      NOTREACHED();
      return false;
  }
  LOG4CXX_INFO(logger_, "Successfully parsed message into smart object");
  return true;
}

bool ApplicationManagerImpl::ConvertSOtoMessage(
  const smart_objects::CSmartObject& message, Message& output) {
  LOG4CXX_INFO(logger_, "Message to convert");

  if (smart_objects::SmartType_Null == message.getType() ||
      smart_objects::SmartType_Invalid == message.getType()) {
    LOG4CXX_WARN(logger_, "Invalid smart object received.");
    return false;
  }

  LOG4CXX_INFO(logger_, "Message with protocol: " <<
               message.getElement(
                 jhs::S_PARAMS).getElement(jhs::S_PROTOCOL_TYPE).asInt());

  std::string output_string;
  switch (message.getElement(
            jhs::S_PARAMS).getElement(jhs::S_PROTOCOL_TYPE).asInt()) {
    case 0: {
      if (!formatters::CFormatterJsonSDLRPCv2::toString(
            message,
            output_string)) {
        LOG4CXX_WARN(logger_, "Failed to serialize smart object");
        return false;
      }
      break;
    }
    case 1: {
      if (!formatters::FormatterJsonRpc::ToString(
            message,
            output_string)) {
        LOG4CXX_WARN(logger_, "Failed to serialize smart object");
        return false;
      }
      break;
    }
    default:
      NOTREACHED();
      return false;
  }

  LOG4CXX_INFO(logger_, "Convertion result: " <<
               output_string);

  output.set_function_id(message.getElement(
                           jhs::S_PARAMS).getElement(
                           jhs::S_FUNCTION_ID).asInt());
  output.set_correlation_id(
    message.getElement(jhs::S_PARAMS).getElement(
      jhs::S_CORRELATION_ID).asInt());
  output.set_message_type(
    static_cast<MessageType>(
      message.getElement(jhs::S_PARAMS).getElement(
        jhs::S_MESSAGE_TYPE).asInt()));
  output.set_json_message(output_string);
  output.set_protocol_version(application_manager::kHMI);

  if (message.keyExists(strings::binary_data)) {
    application_manager::BinaryData* binaryData =
      new application_manager::BinaryData(
      message.getElement(strings::binary_data).asBinary());

    if (NULL == binaryData) {
      LOG4CXX_ERROR(logger_, "Null pointer");
      return false;
    }
    output.set_binary_data(binaryData);
  }

  LOG4CXX_INFO(logger_, "Successfully parsed message into smart object");
  return true;
}

void ApplicationManagerImpl::ProcessMessageFromMobile(
  const utils::SharedPtr<Message>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::ProcessMessageFromMobile()");
  utils::SharedPtr<smart_objects::CSmartObject> smart_object(
    new smart_objects::CSmartObject);

  if (!smart_object) {
    LOG4CXX_ERROR(logger_, "Null pointer");
    return;
  }

  if (!ConvertMessageToSO(*message, *smart_object)) {
    LOG4CXX_ERROR(logger_, "Cannot create smart object from message");
    return;
  }

  LOG4CXX_INFO(logger_, "Converted message, trying to create mobile command");
  if (!ManageMobileCommand(smart_object)) {
    LOG4CXX_ERROR(logger_, "Received command didn't run successfully");
  }
}

void ApplicationManagerImpl::ProcessMessageFromHMI(
  const utils::SharedPtr<Message>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::ProcessMessageFromHMI()");
  utils::SharedPtr<smart_objects::CSmartObject> smart_object(
    new smart_objects::CSmartObject);

  if (!smart_object) {
    LOG4CXX_ERROR(logger_, "Null pointer");
    return;
  }

  if (!ConvertMessageToSO(*message, *smart_object)) {
    LOG4CXX_ERROR(logger_, "Cannot create smart object from message");
    return;
  }

  LOG4CXX_INFO(logger_, "Converted message, trying to create hmi command");
  if (!ManageHMICommand(smart_object)) {
    LOG4CXX_ERROR(logger_, "Received command didn't run successfully");
  }
}

hmi_apis::HMI_API& ApplicationManagerImpl::hmi_so_factory() {
  if (!hmi_so_factory_) {
    hmi_so_factory_ = new hmi_apis::HMI_API;
  }
  return *hmi_so_factory_;
}

}  // namespace application_manager
