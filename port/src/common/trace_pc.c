/* Bring-up trace ($TTYD_TRACE): one line whenever the game sequence or map
 * changes, with the frame number, so a headless run shows how far it got. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dolphin/vi.h>
#include "driver/seqdrv.h"
#include "mario/mariost.h"
#include "manager/evtmgr.h"
#include "evt/evt_cmd.h"
#include "driver/swdrv.h"
#include "port/open_world.h"
#include "mario/mario_party.h"
#include "event.h"
#include "driver/fadedrv.h"
#include "port/platform.h"
#include "port/gameplus.h"
#include "port/toggles.h"
#include "port/practice.h"
#ifdef TTYD_BACKEND_AURORA
#include "port/display.h"
#endif

/* $TTYD_TRACE=2: every running script, where it is and what it calls. */
static void traceEvents(u32 frame) {
    EventWork* wp = evtGetWork();
    char a[160], b[160];
    s32 i;
    printf("[trace] frame %u: events\n", (unsigned)frame);
    for (i = 0; i < wp->count; i++) {
        EventEntry* e = &wp->entries[i];
        if (!(e->flags & 1)) {
            continue;
        }
        printf("  #%d id %d op %d at %s", (int)i, (int)e->eventId, (int)e->opcode, portSymbolName(e->nextCommand, a, sizeof(a)));
        if (e->opcode == OPCODE_USER_FUNC && e->args != NULL) {
            printf(" -> %s", portSymbolName((void*)e->args[0], b, sizeof(b)));
        }
        /* the flags (stopped 0x?), the clock WAIT_MS reads and its start, the speed: why a wait never ends */
        printf(" flags 0x%x lifetime %lld ms ud %lld/%lld/%lld blocked %d scale %.2f%s\n", (unsigned)e->flags,
               (long long)OSTicksToMilliseconds(e->lifetime), (long long)e->userdata[0], (long long)e->userdata[1],
               (long long)e->userdata[2], (int)e->blocked, (double)e->timescale,
               e->waitingOnEvent ? " (waiting on a child)" : "");
    }
    fflush(stdout);
}

/* $TTYD_TRACE=3: every command run by any script, with its script and the
 * user function it calls (first 4000 commands after a map's REL is linked). */
void portTraceCmd(EventEntry* e) {
    static int level = -1;
    static u32 budget = 4000;
    static const char* filter;
    char a[160], b[160];
    if (level < 0) {
        const char* v = getenv("TTYD_TRACE");
        level = v ? atoi(v) : 0;
        /* $TTYD_TRACE_CMD=<text>[,<text>...]: only scripts whose name contains one;
         * $TTYD_TRACE_BUDGET: how many commands (default 4000) */
        filter = getenv("TTYD_TRACE_CMD");
        if ((v = getenv("TTYD_TRACE_BUDGET")) != NULL) {
            budget = (u32)strtoul(v, NULL, 10);
        }
    }
    if (level < 3 || budget == 0 || gp == NULL || gp->currentMapName[0] == '\0') {
        return;
    }
    portSymbolName(e->currCommand, a, sizeof(a));
    if (filter != NULL) {
        const char* f = filter;
        for (;;) {
            size_t n = strcspn(f, ",");
            snprintf(b, sizeof(b), "%.*s", (int)n, f);
            if (strstr(a, b) != NULL) {
                break;
            }
            if (f[n] == '\0') {
                return;
            }
            f += n + 1;
        }
    }
    budget--;
    printf("[cmd] f %u ev %d op %3d %s", (unsigned)VIGetRetraceCount(), (int)e->eventId, (int)e->opcode, a);
    if (e->opcode == OPCODE_USER_FUNC && e->args != NULL) {
        printf(" -> %s", portSymbolName((void*)e->args[0], b, sizeof(b)));
    }
    printf("\n");
}

/* $TTYD_MAP=<map>[,<bero>]: at the first field map (the opening demo) or the
 * title, start a new game on that map (as seq_load's new game does, plus a
 * partner: $TTYD_PARTY), run $TTYD_EXIT_FRAMES frames there (default 600) and
 * exit 0; exit 3 if the map is not reached within $TTYD_WARP_LIMIT seconds of
 * wall time (default 60). tools/port/map_sweep.py runs every map this way. */
static void warpFrame(u32 frame) {
    static int state = -1; /* -1 unread, 0 off, 1 waiting for the title, 2 warped, 3 on the map */
    static char map[32], bero[32];
    static u32 exitFrames = 600, limit = 60, onMap;
    static time_t start;
    if (state < 0) {
        const char* v = getenv("TTYD_MAP");
        const char* comma;
        state = 0;
        if (v && *v) {
            comma = strchr(v, ',');
            snprintf(map, sizeof(map), "%.*s", comma ? (int)(comma - v) : (int)strlen(v), v);
            snprintf(bero, sizeof(bero), "%s", comma ? comma + 1 : "");
            if ((v = getenv("TTYD_EXIT_FRAMES")) && atoi(v) > 0) {
                exitFrames = (u32)atoi(v);
            }
            if ((v = getenv("TTYD_WARP_LIMIT")) && atoi(v) > 0) {
                limit = (u32)atoi(v);
            }
            state = 1;
            start = time(NULL);
        }
    }
    if (state == 0 || gp == NULL) {
        return;
    }
    /* the title, or the opening demo's map (its narration stalls in $TTYD_FAST runs) */
    if (state == 1 && (seqGetSeq() == SEQ_TITLE || (seqGetSeq() == SEQ_GAME && strcmp(gp->currentMapName, map) != 0))) {
        stg0_00_init();
        marioPartyKill();
        swInit();
        {
            /* the story so far (GSW 0, which map scripts branch on):
             * $TTYD_STORY, default 0 (a new game) */
            const char* v = getenv("TTYD_STORY");
            swByteSet(0, v ? atoi(v) : 0);
        }
        {
            /* a partner, as a save at that point of the game would have
             * ($TTYD_PARTY: party id, default 1 = Goombella; 0 = none;
             * more after commas join too, the first one out) */
            const char* v = getenv("TTYD_PARTY");
            s32 party = v ? atoi(v) : 1;
            if (party > 0 && party < 8) {
                const char* more = v != NULL ? strchr(v, ',') : NULL;
                partyJoin(party);
                for (; more != NULL; more = strchr(more + 1, ',')) {
                    const s32 other = atoi(more + 1);
                    if (other > 0 && other < 8) {
                        partyJoin(other);
                    }
                }
                marioPartyEntry(party);
            }
        }
        {
            /* $TTYD_OPEN_SEED: the file is an open-world randomizer file with this seed
             * (port/include/port/open_world.h), its story as $TTYD_STORY says; never
             * online or in a replay (portOpenTestSeed) */
            const char* v = portOpenTestSeed();
            if (v != NULL) {
                portOpenStamp(v, 0x0F, 0);
                if (getenv("TTYD_OPEN_SELFTEST") != NULL) {
                    portOpenSelfTest();
                }
            }
        }
        printf("[warp] frame %u: to %s%s%s\n", (unsigned)frame, map, bero[0] ? " " : "", bero);
        fflush(stdout);
        seqSetSeq(SEQ_MAP_CHANGE, map, bero[0] ? bero : NULL);
        state = 2;
    } else if (state == 2 && seqGetSeq() == SEQ_GAME && strcmp(gp->currentMapName, map) == 0) {
        state = 3;
        onMap = frame;
        portInputStart(); /* a $TTYD_INPUT script runs from here and decides when to exit */
    } else if (state == 3 && !portInputActive() && frame - onMap >= exitFrames) {
        /* $TTYD_SHOT_DIR: save the last frame as <map>.png first (Aurora) */
        static int shot = 0;
        if (shot == 0 && getenv("TTYD_SHOT_DIR") != NULL) {
            portShotRequest(map);
            shot = 1;
        }
        if (shot && portShotPending() && frame - onMap < exitFrames + 60) {
            return;
        }
        printf("[warp] ok: %s ran %u frames\n", map, (unsigned)exitFrames);
        fflush(stdout);
        _Exit(0); /* no teardown: Aurora's GPU threads are still running */
    }
    if (state < 3 && time(NULL) - start >= (time_t)limit) {
        printf("[warp] FAIL: %s not reached in %us, frame %u (sequence %d, map '%s')\n", map, (unsigned)limit,
               (unsigned)frame, (int)seqGetSeq(), gp->currentMapName);
        fflush(stdout);
        _Exit(3);
    }
}

void portTraceCard(const char* step, s32 result) {
    if (getenv("TTYD_TRACE") != NULL) {
        printf("[card] %s: %d\n", step, (int)result);
        fflush(stdout);
    }
}

void portTraceMsgMissing(const char* tag) {
    static char seen[64][48];
    static int count;
    int i;
    if (getenv("TTYD_TRACE") == NULL || tag == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (strncmp(seen[i], tag, sizeof(seen[i]) - 1) == 0) {
            return;
        }
    }
    if (count < 64) {
        snprintf(seen[count++], sizeof(seen[0]), "%s", tag);
    }
    printf("[msg] missing '%s' (map '%s')\n", tag, gp != NULL ? gp->currentMapName : "");
    fflush(stdout);
}

/* $TTYD_SWEEP_FILE=<path>: sequentially warp through <map>,<bero>,<story> entries
 * in one run, optionally capturing screenshots ($TTYD_SWEEP_SHOT_DIR/<map>.png) and
 * reporting steady-state frame timing per scene. */
#define SWEEP_MAX_ENTRIES 320
typedef struct SweepEntry {
    char map[32];
    char bero[32];
    s32 story;
} SweepEntry;

static int cmpFloatAsc(const void* a, const void* b) {
    float fa = *(const float*)a, fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static void sweepFrame(void) {
    static int state = -1; /* -1 unread, 0 off, 1 ready/starting next, 2 loading, 3 running on map, 4 shot wait */
    static SweepEntry entries[SWEEP_MAX_ENTRIES];
    static int total = 0, idx = 0;
    static u32 exitFrames = 90, waitFrames = 0, onMapFrames = 0;
    static const char* shotDir = NULL;
    static time_t loadStart = 0;

    if (state < 0) {
        const char* path = getenv("TTYD_SWEEP_FILE");
        const char* v;
        state = 0;
        if (path && *path) {
            FILE* f = fopen(path, "r");
            if (!f) {
                fprintf(stderr, "[sweep] FAIL: cannot open %s\n", path);
                _Exit(2);
            }
            char line[128];
            while (total < SWEEP_MAX_ENTRIES && fgets(line, sizeof(line), f)) {
                char* hash = strchr(line, '#');
                char* p1;
                char* p2;
                if (hash) *hash = '\0';
                line[strcspn(line, "\r\n")] = '\0';
                if (!line[0]) continue;
                p1 = strchr(line, ',');
                p2 = p1 ? strchr(p1 + 1, ',') : NULL;
                SweepEntry* e = &entries[total++];
                memset(e, 0, sizeof(*e));
                if (p1) {
                    snprintf(e->map, sizeof(e->map), "%.*s", (int)(p1 - line), line);
                    if (p2) {
                        snprintf(e->bero, sizeof(e->bero), "%.*s", (int)(p2 - (p1 + 1)), p1 + 1);
                        e->story = atoi(p2 + 1);
                    } else {
                        snprintf(e->bero, sizeof(e->bero), "%s", p1 + 1);
                    }
                } else {
                    snprintf(e->map, sizeof(e->map), "%s", line);
                }
            }
            fclose(f);
            if ((v = getenv("TTYD_EXIT_FRAMES")) && atoi(v) > 0) {
                exitFrames = (u32)atoi(v);
            }
            shotDir = getenv("TTYD_SWEEP_SHOT_DIR");
            state = total > 0 ? 1 : 0;
        }
    }
    if (state == 0 || gp == NULL) {
        return;
    }

    if (state == 1) {
        if (idx == 0 && seqGetSeq() != SEQ_TITLE && seqGetSeq() != SEQ_GAME) {
            return;
        }
        if (idx >= total) {
            printf("[sweep:all_done] total=%d\n", total);
            fflush(stdout);
            _Exit(0);
        }
        {
            const SweepEntry* e = &entries[idx];
            FILE* cf = fopen("/tmp/ttyd_sweep_cur.txt", "w");
            if (cf) {
                fprintf(cf, "%s\n", e->map);
                fclose(cf);
            }
            printf("[sweep:enter] idx=%d/%d map=%s bero=%s story=%d\n", idx + 1, total, e->map, e->bero,
                   (int)e->story);
            fflush(stdout);

            stg0_00_init();
            marioPartyKill();
            swInit();
            swByteSet(0, e->story);
            if (e->story > 0 && strncmp(e->map, "kpa_", 4) != 0 && strncmp(e->map, "sys_", 4) != 0 &&
                strncmp(e->map, "dmo_", 4) != 0 && strncmp(e->map, "dig_", 4) != 0 &&
                strncmp(e->map, "end_", 4) != 0) {
                partyJoin(1);
                marioPartyEntry(1);
            }
            seqSetSeq(SEQ_MAP_CHANGE, (char*)e->map, e->bero[0] ? (char*)e->bero : NULL);
            waitFrames = 0;
            loadStart = time(NULL);
            state = 2;
        }
        return;
    }

    if (state == 2) {
        const SequenceType s = seqGetSeq();
        waitFrames++;
        if (waitFrames >= 5 && s != SEQ_MAP_CHANGE && s != SEQ_LOGO && s != SEQ_TITLE) {
            onMapFrames = 0;
            state = 3;
            return;
        }
        if (time(NULL) - loadStart >= 25) {
            const SweepEntry* e = &entries[idx];
            printf("[sweep:timeout] map=%s seq=%d cur='%.*s'\n", e->map, (int)s,
                   (int)sizeof(gp->currentMapName), gp->currentMapName);
            fflush(stdout);
            _Exit(3);
        }
        return;
    }

    if (state == 3) {
        const SweepEntry* e = &entries[idx];
        onMapFrames++;
        if (onMapFrames >= exitFrames) {
            float fps = 0.0f, mean_iv = 0.0f, p50_iv = 0.0f, mean_work = 0.0f, max_work = 0.0f;
#ifdef TTYD_BACKEND_AURORA
            float iv[40], wk[40], sorted_iv[40];
            int want = onMapFrames < 40 ? (int)onMapFrames : 40;
            int n = portPerfSamples(iv, wk, want);
            if (n > 0) {
                int i;
                for (i = 0; i < n; i++) {
                    mean_iv += iv[i];
                    sorted_iv[i] = iv[i];
                    mean_work += wk[i];
                    if (wk[i] > max_work) max_work = wk[i];
                }
                mean_iv /= (float)n;
                mean_work /= (float)n;
                qsort(sorted_iv, (size_t)n, sizeof(float), cmpFloatAsc);
                p50_iv = sorted_iv[n / 2];
                if (mean_iv > 0.01f) fps = 1000.0f / mean_iv;
            }
#endif
            printf("[sweep:done] map=%s active_map=%.*s seq=%d fps=%.2f mean_iv=%.2f p50_iv=%.2f mean_work=%.2f max_work=%.2f\n",
                   e->map, (int)sizeof(gp->currentMapName), gp->currentMapName, (int)seqGetSeq(), fps, mean_iv, p50_iv,
                   mean_work, max_work);
            fflush(stdout);
            if (shotDir && *shotDir) {
                FILE* rf = fopen("/tmp/ttyd_screenshot.req", "w");
                if (rf) {
                    fprintf(rf, "%s/%s.ppm\n", shotDir, e->map);
                    fclose(rf);
                }
                state = 4;
                return;
            }
            idx++;
            state = 1;
        }
        return;
    }

    if (state == 4) {
        idx++;
        state = 1;
    }
}

/* $TTYD_TEST_ACT1_END=1: reproduce gon_11 Act 1 end -> FADE_OUT_WHITE -> FADE_STORY_MARIO_A1/A3 -> aji_10
 * (peach_evt_stg1) and capture rapid .ppm screenshots in $TTYD_ACT1_SHOT_DIR (default /tmp/act1_shots). */
static void testAct1EndFrame(void) {
    static int state = -1;
    static const char* shotDir = NULL;
    static u32 ticks = 0;
    static u32 shotIdx = 0;

    if (state < 0) {
        const char* v = getenv("TTYD_TEST_ACT1_END");
        state = (v && atoi(v) != 0) ? 1 : 0;
        shotDir = getenv("TTYD_ACT1_SHOT_DIR");
        if (!shotDir || !*shotDir) shotDir = "/tmp/act1_shots";
    }
    if (state == 0 || gp == NULL) return;

    if (state == 1) {
        if (seqGetSeq() != SEQ_TITLE && seqGetSeq() != SEQ_GAME) return;
        stg0_00_init();
        marioPartyKill();
        swInit();
        swByteSet(0, 56); /* STORY_STG1_17_FATHER_AND_SON_REUNITED */
        partyJoin(1);
        marioPartyEntry(1);
        seqSetSeq(SEQ_MAP_CHANGE, "gon_11", NULL);
        printf("[act1_test] warping to gon_11 (story=56)\n");
        fflush(stdout);
        ticks = 0;
        state = 2;
        return;
    }

    if (state == 2) {
        if (seqGetSeq() != SEQ_GAME || strcmp(gp->currentMapName, "gon_11") != 0) return;
        ticks++;
        if (ticks == 20) {
            printf("[act1_test] closing curtain + FADE_OUT_WHITE in gon_11\n");
            fflush(stdout);
            fadeEntry(FADE_CURTAIN_CENTER_A2_3D, 0, (GXColor){0, 0, 0, 0});
            fadeEntry(FADE_OUT_WHITE, 500, (GXColor){0, 0, 0, 0});
        } else if (ticks == 60) {
            printf("[act1_test] starting mapchange gon_11 -> aji_10 with FADE_STORY_MARIO_A1/A3\n");
            fflush(stdout);
            gp->areaFadeInType = FADE_STORY_MARIO_A1;
            gp->areaFadeInDuration = 0;
            gp->areaFadeOutType = FADE_STORY_MARIO_A3;
            gp->areaFadeOutDuration = 0;
            seqSetSeq(SEQ_MAP_CHANGE, "aji_10", NULL);
            ticks = 0;
            state = 3;
        }
        return;
    }

    if (state == 3) {
        ticks++;
        if ((ticks % 20) == 1) {
            char req[256];
            FILE* rf = fopen("/tmp/ttyd_screenshot.req", "w");
            snprintf(req, sizeof(req), "%s/f_%03u.ppm", shotDir, (unsigned)shotIdx++);
            if (rf) {
                fprintf(rf, "%s\n", req);
                fclose(rf);
            }
            printf("[act1_test] shot=%u tick=%u seq=%d map=%.*s\n", (unsigned)(shotIdx - 1), (unsigned)ticks,
                   (int)seqGetSeq(), (int)sizeof(gp->currentMapName), gp->currentMapName);
            fflush(stdout);
        }
        if (ticks >= 1200) {
            printf("[act1_test] done shots=%u\n", (unsigned)shotIdx);
            fflush(stdout);
            _Exit(0);
        }
    }
}

void portTraceFrame(u32 frame) {
    static int enabled = -1;
    portWatchdogFrame();
    testAct1EndFrame();
    sweepFrame();
    warpFrame(frame);
    portGameplusFrame();
    portChaosFrame();    /* chaos mode's timer and effects (port/toggles.h) */
    portPracticeFrame(); /* the speedrun timer, achievements */
    static s32 lastSeq = -2;
    static char lastMap[sizeof(gp->currentMapName)];
    s32 seq;

    if (gp != NULL) {
        /* the crash report's "game:" line, kept current */
        static s32 sCtxSeq = -3;
        static char sCtxMap[sizeof(gp->currentMapName)];
        const s32 s = (s32)seqGetSeq();
        if (s != sCtxSeq || strncmp(sCtxMap, gp->currentMapName, sizeof(sCtxMap)) != 0) {
            char ctx[160];
            sCtxSeq = s;
            memcpy(sCtxMap, gp->currentMapName, sizeof(sCtxMap));
            snprintf(ctx, sizeof(ctx), "sequence %d, map '%.*s', frame %u", (int)s, (int)sizeof(sCtxMap),
                     sCtxMap, (unsigned)frame);
            portCrashSetContext(ctx);
        }
    }
    if (enabled < 0) {
        const char* v = getenv("TTYD_TRACE");
        enabled = v == NULL ? 0 : atoi(v) > 1 ? 2 : 1;
    }
    if (!enabled || gp == NULL) {
        return;
    }
    seq = (s32)seqGetSeq();
    static u32 every;
    if (every == 0) {
        const char* v = getenv("TTYD_TRACE_EVERY");
        every = v != NULL && atoi(v) > 0 ? (u32)atoi(v) : 3000;
    }
    if (enabled > 1 && frame % every == 0) {
        traceEvents(frame);
    }
    if (frame % 600 == 0) {
        printf("[trace] frame %u: dvdError %d renderTime %lld\n", (unsigned)frame, (int)gp->dvdError,
               (long long)gp->renderTime);
        fflush(stdout);
    }
    {
        static s32 lastStory = -1;
        if (swByteGet(0) != lastStory) { /* the story advanced (GSW 0) */
            lastStory = swByteGet(0);
            printf("[trace] frame %u: story %d (map '%.*s')\n", (unsigned)frame, (int)lastStory,
                   (int)sizeof(gp->currentMapName), gp->currentMapName);
            fflush(stdout);
        }
    }
    if (seq != lastSeq || strncmp(lastMap, gp->currentMapName, sizeof(lastMap)) != 0) {
        lastSeq = seq;
        memcpy(lastMap, gp->currentMapName, sizeof(lastMap));
        printf("[trace] frame %u: sequence %d, map '%.*s', area '%.*s', story %d\n", (unsigned)frame, (int)seq,
               (int)sizeof(gp->currentMapName), gp->currentMapName, (int)sizeof(gp->currentAreaName),
               gp->currentAreaName, (int)swByteGet(0));
        fflush(stdout);
    }
}
