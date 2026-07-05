#include "SendBus.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"

//------------------------------------------------------------ SendBus base

SendBus::SendBus(const char *name) {
    name_ = name;
    accum_ = (fixed *)SYS_MALLOC(SEND_MAX_FRAMES * 2 * sizeof(fixed));
    SYS_MEMSET(accum_, 0, SEND_MAX_FRAMES * 2 * sizeof(fixed));
    active_ = false;
    logged_ = false;
}

SendBus::~SendBus() { SYS_FREE(accum_); }

void SendBus::Accumulate(fixed *src, int samplecount, fixed level) {
    if (samplecount > SEND_MAX_FRAMES) {
        samplecount = SEND_MAX_FRAMES;
    }
    if (!logged_) {
        Trace::Log("SENDBUS", "%s: first accumulate (n=%d)", name_,
                   samplecount);
        logged_ = true;
    }
    fixed *dst = accum_;
    int count = samplecount * 2;
    while (count--) {
        *dst += fp_mul(*src++, level);
        dst++;
    }
    active_ = true;
}

bool SendBus::Render(fixed *buffer, int samplecount) {
    if (!active_) {
        return false;
    }
    if (samplecount > SEND_MAX_FRAMES) {
        samplecount = SEND_MAX_FRAMES;
    }
    process(accum_, buffer, samplecount);
    SYS_MEMSET(accum_, 0, samplecount * 2 * sizeof(fixed));
    return true;
}

//------------------------------------------------------------ DelayBus

DelayBus::DelayBus() : SendBus("delay") {
    delayLine_ = (fixed *)SYS_MALLOC(DELAY_LINE_FRAMES * 2 * sizeof(fixed));
    SYS_MEMSET(delayLine_, 0, DELAY_LINE_FRAMES * 2 * sizeof(fixed));
    writePos_ = 0;
    delayFrames_ = 13230;
    feedback_ = fl2fp(0.55f);
    wet_ = fl2fp(0.5f);
}

DelayBus::~DelayBus() { SYS_FREE(delayLine_); }

void DelayBus::SetParams(int delayFrames, fixed feedback, fixed wet) {
    if (delayFrames < 1) {
        delayFrames = 1;
    }
    if (delayFrames > DELAY_LINE_FRAMES - 1) {
        delayFrames = DELAY_LINE_FRAMES - 1;
    }
    delayFrames_ = delayFrames;
    feedback_ = feedback;
    wet_ = wet;
}

void DelayBus::process(const fixed *in, fixed *out, int frames) {
    int mask = DELAY_LINE_FRAMES - 1;
    for (int i = 0; i < frames; i++) {
        int readPos = (writePos_ - delayFrames_) & mask;
        fixed dl = delayLine_[readPos * 2];
        fixed dr = delayLine_[readPos * 2 + 1];
        // Asymmetric ping-pong: input feeds the left line only; the right
        // line is fed purely from the left tap so repeats alternate ears
        // even when the source is centered.
        fixed inL = *in++;
        fixed inR = *in++;
        fixed inMono = (inL >> 1) + (inR >> 1);
        delayLine_[writePos_ * 2] = inMono + fp_mul(dr, feedback_);
        delayLine_[writePos_ * 2 + 1] = fp_mul(dl, feedback_);
        *out++ = fp_mul(dl, wet_);
        *out++ = fp_mul(dr, wet_);
        writePos_ = (writePos_ + 1) & mask;
    }
}

//------------------------------------------------------------ ChorusBus

#define CHORUS_BASE_FRAMES 882 // 20ms @ 44.1kHz
#define CHORUS_DEPTH_FRAMES 265 // 6ms @ 44.1kHz
#define CHORUS_LFO_PERIOD 110250 // 0.4Hz @ 44.1kHz

ChorusBus::ChorusBus() : SendBus("chorus") {
    line_ = (fixed *)SYS_MALLOC(CHORUS_LINE_FRAMES * sizeof(fixed));
    SYS_MEMSET(line_, 0, CHORUS_LINE_FRAMES * sizeof(fixed));
    writePos_ = 0;
    lfoPhase_ = 0;
    wet_ = fl2fp(0.5f);
}

ChorusBus::~ChorusBus() { SYS_FREE(line_); }

void ChorusBus::SetParams(fixed wet) { wet_ = wet; }

// triangle 0..256..0 over CHORUS_LFO_PERIOD frames
static inline int chorusTriQ8(int phase) {
    int half = CHORUS_LFO_PERIOD / 2;
    if (phase < half) {
        return (phase * 256) / half;
    }
    return 256 - ((phase - half) * 256) / half;
}

void ChorusBus::process(const fixed *in, fixed *out, int frames) {
    int mask = CHORUS_LINE_FRAMES - 1;
    int quarter = CHORUS_LFO_PERIOD / 4;
    for (int i = 0; i < frames; i++) {
        fixed inL = *in++;
        fixed inR = *in++;
        line_[writePos_] = (inL >> 1) + (inR >> 1);

        // two taps on the same line, LFOs in quadrature -> stereo width
        int phaseR = lfoPhase_ + quarter;
        if (phaseR >= CHORUS_LFO_PERIOD) {
            phaseR -= CHORUS_LFO_PERIOD;
        }
        int delayQ8L =
            (CHORUS_BASE_FRAMES << 8) + CHORUS_DEPTH_FRAMES * chorusTriQ8(lfoPhase_);
        int delayQ8R =
            (CHORUS_BASE_FRAMES << 8) + CHORUS_DEPTH_FRAMES * chorusTriQ8(phaseR);

        int idxL = (writePos_ - (delayQ8L >> 8)) & mask;
        int fracL = delayQ8L & 0xFF;
        fixed aL = line_[idxL];
        fixed bL = line_[(idxL - 1) & mask];
        fixed sL = aL + (fixed)(((long long)(bL - aL) * fracL) >> 8);

        int idxR = (writePos_ - (delayQ8R >> 8)) & mask;
        int fracR = delayQ8R & 0xFF;
        fixed aR = line_[idxR];
        fixed bR = line_[(idxR - 1) & mask];
        fixed sR = aR + (fixed)(((long long)(bR - aR) * fracR) >> 8);

        *out++ = fp_mul(sL, wet_);
        *out++ = fp_mul(sR, wet_);

        writePos_ = (writePos_ + 1) & mask;
        lfoPhase_++;
        if (lfoPhase_ >= CHORUS_LFO_PERIOD) {
            lfoPhase_ = 0;
        }
    }
}

//------------------------------------------------------------ ReverbBus

// Freeverb-style plate (public domain topology), fixed point.
static const int combSizes[REVERB_COMBS] = {1116, 1188, 1277, 1356,
                                            1422, 1491, 1557, 1617};
static const int allpassSizes[REVERB_ALLPASSES] = {556, 441, 341, 225};
#define REVERB_STEREO_SPREAD 23
#define REVERB_INPUT_GAIN fl2fp(0.03f)

ReverbBus::ReverbBus() : SendBus("reverb") {
    for (int c = 0; c < 2; c++) {
        int spread = c * REVERB_STEREO_SPREAD;
        for (int i = 0; i < REVERB_COMBS; i++) {
            Comb &cb = comb_[c][i];
            cb.size = combSizes[i] + spread;
            cb.buf = (fixed *)SYS_MALLOC(cb.size * sizeof(fixed));
            SYS_MEMSET(cb.buf, 0, cb.size * sizeof(fixed));
            cb.idx = 0;
            cb.filterstore = 0;
        }
        for (int i = 0; i < REVERB_ALLPASSES; i++) {
            Allpass &ap = allpass_[c][i];
            ap.size = allpassSizes[i] + spread;
            ap.buf = (fixed *)SYS_MALLOC(ap.size * sizeof(fixed));
            SYS_MEMSET(ap.buf, 0, ap.size * sizeof(fixed));
            ap.idx = 0;
        }
    }
    wet_ = fl2fp(0.75f);
    room_ = fl2fp(0.84f);
    damp_ = fl2fp(0.4f);
}

ReverbBus::~ReverbBus() {
    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < REVERB_COMBS; i++) {
            SYS_FREE(comb_[c][i].buf);
        }
        for (int i = 0; i < REVERB_ALLPASSES; i++) {
            SYS_FREE(allpass_[c][i].buf);
        }
    }
}

void ReverbBus::SetParams(fixed wet, fixed room) {
    wet_ = wet;
    room_ = room;
}

void ReverbBus::processChannel(fixed input, fixed *out, Comb *combs,
                               Allpass *allpasses) {
    fixed acc = 0;
    for (int i = 0; i < REVERB_COMBS; i++) {
        Comb &cb = combs[i];
        fixed y = cb.buf[cb.idx];
        cb.filterstore =
            fp_mul(y, FP_ONE - damp_) + fp_mul(cb.filterstore, damp_);
        cb.buf[cb.idx] = input + fp_mul(cb.filterstore, room_);
        if (++cb.idx >= cb.size) {
            cb.idx = 0;
        }
        acc += y;
    }
    for (int i = 0; i < REVERB_ALLPASSES; i++) {
        Allpass &ap = allpasses[i];
        fixed bufout = ap.buf[ap.idx];
        ap.buf[ap.idx] = acc + (bufout >> 1);
        acc = bufout - acc;
        if (++ap.idx >= ap.size) {
            ap.idx = 0;
        }
    }
    *out = fp_mul(acc, wet_);
}

void ReverbBus::process(const fixed *in, fixed *out, int frames) {
    for (int i = 0; i < frames; i++) {
        fixed inL = *in++;
        fixed inR = *in++;
        fixed input = fp_mul((inL >> 1) + (inR >> 1), REVERB_INPUT_GAIN);
        processChannel(input, out++, comb_[0], allpass_[0]);
        processChannel(input, out++, comb_[1], allpass_[1]);
    }
}
