#pragma once

namespace tvshow::app {

// Custom commands above the tvision built-in range (0–99).
constexpr unsigned short cmNewTab = 100;
constexpr unsigned short cmOpenUrl = 101;
constexpr unsigned short cmReload = 102;
constexpr unsigned short cmCloseTab = 103;
constexpr unsigned short cmBack = 104;
constexpr unsigned short cmForward = 105;
constexpr unsigned short cmWindowList = 106;
constexpr unsigned short cmPageLoaded = 107;
constexpr unsigned short cmLoadingTick = 108;
constexpr unsigned short cmStyleAuto    = 109;
constexpr unsigned short cmStyleTvision = 110;
constexpr unsigned short cmStyleLight   = 111;
constexpr unsigned short cmStyleDark    = 112;
constexpr unsigned short cmSettings     = 113;
// Internal: fires in the next event loop iteration to open deferred_open_url_.
constexpr unsigned short cmDeferredOpen = 114;
// 115 was cmOpenExtension (Ctrl-X window-provider picker) -- menu entry
// retired once Tools covered its only real use case (native calculator/
// calendar). ExtensionWindow itself is unchanged, just unreachable from the
// UI now; see adr-external-window-provider.
// adr-translator-native-window: opens the native Translator window.
constexpr unsigned short cmOpenTranslator = 116;
// adr-native-demo-windows: opens each native demo window.
constexpr unsigned short cmOpenCalculator = 117;
constexpr unsigned short cmOpenCalendarWin = 118;
constexpr unsigned short cmOpenPuzzle = 119;
constexpr unsigned short cmOpenCharChart = 120;
constexpr unsigned short cmOpenEventViewer = 121;
// adr-extension-server: opens the calculator HTTP-extension proof of
// concept as a normal browser tab (lazily starts the internal extension
// server on first use).
constexpr unsigned short cmOpenHttpCalculator = 122;

}  // namespace tvshow::app
