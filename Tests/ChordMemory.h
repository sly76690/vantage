#pragma once
// VANTAGE chord memory: a key plays the stored chord on its own pitch, seen by the sequencer too; the chord travels with the state.
static bool chordMemoryTests(){
 using namespace juce;
 std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
 auto set=[&](const char* id,float v){auto* r=dynamic_cast<RangedAudioParameter*>(parameterOf(*p,id));r->setValueNotifyingHost(r->convertTo0to1(v));};
 auto block=[&](std::initializer_list<MidiMessage> msgs){AudioBuffer<float> b(2,256);b.clear();MidiBuffer m;for(auto& x:msgs)m.addEvent(x,0);p->processBlock(b,m);};
 MemoryBlock data;p->getStateInformation(data);auto xml=AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
 xml->setAttribute("chord","0,3,7,10");AudioProcessor::copyXmlToBinary(*xml,data);p->setStateInformation(data.getData(),int(data.getSize()));
 set("chordOn",1);set("seqRec",1);block({});block({MidiMessage::noteOn(1,48,.8f)});block({MidiMessage::noteOff(1,48)});
 set("chordOn",0);block({MidiMessage::noteOn(1,50,.8f)});block({MidiMessage::noteOff(1,50)});set("seqRec",0);block({});
 p->getStateInformation(data);xml=AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
 const auto seq=xml->getStringAttribute("sequence"),chord=xml->getStringAttribute("chord");
 if(seq!="48,51,55,58;50"||chord!="0,3,7,10"){std::cerr<<"CHORD MEMORY: sequence "<<seq<<", chord "<<chord<<"\n";return false;}
 std::cout<<"PASS chord memory: one key records the minor seventh chord (48,51,55,58), chord off plays the key alone, chord kept in the state\n";return true;
}
