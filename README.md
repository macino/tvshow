# tvshow

Terminal "web browser" rendered with TurboVision.

- **`SPEC.md`** — v1 contract.
- **`EXECUTION.md`** — methodology, automated test gates, milestone roadmap.
- **`CLAUDE.md`** — agent guide.

## Build

```
cmake --preset debug
cmake --build --preset debug
```

## Run

```
./build/debug/client/tvshow
```

(M0 ships an empty TApplication. Real navigation lands in M11+.)

## Test (full local CI)

```
./scripts/ci.sh
```

Runs configure / build / format / tidy / unit / sanitizer / release in fail-fast order.

## License

MIT — see `LICENSE`.
