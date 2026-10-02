# mmi_engine

A UCI chess engine in C, built together by inph (inphish), Mai (MaiEngine) and
alp (Morstilia). `mmi_engine` is the codename; the public name is not chosen
yet.

This is the starting skeleton: correct and playable, but deliberately simple.
Each part is meant to be replaced by its owner (see `AGENTS.md`).

## What is in it

- Bitboards with magic sliders, Zobrist hashing, FEN, make/unmake
- Legal move generation with check and pin masks, verified by perft
- Iterative deepening PVS with quiescence, a transposition table and MVV-LVA ordering
- Material-only evaluation
- UCI with `Hash`, `Clear Hash` and `Move Overhead`

## Building

```sh
make                 # optimised build for this machine
make ARCH=x86-64-v3  # or a specific target
make debug           # ASan + UBSan build
make tsan            # ThreadSanitizer build
make test            # perft suite, UCI smoke test, bench determinism
```

Needs a C17 compiler (gcc, clang or MinGW) and `make`.

## Commands

```
mmi_engine                          UCI mode
mmi_engine bench [depth]            fixed-depth search; the node count is the bench signature
mmi_engine perft <depth> [fen]      leaf node count
mmi_engine divide <depth> [fen]     node count per root move
mmi_engine perftsuite [file] [max]  check an EPD perft file (default tests/perft.epd)
```

In UCI mode, `d` prints the current position.

## Contributing

Read `AGENTS.md` first. It says who owns which part of the engine and which
checks every change must pass.
