#pragma once
#include "OscillatorBank.h"
#include <unordered_map>
// A frozen copy of every parameter, read in place of the panel by a second sound (the sequence's own sound in LIVE).
struct ParamLayer {
 using Snapshot=std::array<float,512>;
 void build(AudioProcessorValueTreeState& s,const Array<AudioProcessorParameter*>& all){index.clear();int n=0;
  for(auto* base:all)if(auto* p=dynamic_cast<RangedAudioParameter*>(base))if(auto* a=s.getRawParameterValue(p->paramID)){jassert(n<int(Snapshot{}.size()));index[a]=n++;}
  values.reset(new std::atomic<float>[size_t(jmax(1,n))]);capture();}
 Snapshot panel() const {Snapshot result{};for(const auto& [a,i]:index)result[size_t(i)]=a->load();return result;}
 void capture(){apply(panel());} // one writer: the audio thread after construction
 void assign(Snapshot& data,const std::atomic<float>* a,float value) const {const auto it=index.find(a);if(it!=index.end())data[size_t(it->second)]=value;}
 float get(const std::atomic<float>* a) const {const auto it=index.find(a);return it==index.end()?a->load():values[size_t(it->second)].load();}
 float get(const Snapshot& data,const std::atomic<float>* a) const {const auto it=index.find(a);return it==index.end()?a->load():data[size_t(it->second)];}
 // Restore at a block boundary, never halfway through a voice. No allocation or wait on audio.
 void queue(const Snapshot& data,bool live){const SpinLock::ScopedLockType lock(pendingLock);pending=data;pendingLive=live;hasPending=true;}
 bool applyPending(bool& live){const SpinLock::ScopedTryLockType lock(pendingLock);if(!lock.isLocked()||!hasPending)return false;apply(pending);live=pendingLive;hasPending=false;return true;}
 Snapshot forSave(bool& queued,bool& live) const {
  {const SpinLock::ScopedLockType lock(pendingLock);queued=hasPending;live=pendingLive;if(queued)return pending;}
  // A session save may overlap LIVE activation; retry on the saving thread only.
  Snapshot result{};for(;;){const auto before=revision.load();if(before&1u)continue;
   for(const auto& entry:index)result[size_t(entry.second)]=values[size_t(entry.second)].load();
   if(before==revision.load())return result;}
 }
private:
 void apply(const Snapshot& data){revision.fetch_add(1);for(const auto& entry:index)values[size_t(entry.second)].store(data[size_t(entry.second)]);revision.fetch_add(1);}
 std::unordered_map<const std::atomic<float>*,int> index;std::unique_ptr<std::atomic<float>[]> values;
 std::atomic<unsigned> revision{0};mutable SpinLock pendingLock;Snapshot pending{};bool hasPending=false,pendingLive=false;
};
// Parameter objects and their atomics live as long as the processor. Cache their
// addresses once; automation still reads the same atomics at the same call sites.
// Fixed storage and literal-key hashing avoid APVTS tree/string lookups during audio.
class ParameterReadCache {
public:
 explicit ParameterReadCache(AudioProcessorValueTreeState& owner):state(owner){
  for(auto* base:state.processor.getParameters())if(auto* p=dynamic_cast<RangedAudioParameter*>(base)){
   size_t slot=hash(p->paramID.toRawUTF8());for(size_t probe=0;probe<entries.size();++probe,slot=(slot+1)&mask)if(entries[slot].raw==nullptr){entries[slot]={p->paramID,state.getRawParameterValue(p->paramID)};break;}
  }
 }
 const std::atomic<float>* get(const char* key) const {
  size_t slot=hash(key);for(size_t probe=0;probe<entries.size();++probe,slot=(slot+1)&mask){const auto& entry=entries[slot];if(entry.raw==nullptr)break;if(entry.id==key)return entry.raw;}
  return state.getRawParameterValue(key); // also handles a future parameter beyond the cache capacity
 }
private:
 static constexpr size_t mask=255;
 static constexpr size_t hash(const char* key){unsigned value=2166136261u;while(*key){value=(value^static_cast<unsigned char>(*key++))*16777619u;}return value&mask;}
 struct Entry{String id;const std::atomic<float>* raw=nullptr;};std::array<Entry,mask+1> entries{};AudioProcessorValueTreeState& state;
};
// Envelope with curved segments, shaped like the charge and discharge of a capacitor rather than
// straight lines. Attack bends towards a target above full level, so it rises fast then eases in and
// still arrives at full level in the set time. Decay and release fall quickly at first and then
// settle: at the set time they are within about 3 % of where they are heading.
// Same calls as juce::ADSR, which it replaces.
class Contour {
public:
 struct Parameters { float attack, decay, sustain, release; };
 void setSampleRate(double sr) { if(sampleRate!=sr)parametersValid=false;sampleRate=sr; }
 void setParameters(const Parameters& p) {
  if(parametersValid&&p.attack==previous.attack&&p.decay==previous.decay&&p.sustain==previous.sustain&&p.release==previous.release)return;
  previous=p;parametersValid=true;
  sustain=p.sustain;
  attackRate=rate(p.attack/std::log(overshoot/(overshoot-1.f)));
  decayRate=rate(p.decay/curve); releaseRate=rate(p.release/curve);
 }
 void reset() { level=0; stage=idle; }
 void noteOn() { stage=attack; }
 void noteOff() { if(stage!=idle) stage=release; }
 bool isActive() const { return stage!=idle; }
 float getNextSample() {
  switch(stage) {
   case attack: level+=(overshoot-level)*attackRate; if(level>=1.f){level=1.f;stage=decay;} break;
   case decay: level+=(sustain-level)*decayRate; break;                 // also holds, and follows, the sustain level
   case release: level-=level*releaseRate; if(level<silence){level=0;stage=idle;} break;
   case idle: break;
  }
  return level;
 }
private:
 enum Stage { idle, attack, decay, release };
 static constexpr float overshoot=1.3f, curve=3.5f, silence=.001f;
 float rate(float timeConstant) const { return 1.f-std::exp(-1.f/(jmax(.0001f,timeConstant)*float(sampleRate))); }
 Parameters previous{};bool parametersValid=false;
 double sampleRate=44100; float level=0,sustain=1,attackRate=1,decayRate=1,releaseRate=1; Stage stage=idle;
};

// One LFO for the whole instrument, rendered once per audio block by the processor so that
// every voice reads the same value at the same sample.
struct ModulationBus {
 std::vector<float> lfo,phase; int shape=0; double step=0; // value and phase per sample, waveform, cycles per sample
 bool manual=false; double manualAge=-1; // LFO MAN: samples since LFO TRIG was pressed, -1 while it is up
 static float wave(int shape,float t){if(shape==1)return std::sin(t*MathConstants<float>::twoPi);if(shape==2)return 2*t-1;if(shape==3)return t<.5f?1.f:-1.f;return t<.25f?4*t:(t<.75f?2-4*t:4*t-4);}
};

// Two independently selectable low-pass topologies; each slot owns its history.
// Cutoff and resonance are applied on the very sample they are given: nothing is smoothed inside
// the filter, so a fast filter envelope keeps its edge.
class FilterSlot {
public:
 void prepare(double sr) { sampleRate=float(sr);configured=false;lastLadderCutoff=lastSvfCutoff=-1.f; for(auto& f:svf){f.prepare({sr,512,1});f.setType(dsp::StateVariableTPTFilterType::lowpass);} reset(); }
 void reset() { ladder.fill(0.f); for(auto& f:svf)f.reset(); multi={}; }
 void setDrive(float amount) { drive=jmax(.2f,amount); } // how hard the ladder is pushed: more = earlier, rounder limiting
 void configure(int model,int slope,float resonance) {
  if(configured&&model==type&&slope==stages&&resonance==lastResonance)return;
  configured=true;lastResonance=resonance;lastLadderCutoff=lastSvfCutoff=-1.f;
  type=model; stages=slope; feedback=maxFeedback*jlimit(0.f,1.f,resonance/.95f); // RES fader: a gentle, even rise, matched by measurement to the reference recording (about +2.5 dB at 5, +8 dB at 10)
  // The last few steps of RES tip the ladder into self-oscillation, as on the Juno-106 (resonance "0 to self oscillation").
  if(const float t=jlimit(0.f,1.f,(resonance-.9f)/.05f);t>0.f)feedback+=selfOscillation*t*t*(3.f-2.f*t); for(auto& f:svf)f.setResonance(.7071f+resonance*7.f);
  // Ladder FREQ: without resonance the four poles sit an octave up (cutoffScale) so the ladder is as bright as the
  // state-variable model; as resonance comes in they move down so that the resonant peak sits on FREQ itself
  // (measured: with the poles left an octave up, the peak sang 1.6 to 2 times higher than the fader said).
  const float r=jlimit(0.f,1.f,resonance),k=jmin(1.f,r/.3f),blend=k*k*(3.f-2.f*k),onPeak=1.f+.35f*(1.f-r)*(1.f-r);
  ladderScale=cutoffScale*(1.f-blend)+onPeak*blend;
  damping=2.f*(1.f-.97f*jlimit(0.f,1.f,resonance)); } // MULTI: 2R from 2 (Q .5) down to .06 (Q about 16)
 // MULTI (Vantage): a 12 dB state-variable filter in the spirit of the classic semi-modular voice. MODE sweeps the
 // output continuously from low-pass through notch (low-pass plus high-pass) to high-pass; BP takes the band-pass.
 void setMode(float sweep,bool bandPass){mode=jlimit(0.f,1.f,sweep);band=bandPass;}
 float process(float x,float cutoff) {
  if(type==0) return processLadder(x,cutoff);
  if(type==2) return processMulti(x,cutoff);
  if(cutoff!=lastSvfCutoff){for(auto& f:svf)f.setCutoffFrequency(cutoff);lastSvfCutoff=cutoff;}
  x=svf[0].processSample(0,x);
  if(stages)x=svf[1].processSample(0,x);
  return std::tanh(x);
 }
private:
 // Four one-pole stages in series with resonance fed back from the last one, solved without a
 // one-sample delay in the loop. The 12 dB setting taps the signal after the second stage.
 // Voiced to stand next to the state-variable model: the same loudness, a cutoff that opens as far for the same
 // setting (four poles are already 12 dB down at their own frequency, hence cutoffScale), a resonance that comes in
 // early and sings at the top of its range, and the same soft output stage, so a strong peak growls instead of clipping.
 float processLadder(float x,float cutoff) {
  // Exact-value cache: never quantize cutoff or skip audio-rate modulation.
  // Usually the two oversampled substeps share a cutoff; Poly-Mod can invalidate it every sample.
  if(cutoff!=lastLadderCutoff){const float g=std::tan(MathConstants<float>::pi*jmin(cutoff*ladderScale,sampleRate*.45f)/sampleRate);cachedG=g/(1.f+g);lastLadderCutoff=cutoff;}
  const float G=cachedG,keep=1.f-G,G2=G*G;
  const float pending=keep*(G2*G*ladder[0]+G2*ladder[1]+G*ladder[2]+ladder[3]);
  // The signal enters at a low level so the saturation inside the loop only bites on the resonant peak (the
  // resonance builds up instead of being flattened); the lows the resonance takes away are given back at the output.
  // Self-oscillation (feedback above the calibrated maximum) is kept to the low and middle range: it fades out between
  // 2 and 6 kHz, where a whistle only hurts, and the sine it makes comes out well below a full saw.
  const float over=jmax(0.f,feedback-maxFeedback),sing=over>0.f?jlimit(0.f,1.f,(6000.f-cutoff)/4000.f):0.f;
  const float fb=over>0.f?maxFeedback+over*sing:feedback;
  float y=(x*inputLevel-fb*pending)/(1.f+fb*G2*G2);
  y=std::tanh(y*drive)/drive;
  float tap=0;
  for(size_t n=0;n<4;++n){const float v=(y-ladder[n])*G; y=v+ladder[n]; ladder[n]=y+v; if(n==1)tap=y;}
  // Output stage with headroom: it only rounds off real peaks, so a resonant peak and an envelope sweep stand out
  // above the rest of the sound instead of being flattened into it.
  return (1.f-selfOscillationCut*sing)*std::tanh((stages?y:tap)/inputLevel*outputLevel*(1.f+bassReturn*jmin(feedback,maxFeedback))*outputHeadroom)/outputHeadroom*.85f;
 }
 // One or two (24 dB) state-variable stages solved without delay in the loop (trapezoidal integrators). The band-pass
 // state is softly limited, so a high resonance thickens and rings rather than screaming; the output stage is the same
 // soft limit as the other models. Weights: low-pass full up to the middle of MODE, high-pass full from the middle,
 // both full in the middle (the notch), so the level holds across the sweep.
 float processMulti(float x,float cutoff){
  if(cutoff!=lastMultiCutoff){multiG=std::tan(MathConstants<float>::pi*jmin(cutoff,sampleRate*.45f)/sampleRate);lastMultiCutoff=cutoff;}
  const float g=multiG,k=damping,norm=1.f/(1.f+g*(g+k)),low=jmin(1.f,2.f*(1.f-mode)),high=jmin(1.f,2.f*mode);
  float y=x;
  for(int s=0;s<=stages;++s){auto& st=multi[size_t(s)];
   const float hp=(y-(k+g)*st[0]-st[1])*norm,bp=g*hp+st[0],lp=g*bp+st[1];
   st[0]=2.f*std::tanh(.5f*(g*hp+bp));st[1]=g*bp+lp;
   y=band?bp*3.2f*std::sqrt(k*.5f):low*lp+high*hp;} // band-pass: the peak rises with resonance, the level holds
  return std::tanh(y);
 }
 std::array<std::array<float,2>,2> multi{};float multiG=0.f,lastMultiCutoff=-1.f,damping=2.f,mode=0.f;bool band=false;
 bool configured=false;float lastResonance=-1.f,lastLadderCutoff=-1.f,lastSvfCutoff=-1.f,cachedG=0.f;
 std::array<float,4> ladder{}; std::array<dsp::StateVariableTPTFilter<float>,2> svf; float sampleRate=44100.f,feedback=.4f,drive=1.2f,ladderScale=cutoffScale; int type=0,stages=1;
 // Ladder voicing, measured against the state-variable model and to adjust by ear.
 // Vantage: the last steps of RES (above 9) push the loop past self-oscillation, so the ladder sings on its own as a sine
 // at FREQ; below that the response is unchanged from Orbit's calibrated curve.
 static constexpr float inputLevel=.2f,outputLevel=1.8f,bassReturn=.45f,maxFeedback=2.2f,selfOscillation=1.95f,selfOscillationCut=.8f,resonanceCurve=1.f,cutoffScale=2.f,outputHeadroom=.35f;
};
// What the voices of one instrument share for the glide: the pitch the most recent note is sounding at right now
// (in Hz, part-way through its own glide if it has one), and a count of note starts that tells which note that is.
struct GlideMemory { double pitch=0; uint64 starts=0; GlideMemory& operator=(double hz){pitch=hz;return *this;} };
// Back from twice the sample rate: a 31-tap half-band low-pass (Blackman-windowed sinc), evaluated once per pair.
struct HalfBand {
 static constexpr int taps=31;
 static const std::array<float,taps>& coefficients(){
  static const std::array<float,taps> h=[]{std::array<float,taps> c{};double sum=0;
   for(int n=0;n<taps;++n){const double m=n-(taps-1)/2.,sinc=m==0?.5:std::sin(MathConstants<double>::pi*.5*m)/(MathConstants<double>::pi*m);
    const double w=.42-.5*std::cos(MathConstants<double>::twoPi*n/(taps-1))+.08*std::cos(2*MathConstants<double>::twoPi*n/(taps-1));c[size_t(n)]=float(sinc*w);sum+=c[size_t(n)];}
   for(auto& v:c)v=float(v/sum);return c;}();
  return h;
 }
 void reset(){line.fill(0.f);pos=0;}
 float process(float first,float second){line[size_t(pos)]=first;pos=(pos+1)&63;line[size_t(pos)]=second;pos=(pos+1)&63;
  const auto& h=coefficients();float y=0;for(int n=0;n<taps;++n)y+=h[size_t(n)]*line[size_t((pos-1-n)&63)];return y;}
 std::array<float,64> line{};int pos=0;
};
class Voice : public SynthesiserVoice {
public:
 Voice(AudioProcessorValueTreeState& s,GlideMemory& previous,const ModulationBus& shared,const ParamLayer* frozen=nullptr) : state(s),parameterReads(s),previousPitch(previous),bus(shared),layer(frozen) {}
 bool canPlaySound(SynthesiserSound* s) override { return dynamic_cast<Sound*>(s)!=nullptr; }
 float p(const char* key) const {const auto* a=parameterReads.get(key);return layer!=nullptr?layer->get(a):a->load();}
 void setVoiceIndex(int index){oscillators.setVoiceIndex(index);
  // Fixed per-voice differences, balanced over the eight voices so that the instrument as a whole stays centred.
  static constexpr float cutoffs[8]={.2f,-.6f,.5f,-.9f,.75f,-.3f,.95f,-.6f},times[8]={-.4f,.7f,-.85f,.3f,.9f,-.6f,.15f,-.2f};
  static constexpr float lfoRates[8]={.5f,-.85f,.2f,.95f,-.35f,.7f,-.6f,-.55f};
  const int i=index&7;cutOffset=cutoffs[i];timeOffset=times[i];lfoTune=lfoRates[i];}
 // Eight-voice vintage character: each voice is a slightly different circuit (AGE sets how different: filter cutoff,
 // envelope times, LFO speed), the envelopes run faster up the keyboard and slower down it, and each note starts
 // with a few milliseconds of extra brightness and level.
 void setVintage(bool on){vintage=on;}
 void follow(int note){keyStretch=vintage?jlimit(.55f,1.6f,std::pow(keyFollow,float(note-60)/12.f)):1.f;}
 void setPan(float position){pan=jlimit(-1.f,1.f,position);} // -1 left, 0 centre, 1 right
 void envelopes(){const float stretch=vintage?(1.f+p("age")*envelopeError*timeOffset)*keyStretch:1.f;
  env.setParameters({p("attack")*stretch,p("decay")*stretch,p("sustain"),p("release")*stretch});filterEnv.setParameters({p("fattack")*stretch,p("fdecay")*stretch,p("fsustain"),p("frelease")*stretch});gate.setParameters({.002f,.002f,1.f,.006f});}
 void retarget(int note,float velocity,int wheel){follow(note);held=true;double current=pitch.getCurrentValue();hz=MidiMessage::getMidiNoteInHertz(note);pitch.reset(getSampleRate(),p("portamento"));pitch.setCurrentAndTargetValue(current);pitch.setTargetValue(std::log2(hz));serial=++previousPitch.starts;previousPitch.pitch=pitched?std::exp2(current):hz;vel=velocity;pitchWheelMoved(wheel);}
 void startNote(int note,float velocity,SynthesiserSound*,int wheel) override {
  // The glide runs on the pitch (log2 of the frequency), so an octave takes the same time wherever it is on the keyboard.
  // A new note glides from where the previous one is sounding at this instant, which this voice then keeps up to date
  // while it glides itself: a note played twice in a row, or a fast run, carries the glide on instead of cutting it short.
  follow(note);punch=vintage?1.f:0.f;
  double from=voiceGain<1&&pitched?std::exp2(pitch.getCurrentValue()):previousPitch.pitch;hz=MidiMessage::getMidiNoteInHertz(note);pitch.reset(getSampleRate(),p("portamento"));
  const double origin=p("portamento")>.001f&&from>0?from:hz;pitch.setCurrentAndTargetValue(std::log2(origin));pitch.setTargetValue(std::log2(hz));serial=++previousPitch.starts;previousPitch.pitch=origin;pitched=true;
  // A note taken over while it still sounds: what it was putting out fades over a few milliseconds instead of stopping dead.
  tailL+=lastL;tailR+=lastR;lastL=lastR=0;fresh=true;vel=velocity;pitchWheelMoved(wheel);
  a.reset();b.reset();highpass.reset();down.reset();bass=0;env.reset();filterEnv.reset();gate.reset();envelopes();env.noteOn();filterEnv.noteOn();gate.noteOn();lfoAge=0;pressure=0;lfoOffset=0;trigPhase=0;lfoLast=0;held=true;
 }
 void stopNote(float,bool tail) override {held=false;env.noteOff();filterEnv.noteOff();gate.noteOff();if(!tail){env.reset();filterEnv.reset();gate.reset();clearCurrentNote();}}
 void pitchWheelMoved(int value) override {wheelValue=value;}
 void controllerMoved(int cc,int value) override {if(cc==1)wheel=float(value)/127.f;}
 // Pressure: channel aftertouch, or polyphonic aftertouch on this voice's key. It opens the filters (PRESS) and,
 // like the mod wheel, brings in the vibrato.
 void channelPressureChanged(int value) override {pressure=float(value)/127.f;}
 void aftertouchChanged(int value) override {pressure=float(value)/127.f;}
 void setUnison(float cents,float amplitude){unisonCents=cents;voiceGain=amplitude;}
 void prepare(double sr){oscillators.prepare(sr);env.setSampleRate(sr);filterEnv.setSampleRate(sr);gate.setSampleRate(sr);highpass.prepare({2.*sr,1024,1});highpass.setType(dsp::StateVariableTPTFilterType::highpass);a.prepare(2.*sr);b.prepare(2.*sr);down.reset();} // the audio path runs at twice the rate
 void renderNextBlock(AudioBuffer<float>& out,int start,int count) override {
  if(!isVoiceActive()){lastL=lastR=tailL=tailR=0;return;} // silent since the last block: nothing left to fade
  envelopes();const double sr=getSampleRate();
  // HPF, one continuous fader: the bottom quarter boosts the bass (less and less towards the flat zone), a short flat
  // zone, then a high-pass rising from 40 Hz to 1 kHz.
  const float hpfPos=p("hpfFreq");const int hpfMode=hpfPos<.25f?0:(hpfPos<=.3f?1:2);const float boostGain=bassBoostGain*jlimit(0.f,1.f,(.25f-hpfPos)/.25f);
  if(hpfMode==2)highpass.setCutoffFrequency(40.f*std::pow(25.f,(hpfPos-.3f)/.7f));
  const float bassRate=1.f-std::exp(-MathConstants<float>::twoPi*bassBoostHz/float(2.*sr)); // runs at twice the rate (see below)
  a.configure(int(p("filterAType")),int(p("slope")),p("resonance"));b.configure(int(p("filterBType")),int(p("slopeB")),p("resonanceB"));
  a.setMode(p("filterMode"),p("filterBP")>.5f);b.setMode(0.f,false); // MULTI: MODE and BP on filter A; filter B's multimode is a low-pass
  const int route=int(p("filterRoute"));
  const float cutoff=p("cutoff"),cutoffB=p("cutoffB"),amount=p("env"),amountB=p("envB"),velocity=p("velocity"),pwmDepth=p("pwm"),vcoPwmDepth=p("vcoPwm"),blend=p("filterBlend"),pitchDepth=p("lfoPitch"),vibratoDepth=p("wheelDepth"),filterDepth=p("lfoFilter"),envPitch=p("envPitch");
  OscillatorBank::Settings source;
  source.wave=p("wave");source.width=p("width");source.sub=p("sub");source.noise=p("noise");source.level=p("osc1Level");
  source.blend=p("jupiterMix");source.sourceMix=p("oscLayer");source.width1=p("vco1Width");source.width2=p("osc2Width");
  source.fine=p("detune");source.tune=p("osc2Tune");source.crossMod=p("crossMod");source.age=p("age");
  source.shape1=int(p("osc1Shape"));source.shape2=int(p("osc2Shape"));source.range1=int(p("osc1Octave"));source.range2=int(p("osc2Octave"));
  source.sync=p("oscSync")>.5f;source.lowFrequency=p("vco2Low")>.5f;
  source.morph1=p("vco1Wave");source.morph2=p("vco2Wave");source.vcoSub=p("vcoSub");source.vcoNoise=p("vcoNoise");
  source.polyEnv=p("polyEnv");source.polyOsc=p("polyOsc");source.polyFreq=p("polyFreq")>.5f;source.polyWidth=p("polyWidth")>.5f;
  const bool polyFilter=p("polyFilter")>.5f;
  oscillators.configure(source,fresh);
  const bool linked=p("envLink")>.5f;
  // VCA mode GATE: the amplifier simply opens and closes with the key; the ADSR keeps running for a filter that follows it.
  // Filter A envelope can be inverted. PWM can come from the LFO, stay manual (WIDTH only), or follow the filter envelope.
  // VCA source: 0 the amp envelope, 1 the gate, 2 the filter envelope.
  const int vcaSource=int(p("vcaMode"));const bool gated=vcaSource==1;const float polarity=p("envInvert")>.5f?-1.f:1.f;const int pwmSource=int(p("pwmSource"));
  // The bender can also move both filter cutoffs, by up to the set number of octaves.
  const float bendCutoff=std::pow(2.f,float((wheelValue-8192)/8192.)*p("bendFilter"));
  const double bend=std::pow(2.,((wheelValue-8192)/8192.)*p("bendRange")/12.+unisonCents/1200.+p("masterTune")/1200.); // TUNE: the whole instrument, in cents
  // LFO delay: after a new note the LFO stays out for the first part of the delay time, then fades in.
  // The mod wheel vibrato is not delayed.
  const float lfoDelay=p("lfoDelay")*float(sr),lfoHold=lfoDelay*.6f,lfoRamp=jmax(1.f,lfoDelay*.4f);
  const float manualRate=1.f-std::exp(-1.f/(.015f*float(sr))); // LFO MAN: the LFO comes and goes over about 15 ms, never with a click
  const float tracking=std::pow(float(hz/261.625565),p("keytrack"));
  // VEL > FILTER: a softer note is darker, by up to four octaves; a full-velocity note is unchanged.
  const float velocityOpen=std::pow(2.f,p("velFilter")*4.f*(vel-1.f)),pressFilter=p("atFilter")*3.f,pressLfo=p("atLfo")*2.f,pressVolume=p("atVolume"),pressDrive=p("atDrive"),pressRate=1.f-std::exp(-1.f/(.01f*float(sr)));
  // KEY TRIG: this voice's LFO starts its cycle at each note. ENV TRIG: while the key is down, both envelopes start
  // again at each LFO cycle (rhythmic, gated sounds). The mod wheel can also open both filters (WHL>FLT).
  // Vintage voices run their own copy of the LFO at a slightly different speed (AGE), starting in step at each note
  // and drifting apart, so a held chord moves instead of wobbling as one block.
  const bool ownLfo=vintage&&p("age")>0.f;const double lfoDrift=bus.step*double(p("age")*lfoRateError*lfoTune);
  const bool keyTrig=p("lfoKeyTrig")>.5f,envTrig=p("envLfoTrig")>.5f;const double ownStep=bus.step+(ownLfo?lfoDrift:0.);
  const float cutSpread=vintage?std::pow(2.f,p("age")*cutoffError*cutOffset):1.f,punchDecay=std::exp(-1.f/(punchSeconds*float(sr)));
  const float wheelOpen=std::exp2(p("wheelFilter")*3.f*wheel);
  // PRE drive is shared by both source sections, independently of voice count.
  const float grit=p("voiceDrive"),push=1.f+3.f*grit,lean=.15f*grit,leanOffset=std::tanh(lean);
  // Quiet signals come up by (1 + PRE) only and peaks go down, so the voice keeps about the same loudness whatever PRE is set to.
  // Blended in over the first quarter of the knob so that turning it up from zero is seamless.
  const float pushScale=(1.f+grit)/push,driveMix=jmin(1.f,grit*4.f);
  const float dcRate=1.f-std::exp(-MathConstants<float>::twoPi*10.f/float(2.*sr));
  // Faders are read once per block: these follow them over a few milliseconds, so moving one while a note sounds
  // glides instead of stepping. A new note starts on the current settings at once.
  const float follow5ms=1.f-std::exp(-1.f/(.005f*float(sr))),tailDecay=std::exp(-1.f/(.004f*float(sr)));
  if(fresh){cutNow=cutoff;cutBNow=cutoffB;fresh=false;}
  // Equal-power placement; exactly 1 on both sides at the centre.
  const float angle=(pan+1.f)*MathConstants<float>::pi*.25f,gainL=pan==0.f?1.f:1.41421356f*std::cos(angle),gainR=pan==0.f?1.f:1.41421356f*std::sin(angle);
  for(int i=0;i<count;++i){
   float e=env.getNextSample(),fe=filterEnv.getNextSample();if(linked)fe=e;
   const float open=gate.getNextSample(),amp=gated?open:(vcaSource==2?fe:e);
   pressureNow+=pressRate*(pressure-pressureNow);
   float lfoNow=bus.lfo[size_t(start+i)],t=bus.phase[size_t(start+i)];
   if(keyTrig){trigPhase+=ownStep;trigPhase-=std::floor(trigPhase);t=float(trigPhase);lfoNow=ModulationBus::wave(bus.shape,t);}
   else if(ownLfo){lfoOffset+=lfoDrift;lfoOffset-=std::floor(lfoOffset);t=bus.phase[size_t(start+i)]+float(lfoOffset);t-=std::floor(t);lfoNow=ModulationBus::wave(bus.shape,t);}
   if(envTrig&&held&&t<lfoLast-.5f){env.noteOn();filterEnv.noteOn();} // the LFO has just started a new cycle
   lfoLast=t;
   float lfoFade=lfoDelay<1.f?1.f:jlimit(0.f,1.f,(lfoAge-lfoHold)/lfoRamp);
   if(bus.manual){ // LFO MAN: the delay runs from the press of LFO TRIG, not from the note
    const float since=bus.manualAge<0?-1.f:float(bus.manualAge)+float(i);
    const float target=since<0?0.f:(lfoDelay<1.f?1.f:jlimit(0.f,1.f,(since-lfoHold)/lfoRamp));
    manualLevel+=manualRate*(target-manualLevel);lfoFade=manualLevel;}
   const float modulation=lfoNow*lfoFade;
   if(lfoAge<1.e7f)lfoAge+=1.f;
   cutNow+=follow5ms*(cutoff-cutNow);cutBNow+=follow5ms*(cutoffB-cutBNow);
   const bool gliding=pitch.isSmoothing();const double pitchNow=pitch.getNextValue(),frequency=gliding?std::exp2(pitchNow):hz; // exactly the note once the glide is over
   if(serial==previousPitch.starts)previousPitch.pitch=frequency;
   const double frequencyNow=frequency*bend*std::exp2((modulation*pitchDepth+lfoNow*vibratoDepth*wheel)/12.);
   const float pwmSignal=pwmSource==0?.18f*modulation:pwmSource==2?.4f*fe:pwmSource==3?-.4f*fe:pwmSource==4?.4f*e:pwmSource==5?-.4f*e:0.f; // LFO, MAN, filter env +/-, amp env +/-
   punch*=punchDecay; // the percussive front of a vintage note
   const float caBase=jlimit(25.f,float(sr*.45),(1.f+punchCutoff*punch)*cutNow*cutSpread*tracking*velocityOpen*wheelOpen*std::exp2(pressFilter*pressureNow)*bendCutoff*std::pow(2.f,fe*amount*polarity*6+modulation*filterDepth+lfoNow*pressLfo*pressureNow));
   const float cbBase=jlimit(25.f,float(sr*.45),cutBNow*cutSpread*tracking*velocityOpen*wheelOpen*std::exp2(pressFilter*pressureNow)*bendCutoff*std::pow(2.f,fe*amountB*6+modulation*filterDepth+lfoNow*pressLfo*pressureNow));
   // Oscillators, mixer, filters and drive run at twice the sample rate and come back down through a half-band
   // filter to reduce aliasing from sync, cross modulation and saturation. This does not eliminate all aliasing.
   // Pressure can also push the drive (from any PRE setting, and in both voice counts), add LFO on the filters, and
   // lift the level by up to 6 dB: the harder the key is pressed, the more the note opens, moves, growls and grows.
   float g=grit,gPush=push,gLean=lean,gOffset=leanOffset,gScale=pushScale,gMix=driveMix;
   if(pressDrive>0.f&&pressureNow>.0005f){g=jmin(1.f,grit+pressDrive*pressureNow);gPush=1.f+3.f*g;gLean=.15f*g;gOffset=std::tanh(gLean);gScale=(1.f+g)/gPush;gMix=jmin(1.f,g*4.f);}
   float twice[2];
   for(int k=0;k<2;++k){
    float sample=oscillators.process(frequencyNow,fe*envPitch,pwmSignal*pwmDepth,pwmSignal*vcoPwmDepth,fe);
    // POLY-MOD > FILTER: the same signal moves both cutoffs, up to three octaves each way, at audio rate.
    const float polyOpen=polyFilter?std::exp2(polyFilterOctaves*oscillators.polyMod()):1.f;
    const float ca=jlimit(25.f,float(sr*.9),caBase*polyOpen),cb=jlimit(25.f,float(sr*.9),cbBase*polyOpen);
    if(hpfMode>=2)sample=highpass.processSample(0,sample);else if(hpfMode==0){bass+=bassRate*(sample-bass);sample+=boostGain*bass;}
    switch(route){case 1:sample=b.process(sample,cb);break;case 2:sample=b.process(a.process(sample,ca),cb);break;case 3:sample=a.process(b.process(sample,cb),ca);break;case 4:sample=(1-blend)*a.process(sample,ca)+blend*b.process(sample,cb);break;default:sample=a.process(sample,ca);}
    if(g>0.f){sample+=gMix*((std::tanh(gPush*sample+gLean)-gOffset)*gScale-sample);dc+=dcRate*(sample-dc);sample-=dc;} // the lopsided curve leaves a small offset: removed here
    twice[k]=sample;
   }
   float sample=down.process(twice[0],twice[1]);
   sample*=amp*(1-velocity+velocity*vel)*.18f*voiceGain*(1.f+punchLevel*punch)*(pressVolume>0.f?std::exp2(pressVolume*pressureNow):1.f);
   lastL=sample*gainL;lastR=sample*gainR;tailL*=tailDecay;tailR*=tailDecay;
   for(int ch=0;ch<out.getNumChannels();++ch)out.addSample(ch,start+i,ch==0?lastL+tailL:(ch==1?lastR+tailR:sample));
   if(!(gated?gate.isActive():(vcaSource==2&&!linked?filterEnv.isActive():env.isActive()))){clearCurrentNote();break;}
  }
 }
private:
 AudioProcessorValueTreeState& state;ParameterReadCache parameterReads;GlideMemory& previousPitch;uint64 serial=0;const ModulationBus& bus;const ParamLayer* layer=nullptr;SmoothedValue<double> pitch;Contour env,filterEnv,gate;FilterSlot a,b;HalfBand down;OscillatorBank oscillators;dsp::StateVariableTPTFilter<float> highpass;
 float cutNow=0,cutBNow=0,lastL=0,lastR=0,tailL=0,tailR=0;bool fresh=true,pitched=false;
 double hz=440;float vel=0,wheel=0,unisonCents=0,voiceGain=1,lfoAge=0,bass=0,pan=0,dc=0;
 float pressure=0,pressureNow=0,lfoLast=0,manualLevel=0;double trigPhase=0,lfoOffset=0;bool held=false;
 bool vintage=false;float cutOffset=0,timeOffset=0,lfoTune=0,keyStretch=1,punch=0;
 // Vintage character, to adjust by ear: envelope time ratio per octave, punch length and its share of cutoff and level,
 // and the spread between voices at AGE = 10 (cutoff in octaves, envelope times, LFO speed).
 static constexpr float keyFollow=.85f,punchSeconds=.006f,punchCutoff=.4f,punchLevel=.25f;
 static constexpr float cutoffError=.12f,envelopeError=.15f,lfoRateError=.12f;
 // Tuning values for the high-pass switch: starting points to adjust by ear, not hardware measurements.
 static constexpr float bassBoostHz=90.f,bassBoostGain=1.2f,polyFilterOctaves=3.f;int wheelValue=8192;
};
