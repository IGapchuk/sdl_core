/*

 Copyright (c) 2013, Ford Motor Company
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

#ifndef SRC_COMPONENTS_APPLICATION_MANAGER_INCLUDE_APPLICATION_MANAGER_COMMANDS_MOBILE_PERFORM_INTERACTION_REQUEST_H_
#define SRC_COMPONENTS_APPLICATION_MANAGER_INCLUDE_APPLICATION_MANAGER_COMMANDS_MOBILE_PERFORM_INTERACTION_REQUEST_H_

#include <algorithm>

#include "application_manager/commands/command_request_impl.h"
#include "application_manager/application.h"
#include "utils/macro.h"

namespace application_manager {

class Application;

namespace commands {

/**
 * @brief PerformInteractionRequest command class
 **/
class PerformInteractionRequest : public CommandRequestImpl {
 public:
  /**
   * @brief PerformInteractionRequest class constructor
   *
   * @param message Incoming SmartObject message
   **/
  PerformInteractionRequest(const MessageSharedPtr& message,
                            ApplicationManager& application_manager);

  /**
   * @brief PerformInteractionRequest class destructor
   **/
  virtual ~PerformInteractionRequest();

  /**
   * @brief Initialize request params
   **/
  bool Init() OVERRIDE;

  /**
   * @brief Execute command
   **/
  void Run() OVERRIDE;

  /**
   * @brief Interface method that is called whenever new event received
   *
   * @param event The received event
   */
  virtual void on_event(const event_engine::Event& event) OVERRIDE;

#if BUILD_TESTS
  enum CheckMethod { kCheckVrSynonyms = 0, kCheckMenuNames, kCheckVrHelpItem };

  /**
   * @brief Method for easier check methods in unit tests.
   * Needed because Run method have ways which cannot be validated.
   */
  bool CallCheckMethod(CheckMethod);
#endif  // BUILD_TESTS

  /*
   * @brief Function is called by RequestController when request execution time
   * has exceed it's limit
   *
   */
  void onTimeOut() OVERRIDE;

 private:
  /*
   * @brief Function will be called when VR_OnCommand event
   * comes
   *
   * @param message which should send to mobile side
   *
   */
  void ProcessVRResponse(const smart_objects::SmartObject& message,
                         smart_objects::SmartObject& msg_params);

  /*
   * @brief Sends PerformInteraction response to mobile side
   *
   * @param message which should send to mobile side
   *
   */
  void ProcessPerformInteractionResponse(
      const smart_objects::SmartObject& message,
      smart_objects::SmartObject& msg_params);

  /*
   * @brief Sends UI PerformInteraction request to HMI
   *
   * @param app_id Application ID
   *
   */
  void SendUIPerformInteractionRequest(
      application_manager::ApplicationSharedPtr const app);

  /*
   * @brief Sends TTS PerformInteraction request to HMI
   *
   * @param app_id Application ID
   *
   */
  void SendVRPerformInteractionRequest(
      application_manager::ApplicationSharedPtr const app);

  /*
   * @brief Sends UI Show VR help request to HMI
   *
   * @param app_id Application ID
   */
  void SendUIShowVRHelpRequest(ApplicationSharedPtr const app);

  /*
   * @brief Checks incoming choice set by checker.
   *
   * @param app shared pointer to Application
   * @param checker a functor which checks choises;
   *
   * return Return TRUE if checker returned TRUE
   * for each choice in a choice set form the list,
   * otherwise FALSE
   */
  template <typename FunctorT>
  bool ProcessChoiceSet(const application_manager::ApplicationSharedPtr app,
                        FunctorT checker);

  /*
   * @brief Checks if request with non-sequential positions of vrHelpItems
   * SDLAQ-CRS-466
   *
   * @param app_id Application ID
   *
   * @return TRUE if vrHelpItems positions are sequential,
   * otherwise FALSE
   */
  bool CheckVrHelpItemPositions(
      application_manager::ApplicationSharedPtr const app);

  /*
   * @brief Disable PerformInteraction state in application and
   * delete VR commands from HMI
   */
  void DisablePerformInteraction();

  /**
   * @brief Checks perform interaction params(initialText, initialPrompt, ...).
   * When type is String there is a check on the contents \t\n \\t \\n
   * @return if perform interaction contains \t\n \\t \\n return TRUE,
   * FALSE otherwise
   */
  bool IsWhiteSpaceExist();

  /**
   * @brief Help function for IsWhiteSpaceExist, which validate one of fields
   * for white space existence
   * @param field_name - name for find field in message smart object.
   * Field must have type Array of Strings
   * and could have [image][value] field inside
   * @return false if whitespace not exists and vice versa
   */
  bool IsFieldHaveWhiteSpaces(const std::string& field_name);

  /**
   * @brief Send HMI close PopUp and call DisablePerformInteraction
   */
  void TerminatePerformInteraction();

  /**
   * @brief Checks parameter ChoiceID from response.
   * @param app contains pointer to application
   * @param choice_id contains ChoiceID from response.
   * @return if ChoiceID from response is correct method returns TRUE
   * otherwise returns FALSE.
   */
  bool CheckChoiceIDFromResponse(ApplicationSharedPtr app, int32_t choice_id);
  /**
   * @brief Checks for a match of choice ID, in
   * choice sets.
   * @param app contains pointer to application.
   * @param choice_set_id_list_length contains amount
   * of choice set ids.
   * @param choice_set_id_list array of choice set ids
   * @return If request contains several choice sets with
   * same choice id returns false, otherwise returns
   * true.
   */
  bool CheckChoiceIDFromRequest(
      ApplicationSharedPtr app,
      const size_t choice_set_id_list_length,
      const smart_objects::SmartObject& choice_set_id_list) const;

  /**
   * @brief Tells if there are sent requests without responses
   * @return If there is request without response method returns TRUE
   * otherwise returns FALSE
   */
  const bool HasHMIResponsesToWait() const;

  /**
   * @brief Check VR response result code, in case GENERIC_ERROR, REJECTED,
   * send resultCode FALSE, in case WARNINGS send resultCode TRUE
   */
  void CheckResponseResultCode();

  /**
   * @brief Check UI & VR result codes, send response to mobile
   * @param msg_param Message params to send
   */
  void SendBothModeResponse(const smart_objects::SmartObject& msg_param);

  mobile_apis::InteractionMode::eType interaction_mode_;
  bool ui_response_recived_;
  bool vr_response_recived_;
  bool ui_result_;
  bool vr_result_;
  bool app_pi_was_active_before_;
  static uint32_t pi_requests_count_;
  mobile_apis::Result::eType vr_resultCode_;
  mobile_apis::Result::eType ui_resultCode_;

  DISALLOW_COPY_AND_ASSIGN(PerformInteractionRequest);
};

template <typename FunctorT>
bool PerformInteractionRequest::ProcessChoiceSet(
    const application_manager::ApplicationSharedPtr app, FunctorT checker) {
  SDL_AUTO_TRACE();
  using smart_objects::SmartObject;
  using smart_objects::SmartArray;
  SmartObject& choice_set_list =
      (*message_)[strings::msg_params][strings::interaction_choice_set_id_list];

  for (size_t choice_set_i = 0; choice_set_i < choice_set_list.length();
       ++choice_set_i) {
    const SmartObject* const choice_set_ptr =
        app->FindChoiceSet(choice_set_list[choice_set_i].asInt());
    if (!choice_set_ptr) {
      SDL_ERROR("Invalid choice set ID");
      SendResponse(false, mobile_apis::Result::INVALID_ID);
      return false;
    }

    const SmartArray* choice_set =
        (*choice_set_ptr)[strings::choice_set].asArray();

    DCHECK_OR_RETURN(choice_set, false);

    const SmartArray::const_iterator it =
        std::find_if_not(choice_set->begin(), choice_set->end(), checker);
    if (it != choice_set->end()) {
      SDL_ERROR(checker.error_msg_);
      SendResponse(false, checker.result_code_, checker.error_msg_);
      return false;
    }
  }
  return true;
}

}  // namespace commands
}  // namespace application_manager

#endif  // SRC_COMPONENTS_APPLICATION_MANAGER_INCLUDE_APPLICATION_MANAGER_COMMANDS_MOBILE_PERFORM_INTERACTION_REQUEST_H_
