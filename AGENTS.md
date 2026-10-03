# AGENTS.md

Shared instructions for every developer and every AI coding tool working on
this repository. Read this file in full before changing anything.

## Project

`mmi_engine` is a UCI chess engine in C17, built by three developers:

| Developer | Own engine | Handle in branches |
|---|---|---|
| inph | inphish | `inph` |
| Mai | MaiEngine | `mai` |
| alp | Morstilia | `alp` |

`mmi_engine` is the codename and stays in the code: the `mmi_` prefix, the
`mmi-<commit>` version suffix, `MMI_*` macros and environment variables, and
`mmi-<hash>.nnue` network names. The public engine name is not chosen yet and
lives only in `MMI_NAME` in `src/mmi.h`.

## Ownership

Each area has one owner. The owner (or their AI tool) makes changes there and
reviews changes from others. **Work only inside the area your task belongs
to.** If a task needs a change in someone else's area, do not make it: describe
the needed change in the pull request or an issue and leave it to the owner.

| Area | Owner | Paths |
|---|---|---|
| Board, move generation, Chess960, perft and correctness tools | Mai | `src/board/`, `tests/perft.epd` |
| NNUE accumulators (incremental updates) and SIMD kernels (AVX2, NEON, ...) | Mai | `src/nnue/accumulator.*`, `src/nnue/simd/` (to be created) |
| Syzygy tablebase probing | Mai | `src/syzygy/` (to be created) |
| Search, transposition table, time management, move ordering | inph | `src/search/` |
| NNUE architecture, network loading, reference exactness tests | inph | `src/nnue/` except the accumulator and `simd/` (to be created) |
| UCI, bench, CLI, threads and platform utilities, extra UCI options | inph | `src/uci/`, `src/util/`, `src/main.c` |
| Build, CI and releases | inph | `Makefile`, `.github/` |
| Evaluation (everything that turns raw network output into the final score) | alp | `src/eval/` |
| Opening book | alp | `src/book/` (to be created) |
| Parameter tuning, testing and match tooling, testing rules | alp | `tools/` (to be created), `tests/run_tests.sh`, `docs/TESTING.md` (to be created) |
| Shared files | all three | `src/mmi.h`, `AGENTS.md`, `CLAUDE.md`, `.clang-format`, `README.md`, `LICENSE` |

Changes to shared files need approval from all three developers.

## Interfaces

These headers are contracts between areas. Change them only with sign-off from
every owner whose code uses them, in a pull request that does nothing else.

- `src/mmi.h`: core types, the 16-bit move encoding (castling is king takes own rook), value constants.
- `src/board/position.h`: `MmiPosition`, `MmiState`, make/unmake, FEN.
- `src/board/movegen.h`: legal generation with `MMI_GEN_CAPTURES`, `MMI_GEN_QUIETS`, `MMI_GEN_ALL` (disjoint captures and quiets).
- `src/eval/eval.h`: `MmiValue mmi_evaluate(const MmiPosition *pos)`, centipawns, side to move's view.
- `src/search/search.h`: start, stop, wait, fixed-depth search.

**NNUE hook.** Every board change in make/unmake goes through `put_piece`,
`remove_piece` and `move_piece` in `src/board/position.c`. Accumulator updates
attach there and nowhere else. Mai owns both sides of this hook. The
accumulator reads weights in the layout defined by inph's network code; agree
on that layout in one small pull request before either side builds on it.

## Rules for AI tools

- Do not edit `AGENTS.md`, `CLAUDE.md` or anything in `.github/`. Only the developers change them, by an agreed pull request.
- Keep your own notes and memory out of the repository. Use the git-ignored files `AGENTS.local.md` or `CLAUDE.local.md`, or your tool's own directory (`.claude/`, `.codex/`, `.cursor/`, `.agents/`). Never commit them.
- One branch per change, named `<handle>/<topic>` (for example `mai/pext-sliders`). Never commit to `main` directly and never force-push a shared branch.
- Keep pull requests small: one idea each. Rebase on `main` before asking for review.
- If something outside your area looks wrong, report it; do not fix it in passing.
- Do not copy code from Stockfish or any other engine you did not write. Ideas are fine; credit the source in the commit message. Developers may port code they wrote themselves from their own engines.
- No new dependencies. C17 and the C standard library only, plus pthreads/Win32 through `src/util/thread.h`.
- No emojis in code, comments, commits or docs.

## Code style

- C17. It must build without warnings under `-Wall -Wextra -Wshadow -Wpedantic -Werror` with gcc, clang and MinGW.
- Public names: functions and variables `mmi_*`, types `Mmi*`, macros and include guards `MMI_*`. File-local helpers are `static` and need no prefix.
- Includes are relative to `src/`: `#include "board/position.h"`.
- Format with `make format` (clang-format 21, config in `.clang-format`). On macOS: `make format CLANG_FORMAT=$(xcrun --find clang-format)`.
- Comments explain why, not what. No commented-out code, no placeholder stubs.
- No heap allocation inside the search; allocate at startup or on option changes.

## Before opening a pull request

Run all of these and fix every failure:

```sh
make format-check      # formatting
make                   # optimised build, no warnings
make test              # perft suite, UCI smoke test, bench determinism
make debug && sh tests/run_tests.sh ./mmi_engine-debug 4   # ASan + UBSan
make tsan && sh tests/run_tests.sh ./mmi_engine-tsan 3     # ThreadSanitizer
```

CI runs the same checks on Linux, macOS and Windows.

## Bench and strength

- Every commit message ends with a line `Bench: <nodes>`, the number printed by `./mmi_engine bench`.
- A change meant to leave play unchanged (refactor, speed-up, tooling) must keep the bench number. A different number means behaviour changed.
- A change meant to gain strength needs a match result against current `main` in the pull request: the opponent build, time control, number of games, openings and score. alp's `docs/TESTING.md` sets the required format and minimum once it exists.
- Speed claims need `bench` nodes per second from both builds on the same machine.

## Open decisions

To be settled by the three developers; until then, do not act on them.

- Public engine name (`MMI_NAME`).
- Repository hosting (a shared GitHub organisation is suggested).
- License. Using a Stockfish network would require GPL-3.0 for the whole engine.
- Network plan: a borrowed network for now, or training our own.
- Hardware and rules for strength testing.
