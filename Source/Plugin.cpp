#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <optional>

using namespace juce;
#include "Arpeggiator.h"
#include "PresetStorage.h"
#include "EnsembleChorus.h"
#include "Reverbs.h"
#include "CharacterDelay.h"
#include "OutputDrive.h"
#include "NoteHold.h"
#include "ForceTouch.h"
class Orbit;
struct Sound : SynthesiserSound {
 bool appliesToNote(int) override { return true; }
 bool appliesToChannel(int) override { return true; }
};
#include "Voice.h"
#include "VoiceAllocator.h"
class Orbit : public AudioProcessor, private AudioProcessorValueTreeState::Listener, private AudioProcessorListener {
 private:
 struct Engine; // one sound: voices, LFO, effects (see below)
 public:
 AudioProcessorValueTreeState state;
 MidiKeyboardState keyboard;
 Orbit() : AudioProcessor(BusesProperties().withOutput("Stereo",AudioChannelSet::stereo(),true)), state(*this,nullptr,"OrbitState",parameters()), main(state,nullptr), sequenceSound(state,&sequenceLayer) {
  sequenceLayer.build(state,getParameters());
  recordHeld.ensureStorageAllocated(2048);recordChord.ensureStorageAllocated(128); // no allocation on the audio thread while recording
  for(auto* param:getParameters()) if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param)) state.addParameterListener(identified->paramID,this);
  state.state.setProperty("stateVersion",5,nullptr);
  for(auto& slot:ccTarget)slot.store(-1);addListener(this);
 }
 ~Orbit() override {removeListener(this);for(auto* param:getParameters()) if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param)) state.removeParameterListener(identified->paramID,this);}
 String userPresetName() const {return state.state.getProperty("userPresetName").toString();}
 // User presets live in the user's library, so they survive moving or rebuilding the project.
 static File presetRoot(){return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Vantage");}
 StringArray userPresetNames() const {return PresetStorage(presetRoot()).names();}
 // Sounds: complete presets shipped by the installer (shared library), plus any the user drops in their own Presets/Sounds.
 static File sharedRoot(){return File::getSpecialLocation(File::commonApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Vantage");}
 String soundName() const {return state.state.getProperty("soundName").toString();}
 StringArray soundNames() const {auto names=PresetStorage(sharedRoot(),"Sounds").names();names.mergeArray(PresetStorage(presetRoot(),"Sounds").names());names.sort(true);return names;}
 Result loadSound(const String& name){auto xml=PresetStorage(presetRoot(),"Sounds").load(name);if(!xml)xml=PresetStorage(sharedRoot(),"Sounds").load(name);if(!xml)return Result::fail("Son absent ou invalide dans Presets/Sounds.");
  MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);state.state.removeProperty("userPresetName",nullptr);state.state.setProperty("soundName",name,nullptr);modified.store(false);return Result::ok();}
 bool userPresetExists(const String& name) const {return PresetStorage(presetRoot()).exists(name);}
 Result saveUserPreset(const String& name,bool replace=false){auto saved=state.copyState();saved.setProperty("sequence",sequenceText(),nullptr);saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",false,nullptr);saved.setProperty("userPresetName",name,nullptr);saved.removeProperty("soundName",nullptr);auto xml=saved.createXml();auto result=PresetStorage(presetRoot()).save(name,*xml,replace);if(result.wasOk()){state.state.setProperty("userPresetName",name,nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}return result;}
 Result loadUserPreset(const String& name){auto xml=PresetStorage(presetRoot()).load(name);if(!xml)return Result::fail("Preset absent, invalide ou inaccessible dans Presets/User.");MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);state.state.removeProperty("soundName",nullptr);state.state.setProperty("userPresetName",name,nullptr);modified.store(false);return Result::ok();}
 bool isPatchModified() const {return modified.load();}
 String sequenceText(){return sequenceCopy().toString();}
 // SEQUENCE MEMORIES: eight slots kept apart from the sounds (Presets/Sequences/SEQ n.txt), so a sequence can be
 // played with any patch. The current slot is shown on the panel; a new recording or a patch's own sequence clears it.
 static constexpr int sequenceSlots=8;
 static File sequenceFile(int slot){return presetRoot().getChildFile("Presets").getChildFile("Sequences").getChildFile("SEQ "+String(slot+1)+".txt");}
 bool sequenceSlotUsed(int slot) const {return sequenceFile(slot).existsAsFile();}
 int sequenceSlotLength(int slot) const {Arpeggiator::Sequence probe;probe.fromString(sequenceFile(slot).loadFileAsString());return probe.length;}
 bool storeSequence(int slot){if(!isPositiveAndBelow(slot,sequenceSlots)||sequenceLength()==0)return false;const auto file=sequenceFile(slot);file.getParentDirectory().createDirectory();
  if(!file.replaceWithText(sequenceText()))return false;currentSequenceSlot.store(slot);return true;}
 bool loadSequence(int slot){if(!isPositiveAndBelow(slot,sequenceSlots)||!sequenceSlotUsed(slot))return false;const auto text=sequenceFile(slot).loadFileAsString();
  Arpeggiator::Sequence loaded;loaded.fromString(text);{const SpinLock::ScopedLockType lock(sequenceLock);sequence=loaded;sequenceSteps.store(sequence.length);}currentSequenceSlot.store(slot);return true;}
 int sequenceSlot() const {return currentSequenceSlot.load();}
 int sequencePosition() const {return arp.position.load();}
 // Step editing from the panel (message thread). editStep: the selected step, -1 when the editor is closed.
 std::atomic<int> editStep{-1};std::atomic<bool> editAdds{false},editLane{false}; // editLane: false = SEQ (recorded) line, true = OVD line
 Arpeggiator::Sequence sequenceCopy(){const SpinLock::ScopedLockType lock(sequenceLock);return sequence;}
 void editSequence(const std::function<void(Arpeggiator::Sequence&)>& change){{const SpinLock::ScopedLockType lock(sequenceLock);change(sequence);sequenceSteps.store(sequence.length);}currentSequenceSlot.store(-1);}
 std::atomic<int> currentSequenceSlot{-1};
 int sequenceLength() const {return sequenceSteps.load();}
 std::atomic<int> sequenceSteps{0};
  // Host pages of eight, in the order of the front panel.
  static constexpr const char* hostPages[]={
   "polyEnv","polyOsc","polyFreq","polyWidth","polyFilter","oscLayer","vcoSub","vcoNoise",      // POLY-MOD, MIXER
   "vco1Wave","osc1Octave","vco1Width","crossMod","vco2Wave","osc2Octave","osc2Width","detune",   // OSCILLATORS
   "cutoff","resonance","env","keytrack","lfoFilter","hpfFreq","slope","filterAType",             // FILTER A
   "attack","decay","sustain","release","velocity","vcaMode","portamento","legato",              // AMP
   "fattack","fdecay","fsustain","frelease","envLink","envInvert","envPitch","pwmSource",         // FILTER ENVELOPE
   "rate","lfoSync","lfoDelay","lfoShape","lfoKeyTrig","envLfoTrig","lfoPitch","wheelDepth",      // LFO
   "wheelFilter","velFilter","atFilter","bendRange","bendFilter","unisonSpread","masterTune","gain",// EXPRESSION, TUNE, VOLUME
   "filterBType","cutoffB","resonanceB","envB","slopeB","filterRoute","filterBlend","voiceDrive",  // FILTER B, ROUTING
   "mode","voiceMode","age","voiceWidth","hold","chorusMode","chorus","fxDrive",                  // VOICES, CHORUS
   "arpOn","arpSync","arpPattern","arpDivision","arpOctaves","arpTempo","arpGate","delayOn",      // ARPEGGIO
   "delayTime","delaySync","delayFeedback","delayMod","delayMix","reverbOn","reverbType","reverbSize", // DELAY, REVERB
   "reverbDamp","reverbColor","reverbMix","lfoManual","lfoTrig","atVolume","atLfo","atDrive",
   "osc2Tune","oscSync","vco2Low","vcoPwm",
   "phaserOn","phaserRate","phaserDepth","phaserFeedback","phaserMix","seqRec","seqRest","seqPlay",  // PHASER, SEQUENCER
   "seqOvd","seqLive",
   "osc1Shape","osc2Shape","wave","width","pwm","sub","noise","osc1Level","jupiterMix"}; // fixed shapes and the DCO, kept for old patches                                        // REVERB, EXPRESSION
  // Short names for the host and controller displays (Push shows about a dozen characters).
  static String label(const String& id,const String& longName){
   static const std::map<String,String> names{
    {"jupiterMix","DCO/VCO"},{"phaserOn","Phaser On"},{"phaserRate","Ph Rate"},{"phaserDepth","Ph Depth"},{"phaserFeedback","Ph Fdbk"},{"phaserMix","Ph Mix"},{"vco1Wave","VCO1 Shape"},{"vco2Wave","VCO2 Shape"},{"vcoSub","VCO Sub"},{"vcoNoise","VCO Noise"},{"polyEnv","PM Env"},{"polyOsc","PM Osc1"},{"polyFreq","PM>Freq2"},{"polyWidth","PM>PW2"},{"polyFilter","PM>Filter"},{"vcoPwm","VCO PWM"},{"vco1Width","VCO1 Width"},{"vco2Low","VCO2 Low"},{"wave","DCO SawPls"},{"width","DCO Width"},{"pwm","DCO PWM"},{"sub","Sub Level"},{"noise","Noise"},{"osc1Shape","VCO1 Wave"},{"osc1Octave","VCO1 Range"},{"osc1Level","DCO Level"},
    {"oscLayer","Source Mix"},{"osc2Width","VCO2 Width"},{"osc2Tune","VCO2 Tune"},{"detune","VCO2 Fine"},{"osc2Shape","VCO2 Wave"},{"osc2Octave","VCO2 Range"},{"oscSync","VCO2 Sync"},{"crossMod","X-Mod"},
    {"cutoff","A Freq"},{"resonance","A Reso"},{"env","A Env"},{"keytrack","Filter Kybd"},{"lfoFilter","Filter LFO"},{"hpfFreq","HPF"},{"slope","A Slope"},{"filterAType","A Model"},
    {"attack","Amp Attack"},{"decay","Amp Decay"},{"sustain","Amp Sustain"},{"release","Amp Release"},{"velocity","Velocity"},{"vcaMode","VCA Mode"},{"portamento","Portamento"},{"legato","Legato"},
    {"fattack","Flt Attack"},{"fdecay","Flt Decay"},{"fsustain","Flt Sustain"},{"frelease","Flt Release"},{"envLink","Flt Env Src"},{"envInvert","A Env Pol"},{"envPitch","Env>Pitch"},{"pwmSource","PWM Source"},
    {"rate","LFO Rate"},{"lfoDelay","LFO Delay"},{"lfoShape","LFO Wave"},{"lfoPitch","LFO Pitch"},{"wheelDepth","Wheel Vib"},{"bendRange","Bend Range"},{"bendFilter","Bend>Filter"},{"unisonSpread","Uni Spread"},
    {"filterBType","B Model"},{"cutoffB","B Freq"},{"resonanceB","B Reso"},{"envB","B Env"},{"slopeB","B Slope"},{"filterRoute","Routing"},{"filterBlend","A/B Mix"},{"voiceDrive","Drive Pre"},
    {"mode","Voices"},{"voiceMode","Assign"},{"age","Age"},{"voiceWidth","Width"},{"hold","Hold"},{"chorusMode","Chorus"},{"chorus","Chorus Mix"},{"gain","Volume"},
    {"arpOn","Arp On"},{"arpSync","Arp Sync"},{"arpPattern","Arp Mode"},{"arpDivision","Arp Step"},{"arpOctaves","Arp Range"},{"arpTempo","Arp Tempo"},{"arpGate","Arp Gate"},{"fxDrive","Drive Post"},
    {"delayTime","Dly Time"},{"delaySync","Dly Sync"},{"delayFeedback","Dly Fdbk"},{"delayMod","Dly Mod"},{"delayMix","Dly Level"},{"reverbType","Rev Type"},{"reverbSize","Rev Size"},{"reverbDamp","Rev Damp"},
    {"reverbColor","Rev Color"},{"reverbMix","Rev Level"},{"velFilter","Vel>Filter"},{"atFilter","Press>Filt"},{"delayOn","Dly On"},{"reverbOn","Rev On"},{"lfoSync","LFO Sync"},{"lfoKeyTrig","LFO KeyTrig"},{"envLfoTrig","Env LFO Trig"},{"wheelFilter","Wheel>Filt"},{"lfoManual","LFO Manual"},{"atVolume","Press>Vol"},{"atLfo","Press>LFO"},{"atDrive","Press>Drive"},{"lfoTrig","LFO Trig"},{"masterTune","Master Tune"}};
   const auto found=names.find(id);return found!=names.end()?found->second:longName;
  }
 static AudioProcessorValueTreeState::ParameterLayout parameters() {
   // Parameters are made in any order, then handed to the host page by page (see hostPages): a controller that shows
   // eight at a time, such as Push, walks through the instrument section by section.
   // Version 5 deliberately changes the oscillator architecture; old sound identity is not preserved.
   std::vector<std::unique_ptr<RangedAudioParameter>> made;
   auto keep=[&made](auto parameter){made.push_back(std::move(parameter));};
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"mode",1},label("mode","Voices"),StringArray{"6 voices","8 voices"},0));
  // Values are shown on the scale printed around the control: the eleven marks read 0 to 10 (or -5 to +5 for a
  // control centred on zero), so mark 3 always displays 3.0. Controls with a real unit add it in brackets,
  // for example "4.0 (120 BPM)" or "6.3 (1800 Hz)".
  auto add=[&](const char* id,const char* name,float lo,float hi,float def,float skew=1.f) {
   // Filter A FREQ: the same number of octaves for every mark of the fader, as on the Juno (a slider voltage into an
   // exponential converter). With a power curve the top half of the travel held 1.8 to 18 kHz, where little changes.
   const bool octaves=String(id)=="cutoff";
   // SHAPE: a detent on each pure wave (triangle 0, saw 1/3, square 2/3, pulse 1), 3 % of the travel wide each side,
   // felt while turning the knob and kept in automation.
   const bool shape=String(id)=="vco1Wave"||String(id)=="vco2Wave";
   const NormalisableRange<float> range=shape?NormalisableRange<float>(lo,hi,[](float s,float e,float x){return s+(e-s)*x;},[](float s,float e,float v){return (v-s)/(e-s);},
                                                [](float,float,float v){for(int k=0;k<4;++k)if(std::abs(v-float(k)/3.f)<.03f)return float(k)/3.f;return v;})
                                       :octaves?NormalisableRange<float>(lo,hi,[](float s,float e,float x){return s*std::pow(e/s,x);},[](float s,float e,float v){return std::log(v/s)/std::log(e/s);},nullptr)
                                               :NormalisableRange<float>(lo,hi,0,skew);const bool centred=lo<0.f,whole=hi>=100.f,fine=lo>0.f&&lo<.06f;
   String unit=String(name).fromFirstOccurrenceOf("(",false,false).upToFirstOccurrenceOf(")",false,false);
   const String key(id);if(key=="arpTempo")unit="BPM";if(key=="fattack"||key=="fdecay"||key=="frelease")unit="s";if(key=="osc2Tune")unit="st";
   if(unit=="semitones")unit="st";if(unit=="octaves")unit="oct";if(unit=="cents")unit="ct";
   auto text=[range,centred,whole,fine,unit](float v,int){
    const float mark=range.convertTo0to1(range.snapToLegalValue(v))*10.f-(centred?5.f:0.f);
    String shown=(centred&&mark>.04f?"+":"")+String(mark,1);
    if(unit.isNotEmpty())shown+=" ("+(whole?String(roundToInt(v)):String(v,fine&&std::abs(v)<1.f?2:1))+" "+unit+")";
    return shown;
   };
   auto parse=[range,centred](const String& typed){return range.convertFrom0to1(jlimit(0.f,1.f,(typed.getFloatValue()+(centred?5.f:0.f))/10.f));};
   keep(std::make_unique<AudioParameterFloat>(ParameterID{id,1},label(id,name),range,def,AudioParameterFloatAttributes().withStringFromValueFunction(text).withValueFromStringFunction(parse)));
  };
  add("wave","Saw / Pulse",0,1,0.35f);add("width","Pulse width",0.08f,0.92f,0.5f);add("sub","Sub level",0,1,0.7f);add("detune","VCO 2 fine (cents)",-50,50,0);
  add("cutoff","Cutoff (Hz)",25,18000,1800,0.3f);add("resonance","Resonance",0,0.95f,0.2f);add("env","Filter envelope",0,1,0.35f);
  add("attack","Attack (s)",0.001f,5,0.02f,0.33f);add("decay","Decay (s)",0.002f,12,0.6f,0.32f);add("sustain","Sustain",0,1,0.7f);add("release","Release (s)",0.002f,12,0.8f,0.32f); // Juno-60 spec sheet: decay and release to 12 s
  add("rate","LFO (Hz)",0.05f,20,0.6f,0.33f);add("pwm","PWM depth",0,1,0.3f);add("chorus","Chorus",0,1,0.35f);add("gain","Output",0,1,0.7f);
  add("fattack","Filter attack",0.001f,5,0.02f,0.33f);add("fdecay","Filter decay",0.002f,12,0.6f,0.32f);add("fsustain","Filter sustain",0,1,0.3f);add("frelease","Filter release",0.002f,12,0.8f,0.32f);
  add("hpfFreq","High-pass",0,1,.25f); /* bass boost below a quarter, flat, then a rising high-pass */ add("keytrack","Filter key track",0,1,0.5f);add("noise","Noise",0,1,0);add("velocity","Velocity",0,1,0);
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"slope",1},label("slope","Filter A slope"),StringArray{"12 dB","24 dB"},1));
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"chorusMode",1},label("chorusMode","Chorus mode"),StringArray{"OFF","I","II","I+II"},1));
  add("portamento","Portamento (s)",0,2,0,0.4f);
  keep(std::make_unique<AudioParameterBool>(ParameterID{"arpOn",1},label("arpOn","Arpeggiator"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"arpSync",1},label("arpSync","Host tempo"),false));
  add("arpTempo","Arp tempo",40,300,120);add("arpGate","Arp gate",0.1f,0.95f,0.65f);
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"arpDivision",1},label("arpDivision","Arp division"),StringArray{"1/4","1/8","1/16","1/32"},1));
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"arpPattern",1},label("arpPattern","Arp pattern"),StringArray{"UP","DOWN","UP/DOWN","RANDOM"},0));
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"arpOctaves",1},label("arpOctaves","Arp octaves"),StringArray{"1 OCT","2 OCT","3 OCT"},0));
  add("fxDrive","Drive",0,1,0);add("delayTime","Delay time (s)",0.03f,1.5f,0.3f);add("delayFeedback","Delay feedback",0,1.1f,0.3f);add("delayMod","Delay modulation",0,1,0.3f);add("delayMix","Delay mix",0,0.65f,0);
  add("reverbSize","Reverb size",0,1,0.5f);add("reverbDamp","Reverb damping",0,1,0.5f);add("reverbMix","Reverb mix",0,0.65f,0);add("reverbColor","Reverb color",0,1,0.4f);
  add("oscLayer","Jupiter source mix",0,1,.5f);
  add("jupiterMix","DCO / VCO mix",0,1,1); /* Vantage plays its two VCOs; the DCO stays for old patches */ add("vco1Width","VCO 1 pulse width",.08f,.92f,.5f);add("vcoPwm","VCO PWM depth",0,1,0);
  keep(std::make_unique<AudioParameterBool>(ParameterID{"vco2Low",1},label("vco2Low","VCO 2 low frequency"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"envLink",1},label("envLink","Link filter to amp envelope"),true));
  auto choice=[&](const char* id,const char* name,StringArray values,int initial=0){keep(std::make_unique<AudioParameterChoice>(ParameterID{id,1},label(id,name),values,initial));};
  choice("osc1Shape","VCO 1 shape",{"Saw","Pulse","Triangle","Sine","Square","Noise","Morph"},6);
  choice("osc2Shape","VCO 2 shape",{"Saw","Pulse","Triangle","Sine","Square","Noise","Morph"},6);
  // Vantage: one continuous wave control per VCO (triangle, saw, square, narrow pulse), the mixer, and POLY-MOD.
  add("vco1Wave","VCO 1 shape",0,1,.33f);add("vco2Wave","VCO 2 shape",0,1,.33f);add("vcoSub","VCO sub",0,1,0);add("vcoNoise","VCO noise",0,1,0);
  add("polyEnv","Poly-mod filter envelope",-1,1,0);add("polyOsc","Poly-mod VCO 1",-1,1,0);
  keep(std::make_unique<AudioParameterBool>(ParameterID{"polyFreq",1},label("polyFreq","Poly-mod to VCO 2 frequency"),true));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"polyWidth",1},label("polyWidth","Poly-mod to VCO 2 pulse width"),false));
  // PHASER (after the chorus).
  keep(std::make_unique<AudioParameterBool>(ParameterID{"phaserOn",1},label("phaserOn","Phaser on"),false));
  add("phaserRate","Phaser rate (Hz)",.05f,5,.35f,.4f);add("phaserDepth","Phaser depth",0,1,.6f);add("phaserFeedback","Phaser feedback",0,.85f,.45f);add("phaserMix","Phaser mix",0,1,.5f);
  // SEQUENCER: REC (then play notes or chords, one step each), REST (a silent step while recording), PLAY.
  keep(std::make_unique<AudioParameterBool>(ParameterID{"seqRec",1},label("seqRec","Seq Record"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"seqRest",1},label("seqRest","Seq Rest"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"seqPlay",1},label("seqPlay","Seq Play"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"seqOvd",1},label("seqOvd","Seq Overdub"),false)); // the REC key's second state
  keep(std::make_unique<AudioParameterBool>(ParameterID{"seqLive",1},label("seqLive","Seq Live"),false)); // LIVE: while the sequence plays, the keys play over it
  keep(std::make_unique<AudioParameterBool>(ParameterID{"polyFilter",1},label("polyFilter","Poly-mod to filters"),false));
  add("osc1Level","Oscillator I level",0,1,1);add("osc2Width","Oscillator II pulse width",.08f,.92f,.5f);
  choice("osc1Octave","VCO 1 range",{"64'","32'","16'","8'","4'","2'"},3);
  choice("osc2Octave","VCO 2 range",{"64'","32'","16'","8'","4'","2'"},3);
  add("osc2Tune","Oscillator II semitones",-12,12,0);
  choice("lfoShape","LFO waveform",{"Triangle","Sine","Saw","Square"});
  add("lfoDelay","LFO delay (s)",0,3,0,.5f);add("lfoPitch","LFO pitch (semitones)",0,12,0);add("lfoFilter","LFO filter (octaves)",0,4,0);
  add("envPitch","Envelope pitch (semitones)",-24,24,0);add("wheelDepth","Mod wheel vibrato (semitones)",0,12,1);
  choice("reverbType","Reverb type",{"VINT","PLATE","SPACE"});
  choice("delaySync","Delay sync",{"FREE","1/2","1/4 D","1/4","1/4 T","1/8 D","1/8","1/8 T","1/16 D","1/16","1/16 T"});
  keep(std::make_unique<AudioParameterBool>(ParameterID{"hold",1},label("hold","Hold"),false));keep(std::make_unique<AudioParameterBool>(ParameterID{"oscSync",1},label("oscSync","Oscillator II sync"),false));add("crossMod","Cross modulation II > I",0,1,0);
  choice("vcaMode","VCA mode",{"Envelope","Gate","Filter envelope"});choice("envInvert","Filter A envelope polarity",{"Normal","Inverted"});choice("pwmSource","PWM source",{"LFO","Manual","Envelope","Envelope inverted","Amp envelope","Amp envelope inverted"});
  add("bendFilter","Pitch bend to filter (octaves)",0,3,0);
  choice("voiceMode","Voice mode",{"Poly","Mono","Unison"});
  keep(std::make_unique<AudioParameterBool>(ParameterID{"legato",1},label("legato","Legato envelopes"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"delayOn",1},label("delayOn","Delay on"),true));keep(std::make_unique<AudioParameterBool>(ParameterID{"reverbOn",1},label("reverbOn","Reverb on"),true));
  add("velFilter","Velocity to filter",0,1,0);
  keep(std::make_unique<AudioParameterBool>(ParameterID{"lfoKeyTrig",1},label("lfoKeyTrig","LFO key trigger"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"envLfoTrig",1},label("envLfoTrig","Envelopes on LFO cycle"),false));
  keep(std::make_unique<AudioParameterChoice>(ParameterID{"lfoSync",1},label("lfoSync","LFO sync"),StringArray{"FREE","4 BARS","2 BARS","1 BAR","1/2","1/4","1/8","1/16","1/4 T","1/8 T","1/16 T"},0));
  add("wheelFilter","Mod wheel to filter",0,1,0);
  add("atVolume","Pressure to volume",0,1,0);add("atLfo","Pressure to LFO on the filters",0,1,0);add("atDrive","Pressure to drive",0,1,0);
  // LFO MAN: the LFO only runs while LFO TRIG is held (a panel key, or any learned MIDI button), its DELAY counted from the press.
  keep(std::make_unique<AudioParameterBool>(ParameterID{"lfoManual",1},label("lfoManual","LFO manual trigger mode"),false));
  keep(std::make_unique<AudioParameterBool>(ParameterID{"lfoTrig",1},label("lfoTrig","LFO trigger"),false));add("masterTune","Master tune (cents)",-50,50,0);add("atFilter","Pressure to filter",0,1,0);
  add("voiceDrive","Voice drive",0,1,.3f);add("age","Age",0,1,.3f);add("voiceWidth","Voice width",0,1,.5f);
  add("unisonSpread","Unison detune (cents)",0,40,12);add("bendRange","Pitch bend (semitones)",0,24,2);
  choice("filterRoute","Filter routing",{"A only","B only","A > B","B > A","Parallel"});
  choice("filterAType","Filter A model",{"Ladder","State variable"});
  choice("filterBType","Filter B model",{"Ladder","State variable"},1);
  add("cutoffB","Filter B cutoff (Hz)",25,18000,1800,.3f);add("resonanceB","Filter B resonance",0,.95f,.2f);
  choice("slopeB","Filter B slope",{"12 dB","24 dB"},1);
  add("filterBlend","Parallel A / B balance",0,1,.5f);add("envB","Filter B envelope",-1,1,.35f);
   AudioProcessorValueTreeState::ParameterLayout l;
   for(const char* id:hostPages)for(auto& parameter:made)if(parameter&&parameter->paramID==id)l.add(std::move(parameter));
   for(auto& parameter:made)if(parameter)l.add(std::move(parameter)); // anything not placed on a page goes last, never lost
  return l;
 }
 const String getName() const override {return "Vantage";}
 bool acceptsMidi() const override {return true;} bool producesMidi() const override {return false;}
 double getTailLengthSeconds() const override {return 30;}
 struct Patch {const char* name; int engine; float cutoff, resonance, attack, decay, sustain, release, wave, sub, detune, chorus, amount;};
 static const std::array<Patch,24>& patches() {
  static const std::array<Patch,24> bank{{
   {"INIT / Classic",0,1800,.2f,.02f,.6f,.7f,.8f,.35f,.7f,8,.35f,.35f},
   {"PAD / Velvet",0,1200,.15f,1.2f,1.4f,.8f,2.8f,.3f,.5f,8,.65f,.25f},
   {"BASS / Midnight",0,350,.3f,.004f,.28f,.25f,.18f,.1f,.9f,8,.12f,.6f},
   {"PLUCK / Glass",0,900,.5f,.003f,.32f,.05f,.4f,.65f,.2f,8,.45f,.7f},
   {"BRASS / Solar",1,1300,.2f,.05f,.5f,.65f,.35f,.1f,0,12,.2f,.5f},
   {"STRINGS / Aurora",1,2100,.12f,.7f,1.1f,.8f,2.2f,.2f,0,18,.5f,.2f},
   {"KEYS / Neon",1,1700,.25f,.006f,.7f,.3f,.65f,.55f,0,7,.35f,.45f},
   {"PAD / Slow Horizon",1,650,.35f,2.1f,2.0f,.8f,4.0f,.4f,0,22,.55f,.6f},
   {"BASS / Round Sub",0,220,.12f,.003f,.35f,.55f,.15f,.85f,1,3,0,.35f},
   {"BASS / Rubber Funk",0,420,.55f,.003f,.2f,.12f,.12f,.45f,.8f,5,.08f,.72f},
   {"BASS / Hybrid Drive",0,650,.2f,.005f,.35f,.4f,.2f,.12f,.75f,9,.05f,.45f},
   {"BASS / Pulse Runner",1,510,.32f,.003f,.18f,.25f,.13f,.9f,.6f,5,0,.5f},
   {"BASS / Deep Velvet",0,180,.18f,.012f,.65f,.8f,.3f,.25f,1,4,.15f,.18f},
   {"BRASS / Big Leap",1,2500,.08f,.008f,.65f,.72f,.22f,0,0,7,0,.36f},
   {"BRASS / Big Leap Wide",1,2100,.1f,.01f,.75f,.75f,.3f,0,0,13,.18f,.4f},
   {"BRASS / Golden Horns",1,1400,.18f,.07f,.55f,.7f,.4f,.1f,0,10,.12f,.5f},
   {"BRASS / Hybrid Stabs",0,1700,.24f,.006f,.22f,.2f,.18f,.1f,.25f,11,.1f,.6f},
   {"BRASS / Soft Ensemble",1,900,.13f,.18f,.8f,.7f,.7f,.2f,0,15,.3f,.35f},
   {"PAD / Silk Cloud",0,800,.15f,1.8f,2.2f,.85f,3.5f,.35f,.35f,12,.6f,.3f},
   {"PAD / Hybrid Aurora",0,1200,.18f,1.2f,1.7f,.75f,3.8f,.15f,.3f,21,.55f,.3f},
   {"PAD / Star Choir",1,1500,.3f,.9f,1.4f,.7f,2.8f,.7f,0,16,.4f,.25f},
   {"PAD / Midnight Cinema",1,450,.28f,2.6f,2.5f,.8f,5,.25f,.15f,24,.5f,.5f},
   {"PAD / Glass Atmosphere",0,2400,.4f,.8f,1.8f,.55f,3,.9f,.15f,10,.65f,.2f},
   {"PAD / Warm Horizon",1,700,.1f,1.5f,2,.8f,4,.05f,.2f,18,.4f,.3f}
  }};return bank;
 }
 int getNumPrograms() override {return int(patches().size());} int getCurrentProgram() override {return currentProgram.load();}
 void setCurrentProgram(int index) override {
  if(!isPositiveAndBelow(index,getNumPrograms())) return;
  const auto& p=patches()[size_t(index)];
  // A patch change leaves the performance settings alone: output level, arpeggiator, hold, bend range.
  static const StringArray performance{"gain","arpOn","arpSync","arpTempo","arpGate","arpDivision","arpPattern","arpOctaves","hold","bendRange","seqRec","seqRest","seqPlay","seqOvd","seqLive"};
  for(auto* param:getParameters()){if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param))if(performance.contains(identified->paramID))continue;param->setValueNotifyingHost(param->getDefaultValue());}
  auto set=[&](const char* id,float value){auto* param=state.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));};
  set("mode",float(p.engine));set("cutoff",p.cutoff);set("resonance",p.resonance);set("attack",p.attack);set("decay",p.decay);set("sustain",p.sustain);set("release",p.release);set("wave",p.wave);set("sub",p.sub);set("detune",p.detune);set("chorus",p.chorus);set("env",p.amount);
  set("fattack",p.attack*.6f);set("fdecay",p.decay);set("fsustain",p.sustain*.6f);set("frelease",p.release);
  // Vantage: every patch plays the two VCOs. The patch's saw/pulse balance becomes the SHAPE of both (saw to square),
  // its sub level the VCO sub, and VCO 2 is detuned against VCO 1 by the patch's detune.
  set("jupiterMix",1.f);set("oscLayer",.5f);
  set("vco1Wave",.33f+.33f*p.wave);set("vco2Wave",.33f+.33f*p.wave);set("vcoSub",p.sub*.6f);
  set("envLink",p.engine==0?1.f:0.f);
  if(index==10 || index==16 || index==19){set("envLink",0);set("slope",0);}
  if(index==10)set("fxDrive",.25f);
  // INIT: the filter has its own envelope, falling back low after each attack, so ENV sweeps every note and FREQ sets
  // the held tone (following the amp envelope it stayed 70 % open and pushed FREQ, and the resonance, far up).
  if(index==0){set("envLink",0);set("fattack",.005f);set("fdecay",.5f);set("fsustain",.2f);set("frelease",.4f);}
  // Factory sounds are starting points for the new architecture, not legacy-compatible patches.
  if(index>=18){set("reverbMix",.18f);set("reverbSize",.7f);}
  if(index==13 || index==14){set("fattack",.012f);set("fdecay",.48f);set("fsustain",.55f);set("frelease",.22f);set("slope",0);set("keytrack",.35f);set("pwm",0);set("reverbMix",.09f);}
  currentProgram.store(index);state.state.removeProperty("userPresetName",nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);
 }
 const String getProgramName(int index) override {return isPositiveAndBelow(index,getNumPrograms())?patches()[size_t(index)].name:String();}
 void changeProgramName(int,const String&) override {}
 void prepareToPlay(double sr,int block) override {
  blockCapacity=jmax(block,8192);chunkMidi.ensureSize(8192);
  arp.prepare(sr);hold.prepare();sampleRate=sr;main.prepare(sr,blockCapacity);sequenceSound.prepare(sr,blockCapacity);
  sequenceBuffer.setSize(2,blockCapacity);sequenceTail=0;wasLayered=false;wasRecording=wasResting=wasOverdubbing=liveFresh=false;recordHeld.clearQuick();recordChord.clearQuick();
 }
 void releaseResources() override {main.reset();sequenceSound.reset();keyboard.reset();arp.reset();hold.reset();}
 bool isBusesLayoutSupported(const BusesLayout& b) const override {return b.getMainOutputChannelSet()==AudioChannelSet::stereo();}
 // A block larger than prepared is played in pieces of the prepared size: nothing is ever allocated while playing.
 void processBlock(AudioBuffer<float>& buffer,MidiBuffer& midi) override {
  const int total=buffer.getNumSamples();if(total<=blockCapacity){processPiece(buffer,midi);return;}
  for(int done=0;done<total;done+=blockCapacity){const int count=jmin(blockCapacity,total-done);
   AudioBuffer<float> piece(buffer.getArrayOfWritePointers(),buffer.getNumChannels(),done,count);
   chunkMidi.clear();chunkMidi.addEvents(midi,done,count,-done);processPiece(piece,chunkMidi);}
  midi.clear();
 }
 void processPiece(AudioBuffer<float>& buffer,MidiBuffer& midi){
  ScopedNoDenormals noDenormals;
  bool restoredLive=false;
  if(sequenceLayer.applyPending(restoredLive)){restoredSequenceLayer.store(restoredLive);sequenceLayerReady.store(restoredLive);main.reset();sequenceSound.reset();arp.reset();hold.reset();sequenceTail=0;wasLayered=false;wasRecording=wasResting=wasOverdubbing=liveFresh=false;recordHeld.clearQuick();recordChord.clearQuick();}
  buffer.clear();keyboard.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
  if(const int bend=screenPitch.exchange(-1);bend>=0)midi.addEvent(MidiMessage::pitchWheel(1,bend),0);
  if(const int press=screenPressure.exchange(-1);press>=0)midi.addEvent(MidiMessage::channelPressureChange(1,press),0); // trackpad force on the panel keyboard
  // MIDI learn and learned controls. The mod wheel, sustain, bank select and channel messages keep their own jobs.
  for(const auto event:midi){const auto message=event.getMessage();if(!message.isController())continue;
   const int cc=message.getControllerNumber();if(cc==0||cc==1||cc==32||cc==64||cc>=120)continue;
   if(learning.load()&&lastTouched.load()>=0){const int target=lastTouched.load();forget(target);ccTarget[size_t(cc)].store(target);learning.store(false);}
   const int target=ccTarget[size_t(cc)].load();if(isPositiveAndBelow(target,getParameters().size()))getParameters()[target]->setValueNotifyingHost(float(message.getControllerValue())/127.f);
  }
  auto value=[this](const char* key){return state.getRawParameterValue(key)->load();};
  hold.process(midi,value("hold")>.5f);
  double bpm=value("arpTempo"),samplesToGrid=-1;const int division=int(value("arpDivision"));
  if(value("arpSync")>.5f) if(auto* playHead=getPlayHead()) if(auto position=playHead->getPosition()) if(auto hostBpm=position->getBpm()) {
   bpm=*hostBpm;
   // While the host plays, line the steps up with its beat grid (quarter, eighth or sixteenth notes).
   if(position->getIsPlaying()&&bpm>1) if(auto beats=position->getPpqPosition()) {
    const double steps[]={1.,.5,.25,.125},step=steps[jlimit(0,3,division)],halfSample=.5*bpm/(60.*sampleRate);
    double into=std::fmod(*beats,step);if(into<0)into+=step;
    samplesToGrid=(into<halfSample||step-into<halfSample)?0.:(step-into)*60./bpm*sampleRate;
   }
  }
  // SEQUENCER. REC while stopped starts a new sequence: each time every key is up again, the keys pressed since become
  // one step; REST adds a silent step. REC while PLAY runs is OVERDUB: each key pressed joins the step being played.
  // EDIT (from the panel): the next note or chord replaces (or joins) the selected step, then the next step is selected.
  // PLAY runs the arpeggiator clock over the sequence instead of the pattern; a held key transposes it.
  const bool rec=value("seqRec")>.5f,rest=value("seqRest")>.5f,play=value("seqPlay")>.5f;
  const bool ovd=value("seqOvd")>.5f,recording=rec&&!play&&!ovd,overdub=ovd||(rec&&play),playing=play||ovd;
  Arpeggiator::Sequence playingSequence;
  {const SpinLock::ScopedLockType lock(sequenceLock);
   if(recording&&!wasRecording){sequence.clear();recordHeld.clearQuick();recordChord.clearQuick();currentSequenceSlot.store(-1);}
   const int editing=editStep.load();
   // EDIT works while the sequence plays too: the keys are then heard and written, not used to transpose.
   if(recording||editing>=0){for(const auto meta:midi){const auto msg=meta.getMessage();
     if(msg.isNoteOn()){recordHeld.addIfNotAlreadyThere((msg.getChannel()-1)*128+msg.getNoteNumber());recordChord.addIfNotAlreadyThere(msg.getNoteNumber());}
     else if(msg.isNoteOff()){recordHeld.removeFirstMatchingValue((msg.getChannel()-1)*128+msg.getNoteNumber());
      if(recordHeld.isEmpty()&&!recordChord.isEmpty()){recordChord.sort();
       if(recording)sequence.add(recordChord.getRawDataPointer(),recordChord.size());
       else{const int selected=editStep.load();const bool lane=editLane.load();if(editAdds.load()){for(int n:recordChord)sequence.merge(selected,n,lane);}else sequence.setLane(selected,recordChord.getRawDataPointer(),recordChord.size(),lane);
        editStep.store(jmin(selected+1,jmax(0,sequence.length-1)));}
       recordChord.clearQuick();}}}
    if(recording&&rest&&!wasResting&&recordHeld.isEmpty())sequence.add(nullptr,0);}
   else{recordHeld.clearQuick();recordChord.clearQuick();}
   // LIVE RECORDING: overdub on an empty sequence starts a blank 16-step loop; what is played lands on it, quantized to
   // the nearest step, on the SEQ line (on the OVD line when the loop already held notes).
   if(overdub&&!wasOverdubbing){currentSequenceSlot.store(-1);liveFresh=sequence.length==0;if(liveFresh)for(int i=0;i<16;++i)sequence.add(nullptr,0);}
   if(!overdub&&wasOverdubbing&&liveFresh){sequence.updateRoot();liveFresh=false;}
   wasRecording=recording;wasOverdubbing=overdub;wasResting=rest;
   playingSequence=sequence;sequenceSteps.store(sequence.length);}
   const bool live=playing&&value("seqLive")>.5f;
   arp.process(midi,buffer.getNumSamples(),value("arpOn")>.5f||playing,bpm,division,playing?4:int(value("arpPattern")),int(value("arpOctaves"))+1,value("arpGate"),samplesToGrid,&playingSequence,overdub||editStep.load()>=0||live,overdub,live);
   if(arp.capturedCount>0){const SpinLock::ScopedLockType lock(sequenceLock);for(int i=0;i<arp.capturedCount;++i)sequence.merge(arp.capturedStep[size_t(i)],arp.capturedNote[size_t(i)],!liveFresh);
   sequenceSteps.store(sequence.length);}
  // LIVE: the sequence plays on its own sound, frozen when LIVE (or PLAY with LIVE on) started; the keys play the panel's.
  const bool layered=playing&&value("seqLive")>.5f;
  if(layered&&!wasLayered){if(!restoredSequenceLayer.exchange(false))sequenceLayer.capture();sequenceLayerReady.store(true);}
  if(!layered){restoredSequenceLayer.store(false);sequenceLayerReady.store(false);}
  if(!layered&&wasLayered)for(int ch=1;ch<=16;++ch)arp.sequenceOutput.addEvent(MidiMessage::allNotesOff(ch),0);
  if(layered||wasLayered)sequenceTail=int(sampleRate*getTailLengthSeconds()); // the frozen sound rings on (release, echo, reverb) after LIVE ends
  wasLayered=layered;
  renderSound(main,buffer,midi);
  if(sequenceTail>0&&buffer.getNumChannels()>=2){const int n=buffer.getNumSamples();sequenceBuffer.setSize(2,n,false,false,true);sequenceBuffer.clear();
   renderSound(sequenceSound,sequenceBuffer,arp.sequenceOutput);
   if(!layered){const float fadeSamples=float(sampleRate*.02);for(int i=0;i<n;++i){const float fade=jlimit(0.f,1.f,float(sequenceTail-i)/fadeSamples);for(int c=0;c<2;++c)sequenceBuffer.getWritePointer(c)[i]*=fade;}}
   for(int c=0;c<2;++c)buffer.addFrom(c,0,sequenceBuffer,c,0,n);
   for(int c=0;c<2;++c)for(int i=0;i<n;++i){auto& x=buffer.getWritePointer(c)[i];constexpr float knee=.98f;const float a=std::abs(x);if(a>knee)x=std::copysign(knee+(1.f-knee)*std::tanh((a-knee)/(1.f-knee)),x);}
   // Apply the safety ceiling after summing both engines. Individual limits cannot
   // protect the combined LIVE signal. Fade only the last 20 ms of a stopped tail.
   if(!layered)sequenceTail-=n;}
  else arp.sequenceOutput.clear();
 }
 // One sound through its voices, LFO, effects and output level.
 void renderSound(Engine& e,AudioBuffer<float>& buffer,MidiBuffer& midi){
  int mode=e.value("mode")>0.5f?1:0;
  e.voices8.updateMode();e.voices6.updateMode();
  // 6 <-> 8 VOICES: the notes of the engine left behind end with their own release while the other engine takes over.
  if(mode!=e.lastMode) {if(e.lastMode>=0)(e.lastMode==0?e.voices6:e.voices8).allNotesOff(0,true);e.lastMode=mode;}
  double lfoBpm=e.value("arpTempo");std::optional<double> lfoBeats;
  if(auto* playHead=getPlayHead()) if(auto position=playHead->getPosition()){if(auto hostBpm=position->getBpm())lfoBpm=*hostBpm;if(position->getIsPlaying())if(auto beats=position->getPpqPosition())lfoBeats=*beats;}
  renderLfo(e,buffer.getNumSamples(),lfoBpm,lfoBeats);
  (mode==0?e.voices6:e.voices8).renderNextBlock(buffer,midi,0,buffer.getNumSamples());
  {auto& other=mode==0?e.voices8:e.voices6;bool ringing=false;for(int i=0;i<other.getNumVoices()&&!ringing;++i)ringing=other.getVoice(i)->isVoiceActive();
   if(ringing){e.silence.clear();other.renderNextBlock(buffer,e.silence,0,buffer.getNumSamples());}}
  e.chorus.process(buffer,int(e.value("chorusMode")),e.value("chorus"));
  // PHASER: always running, its mix brought to zero (smoothly) when it is off, so switching it never clicks.
  if(buffer.getNumChannels()>=2){e.phaser.setRate(e.value("phaserRate"));e.phaser.setDepth(e.value("phaserDepth"));e.phaser.setFeedback(e.value("phaserFeedback"));
   e.phaser.setCentreFrequency(900.f);e.phaser.setMix(e.value("phaserOn")>.5f?e.value("phaserMix"):0.f);
   dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),2,size_t(buffer.getNumSamples()));e.phaser.process(dsp::ProcessContextReplacing<float>(block));}
  // Keep the POST oversampler warm and ramp its controls without changing its settled curve.
  e.outputDrive.process(buffer,e.value("fxDrive"));
  // Delay time: the TIME knob when free, otherwise a note value at the host tempo (the arpeggio TEMPO knob when there is no host).
  float delaySeconds=e.value("delayTime");
  if(const int syncStep=int(e.value("delaySync"));syncStep>0) {
   double tempo=e.value("arpTempo");
   if(auto* playHead=getPlayHead()) if(auto position=playHead->getPosition()) if(auto hostBpm=position->getBpm()) tempo=*hostBpm;
   static const double beats[]={0,2.,1.5,1.,2./3.,.75,.5,1./3.,.375,.25,1./6.}; // D = dotted, T = triplet
   double seconds=beats[jlimit(1,10,syncStep)]*60./jmax(20.,tempo);
   while(seconds>4.)seconds*=.5; // only beyond the 4 s line (a half note under 30 BPM) does it fall back to half the value
   delaySeconds=float(seconds);
  }
  // ON keys: off fades the effect out (as its LEVEL at zero would) and leaves every setting where it is.
  e.echo.process(buffer,delaySeconds,e.value("delayFeedback"),e.value("delayOn")>.5f?e.value("delayMix"):0.f,e.value("delayMod"));
  e.reverb.process(buffer,int(e.value("reverbType")),e.value("reverbSize"),e.value("reverbDamp"),e.value("reverbColor"),e.value("reverbOn")>.5f?e.value("reverbMix"):0.f);
  e.gain.setTargetValue(e.value("gain"));
  // Output: untouched up to the knee, then rounded off towards full scale, so loud passages never clip but normal
  // levels keep all their punch (a saturation applied everywhere used to soften every note a little).
  const auto limit=[](float x){constexpr float knee=.7f;const float a=std::abs(x);if(a<=knee)return x;return std::copysign(knee+(1.f-knee)*std::tanh((a-knee)/(1.f-knee)),x);};
  for(int i=0;i<buffer.getNumSamples();++i) {float g=e.gain.getNextValue();for(int c=0;c<buffer.getNumChannels();++c) buffer.setSample(c,i,limit(buffer.getSample(c,i)*g));}
 }
 bool hasEditor() const override {return true;} AudioProcessorEditor* createEditor() override;
 void getStateInformation(MemoryBlock& dest) override {auto saved=state.copyState();saved.setProperty("sequence",sequenceText(),nullptr);saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",modified.load(),nullptr);auto xml=saved.createXml();
  if(state.getRawParameterValue("seqLive")->load()>.5f && (state.getRawParameterValue("seqPlay")->load()>.5f||state.getRawParameterValue("seqOvd")->load()>.5f)){
   bool queued=false,queuedLive=false;const auto frozen=sequenceLayer.forSave(queued,queuedLive);const bool frozenReady=queued?queuedLive:sequenceLayerReady.load();
   auto* live=xml->createNewChildElement("LiveSound");
   for(auto* base:getParameters())if(auto* p=dynamic_cast<RangedAudioParameter*>(base)){auto* child=live->createNewChildElement("PARAM");child->setAttribute("id",p->paramID);child->setAttribute("value",frozenReady?sequenceLayer.get(frozen,state.getRawParameterValue(p->paramID)):state.getRawParameterValue(p->paramID)->load());}
  }
  auto* map=xml->createNewChildElement("MidiMap");for(int cc=0;cc<128;++cc)if(const int index=ccTarget[size_t(cc)].load();index>=0){auto* entry=map->createNewChildElement("Map");entry->setAttribute("cc",cc);entry->setAttribute("id",parameterId(index));}
  copyXmlToBinary(*xml,dest);}
 // ---- MIDI learn: one CC per control. Learning: touch a control on the panel, then move a knob on the controller.
 void startLearning(){lastTouched.store(-1);learning.store(true);}
 void stopLearning(){learning.store(false);}
 bool isLearning() const {return learning.load();}
 int lastTouchedParameter() const {return lastTouched.load();}
 int ccFor(int index) const {for(int cc=0;cc<128;++cc)if(ccTarget[size_t(cc)].load()==index)return cc;return -1;}
 void forget(int index){for(auto& slot:ccTarget)if(slot.load()==index)slot.store(-1);}
 void forgetAll(){for(auto& slot:ccTarget)slot.store(-1);}
 String parameterId(int index) const {if(auto* p=dynamic_cast<RangedAudioParameter*>(getParameters()[index]))return p->paramID;return {};}
 int parameterIndex(const String& id) const {for(int i=0;i<getParameters().size();++i)if(parameterId(i)==id)return i;return -1;}
 void setStateInformation(const void* data,int size) override {auto xml=getXmlFromBinary(data,size);if(xml && xml->hasTagName(state.state.getType())) {
   // MIDI learn assignments belong to this instance, not to a sound: they are restored with a session, and a preset
   // (which never contains them) leaves them as they are.
   if(auto* map=xml->getChildByName("MidiMap")){for(auto& slot:ccTarget)slot.store(-1);
    for(auto* entry:map->getChildIterator()){const int cc=entry->getIntAttribute("cc",-1);const int index=parameterIndex(entry->getStringAttribute("id"));if(isPositiveAndBelow(cc,128)&&index>=0)ccTarget[size_t(cc)].store(index);}
    xml->removeChildElement(map,true);}
   std::unique_ptr<XmlElement> liveSound;
   if(auto* live=xml->getChildByName("LiveSound")){if(!loadingPreset.load())liveSound=std::make_unique<XmlElement>(*live);xml->removeChildElement(live,true);}
   auto restored=ValueTree::fromXml(*xml);
   // LIVE: a sound or preset changed on stage keeps the sequencer keys as they are; while the sequence runs it also keeps
   // the sequence and its clock, so the loop goes on with the new sound.
   if(loadingPreset.load()){auto value=[this](const String& id){return state.getRawParameterValue(id)->load();};const bool running=value("seqPlay")>.5f||value("seqOvd")>.5f;
    StringArray kept{"seqRec","seqRest","seqPlay","seqOvd","seqLive"};if(running){kept.addArray(StringArray{"arpTempo","arpDivision","arpGate","arpSync"});restored.removeProperty("sequence",nullptr);}
    for(const auto& id:kept){ValueTree child;for(auto c:restored)if(c.getProperty("id").toString()==id){child=c;break;}
     if(!child.isValid()){child=ValueTree("PARAM");child.setProperty("id",id,nullptr);restored.addChild(child,-1,nullptr);}
     child.setProperty("value",value(id),nullptr);}}
   // A patch saved with a sequence brings it back; one saved without leaves the current sequence alone.
   if(restored.hasProperty("sequence")){Arpeggiator::Sequence loaded;loaded.fromString(restored.getProperty("sequence").toString());{const SpinLock::ScopedLockType lock(sequenceLock);sequence=loaded;sequenceSteps.store(sequence.length);currentSequenceSlot.store(-1);}restored.removeProperty("sequence",nullptr);}
   // Sessions and presets saved before state version 2 stored chorus mode as 0 = I, 1 = II.
   if(int(restored.getProperty("stateVersion",1))<2){for(auto child:restored)if(child.getProperty("id").toString()=="chorusMode")child.setProperty("value",float(child.getProperty("value"))+1.f,nullptr);}
   // Before state version 3 the high-pass was a continuous cutoff in Hz; it is now a four-position switch.
   if(int(restored.getProperty("stateVersion",1))<3){for(auto child:restored)if(child.getProperty("id").toString()=="hpf"){const float hz=float(child.getProperty("value"));child.setProperty("value",hz<100.f?1.f:(hz<480.f?2.f:3.f),nullptr);}}
   // Before state version 4 the high-pass was a four-position switch; it is now one continuous fader (hpfFreq).
   if(int(restored.getProperty("stateVersion",1))<4){for(auto child:restored)if(child.getProperty("id").toString()=="hpf"){static const float positions[]={0.f,.25f,.676f,.922f};
    ValueTree moved("PARAM");moved.setProperty("id","hpfFreq",nullptr);moved.setProperty("value",positions[jlimit(0,3,int(child.getProperty("value")))],nullptr);restored.addChild(moved,-1,nullptr);break;}}
   restored.setProperty("stateVersion",5,nullptr);
   currentProgram.store(jlimit(0,getNumPrograms()-1,int(restored.getProperty("program",0))));for(auto* base:getParameters())if(auto* parameter=dynamic_cast<RangedAudioParameter*>(base)) {
   bool found=false;for(auto child:restored)if(child.getProperty("id").toString()==parameter->paramID){found=true;break;}
   if(!found){ValueTree child("PARAM");child.setProperty("id",parameter->paramID,nullptr);child.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);restored.addChild(child,-1,nullptr);}
  }state.replaceState(restored);
   if(!loadingPreset.load()){
    auto frozen=sequenceLayer.panel();
    if(liveSound)for(auto* child:liveSound->getChildIterator())if(auto* p=state.getParameter(child->getStringAttribute("id"))){const float value=float(child->getDoubleAttribute("value"));if(std::isfinite(value)){const auto& range=p->getNormalisableRange();sequenceLayer.assign(frozen,state.getRawParameterValue(p->paramID),jlimit(range.start,range.end,value));}}
    sequenceLayer.queue(frozen,liveSound!=nullptr);
   }
   modified.store(bool(restored.getProperty("modified",false)));}}
private:
 void parameterChanged(const String&,float) override {modified.store(true);}
 void audioProcessorParameterChanged(AudioProcessor*,int,float) override {}
 void audioProcessorChanged(AudioProcessor*,const AudioProcessorListener::ChangeDetails&) override {}
 void audioProcessorParameterChangeGestureBegin(AudioProcessor*,int index) override {lastTouched.store(index);} // the control just touched on the panel
 std::array<std::atomic<int>,128> ccTarget;std::atomic<bool> learning{false};std::atomic<int> lastTouched{-1};
 // The single LFO shared by all voices: free-running, one value per sample of the current block.
 void renderLfo(Engine& e,int count,double bpm,std::optional<double> beats) {
  // Sized once in prepare: processBlock never hands over more than blockCapacity samples, so nothing is resized here.
  jassert(count<=int(e.modulation.lfo.size()));count=jmin(count,int(e.modulation.lfo.size()));
  const int shape=int(e.value("lfoShape"));double step=e.value("rate")/sampleRate;
  // LFO SYNC: one cycle lasts a note value at the host tempo (the arpeggio TEMPO without a host); while the host plays,
  // the cycle is also lined up with its bars.
  if(const int sync=int(e.value("lfoSync"));sync>0){static const double quarters[]={0,16,8,4,2,1,.5,.25,2./3.,1./3.,1./6.};
   const double length=quarters[jlimit(1,10,sync)];step=jmax(20.,bpm)/60./length/sampleRate;
   if(beats){double into=std::fmod(*beats/length,1.);if(into<0)into+=1.;e.lfoPhase=into;}}
  e.modulation.shape=shape;e.modulation.step=step;
  // LFO MAN: tells the voices whether LFO TRIG is held, and for how long (samples, at the start of this block).
  e.modulation.manual=e.value("lfoManual")>.5f;
  if(e.modulation.manual&&e.value("lfoTrig")>.5f){e.modulation.manualAge=e.manualCount;e.manualCount+=count;}else{e.manualCount=0;e.modulation.manualAge=-1;}
  for(int i=0;i<count;++i) {
   const float t=float(e.lfoPhase);e.modulation.phase[size_t(i)]=t;e.modulation.lfo[size_t(i)]=ModulationBus::wave(shape,t);e.lfoPhase+=step;if(e.lfoPhase>=1)e.lfoPhase-=1;
  }
 }

 // ONE SOUND: voices, LFO, effects and output level. main plays the panel; sequenceSound plays a frozen copy of it (LIVE).
 struct Engine {
  Engine(AudioProcessorValueTreeState& s,const ParamLayer* frozen):state(s),parameterReads(s),layer(frozen),voices8(s,true,frozen),voices6(s,true,frozen){
   for(int i=0;i<8;++i){auto* voice=new Voice(s,previousEight,modulation,frozen);voice->setVoiceIndex(i);voice->setVintage(true);voices8.addVoice(voice);} voices8.addSound(new Sound());voices8.setNoteStealingEnabled(true);
   for(int i=0;i<6;++i){auto* voice=new Voice(s,previousSix,modulation,frozen);voice->setVoiceIndex(i);voices6.addVoice(voice);} voices6.addSound(new Sound());voices6.setNoteStealingEnabled(true);}
  float value(const char* key) const {const auto* a=parameterReads.get(key);return layer!=nullptr?layer->get(a):a->load();}
  void prepare(double sr,int block){
   outputDrive.prepare(sr,block,value("fxDrive"));
   previousSix=previousEight=0;modulation.lfo.assign(size_t(jmax(block,8192)),0.f);modulation.phase.assign(modulation.lfo.size(),0.f);lfoPhase=0;
   echo.prepare(sr);reverb.prepare(sr);
   voices8.setCurrentPlaybackSampleRate(sr); for(int i=0;i<voices8.getNumVoices();++i) static_cast<Voice*>(voices8.getVoice(i))->prepare(sr);
   voices6.setCurrentPlaybackSampleRate(sr); for(int i=0;i<voices6.getNumVoices();++i) static_cast<Voice*>(voices6.getVoice(i))->prepare(sr);
   chorus.prepare(sr);phaser.prepare({sr,uint32(jmax(block,512)),2});silence.ensureSize(64);phaser.reset();
   gain.reset(sr,0.02);gain.setCurrentAndTargetValue(value("gain"));}
  void reset(){voices8.allNotesOff(0,false);voices6.allNotesOff(0,false);chorus.reset();phaser.reset();outputDrive.reset(value("fxDrive"));echo.reset();reverb.reset();previousSix=previousEight=0;lfoPhase=manualCount=0;}
  AudioProcessorValueTreeState& state;ParameterReadCache parameterReads;const ParamLayer* layer;
  GlideMemory previousSix,previousEight;ModulationBus modulation;double lfoPhase=0,manualCount=0;
  VoiceAllocator voices8,voices6;EnsembleChorus chorus;dsp::Phaser<float> phaser;CharacterDelay echo;OrbitReverb reverb;
  OutputDrive outputDrive;SmoothedValue<float> gain;int lastMode=-1;MidiBuffer silence;
 };
 public: std::atomic<int> screenPitch{-1};
 public: std::atomic<int> screenPressure{-1}; // set by the panel keyboard (Force Touch), sent as channel pressure
 private:
 std::atomic<bool> modified{false};
 std::atomic<int> currentProgram{0};
 double sampleRate=44100;Arpeggiator arp;Arpeggiator::Sequence sequence;SpinLock sequenceLock;Array<int> recordHeld,recordChord;bool wasRecording=false,wasResting=false,wasOverdubbing=false,liveFresh=false;std::atomic<bool> loadingPreset{false},restoredSequenceLayer{false},sequenceLayerReady{false};NoteHold hold;
 int blockCapacity=8192;MidiBuffer chunkMidi;
 ParamLayer sequenceLayer;Engine main,sequenceSound;AudioBuffer<float> sequenceBuffer;int sequenceTail=0;bool wasLayered=false;
};
#include "Editor.h"
AudioProcessorEditor* Orbit::createEditor(){return new Editor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Orbit();}
