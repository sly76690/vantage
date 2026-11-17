#pragma once
#include "../Source/OscillatorBank.h"
#include "../Source/Arpeggiator.h"
// The voice's filter, compiled here on its own (the plugin builds the same header with its own Sound type).
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
namespace poly8test {
 using namespace juce;
 struct Sound : SynthesiserSound { bool appliesToNote(int) override {return true;} bool appliesToChannel(int) override {return true;} };
 #include "../Source/Voice.h"
}
// Vantage: continuous SHAPE, the VCO mixer and POLY-MOD, tested on the oscillator bank and through the plugin.
static bool polyModTests(){
 using Settings=OscillatorBank::Settings;
 auto render=[](Settings s,float filterEnvelope=0,int length=12000){
  OscillatorBank bank;bank.setVoiceIndex(2);bank.prepare(48000);bank.configure(s,true);std::vector<float> r;r.reserve(size_t(length));
  for(int i=0;i<length;++i)r.push_back(bank.process(220,0,0,0,filterEnvelope));return r;};
 auto diff=[](const std::vector<float>& a,const std::vector<float>& b){double d=0;for(size_t i=0;i<a.size();++i)d+=std::abs(a[i]-b[i]);return d/double(a.size());};
 auto require=[](bool ok,const juce::String& what){if(!ok)std::cerr<<"Poly-mod: "<<what<<"\n";return ok;};
 Settings base;base.blend=1;base.age=0;base.shape1=base.shape2=6;base.sourceMix=.5f;
 // SHAPE: every quarter of the travel sounds different, small moves stay small (no jump between regions).
 std::vector<std::vector<float>> shapes;for(float m:{0.f,.33f,.66f,1.f}){auto s=base;s.morph1=s.morph2=m;shapes.push_back(render(s));}
 for(size_t i=1;i<shapes.size();++i)if(!require(diff(shapes[i-1],shapes[i])>.02,"SHAPE positions sound alike"))return false;
 for(float edge:{1.f/3.f,2.f/3.f}){auto a=base,b=base;a.morph1=a.morph2=edge-.002f;b.morph1=b.morph2=edge+.002f;
  if(!require(diff(render(a),render(b))<.02,"SHAPE jumps between regions"))return false;}
 for(auto& s:shapes)for(float v:s)if(!require(std::isfinite(v)&&std::abs(v)<2.f,"SHAPE output out of range"))return false;
 // Mixer: SUB and NOISE are heard on the VCO section, and only when turned up.
 auto clean=render(base);
 {auto s=base;s.vcoSub=1;if(!require(diff(clean,render(s))>.02,"VCO SUB inactive"))return false;}
 {auto s=base;s.vcoNoise=1;if(!require(diff(clean,render(s))>.02,"VCO NOISE inactive"))return false;}
 // POLY-MOD at zero changes nothing, whatever the envelope does and whichever destinations are on.
 {auto s=base;s.polyFreq=s.polyWidth=true;if(!require(diff(clean,render(s,1.f))==0,"POLY-MOD at zero alters the sound"))return false;}
 // Each source moves VCO 2's frequency, both directions, only through an enabled destination.
 for(float amount:{-1.f,.5f}){auto s=base;s.polyEnv=amount;if(!require(diff(clean,render(s,1.f))>.02,"POLY-MOD envelope has no effect on FREQ 2"))return false;
  s.polyFreq=false;if(!require(diff(clean,render(s,1.f))==0,"POLY-MOD reaches FREQ 2 while it is off"))return false;}
 {auto s=base;s.polyOsc=.6f;if(!require(diff(clean,render(s))>.02,"POLY-MOD VCO 1 has no effect"))return false;}
 {auto s=base;s.morph2=.8f;auto pulse=s;auto before=render(pulse);s.polyFreq=false;s.polyWidth=true;s.polyEnv=.7f;
  if(!require(diff(before,render(s,1.f))>.01,"POLY-MOD has no effect on PW 2"))return false;}
 {auto s=base;s.polyOsc=1;s.polyEnv=1;OscillatorBank bank;bank.setVoiceIndex(0);bank.prepare(48000);bank.configure(s,true);
  for(int i=0;i<48000;++i){float v=bank.process(55+i*.05,0,0,0,float(i%4800)/4800.f);if(!require(std::isfinite(v)&&std::abs(v)<3.f,"POLY-MOD at full depth is unstable"))return false;}}
 // Through the plugin: POLY-MOD > FILTER moves the filters, and the destinations are independent.
 auto plug=[](std::initializer_list<std::pair<const char*,float>> values){std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());
  for(auto& kv:values)for(auto* b:q->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(b))if(r->paramID==kv.first)r->setValueNotifyingHost(r->convertTo0to1(kv.second));
  q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);std::vector<float> out;
  for(int k=0;k<60;++k){juce::AudioBuffer<float> b(2,256);b.clear();juce::MidiBuffer m;if(k==0)m.addEvent(juce::MidiMessage::noteOn(1,48,.9f),0);q->processBlock(b,m);for(int i=0;i<256;++i)out.push_back(b.getSample(0,i));}
  return out;};
 const auto dry=plug({{"polyOsc",.8f},{"polyFreq",0},{"cutoff",600}});
 const auto filtered=plug({{"polyOsc",.8f},{"polyFreq",0},{"polyFilter",1},{"cutoff",600}});
 if(!require(diff(dry,filtered)>.001,"POLY-MOD > FILTER inactive"))return false;
 for(float v:filtered)if(!require(std::isfinite(v)&&std::abs(v)<4.f,"POLY-MOD > FILTER unstable"))return false;
 // Ladder at the top of RES: rings on by itself after a short click (self-oscillation), stays bounded; at RES 5 it dies away.
 {auto ring=[](float resonance,float hz=1000.f){poly8test::FilterSlot f;f.prepare(96000);f.setDrive(1.2f);f.configure(0,1,resonance);double tail=0,peak=0;
   for(int i=0;i<96000;++i){float y=f.process(i<20?1.f:0.f,hz);if(!std::isfinite(y))return -1.0;peak=std::max(peak,double(std::abs(y)));if(i>86400)tail+=double(y)*y;}
   return peak>4.?-1.:std::sqrt(tail/9600.);};
  const double singing=ring(.95f),damped=ring(.5f),edge=ring(.88f);
  if(!require(edge>=0&&edge<1.e-3,"ladder self-oscillates below RES 9"))return false;
  const double high=ring(.95f,9000.f);if(!require(high>=0&&high<1.e-3,"ladder whistles in the treble at full RES"))return false;
  if(!require(singing<.35,"ladder self-oscillation too loud"))return false;
  if(!require(singing>.05,"ladder does not self-oscillate at full RES"))return false;
  if(!require(damped>=0&&damped<1.e-3,"ladder keeps ringing at half RES"))return false;
  std::cout<<"  ladder self-oscillation: tail rms "<<juce::Decibels::gainToDecibels(singing)<<" dB at RES 10\n";}
 // SHAPE detents: a value close to a pure wave lands on it, one between waves is kept.
 {std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());for(auto* b:q->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(b))if(r->paramID=="vco1Wave"){
   const auto& range=r->getNormalisableRange();
   if(!require(std::abs(range.snapToLegalValue(.35f)-1.f/3.f)<1.e-6f&&std::abs(range.snapToLegalValue(.98f)-1.f)<1.e-6f&&std::abs(range.snapToLegalValue(.5f)-.5f)<1.e-6f,"SHAPE detents"))return false;}}
 // SEQUENCER playback: recorded chords and rests, moved by the lowest held key (relative to the first step's lowest note).
 {Arpeggiator::Sequence seq;const int a[]={60},b[]={64,67},c[]={62};seq.add(a,1);seq.add(b,2);seq.add(nullptr,0);seq.add(c,1);
  if(!require(seq.root==60&&seq.length==4&&seq.toString()=="60;64,67;r;62","sequence text form"))return false;
  {Arpeggiator::Sequence lanes;lanes.fromString("60,67o;64");const int g[]={55};lanes.setLane(0,g,1,true);
   if(!require(lanes.toString()=="55o,60;64","OVD line not replaced on its own: "+lanes.toString()))return false;
   const int h[]={62};lanes.setLane(0,h,1,false);if(!require(lanes.toString()=="55o,62;64"&&lanes.root==62,"SEQ line not replaced on its own: "+lanes.toString()))return false;
   auto merged=lanes;merged.mergeOverdub();lanes.clearOverdub();
   if(!require(lanes.toString()=="62;64"&&merged.toString()=="55,62;64","clear / merge overdub"))return false;}
  Arpeggiator::Sequence back;back.fromString(seq.toString());if(!require(back.toString()==seq.toString()&&back.root==60,"sequence does not read back"))return false;
  Arpeggiator arp;arp.prepare(48000);std::vector<std::vector<int>> steps;int last=-1;
  for(int blk=0;blk<200;++blk){juce::MidiBuffer m;if(blk==0)m.addEvent(juce::MidiMessage::noteOn(1,62,.8f),0);
   arp.process(m,512,true,120,2,4,1,.5f,-1.,&seq);
   for(const auto meta:m){const auto msg=meta.getMessage();if(!msg.isNoteOn())continue;const int at=blk*512+meta.samplePosition;if(at!=last){steps.push_back({});last=at;}steps.back().push_back(msg.getNoteNumber());}}
  const std::vector<std::vector<int>> expected{{62},{66,69},{64},{62},{66,69},{64}}; // the rest gives no note
  if(!require(steps.size()>=expected.size(),"sequencer plays too few steps"))return false;
  for(size_t k=0;k<expected.size();++k)if(!require(steps[k]==expected[k],"sequencer chords, rest or transposition wrong"))return false;
  // No key held: PLAY alone runs the sequence as recorded.
  Arpeggiator free;free.prepare(48000);std::vector<int> first;
  for(int blk=0;blk<40&&first.size()<2;++blk){juce::MidiBuffer m;free.process(m,512,true,120,2,4,1,.5f,-1.,&seq);for(const auto meta:m)if(meta.getMessage().isNoteOn())first.push_back(meta.getMessage().getNoteNumber());}
  if(!require(first.size()>=2&&first[0]==60&&first[1]==64,"sequencer does not run on its own"))return false;}
 // SEQUENCER recording through the plugin: REC, a note, a chord, REST, a note; the sequence is saved with the patch.
 {std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
  auto set=[&](const char* id,float v){for(auto* b:q->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(b))if(r->paramID==id)r->setValueNotifyingHost(v);};
  auto block=[&](std::initializer_list<juce::MidiMessage> msgs){juce::AudioBuffer<float> b(2,256);b.clear();juce::MidiBuffer m;int at=0;for(auto& msg:msgs)m.addEvent(msg,at++);q->processBlock(b,m);};
  set("seqRec",1);block({});
  block({juce::MidiMessage::noteOn(1,60,.8f)});block({juce::MidiMessage::noteOff(1,60)});
  block({juce::MidiMessage::noteOn(1,64,.8f),juce::MidiMessage::noteOn(1,67,.8f)});block({juce::MidiMessage::noteOff(1,67)});block({juce::MidiMessage::noteOff(1,64)});
  set("seqRest",1);block({});set("seqRest",0);block({});
  block({juce::MidiMessage::noteOn(1,62,.8f)});block({juce::MidiMessage::noteOff(1,62)});set("seqRec",0);block({});
  juce::MemoryBlock data;q->getStateInformation(data);auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
  if(!require(xml&&xml->getStringAttribute("sequence")=="60;64,67;r;62","sequencer recording wrong: "+(xml?xml->getStringAttribute("sequence"):juce::String())))return false;
  std::unique_ptr<juce::AudioProcessor> r(createPluginFilter());r->setStateInformation(data.getData(),int(data.getSize()));juce::MemoryBlock again;r->getStateInformation(again);
  auto xml2=juce::AudioProcessor::getXmlFromBinary(again.getData(),int(again.getSize()));if(!require(xml2&&xml2->getStringAttribute("sequence")=="60;64,67;r;62","sequence not restored with the session"))return false;
  // OVERDUB: REC while PLAY runs adds the key to the step being played, without clearing the sequence.
  set("seqOvd",1);for(int i=0;i<20;++i)block({});block({juce::MidiMessage::noteOn(1,50,.8f)});block({juce::MidiMessage::noteOff(1,50)});set("seqOvd",0);block({});
  juce::MemoryBlock dub;q->getStateInformation(dub);auto xml3=juce::AudioProcessor::getXmlFromBinary(dub.getData(),int(dub.getSize()));
  const auto text=xml3?xml3->getStringAttribute("sequence"):juce::String();
  if(!require(text.contains("50o")&&text.contains("64,67")&&juce::StringArray::fromTokens(text,";","").size()==4,"overdub wrong: "+text))return false;}
 // LIVE RECORDING: REC then PLAY on an empty sequence: a blank 16-step loop, each key on the nearest step.
 {std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
  auto set=[&](const char* id,float v){for(auto* b:q->getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(b))if(r->paramID==id)r->setValueNotifyingHost(r->convertTo0to1(v));};
  int blocks=0;auto block=[&](std::initializer_list<juce::MidiMessage> msgs){juce::AudioBuffer<float> b(2,256);b.clear();juce::MidiBuffer m;for(auto& msg:msgs)m.addEvent(msg,0);q->processBlock(b,m);++blocks;};
  set("arpSync",0);set("arpTempo",120);set("arpDivision",2);set("seqRec",1);block({});set("seqPlay",1);blocks=0; // a step: 6000 samples
  while(blocks<14)block({});block({juce::MidiMessage::noteOn(1,60,.8f)});block({juce::MidiMessage::noteOff(1,60)}); // 3584: late, step 2
  while(blocks<95)block({});block({juce::MidiMessage::noteOn(1,62,.8f)});block({juce::MidiMessage::noteOff(1,62)});  // 24320: early, step 5
  set("seqRec",0);set("seqPlay",0);block({});
  juce::MemoryBlock data;q->getStateInformation(data);auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
  const auto steps=juce::StringArray::fromTokens(xml?xml->getStringAttribute("sequence"):juce::String(),";","");
  if(!require(steps.size()==16&&steps[1]=="60"&&steps[4]=="62"&&steps[0]=="r","live recording wrong: "+steps.joinIntoString(";")))return false;}
 // PHASER: heard when on, transparent when off.
 {const auto off=plug({{"phaserOn",0},{"cutoff",8000}}),on=plug({{"phaserOn",1},{"phaserMix",1},{"cutoff",8000}});
  if(!require(diff(off,on)>.001,"phaser inactive"))return false;for(float v:on)if(!require(std::isfinite(v)&&std::abs(v)<4.f,"phaser unstable"))return false;}
 std::cout<<"PASS poly 8: continuous SHAPE, VCO mixer, POLY-MOD, ladder self-oscillation, sequencer, phaser\n";
 return true;
}
