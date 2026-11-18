#pragma once
// VANTAGE modulation: filter MODE and band-pass driven per sample, LFO tremolo and S&H, POLY-MOD to SHAPE 2.
static bool modulationTests(){
 using namespace juce;
 auto require=[](bool ok,const char* what){if(!ok)std::cerr<<"MODULATION: "<<what<<"\n";return ok;};
 auto lowGain=[](float offset,float band){poly8test::FilterSlot f;f.prepare(96000);f.configure(2,0,0.f);f.setMode(0.f,false);f.modulate(offset,band);
  double in=0,out=0;for(int i=0;i<96000;++i){const float x=.05f*std::sin(2*MathConstants<float>::pi*100.f*i/96000.f),y=f.process(x,1000.f);if(i>48000){in+=x*x;out+=double(y)*y;}}return 10*std::log10(out/in);};
 if(!require(lowGain(0,0)>-1&&lowGain(1,0)<-30,"MODE offset turns the low-pass into a high-pass"))return false;
 if(!require(lowGain(0,1)<-6,"band-pass amount takes the lows away"))return false;
 auto render=[](std::initializer_list<std::pair<const char*,float>> settings,std::vector<float>* blocks=nullptr){std::unique_ptr<AudioProcessor> p(createPluginFilter());p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
  for(auto [id,v]:std::initializer_list<std::pair<const char*,float>>{{"reverbOn",0},{"delayOn",0},{"chorusMode",0},{"age",0},{"sustain",1},{"env",0},{"cutoff",4000}})if(auto* r=dynamic_cast<RangedAudioParameter*>(parameterOf(*p,id)))r->setValueNotifyingHost(r->convertTo0to1(v));
  for(auto [id,v]:settings)if(auto* r=dynamic_cast<RangedAudioParameter*>(parameterOf(*p,id)))r->setValueNotifyingHost(r->convertTo0to1(v));
  AudioBuffer<float> b(2,256);AudioBuffer<float> all(1,256*200);for(int i=0;i<200;++i){MidiBuffer m;if(i==0)m.addEvent(MidiMessage::noteOn(1,57,.8f),0);p->processBlock(b,m);all.copyFrom(0,i*256,b,0,0,256);if(blocks&&i>20)blocks->push_back(b.getRMSLevel(0,0,256));}
  return all;};
 auto spread=[](const std::vector<float>& v){float lo=1e9f,hi=0;for(float x:v){lo=jmin(lo,x);hi=jmax(hi,x);}return Decibels::gainToDecibels(hi/jmax(lo,1e-9f));};
 std::vector<float> steady,trem,sh;render({{"rate",4}},&steady);render({{"rate",4},{"lfoAmp",1}},&trem);render({{"rate",6},{"lfoAmp",1},{"lfoShape",5}},&sh);
 if(!require(spread(steady)<1.5f&&spread(trem)>4,"LFO > AMP tremolo"))return false;
 if(!require(spread(sh)>2,"S&H LFO moves the level"))return false;
 const auto plain=render({{"polyEnv",1},{"env",.8f},{"oscLayer",1}}),shaped=render({{"polyEnv",1},{"env",.8f},{"oscLayer",1},{"polyShape",1}});
 double diff=0;for(int i=0;i<plain.getNumSamples();++i)diff+=std::abs(plain.getSample(0,i)-shaped.getSample(0,i));
 if(!require(diff/plain.getNumSamples()>.002,"POLY-MOD > SHAPE 2 changes VCO 2"))return false;
 std::cout<<"PASS modulation: MODE offset and band-pass blend, LFO tremolo and S&H, POLY-MOD to SHAPE 2 (tremolo "<<spread(trem)<<" dB)\n";return true;
}
