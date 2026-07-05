// Impulse-response tests for minimate's send-FX DSP (host build).
#include "Application/Mixer/SendBus.h"
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define FRAMES 128
static fixed inbuf[FRAMES * 2];
static fixed outbuf[FRAMES * 2];

static int failures = 0;

static void check(bool ok, const char *what) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static long long blockEnergy(const fixed *b, int samples) {
    long long e = 0;
    for (int i = 0; i < samples; i++) {
        long long v = b[i];
        e += (v < 0) ? -v : v;
    }
    return e;
}

int main() {
    // ---------------- Delay: impulse -> L echo at N, R echo at 2N
    {
        DelayBus d;
        int N = 1000;
        d.SetParams(N, fl2fp(0.5f), fl2fp(1.0f));
        memset(inbuf, 0, sizeof(inbuf));
        inbuf[0] = i2fp(8000);
        inbuf[1] = i2fp(8000);
        d.Accumulate(inbuf, FRAMES, fl2fp(1.0f));
        long firstL = -1, firstR = -1;
        fixed peak = 0;
        for (int blk = 0; blk < 60; blk++) {
            memset(outbuf, 0, sizeof(outbuf));
            d.Render(outbuf, FRAMES);
            for (int i = 0; i < FRAMES; i++) {
                long f = (long)blk * FRAMES + i;
                if (firstL < 0 && outbuf[2 * i] != 0)
                    firstL = f;
                if (firstR < 0 && outbuf[2 * i + 1] != 0)
                    firstR = f;
                fixed a = outbuf[2 * i];
                if (a < 0)
                    a = -a;
                if (a > peak)
                    peak = a;
            }
        }
        printf("delay: firstL=%ld firstR=%ld peakL=%d\n", firstL, firstR,
               fp2i(peak));
        check(firstL == N, "delay: left echo lands exactly at delay time");
        check(firstR == 2 * N, "delay: right echo lands at 2x (ping-pong)");
        check(peak > 0 && peak <= i2fp(8000),
              "delay: level sane (no overflow, no gain explosion)");
    }

    // ---------------- Chorus: impulse -> smeared taps near 20ms, L/R differ
    {
        ChorusBus c;
        c.SetParams(fl2fp(1.0f));
        memset(inbuf, 0, sizeof(inbuf));
        inbuf[0] = i2fp(8000);
        inbuf[1] = i2fp(8000);
        c.Accumulate(inbuf, FRAMES, fl2fp(1.0f));
        long firstL = -1, firstR = -1;
        for (int blk = 0; blk < 20; blk++) {
            memset(outbuf, 0, sizeof(outbuf));
            c.Render(outbuf, FRAMES);
            for (int i = 0; i < FRAMES; i++) {
                long f = (long)blk * FRAMES + i;
                if (firstL < 0 && outbuf[2 * i] != 0)
                    firstL = f;
                if (firstR < 0 && outbuf[2 * i + 1] != 0)
                    firstR = f;
            }
        }
        printf("chorus: firstL=%ld firstR=%ld\n", firstL, firstR);
        check(firstL >= 600 && firstL <= 1200,
              "chorus: left tap within base+/-depth window");
        check(firstR >= 600 && firstR <= 1200,
              "chorus: right tap within base+/-depth window");
        check(firstL != firstR, "chorus: L/R taps differ (stereo width)");
    }

    // ---------------- Reverb: impulse -> decaying tail, stable, no overflow
    {
        ReverbBus r;
        r.SetParams(fl2fp(1.0f), fl2fp(0.84f));
        memset(inbuf, 0, sizeof(inbuf));
        inbuf[0] = i2fp(8000);
        inbuf[1] = i2fp(8000);
        r.Accumulate(inbuf, FRAMES, fl2fp(1.0f));
        long long e100ms = 0, e500ms = 0, e1500ms = 0;
        fixed peak = 0;
        int blocks = (int)(2.0 * 44100 / FRAMES);
        for (int blk = 0; blk < blocks; blk++) {
            memset(outbuf, 0, sizeof(outbuf));
            r.Render(outbuf, FRAMES);
            long f = (long)blk * FRAMES;
            if (f <= (long)(0.10 * 44100) && (long)(0.10 * 44100) < f + FRAMES)
                e100ms = blockEnergy(outbuf, FRAMES * 2);
            if (f <= (long)(0.50 * 44100) && (long)(0.50 * 44100) < f + FRAMES)
                e500ms = blockEnergy(outbuf, FRAMES * 2);
            if (f <= (long)(1.50 * 44100) && (long)(1.50 * 44100) < f + FRAMES)
                e1500ms = blockEnergy(outbuf, FRAMES * 2);
            for (int i = 0; i < FRAMES * 2; i++) {
                fixed a = outbuf[i];
                if (a < 0)
                    a = -a;
                if (a > peak)
                    peak = a;
            }
        }
        printf("reverb: e@100ms=%" PRId64 " e@500ms=%" PRId64
               " e@1500ms=%" PRId64 " peak=%d\n",
               (int64_t)e100ms, (int64_t)e500ms, (int64_t)e1500ms, fp2i(peak));
        check(e100ms > 0, "reverb: tail present at 100ms");
        check(e500ms > 0 && e500ms < e100ms, "reverb: tail decaying by 500ms");
        check(e1500ms < e500ms, "reverb: tail still decaying at 1.5s");
        check(peak < i2fp(32767), "reverb: no overflow/instability");
    }

    printf(failures ? "== %d FAILURE(S)\n" : "== ALL PASS\n", failures);
    return failures ? 1 : 0;
}
