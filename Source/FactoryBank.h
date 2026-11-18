#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// VANTAGE factory bank: the 24 factory sounds (Presets/Sounds) embedded in the binary, played by BANK A/B/C and
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
   "SEQ - Chord Steps","SEQ - Notch Pulse"};
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
