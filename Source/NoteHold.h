#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// HOLD: keeps notes sounding after the keys are released. Releasing every key and then playing
// again replaces the held notes with the new ones, so a chord (or an arpeggio) can be changed
// without touching the button. Switching HOLD off releases whatever is still held.
// It works on the MIDI stream ahead of the arpeggiator, which therefore keeps running on held notes.
class NoteHold {
public:
 void prepare() { output.ensureSize(32768); reset(); }
 void reset() { down.fill(false); held.fill(false); pressed = 0; anyHeld = false; wasOn = false; }
 void process(juce::MidiBuffer& midi, bool on) {
  output.clear();
  if (wasOn && !on) releaseHeld(0);
  wasOn = on;
  for (const auto event : midi) {
   const auto message = event.getMessage(); const int at = event.samplePosition;
   if (message.isNoteOn()) {
    const auto key = index(message);
    if (on && pressed == 0 && anyHeld) releaseHeld(at);       // first key of a new chord: drop the old one
    if (held[key]) { held[key] = false; output.addEvent(juce::MidiMessage::noteOff(message.getChannel(), message.getNoteNumber()), at); }
    if (!down[key]) { down[key] = true; ++pressed; }
    output.addEvent(message, at);
   } else if (message.isNoteOff()) {
    const auto key = index(message);
    if (down[key]) { down[key] = false; --pressed; }
    if (on) { held[key] = true; anyHeld = true; } else output.addEvent(message, at);
   } else {
    if (message.isAllNotesOff() || message.isAllSoundOff()) resetChannel(message.getChannel());
    output.addEvent(message, at);
   }
  }
  midi.swapWith(output);
 }
private:
 static size_t index(const juce::MidiMessage& message) { return size_t((message.getChannel() - 1) * 128 + message.getNoteNumber()); }
 void resetChannel(int channel) {
  const size_t first = size_t((channel - 1) * 128);
  for (size_t key = first; key < first + 128; ++key) {
   if (down[key]) --pressed;
   down[key] = held[key] = false;
  }
  anyHeld = false;
  for (bool note : held) anyHeld = anyHeld || note;
 }
 void releaseHeld(int at) {
  if (!anyHeld) return;
  for (size_t key = 0; key < held.size(); ++key) if (held[key]) { held[key] = false; if (!down[key]) output.addEvent(juce::MidiMessage::noteOff(int(key / 128) + 1, int(key % 128)), at); }
  anyHeld = false;
 }
 std::array<bool, 2048> down{}, held{}; int pressed = 0; bool anyHeld = false, wasOn = false; juce::MidiBuffer output;
};
