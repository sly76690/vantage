#pragma once
// VANTAGE unison voice count: UNI with one voice sounds like MONO (and stays finite), with eight it is a different, wider stack.
static bool unisonTests(){
 using namespace juce;
 auto render=[](float mode,float count){std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  for(auto [id,v]:std::initializer_list<std::pair<const char*,float>>{{"reverbOn",0},{"delayOn",0},{"chorusMode",0},{"age",0},{"voiceMode",mode},{"unisonVoices",count},{"voiceWidth",0}})
   if(auto* r=dynamic_cast<RangedAudioParameter*>(parameterOf(*p,id)))r->setValueNotifyingHost(r->convertTo0to1(v));
  AudioBuffer<float> b(2,256),all(1,256*60);for(int i=0;i<60;++i){MidiBuffer m;if(i==0)m.addEvent(MidiMessage::noteOn(1,57,.8f),0);p->processBlock(b,m);all.copyFrom(0,i*256,b,0,0,256);}return all;};
 const auto mono=render(1,0),one=render(2,0),eight=render(2,7);
 double same=0,diff=0;bool finite=true;for(int i=0;i<mono.getNumSamples();++i){finite&=std::isfinite(one.getSample(0,i));same=jmax(same,double(std::abs(mono.getSample(0,i)-one.getSample(0,i))));diff+=std::abs(mono.getSample(0,i)-eight.getSample(0,i));}
 if(!finite||same>1e-3||diff/mono.getNumSamples()<.003){std::cerr<<"UNISON: one voice vs mono "<<same<<", eight vs mono "<<diff/mono.getNumSamples()<<"\n";return false;}
 std::cout<<"PASS unison voices: UNI x1 equals MONO, UNI x8 stacks\n";return true;
}
