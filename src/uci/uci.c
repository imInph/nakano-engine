#include "uci/uci.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board/movegen.h"
#include "search/search.h"
#include "search/tt.h"

#define MMI_HASH_DEFAULT 16
#define MMI_HASH_MAX 65536
#define MMI_OVERHEAD_DEFAULT 10
#define MMI_OVERHEAD_MAX 5000

static MmiPosition position;
/* The running search, if any, ends only on "stop". */
static bool search_unbounded;

static void reply(const char *text) {
    fputs(text, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

static void cmd_uci(void) {
    printf("id name %s %s (mmi-%s)\n", MMI_NAME, MMI_VERSION, MMI_GIT_HASH);
    printf("id author %s\n", MMI_AUTHORS);
    printf("option name Hash type spin default %d min 1 max %d\n", MMI_HASH_DEFAULT, MMI_HASH_MAX);
    printf("option name Clear Hash type button\n");
    printf("option name Move Overhead type spin default %d min 0 max %d\n", MMI_OVERHEAD_DEFAULT, MMI_OVERHEAD_MAX);
    reply("uciok");
}

/* args: "name <id> [value <x>]"; option names may contain spaces. */
static void cmd_setoption(char *args) {
    char *name = strstr(args, "name ");
    if (!name) return;
    name += 5;
    char *value = strstr(name, " value ");
    if (value) {
        *value = '\0';
        value += 7;
    }

    if (strcmp(name, "Hash") == 0 && value) {
        int mb = atoi(value);
        if (mb < 1) mb = 1;
        if (mb > MMI_HASH_MAX) mb = MMI_HASH_MAX;
        if (!mmi_tt_resize((size_t)mb)) reply("info string mmi: hash allocation failed, keeping the old table");
    } else if (strcmp(name, "Clear Hash") == 0) {
        mmi_tt_clear();
    } else if (strcmp(name, "Move Overhead") == 0 && value) {
        int ms = atoi(value);
        mmi_search_set_move_overhead(ms < 0 ? 0 : ms > MMI_OVERHEAD_MAX ? MMI_OVERHEAD_MAX : ms);
    } else {
        printf("info string mmi: unknown option '%s'\n", name);
        fflush(stdout);
    }
}

/* args: "startpos|fen <fen> [moves m1 m2 ...]". A bad FEN keeps the previous position. */
static void cmd_position(char *args) {
    static MmiPosition parsed;
    char *moves = strstr(args, "moves");
    if (moves) *moves = '\0';

    bool ok;
    if (strncmp(args, "startpos", 8) == 0)
        ok = mmi_position_set_fen(&parsed, MMI_START_FEN);
    else if (strncmp(args, "fen ", 4) == 0)
        ok = mmi_position_set_fen(&parsed, args + 4);
    else
        ok = false;
    if (!ok) {
        reply("info string mmi: invalid position, keeping the previous one");
        return;
    }
    position = parsed;

    if (!moves) return;
    for (char *tok = strtok(moves + 5, " \t"); tok; tok = strtok(NULL, " \t")) {
        if (position.state_index >= MMI_MAX_GAME_PLY - MMI_MAX_PLY - 2) {
            reply("info string mmi: game too long, ignoring further moves");
            return;
        }
        MmiMove m = mmi_move_from_uci(&position, tok);
        if (m == MMI_MOVE_NONE) {
            printf("info string mmi: illegal move '%s', ignoring the rest\n", tok);
            fflush(stdout);
            return;
        }
        mmi_position_make(&position, m);
    }
}

/* Parses a go parameter, clamped to [lo, hi]. Garbage reads as lo; strtoll saturates instead of overflowing. */
static long long parse_clamped(const char *s, long long lo, long long hi) {
    long long v = s ? strtoll(s, NULL, 10) : lo;
    return v < lo ? lo : v > hi ? hi : v;
}

static void cmd_go(char *args) {
    /* About 31 years: small enough that the time manager's arithmetic on clock values cannot overflow. */
    const long long max_ms = 1000000000000LL;
    MmiLimits limits;
    mmi_limits_clear(&limits);
    for (char *tok = strtok(args, " \t"); tok; tok = strtok(NULL, " \t")) {
        /* Keywords without a value, and unknown words, consume only themselves. */
        bool takes_value = strcmp(tok, "wtime") == 0 || strcmp(tok, "btime") == 0 || strcmp(tok, "winc") == 0 ||
                           strcmp(tok, "binc") == 0 || strcmp(tok, "movetime") == 0 || strcmp(tok, "movestogo") == 0 ||
                           strcmp(tok, "depth") == 0 || strcmp(tok, "nodes") == 0;
        if (strcmp(tok, "infinite") == 0) limits.infinite = true;
        if (!takes_value) continue;

        const char *value = strtok(NULL, " \t");
        /* A clock below zero (some GUIs report overtime this way) means no time left, not no clock. */
        if (strcmp(tok, "wtime") == 0)
            limits.time[MMI_WHITE] = parse_clamped(value, 0, max_ms);
        else if (strcmp(tok, "btime") == 0)
            limits.time[MMI_BLACK] = parse_clamped(value, 0, max_ms);
        else if (strcmp(tok, "winc") == 0)
            limits.inc[MMI_WHITE] = parse_clamped(value, 0, max_ms);
        else if (strcmp(tok, "binc") == 0)
            limits.inc[MMI_BLACK] = parse_clamped(value, 0, max_ms);
        else if (strcmp(tok, "movetime") == 0)
            limits.movetime = parse_clamped(value, 1, max_ms);
        else if (strcmp(tok, "movestogo") == 0)
            limits.movestogo = (int)parse_clamped(value, 1, 1000);
        else if (strcmp(tok, "depth") == 0)
            limits.depth = (int)parse_clamped(value, 1, MMI_MAX_PLY - 1);
        else
            limits.nodes = (uint64_t)parse_clamped(value, 1, LLONG_MAX);
        if (!value) break;
    }
    search_unbounded = limits.infinite || (limits.time[position.side] < 0 && limits.movetime == 0 &&
                                           limits.depth == 0 && limits.nodes == 0);
    mmi_search_start(&position, &limits);
}

/*
 * Commands that change engine state wait for the running search, as before. A
 * search that only "stop" can end is stopped first, or waiting would hang.
 */
static void finish_search(void) {
    if (search_unbounded) mmi_search_stop();
    mmi_search_wait();
}

static void stop_search(void) {
    mmi_search_stop();
    mmi_search_wait();
}

void mmi_uci_loop(void) {
    static char line[65536];
    mmi_position_set_fen(&position, MMI_START_FEN);

    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *cmd = line;
        while (*cmd == ' ' || *cmd == '\t') cmd++;
        char *args = strchr(cmd, ' ');
        if (args) {
            *args++ = '\0';
            while (*args == ' ' || *args == '\t') args++;
        } else {
            args = cmd + strlen(cmd);
        }

        if (strcmp(cmd, "uci") == 0) {
            cmd_uci();
        } else if (strcmp(cmd, "isready") == 0) {
            reply("readyok");
        } else if (strcmp(cmd, "ucinewgame") == 0) {
            finish_search();
            mmi_tt_clear();
        } else if (strcmp(cmd, "setoption") == 0) {
            finish_search();
            cmd_setoption(args);
        } else if (strcmp(cmd, "position") == 0) {
            finish_search();
            cmd_position(args);
        } else if (strcmp(cmd, "go") == 0) {
            finish_search();
            cmd_go(args);
        } else if (strcmp(cmd, "stop") == 0) {
            stop_search();
        } else if (strcmp(cmd, "quit") == 0) {
            break;
        } else if (strcmp(cmd, "d") == 0) {
            finish_search();
            mmi_position_print(&position, stdout);
            fflush(stdout);
        } else if (*cmd) {
            printf("info string mmi: unknown command '%s'\n", cmd);
            fflush(stdout);
        }
    }
    stop_search();
}
