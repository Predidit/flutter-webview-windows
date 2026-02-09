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

  /// Returns `true` if environment was disposed (last instance).
  /// Caller should skip individual dispose when this returns `true`,
  /// as [WebviewController.disposeEnvironment] already handles cleanup.
  static Future<bool> unregister() async {
    if (_activeInstanceCount <= 0) return false;
    _activeInstanceCount--;
    debugPrint('WebViewInstanceTracker: unregister, active instances: $_activeInstanceCount');
    if (_activeInstanceCount == 0) {
      debugPrint('WebViewInstanceTracker: all instances disposed, resetting environment');
      await WebviewController.disposeEnvironment();
      return true;
    }
    return false;
  }
}
