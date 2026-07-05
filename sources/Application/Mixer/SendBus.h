#ifndef _SEND_BUS_H_
#define _SEND_BUS_H_

#include "Services/Audio/AudioModule.h"

// Global send-effect buses. Player channels tap a copy of their output
// into a bus accumulator via Accumulate(); Render() then overwrites the
// buffer with the wet signal only, and the parent mixer sums it with the
// dry path.

#define SEND_MAX_FRAMES 4096

class SendBus : public AudioModule {
public:
    SendBus(const char *name);
    virtual ~SendBus();
    virtual bool Render(fixed *buffer, int samplecount);
    void Accumulate(fixed *src, int samplecount, fixed level);

protected:
    // reads frames*2 fixed from in, writes frames*2 fixed wet to out
    virtual void process(const fixed *in, fixed *out, int frames) = 0;

private:
    const char *name_;
    fixed *accum_;
    bool active_;
    bool logged_;
};

#define DELAY_LINE_FRAMES 131072 // power of two; ~2.97s @ 44.1kHz

class DelayBus : public SendBus {
public:
    DelayBus();
    virtual ~DelayBus();
    void SetParams(int delayFrames, fixed feedback, fixed wet);

protected:
    virtual void process(const fixed *in, fixed *out, int frames);

private:
    fixed *delayLine_;
    int writePos_;
    int delayFrames_;
    fixed feedback_;
    fixed wet_;
};

#define CHORUS_LINE_FRAMES 4096 // power of two; ~93ms @ 44.1kHz

class ChorusBus : public SendBus {
public:
    ChorusBus();
    virtual ~ChorusBus();
    void SetParams(fixed wet);

protected:
    virtual void process(const fixed *in, fixed *out, int frames);

private:
    fixed *line_; // mono modulation source line
    int writePos_;
    int lfoPhase_;
    fixed wet_;
};

#define REVERB_COMBS 8
#define REVERB_ALLPASSES 4

class ReverbBus : public SendBus {
public:
    ReverbBus();
    virtual ~ReverbBus();
    void SetParams(fixed wet, fixed room);

protected:
    virtual void process(const fixed *in, fixed *out, int frames);

private:
    struct Comb {
        fixed *buf;
        int size;
        int idx;
        fixed filterstore;
    };
    struct Allpass {
        fixed *buf;
        int size;
        int idx;
    };
    void processChannel(fixed input, fixed *out, Comb *combs,
                        Allpass *allpasses);
    Comb comb_[2][REVERB_COMBS];
    Allpass allpass_[2][REVERB_ALLPASSES];
    fixed wet_;
    fixed room_;
    fixed damp_;
};
#endif
