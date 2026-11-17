#pragma once
#include "Reverbs.h" // for orbitfx::Line

// Stereo echo voiced after the classic bucket-brigade delay pedals with built-in chorus / vibrato:
//  - repeats are dark and round: two low-pass stages in the loop, and the longer the delay time the darker
//    they get, as a bucket-brigade line clocked more slowly loses more treble;
//  - the input and the loop saturate softly, so the echo has a little grit and high FEEDBACK thickens,
//    up to a self-oscillation that stays under control;
//  - MOD wobbles the delay time: low settings give a slow chorus on the repeats, high settings a faster vibrato;
//  - turning TIME glides the pitch of the echoes;
//  - the two sides are modulated in opposite directions and feed each other a little, for width.
// An original model: every value below is a tuning choice to adjust by ear, not a measurement of any unit.
class CharacterDelay {
public:
 void prepare(double sampleRate) {
  sr = float(sampleRate);
  left.prepare(int(4.1 * sampleRate) + 64); right.prepare(int(4.1 * sampleRate) + 64);
  highPass = coefficient(loopHighPassHz); lowPass = coefficient(brightHz);
  time.reset(sampleRate, .3); time.setCurrentAndTargetValue(.3f * sr); depth.reset(sampleRate, .1); depth.setCurrentAndTargetValue(0.f);
  wet.reset(sampleRate, .02);
  reset();
 }
 void reset() { wet.setCurrentAndTargetValue(0.f); left.clear(); right.clear(); for (auto* state : {&a1, &a2, &b1, &b2, &hpL, &hpR}) *state = 0; phase = 0; wow = 0; }
 void process(juce::AudioBuffer<float>& buffer, float seconds, float feedback, float mix, float modulation) {
  if (buffer.getNumChannels() < 2) return;
  seconds = juce::jlimit(.01f, 4.f, seconds); modulation = juce::jlimit(0.f, 1.f, modulation);
  wet.setTargetValue(juce::jlimit(0.f, 1.f, mix));
  time.setTargetValue(seconds * sr); depth.setTargetValue(modDepthMs * .001f * sr * std::sqrt(modulation));
  lowPass = coefficient(brightHz - (brightHz - darkHz) * juce::jlimit(0.f, 1.f, (seconds - .03f) / .5f));
  const float rate = slowHz + (fastHz - slowHz) * modulation * modulation;
  auto* l = buffer.getWritePointer(0); auto* r = buffer.getWritePointer(1);
  for (int i = 0; i < buffer.getNumSamples(); ++i) {
   phase += rate / sr; if (phase >= 1.f) phase -= 1.f; wow += wowHz / sr; if (wow >= 1.f) wow -= 1.f;
   const float drift = depth.getNextValue() * sine(phase) + wowMs * .001f * sr * sine(wow), t = time.getNextValue();
   const float echoL = left.tap(juce::jmax(2.f, t + drift)), echoR = right.tap(juce::jmax(2.f, t * rightStretch - drift));
   a1 += lowPass * (echoL - a1); a2 += lowPass * (a1 - a2); hpL += highPass * (a2 - hpL);
   b1 += lowPass * (echoR - b1); b2 += lowPass * (b1 - b2); hpR += highPass * (b2 - hpR);
   const float toneL = a2 - hpL, toneR = b2 - hpR;
   left.write(soft(l[i] + feedback * ((1.f - cross) * toneL + cross * toneR)));
   right.write(soft(r[i] + feedback * ((1.f - cross) * toneR + cross * toneL)));
   const float wetLevel = wet.getNextValue();
   l[i] += wetLevel * toneL; r[i] += wetLevel * toneR;
  }
 }
private:
 static constexpr float brightHz = 3400.f, darkHz = 2100.f;   // loop low-pass at short and at long delay times
 static constexpr float loopHighPassHz = 90.f;                // keeps the repeats from getting boomy
 static constexpr float modDepthMs = 1.2f, slowHz = .6f, fastHz = 4.6f; // MOD: depth, and speed from chorus to vibrato
 static constexpr float wowHz = .31f, wowMs = .2f;            // a trace of drift even with MOD at zero
 static constexpr float rightStretch = 1.012f, cross = .15f, saturation = 1.5f;
 static float sine(float p) { const float x = 2.f * p - 1.f; return 4.f * x * (1.f - std::abs(x)); }
 static float soft(float x) { return std::tanh(saturation * x) / saturation; }
 float coefficient(float hz) const { return 1.f - std::exp(-juce::MathConstants<float>::twoPi * hz / sr); }
 orbitfx::Line left, right; juce::SmoothedValue<float> time, depth, wet;
 float sr = 44100.f, lowPass = .3f, highPass = .01f, a1 = 0, a2 = 0, b1 = 0, b2 = 0, hpL = 0, hpR = 0, phase = 0, wow = 0;
};
