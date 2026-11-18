#pragma once
// Last-note priority across MIDI channels, fixed storage, sample-timed JUCE dispatch.
class VoiceAllocator : public Synthesiser {
public:
 // wide: this instrument spreads its voices across the stereo field (WIDTH), each voice keeping its own place.
 explicit VoiceAllocator(AudioProcessorValueTreeState& s,bool spreadVoices=false,const ParamLayer* frozen=nullptr):state(s),parameterReads(s),wide(spreadVoices),layer(frozen){setMinimumRenderingSubdivisionSize(1,true);}
 void updateMode(){int next=int(value("voiceMode"));if(next!=mode){allNotesOff(0,false);mode=next;}}
 void noteOn(int channel,int note,float velocity) override {
  if(mode==0){for(auto* base:voices)static_cast<Voice*>(base)->setUnison(0,1);Synthesiser::noteOn(channel,note,velocity);for(auto* base:voices)if(base->isPlayingChannel(channel))base->controllerMoved(1,modWheel[size_t(channel-1)]);return;}
  auto& key=keys[size_t((channel-1)*128+note)];key.down=true;key.velocity=velocity;key.order=++counter;select(true);
 }
 void noteOff(int channel,int note,float velocity,bool tail) override {
  if(mode==0){Synthesiser::noteOff(channel,note,velocity,tail);return;}
  auto& key=keys[size_t((channel-1)*128+note)];key.down=false;if(!sustain[size_t(channel-1)])key.order=0;select();
 }
 void allNotesOff(int channel,bool tail) override {
  if(channel<=0){keys={};sustain={};active=-1;Synthesiser::allNotesOff(0,tail);}
  else {for(int n=0;n<128;++n)keys[size_t((channel-1)*128+n)]={};sustain[size_t(channel-1)]=false;if(mode==0)Synthesiser::allNotesOff(channel,tail);else {if(!tail&&active>=0&&active/128+1==channel){for(auto* v:voices)v->stopNote(0,false);active=-1;}select();}}
 }
 void handleController(int channel,int cc,int amount) override {
  if(cc==1)modWheel[size_t(channel-1)]=amount;
  if(mode!=0 && cc==64){sustain[size_t(channel-1)]=amount>=64;if(amount<64){for(int n=0;n<128;++n){auto& key=keys[size_t((channel-1)*128+n)];if(!key.down)key.order=0;}select();}return;}
  if(mode==0)Synthesiser::handleController(channel,cc,amount);
  else if(active>=0 && active/128+1==channel)for(auto* v:voices)v->controllerMoved(cc,amount);
 }
 void handlePitchWheel(int channel,int amount) override {if(mode==0)Synthesiser::handlePitchWheel(channel,amount);else if(active>=0&&active/128+1==channel)for(auto* v:voices)v->pitchWheelMoved(amount);}
 // Legato retargeting keeps JUCE voice metadata on the original key; route pressure by our active key.
 void handleAftertouch(int channel,int note,int amount) override {
  if(mode==0)Synthesiser::handleAftertouch(channel,note,amount);
  else if(active>=0&&active%128==note&&(channel<=0||active/128+1==channel))for(auto* v:voices)if(v->isVoiceActive())v->aftertouchChanged(amount);
 }
 void handleChannelPressure(int channel,int amount) override {
  if(mode==0)Synthesiser::handleChannelPressure(channel,amount);
  else if(active>=0&&(channel<=0||active/128+1==channel))for(auto* v:voices)if(v->isVoiceActive())v->channelPressureChanged(amount);
 }
protected:
 // The wide instrument hands notes to its voices in turn, so repeated notes do not always fall on the same voice
 // (each one has its own small errors and its own place in the stereo field).
 SynthesiserVoice* findFreeVoice(SynthesiserSound* sound,int channel,int note,bool steal) const override {
  if(wide){const int n=voices.size();for(int k=0;k<n;++k){auto* voice=voices[(turn+k)%n];if(!voice->isVoiceActive()&&voice->canPlaySound(sound)){turn=(turn+k+1)%n;return voice;}}}
  return Synthesiser::findFreeVoice(sound,channel,note,steal);
 }
 void handleMidiEvent(const MidiMessage& message) override {if(message.isAllSoundOff()){allNotesOff(message.getChannel(),false);return;}Synthesiser::handleMidiEvent(message);}
 void renderVoices(AudioBuffer<float>& output,int start,int count)override {
  const int n=mode==2?unisonCount():1;for(int i=0;i<voices.size();++i)static_cast<Voice*>(voices[i])->setUnison(mode==2&&n>1?(2.f*float(jmin(i,n-1))/float(n-1)-1)*value("unisonSpread"):0,mode==2?unisonGain(n):1.f);
  // SPREAD, scaled by WIDTH; a single mono voice stays in the centre.
  static constexpr float places[8]={-.35f,.35f,-.7f,.7f,-.15f,.15f,-.9f,.9f}; // ALT: neighbours alternate sides
  const float width=wide&&mode!=1?value("voiceWidth"):0.f;const int spread=int(value("voiceSpread"));
  for(int i=0;i<voices.size();++i){auto* v=static_cast<Voice*>(voices[i]);const int note=v->getCurrentlyPlayingNote();
   const float place=spread==1?(note>=0?jlimit(-1.f,1.f,float(note-60)/30.f):0.f):spread==2?v->randomPlace():places[i%8];
   v->setPan(place*width);}
  Synthesiser::renderVoices(output,start,count);
 }
private:
 struct Key{uint64 order=0;float velocity=0;bool down=false;};
 // UNISON: how many voices stack on the note (1 to all of them).
 int unisonCount()const{return jlimit(1,voices.size(),int(value("unisonVoices"))+1);}
 float value(const char* id)const{const auto* a=parameterReads.get(id);return layer!=nullptr?layer->get(a):a->load();}
 // Level of each voice in a unison stack of n. Detuned voices add up to about sqrt(n) times one voice,
 // so 1/sqrt(n) would only match a single voice; this exponent leaves the stack about 4 to 5 dB louder.
 // Closer to 0 = more powerful (0 = every voice at full level), -0.5 = same loudness as mono.
 static constexpr float unisonExponent=-.25f;
 static float unisonGain(int n){return std::pow(float(jmax(1,n)),unisonExponent);}
 void select(bool retrigger=false){int newest=-1;uint64 order=0;for(int i=0;i<2048;++i)if(keys[size_t(i)].order>order){order=keys[size_t(i)].order;newest=i;}
  if(newest==active&&!retrigger)return;
  if(newest<0){for(auto* v:voices)v->stopNote(0,true);active=-1;return;}
  bool keep=active>=0&&value("legato")>.5f;int channel=newest/128+1,note=newest%128,n=mode==2?unisonCount():1;float velocity=keys[size_t(newest)].velocity;
  for(int i=0;i<n;++i){auto* v=static_cast<Voice*>(voices[i]);v->setUnison(n>1?(2.f*i/(n-1)-1)*value("unisonSpread"):0,unisonGain(n));
   if(keep&&v->isVoiceActive())v->retarget(note,velocity,lastPitchWheelValues[channel-1]);else startVoice(v,sounds[0],channel,note,velocity);
   v->controllerMoved(1,modWheel[size_t(channel-1)]);
  }active=newest;
 }
 AudioProcessorValueTreeState& state;ParameterReadCache parameterReads;bool wide=false;const ParamLayer* layer=nullptr;mutable int turn=0;std::array<Key,2048> keys{};std::array<bool,16> sustain{};std::array<int,16> modWheel{};uint64 counter=0;int mode=-1,active=-1;
};
