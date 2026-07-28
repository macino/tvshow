#include "tvshow/util/media_kind.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace tvshow::util {

namespace {

constexpr std::array<std::string_view, 5> k_video_exts = {".mp4", ".webm", ".mkv", ".avi", ".mov"};
constexpr std::array<std::string_view, 4> k_audio_exts = {".mp3", ".wav", ".ogg", ".flac"};

}  // namespace

std::vector<std::string> build_handler_argv(std::string_view command_template,
                                             std::string_view url) {
    std::vector<std::string> argv;
    std::string_view rest = command_template;
    while (!rest.empty()) {
        const auto sp = rest.find_first_of(" \t");
        const auto tok = rest.substr(0, sp);
        rest = (sp == std::string_view::npos) ? std::string_view{} : rest.substr(sp + 1);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.remove_prefix(1);
        }
        if (tok.empty()) { continue; }
        argv.emplace_back(tok == "%s" ? std::string(url) : std::string(tok));
    }
    return argv;
}

MediaKind classify_media_url(std::string_view url) noexcept {
    // Strip query/fragment so "clip.mp4?t=10" still matches.
    const auto query = url.find_first_of("?#");
    if (query != std::string_view::npos) {
        url = url.substr(0, query);
    }

    std::string lower(url);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto ext : k_video_exts) {
        if (lower.ends_with(ext)) { return MediaKind::Video; }
    }
    for (const auto ext : k_audio_exts) {
        if (lower.ends_with(ext)) { return MediaKind::Audio; }
    }
    return MediaKind::None;
}

}  // namespace tvshow::util
