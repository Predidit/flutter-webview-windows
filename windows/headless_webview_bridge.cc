#include "headless_webview_bridge.h"

#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_result_functions.h>
#include <flutter/standard_method_codec.h>

#include <format>
#include <random>
#include <sstream>
#include <iomanip>

namespace {
constexpr auto kErrorInvalidArgs = "invalidArguments";

constexpr auto kMethodLoadUrl = "loadUrl";
constexpr auto kMethodLoadStringContent = "loadStringContent";
constexpr auto kMethodReload = "reload";
constexpr auto kMethodStop = "stop";
constexpr auto kMethodGoBack = "goBack";
constexpr auto kMethodGoForward = "goForward";
constexpr auto kMethodAddScriptToExecuteOnDocumentCreated =
    "addScriptToExecuteOnDocumentCreated";
constexpr auto kMethodRemoveScriptToExecuteOnDocumentCreated =
    "removeScriptToExecuteOnDocumentCreated";
constexpr auto kMethodExecuteScript = "executeScript";
constexpr auto kMethodPostWebMessage = "postWebMessage";
constexpr auto kMethodSetUserAgent = "setUserAgent";
constexpr auto kMethodSuspend = "suspend";
constexpr auto kMethodResume = "resume";
constexpr auto kMethodSetVirtualHostNameMapping = "setVirtualHostNameMapping";
constexpr auto kMethodClearVirtualHostNameMapping =
    "clearVirtualHostNameMapping";
constexpr auto kMethodClearCookies = "clearCookies";
constexpr auto kMethodClearCache = "clearCache";
constexpr auto kMethodGetCookies = "getCookies";
constexpr auto kMethodSetCacheDisabled = "setCacheDisabled";
constexpr auto kMethodSetPopupWindowPolicy = "setPopupWindowPolicy";

constexpr auto kEventType = "type";
constexpr auto kEventValue = "value";

constexpr auto kErrorNotSupported = "not_supported";
constexpr auto kScriptFailed = "script_failed";
constexpr auto kMethodFailed = "method_failed";

static std::string GenerateWebviewId() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  
  std::string id = "headless_";
  for (int i = 0; i < 8; ++i) {
    int value = dis(gen);
    id += static_cast<char>((value < 10) ? ('0' + value) : ('a' + value - 10));
  }
  return id;
}

}  // namespace

HeadlessWebviewBridge::HeadlessWebviewBridge(
    flutter::BinaryMessenger* messenger,
    std::unique_ptr<Webview> webview)
    : webview_(std::move(webview)),
      webview_id_(GenerateWebviewId()),
      messenger_(messenger) {

  const auto method_channel_name =
      std::format("io.jns.webview.win.headless/{}", webview_id_);
  method_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          messenger, method_channel_name,
          &flutter::StandardMethodCodec::GetInstance());
  method_channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    HandleMethodCall(call, std::move(result));
  });

  const auto event_channel_name =
      std::format("io.jns.webview.win.headless/{}/events", webview_id_);
  event_channel_ =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          messenger, event_channel_name,
          &flutter::StandardMethodCodec::GetInstance());

  auto handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [this](const flutter::EncodableValue* arguments,
             std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&&
                 events) {
        event_sink_ = std::move(events);
        return nullptr;
      },
      [this](const flutter::EncodableValue* arguments) {
        event_sink_.reset();
        return nullptr;
      });

  event_channel_->SetStreamHandler(std::move(handler));
}

HeadlessWebviewBridge::~HeadlessWebviewBridge() {
  method_channel_->SetMethodCallHandler(nullptr);
  event_sink_.reset();
}

void HeadlessWebviewBridge::RegisterEventHandlers() {
  webview_->OnUrlChanged([this](const std::string& url) {
    if (event_sink_) {
      event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
          {flutter::EncodableValue(kEventType),
           flutter::EncodableValue("urlChanged")},
          {flutter::EncodableValue(kEventValue), flutter::EncodableValue(url)},
      }));
    }
  });

  webview_->OnLoadError([this](COREWEBVIEW2_WEB_ERROR_STATUS web_status) {
    if (event_sink_) {
      event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
          {flutter::EncodableValue(kEventType),
           flutter::EncodableValue("onLoadError")},
          {flutter::EncodableValue(kEventValue),
           flutter::EncodableValue(static_cast<int>(web_status))},
      }));
    }
  });

  webview_->OnLoadingStateChanged([this](WebviewLoadingState state) {
    if (event_sink_) {
      event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
          {flutter::EncodableValue(kEventType),
           flutter::EncodableValue("loadingStateChanged")},
          {flutter::EncodableValue(kEventValue),
           flutter::EncodableValue(static_cast<int>(state))},
      }));
    }
  });

  webview_->OnDevtoolsProtocolEvent([this](const std::string& json) {
    if (event_sink_) {
      event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
          {flutter::EncodableValue(kEventType),
           flutter::EncodableValue("securityStateChanged")},
          {flutter::EncodableValue(kEventValue), flutter::EncodableValue(json)},
      }));
    }
  });

  webview_->OnDocumentTitleChanged([this](const std::string& title) {
    if (event_sink_) {
      event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
          {flutter::EncodableValue(kEventType),
           flutter::EncodableValue("titleChanged")},
          {flutter::EncodableValue(kEventValue),
           flutter::EncodableValue(title)},
      }));
    }
  });

  webview_->OnWebMessageReceived([this](const std::string& message) {
    if (event_sink_) {
      event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
          {flutter::EncodableValue(kEventType),
           flutter::EncodableValue("webMessageReceived")},
          {flutter::EncodableValue(kEventValue),
           flutter::EncodableValue(message)},
      }));
    }
  });

  webview_->OnPermissionRequested(
      [this](const std::string& url, WebviewPermissionKind kind,
             bool is_user_initiated,
             Webview::WebviewPermissionRequestedCompleter completer) {
        OnPermissionRequested(url, kind, is_user_initiated, completer);
      });

  webview_->OnWebResourceResponseReceived(
      [this](const std::string& url, const std::string& method, const std::string& response_body) {
        if (event_sink_) {
          event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
              {flutter::EncodableValue(kEventType),
               flutter::EncodableValue("onM3USourceLoaded")},
              {flutter::EncodableValue(kEventValue),
               flutter::EncodableValue(flutter::EncodableMap{
                   {"url", flutter::EncodableValue(url)},
                   {"method", flutter::EncodableValue(method)},
                   {"responseBody", flutter::EncodableValue(response_body)},
               })},
          }));
        }
      });

  webview_->OnVideoSourceLoaded(
      [this](const std::string& url, const std::string& method, const std::string& content_type) {
        if (event_sink_) {
          event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
              {flutter::EncodableValue(kEventType),
               flutter::EncodableValue("onVideoSourceLoaded")},
              {flutter::EncodableValue(kEventValue),
               flutter::EncodableValue(flutter::EncodableMap{
                   {"url", flutter::EncodableValue(url)},
                   {"method", flutter::EncodableValue(method)},
                   {"contentType", flutter::EncodableValue(content_type)},
               })},
          }));
        }
      });

  webview_->OnSourceLoaded(
      [this](const std::string& url, const std::string& method, const std::string& content_type) {
        if (event_sink_) {
          event_sink_->Success(flutter::EncodableValue(flutter::EncodableMap{
              {flutter::EncodableValue(kEventType),
               flutter::EncodableValue("onSourceLoaded")},
              {flutter::EncodableValue(kEventValue),
               flutter::EncodableValue(flutter::EncodableMap{
                   {"url", flutter::EncodableValue(url)},
                   {"method", flutter::EncodableValue(method)},
                   {"contentType", flutter::EncodableValue(content_type)},
               })},
          }));
        }
      });
}

void HeadlessWebviewBridge::OnPermissionRequested(
    const std::string& url,
    WebviewPermissionKind permissionKind,
    bool isUserInitiated,
    Webview::WebviewPermissionRequestedCompleter completer) {
  auto args = std::make_unique<flutter::EncodableValue>(flutter::EncodableMap{
      {"url", url},
      {"isUserInitiated", isUserInitiated},
      {"permissionKind", static_cast<int>(permissionKind)}});

  method_channel_->InvokeMethod(
      "permissionRequested", std::move(args),
      std::make_unique<flutter::MethodResultFunctions<flutter::EncodableValue>>(
          [completer](const flutter::EncodableValue* result) {
            if (const auto allow = std::get_if<bool>(result)) {
              completer(*allow ? WebviewPermissionState::Allow
                               : WebviewPermissionState::Deny);
            } else {
              completer(WebviewPermissionState::Default);
            }
          },
          [completer](const std::string& error_code,
                      const std::string& error_message,
                      const flutter::EncodableValue* error_details) {
            completer(WebviewPermissionState::Deny);
          },
          [completer]() { completer(WebviewPermissionState::Default); }));
}

void HeadlessWebviewBridge::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const auto& method_name = method_call.method_name();

  // loadUrl: string
  if (method_name.compare(kMethodLoadUrl) == 0) {
    if (const auto url = std::get_if<std::string>(method_call.arguments())) {
      webview_->LoadUrl(*url);
      result->Success();
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // loadStringContent: string
  if (method_name.compare(kMethodLoadStringContent) == 0) {
    if (const auto content =
            std::get_if<std::string>(method_call.arguments())) {
      webview_->LoadStringContent(*content);
      result->Success();
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // reload
  if (method_name.compare(kMethodReload) == 0) {
    if (webview_->Reload()) {
      result->Success();
    } else {
      result->Error(kMethodFailed);
    }
    return;
  }

  // stop
  if (method_name.compare(kMethodStop) == 0) {
    if (webview_->Stop()) {
      result->Success();
    } else {
      result->Error(kMethodFailed);
    }
    return;
  }

  // goBack
  if (method_name.compare(kMethodGoBack) == 0) {
    if (webview_->GoBack()) {
      result->Success();
    } else {
      result->Error(kMethodFailed);
    }
    return;
  }

  // goForward
  if (method_name.compare(kMethodGoForward) == 0) {
    if (webview_->GoForward()) {
      result->Success();
    } else {
      result->Error(kMethodFailed);
    }
    return;
  }

  // suspend
  if (method_name.compare(kMethodSuspend) == 0) {
    webview_->Suspend();
    result->Success();
    return;
  }

  // resume
  if (method_name.compare(kMethodResume) == 0) {
    webview_->Resume();
    result->Success();
    return;
  }

  // setVirtualHostNameMapping [string hostName, string path, int accessKind]
  if (method_name.compare(kMethodSetVirtualHostNameMapping) == 0) {
    if (const auto args = std::get_if<flutter::EncodableList>(method_call.arguments())) {
      if (args->size() == 3) {
        const auto host_name = std::get_if<std::string>(&(*args)[0]);
        const auto path = std::get_if<std::string>(&(*args)[1]);
        const auto access_kind = std::get_if<int>(&(*args)[2]);
        
        if (host_name && path && access_kind) {
          webview_->SetVirtualHostNameMapping(
              *host_name, *path, static_cast<WebviewHostResourceAccessKind>(*access_kind));
          result->Success();
          return;
        }
      }
    }
    result->Error(kErrorInvalidArgs);
    return;
  }

  // clearVirtualHostNameMapping: string
  if (method_name.compare(kMethodClearVirtualHostNameMapping) == 0) {
    if (const auto host_name = std::get_if<std::string>(method_call.arguments())) {
      if (webview_->ClearVirtualHostNameMapping(*host_name)) {
        result->Success();
      } else {
        result->Error(kMethodFailed);
      }
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  if (method_name.compare(kMethodAddScriptToExecuteOnDocumentCreated) == 0) {
    if (const auto script = std::get_if<std::string>(method_call.arguments())) {
      std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>
          shared_result = std::move(result);
      webview_->AddScriptToExecuteOnDocumentCreated(
          *script,
          [shared_result](bool success, const std::string& script_id) {
            if (success) {
              shared_result->Success(flutter::EncodableValue(script_id));
            } else {
              shared_result->Error(kScriptFailed, "Adding script failed.");
            }
          });
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  if (method_name.compare(kMethodRemoveScriptToExecuteOnDocumentCreated) == 0) {
    if (const auto script_id = std::get_if<std::string>(method_call.arguments())) {
      webview_->RemoveScriptToExecuteOnDocumentCreated(*script_id);
      result->Success();
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // executeScript: string
  if (method_name.compare(kMethodExecuteScript) == 0) {
    if (const auto script = std::get_if<std::string>(method_call.arguments())) {
      std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>
          shared_result = std::move(result);
      webview_->ExecuteScript(
          *script, [shared_result](bool success, const std::string& script_result) {
            if (success) {
              shared_result->Success(flutter::EncodableValue(script_result));
            } else {
              shared_result->Error(kScriptFailed, "Executing script failed.");
            }
          });
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // postWebMessage: string
  if (method_name.compare(kMethodPostWebMessage) == 0) {
    if (const auto message = std::get_if<std::string>(method_call.arguments())) {
      if (webview_->PostWebMessage(*message)) {
        result->Success();
      } else {
        result->Error(kMethodFailed);
      }
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // setUserAgent: string
  if (method_name.compare(kMethodSetUserAgent) == 0) {
    if (const auto user_agent = std::get_if<std::string>(method_call.arguments())) {
      if (webview_->SetUserAgent(*user_agent)) {
        result->Success();
      } else {
        result->Error(kMethodFailed);
      }
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // clearCookies
  if (method_name.compare(kMethodClearCookies) == 0) {
    if (webview_->ClearCookies()) {
      result->Success();
    } else {
      result->Error(kMethodFailed);
    }
    return;
  }

  // clearCache
  if (method_name.compare(kMethodClearCache) == 0) {
    if (webview_->ClearCache()) {
      result->Success();
    } else {
      result->Error(kMethodFailed);
    }
    return;
  }

  // getCookies: string
  if (method_name.compare(kMethodGetCookies) == 0) {
    if (const auto url = std::get_if<std::string>(method_call.arguments())) {
      std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>
          shared_result = std::move(result);
      webview_->GetCookies(*url, [shared_result](bool success, const std::string& cookies) {
        if (success) {
          if (cookies.empty()) {
            shared_result->Success();
          } else {
            shared_result->Success(flutter::EncodableValue(cookies));
          }
        } else {
          shared_result->Error(kMethodFailed, "Getting cookies failed.");
        }
      });
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // setCacheDisabled: bool
  if (method_name.compare(kMethodSetCacheDisabled) == 0) {
    if (const auto disabled = std::get_if<bool>(method_call.arguments())) {
      if (webview_->SetCacheDisabled(*disabled)) {
        result->Success();
      } else {
        result->Error(kMethodFailed);
      }
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  // setPopupWindowPolicy: int
  if (method_name.compare(kMethodSetPopupWindowPolicy) == 0) {
    if (const auto policy = std::get_if<int>(method_call.arguments())) {
      webview_->SetPopupWindowPolicy(static_cast<WebviewPopupWindowPolicy>(*policy));
      result->Success();
    } else {
      result->Error(kErrorInvalidArgs);
    }
    return;
  }

  result->NotImplemented();
}