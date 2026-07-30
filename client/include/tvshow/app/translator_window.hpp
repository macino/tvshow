#pragma once

#include "tvshow/app/extension_process.hpp"
#include "tvshow/app/label_view.hpp"

#define Uses_TInputLine
#define Uses_TButton
#define Uses_TWindow
#include <tvision/tv.h>

#include <chrono>
#include <memory>
#include <string>

namespace tvshow::app {

// adr-translator-native-window: a bespoke form (source/target language,
// text to translate, a Translate button, a read-only result) backed by a
// fresh translator.py subprocess per click -- not a window-provider
// extension (translation needs real form widgets no window-provider child
// could describe at the time this was built; see the ADR for why this
// stayed a native feature instead of growing the UI protocol to cover
// combo-boxes/multi-field forms for a single example).
class TranslatorWindow : public TWindow {
public:
    // script_path: absolute path to translator.py (config key
    // "translator-script"). Empty -> Translate always shows a config error.
    TranslatorWindow(const TRect& bounds, std::string script_path);

    void handleEvent(TEvent& event) override;
    void changeBounds(const TRect& bounds) override;
    TPalette& getPalette() const override;
    void sizeLimits(TPoint& min, TPoint& max) override;

    // Called from Application::idle(): pulls the in-flight request's output,
    // if any. No-op when no request is pending.
    void poll();

private:
    std::string script_path_;
    std::unique_ptr<ExtensionProcess> process_;
    bool awaiting_response_ = false;
    std::chrono::steady_clock::time_point request_start_;

    TInputLine* source_lang_{nullptr};
    TInputLine* target_lang_{nullptr};
    TInputLine* text_input_{nullptr};
    LabelView* result_{nullptr};

    void start_translate();
    void reposition(const TRect& inner);
};

}  // namespace tvshow::app
