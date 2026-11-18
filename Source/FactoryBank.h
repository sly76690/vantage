#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// VANTAGE factory bank: the 40 factory sounds (Presets/Sounds) embedded in the binary, played by BANK A to E and
// PATCH 1-8 in the order below, so the program buttons give Vantage's own sounds even before the installer ran.
class FactoryBank {
public:
 static const juce::StringArray& order() {
  static const juce::StringArray names{
   // A: brass and chords
   "BRASS - Vantage Brass","BRASS - Arena Stabs","BRASS - Soft Horns","BRASS - Hollow Notch",
   "CHORD - Minor Seventh Stabs","CHORD - Power Fifths","CHORD - Sus Horizon","CHORD - Major Ninth Keys",
   // B: pads, keys and air
   "PAD - Wide Strings","PAD - Notch Drift","PAD - Band Choir","PAD - Highpass Air","PAD - Eight Rivers",
   "KEYS - Bright Poly","KEYS - Band Clav","FX - Filter Wind",
   // C: basses, leads and sequences
   "BASS - Round Twelve","BASS - Unison Wall","BASS - Band Pulse","LEAD - Sync Horn","LEAD - Singing Band","LEAD - Unison Solo",
   "SEQ - Chord Steps","SEQ - Notch Pulse",
   // D: the modulation section
   "BASS - Talking Mode","BASS - Mode Pluck","PAD - Mode Sweep","PAD - Tremolo Strings","PAD - Pressure Choir","KEYS - Shape Bells","KEYS - Metal Comb","LEAD - Vowel Lead",
   // E: unison counts, S&H, ramps and movement
   "LEAD - Three Voice Unison","BASS - Four Voice Unison","BRASS - Unison Section","FX - Random Band","FX - Ramp Rise","PAD - Spread Pitch","CHORD - Notch Organ","SEQ - Mode Steps"};
  return names;
 }
 static std::unique_ptr<juce::XmlElement> load(const juce::String& name) {
  for(int i=0;i<FactoryData::namedResourceListSize;++i)
   if(juce::String::fromUTF8(FactoryData::originalFilenames[i]).upToLastOccurrenceOf(".orbitpreset",false,false)==name){
    int size=0;const auto* data=FactoryData::getNamedResource(FactoryData::namedResourceList[i],size);
    return data?juce::XmlDocument::parse(juce::String::fromUTF8(data,size)):nullptr;}
  return {};
 }
};
