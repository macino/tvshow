# tvshow

A terminal "web browser" built with TurboVision. An HTTP server returns HTML+CSS; the client parses, lays out into character cells, and renders with box-drawing glyphs and truecolor attributes.

![tvshow rendering the demo index page](docs/screenshots/startup.png)

## Documentation

| Audience | Document |
|----------|----------|
| End users | [docs/user-guide.md](docs/user-guide.md) |
| Contributors | [docs/contributing.md](docs/contributing.md) |
| Server / app authors | [docs/server-guide.md](docs/server-guide.md) |
| C++ library embedders | [docs/embedding.md](docs/embedding.md) |
| Full specification | [SPEC.md](SPEC.md) |

## Quick start

**Build:**
```sh
cmake --preset debug
cmake --build --preset debug
```

**Run demo server + client:**
```sh
./build/debug/server/tvshow-srv --port 8080 &
./build/debug/client/tvshow http://localhost:8080/
```

**Run tests:**
```sh
./scripts/ci.sh          # full local CI (format, tidy, unit, asan, release)
ctest --preset debug     # unit tests only
```

## Stack

- C++20, [magiblot/tvision](https://github.com/magiblot/tvision)
- HTML parser: [google/gumbo-parser](https://github.com/google/gumbo-parser)
- CSS parser: [hackers/katana-parser](https://github.com/nicowillis/katana-parser)
- HTTP: [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only)
- Tests: [doctest](https://github.com/doctest/doctest) (header-only)
- Build: CMake ≥ 3.20, Ninja

## License

MIT — see [LICENSE](LICENSE).
