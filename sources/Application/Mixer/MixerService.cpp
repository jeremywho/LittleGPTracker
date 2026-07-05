#include "MixerService.h"
#include "Application/Audio/DummyAudioOut.h"
#include "Application/Model/Config.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioDriver.h"
#include "Services/Midi/MidiService.h"
#include "System/Console/Trace.h"

MixerService::MixerService() : out_(0), sync_(0), isRendering_(false) {
    mode_ = MSRM_PLAYBACK;
};

MixerService::~MixerService(){};

/*
 * initializes the mixer service, config changes depending if we're in sequencer or render mode
 */
bool MixerService::Init() {
    // create the output depending on rendering mode
    out_ = 0;
	switch (mode_) {
    case MSRM_STEREO:
    case MSRM_STEMS:
        out_ = new DummyAudioOut();
        break;
    default:
        Audio *audio = Audio::GetInstance();
        out_ = audio->GetFirst();
        break;
	}

	for (int i=0;i<MAX_BUS_COUNT;i++) {
		master_.Insert(bus_[i]);
	}

	// send FX render after all buses have deposited their taps
	master_.Insert(delaySend_);
	master_.Insert(chorusSend_);
	master_.Insert(reverbSend_);

	bool result = false;
	if (out_) {
		result = out_->Init();
		if (result) {
			out_->Insert(master_);
		}

        initRendering(mode_);
        out_->AddObserver(*MidiService::GetInstance());
	}

	sync_=SDL_CreateMutex();
	NAssert(sync_);

	if (result) {
		Trace::Log("MixerService", "output initialized");
	} else {
		Trace::Log("MixerService", "failed to initialize output");
	}
	return (result);
};

void MixerService::initRendering(MixerServiceRenderMode mode) {
    switch(mode) {
    case MSRM_PLAYBACK:
        break;
    case MSRM_STEREO:
        out_->SetFileRenderer("project:mixdown.wav");
        break;
    case MSRM_STEMS:
        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            char buffer[1024];
            sprintf(buffer, "project:channel%d.wav", i);
            bus_[i].SetFileRenderer(buffer);
        }
        break;
    }
}

void MixerService::Close() {
	if (out_) {
    out_->RemoveObserver(*MidiService::GetInstance());
		out_->Close() ;
		out_->Empty() ;
		master_.Empty() ;

		switch(mode_) {
        case MSRM_STEMS:
        case MSRM_STEREO:
            break;
        default:
            break;
        }
    }
   for (int i=0;i<MAX_BUS_COUNT;i++) {
	   bus_[i].Empty() ;
   }
	out_=0 ;
	SDL_DestroyMutex(sync_) ;
	sync_=0 ;
} ;

void MixerService::SetRenderMode(int mode) {
    mode_ = MixerServiceRenderMode(mode);
}

bool MixerService::IsRendering() { return isRendering_; }

bool MixerService::Start() {
    MidiService::GetInstance()->Start();
    if (out_) {
        out_->AddObserver(*this);
        out_->Start();
     }
	return true ;
} ;

void MixerService::Stop() {
	MidiService::GetInstance()->Stop() ;
     if (out_) {
      out_->Stop() ;
      out_->RemoveObserver(*this) ;
     }
}

MixBus *MixerService::GetMixBus(int i) {
	return &(bus_[i]) ;
} ;

void MixerService::Update(Observable &o,I_ObservableData *d)  {

  AudioDriver::Event *event=(AudioDriver::Event *)d;
  if (event->type_ == AudioDriver::Event::ADET_BUFFERNEEDED)
  {  
    Lock() ;
    SetChanged() ;
    NotifyObservers() ;

    out_->Trigger();
    Unlock();
  }
}

bool MixerService::Clipped() {
     return out_->Clipped() ;
} ;

void MixerService::SetPregain(int vol) {
    Mixer *mixer = Mixer::GetInstance();

    fixed masterVolume = fp_mul(i2fp(vol), fl2fp(0.01f));

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
        bus_[i].SetVolume(masterVolume);
  }
};

void MixerService::SetSoftclip(int clip, int gain) {
    out_->SetSoftclip(clip, gain);
}

void MixerService::SetMasterVolume(int attn) { out_->SetMasterVolume(attn); }

void MixerService::SetFxParams(int dlySixteenths, int dlyFbPct, int dlyWetPct,
                               int choWetPct, int rvWetPct, int rvSizePct) {
    float framesPerSixteenth =
        SyncMaster::GetInstance()->GetPlaySampleCount() * 6.0f;
    int frames = (int)(dlySixteenths * framesPerSixteenth);
    delaySend_.SetParams(frames, fl2fp(dlyFbPct / 100.0f),
                         fl2fp(dlyWetPct / 100.0f));
    chorusSend_.SetParams(fl2fp(choWetPct / 100.0f));
    // freeverb room feedback range: 0.7 - 0.98
    reverbSend_.SetParams(fl2fp(rvWetPct / 100.0f * 1.5f),
                          fl2fp(0.7f + rvSizePct * 0.0028f));
}

int MixerService::GetPlayedBufferPercentage() {
	return out_->GetPlayedBufferPercentage() ;
}

void MixerService::toggleRendering(bool enable) {
    isRendering_ = enable;
    switch (mode_) {
    case MSRM_PLAYBACK:
        initRendering(MSRM_PLAYBACK);
        break;
    case MSRM_STEREO:
        initRendering(MSRM_STEREO);
        out_->EnableRendering(enable);
        break;
    case MSRM_STEMS:
        initRendering(MSRM_STEMS);
        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            bus_[i].EnableRendering(enable);
        };
        break;
    }
}

void MixerService::OnPlayerStart() {
	toggleRendering(true) ;
} ;

void MixerService::OnPlayerStop() {
	toggleRendering(false) ;
} ;

void MixerService::Execute(FourCC id,float value) {
     if (value>0.5) {
        Audio *audio=Audio::GetInstance() ;
        int volume=audio->GetMixerVolume() ;
        switch(id) {
           case TRIG_VOLUME_INCREASE:
                if (volume<100) volume+=1 ;
                break ;
           case TRIG_VOLUME_DECREASE:
                if (volume>0) volume-=1 ;
                break ;                       
        } ;
        audio->SetMixerVolume(volume) ;
     } ;
}

AudioOut *MixerService::GetAudioOut() {
	return out_ ;
} ;


void MixerService::Lock() {
	if (sync_) SDL_LockMutex(sync_) ;
}

void MixerService::Unlock() {
	if (sync_) SDL_UnlockMutex(sync_) ;
}
