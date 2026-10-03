#include "search/search.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#include "board/movegen.h"
#include "eval/eval.h"
#include "search/moveorder.h"
#include "search/tt.h"
#include "util/misc.h"
#include "util/thread.h"

typedef struct {
    MmiPosition pos;
    MmiLimits limits;
    MmiTimeManager tm;
    uint64_t nodes;
    int seldepth;
    bool silent;
    MmiMove iteration_best;
    MmiValue iteration_score;
    MmiMove ponder_move;
    MmiMove pv[MMI_MAX_PLY + 1][MMI_MAX_PLY + 1];
    /* Quiet moves that recently caused a beta cutoff at each ply, newest first. */
    MmiMove killers[MMI_MAX_PLY + 1][2];
    /* Butterfly history by side, from and to: how often a quiet move caused a cutoff, minus how often it failed to. */
    MmiButterflyHistory history[2];
    int pv_length[MMI_MAX_PLY + 1];
} MmiSearchWorker;

static MmiSearchWorker worker;
static MmiThread search_thread;
static bool thread_running;
static atomic_bool stop_requested;
/* While pondering the clock does not count, so no time limit applies. */
static atomic_bool pondering;
static int64_t move_overhead = 30;

void mmi_search_set_move_overhead(int ms) { move_overhead = ms; }

static bool stopped(void) { return atomic_load_explicit(&stop_requested, memory_order_relaxed); }
static bool is_pondering(void) { return atomic_load_explicit(&pondering, memory_order_relaxed); }

static void check_limits(MmiSearchWorker *w) {
    if (w->limits.nodes && w->nodes >= w->limits.nodes) atomic_store(&stop_requested, true);
    if (w->tm.maximum >= 0 && !is_pondering() && mmi_time_elapsed(&w->tm) >= w->tm.maximum)
        atomic_store(&stop_requested, true);
}

static void count_node(MmiSearchWorker *w, int ply) {
    if ((++w->nodes & 1023) == 0) check_limits(w);
    if (ply > w->seldepth) w->seldepth = ply;
}

static void update_pv(MmiSearchWorker *w, int ply, MmiMove m) {
    w->pv[ply][ply] = m;
    for (int i = ply + 1; i < w->pv_length[ply + 1]; i++) w->pv[ply][i] = w->pv[ply + 1][i];
    w->pv_length[ply] = w->pv_length[ply + 1];
}

#define HISTORY_MAX 16384

/* Neither a capture nor a promotion: the moves killers and history are about. */
static bool is_quiet(const MmiPosition *pos, MmiMove m) {
    return !mmi_is_capture(pos, m) && mmi_move_type(m) != MMI_MOVE_PROMOTION;
}

/* Gravity keeps entries within +-HISTORY_MAX: the closer an entry is to the bound, the less a bonus moves it. */
static void update_history(int *entry, int bonus) {
    *entry += bonus - *entry * (bonus < 0 ? -bonus : bonus) / HISTORY_MAX;
}

static MmiValue qsearch(MmiSearchWorker *w, MmiValue alpha, MmiValue beta, int ply) {
    MmiPosition *pos = &w->pos;
    w->pv_length[ply] = ply;
    count_node(w, ply);
    if (stopped()) return 0;

    bool in_check = mmi_in_check(pos);
    if (ply >= MMI_MAX_PLY - 1) return in_check ? MMI_VALUE_DRAW : mmi_evaluate(pos);

    MmiValue best = -MMI_VALUE_INFINITE;
    if (!in_check) {
        best = mmi_evaluate(pos);
        if (best >= beta) return best;
        if (best > alpha) alpha = best;
    }

    MmiMoveList list;
    int scores[MMI_MAX_MOVES];
    mmi_generate(pos, &list, in_check ? MMI_GEN_ALL : MMI_GEN_CAPTURES);
    if (in_check && list.count == 0) return -MMI_VALUE_MATE + ply;
    mmi_order_score(pos, &list, scores, MMI_MOVE_NONE, NULL, NULL);

    for (int i = 0; i < list.count; i++) {
        MmiMove m = mmi_order_pick(&list, scores, i);
        mmi_position_make(pos, m);
        MmiValue v = -qsearch(w, -beta, -alpha, ply + 1);
        mmi_position_unmake(pos, m);
        if (stopped()) return 0;
        if (v > best) {
            best = v;
            if (v > alpha) {
                alpha = v;
                if (v >= beta) break;
            }
        }
    }
    return best;
}

/* Keeps only the root moves "go searchmoves" allows. */
static void restrict_root(const MmiSearchWorker *w, MmiMoveList *list) {
    if (!w->limits.searchmoves_given) return;
    int kept = 0;
    for (int i = 0; i < list->count; i++)
        for (int j = 0; j < w->limits.searchmoves_count; j++)
            if (list->moves[i] == w->limits.searchmoves[j]) {
                list->moves[kept++] = list->moves[i];
                break;
            }
    list->count = kept;
}

static MmiValue search(MmiSearchWorker *w, MmiValue alpha, MmiValue beta, int depth, int ply) {
    MmiPosition *pos = &w->pos;
    bool root = ply == 0, pv_node = beta - alpha > 1;
    bool in_check = mmi_in_check(pos);
    w->pv_length[ply] = ply;

    if (in_check) depth++;
    if (depth <= 0) return qsearch(w, alpha, beta, ply);

    count_node(w, ply);
    if (stopped()) return 0;

    if (!root) {
        if (mmi_position_is_draw(pos)) return MMI_VALUE_DRAW;
        if (ply >= MMI_MAX_PLY - 1) return in_check ? MMI_VALUE_DRAW : mmi_evaluate(pos);
        /* Mate distance pruning: no line from here can beat a mate already found closer to the root. */
        if (alpha < -MMI_VALUE_MATE + ply) alpha = -MMI_VALUE_MATE + ply;
        if (beta > MMI_VALUE_MATE - ply - 1) beta = MMI_VALUE_MATE - ply - 1;
        if (alpha >= beta) return alpha;
    }

    MmiKey key = mmi_position_key(pos);
    MmiTTData tt;
    bool tt_hit = mmi_tt_probe(key, &tt);
    MmiMove tt_move = tt_hit ? tt.move : MMI_MOVE_NONE;
    if (tt_hit && !pv_node && tt.depth >= depth) {
        MmiValue v = mmi_value_from_tt(tt.value, ply);
        if (tt.bound == MMI_BOUND_EXACT || (tt.bound == MMI_BOUND_LOWER && v >= beta) ||
            (tt.bound == MMI_BOUND_UPPER && v <= alpha))
            return v;
    }

    MmiMoveList list;
    int scores[MMI_MAX_MOVES];
    mmi_generate(pos, &list, MMI_GEN_ALL);
    if (list.count == 0) return in_check ? -MMI_VALUE_MATE + ply : MMI_VALUE_DRAW;
    if (root) restrict_root(w, &list);
    mmi_order_score(pos, &list, scores, tt_move, w->killers[ply], &w->history[pos->side]);

    MmiValue best = -MMI_VALUE_INFINITE;
    MmiMove best_move = MMI_MOVE_NONE;
    MmiMove quiets_tried[64];
    int quiet_count = 0;
    for (int i = 0; i < list.count; i++) {
        MmiMove m = mmi_order_pick(&list, scores, i);
        bool quiet = is_quiet(pos, m);
        MmiValue v;
        mmi_position_make(pos, m);
        if (i == 0) {
            v = -search(w, -beta, -alpha, depth - 1, ply + 1);
        } else {
            v = -search(w, -alpha - 1, -alpha, depth - 1, ply + 1);
            if (v > alpha && v < beta) v = -search(w, -beta, -alpha, depth - 1, ply + 1);
        }
        mmi_position_unmake(pos, m);
        if (stopped()) return 0;

        if (v > best) {
            best = v;
            if (v > alpha) {
                alpha = v;
                best_move = m;
                update_pv(w, ply, m);
                if (root) {
                    w->iteration_best = m;
                    w->iteration_score = v;
                }
                if (v >= beta) {
                    if (quiet) {
                        if (w->killers[ply][0] != m) {
                            w->killers[ply][1] = w->killers[ply][0];
                            w->killers[ply][0] = m;
                        }
                        /* Reward the cutoff move and penalise the quiets searched before it, which failed. */
                        int bonus = depth * depth * 16 < 1600 ? depth * depth * 16 : 1600;
                        int (*h)[64] = w->history[pos->side].score;
                        update_history(&h[mmi_move_from(m)][mmi_move_to(m)], bonus);
                        for (int j = 0; j < quiet_count; j++)
                            update_history(&h[mmi_move_from(quiets_tried[j])][mmi_move_to(quiets_tried[j])], -bonus);
                    }
                    break;
                }
            }
        }
        if (quiet && quiet_count < 64) quiets_tried[quiet_count++] = m;
    }

    MmiBound bound = best >= beta ? MMI_BOUND_LOWER : best_move != MMI_MOVE_NONE ? MMI_BOUND_EXACT : MMI_BOUND_UPPER;
    mmi_tt_store(key, mmi_value_to_tt(best, ply), best_move, depth, bound);
    return best;
}

static void print_info(const MmiSearchWorker *w, int depth, MmiValue score) {
    int64_t ms = mmi_time_elapsed(&w->tm);
    char move[6];
    if (score >= MMI_VALUE_MATE_IN_MAX_PLY)
        printf("info depth %d seldepth %d score mate %d", depth, w->seldepth, (MMI_VALUE_MATE - score + 1) / 2);
    else if (score <= -MMI_VALUE_MATE_IN_MAX_PLY)
        printf("info depth %d seldepth %d score mate %d", depth, w->seldepth, -(MMI_VALUE_MATE + score) / 2);
    else
        printf("info depth %d seldepth %d score cp %d", depth, w->seldepth, score);
    printf(" nodes %" PRIu64 " nps %" PRIu64 " hashfull %d time %" PRId64 " pv", w->nodes,
           w->nodes * 1000 / (uint64_t)(ms > 0 ? ms : 1), mmi_tt_hashfull(), ms);
    for (int i = 0; i < w->pv_length[0]; i++) {
        mmi_move_to_uci(w->pv[0][i], move);
        printf(" %s", move);
    }
    printf("\n");
    fflush(stdout);
}

/* The second move of the PV, if the PV starts with best: the reply to ponder on. */
static MmiMove pv_reply(const MmiSearchWorker *w, MmiMove best) {
    return w->pv_length[0] > 1 && w->pv[0][0] == best ? w->pv[0][1] : MMI_MOVE_NONE;
}

static void iterate(MmiSearchWorker *w) {
    MmiMoveList root_moves;
    mmi_generate(&w->pos, &root_moves, MMI_GEN_ALL);
    restrict_root(w, &root_moves);
    MmiMove best = root_moves.count ? root_moves.moves[0] : MMI_MOVE_NONE;
    int max_depth = w->limits.depth > 0 && w->limits.depth < MMI_MAX_PLY - 1 ? w->limits.depth : MMI_MAX_PLY - 1;
    if (w->limits.mate > 0 && 2 * w->limits.mate < max_depth) max_depth = 2 * w->limits.mate;
    w->ponder_move = MMI_MOVE_NONE;

    MmiValue previous = 0;
    for (int depth = 1; depth <= max_depth && root_moves.count; depth++) {
        w->iteration_best = MMI_MOVE_NONE;
        w->seldepth = 0;

        /*
         * Aspiration: from depth 5 the score rarely moves far between iterations, so search a narrow window
         * around the last one and widen it on a fail. Mate scores move in steps too large for a window.
         */
        MmiValue alpha = -MMI_VALUE_INFINITE, beta = MMI_VALUE_INFINITE, delta = 25;
        if (depth >= 5 && previous > -MMI_VALUE_MATE_IN_MAX_PLY && previous < MMI_VALUE_MATE_IN_MAX_PLY) {
            alpha = previous - delta;
            beta = previous + delta;
        }
        MmiValue score;
        for (;;) {
            score = search(w, alpha, beta, depth, 0);
            if (stopped()) break;
            if (score <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = score - delta;
            } else if (score >= beta) {
                beta = score + delta;
            } else {
                break;
            }
            delta += delta / 2;
            if (delta > 400) {
                alpha = -MMI_VALUE_INFINITE;
                beta = MMI_VALUE_INFINITE;
            }
            if (alpha < -MMI_VALUE_INFINITE) alpha = -MMI_VALUE_INFINITE;
            if (beta > MMI_VALUE_INFINITE) beta = MMI_VALUE_INFINITE;
        }
        previous = score;
        /* A root move that raised alpha was searched in full, so it is safe to use even after a stop. */
        if (w->iteration_best != MMI_MOVE_NONE) {
            best = w->iteration_best;
            w->ponder_move = pv_reply(w, best);
        }
        if (stopped()) {
            /* Report the partial result so the last PV starts with the move we play. */
            if (!w->silent && w->iteration_best != MMI_MOVE_NONE) print_info(w, depth, w->iteration_score);
            break;
        }
        if (!w->silent) print_info(w, depth, score);
        if (root_moves.count == 1 && w->tm.maximum >= 0) break;
        /* Full-width search: a mate in n plies found at depth >= n cannot get shorter. */
        int mate_plies = MMI_VALUE_MATE - (score < 0 ? -score : score);
        if (mate_plies <= MMI_MAX_PLY && depth >= mate_plies) break;
        if (w->limits.mate > 0 && score >= MMI_VALUE_MATE_IN_MAX_PLY && (mate_plies + 1) / 2 <= w->limits.mate) break;
        /* The next iteration takes several times as long as this one, so past half the target it would not finish. */
        if (w->tm.optimum >= 0 && !is_pondering() && mmi_time_elapsed(&w->tm) >= w->tm.optimum / 2) break;
    }

    /* "go infinite" and "go ponder" must not answer before "stop" (or, for ponder, "ponderhit"). */
    while ((w->limits.infinite || is_pondering()) && !stopped()) mmi_sleep_ms(1);

    if (!w->silent) {
        char move[6], reply[6];
        mmi_move_to_uci(best, move);
        if (w->ponder_move != MMI_MOVE_NONE) {
            mmi_move_to_uci(w->ponder_move, reply);
            printf("bestmove %s ponder %s\n", move, reply);
        } else {
            printf("bestmove %s\n", move);
        }
        fflush(stdout);
    }
}

static void thread_main(void *arg) { iterate(arg); }

void mmi_search_start(const MmiPosition *pos, const MmiLimits *limits) {
    mmi_search_wait();
    memcpy(&worker.pos, pos, sizeof(*pos));
    worker.limits = *limits;
    worker.nodes = 0;
    memset(worker.killers, 0, sizeof(worker.killers));
    memset(worker.history, 0, sizeof(worker.history));
    worker.silent = false;
    mmi_time_init(&worker.tm, limits, pos->side, move_overhead);
    mmi_tt_new_search();
    atomic_store(&pondering, limits->ponder);
    atomic_store(&stop_requested, false);
    thread_running = mmi_thread_start(&search_thread, thread_main, &worker);
}

void mmi_search_ponderhit(void) { atomic_store(&pondering, false); }

void mmi_search_stop(void) { atomic_store(&stop_requested, true); }

void mmi_search_wait(void) {
    if (!thread_running) return;
    mmi_thread_join(&search_thread);
    thread_running = false;
}

uint64_t mmi_search_fixed_depth(const MmiPosition *pos, int depth) {
    mmi_search_wait();
    memcpy(&worker.pos, pos, sizeof(*pos));
    mmi_limits_clear(&worker.limits);
    worker.limits.depth = depth;
    worker.nodes = 0;
    memset(worker.killers, 0, sizeof(worker.killers));
    memset(worker.history, 0, sizeof(worker.history));
    worker.silent = true;
    mmi_time_init(&worker.tm, &worker.limits, pos->side, 0);
    mmi_tt_new_search();
    atomic_store(&pondering, false);
    atomic_store(&stop_requested, false);
    iterate(&worker);
    return worker.nodes;
}
