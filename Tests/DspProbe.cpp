#include <juce_audio_utils/juce_audio_utils.h>
#include <fstream>
#include <iostream>
#include <chrono>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main(int argc,char** argv){
 if(argc!=2)return 1;juce::ScopedJuceInitialiser_GUI gui;std::ofstream out(argv[1],std::ios::binary);if(!out)return 2;
 for(int scene=0;scene<9;++scene){std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
  auto set=[&](const char* id,float v){for(auto* base:p->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(base))if(r->paramID==id)r->setValueNotifyingHost(r->convertTo0to1(v));};
  set("mode",1);set("age",float(scene%3)*.5f);set("chorus",0);set("phaserOn",0);set("delayMix",0);set("reverbMix",0);set("jupiterMix",1);set("vco1Wave",scene<3?1.f/3.f:scene<6?2.f/3.f:0.f);set("vco2Wave",scene<3?1.f/3.f:scene<6?2.f/3.f:0.f);set("lfoFilter",.3f);set("lfoPitch",.1f);set("envLink",0);set("release",.3f);
  p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  auto begin=std::chrono::steady_clock::now();
  for(int block=0;block<375;++block){juce::AudioBuffer<float> b(2,256);juce::MidiBuffer midi;
   if(block==0)for(int note:{48,55,60,64,67,72,76,79})midi.addEvent(juce::MidiMessage::noteOn(1,note,.8f),0);
   if(block==250)for(int note:{48,55,60,64,67,72,76,79})midi.addEvent(juce::MidiMessage::noteOff(1,note),0);
   p->processBlock(b,midi);for(int c=0;c<2;++c)out.write(reinterpret_cast<const char*>(b.getReadPointer(c)),256*sizeof(float));
  }
  std::cout<<"scene "<<scene<<" age "<<float(scene%3)*.5f<<" seconds "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"\n";
 }
}
