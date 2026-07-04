
#include "MixBus.h"

void MixBus::SetSend(SendBus *bus, fixed level) {
    sendBus_ = bus;
    sendLevel_ = level;
}

bool MixBus::Render(fixed *buffer, int samplecount) {
    bool gotData = AudioMixer::Render(buffer, samplecount);
    if (gotData && sendBus_ && sendLevel_ != 0) {
        sendBus_->Accumulate(buffer, samplecount, sendLevel_);
    }
    return gotData;
}
