// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/client_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AutofillAssistantClient_jni.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace autofill_assistant {
namespace switches {
const char* const kAutofillAssistantServerKey = "autofill-assistant-key";
const char* const kAutofillAssistantUrl = "autofill-assistant-url";
}  // namespace switches

namespace {

const char* const kDefaultAutofillAssistantServerUrl =
    "https://automate-pa.googleapis.com";

// Fills a map from two Java arrays of strings of the same length.
void FillStringMapFromJava(JNIEnv* env,
                           const JavaRef<jobjectArray>& names,
                           const JavaRef<jobjectArray>& values,
                           std::map<std::string, std::string>* parameters) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  DCHECK_EQ(names_vector.size(), values_vector.size());
  for (size_t i = 0; i < names_vector.size(); ++i) {
    parameters->insert(std::make_pair(names_vector[i], values_vector[i]));
  }
}

std::unique_ptr<TriggerContext> CreateTriggerContext(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values) {
  std::map<std::string, std::string> parameters;
  FillStringMapFromJava(env, jparameter_names, jparameter_values, &parameters);
  return TriggerContext::Create(
      std::move(parameters),
      base::android::ConvertJavaStringToUTF8(env, jexperiment_ids));
}

}  // namespace

static base::android::ScopedJavaLocalRef<jobject>
JNI_AutofillAssistantClient_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  ClientAndroid::CreateForWebContents(web_contents);
  return ClientAndroid::FromWebContents(web_contents)->GetJavaObject();
}

ClientAndroid::ClientAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents),
      java_object_(Java_AutofillAssistantClient_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))),
      weak_ptr_factory_(this) {
  server_url_ = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAutofillAssistantUrl);
  if (server_url_.empty()) {
    server_url_ = kDefaultAutofillAssistantServerUrl;
  }
}

ClientAndroid::~ClientAndroid() {
  if (controller_ != nullptr) {
    // In the case of an unexpected closing of the activity or tab, controller_
    // will not yet have been cleaned up (since that happens when a web
    // contents object gets destroyed).
    Metrics::RecordDropOut(Metrics::DropOutReason::CONTENT_DESTROYED);
  }
  Java_AutofillAssistantClient_clearNativePtr(AttachCurrentThread(),
                                              java_object_);
}

base::android::ScopedJavaLocalRef<jobject> ClientAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

bool ClientAndroid::Start(JNIEnv* env,
                          const JavaParamRef<jobject>& jcaller,
                          const JavaParamRef<jstring>& jinitial_url,
                          const JavaParamRef<jstring>& jexperiment_ids,
                          const JavaParamRef<jobjectArray>& parameter_names,
                          const JavaParamRef<jobjectArray>& parameter_values,
                          const JavaParamRef<jobject>& joverlay_coordinator,
                          jlong jservice) {
  std::unique_ptr<Service> service = nullptr;
  if (jservice) {
    service.reset(static_cast<Service*>(reinterpret_cast<void*>(jservice)));
  }
  CreateController(std::move(service));

  // If an overlay is already shown, then show the rest of the UI.
  if (joverlay_coordinator) {
    AttachUI(joverlay_coordinator);
  }

  GURL initial_url(base::android::ConvertJavaStringToUTF8(env, jinitial_url));
  return controller_->Start(
      initial_url, CreateTriggerContext(env, jexperiment_ids, parameter_names,
                                        parameter_values));
}

void ClientAndroid::DestroyUI(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DestroyUI();
}

void ClientAndroid::TransferUITo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jother_web_contents) {
  if (!ui_controller_android_)
    return;

  auto ui_ptr = std::move(ui_controller_android_);
  // From this point on, the UIController, in ui_ptr, is either transferred or
  // deleted.

  if (!jother_web_contents)
    return;

  auto* other_web_contents =
      content::WebContents::FromJavaWebContents(jother_web_contents);
  DCHECK_NE(other_web_contents, web_contents_);

  ClientAndroid* other_client =
      ClientAndroid::FromWebContents(other_web_contents);
  if (!other_client || !other_client->NeedsUI())
    return;

  other_client->ui_controller_android_ = std::move(ui_ptr);
  other_client->AttachUI();
}

base::android::ScopedJavaLocalRef<jstring> ClientAndroid::GetPrimaryAccountName(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  CoreAccountInfo account_info =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetPrimaryAccountInfo();
  return base::android::ConvertUTF8ToJavaString(env, account_info.email);
}

void ClientAndroid::OnAccessToken(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  jboolean success,
                                  const JavaParamRef<jstring>& access_token) {
  if (fetch_access_token_callback_) {
    std::move(fetch_access_token_callback_)
        .Run(success, base::android::ConvertJavaStringToUTF8(access_token));
  }
}

void ClientAndroid::ListDirectActions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jargument_names,
    const base::android::JavaParamRef<jobjectArray>& jargument_values,
    const base::android::JavaParamRef<jobject>& jcallback) {
  if (!controller_)
    CreateController(nullptr);

  base::android::ScopedJavaGlobalRef<jobject> scoped_jcallback(env, jcallback);
  controller_->Track(
      CreateTriggerContext(env, jexperiment_ids, jargument_names,
                           jargument_values),
      base::BindOnce(&ClientAndroid::OnListDirectActions,
                     weak_ptr_factory_.GetWeakPtr(), scoped_jcallback));
}

void ClientAndroid::OnListDirectActions(
    const base::android::JavaRef<jobject>& jcallback) {
  // Using a set here helps remove duplicates.
  std::set<std::string> names;
  for (const UserAction& user_action : controller_->GetUserActions()) {
    if (!user_action.enabled())
      continue;

    for (const std::string& name : user_action.direct_action().names) {
      names.insert(name);
    }
  }

  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_sendDirectActionList(
      env, java_object_, jcallback,
      base::android::ToJavaArrayOfStrings(
          env, std::vector<std::string>(names.begin(), names.end())));
}

bool ClientAndroid::PerformDirectAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jaction_name,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jargument_names,
    const base::android::JavaParamRef<jobjectArray>& jargument_values,
    const base::android::JavaParamRef<jobject>& joverlay_coordinator) {
  // It's too late to create a controller. This should have been done in
  // ListDirectActions.
  if (!controller_)
    return false;

  std::string action_name =
      base::android::ConvertJavaStringToUTF8(env, jaction_name);

  const std::vector<UserAction>& user_actions = controller_->GetUserActions();
  int user_action_count = user_actions.size();
  for (int i = 0; i < user_action_count; i++) {
    const UserAction& user_action = user_actions[i];
    if (!user_action.enabled())
      continue;

    const std::set<std::string>& action_names =
        user_action.direct_action().names;
    if (action_names.count(action_name) != 0) {
      // If an overlay is already shown, then show the rest of the UI
      // immediately.
      if (joverlay_coordinator) {
        AttachUI(joverlay_coordinator);
      }

      return controller_->PerformUserActionWithContext(
          i, CreateTriggerContext(env, jexperiment_ids, jargument_names,
                                  jargument_values));
    }
  }
  return false;
}

void ClientAndroid::AttachUI() {
  AttachUI(nullptr);
}

void ClientAndroid::AttachUI(
    const JavaParamRef<jobject>& joverlay_coordinator) {
  if (!ui_controller_android_) {
    ui_controller_android_ = UiControllerAndroid::CreateFromWebContents(
        web_contents_, joverlay_coordinator);
    if (!ui_controller_android_) {
      // The activity is not or not yet in a mode where attaching the UI is
      // possible.
      return;
    }
  }

  if (!ui_controller_android_->IsAttached()) {
    if (!controller_)
      CreateController(nullptr);

    ui_controller_android_->Attach(web_contents_, this, controller_.get());
  }
}

void ClientAndroid::DestroyUI() {
  ui_controller_android_.reset();
}

std::string ClientAndroid::GetApiKey() {
  std::string api_key;
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    api_key = chrome::GetChannel() == version_info::Channel::STABLE
                  ? google_apis::GetAPIKey()
                  : google_apis::GetNonStableAPIKey();
  }
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAutofillAssistantServerKey)) {
    api_key = command_line->GetSwitchValueASCII(
        switches::kAutofillAssistantServerKey);
  }
  return api_key;
}

std::string ClientAndroid::GetAccountEmailAddress() {
  JNIEnv* env = AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getAccountEmailAddress(env, java_object_));
}

AccessTokenFetcher* ClientAndroid::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* ClientAndroid::GetPersonalDataManager() {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
}

std::string ClientAndroid::GetServerUrl() {
  return server_url_;
}

std::string ClientAndroid::GetLocale() {
  return base::android::GetDefaultLocaleString();
}

std::string ClientAndroid::GetCountryCode() {
  return base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getCountryCode(AttachCurrentThread(),
                                                  java_object_));
}

void ClientAndroid::Shutdown(Metrics::DropOutReason reason) {
  if (!controller_)
    return;

  if (ui_controller_android_ && ui_controller_android_->IsAttached())
    DestroyUI();

  Metrics::RecordDropOut(reason);

  // Delete the controller in a separate task. This avoids tricky ordering
  // issues when Shutdown is called from the controller.
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(&ClientAndroid::DestroyController,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ClientAndroid::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);

  fetch_access_token_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_fetchAccessToken(env, java_object_);
}

void ClientAndroid::InvalidateAccessToken(const std::string& access_token) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_invalidateAccessToken(
      env, java_object_,
      base::android::ConvertUTF8ToJavaString(env, access_token));
}

void ClientAndroid::CreateController(std::unique_ptr<Service> service) {
  if (controller_) {
    return;
  }
  controller_ = std::make_unique<Controller>(
      web_contents_, /* client= */ this, base::DefaultTickClock::GetInstance(),
      std::move(service));
}

void ClientAndroid::DestroyController() {
  controller_.reset();
}

bool ClientAndroid::NeedsUI() {
  return !ui_controller_android_ && controller_ && controller_->NeedsUI();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientAndroid)

}  // namespace autofill_assistant.
