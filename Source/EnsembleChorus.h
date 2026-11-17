#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

// Stereo ensemble chorus in the manner of the bucket-brigade choruses of early-80s polysynths.
// The input is summed to mono and written into one short delay line. Two taps read it back, one
// per output side, their delay times swept by the same triangle LFO in opposite directions: as
// the left tap shortens the right one lengthens, which is what gives the wide, slowly rotating image.
// Both the signal going in and each tap coming out pass a gentle low-pass, as a bucket-brigade
// line needs to keep clock noise out, so the effect is slightly darker than the dry sound.
//
// This is an original model. The constants below are starting values chosen to sit in the range
// usually quoted for that family of circuits; they are meant to be tuned by ear and measurement.
class EnsembleChorus {
public:
 enum Mode { off = 0, modeI = 1, modeII = 2, modeBoth = 3 }; // modeBoth: fast and shallow, closer to a vibrato

 void prepare(double sampleRate) {
  sr = float(sampleRate);
  line.assign(size_t(juce::nextPowerOfTwo(int(sampleRate * .012) + 8)), 0.f); mask = int(line.size()) - 1;
  tone = float(1. - std::exp(-juce::MathConstants<double>::twoPi * toneHz / sampleRate));
  wet.reset(sampleRate, .04); rate.reset(sampleRate, .25); depth.reset(sampleRate, .25);
  wet.setCurrentAndTargetValue(0.f); rate.setCurrentAndTargetValue(rateI); depth.setCurrentAndTargetValue(depthMs);
  reset();
 }
 void reset() { std::fill(line.begin(), line.end(), 0.f); write = 0; pre = postL = postR = 0.f; }

 // amount: 0 = dry only, 1 = dry and delayed signal at equal level (the classic setting).
 void process(juce::AudioBuffer<float>& buffer, int mode, float amount) {
  if (line.empty() || buffer.getNumChannels() < 2) return;
  wet.setTargetValue(mode == off ? 0.f : juce::jlimit(0.f, 1.f, amount));
  if (mode != off) { rate.setTargetValue(mode == modeBoth ? rateBoth : (mode == modeII ? rateII : rateI)); depth.setTargetValue(mode == modeBoth ? depthBothMs : depthMs); }
  auto* left = buffer.getWritePointer(0); auto* right = buffer.getWritePointer(1);
  const float perMs = sr * .001f;
  for (int i = 0; i < buffer.getNumSamples(); ++i) {
   const float dryL = left[i], dryR = right[i];
   pre += tone * (.5f * (dryL + dryR) - pre); line[size_t(write)] = pre;
   const float triangle = 4.f * std::abs(phase - .5f) - 1.f, sweep = depth.getNextValue() * triangle;
   postL += tone * (read((centreMs + sweep) * perMs) - postL);
   postR += tone * (read((centreMs - sweep) * perMs) - postR);
   const float w = wet.getNextValue(), level = 1.f - .25f * w;
   left[i] = level * (dryL + w * postL); right[i] = level * (dryR + w * postR);
   phase += rate.getNextValue() / sr; if (phase >= 1.f) phase -= 1.f;
   write = (write + 1) & mask;
  }
 }

private:
 // Tuning values (see the note at the top of the file).
 static constexpr float rateI = .5f, rateII = .83f, rateBoth = 9.75f; // LFO speed of each mode, Hz
 static constexpr float depthBothMs = .22f;                           // much shallower sweep when both are on
 static constexpr float centreMs = 3.5f, depthMs = 1.85f; // delay sweeps centre +/- depth
 static constexpr double toneHz = 9000.;                  // low-pass around the delay line

 float read(float delaySamples) const {
  const float position = float(write) - delaySamples, whole = std::floor(position), fraction = position - whole;
  const int index = int(whole);
  const float first = line[size_t(index & mask)], second = line[size_t((index + 1) & mask)];
  return first + fraction * (second - first);
 }
 std::vector<float> line; int mask = 0, write = 0;
 float sr = 44100.f, tone = 1.f, phase = 0.f, pre = 0.f, postL = 0.f, postR = 0.f;
 juce::SmoothedValue<float> wet, rate, depth;
};
