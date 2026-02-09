import 'package:flutter/foundation.dart';

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
    debugPrint('WebViewInstanceTracker: register, active instances: $_activeInstanceCount');
  }

  static void unregister() {
    if (_activeInstanceCount <= 0) return;
    _activeInstanceCount--;
    debugPrint('WebViewInstanceTracker: unregister, active instances: $_activeInstanceCount');
    if (_activeInstanceCount == 0) {
      debugPrint('WebViewInstanceTracker: all instances disposed, resetting environment');
      WebviewController.disposeEnvironment();
    }
  }
}
