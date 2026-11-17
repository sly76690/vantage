#pragma once
#include "../Source/OutputDrive.h"

static bool dspRegressionTests(){
 using namespace juce;
 auto require=[](bool ok,const char* what){if(!ok)std::cerr<<"DSP regression: "<<what<<"\n";return ok;};
 // Coherent FFT after the real 2x decimator: reject every bin except intended odd harmonics.
 // The comparison uses the previous analytic triangle at the same phase and sample rate.
 for(int bin:{2053,4789,7517}){
  constexpr int n=32768;const double hz=48000.*bin/n,dt=hz/96000.;
  OscillatorBank bank;OscillatorBank::Settings settings;settings.blend=1;settings.sourceMix=0;settings.shape1=2;settings.age=0;
  bank.setVoiceIndex(0);bank.prepare(48000);bank.configure(settings,true);
  poly8test::HalfBand oldDown,newDown;oldDown.reset();newDown.reset();double phase=0;
  std::vector<float> before(size_t(n)*2,0.f),after(size_t(n)*2,0.f);
  for(int i=-64;i<n;++i){float oldPair[2],newPair[2];for(int k=0;k<2;++k){
    oldPair[k]=.7f*float(2./MathConstants<double>::pi*std::asin(std::sin(phase*MathConstants<double>::twoPi)));
    newPair[k]=bank.process(hz,0,0);phase+=dt;phase-=std::floor(phase);
   }const float a=oldDown.process(oldPair[0],oldPair[1]),b=newDown.process(newPair[0],newPair[1]);if(i>=0){before[size_t(i)]=a;after[size_t(i)]=b;}}
  dsp::FFT fft(15);fft.performFrequencyOnlyForwardTransform(before.data());fft.performFrequencyOnlyForwardTransform(after.data());
  std::vector<bool> wanted(n/2+1,false);for(int k=1;k*bin<n/2;k+=2)wanted[size_t(k*bin)]=true;
  double oldAlias=0,newAlias=0;for(int k=1;k<n/2;++k)if(!wanted[size_t(k)]){oldAlias+=double(before[size_t(k)])*before[size_t(k)];newAlias+=double(after[size_t(k)])*after[size_t(k)];}
  const double improvement=10.*std::log10(oldAlias/newAlias),fundamental=20.*std::log10(after[size_t(bin)]/before[size_t(bin)]);
  std::cout<<"DSP triangle "<<hz<<" Hz: alias reduction "<<improvement<<" dB; fundamental change "<<fundamental<<" dB\n";
  if(!require(improvement>6.,"triangle alias reduction below 6 dB")||!require(std::abs(fundamental)<.6,"triangle fundamental changed excessively"))return false;
 }
 // Static transfer curve stays identical; bypass stays sample-exact, including large host blocks.
 for(float amount:{0.f,.3f,1.f}){
  OutputDrive drive;drive.prepare(48000,64,amount);
  dsp::Oversampling<float> reference(2,1,dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,false);reference.initProcessing(64);
  for(int block=0;block<20;++block){AudioBuffer<float> a(2,64),b(2,64);
   for(int c=0;c<2;++c)for(int i=0;i<64;++i){const float x=.8f*std::sin(float(block*64+i)*.13f+float(c)*.3f);a.setSample(c,i,x);b.setSample(c,i,x);}
   drive.process(a,amount);
   if(amount>0){dsp::AudioBlock<float> chunk(b);auto up=reference.processSamplesUp(chunk);for(size_t c=0;c<2;++c)for(size_t i=0;i<up.getNumSamples();++i){auto& x=up.getChannelPointer(c)[i];x=std::tanh(x*(1+amount*9))/(1+amount*2);}reference.processSamplesDown(chunk);}
   for(int c=0;c<2;++c)for(int i=0;i<64;++i)if(!require(std::abs(a.getSample(c,i)-b.getSample(c,i))<1.e-6f,"POST static curve or bypass changed"))return false;
  }
 }
 auto automated=[](int blockSize){OutputDrive drive;drive.prepare(48000,64,0);std::vector<float> result;result.reserve(2400);
  for(int start=0;start<2400;){const int edge=start<800?800:start<1600?1600:2400;const int length=std::min(blockSize,edge-start);AudioBuffer<float> b(2,length);
   for(int c=0;c<2;++c)for(int i=0;i<length;++i)b.setSample(c,i,.4f);drive.process(b,start>=800&&start<1600?1.f:0.f);
   result.insert(result.end(),b.getReadPointer(0),b.getReadPointer(0)+length);start+=length;}return result;};
 auto small=automated(17),large=automated(1024);double peakStep=0;
 for(size_t i=0;i<small.size();++i){if(!require(std::abs(small[i]-large[i])<1.e-6f,"POST automation depends on host block size"))return false;if(i>1)peakStep=std::max(peakStep,double(std::abs(small[i]-small[i-1])));}
 if(!require(peakStep<.005,"POST toggle clicks")||!require(small.back()==.4f,"POST bypass not restored exactly"))return false;
 std::cout<<"PASS DSP: triangle alias rejection, POST static equivalence, click-free automation and oversized blocks\n";
 return true;
}
