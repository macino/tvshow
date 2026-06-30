#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TButton
#define Uses_TInputLine
#define Uses_TLabel
#define Uses_TProgram
#define Uses_TRadioButtons
#define Uses_TSItem
#define Uses_TStaticText
#include "tvshow/app/settings_dialog.hpp"
#include "tvshow/util/config.hpp"

#include <tvision/tv.h>

#include <array>
#include <cstring>
#include <string>

namespace tvshow::app {

namespace {

// Map ForcedStyle → radio button index (0=Auto 1=tvision 2=Light 3=Dark).
ushort style_to_idx(ForcedStyle fs) {
    switch (fs) {
        case ForcedStyle::Tvision: return 1;
        case ForcedStyle::Light:   return 2;
        case ForcedStyle::Dark:    return 3;
        default:                   return 0;
    }
}

ForcedStyle idx_to_style(ushort idx) {
    switch (idx) {
        case 1:  return ForcedStyle::Tvision;
        case 2:  return ForcedStyle::Light;
        case 3:  return ForcedStyle::Dark;
        default: return ForcedStyle::Auto;
    }
}

const char* style_name(ushort idx) {
    static constexpr std::array<const char*, 4> names = {"auto", "tvision", "light", "dark"};
    return (idx < 4) ? names[idx] : "auto";
}

}  // namespace

void show_settings_dialog(SharedBrowsingState& shared) {
    const util::Config cfg = util::load_config();

    // kH=13 → interior rows 1-11, buttons at rows 10-11, frame at row 12.
    // TButton requires height>=2 (its draw loop runs for y=0..size.y-2).
    constexpr int kW     = 50;
    constexpr int kH     = 13;
    constexpr int kUrlMax = 256;

    const TRect desk = TProgram::deskTop->getExtent();
    const TRect bounds(
        std::max(0, (desk.b.x - kW) / 2),
        std::max(0, (desk.b.y - kH) / 2),
        std::min(desk.b.x, (desk.b.x + kW) / 2),
        std::min(desk.b.y, (desk.b.y + kH) / 2));

    auto* dlg = new TDialog(bounds, "Settings");

    // ── Default style ──────────────────────────────────────────────────────
    dlg->insert(new TLabel(TRect(2, 1, 18, 2), "Default style", nullptr));

    // height=1 → TCluster places all 4 items in one row of columns.
    auto* rb_style = new TRadioButtons(
        TRect(2, 2, kW - 2, 3),
        new TSItem("Auto",
        new TSItem("tvision",
        new TSItem("Light",
        new TSItem("Dark", nullptr)))));

    ushort style_sel = style_to_idx(shared.forced_style);
    rb_style->setData(&style_sel);
    dlg->insert(rb_style);

    // ── Address bar ────────────────────────────────────────────────────────
    dlg->insert(new TLabel(TRect(2, 4, 18, 5), "Address bar", nullptr));

    auto* rb_bar = new TRadioButtons(
        TRect(2, 5, kW - 2, 6),
        new TSItem("Modal",
        new TSItem("Persistent", nullptr)));

    ushort bar_sel = (cfg.address_bar == "persistent") ? 1 : 0;
    rb_bar->setData(&bar_sel);
    dlg->insert(rb_bar);

    // ── Start URL ──────────────────────────────────────────────────────────
    dlg->insert(new TLabel(TRect(2, 7, 12, 8), "Start URL", nullptr));

    auto* url_input = new TInputLine(TRect(2, 8, kW - 2, 9), kUrlMax);
    if (!cfg.start_url.empty()) {
        std::array<char, kUrlMax + 1> buf{};
        std::strncpy(buf.data(), cfg.start_url.c_str(), kUrlMax);
        url_input->setData(buf.data());
    }
    dlg->insert(url_input);

    // ── Buttons ────────────────────────────────────────────────────────────
    // height=2 required: TButton's draw loop runs for y=0..size.y-2, so
    // size.y=1 → loop body never executes → invisible button.
    dlg->insert(new TButton(TRect(8,  10, 20, 12), "~O~K",     cmOK,     bfDefault));
    dlg->insert(new TButton(TRect(28, 10, 42, 12), "~C~ancel", cmCancel, bfNormal));

    const unsigned short res = TProgram::deskTop->execView(dlg);

    if (res == cmOK) {
        ushort new_style_idx = 0;
        ushort new_bar_idx   = 0;
        rb_style->getData(&new_style_idx);
        rb_bar->getData(&new_bar_idx);

        std::array<char, kUrlMax + 1> url_buf{};
        url_input->getData(url_buf.data());

        util::Config new_cfg;
        new_cfg.log_level     = cfg.log_level;
        new_cfg.default_style = style_name(new_style_idx);
        new_cfg.address_bar   = (new_bar_idx == 1) ? "persistent" : "modal";
        new_cfg.start_url     = url_buf.data();

        util::save_config(new_cfg);

        // Apply style change immediately to all open windows.
        shared.forced_style = idx_to_style(new_style_idx);
    }

    TObject::destroy(dlg);
}

}  // namespace tvshow::app
