#pragma once
// VANTAGE multimode filter: low-pass, notch, high-pass and band-pass responses, resonance, stability.
static bool multimodeTests(){
 auto gain=[](float mode,bool band,float res,int slope,float hz){poly8test::FilterSlot f;f.prepare(96000);f.configure(2,slope,res);f.setMode(mode,band);
  double in=0,out=0;for(int i=0;i<96000;++i){const float x=.05f*std::sin(2*juce::MathConstants<float>::pi*hz*i/96000.f);const float y=f.process(x,1000.f);
   if(!std::isfinite(y))return -999.;if(i>=48000){in+=x*x;out+=double(y)*y;}}return 10*std::log10(out/in);};
 auto require=[](bool ok,const char* what,double v){if(!ok)std::cerr<<"MULTIMODE: "<<what<<" ("<<v<<" dB)\n";return ok;};
 const double lpLow=gain(0,false,0,0,100),lpHigh=gain(0,false,0,0,8000),hpLow=gain(1,false,0,0,100),hpHigh=gain(1,false,0,0,8000);
 const double notchLow=gain(.5f,false,0,0,100),notchMid=gain(.5f,false,0,0,1000),notchHigh=gain(.5f,false,0,0,8000);
 const double bpLow=gain(0,true,.5f,0,100),bpMid=gain(0,true,.5f,0,1000),bpHigh=gain(0,true,.5f,0,8000); // band-pass at half resonance
 const double flatPeak=gain(0,false,0,0,1000),resPeak=gain(0,false,.95f,0,1000),steep=gain(0,false,0,1,8000),loud=gain(0,true,1,1,1000);
 if(!require(std::abs(lpLow)<1&&lpHigh<-30,"low-pass",lpHigh))return false;
 if(!require(std::abs(hpHigh)<1&&hpLow<-30,"high-pass",hpLow))return false;
 if(!require(notchMid<notchLow-20&&notchMid<notchHigh-20,"notch",notchMid))return false;
 if(!require(bpMid>bpLow+15&&bpMid>bpHigh+15,"band-pass",bpMid))return false;
 if(!require(resPeak>flatPeak+12,"resonance peak",resPeak-flatPeak))return false;
 if(!require(steep<lpHigh-25,"24 dB slope",steep))return false;
 if(!require(loud>-999,"stability at full resonance",loud))return false;
 std::cout<<"PASS multimode filter: LP "<<lpHigh<<" dB at 8 kHz, HP "<<hpLow<<" dB at 100 Hz, notch "<<notchMid<<" dB, band-pass, resonance +"<<resPeak-flatPeak<<" dB, 24 dB slope\n";
 return true;
}
