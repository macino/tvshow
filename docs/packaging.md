# Packaging tvshow (Homebrew and similar)

tvshow's normal build fetches its compiled dependencies (tvision, gumbo-parser, katana-parser,
lua) via CMake `FetchContent` (git clone at configure time) and two header-only deps
(cpp-httplib, doctest) via `file(DOWNLOAD ...)`. That's fine for a local dev build, but package
managers that build in a **network-sandboxed** environment — Homebrew's `brew install` is the
motivating case — block network access during the build step entirely, except for tarballs the
packaging system downloads itself ahead of time (Homebrew's `resource` blocks).

Every dependency-fetching CMake module (`cmake/{Tvision,Gumbo,Katana,Lua,HttpLib,Doctest}.cmake`)
therefore takes an optional override variable that, when set, uses an **already-staged local
directory** instead of reaching the network. Unset (the default), behavior is unchanged — this
is purely additive.

| CMake variable | Points at | Used by |
|---|---|---|
| `TVSHOW_TVISION_SOURCE_DIR` | a checked-out `magiblot/tvision` tree | `cmake/Tvision.cmake` |
| `TVSHOW_GUMBO_SOURCE_DIR` | a checked-out `google/gumbo-parser` tree | `cmake/Gumbo.cmake` |
| `TVSHOW_KATANA_SOURCE_DIR` | a checked-out, **already-patched** `hackers-painters/katana-parser` tree | `cmake/Katana.cmake` |
| `TVSHOW_LUA_SOURCE_DIR` | a checked-out `lua/lua` tree | `cmake/Lua.cmake` |
| `TVSHOW_HTTPLIB_HEADER_DIR` | a directory containing `httplib.h` | `cmake/HttpLib.cmake` |
| `TVSHOW_DOCTEST_HEADER_DIR` | a directory containing `doctest/doctest.h` | `cmake/Doctest.cmake` |

**The katana-parser exception**: the normal fetch path applies six upstream bug fixes via
`PATCH_COMMAND` (`cmake/patches/katana-*.cmake` — real crashes: double-free, null-pointer,
assert-on-malformed-CSS; see the comment block in `cmake/Katana.cmake` for what each one fixes).
`FetchContent`'s `SOURCE_DIR` option skips the update/patch step entirely (it treats the directory
as already-final), so a packager using `TVSHOW_KATANA_SOURCE_DIR` is responsible for applying
those patches themselves before staging the resource — e.g. Homebrew's own `resource ... do ...
patch ... end` block, referencing the same `cmake/patches/katana-*.cmake` scripts (they're plain
CMake `-P` scripts, runnable standalone: `cmake -DKATANA_SRC=<dir> -P
cmake/patches/katana-fix-chs-unit.cmake`, one invocation per patch file, in the order listed in
`cmake/Katana.cmake`).

## Verifying the override works

```sh
# 1. Populate the _deps/ cache normally once (needs network).
cmake -B build -G Ninja

# 2. Reconfigure from scratch pointing at the already-staged sources —
#    no network calls should happen (no "Downloading"/"Cloning" messages,
#    and configure finishes in well under a second instead of ~15s).
rm -rf build2
cmake -B build2 -G Ninja \
  -DTVSHOW_GUMBO_SOURCE_DIR=build/_deps/gumbo_parser-src \
  -DTVSHOW_KATANA_SOURCE_DIR=build/_deps/katana_parser-src \
  -DTVSHOW_TVISION_SOURCE_DIR=build/_deps/tvision-src \
  -DTVSHOW_LUA_SOURCE_DIR=build/_deps/lua-src \
  -DTVSHOW_HTTPLIB_HEADER_DIR=build/_httplib_include \
  -DTVSHOW_DOCTEST_HEADER_DIR=build/_doctest_include
cmake --build build2 && ctest --test-dir build2
```

(This project's own CI/dev environment can't create a network namespace to *prove* zero network
access — `unshare --net` needs privileges a sandboxed container doesn't grant — so this has been
verified by absence of download output and configure time, not a hard network-blocked run. The
mechanism itself, `FetchContent_Declare(... SOURCE_DIR ...)`, is CMake's own documented way to
skip the download/update steps, not a bespoke guess.)

## Sketch: a Homebrew Formula using this

Not the actual formula (see the tap repo for that) — the shape of how `resource` blocks feed
these variables:

```ruby
class Tvshow < Formula
  desc "Terminal web browser built with TurboVision"
  homepage "https://github.com/macino/tvshow"
  url "https://github.com/macino/tvshow/archive/refs/tags/vX.Y.Z.tar.gz"
  sha256 "..."
  license "MIT"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "openssl@3"
  depends_on "zlib"

  resource "tvision" do
    url "https://github.com/magiblot/tvision/archive/<pinned-commit>.tar.gz"
    sha256 "..."
  end
  resource "gumbo-parser" do
    url "https://github.com/google/gumbo-parser/archive/refs/tags/v0.10.1.tar.gz"
    sha256 "..."
  end
  resource "katana-parser" do
    url "https://github.com/hackers-painters/katana-parser/archive/<pinned-commit>.tar.gz"
    sha256 "..."
    # patch each cmake/patches/katana-*.cmake fix in here, in order --
    # see "The katana-parser exception" above.
  end
  resource "lua" do
    url "https://github.com/lua/lua/archive/refs/tags/v5.4.7.tar.gz"
    sha256 "..."
  end
  resource "httplib" do
    url "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.3/httplib.h"
    sha256 "..."
  end
  resource "doctest" do
    url "https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h"
    sha256 "..."
  end

  def install
    resource("tvision").stage buildpath/"_stage/tvision"
    resource("gumbo-parser").stage buildpath/"_stage/gumbo"
    resource("katana-parser").stage buildpath/"_stage/katana"
    resource("lua").stage buildpath/"_stage/lua"
    (buildpath/"_stage/httplib").install resource("httplib")
    (buildpath/"_stage/doctest/doctest").install resource("doctest")

    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
           "-DTVSHOW_TVISION_SOURCE_DIR=#{buildpath}/_stage/tvision",
           "-DTVSHOW_GUMBO_SOURCE_DIR=#{buildpath}/_stage/gumbo",
           "-DTVSHOW_KATANA_SOURCE_DIR=#{buildpath}/_stage/katana",
           "-DTVSHOW_LUA_SOURCE_DIR=#{buildpath}/_stage/lua",
           "-DTVSHOW_HTTPLIB_HEADER_DIR=#{buildpath}/_stage/httplib",
           "-DTVSHOW_DOCTEST_HEADER_DIR=#{buildpath}/_stage/doctest",
           *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/client/tvshow"
  end
end
```

This is a sketch to build the real formula from, not a drop-in file — `sha256`s, the tvision pin,
and the katana patch application inside its `resource` block all need filling in when the tap
repo is actually built.
