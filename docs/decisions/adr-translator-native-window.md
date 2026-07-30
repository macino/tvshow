# adr-translator-native-window — translator leaves the window-provider mechanism

- **Status:** accepted
- **Date:** 2026-07-28
- **Touches SPEC:** §20 q-extensions-api

## Context

The translator was originally a third `window-provider-*` example (scrollback mode, later a
candidate for `adr-extension-ui-protocol`'s structured mode). Two problems surfaced:

1. It was actually broken: `translator.py` declared `source: str | None` without
   `from __future__ import annotations` — Python 3.10+ union syntax evaluated eagerly at import
   time. On an older `python3`, the script crashed on startup with zero output; the process was
   dead, so nothing typed into the window did anything ("I cannot even type anything into
   translate").
2. Even fixed, a translator's natural UI is a form — source language, target language, input
   text, a Translate button, a result field — not a REPL log and not a button grid.
   `adr-extension-ui-protocol`'s `BUTTON`/`TEXT` vocabulary has no notion of a labeled text-entry
   field or a language picker; teaching it those for one example would be exactly the kind of
   speculative protocol growth both that ADR and `adr-external-window-provider` before it declined
   to do ahead of a second real use case.

## Decision

Pull translator out of the generic window-provider mechanism entirely. It's now a first-class
native `TranslatorWindow : TWindow` (own menu entry, Tools → Translate..., no window-provider
config needed) with real `TInputLine` fields (source lang, target lang, text) and a `TButton`
("Translate") wired natively — no protocol required to build a form when the form lives in C++
directly.

The actual translation call still shells out: each click spawns a fresh
`translator.py` subprocess (reusing `ExtensionProcess`, the same pipe plumbing
`ExtensionWindow` uses), writes one line, and polls (via `Application::idle()`, same forEach
pattern as `ExtensionWindow`/`BrowserWindow`) until a response line arrives or a 15s timeout
fires — then tears the process down. "Fresh spawn per click" rather than a persistent chat
process: matches the form's request/response shape (one input, one output) instead of pretending
it's a conversation.

Config: a single `translator-script = "<path>"` key (not `window-provider-translate` — it isn't
one anymore).

The `str | None` bug is fixed regardless (`from __future__ import annotations` added) — it would
still be wrong even if translator had stayed a window-provider extension.

## Consequences

- New `TranslatorWindow` + `LabelView` (shared with `adr-extension-ui-protocol`'s `TEXT` widget)
  for the read-only result field — `TStaticText` can't have its text changed after construction.
- New `translator-script` config key, parsed/saved alongside the existing flat keys.
- `Application::idle()` gains a third `deskTop->forEach` callback (`poll_translator_window`),
  alongside `tick_loading_window`/`poll_extension_window`.
- Translator is no longer reachable via Ctrl-X — removed from `window-provider-*` config examples
  in `extensions/README.md` and the shipped `~/.config/tvshow/config.toml`.
- `translator.py` keeps its persistent stdin-reading loop (unchanged) — `ExtensionProcess`'s
  `SIGTERM`-on-teardown makes "one request per process" cheap to implement on the tvshow side
  without needing a `--once` flag on the script itself.

## Alternatives Considered

- **Teach `adr-extension-ui-protocol` a text-entry-field widget + combo-box, keep translator as a
  window-provider**: rejected — designing a labeled-input-field/combo-box wire format against
  exactly one motivating example is the speculative-protocol-growth trap both prior ADRs already
  declined; a bespoke native window is less code and ships today.
- **Keep the persistent chat-log window, just fix the crash**: rejected per direct user feedback
  — "another interface" (a scrollback log) is not what a translator's interaction should look
  like; a request/response form is.

## References

- SPEC §20 q-extensions-api
- `adr-external-window-provider`, `adr-extension-ui-protocol` (mechanisms this deliberately does
  *not* use, and why)
- `extensions/translator/translator.py`
