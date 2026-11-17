#pragma once
// Audit fixes (Orbit / Vantage engine): oversized host blocks, voice-count change during a note, synced delay length.
static bool engineFixTests(){
 using namespace juce;
 auto require=[](bool ok,const char* text){if(!ok)std::cerr<<"ENGINE fixes: "<<text<<"\n";return ok;};
 auto set=[](AudioProcessor& p,const char* id,float value){auto* parameter=dynamic_cast<RangedAudioParameter*>(parameterOf(p,id));parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
 auto make=[&](int block){std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,block);p->prepareToPlay(48000,block);
  set(*p,"reverbOn",0);set(*p,"delayOn",0);set(*p,"chorusMode",0);set(*p,"release",.3f);return p;};
 // 1. A host block four times the prepared capacity plays in pieces, like the same audio in normal blocks.
 {auto big=make(256),small=make(256);const int length=40000;
  AudioBuffer<float> a(2,length);MidiBuffer m;m.addEvent(MidiMessage::noteOn(1,57,.8f),0);m.addEvent(MidiMessage::noteOff(1,57),30000);big->processBlock(a,m);
  AudioBuffer<float> b(2,length);b.clear();for(int done=0;done<length;done+=256){AudioBuffer<float> piece(b.getArrayOfWritePointers(),2,done,256);MidiBuffer n;
   if(done==0)n.addEvent(MidiMessage::noteOn(1,57,.8f),0);if(done==29952)n.addEvent(MidiMessage::noteOff(1,57),48);small->processBlock(piece,n);}
  const float ra=a.getRMSLevel(0,0,length),rb=b.getRMSLevel(0,0,length);
  if(!require(ra>.001f&&std::abs(Decibels::gainToDecibels(ra/rb))<1.f,"oversized block does not match normal blocks"))return false;}
 // 2. Switching 8 -> 6 voices while a note sounds: the note ends with its release instead of being cut.
 {auto p=make(256);set(*p,"mode",1);set(*p,"release",1.5f);
  AudioBuffer<float> audio(2,256);MidiBuffer m;m.addEvent(MidiMessage::noteOn(1,60,.8f),0);p->processBlock(audio,m);
  for(int i=0;i<40;++i){m.clear();p->processBlock(audio,m);}
  const float before=audio.getRMSLevel(0,0,256);set(*p,"mode",0);m.clear();
  float after=0;for(int i=0;i<4;++i){p->processBlock(audio,m);after=audio.getRMSLevel(0,0,256);}
  if(!require(before>.001f&&after>before*.25f,"voice-count change cut the sounding note"))return false;}
 // 3. Synced delay: a half note at 60 BPM repeats after 2 s, not after 1 s.
 {auto p=make(256);set(*p,"delayOn",1);set(*p,"delayMix",1);set(*p,"delayFeedback",0);set(*p,"delaySync",1);set(*p,"arpSync",0);set(*p,"arpTempo",60);
  set(*p,"attack",.002f);set(*p,"decay",.05f);set(*p,"sustain",0);set(*p,"release",.03f);
  const int length=48000*5/2;AudioBuffer<float> all(2,length);all.clear();
  for(int done=0;done<length;done+=256){AudioBuffer<float> piece(all.getArrayOfWritePointers(),2,done,jmin(256,length-done));MidiBuffer m;
   if(done==0)m.addEvent(MidiMessage::noteOn(1,60,1.f),0);if(done==2560)m.addEvent(MidiMessage::noteOff(1,60),0);p->processBlock(piece,m);}
  const float oneSecond=all.getRMSLevel(0,48000-2400,9600),twoSeconds=all.getRMSLevel(0,96000-2400,9600);
  if(!require(twoSeconds>.002f&&twoSeconds>oneSecond*4,"synced half note at 60 BPM did not repeat at 2 s"))return false;}
 std::cout<<"PASS engine fixes: oversized host blocks, voice-count change keeps the release, synced delay up to 4 s\n";
 return true;
}
