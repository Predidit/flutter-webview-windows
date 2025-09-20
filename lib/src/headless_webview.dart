import 'dart:async';
import 'dart:convert';

import 'package:flutter/services.dart';

import 'enums.dart';

typedef HeadlessPermissionRequestedDelegate
    = FutureOr<WebviewPermissionDecision> Function(
        String url, WebviewPermissionKind permissionKind, bool isUserInitiated);

typedef HeadlessScriptID = String;

class HeadlessWebviewValue {
  const HeadlessWebviewValue({
    required this.isInitialized,
    required this.isRunning,
  });

  final bool isInitialized;
  final bool isRunning;

  HeadlessWebviewValue copyWith({
    bool? isInitialized,
    bool? isRunning,
  }) {
    return HeadlessWebviewValue(
      isInitialized: isInitialized ?? this.isInitialized,
      isRunning: isRunning ?? this.isRunning,
    );
  }

  HeadlessWebviewValue.uninitialized()
      : this(
          isInitialized: false,
          isRunning: false,
        );
}

/// A headless WebView that operates without UI components.
/// This is useful for background web operations, scraping, or running JavaScript
/// without needing to display the web content.
class HeadlessWebview {
  static const String _pluginChannelPrefix = 'io.jns.webview.win.headless';
  static const MethodChannel _pluginChannel = MethodChannel(_pluginChannelPrefix);

  late Completer<void> _creatingCompleter;
  String _webviewId = '';
  bool _isDisposed = false;
  HeadlessWebviewValue _value = HeadlessWebviewValue.uninitialized();

  /// Current state of the headless webview
  HeadlessWebviewValue get value => _value;

  /// Future that completes when the webview is ready to use
  Future<void> get ready => _creatingCompleter.future;

  HeadlessPermissionRequestedDelegate? _permissionRequested;

  late MethodChannel _methodChannel;
  late EventChannel _eventChannel;
  StreamSubscription? _eventStreamSubscription;

  // Stream controllers for various events
  final StreamController<String> _urlStreamController =
      StreamController<String>.broadcast();
  final StreamController<LoadingState> _loadingStateStreamController =
      StreamController<LoadingState>.broadcast();
  final StreamController<WebErrorStatus> _onLoadErrorStreamController =
      StreamController<WebErrorStatus>.broadcast();
  final StreamController<String> _titleStreamController =
      StreamController<String>.broadcast();
  final StreamController<String> _securityStateChangedStreamController =
      StreamController<String>.broadcast();
  final StreamController<dynamic> _webMessageStreamController =
      StreamController<dynamic>.broadcast();
  final StreamController<Map<String, String>> _onM3USourceLoadedStreamController =
      StreamController<Map<String, String>>.broadcast();
  final StreamController<Map<String, String>> _onVideoSourceLoadedStreamController =
      StreamController<Map<String, String>>.broadcast();
  final StreamController<Map<String, String>> _onSourceLoadedStreamController =
      StreamController<Map<String, String>>.broadcast();

  /// A stream reflecting the current URL.
  Stream<String> get url => _urlStreamController.stream;

  /// A stream reflecting the current loading state.
  Stream<LoadingState> get loadingState => _loadingStateStreamController.stream;

  /// A stream reflecting the navigation error when navigation completed with an error.
  Stream<WebErrorStatus> get onLoadError => _onLoadErrorStreamController.stream;

  /// A stream reflecting the current document title.
  Stream<String> get title => _titleStreamController.stream;

  /// A stream reflecting the current security state.
  Stream<String> get securityStateChanged =>
      _securityStateChangedStreamController.stream;

  /// A stream for receiving messages from the web page.
  Stream<dynamic> get webMessage => _webMessageStreamController.stream;

  /// A stream reflecting when M3U source files are loaded with #ext content.
  Stream<Map<String, String>> get onM3USourceLoaded =>
      _onM3USourceLoadedStreamController.stream;

  /// A stream reflecting when video source files are loaded.
  Stream<Map<String, String>> get onVideoSourceLoaded =>
      _onVideoSourceLoadedStreamController.stream;

  /// A stream reflecting when any source files are loaded (for debugging).
  Stream<Map<String, String>> get onSourceLoaded =>
      _onSourceLoadedStreamController.stream;

  /// Creates a new headless webview instance.
  /// 
  /// [permissionRequested] - Optional callback for handling permission requests.
  HeadlessWebview({
    HeadlessPermissionRequestedDelegate? permissionRequested,
  }) : _permissionRequested = permissionRequested;

  /// Get the browser version info including channel name if it is not the
  /// WebView2 Runtime.
  /// Returns [null] if the webview2 runtime is not installed.
  static Future<String?> getWebViewVersion() async {
    return _pluginChannel.invokeMethod<String>('getWebViewVersion');
  }

  /// Initializes and runs the headless webview.
  /// This creates the underlying webview instance and starts it.
  Future<void> run() async {
    if (_isDisposed || _value.isRunning) {
      return;
    }

    _creatingCompleter = Completer<void>();
    
    try {
      final reply = await _pluginChannel.invokeMapMethod<String, dynamic>('createHeadless');
      
      _webviewId = reply!['webviewId'];
      _methodChannel = MethodChannel('$_pluginChannelPrefix/$_webviewId');
      _eventChannel = EventChannel('$_pluginChannelPrefix/$_webviewId/events');
      
      _eventStreamSubscription = _eventChannel.receiveBroadcastStream().listen((event) {
        final map = event as Map<dynamic, dynamic>;
        _handleEvent(map);
      });

      _methodChannel.setMethodCallHandler((call) {
        if (call.method == 'permissionRequested') {
          return _onPermissionRequested(call.arguments as Map<dynamic, dynamic>);
        }
        throw MissingPluginException('Unknown method ${call.method}');
      });

      _value = _value.copyWith(isInitialized: true, isRunning: true);
      _creatingCompleter.complete();
    } on PlatformException catch (e) {
      _creatingCompleter.completeError(e);
    }

    return _creatingCompleter.future;
  }

  /// Handles events from the native webview
  void _handleEvent(Map<dynamic, dynamic> map) {
    switch (map['type']) {
      case 'urlChanged':
        _urlStreamController.add(map['value']);
        break;
      case 'onLoadError':
        final value = WebErrorStatus.values[map['value']];
        _onLoadErrorStreamController.add(value);
        break;
      case 'loadingStateChanged':
        final value = LoadingState.values[map['value']];
        _loadingStateStreamController.add(value);
        break;
      case 'securityStateChanged':
        _securityStateChangedStreamController.add(map['value']);
        break;
      case 'titleChanged':
        _titleStreamController.add(map['value']);
        break;
      case 'webMessageReceived':
        try {
          final message = json.decode(map['value']);
          _webMessageStreamController.add(message);
        } catch (ex) {
          _webMessageStreamController.addError(ex);
        }
        break;
      case 'onM3USourceLoaded':
        final value = map['value'] as Map<dynamic, dynamic>;
        final m3uData = <String, String>{
          'url': value['url'] as String,
          'method': value['method'] as String,
          'responseBody': value['responseBody'] as String,
        };
        _onM3USourceLoadedStreamController.add(m3uData);
        break;
      case 'onVideoSourceLoaded':
        final value = map['value'] as Map<dynamic, dynamic>;
        final videoData = <String, String>{
          'url': value['url'] as String,
          'method': value['method'] as String,
          'contentType': value['contentType'] as String,
        };
        _onVideoSourceLoadedStreamController.add(videoData);
        break;
      case 'onSourceLoaded':
        final value = map['value'] as Map<dynamic, dynamic>;
        final sourceData = <String, String>{
          'url': value['url'] as String,
          'method': value['method'] as String,
          'contentType': value['contentType'] as String,
        };
        _onSourceLoadedStreamController.add(sourceData);
        break;
    }
  }

  /// Handles permission requests from the webview
  Future<bool?> _onPermissionRequested(Map<dynamic, dynamic> args) async {
    if (_permissionRequested == null) {
      return null;
    }

    final url = args['url'] as String?;
    final permissionKindIndex = args['permissionKind'] as int?;
    final isUserInitiated = args['isUserInitiated'] as bool?;

    if (url != null && permissionKindIndex != null && isUserInitiated != null) {
      final permissionKind = WebviewPermissionKind.values[permissionKindIndex];
      final decision = await _permissionRequested!(url, permissionKind, isUserInitiated);
      
      switch (decision) {
        case WebviewPermissionDecision.allow:
          return true;
        case WebviewPermissionDecision.deny:
          return false;
        case WebviewPermissionDecision.none:
          return null;
      }
    }

    return null;
  }

  /// Disposes the headless webview and releases all resources.
  Future<void> dispose() async {
    if (_isDisposed) {
      return;
    }

    if (_value.isRunning) {
      await _creatingCompleter.future;
    }

    _isDisposed = true;
    _value = _value.copyWith(isRunning: false);

    await _eventStreamSubscription?.cancel();

    // Close all stream controllers
    try {
      _urlStreamController.close();
      _loadingStateStreamController.close();
      _onLoadErrorStreamController.close();
      _titleStreamController.close();
      _securityStateChangedStreamController.close();
      _webMessageStreamController.close();
      _onM3USourceLoadedStreamController.close();
      _onVideoSourceLoadedStreamController.close();
      _onSourceLoadedStreamController.close();
    } catch (_) {}

    if (_webviewId.isNotEmpty) {
      await _pluginChannel.invokeMethod('disposeHeadless', _webviewId);
    }
  }

  /// Loads the given [url].
  Future<void> loadUrl(String url) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('loadUrl', url);
  }

  /// Loads a document from the given string content.
  Future<void> loadStringContent(String content) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('loadStringContent', content);
  }

  /// Reloads the current document.
  Future<void> reload() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('reload');
  }

  /// Stops all navigations and pending resource fetches.
  Future<void> stop() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('stop');
  }

  /// Navigates the WebView to the previous page in the navigation history.
  Future<void> goBack() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('goBack');
  }

  /// Navigates the WebView to the next page in the navigation history.
  Future<void> goForward() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('goForward');
  }

  /// Adds the provided JavaScript [script] to a list of scripts that should be run after the global
  /// object has been created, but before the HTML document has been parsed and before any
  /// other script included by the HTML document is run.
  ///
  /// Returns a [HeadlessScriptID] on success which can be used for [removeScriptToExecuteOnDocumentCreated].
  Future<HeadlessScriptID?> addScriptToExecuteOnDocumentCreated(String script) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    final result = await _methodChannel.invokeMethod<String>('addScriptToExecuteOnDocumentCreated', script);
    return result;
  }

  /// Removes the script identified by [scriptId] from the list of registered scripts.
  Future<void> removeScriptToExecuteOnDocumentCreated(HeadlessScriptID scriptId) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('removeScriptToExecuteOnDocumentCreated', scriptId);
  }

  /// Runs the JavaScript [script] in the current top-level document rendered in
  /// the WebView and returns its result.
  Future<dynamic> executeScript(String script) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    final result = await _methodChannel.invokeMethod('executeScript', script);
    try {
      return json.decode(result);
    } catch (ex) {
      return result;
    }
  }

  /// Posts the given JSON-formatted message to the current document.
  Future<void> postWebMessage(String message) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('postWebMessage', message);
  }

  /// Sets the user agent value.
  Future<void> setUserAgent(String userAgent) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('setUserAgent', userAgent);
  }

  /// Clears browser cookies.
  Future<void> clearCookies() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('clearCookies');
  }

  /// Get Browser Cookies for the specified URL.
  Future<String?> getCookies(String url) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    return await _methodChannel.invokeMethod<String>('getCookies', url);
  }

  /// Clears browser cache.
  Future<void> clearCache() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('clearCache');
  }

  /// Toggles ignoring cache for each request. If true, cache will not be used.
  Future<void> setCacheDisabled(bool disabled) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('setCacheDisabled', disabled);
  }

  /// Sets the [WebviewPopupWindowPolicy].
  Future<void> setPopupWindowPolicy(WebviewPopupWindowPolicy policy) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('setPopupWindowPolicy', policy.index);
  }

  /// Suspends the web view.
  Future<void> suspend() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('suspend');
  }

  /// Resumes the web view.
  Future<void> resume() async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('resume');
  }

  /// Adds a Virtual Host Name Mapping.
  Future<void> addVirtualHostNameMapping(
    String hostName, 
    String folderPath,
    WebviewHostResourceAccessKind accessKind
  ) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('setVirtualHostNameMapping', [
      hostName,
      folderPath,
      accessKind.index,
    ]);
  }

  /// Removes a Virtual Host Name Mapping.
  Future<void> removeVirtualHostNameMapping(String hostName) async {
    if (_isDisposed || !_value.isRunning) {
      throw StateError('HeadlessWebview is not running');
    }
    await _methodChannel.invokeMethod('clearVirtualHostNameMapping', hostName);
  }
}