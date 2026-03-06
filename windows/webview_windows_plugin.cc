#include "include/webview_windows/webview_windows_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <windows.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "webview_bridge.h"
#include "headless_webview_bridge.h"
#include "webview_host.h"
#include "webview_platform.h"
#include "util/string_converter.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

namespace {

constexpr auto kMethodInitialize = "initialize";
constexpr auto kMethodDispose = "dispose";
constexpr auto kMethodInitializeEnvironment = "initializeEnvironment";
constexpr auto kMethodDisposeEnvironment = "disposeEnvironment";
constexpr auto kMethodGetWebViewVersion = "getWebViewVersion";
constexpr auto kMethodCreateHeadless = "createHeadless";
constexpr auto kMethodDisposeHeadless = "disposeHeadless";

constexpr auto kErrorCodeInvalidId = "invalid_id";
constexpr auto kErrorCodeEnvironmentCreationFailed =
    "environment_creation_failed";
constexpr auto kErrorCodeEnvironmentAlreadyInitialized =
    "environment_already_initialized";
constexpr auto kErrorCodeWebviewCreationFailed = "webview_creation_failed";
constexpr auto kErrorUnsupportedPlatform = "unsupported_platform";

// Window procedure for headless debug windows
LRESULT CALLBACK HeadlessDebugWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_ACTIVATE:
      // Force activation when window is clicked
      if (LOWORD(wParam) != WA_INACTIVE) {
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        BringWindowToTop(hwnd);
      }
      return 0;
    case WM_MOUSEACTIVATE:
      // Activate on mouse click
      SetForegroundWindow(hwnd);
      SetFocus(hwnd);
      return MA_ACTIVATE;
    case WM_NCACTIVATE:
      // Allow non-client activation
      return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_CLOSE:
      // Don't close, just hide for now
      ShowWindow(hwnd, SW_HIDE);
      return 0;
    case WM_DESTROY:
      return 0;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

template <typename T>
std::optional<T> GetOptionalValue(const flutter::EncodableMap& map,
                                  const std::string& key) {
  const auto it = map.find(flutter::EncodableValue(key));
  if (it != map.end()) {
    const auto val = std::get_if<T>(&it->second);
    if (val) {
      return *val;
    }
  }
  return std::nullopt;
}

class WebviewWindowsPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  WebviewWindowsPlugin(flutter::TextureRegistrar* textures,
                       flutter::BinaryMessenger* messenger);

  virtual ~WebviewWindowsPlugin();

 private:
  std::unique_ptr<WebviewPlatform> platform_;
  std::unique_ptr<WebviewHost> webview_host_;
  std::unordered_map<int64_t, std::unique_ptr<WebviewBridge>> instances_;
  std::unordered_map<std::string, std::unique_ptr<HeadlessWebviewBridge>> headless_instances_;

  WNDCLASS window_class_ = {};
  WNDCLASSEX headless_debug_window_class_ = {};
  flutter::TextureRegistrar* textures_;
  flutter::BinaryMessenger* messenger_;

  bool InitPlatform();

  void CreateWebviewInstance(
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>);
  void CreateHeadlessWebviewInstance(
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>);
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

// static
void WebviewWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "io.jns.webview.win",
          &flutter::StandardMethodCodec::GetInstance());

  auto headless_channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "io.jns.webview.win.headless",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<WebviewWindowsPlugin>(
      registrar->texture_registrar(), registrar->messenger());

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  headless_channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

WebviewWindowsPlugin::WebviewWindowsPlugin(flutter::TextureRegistrar* textures,
                                           flutter::BinaryMessenger* messenger)
    : textures_(textures), messenger_(messenger) {
  HINSTANCE hInstance = GetModuleHandle(nullptr);
  
  // Register window class for message windows
  window_class_.lpszClassName = L"FlutterWebviewMessage";
  window_class_.lpfnWndProc = &DefWindowProc;
  window_class_.hInstance = hInstance;
  RegisterClass(&window_class_);
  
  // Register window class for headless debug windows
  ZeroMemory(&headless_debug_window_class_, sizeof(WNDCLASSEX));
  headless_debug_window_class_.cbSize = sizeof(WNDCLASSEX);
  headless_debug_window_class_.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  headless_debug_window_class_.lpfnWndProc = HeadlessDebugWindowProc;
  headless_debug_window_class_.hInstance = hInstance;
  headless_debug_window_class_.hCursor = LoadCursor(nullptr, IDC_ARROW);
  headless_debug_window_class_.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
  headless_debug_window_class_.lpszClassName = L"FlutterWebviewHeadlessDebug";
  if (!RegisterClassEx(&headless_debug_window_class_)) {
    // Log error but don't fail - will handle at window creation
  }
}

WebviewWindowsPlugin::~WebviewWindowsPlugin() {
  instances_.clear();
  headless_instances_.clear();
  UnregisterClass(window_class_.lpszClassName, nullptr);
  UnregisterClass(headless_debug_window_class_.lpszClassName, nullptr);
}

void WebviewWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare(kMethodInitializeEnvironment) == 0) {
    if (webview_host_) {
      return result->Error(kErrorCodeEnvironmentAlreadyInitialized,
                           "The webview environment is already initialized");
    }

    if (!InitPlatform()) {
      return result->Error(kErrorUnsupportedPlatform,
                           "The platform is not supported");
    }

    const auto& map = std::get<flutter::EncodableMap>(*method_call.arguments());

    std::optional<std::wstring> browser_exe_wpath = std::nullopt;
    std::optional<std::string> browser_exe_path =
        GetOptionalValue<std::string>(map, "browserExePath");
    if (browser_exe_path) {
      browser_exe_wpath = util::Utf16FromUtf8(*browser_exe_path);
    }

    std::optional<std::wstring> user_data_wpath = std::nullopt;
    std::optional<std::string> user_data_path =
        GetOptionalValue<std::string>(map, "userDataPath");
    if (user_data_path) {
      user_data_wpath = util::Utf16FromUtf8(*user_data_path);
    } else {
      user_data_wpath = platform_->GetDefaultDataDirectory();
    }

    std::optional<std::string> additional_args =
        GetOptionalValue<std::string>(map, "additionalArguments");

    webview_host_ = std::move(WebviewHost::Create(
        platform_.get(), user_data_wpath, browser_exe_wpath, additional_args));
    if (!webview_host_) {
      return result->Error(kErrorCodeEnvironmentCreationFailed);
    }

    return result->Success();
  }

  if (method_call.method_name().compare(kMethodGetWebViewVersion) == 0) {
    LPWSTR version_info = nullptr;
    auto hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version_info);
    if (SUCCEEDED(hr) && version_info != nullptr) {
      return result->Success(flutter::EncodableValue(util::Utf8FromUtf16(version_info)));
    } else {
      return result->Success();
    }
  }

  if (method_call.method_name().compare(kMethodInitialize) == 0) {
    return CreateWebviewInstance(std::move(result));
  }

  if (method_call.method_name().compare(kMethodCreateHeadless) == 0) {
    return CreateHeadlessWebviewInstance(std::move(result));
  }

  if (method_call.method_name().compare(kMethodDisposeHeadless) == 0) {
    if (const auto webview_id = std::get_if<std::string>(method_call.arguments())) {
      const auto it = headless_instances_.find(*webview_id);
      if (it != headless_instances_.end()) {
        headless_instances_.erase(it);
        return result->Success();
      }
    }
    return result->Error(kErrorCodeInvalidId);
  }

  if (method_call.method_name().compare(kMethodDispose) == 0) {
    if (const auto texture_id = std::get_if<int64_t>(method_call.arguments())) {
      const auto it = instances_.find(*texture_id);
      if (it != instances_.end()) {
        instances_.erase(it);
        return result->Success();
      }
    }
    return result->Error(kErrorCodeInvalidId);
  }

  if (method_call.method_name().compare(kMethodDisposeEnvironment) == 0) {
    instances_.clear();
    headless_instances_.clear();
    webview_host_.reset();
    return result->Success();
  }

  result->NotImplemented();
}

void WebviewWindowsPlugin::CreateWebviewInstance(
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!InitPlatform()) {
    return result->Error(kErrorUnsupportedPlatform,
                         "The platform is not supported");
  }

  if (!webview_host_) {
    webview_host_ = std::move(WebviewHost::Create(
        platform_.get(), platform_->GetDefaultDataDirectory()));
    if (!webview_host_) {
      return result->Error(kErrorCodeEnvironmentCreationFailed);
    }
  }

  auto hwnd = CreateWindowEx(0, window_class_.lpszClassName, L"", 0, CW_DEFAULT,
                             CW_DEFAULT, 0, 0, HWND_MESSAGE, nullptr,
                             window_class_.hInstance, nullptr);

  std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>
      shared_result = std::move(result);
  webview_host_->CreateWebview(
      hwnd, true, true, false,
      [shared_result, this](std::unique_ptr<Webview> webview,
                            std::unique_ptr<WebviewCreationError> error) {
        if (!webview) {
          if (error) {
            return shared_result->Error(
                kErrorCodeWebviewCreationFailed,
                std::format(
                    "Creating the webview failed: {} (HRESULT: {:#010x})",
                    error->message, error->hr));
          }
          return shared_result->Error(kErrorCodeWebviewCreationFailed,
                                      "Creating the webview failed.");
        }

        auto bridge = std::make_unique<WebviewBridge>(
            messenger_, textures_, platform_->graphics_context(),
            std::move(webview));
        auto texture_id = bridge->texture_id();
        instances_[texture_id] = std::move(bridge);

        auto response = flutter::EncodableValue(flutter::EncodableMap{
            {flutter::EncodableValue("textureId"),
             flutter::EncodableValue(texture_id)},
        });

        shared_result->Success(response);
      });
}

void WebviewWindowsPlugin::CreateHeadlessWebviewInstance(
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!InitPlatform()) {
    return result->Error(kErrorUnsupportedPlatform,
                         "The platform is not supported");
  }

  if (!webview_host_) {
    webview_host_ = std::move(WebviewHost::Create(
        platform_.get(), platform_->GetDefaultDataDirectory()));
    if (!webview_host_) {
      return result->Error(kErrorCodeEnvironmentCreationFailed);
    }
  }

  // Create a visible standalone window for the headless webview (for debugging)
  auto hwnd = CreateWindowEx(
      WS_EX_APPWINDOW,  // Independent taskbar button
      headless_debug_window_class_.lpszClassName,
      L"Headless WebView (Debug)",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE,  // Visible from start
      CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
      nullptr,  // No parent
      nullptr,  // No menu
      headless_debug_window_class_.hInstance,
      nullptr);
  
  if (!hwnd) {
    DWORD error = GetLastError();
    return result->Error(kErrorCodeWebviewCreationFailed,
                        std::format("Failed to create debug window: {}", error));
  }
  
  // Ensure window is shown and painted
  ShowWindow(hwnd, SW_SHOWNORMAL);
  UpdateWindow(hwnd);
  EnableWindow(hwnd, TRUE);  // Ensure window is enabled
  
  // Bring to foreground (will work if user interacts or within timeout)
  SwitchToThisWindow(hwnd, TRUE);
  SetForegroundWindow(hwnd);
  BringWindowToTop(hwnd);

  std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>
      shared_result = std::move(result);
  webview_host_->CreateWebview(
      hwnd, false, true, true,
      [shared_result, this, hwnd](std::unique_ptr<Webview> webview,
                            std::unique_ptr<WebviewCreationError> error) {
        if (!webview) {
          if (error) {
            return shared_result->Error(
                kErrorCodeWebviewCreationFailed,
                std::format(
                    "Creating the headless webview failed: {} (HRESULT: {:#010x})",
                    error->message, error->hr));
          }
          return shared_result->Error(kErrorCodeWebviewCreationFailed,
                                      "Creating the headless webview failed.");
        }

        // Try again to bring window to foreground after webview is created
        if (hwnd && IsWindow(hwnd)) {
          // Use SetWindowPos to force window to top
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
          SetForegroundWindow(hwnd);
          SetFocus(hwnd);
          SetActiveWindow(hwnd);
          BringWindowToTop(hwnd);
        }

        auto bridge = std::make_unique<HeadlessWebviewBridge>(
            messenger_, std::move(webview));
        bridge->RegisterEventHandlers();
        
        auto webview_id = bridge->webview_id();
        headless_instances_[webview_id] = std::move(bridge);

        auto response = flutter::EncodableValue(flutter::EncodableMap{
            {flutter::EncodableValue("webviewId"),
             flutter::EncodableValue(webview_id)},
        });

        shared_result->Success(response);
      });
}

bool WebviewWindowsPlugin::InitPlatform() {
  if (!platform_) {
    platform_ = std::make_unique<WebviewPlatform>();
  }
  return platform_->IsSupported();
}

}  // namespace

void WebviewWindowsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  WebviewWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
