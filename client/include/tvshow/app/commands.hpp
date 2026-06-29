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
constexpr unsigned short cmStyleAuto   = 109;
constexpr unsigned short cmStyleTvision = 110;
constexpr unsigned short cmStyleLight  = 111;
constexpr unsigned short cmStyleDark   = 112;

}  // namespace tvshow::app
