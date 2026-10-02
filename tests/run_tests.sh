#!/bin/sh
# mmi test script: perft suite, UCI smoke test and bench determinism.
# Usage: sh tests/run_tests.sh [engine] [max perft depth]
set -eu

ENGINE=${1:-./mmi_engine}
DEPTH=${2:-5}
DIR=$(dirname "$0")

echo "mmi test: perft suite to depth $DEPTH"
"$ENGINE" perftsuite "$DIR/perft.epd" "$DEPTH"

echo "mmi test: UCI smoke"
OUT=$( (printf 'uci\nisready\nposition startpos moves e2e4 e7e5 g1f3 b8c6\ngo depth 5\n'
        sleep 1
        printf 'position fen 6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1\ngo movetime 200\n'
        sleep 1
        printf 'position startpos\ngo infinite\n'
        sleep 1
        printf 'stop\nposition startpos\ngo wtime 1000 btime 1000 winc 10 binc 10\n'
        sleep 1
        printf 'quit\n') | "$ENGINE")
for expected in "uciok" "readyok" "bestmove d1d8"; do
    if ! printf '%s\n' "$OUT" | grep -q "$expected"; then
        echo "FAIL: missing '$expected'"
        printf '%s\n' "$OUT"
        exit 1
    fi
done
COUNT=$(printf '%s\n' "$OUT" | grep -c '^bestmove [a-h][1-8][a-h][1-8]')
if [ "$COUNT" -ne 4 ]; then
    echo "FAIL: expected 4 legal-looking bestmoves, got $COUNT"
    printf '%s\n' "$OUT"
    exit 1
fi

echo "mmi test: bench determinism"
A=$("$ENGINE" bench | sed -n 's/^mmi bench: \([0-9]*\) nodes.*/\1/p')
B=$("$ENGINE" bench | sed -n 's/^mmi bench: \([0-9]*\) nodes.*/\1/p')
if [ -z "$A" ] || [ "$A" != "$B" ]; then
    echo "FAIL: bench not deterministic ($A vs $B)"
    exit 1
fi
echo "mmi test: all passed, Bench: $A"
