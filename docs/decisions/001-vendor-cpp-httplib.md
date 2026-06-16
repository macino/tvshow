# 001 — Vendor cpp-httplib for the HTTP client and demo server

- **Status:** accepted
- **Date:** 2026-06-16
- **Touches SPEC:** §4, §16

## Context

M12 needs two things: a real `net::HttpClient` implementation (the interface
already exists, backed so far only by `FakeHttpClient` for tests) and
`tvshow-srv`, a demo HTTP server serving the SPEC §16 sample pages. Both need
an HTTP/1.1 implementation. Writing one from scratch (request line, header
parsing, chunked transfer, keep-alive) is exactly the kind of work CLAUDE.md
says not to reach for — it's not the contract this project tests, just
plumbing to reach it.

CLAUDE.md already lists `cpp-httplib` as a planned header-only dependency,
vendored under `third_party/`. `doctest` set the precedent for how
header-only deps land in this repo: not committed as a file, but
`FetchContent`-downloaded once into the build directory and exposed as an
INTERFACE target (`cmake/Doctest.cmake`), keeping the working tree free of
vendored source while still avoiding a system-package dependency.

## Decision

Pull `cpp-httplib`'s single header (`httplib.h`) the same way doctest is
pulled — `cmake/HttpLib.cmake` downloads a pinned release tag into
`${CMAKE_BINARY_DIR}/_httplib_include` and exposes it as `httplib::httplib`.
Both `net::CppHttpClient` (client side) and `tvshow-srv` (server side) link
against it.

## Consequences

- One more network fetch during first configure (same cost already paid for
  `tvision`, `gumbo-parser`, `katana-parser`, and `doctest`).
- `httplib.h` is large (single TU, but pulls in OpenSSL headers if
  `CPPHTTPLIB_OPENSSL_SUPPORT` is defined); we don't define it — v1 is HTTP
  only, no TLS, matching `util::Url` (which only parses `http://`/`https://`
  but `CppHttpClient` will reject `https://` until TLS is in scope).
- `tvshow-srv` and `net::CppHttpClient` both gain a compile dependency on
  this header; pure modules (`net/http_client.hpp`'s `HttpClient` interface,
  `dom`, `css`, `style`, `layout`, `render`) stay untouched — cpp-httplib
  only touches the impure `net/` and `server/` trees, consistent with the
  module boundary rules.

## Alternatives Considered

- Hand-rolled minimal HTTP/1.1 client/server: more code to maintain for a
  part of the system that isn't the subject of this project's tests; ruled
  out per CLAUDE.md ("don't pull anything new because 'it would be nice'"
  cuts both ways — also don't roll your own when a clearly-scoped header-only
  lib already plays this role for the codebase's other deps).
- Vendoring `httplib.h` as a committed file: rejected for consistency with
  the doctest precedent (download-once, not committed).

## References

- `cmake/Doctest.cmake` — the download-and-expose pattern this mirrors.
- CLAUDE.md § Dependency Rules.
- SPEC §4 (component overview), §16 (demo server pages).
- https://github.com/yhirose/cpp-httplib
