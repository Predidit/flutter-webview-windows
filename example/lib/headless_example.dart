import 'package:flutter/material.dart';
import 'package:webview_windows/webview_windows.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Headless WebView Example',
      home: HeadlessWebviewExample(),
    );
  }
}

class HeadlessWebviewExample extends StatefulWidget {
  @override
  _HeadlessWebviewExampleState createState() => _HeadlessWebviewExampleState();
}

class _HeadlessWebviewExampleState extends State<HeadlessWebviewExample> {
  HeadlessWebview? _headlessWebview;
  String _currentUrl = '';
  String _currentTitle = '';
  String _webview2Version = 'Unknown';
  LoadingState _loadingState = LoadingState.none;
  final TextEditingController _urlController = TextEditingController();
  final TextEditingController _scriptController = TextEditingController();
  String _scriptResult = '';
  final List<String> _log = [];

  @override
  void initState() {
    super.initState();
    _urlController.text = 'https://flutter.dev';
    _scriptController.text = 'document.title';
    _initializeHeadlessWebview();
  }

  Future<void> _initializeHeadlessWebview() async {
    try {
      // Optionally initialize the webview environment using
      // a custom user data directory
      // and/or a custom browser executable directory
      // and/or custom chromium command line flags
      // await WebviewController.initializeEnvironment(
      //    additionalArguments: '--show-fps-counter');

      _webview2Version = await HeadlessWebview.getWebViewVersion() ?? 'Unknown';
      
      _headlessWebview = HeadlessWebview(
        permissionRequested: (url, kind, isUserInitiated) async {
          _addToLog('Permission requested: $kind for $url');
          return WebviewPermissionDecision.allow;
        },
      );

      _headlessWebview!.url.listen((url) {
        setState(() {
          _currentUrl = url;
        });
        _addToLog('URL changed: $url');
      });

      _headlessWebview!.title.listen((title) {
        setState(() {
          _currentTitle = title;
        });
        _addToLog('Title changed: $title');
      });

      _headlessWebview!.onM3USourceLoaded.listen((url) {
        _addToLog('M3U source loaded: $url');
      });

      _headlessWebview!.onVideoSourceLoaded.listen((url) {
        _addToLog('MP4 source loaded: $url');
      });

      _headlessWebview!.loadingState.listen((state) {
        setState(() {
          _loadingState = state;
        });
        _addToLog('Loading state: $state');
      });

      _headlessWebview!.onLoadError.listen((error) {
        _addToLog('Load error: $error');
      });

      _headlessWebview!.webMessage.listen((message) {
        _addToLog('Web message: $message');
      });

      await _headlessWebview!.run();
      _addToLog('Headless WebView initialized successfully');

      await _headlessWebview!.loadUrl(_urlController.text);

    } catch (e) {
      _addToLog('Error initializing headless webview: $e');
    }
  }

  void _addToLog(String message) {
    setState(() {
      _log.add('${DateTime.now().toString().substring(11, 23)}: $message');
    });
    // Keep only the last 50 log entries
    if (_log.length > 50) {
      _log.removeRange(0, _log.length - 50);
    }
  }

  Future<void> _loadUrl() async {
    if (_headlessWebview != null) {
      try {
        await _headlessWebview!.loadUrl(_urlController.text);
      } catch (e) {
        _addToLog('Error loading URL: $e');
      }
    }
  }

  Future<void> _executeScript() async {
    if (_headlessWebview != null) {
      try {
        final result = await _headlessWebview!.executeScript(_scriptController.text);
        setState(() {
          _scriptResult = result.toString();
        });
        _addToLog('Script executed successfully');
      } catch (e) {
        _addToLog('Error executing script: $e');
        setState(() {
          _scriptResult = 'Error: $e';
        });
      }
    }
  }

  @override
  void dispose() {
    _headlessWebview?.dispose();
    _urlController.dispose();
    _scriptController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Headless WebView Example'),
      ),
      body: Padding(
        padding: EdgeInsets.all(16.0),
        child: Column(
          children: [
            // Status Section
            Card(
              child: Padding(
                padding: EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Status', style: Theme.of(context).textTheme.titleLarge),
                    Text('Current URL: $_currentUrl'),
                    Text('Title: $_currentTitle'),
                    Text('Loading State: $_loadingState'),
                    Text('WebView Version: $_webview2Version'),
                  ],
                ),
              ),
            ),
            SizedBox(height: 16),
            
            // URL Navigation Section
            Card(
              child: Padding(
                padding: EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Navigation', style: Theme.of(context).textTheme.titleLarge),
                    SizedBox(height: 8),
                    Row(
                      children: [
                        Expanded(
                          child: TextField(
                            controller: _urlController,
                            decoration: InputDecoration(
                              labelText: 'URL',
                              border: OutlineInputBorder(),
                            ),
                            onSubmitted: (_) => _loadUrl(),
                          ),
                        ),
                        SizedBox(width: 8),
                        ElevatedButton(
                          onPressed: _loadUrl,
                          child: Text('Load'),
                        ),
                      ],
                    ),
                    SizedBox(height: 8),
                    Row(
                      children: [
                        ElevatedButton(
                          onPressed: () => _headlessWebview?.goBack(),
                          child: Text('Back'),
                        ),
                        SizedBox(width: 8),
                        ElevatedButton(
                          onPressed: () => _headlessWebview?.goForward(),
                          child: Text('Forward'),
                        ),
                        SizedBox(width: 8),
                        ElevatedButton(
                          onPressed: () => _headlessWebview?.reload(),
                          child: Text('Reload'),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            SizedBox(height: 16),
            
            Card(
              child: Padding(
                padding: EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text('Script Execution', style: Theme.of(context).textTheme.titleLarge),
                    SizedBox(height: 8),
                    Row(
                      children: [
                        Expanded(
                          child: TextField(
                            controller: _scriptController,
                            decoration: InputDecoration(
                              labelText: 'JavaScript Code',
                              border: OutlineInputBorder(),
                            ),
                            onSubmitted: (_) => _executeScript(),
                          ),
                        ),
                        SizedBox(width: 8),
                        ElevatedButton(
                          onPressed: _executeScript,
                          child: Text('Execute'),
                        ),
                      ],
                    ),
                    SizedBox(height: 8),
                    Text('Result: $_scriptResult'),
                  ],
                ),
              ),
            ),
            SizedBox(height: 16),
            
            Expanded(
              child: Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('Log', style: Theme.of(context).textTheme.titleLarge),
                      SizedBox(height: 8),
                      Expanded(
                        child: Container(
                          width: double.infinity,
                          decoration: BoxDecoration(
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: ListView.builder(
                            itemCount: _log.length,
                            itemBuilder: (context, index) {
                              return Padding(
                                padding: EdgeInsets.symmetric(
                                    horizontal: 8.0, vertical: 2.0),
                                child: Text(
                                  _log[index],
                                  style: TextStyle(
                                    fontFamily: 'monospace',
                                    fontSize: 12,
                                  ),
                                ),
                              );
                            },
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}