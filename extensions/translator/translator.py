#!/usr/bin/env python3
"""tvshow window-provider example: DeepL-backed translator.

Speaks the adr-external-window-provider protocol (see calculator.py for the
protocol description). Stdlib only (urllib) -- no third-party dependencies.

Usage in ~/.config/tvshow/config.toml:
    window-provider-translate = "/path/to/tvshow/extensions/translator/translator.py"

Requires the DEEPL_API_KEY environment variable (get a free key at
https://www.deepl.com/pro-api). The key never lives in this repo or in
config.toml -- export it in your shell profile, e.g.:
    export DEEPL_API_KEY="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx:fx"
tvshow's window-provider spawn inherits the parent process's environment, so
whatever shell launched tvshow is what this script sees.

Input line formats (typed into the extension window):
    <text>                 -> translate to TVSHOW_TRANSLATE_TARGET (default "EN")
    <TARGET>: <text>       -> translate to TARGET, e.g. "CS: Hello there"
    <SOURCE>>TARGET: <text> -> explicit source, e.g. "EN>CS: Hello there"

Swapping providers: DeepL's free-tier key ends in ":fx" and uses the
api-free.deepl.com host; a Pro key uses api.deepl.com (handled below
automatically). To use a different service (Google Cloud Translate,
LibreTranslate, etc.) instead, replace translate() with a call to that
service's REST endpoint -- the stdin/stdout loop in main() doesn't change.
"""

import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

_LINE_RE = re.compile(r"^(?:([A-Za-z]{2})>)?([A-Za-z]{2}):\s*(.+)$")
_DEFAULT_TARGET = os.environ.get("TVSHOW_TRANSLATE_TARGET", "EN")


class TranslateError(Exception):
    pass


def deepl_endpoint(api_key: str) -> str:
    # DeepL free-tier keys always end in ":fx"; Pro keys don't.
    host = "api-free.deepl.com" if api_key.endswith(":fx") else "api.deepl.com"
    return f"https://{host}/v2/translate"


def translate(text: str, target: str, source: str | None) -> str:
    api_key = os.environ.get("DEEPL_API_KEY")
    if not api_key:
        raise TranslateError("DEEPL_API_KEY not set -- export it before launching tvshow")

    params = {"text": text, "target_lang": target}
    if source:
        params["source_lang"] = source
    data = urllib.parse.urlencode(params).encode("utf-8")

    req = urllib.request.Request(
        deepl_endpoint(api_key),
        data=data,
        headers={
            "Authorization": f"DeepL-Auth-Key {api_key}",
            "Content-Type": "application/x-www-form-urlencoded",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            body = json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        raise TranslateError(f"DeepL API error {exc.code}: {exc.reason}") from exc
    except urllib.error.URLError as exc:
        raise TranslateError(f"network error: {exc.reason}") from exc

    translations = body.get("translations") or []
    if not translations:
        raise TranslateError("empty response from DeepL")
    return translations[0]["text"]


def handle(line: str) -> str:
    line = line.strip()
    if not line:
        return ""
    if line == "help":
        return ("usage: <text> | TARGET: <text> | SOURCE>TARGET: <text> "
                f"(default target: {_DEFAULT_TARGET})")

    m = _LINE_RE.match(line)
    if m:
        source, target, text = m.groups()
        source = source.upper() if source else None
        target = target.upper()
    else:
        source, target, text = None, _DEFAULT_TARGET, line

    try:
        return translate(text, target, source)
    except TranslateError as exc:
        return f"error: {exc}"


def main() -> None:
    print(f"translator ready (DeepL, default target {_DEFAULT_TARGET}) "
          "-- type text, or 'TARGET: text', or 'help'", flush=True)
    for line in sys.stdin:
        result = handle(line)
        if result:
            print(result, flush=True)


if __name__ == "__main__":
    main()
