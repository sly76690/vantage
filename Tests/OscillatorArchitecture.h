#pragma once
#include "../Source/OscillatorBank.h"
#include <chrono>

static bool oscillatorArchitecture(){
 using Settings=OscillatorBank::Settings;
 auto render=[](Settings settings,double sr=48000,double hz=220,int length=12000){
  OscillatorBank bank;bank.setVoiceIndex(3);bank.prepare(sr);bank.configure(settings,true);
  std::vector<float> result;result.reserve(size_t(length));
  for(int i=0;i<length;++i)result.push_back(bank.process(hz,0,0));return result;
 };
 auto diff=[](const std::vector<float>& a,const std::vector<float>& b){double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(a[i]-b[i]);return sum;};
 auto require=[](bool ok,const char* what){if(!ok)std::cerr<<"Architecture: "<<what<<"\n";return ok;};
 Settings base;base.age=0;base.sub=0;base.noise=.2f;base.shape1=0;base.shape2=3;
 auto juno=render(base);auto changed=base;changed.age=1;changed.crossMod=1;changed.sync=true;changed.lowFrequency=true;
 changed.shape1=5;changed.shape2=5;changed.sourceMix=.95f;changed.range1=5;changed.fine=-50;
 if(!require(diff(juno,render(changed))==0,"Jupiter controls alter pure Juno"))return false;
 auto jupiter=base;jupiter.blend=1;auto cleanJupiter=render(jupiter);changed=jupiter;
 changed.wave=1;changed.width=.1f;changed.level=0;changed.sub=1;changed.noise=1;
 if(!require(diff(cleanJupiter,render(changed))==0,"DCO controls alter pure Jupiter"))return false;
 changed=base;changed.blend=.5f;auto mixed=render(changed);
 for(size_t i=0;i<mixed.size();++i)if(!require(std::abs(mixed[i]-.5f*(juno[i]+cleanJupiter[i]))<1.e-6,"source crossfade is not before filters"))return false;
 for(float endpoint:{0.f,1.f}){
  auto isolated=jupiter;isolated.sourceMix=endpoint;auto before=render(isolated);
  if(endpoint==0)isolated.shape2=5;else isolated.shape1=5;
  if(!require(diff(before,render(isolated))==0,"SOURCE MIX does not isolate VCOs"))return false;
 }
 auto aged=jupiter;aged.age=1;if(!require(diff(cleanJupiter,render(aged))>.01,"AGE does not affect Jupiter"))return false;
 auto pwm=jupiter;pwm.shape1=pwm.shape2=1;pwm.sourceMix=0;auto before=render(pwm);pwm.width1=.2f;
 if(!require(diff(before,render(pwm))>.01,"VCO 1 width inactive"))return false;
 pwm.sourceMix=1;before=render(pwm);pwm.width2=.2f;
 if(!require(diff(before,render(pwm))>.01,"VCO 2 width inactive"))return false;
 // Modulation depths are separate: DCO PWM cannot move VCOs, and vice versa.
 for(float section:{0.f,1.f}){
  Settings pulse;pulse.blend=section;pulse.shape1=pulse.shape2=1;pulse.wave=1;pulse.sub=0;pulse.age=0;
  OscillatorBank clean,modulated,other;for(auto* bank:{&clean,&modulated,&other}){bank->setVoiceIndex(0);bank->prepare(48000);bank->configure(pulse,true);}
  double audible=0;
  for(int i=0;i<12000;++i){float dry=clean.process(220,0,0,0);
   float wet=modulated.process(220,0,section==0?.1f:0,section==1?.1f:0);
   float untouched=other.process(220,0,section==1?.1f:0,section==0?.1f:0);
   if(!require(dry==untouched,"PWM crosses source boundary"))return false;audible+=std::abs(wet-dry);}
  if(!require(audible>.01,"source PWM inactive"))return false;
 }
 auto frequency=[](const std::vector<float>& signal,double rate){int first=-1,last=-1,count=0;for(size_t i=1;i<signal.size();++i)if(signal[i-1]<=0&&signal[i]>0){if(first<0)first=int(i);last=int(i);++count;}return count>1?rate*(count-1)/(last-first):0.;};
 for(double sr:{44100.,48000.,96000.}){
  for(int range=0;range<6;++range){Settings tone;tone.blend=1;tone.sourceMix=1;tone.shape2=3;tone.age=0;tone.range2=range;
   const double target=220.*std::exp2(range-3.);
   if(!require(std::abs(frequency(render(tone,sr,220,int(sr)),2*sr)-target)<.5,"VCO range tuning"))return false;
  }
  Settings low;low.blend=1;low.sourceMix=1;low.shape2=3;low.age=0;low.lowFrequency=true;
  auto slow=render(low,sr,110,int(sr*4));
  if(!require(std::abs(frequency(slow,sr*2)-2.)<.01&&diff(slow,render(low,sr,880,int(sr*4)))==0,"LOW FREQ must be 2 Hz and keyboard-independent"))return false;
  for(int shape=0;shape<6;++shape){auto extreme=jupiter;extreme.shape1=extreme.shape2=shape;extreme.crossMod=1;extreme.sync=true;extreme.range1=extreme.range2=5;extreme.age=1;
   for(float x:render(extreme,sr,12000))if(!require(std::isfinite(x)&&std::abs(x)<2,"extreme oscillator output"))return false;
  }
 }
 auto fine=jupiter;fine.sourceMix=1;fine.shape2=3;fine.fine=-50;
 auto negative=frequency(render(fine,48000,440,96000),96000);fine.fine=50;
 auto positive=frequency(render(fine,48000,440,96000),96000);
 if(!require(negative<440&&positive>440&&std::abs(positive/negative-std::exp2(100./1200))<.001,"bipolar fine tuning"))return false;
 auto mod=jupiter;mod.sourceMix=0;auto plain=render(mod);mod.crossMod=.8f;
 if(!require(diff(plain,render(mod))>.01,"cross modulation without audible VCO 2"))return false;
 auto sync=jupiter;sync.sourceMix=1;sync.tune=7;plain=render(sync);sync.sync=true;
 if(!require(diff(plain,render(sync))>.01,"sync without audible VCO 1"))return false;
 // Processor integration, session recall, count independence and an indicative sustained-unison benchmark.
 for(int count:{0,1}){
  std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
  auto set=[&](const char* id,float value){auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(parameterOf(*p,id));parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
  set("mode",float(count));set("jupiterMix",.5f);set("vco1Width",.23f);set("vco2Low",1);set("voiceMode",2);set("crossMod",.7f);
  juce::MemoryBlock state;p->getStateInformation(state);set("jupiterMix",0);set("vco2Low",0);p->setStateInformation(state.getData(),int(state.getSize()));
  if(!require(std::abs(parameterOf(*p,"jupiterMix")->getValue()-.5f)<1.e-6&&parameterOf(*p,"vco2Low")->getValue()==1,"new parameters not restored"))return false;
  p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;double energy=0;
  const auto start=std::chrono::steady_clock::now();
  for(int block=0;block<375;++block){midi.clear();if(block==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);p->processBlock(audio,midi);
   for(int c=0;c<2;++c)for(int i=0;i<256;++i){float x=audio.getSample(c,i);if(!require(std::isfinite(x)&&std::abs(x)<=1.001f,"processor output invalid"))return false;energy+=x*x;}}
  const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  if(!require(energy>.01,"hybrid unison silent"))return false;
  std::cout<<"BENCH "<<(count?8:6)<<" voices, hybrid unison: "<<elapsed<<" s for 2 s audio ("<<elapsed/2*100<<"% of one real-time budget)\n";
 }
 std::cout<<"PASS oscillator architecture: isolation, mixtures, ranges, LOW FREQ, AGE, PWM, fine, cross-mod, sync, state\n";
 return true;
}
