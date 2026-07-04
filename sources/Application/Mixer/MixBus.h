#ifndef _MIX_BUS_H_
#define _MIX_BUS_H_

#include "Services/Audio/AudioMixer.h"
#include "SendBus.h"

class MixBus: public AudioMixer {
public:
	MixBus():AudioMixer("bus"),sendBus_(0),sendLevel_(0) {} ;
	virtual ~MixBus() {} ;
	void SetSend(SendBus *bus,fixed level) ;
	virtual bool Render(fixed *buffer,int samplecount) ;
private:
	SendBus *sendBus_ ;
	fixed sendLevel_ ;
} ;
#endif
