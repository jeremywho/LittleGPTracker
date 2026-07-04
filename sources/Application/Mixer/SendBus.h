#ifndef _SEND_BUS_H_
#define _SEND_BUS_H_

#include "Services/Audio/AudioModule.h"

// Global send-effect bus. Mix buses tap a copy of their output into the
// accumulator via Accumulate(); Render() then overwrites the buffer with
// the wet signal only, and the parent mixer sums it with the dry path.

#define SEND_MAX_FRAMES 4096
#define DELAY_LINE_FRAMES 32768 // power of two; ~0.74s @ 44.1kHz

class SendBus : public AudioModule {
public:
    SendBus();
    virtual ~SendBus();
    virtual bool Render(fixed *buffer, int samplecount);
    void Accumulate(fixed *src, int samplecount, fixed level);

private:
    fixed *accum_;
    fixed *delayLine_;
    int writePos_;
    int delayFrames_;
    fixed feedback_;
    fixed wet_;
    bool active_;
    bool renderLogged_;
};
#endif
