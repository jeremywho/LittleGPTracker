#include "SendBus.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"

SendBus::SendBus() {
    accum_ = (fixed *)SYS_MALLOC(SEND_MAX_FRAMES * 2 * sizeof(fixed));
    SYS_MEMSET(accum_, 0, SEND_MAX_FRAMES * 2 * sizeof(fixed));
    delayLine_ = (fixed *)SYS_MALLOC(DELAY_LINE_FRAMES * 2 * sizeof(fixed));
    SYS_MEMSET(delayLine_, 0, DELAY_LINE_FRAMES * 2 * sizeof(fixed));
    writePos_ = 0;
    delayFrames_ = 13230; // 300ms @ 44.1kHz
    feedback_ = fl2fp(0.55f);
    wet_ = fl2fp(0.6f);
    active_ = false;
    renderLogged_ = false;
}

SendBus::~SendBus() {
    SYS_FREE(accum_);
    SYS_FREE(delayLine_);
}

void SendBus::Accumulate(fixed *src, int samplecount, fixed level) {
    if (samplecount > SEND_MAX_FRAMES) {
        samplecount = SEND_MAX_FRAMES;
    }
    if (!active_) {
        Trace::Log("SENDBUS", "first accumulate: n=%d level=%d", samplecount,
                   level);
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
    if (!renderLogged_) {
        Trace::Log("SENDBUS", "first wet render: n=%d", samplecount);
        renderLogged_ = true;
    }
    if (samplecount > SEND_MAX_FRAMES) {
        samplecount = SEND_MAX_FRAMES;
    }
    int mask = DELAY_LINE_FRAMES - 1;
    fixed *in = accum_;
    fixed *out = buffer;
    for (int i = 0; i < samplecount; i++) {
        int readPos = (writePos_ - delayFrames_) & mask;
        fixed dl = delayLine_[readPos * 2];
        fixed dr = delayLine_[readPos * 2 + 1];
        // Asymmetric ping-pong: input feeds the left line only; the right
        // line is fed purely from the left tap. Repeats alternate ears
        // even when the source is centered (symmetric feedback collapses
        // to mono for mono input).
        fixed inL = *in++;
        fixed inR = *in++;
        fixed inMono = (inL >> 1) + (inR >> 1);
        delayLine_[writePos_ * 2] = inMono + fp_mul(dr, feedback_);
        delayLine_[writePos_ * 2 + 1] = fp_mul(dl, feedback_);
        *out++ = fp_mul(dl, wet_);
        *out++ = fp_mul(dr, wet_);
        writePos_ = (writePos_ + 1) & mask;
    }
    SYS_MEMSET(accum_, 0, samplecount * 2 * sizeof(fixed));
    return true;
}
