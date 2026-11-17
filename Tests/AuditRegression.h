#pragma once
#include "../Source/NoteHold.h"
#include "../Source/CharacterDelay.h"

static bool auditRegression() {
 auto require=[](bool ok,const char* message){if(!ok)std::cerr<<message<<"\n";return ok;};
 for(bool soundOff:{false,true}) {
  NoteHold hold;hold.prepare();juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
  midi.addEvent(juce::MidiMessage::noteOn(2,67,.8f),1);
  midi.addEvent(juce::MidiMessage::noteOff(1,60),2);
  midi.addEvent(juce::MidiMessage::noteOff(2,67),3);hold.process(midi,true);
  midi.clear();midi.addEvent(soundOff?juce::MidiMessage::allSoundOff(1):juce::MidiMessage::allNotesOff(1),0);hold.process(midi,true);
  midi.clear();hold.process(midi,false);bool released=false;
  for(auto event:midi)if(event.getMessage().isNoteOff()&&event.getMessage().getChannel()==2&&event.getMessage().getNoteNumber()==67)released=true;
  if(!require(released,"HOLD lost another channel's held note"))return false;
 }
 auto render=[](int mode,int vca,bool linked,float ampRelease,float filterRelease,int pressureKind) {
  std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
  auto set=[&](const char* id,float v){auto* param=dynamic_cast<juce::RangedAudioParameter*>(parameterOf(*p,id));param->setValueNotifyingHost(param->convertTo0to1(v));};
  set("voiceMode",float(mode));set("legato",1);set("vcaMode",float(vca));set("envLink",linked?1.f:0.f);
  set("attack",.002f);set("sustain",1);set("release",ampRelease);set("frelease",filterRelease);
  set("chorus",0);set("delayMix",0);set("reverbMix",0);set("atVolume",1);set("env",0);
  p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  std::vector<float> result;
  for(int n=0;n<140;++n){juce::MidiBuffer midi;juce::AudioBuffer<float> audio(2,256);
   if(n==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
   if(pressureKind>=0){
    if(n==4)midi.addEvent(juce::MidiMessage::noteOn(2,67,.8f),0);
    if(n==8){
     if(pressureKind==1)midi.addEvent(juce::MidiMessage::aftertouchChange(2,67,127),0);
     if(pressureKind==2)midi.addEvent(juce::MidiMessage::channelPressureChange(2,127),0);
     if(pressureKind==3)midi.addEvent(juce::MidiMessage::aftertouchChange(1,60,127),0);
     if(pressureKind==4)midi.addEvent(juce::MidiMessage::channelPressureChange(1,127),0);
    }
   }else if(n==8)midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
   p->processBlock(audio,midi);result.insert(result.end(),audio.getReadPointer(0),audio.getReadPointer(0)+256);
  }return result;
 };
 auto difference=[](const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t i=0;i<a.size();++i)d+=std::abs(a[i]-b[i]);return d;};
 for(auto releases:{std::pair<float,float>{.5f,.01f},{.01f,.5f}}){
  auto amp=render(0,0,true,releases.first,releases.second,-1),linked=render(0,2,true,releases.first,releases.second,-1);
  if(!require(difference(amp,linked)<1.e-6,"Linked VCA release differs from amp envelope"))return false;
 }
 for(int mode:{1,2}) {
  auto dry=render(mode,0,false,.1f,.1f,0),poly=render(mode,0,false,.1f,.1f,1),channel=render(mode,0,false,.1f,.1f,2);
  if(!require(difference(dry,poly)>.01&&difference(poly,channel)<1.e-6,"Legato pressure misses active key/channel"))return false;
  for(int old:{3,4})if(!require(difference(dry,render(mode,0,false,.1f,.1f,old))<1.e-6,"Legato pressure follows old key/channel"))return false;
 }
 // Two identical delay lines: compare a toggled wet signal with its uninterrupted reference.
 for(bool enable:{false,true}) {
  CharacterDelay reference,toggled;reference.prepare(48000);toggled.prepare(48000);
  for(int i=0;i<20000;++i){juce::AudioBuffer<float> a(2,1),b(2,1);float x=.2f*std::sin(float(i)*.031f);
   for(int c=0;c<2;++c){a.setSample(c,0,x);b.setSample(c,0,x);}
   reference.process(a,.01f,.4f,1,0);toggled.process(b,.01f,.4f,enable?0.f:1.f,0);
  }
  double energy=0;
  for(int i=0;i<1100;++i){juce::AudioBuffer<float> a(2,1),b(2,1);a.clear();b.clear();
   reference.process(a,.01f,.4f,1,0);toggled.process(b,.01f,.4f,enable?1.f:0.f,0);
   float ramp=juce::jmin(1.f,float(i+1)/960.f),gain=enable?ramp:1.f-ramp;
   for(int c=0;c<2;++c){energy+=std::abs(a.getSample(c,0));if(!require(std::abs(b.getSample(c,0)-gain*a.getSample(c,0))<1.e-5,"Delay toggle does not fade smoothly"))return false;}
  }
  if(!require(energy>.01,"Delay fade test has no echo"))return false;
 }
 std::cout<<"PASS audit regressions: multichannel HOLD, linked VCA release, legato pressure, delay fades\n";
 return true;
}
