#pragma once
#include <juce_dsp/juce_dsp.h>

// Existing POST transfer curve, with a warm oversampler and sample-timed controls.
// At zero the dry path stays bit-identical. The short bypass crossfade prevents
// the oversampler's delay/history from appearing or disappearing in one sample.
class OutputDrive {
public:
 void prepare(double sampleRate,int maximumBlock,float amount){
  blockSize=juce::jmax(1,maximumBlock);wet.setSize(2,blockSize);
  oversampler.initProcessing(size_t(blockSize));drive.reset(sampleRate*2.,.01);mix.reset(sampleRate,.01);reset(amount);
 }
 void reset(float amount){oversampler.reset();drive.setCurrentAndTargetValue(amount);mix.setCurrentAndTargetValue(amount>0.f?1.f:0.f);}
 void process(juce::AudioBuffer<float>& audio,float amount){
  if(audio.getNumChannels()<2)return;
  drive.setTargetValue(juce::jlimit(0.f,1.f,amount));mix.setTargetValue(amount>0.f?1.f:0.f);
  juce::dsp::AudioBlock<float> block(audio.getArrayOfWritePointers(),2,size_t(audio.getNumSamples()));
  for(size_t done=0;done<block.getNumSamples();done+=size_t(blockSize)){
   const auto length=juce::jmin(size_t(blockSize),block.getNumSamples()-done);
   auto dry=block.getSubBlock(done,length);auto up=oversampler.processSamplesUp(dry);
   if(drive.isSmoothing()||drive.getCurrentValue()>0.f||mix.getCurrentValue()>0.f)
   for(size_t i=0;i<up.getNumSamples();++i){const float d=drive.getNextValue();
    for(size_t c=0;c<2;++c){auto& x=up.getChannelPointer(c)[i];x=std::tanh(x*(1+d*9))/(1+d*2);}}
   juce::dsp::AudioBlock<float> wetBlock(wet.getArrayOfWritePointers(),2,length);oversampler.processSamplesDown(wetBlock);
   for(size_t i=0;i<length;++i){const float blend=mix.getNextValue();for(size_t c=0;c<2;++c){auto& x=dry.getChannelPointer(c)[i];if(blend>=1.f)x=wetBlock.getChannelPointer(c)[i];else if(blend>0.f)x+=blend*(wetBlock.getChannelPointer(c)[i]-x);}}
  }
 }
private:
 juce::dsp::Oversampling<float> oversampler{2,1,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,false};
 juce::AudioBuffer<float> wet;juce::SmoothedValue<float> drive,mix;int blockSize=1;
};
