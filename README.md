# what's new about the fork

- `HeadlessWebview` mode to run webview without any window or surface. Useful for background tasks like web scraping.
- `GetCookie` API to solve the limit of Execscript (Can't access cookie marked as HTTPOnly).
- `onM3USourceLoaded` API to notify when m3u8 source is loaded. m3u8 body is also provided.
- `onVideoSourceLoaded` API to notify when video source is loaded.

> [!NOTE]
> The m3u8 / video parser feature is still experimental. And it requires specific webview2 flags which may be removed in future webview2 releases. Please test it carefully before using it in production.

# webview_windows

[![CI](https://github.com/jnschulze/flutter-webview-windows/actions/workflows/ci.yml/badge.svg)](https://github.com/jnschulze/flutter-webview-windows/actions/workflows/ci.yml)
[![Pub](https://img.shields.io/pub/v/webview_windows.svg)](https://pub.dartlang.org/packages/webview_windows)

A [Flutter](https://flutter.dev/) WebView plugin for Windows built on [Microsoft Edge WebView2](https://docs.microsoft.com/en-us/microsoft-edge/webview2/).


### Target platform requirements
- [WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)  
  Before initializing the webview, call `getWebViewVersion()` to check whether the required **WebView2 Runtime** is installed or not on the current system. If `getWebViewVersion()` returns null, guide your user to install **WebView2 Runtime** from this [page](https://developer.microsoft.com/en-us/microsoft-edge/webview2/).
- Windows 10 1809+

### Development platform requirements
- Visual Studio 2019 or higher
- Windows 11 SDK (10.0.22000.194 or higher)
- (recommended) nuget.exe in your $PATH *(The makefile attempts to download nuget if it's not installed, however, this fallback might not work in China)*

## Demo
![image](https://user-images.githubusercontent.com/720469/116823636-d8b9fe00-ab85-11eb-9f91-b7bc819615ed.png)

https://user-images.githubusercontent.com/720469/116716747-66f08180-a9d8-11eb-86ca-63ad5c24f07b.mp4



## Limitations
This plugin provides seamless composition of web-based contents with other Flutter widgets by rendering off-screen.

Unfortunately, [Microsoft Edge WebView2](https://docs.microsoft.com/en-us/microsoft-edge/webview2/) doesn't currently have an explicit API for offscreen rendering.
In order to still be able to obtain a pixel buffer upon rendering a new frame, this plugin currently relies on the `Windows.Graphics.Capture` API provided by Windows 10.
The downside is that older Windows versions aren't currently supported.

Older Windows versions might still be targeted by using `BitBlt` for the time being.

See:
- https://github.com/MicrosoftEdge/WebView2Feedback/issues/20
- https://github.com/MicrosoftEdge/WebView2Feedback/issues/526
- https://github.com/MicrosoftEdge/WebView2Feedback/issues/547
