#include "PT2399.h"

#include "SC_PlugIn.h"

#include <cmath>
#include <new>

static InterfaceTable* ft;

namespace {

enum InputIndex {
  kInAudio = 0,
  kInputLevel = 1,
  kDryLevel = 2,
  kWetLevel = 3,
  kDelayTime = 4,
  kFeedbackHpf = 5,
  kFeedback = 6,
  kC3 = 7,
  kC6 = 8,
  kBrightness = 9,
  kBoostActivated = 10,
  kPassthrough = 11,
  kOversample = 12,
};

inline float readInputAtSample(const Unit* unit, int index, int sampleIndex) {
  if (INRATE(index) == calc_FullRate) {
    return IN(index)[sampleIndex];
  }
  return IN0(index);
}

inline int oversampleFactorFromIndex(float raw) {
  const int idx = static_cast<int>(std::lround(raw));
  switch (idx) {
    case 0:
      return 1;
    case 1:
      return 2;
    case 3:
      return 4;
    case 4:
      return 8;
    case 5:
      return 16;
    case 6:
      return 32;
    default:
      return 16;
  }
}

inline float delayMsToResistanceKOhm(float delayMs) {
  const float clampedMs = onebitdelay::clampf(delayMs, 35.0f, 1175.0f);
  return (clampedMs - 29.7f) / 11.46f;
}

struct PT2399 : public Unit {
  onebitdelay::PT2399Core* core = nullptr;
  onebitdelay::OnePoleCompressor* compressor = nullptr;
  onebitdelay::LinearSmoother* inGain = nullptr;
  onebitdelay::LinearSmoother* dryGain = nullptr;
  onebitdelay::LinearSmoother* wetGain = nullptr;
  onebitdelay::LinearSmoother* passthrough = nullptr;
};

void PT2399_next(PT2399* unit, int inNumSamples) {
  if (unit->core == nullptr || unit->compressor == nullptr || unit->inGain == nullptr ||
      unit->dryGain == nullptr || unit->wetGain == nullptr || unit->passthrough == nullptr) {
    ClearUnitOutputs(unit, inNumSamples);
    return;
  }

  const float* in = IN(kInAudio);
  float* out = OUT(0);

  for (int i = 0; i < inNumSamples; ++i) {
    const float inLevelDb = readInputAtSample(unit, kInputLevel, i);
    const float dryLevelDb = readInputAtSample(unit, kDryLevel, i);
    const float wetLevelDb = readInputAtSample(unit, kWetLevel, i);
    const float delayTimeMs = readInputAtSample(unit, kDelayTime, i);
    const float feedbackHpf = readInputAtSample(unit, kFeedbackHpf, i);
    const float feedback = readInputAtSample(unit, kFeedback, i);
    const float c3 = readInputAtSample(unit, kC3, i);
    const float c6 = readInputAtSample(unit, kC6, i);
    const float brightness = readInputAtSample(unit, kBrightness, i);
    const float boostActivated = readInputAtSample(unit, kBoostActivated, i);
    const float passthrough = readInputAtSample(unit, kPassthrough, i);

    unit->core->setBrightness(onebitdelay::clampf(brightness * 0.01f, 0.0f, 1.0f));
    unit->core->setBoostActivated(!(boostActivated >= 0.5f));
    unit->core->setFeedbackHighPassHz(feedbackHpf);
    unit->core->setDelayResistanceKOhm(delayMsToResistanceKOhm(delayTimeMs));
    unit->core->setFeedback(feedback);
    unit->core->setC3nF(c3);
    unit->core->setC6nF(c6);

    unit->inGain->setTarget(onebitdelay::dbToGain(inLevelDb));
    unit->dryGain->setTarget(onebitdelay::dbToGain(dryLevelDb));
    unit->wetGain->setTarget(onebitdelay::dbToGain(wetLevelDb));
    unit->passthrough->setTarget(passthrough >= 0.5f ? 1.0f : 0.0f);

    const float dry = in[i];
    const float pre = dry * unit->inGain->next();

    float wet = unit->core->processSample(pre);
    wet = unit->compressor->process(wet);

    const float fx = (unit->dryGain->next() * dry) + (unit->wetGain->next() * wet);
    const float passMix = unit->passthrough->next();

    out[i] = fx + ((dry - fx) * passMix);
  }
}

void PT2399_Ctor(PT2399* unit) {
  const int osFactor = oversampleFactorFromIndex(IN0(kOversample));
  unit->core = new (std::nothrow) onebitdelay::PT2399Core(osFactor);
  unit->compressor = new (std::nothrow) onebitdelay::OnePoleCompressor();
  unit->inGain = new (std::nothrow) onebitdelay::LinearSmoother();
  unit->dryGain = new (std::nothrow) onebitdelay::LinearSmoother();
  unit->wetGain = new (std::nothrow) onebitdelay::LinearSmoother();
  unit->passthrough = new (std::nothrow) onebitdelay::LinearSmoother();

  if (unit->core == nullptr || unit->compressor == nullptr || unit->inGain == nullptr ||
      unit->dryGain == nullptr || unit->wetGain == nullptr || unit->passthrough == nullptr) {
    delete unit->core;
    delete unit->compressor;
    delete unit->inGain;
    delete unit->dryGain;
    delete unit->wetGain;
    delete unit->passthrough;
    unit->core = nullptr;
    unit->compressor = nullptr;
    unit->inGain = nullptr;
    unit->dryGain = nullptr;
    unit->wetGain = nullptr;
    unit->passthrough = nullptr;
    SETCALC(PT2399_next);
    ClearUnitOutputs(unit, 1);
    return;
  }

  unit->core->prepare(SAMPLERATE);

  unit->compressor->prepare(SAMPLERATE);
  unit->compressor->reset();

  unit->inGain->reset(SAMPLERATE, 0.2f, onebitdelay::dbToGain(IN0(kInputLevel)));
  unit->dryGain->reset(SAMPLERATE, 0.2f, onebitdelay::dbToGain(IN0(kDryLevel)));
  unit->wetGain->reset(SAMPLERATE, 0.2f, onebitdelay::dbToGain(IN0(kWetLevel)));
  unit->passthrough->reset(SAMPLERATE, 0.1f, IN0(kPassthrough) >= 0.5f ? 1.0f : 0.0f);

  SETCALC(PT2399_next);
  PT2399_next(unit, 1);
}

void PT2399_Dtor(PT2399* unit) {
  delete unit->core;
  delete unit->compressor;
  delete unit->inGain;
  delete unit->dryGain;
  delete unit->wetGain;
  delete unit->passthrough;
  unit->core = nullptr;
  unit->compressor = nullptr;
  unit->inGain = nullptr;
  unit->dryGain = nullptr;
  unit->wetGain = nullptr;
  unit->passthrough = nullptr;
}

}  // namespace

PluginLoad(PT2399UGens) {
  ft = inTable;
  DefineDtorUnit(PT2399);
}
