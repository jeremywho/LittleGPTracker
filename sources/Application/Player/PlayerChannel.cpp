
#include "PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Application/Instruments/SampleInstrument.h"

static void accumulateSend(I_Instrument *instr, FourCC param, SendBus *bus,
                           fixed *buffer, int samplecount) {
    Variable *v = instr->FindVariable(param);
    if (!v) {
        return;
    }
    int send = v->GetInt();
    if (send > 0) {
        bus->Accumulate(buffer, samplecount, fl2fp(send / 255.0f));
    }
}

PlayerChannel::PlayerChannel(int index) {             
    index_=index ;
    instr_=0 ;
    muted_=false ;
	mixBus_=0 ;
	busIndex_=-1 ;
}

PlayerChannel::~PlayerChannel() {
}

void PlayerChannel::StartInstrument(I_Instrument *instr,unsigned char note,bool trigger) {
   if (instr_) {
      StopInstrument() ;
   }
   if (instr->Start(index_,note,trigger)) { // note could be refused coz it's out of the keymap
	   instr_=instr ;
   } else {
	   instr_=0 ;
   };
} ;

void PlayerChannel::StopInstrument() {
     if (instr_) {
       instr_->Stop(index_) ;
     }
     instr_=0 ;
} ;

bool PlayerChannel::Render(fixed *buffer,int samplecount) {
   if (instr_) {
     bool tableSlice=SyncMaster::GetInstance()->TableSlice() ;
     bool status=instr_->Render(index_,buffer,samplecount,tableSlice) ;
     bool audible=((status)&&(!muted_)) ;
     if (audible) {
        MixerService *ms=MixerService::GetInstance() ;
        accumulateSend(instr_,SIP_DELAYSEND,ms->GetDelayBus(),buffer,samplecount) ;
        accumulateSend(instr_,SIP_CHORUSSEND,ms->GetChorusBus(),buffer,samplecount) ;
        accumulateSend(instr_,SIP_REVERBSEND,ms->GetReverbBus(),buffer,samplecount) ;
     }
     return audible ;
   } else {
     return false ;
   }
} ;

I_Instrument *PlayerChannel::GetInstrument() {
   return instr_ ;
} ;

void PlayerChannel::SetMute(bool muted) {
     muted_=muted ;
}

bool PlayerChannel::IsMuted() {
     return muted_ ;
}

void PlayerChannel::SetMixBus(int i) {

	if (i==busIndex_) return ;

	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	mixBus_=MixerService::GetInstance()->GetMixBus(i) ;
	if (mixBus_) {
		mixBus_->Insert(*this) ;
	}
} ;

void PlayerChannel::Reset() {
	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	muted_=false ;
	busIndex_=-1 ;
} ;