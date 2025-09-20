#pragma once

#include <flutter/binary_messenger.h>
#include <flutter/event_channel.h>
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <string>

#include "webview.h"

class HeadlessWebviewBridge {
 public:
  HeadlessWebviewBridge(flutter::BinaryMessenger* messenger,
                        std::unique_ptr<Webview> webview);
  ~HeadlessWebviewBridge();

  const std::string& webview_id() const { return webview_id_; }

  void RegisterEventHandlers();

 private:
  std::unique_ptr<Webview> webview_;
  std::string webview_id_;
  flutter::BinaryMessenger* messenger_;

  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      method_channel_;
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      event_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> event_sink_;

  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  void OnPermissionRequested(
      const std::string& url, WebviewPermissionKind permissionKind,
      bool isUserInitiated,
      Webview::WebviewPermissionRequestedCompleter completer);
};