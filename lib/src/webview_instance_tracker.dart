import 'webview.dart';

/// Internal tracker for active WebView instance count.
///
/// Both [WebviewController] and [HeadlessWebview] register/unregister
/// through this class. When the last instance is disposed, the shared
/// WebView2 environment is automatically destroyed, allowing
/// re-initialization with new parameters (e.g. proxy settings).
class WebViewInstanceTracker {
  WebViewInstanceTracker._();

  static int _activeInstanceCount = 0;

  static void register() {
    _activeInstanceCount++;
  }

  static void unregister() {
    if (_activeInstanceCount <= 0) return;
    _activeInstanceCount--;
    if (_activeInstanceCount == 0) {
      WebviewController.disposeEnvironment();
    }
  }
}
