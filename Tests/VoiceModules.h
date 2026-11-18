#pragma once
// VANTAGE voice SPREAD: PITCH puts a low note on the left and a high note on the right (scaled by WIDTH).
static bool voiceModuleTests(){
 using namespace juce;
 auto side=[](int note){std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  for(auto [id,v]:std::initializer_list<std::pair<const char*,float>>{{"reverbOn",0},{"delayOn",0},{"chorusMode",0},{"voiceWidth",1},{"voiceSpread",1},{"mode",1},{"voiceMode",0}})
   if(auto* r=dynamic_cast<RangedAudioParameter*>(parameterOf(*p,id)))r->setValueNotifyingHost(r->convertTo0to1(v));
  AudioBuffer<float> b(2,256);double l=0,r=0;for(int i=0;i<40;++i){MidiBuffer m;if(i==0)m.addEvent(MidiMessage::noteOn(1,note,.8f),0);p->processBlock(b,m);if(i>10){l+=b.getRMSLevel(0,0,256);r+=b.getRMSLevel(1,0,256);}}
  return Decibels::gainToDecibels(float(r/l));};
 const float low=side(30),centre=side(60),high=side(90);
 if(low>-20||std::abs(centre)>.5f||high<20){std::cerr<<"VOICE SPREAD: R/L "<<low<<" "<<centre<<" "<<high<<" dB\n";return false;}
 std::cout<<"PASS voice spread PITCH: low note left, middle C centred, high note right (R/L "<<low<<", "<<centre<<", "<<high<<" dB)\n";return true;
}
