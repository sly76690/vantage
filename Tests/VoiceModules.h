#pragma once
// VANTAGE voice modules: each voice's PAN places it in the stereo field (scaled by WIDTH).
static bool voiceModuleTests(){
 using namespace juce;
 auto side=[](float pan){std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  for(auto [id,v]:std::initializer_list<std::pair<const char*,float>>{{"reverbOn",0},{"delayOn",0},{"chorusMode",0},{"voiceWidth",1},{"voicePan1",pan},{"mode",1}})
   if(auto* r=dynamic_cast<RangedAudioParameter*>(parameterOf(*p,id)))r->setValueNotifyingHost(r->convertTo0to1(v));
  AudioBuffer<float> b(2,256);double l=0,r=0;for(int i=0;i<40;++i){MidiBuffer m;if(i==0)m.addEvent(MidiMessage::noteOn(1,60,.8f),0);p->processBlock(b,m);if(i>10){l+=b.getRMSLevel(0,0,256);r+=b.getRMSLevel(1,0,256);}}
  return Decibels::gainToDecibels(float(r/l));};
 const float hardLeft=side(-1),centre=side(0),hardRight=side(1);
 if(hardLeft>-30||std::abs(centre)>.5f||hardRight<30){std::cerr<<"VOICE MODULES: pan R/L "<<hardLeft<<" "<<centre<<" "<<hardRight<<" dB\n";return false;}
 std::cout<<"PASS voice modules: voice 1 PAN hard left, centre and hard right (R/L "<<hardLeft<<", "<<centre<<", "<<hardRight<<" dB)\n";return true;
}
