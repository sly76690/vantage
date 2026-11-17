#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Three independent phase accumulators per voice. Both sections feed the same HPF/filter/VCA.
// Called at twice the output sample rate; the voice performs the final downsampling.
class OscillatorBank {
public:
 struct Settings {
  float wave=.35f, width=.5f, sub=.7f, noise=0, level=1;
  float blend=0, sourceMix=.5f, width1=.5f, width2=.5f;
  float fine=0, tune=0, crossMod=0, age=.3f;
  int shape1=0, shape2=0, range1=3, range2=3;
  bool sync=false, lowFrequency=false;
  // Vantage. MORPH shape (shape 6): one continuous wave control, triangle > saw > square > narrow pulse.
  float morph1=.33f, morph2=.33f;
  float vcoSub=0, vcoNoise=0;          // mixer of the VCO section: square one octave under VCO 1, white noise
  // POLY-MOD: the filter envelope and VCO 1 (each with its own signed amount) modulate VCO 2's frequency and/or
  // pulse width, voice by voice and at audio rate; the voice also sends the same signal to the filters.
  float polyEnv=0, polyOsc=0; bool polyFreq=true, polyWidth=false;
 };
 void setVoiceIndex(int index) {
  static constexpr float errors1[8]={.15f,-.55f,.8f,-.3f,.45f,-1.f,.65f,-.2f};
  static constexpr float errors2[8]={-.35f,.6f,-.1f,.9f,-.75f,.25f,-.5f,-.05f};
  error1=errors1[index&7];error2=errors2[index&7];
  driftRandom.setSeed(index*104729+5);noiseDco.setSeed(1001+index);
  noise1.setSeed(3001+index);noise2.setSeed(5001+index);
 }
 void prepare(double outputRate) {
  sr=outputRate*2.;follow=1.f-std::exp(-1.f/(.005f*float(sr)));
  driftFollow=1.f-std::exp(-1.f/(.6f*float(sr)));
  dco=vco1=vco2=subPhase=vcoSubPhase=0;poly=lastFirst=0;drift1=drift2=target1=target2=0;driftCount=0;
 }
 void configure(const Settings& next,bool fresh) {
  settings=next;
  range1=std::exp2(double(next.range1-3));
  range2=std::exp2(double(next.range2-3)+next.tune/12.+next.fine/1200.);
  if(fresh){morph1=next.morph1;morph2=next.morph2;vcoSub=next.vcoSub;vcoNoise=next.vcoNoise;blend=next.blend;sourceMix=next.sourceMix;level=next.level;
   sub=next.sub;noise=next.noise;width=next.width;width1=next.width1;width2=next.width2;waveMix=next.wave;}
 }
 // filterEnvelope: the voice's filter envelope (0..1), the POLY-MOD envelope source.
 float process(double frequency,float envelopeSemitones,float pwm,float vcoPwm=0,float filterEnvelope=0) {
  const auto smooth=[this](float& value,float target){value+=follow*(target-value);};
  smooth(blend,settings.blend);smooth(sourceMix,settings.sourceMix);smooth(level,settings.level);
  smooth(sub,settings.sub);smooth(noise,settings.noise);smooth(width,settings.width);
  smooth(width1,settings.width1);smooth(width2,settings.width2);smooth(waveMix,settings.wave);
  smooth(morph1,settings.morph1);smooth(morph2,settings.morph2);smooth(vcoSub,settings.vcoSub);smooth(vcoNoise,settings.vcoNoise);
  // VCO 1 feeds the POLY-MOD from its previous sample (it is computed after VCO 2, which it can cross-modulate).
  poly=settings.polyEnv*filterEnvelope+settings.polyOsc*lastFirst;
  if(--driftCount<=0){target1=driftRandom.nextFloat()*2-1;target2=driftRandom.nextFloat()*2-1;driftCount=int(sr*(.4+1.2*driftRandom.nextFloat()));}
  drift1+=driftFollow*(target1-drift1);drift2+=driftFollow*(target2-drift2);
  const double env=std::exp2(double(envelopeSemitones)/12.);
  // AGE is confined to the VCOs. DCO, envelopes, filters and LFO do not change with voice count.
  const double detune1=std::exp2(settings.age*(6.*error1+4.*drift1)/1200.);
  const double detune2=std::exp2(settings.age*(6.*error2+4.*drift2)/1200.);
  const double dd=step(frequency*env),d1=step(frequency*range1*detune1*(settings.sync?1.:env));
  // LOW FREQ is free of keyboard tracking: 2 Hz at 8', with RANGE/TUNE/FINE still available.
  const double d2=step((settings.lowFrequency?2.:frequency*env)*range2*detune2*(settings.polyFreq&&poly!=0.f?std::exp2(double(polyFreqOctaves*poly)):1.));
  const float pw2=juce::jlimit(.04f,.96f,width2+vcoPwm+(settings.polyWidth?polyWidthDepth*poly:0.f));
  const float second=settings.shape2==6?morph(vco2,d2,morph2,pw2-width2):oscillator(settings.shape2,vco2,d2,pw2,noise2);
  const double dx=juce::jlimit(1.e-9,.45,d1*(1.+1.5*settings.crossMod*second));
  const float first=settings.shape1==6?morph(vco1,dx,morph1,vcoPwm):oscillator(settings.shape1,vco1,dx,juce::jlimit(.08f,.92f,width1+vcoPwm),noise1);
  lastFirst=first;
  const float juno=.7f*level*wave(dco,dd,juce::jlimit(.08f,.92f,width+pwm),waveMix)
                   +.3f*sub*wave(subPhase,dd*.5,.5f,1.f)+noise*.2828f*(noiseDco.nextFloat()*2-1);
  float jupiter=.7f*((1.f-sourceMix)*first+sourceMix*second);
  if(vcoSub>0.f)jupiter+=.3f*vcoSub*wave(vcoSubPhase,dx*.5,.5f,1.f);
  if(vcoNoise>0.f)jupiter+=vcoNoise*.2828f*(noiseVco.nextFloat()*2-1);
  advance(vcoSubPhase,dx*.5);
  advance(dco,dd);advance(subPhase,dd*.5);
  vco1+=dx;
  if(vco1>=1.){vco1-=std::floor(vco1);if(settings.sync)vco2=vco1/dx*d2-d2;}
  advance(vco2,d2);
  // Linear crossfades keep endpoint gains constant and avoid a +3 dB bump for correlated waves.
  return (1.f-blend)*juno+blend*jupiter;
 }
public:
 // The POLY-MOD amount of the last sample, for the voice to send on to the filters.
 float polyMod() const {return poly;}
 static constexpr float polyFreqOctaves=4.f,polyWidthDepth=.42f;
private:
 // MORPH: 0 triangle, 1/3 saw, 2/3 square, 1 narrow pulse; crossfades between neighbours, band-limited edges.
 static float morph(double t,double dt,float m,float pwm){
  m=juce::jlimit(0.f,1.f,m);
  if(m<1.f/3.f){const float k=m*3.f;return (1.f-k)*triangle(t,dt)+k*wave(t,dt,.5f,0);}
  if(m<2.f/3.f){const float k=m*3.f-1.f;return wave(t,dt,juce::jlimit(.06f,.94f,.5f+pwm),k);}
  return wave(t,dt,juce::jlimit(.04f,.94f,.5f-.44f*(m*3.f-2.f)+pwm),1);
 }
 double step(double hz) const {return juce::jlimit(1.e-9,.225,hz/sr);}
 static void advance(double& phase,double delta){phase+=delta;phase-=std::floor(phase);}
 static double blep(double t,double dt){if(t<dt){t/=dt;return t+t-t*t-1;}if(t>1-dt){t=(t-1)/dt;return t*t+t+t+1;}return 0;}
 static float wave(double t,double dt,float width,float mix){
  const double saw=2*t-1-blep(t,dt),pulse=(t<width?1.:-1.)+blep(t,dt)-blep(std::fmod(t+1-width,1.),dt);
  return float(saw*(1-mix)+pulse*mix);
 }
 // Two-point integrated BLEP (polyBLAMP): round slope discontinuities at the
 // triangle's extrema without moving its phase or changing the oscillator clock.
 // See Esqueda, Valimaki & Bilbao, "Rounding Corners with BLAMP", DAFx-16.
 static float triangle(double t,double dt){
  const double raw=t<.25?4.*t:t<.75?2.-4.*t:4.*t-4.;
  const auto corner=[t,dt](double at){double distance=std::abs(t-at);distance=juce::jmin(distance,1.-distance);if(distance>=dt)return 0.;const double x=1.-distance/dt;return x*x*x;};
  return float(raw+(4.*dt/3.)*(corner(.75)-corner(.25)));
 }
 static float oscillator(int shape,double phase,double dt,float width,juce::Random& random){
  switch(shape){
   case 1:return wave(phase,dt,width,1);
   case 2:return triangle(phase,dt);
   case 3:return float(std::sin(phase*juce::MathConstants<double>::twoPi));
   case 4:return wave(phase,dt,.5f,1);
   case 5:return random.nextFloat()*2-1;
   default:return wave(phase,dt,width,0);
  }
 }
 Settings settings;
 double sr=96000,dco=0,subPhase=0,vco1=0,vco2=0,vcoSubPhase=0,range1=1,range2=1;
 float morph1=.33f,morph2=.33f,vcoSub=0,vcoNoise=0,poly=0,lastFirst=0;juce::Random noiseVco{7001};
 float follow=.004f,driftFollow=0,error1=0,error2=0,drift1=0,drift2=0,target1=0,target2=0;
 float blend=0,sourceMix=.5f,level=1,sub=.7f,noise=0,width=.5f,width1=.5f,width2=.5f,waveMix=.35f;
 int driftCount=0;juce::Random driftRandom{5},noiseDco{1001},noise1{3001},noise2{5001};
};
