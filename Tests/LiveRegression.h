#pragma once
// Regression checks for stage transitions and restoring a two-sound performance.
static bool liveRegressionTests(){
 using namespace juce;
 auto require=[](bool ok,const char* text){if(!ok)std::cerr<<"LIVE regression: "<<text<<"\n";return ok;};
 auto set=[](AudioProcessor& p,const char* id,float value){auto* parameter=dynamic_cast<RangedAudioParameter*>(parameterOf(p,id));parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
 auto save=[](AudioProcessor& p){MemoryBlock data;p.getStateInformation(data);return AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));};
 auto restore=[](AudioProcessor& p,const XmlElement& xml){MemoryBlock data;AudioProcessor::copyXmlToBinary(xml,data);p.setStateInformation(data.getData(),int(data.getSize()));};
 auto render=[](AudioProcessor& p,std::initializer_list<MidiMessage> events){AudioBuffer<float> audio(2,256);MidiBuffer midi;for(const auto& event:events)midi.addEvent(event,0);p.processBlock(audio,midi);return audio;};
 {Arpeggiator::Sequence seq;seq.fromString("48o,60;64");
  if(!require(seq.root==60,"overdub changed the restored root"))return false;
  seq.mergeOverdub();if(!require(seq.root==48,"merging overdub did not update root"))return false;
  Arpeggiator::Sequence back;back.fromString(seq.toString());if(!require(back.root==seq.root,"merged sequence root differs on reload"))return false;
 }
 for(bool live:{false,true}){ // OVD / EDIT direct notes and LIVE both need releases on exit.
  Arpeggiator::Sequence seq;seq.fromString("60;64");Arpeggiator arp;arp.prepare(48000);MidiBuffer midi;
  midi.addEvent(MidiMessage::noteOn(2,72,.8f),0);arp.process(midi,256,true,120,2,4,1,.5f,-1,&seq,true,false,live);
  midi.clear();arp.process(midi,256,true,120,2,4,1,.5f,-1,&seq,false,false,false);
  bool off=false;for(auto e:midi)if(e.getMessage().isNoteOff()&&e.getMessage().getChannel()==2&&e.getMessage().getNoteNumber()==72)off=true;
  if(!require(off,"direct keyboard note stuck after LIVE/OVD exit"))return false;
  midi.clear();midi.addEvent(MidiMessage::noteOff(2,72),0);arp.process(midi,256,true,120,2,4,1,.5f,-1,&seq);
  midi.clear();arp.process(midi,256,false,120,2,4,1,.5f,-1,&seq);
  for(auto e:midi)if(!require(!e.getMessage().isNoteOn(),"released key came back when clock stopped"))return false;
 }
 {Arpeggiator::Sequence seq;seq.fromString("60;64");Arpeggiator arp;arp.prepare(48000);MidiBuffer midi;
  midi.addEvent(MidiMessage::noteOn(1,72,.8f),0);arp.process(midi,256,true,120,2,4,1,.5f,-1,&seq,true,false,true);
  bool original=false;for(auto e:arp.sequenceOutput)if(e.getMessage().isNoteOn()&&e.getMessage().getNoteNumber()==60)original=true;
  if(!require(original,"LIVE keyboard transposed its independent sequence"))return false;
  arp.reset();midi.clear();arp.process(midi,256,true,120,2,4,1,.5f,-1,&seq,true,false,true);
  if(!require(arp.position.load()==0,"reset retained old step"))return false;
 }
 std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
 auto xml=save(*p);xml->setAttribute("sequence","48,55,60,64;50,57,62,65");restore(*p,*xml);
 set(*p,"gain",.4f);set(*p,"cutoff",4000);set(*p,"seqPlay",1);set(*p,"seqLive",1);render(*p,{});
 set(*p,"gain",.8f);set(*p,"cutoff",300);auto saved=save(*p);
 auto liveValue=[](const XmlElement& state,const char* id){if(auto* live=state.getChildByName("LiveSound"))for(auto* child:live->getChildIterator())if(child->getStringAttribute("id")==id)return float(child->getDoubleAttribute("value"));return -1.f;};
 if(!require(std::abs(liveValue(*saved,"gain")-.4f)<1.e-6f && liveValue(*saved,"cutoff")==4000,"frozen LIVE sound not saved independently"))return false;
 std::unique_ptr<AudioProcessor> recalled(createPluginFilter());restore(*recalled,*saved);recalled->setRateAndBufferSizeDetails(48000,256);recalled->prepareToPlay(48000,256);render(*recalled,{});
 auto again=save(*recalled);if(!require(std::abs(liveValue(*again,"gain")-.4f)<1.e-6f && liveValue(*again,"cutoff")==4000,"session recall replaced frozen sound with panel sound"))return false;
 // Check the actual recalled audio against the frozen patch playing the same sequence alone.
 std::unique_ptr<AudioProcessor> reference(createPluginFilter());auto referenceState=std::make_unique<XmlElement>(*saved);
 if(auto* frozen=referenceState->getChildByName("LiveSound")){
  for(auto* source:frozen->getChildIterator())for(auto* dest:referenceState->getChildIterator())if(dest->hasTagName("PARAM")&&dest->getStringAttribute("id")==source->getStringAttribute("id"))dest->setAttribute("value",source->getDoubleAttribute("value"));
  referenceState->removeChildElement(frozen,true);
 }
 for(auto* child:referenceState->getChildIterator())if(child->getStringAttribute("id")=="seqLive")child->setAttribute("value",0);
 restore(*reference,*referenceState);reference->setRateAndBufferSizeDetails(48000,256);reference->prepareToPlay(48000,256);render(*reference,{});
 double error=0,energy=0;
 for(int block=0;block<60;++block){auto a=render(*recalled,{}),b=render(*reference,{});if(block>8)for(int c=0;c<2;++c)for(int i=0;i<256;++i){error+=std::abs(a.getSample(c,i)-b.getSample(c,i));energy+=std::abs(b.getSample(c,i));}}
 if(!require(energy>.1&&error<1.e-4,"recalled LIVE audio differs from its frozen patch"))return false;
 // Loud simultaneous sequence + keyboard: the final sum must remain finite and within full scale.
 set(*p,"seqLive",0);render(*p,{});
 for(auto kv:{std::pair<const char*,float>{"gain",1},{"voiceMode",2},{"unisonSpread",0},{"voiceWidth",0},{"cutoff",18000},{"env",0},{"sustain",1},{"attack",.001f},{"fxDrive",1},{"voiceDrive",1},{"chorus",0},{"vcoSub",1}})set(*p,kv.first,kv.second);
 set(*p,"seqLive",1);double peak=0;
 for(int i=0;i<160;++i){auto b=i==0?render(*p,{MidiMessage::noteOn(1,48,1.f)}):render(*p,{});for(int c=0;c<2;++c)for(int n=0;n<b.getNumSamples();++n){float x=b.getSample(c,n);if(!require(std::isfinite(x)&&std::abs(x)<=1.f,"LIVE sum exceeded full scale"))return false;peak=std::max(peak,double(std::abs(x)));}}
 if(!require(peak>.1,"LIVE ceiling test was silent"))return false;
 // A stopped LIVE release may be longer than the old eight-second cutoff.
 set(*p,"seqPlay",0);set(*p,"seqLive",0);render(*p,{MidiMessage::allSoundOff(1)});
 std::cout<<"PASS LIVE: routing releases, independent transposition, overdub root, frozen sound recall, output ceiling\n";
 return true;
}
