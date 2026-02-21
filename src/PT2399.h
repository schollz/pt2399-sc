#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace onebitdelay {

constexpr float kPi = 3.14159265358979323846f;
constexpr double kPiD = 3.14159265358979323846;

inline float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(v, hi));
}

inline float dbToGain(float db, float minusInfDb = -96.0f) {
  if (db <= minusInfDb) return 0.0f;
  return std::pow(10.0f, db * 0.05f);
}

class Biquad {
 public:
  void reset() {
    z1_ = 0.0f;
    z2_ = 0.0f;
  }

  void setLowpass(float sampleRate, float freqHz, float q) {
    const float nyquistSafe = std::max(1.0f, sampleRate * 0.49f);
    const float f = clampf(freqHz, 1.0f, nyquistSafe);
    const float qq = std::max(1.0e-6f, q);

    const float w0 = 2.0f * kPi * f / sampleRate;
    const float cosw0 = std::cos(w0);
    const float sinw0 = std::sin(w0);
    const float alpha = sinw0 / (2.0f * qq);

    const float b0 = (1.0f - cosw0) * 0.5f;
    const float b1 = 1.0f - cosw0;
    const float b2 = (1.0f - cosw0) * 0.5f;
    const float a0 = 1.0f + alpha;
    const float a1 = -2.0f * cosw0;
    const float a2 = 1.0f - alpha;

    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
  }

  float process(float x) {
    const float y = (b0_ * x) + z1_;
    z1_ = (b1_ * x) - (a1_ * y) + z2_;
    z2_ = (b2_ * x) - (a2_ * y);
    return y;
  }

 private:
  float b0_ = 1.0f;
  float b1_ = 0.0f;
  float b2_ = 0.0f;
  float a1_ = 0.0f;
  float a2_ = 0.0f;
  float z1_ = 0.0f;
  float z2_ = 0.0f;
};

class LinearSmoother {
 public:
  void reset(float sampleRate, float seconds, float initialValue = 0.0f) {
    current_ = initialValue;
    target_ = initialValue;
    const float durationSamples = std::max(1.0f, sampleRate * std::max(0.0f, seconds));
    step_ = 1.0f / durationSamples;
  }

  void setTarget(float target) { target_ = target; }

  float next() {
    current_ += (target_ - current_) * step_;
    return current_;
  }

 private:
  float current_ = 0.0f;
  float target_ = 0.0f;
  float step_ = 1.0f;
};

class OnePoleCompressor {
 public:
  void prepare(float sampleRate) {
    sampleRate_ = std::max(1.0f, sampleRate);
    env_ = 0.0f;
    updateCoeffs();
  }

  void reset() { env_ = 0.0f; }

  float process(float x) {
    const float a = std::fabs(x);
    const float coeff = (a > env_) ? attackCoeff_ : releaseCoeff_;
    env_ = (coeff * env_) + ((1.0f - coeff) * a);

    float gain = 1.0f;
    if (env_ > thresholdLin_) {
      const float over = env_ / thresholdLin_;
      gain = std::pow(over, (1.0f / ratio_) - 1.0f);
    }
    return x * gain;
  }

 private:
  void updateCoeffs() {
    const float atkSec = std::max(1.0e-5f, attackMs_ * 0.001f);
    const float relSec = std::max(1.0e-5f, releaseMs_ * 0.001f);
    attackCoeff_ = std::exp(-1.0f / (sampleRate_ * atkSec));
    releaseCoeff_ = std::exp(-1.0f / (sampleRate_ * relSec));
  }

  float sampleRate_ = 48000.0f;
  float thresholdLin_ = dbToGain(-18.0f);
  float ratio_ = 4.0f;
  float attackMs_ = 8.0f;
  float releaseMs_ = 220.0f;
  float env_ = 0.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
};

class PT2399Core {
 public:
  explicit PT2399Core(int oversampling = 16) : osFactor_(oversampling) {}

  void prepare(double sampleRate) {
    fs_ = std::max(1.0, sampleRate);
    inputFilter2nd_.setLowpass(static_cast<float>(fs_), inputOutputFcHz_, 0.9f);
    inputPole1Alpha_ =
        1.0f - std::exp(-2.0f * kPi * inputOutputFcHz_ / static_cast<float>(fs_));

    dcBlockR_ = 1.0f - (2.0f * kPi * 10.0f / static_cast<float>(fs_));

    bitRing_.resize(44000 * osFactor_);

    updateVCO();
    updateDemodAlpha();
    updateFeedbackHpf();
    reset();
  }

  void reset() {
    i1_ = 0.0f;
    i2_ = 0.0f;
    ramPhase_ = 0.0;
    dacBit_ = 0;

    bitRing_.reset();
    zohBit_ = 0;

    ramHoldValue_ = 0.0f;
    demodState_ = 0.0f;
    demodState2_ = 0.0f;

    inputFilter2nd_.reset();
    outputFilter2nd_.reset();
    inputPole1State_ = 0.0f;

    dcBlockX1_ = 0.0f;
    dcBlockY1_ = 0.0f;

    feedbackSample_ = 0.0f;
    feedbackHpfX1_ = 0.0f;
    feedbackHpfY1_ = 0.0f;

    prevInput_ = 0.0f;

    rngState_ = 0x12345678u;
  }

  void setDelayResistanceKOhm(float rK) {
    rKOhm_ = clampf(rK, 0.5f, 100.0f);
    updateVCO();
    updateDemodAlpha();
  }

  void setFeedback(float gain) { feedbackGain_ = clampf(gain, 0.0f, 2.0f); }

  void setFeedbackHighPassHz(float hz) {
    feedbackHpfHz_ = clampf(hz, 10.0f, 440.0f);
    updateFeedbackHpf();
  }

  void setC3nF(float nF) {
    c3nF_ = clampf(nF, 22.0f, 150.0f);
    integGain_ = 100.0f / c3nF_;
  }

  void setC6nF(float nF) {
    c6nF_ = clampf(nF, 22.0f, 150.0f);
    updateDemodAlpha();
  }

  void setBrightness(float amount) {
    brightness_ = clampf(amount, 0.0f, 1.0f);
    inputOutputFcHz_ = interpLog(kBaseInputOutputFcHz, kMaxInputOutputFcHz, brightness_);
    demodFcScale_ = interpLog(kBaseDemodFcScale, kMaxDemodFcScale, brightness_);
    updateOutputFilter();
    updateDemodAlpha();
    inputFilter2nd_.setLowpass(static_cast<float>(fs_), inputOutputFcHz_, 0.9f);
    inputPole1Alpha_ =
        1.0f - std::exp(-2.0f * kPi * inputOutputFcHz_ / static_cast<float>(fs_));
  }

  void setBoostActivated(bool enabled) {
    boostActivated_ = enabled;
    updateVCO();
  }

  float processSample(float input) {
    float summed = input + feedbackSample_ * feedbackGain_;

    float filtered = inputFilter2nd_.process(summed);
    inputPole1State_ += inputPole1Alpha_ * (filtered - inputPole1State_);
    filtered = inputPole1State_;

    filtered = softClip(filtered, clipDrive_);

    float acc = 0.0f;
    int n = 0;

    const double phaseStep = dsmClockHz_ / fs_;
    ramPhase_ += phaseStep;

    while (ramPhase_ >= 1.0) {
      ramPhase_ -= 1.0;

      float t = static_cast<float>(1.0 - ramPhase_ / phaseStep);
      t = clampf(t, 0.0f, 1.0f);
      const float interpInput = prevInput_ + t * (filtered - prevInput_);

      runDeltaSigmaTick(interpInput);

      acc += demodState_;
      ++n;
    }

    prevInput_ = filtered;

    float wet = (n > 0) ? (acc / static_cast<float>(n)) : demodState_;

    wet = outputFilter2nd_.process(wet);

    const float dcOut = wet - dcBlockX1_ + dcBlockR_ * dcBlockY1_;
    dcBlockX1_ = wet;
    dcBlockY1_ = dcOut;
    wet = dcOut;

    const float wetOut = wet * outputLevelTrim_;

    feedbackSample_ = processFeedbackHpf(wet * feedbackCompensation_);

    return wetOut;
  }

 private:
  struct BitRing {
    std::vector<uint32_t> data;
    int numBits = 0;
    int writePos = 0;

    void resize(int bits) {
      numBits = std::max(1, bits);
      const int numWords = (numBits + 31) / 32;
      data.assign(static_cast<size_t>(numWords), 0u);
      writePos = 0;
    }

    void reset() {
      std::fill(data.begin(), data.end(), 0u);
      writePos = 0;
    }

    int readOldest() const {
      return static_cast<int>((data[static_cast<size_t>(writePos >> 5)] >> (writePos & 31)) & 1u);
    }

    void writeBit(int b) {
      const int word = writePos >> 5;
      const int bit = writePos & 31;

      if (b)
        data[static_cast<size_t>(word)] |= (1u << bit);
      else
        data[static_cast<size_t>(word)] &= ~(1u << bit);

      if (++writePos >= numBits) writePos = 0;
    }
  };

  static float softClip(float x, float drive) {
    const float d = std::max(1.0e-3f, drive);
    return std::tanh(d * x) / d;
  }

  static float interpLog(float a, float b, float t) {
    const float safeA = std::max(1.0e-6f, a);
    const float safeB = std::max(1.0e-6f, b);
    return safeA * std::pow(safeB / safeA, clampf(t, 0.0f, 1.0f));
  }

  float processFeedbackHpf(float x) {
    const float y = feedbackHpfA_ * (feedbackHpfY1_ + x - feedbackHpfX1_);
    feedbackHpfX1_ = x;
    feedbackHpfY1_ = y;
    return y;
  }

  float nextDither() {
    constexpr float kDitherAmt = 0.02f;
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    const float u1 = static_cast<float>(static_cast<int32_t>(rngState_)) * (1.0f / 2147483648.0f);
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    const float u2 = static_cast<float>(static_cast<int32_t>(rngState_)) * (1.0f / 2147483648.0f);
    return (u1 + u2) * 0.5f * kDitherAmt;
  }

  void runDeltaSigmaTick(float input) {
    constexpr float kDacLevel = 0.7f;
    constexpr float k1 = 0.8f;
    constexpr float k2 = 0.4f;
    constexpr float kLeak1 = 0.9995f;
    constexpr float kLeak2 = 0.9990f;

    const float dacFb = dacBit_ ? kDacLevel : -kDacLevel;
    const float error = input * inScale_ - dacFb + nextDither();

    i1_ = (i1_ + error * k1 * integGain_) * kLeak1;
    i2_ = (i2_ + i1_ * k2) * kLeak2;

    dacBit_ = (i2_ >= 0.0f) ? 1 : 0;

    const int oldBit = bitRing_.readOldest();
    bitRing_.writeBit(dacBit_);

    zohBit_ = oldBit;
    ramHoldValue_ = zohBit_ ? kDacLevel : -kDacLevel;

    demodState_ += demodAlphaTick_ * (ramHoldValue_ - demodState_);
    demodState2_ += demodAlphaTick_ * (demodState_ - demodState2_);
  }

  void updateVCO() {
    const float delayMs = 11.46f * rKOhm_ + 29.7f;
    const double fVcoHz = 683.21 / static_cast<double>(delayMs) * 1.0e6;
    const double fRamHz = fVcoHz / 15.5;
    dsmClockHz_ = fRamHz * osFactor_;

    const float delayNorm = (delayMs - 31.0f) / (346.0f - 31.0f);
    inScale_ = 0.68f - 0.08f * delayNorm;
    feedbackCompensation_ = 1.0f / std::max(0.06f, inScale_);
    clipDrive_ = boostActivated_ ? (1.0f + 2.0f * delayNorm) : (0.50f + 0.90f * delayNorm);

    updateOutputFilter();
  }

  void updateOutputFilter() {
    outputFilter2nd_.setLowpass(static_cast<float>(fs_), inputOutputFcHz_, 0.707f);
  }

  void updateDemodAlpha() {
    const float fc = demodFcScale_ / c6nF_;
    if (dsmClockHz_ > 0.0) {
      const double arg = -2.0 * kPiD * static_cast<double>(fc) / dsmClockHz_;
      demodAlphaTick_ = 1.0f - static_cast<float>(std::exp(arg));
    } else {
      demodAlphaTick_ = 1.0f;
    }
  }

  void updateFeedbackHpf() {
    if (fs_ <= 0.0) {
      feedbackHpfA_ = 1.0f;
      return;
    }

    const float rc = 1.0f / (2.0f * kPi * feedbackHpfHz_);
    const float dt = 1.0f / static_cast<float>(fs_);
    feedbackHpfA_ = rc / (rc + dt);
  }

  int osFactor_;
  double fs_ = 48000.0;

  static constexpr float kBaseInputOutputFcHz = 7000.0f;
  static constexpr float kMaxInputOutputFcHz = 14000.0f;
  static constexpr float kBaseDemodFcScale = 220000.0f;
  static constexpr float kMaxDemodFcScale = 250000.0f;

  float brightness_ = 0.0f;
  float inputOutputFcHz_ = kBaseInputOutputFcHz;
  float demodFcScale_ = kBaseDemodFcScale;
  float inScale_ = 0.6f;
  float feedbackCompensation_ = 1.0f / 0.6f;
  float clipDrive_ = 1.5f;
  bool boostActivated_ = false;
  float outputLevelTrim_ = 1.45f;

  float feedbackHpfHz_ = 10.0f;
  float feedbackHpfA_ = 1.0f;
  float feedbackHpfX1_ = 0.0f;
  float feedbackHpfY1_ = 0.0f;

  float rKOhm_ = 10.0f;
  double dsmClockHz_ = 305484.0 * 8;
  double ramPhase_ = 0.0;

  float i1_ = 0.0f;
  float i2_ = 0.0f;
  float integGain_ = 1.0f;
  int dacBit_ = 0;

  float c3nF_ = 100.0f;
  float c6nF_ = 100.0f;

  BitRing bitRing_;
  int zohBit_ = 0;

  float ramHoldValue_ = 0.0f;
  float demodState_ = 0.0f;
  float demodState2_ = 0.0f;
  float demodAlphaTick_ = 0.01f;

  Biquad inputFilter2nd_;
  float inputPole1Alpha_ = 0.0f;
  float inputPole1State_ = 0.0f;

  Biquad outputFilter2nd_;

  float dcBlockR_ = 0.999f;
  float dcBlockX1_ = 0.0f;
  float dcBlockY1_ = 0.0f;

  float feedbackGain_ = 0.0f;
  float feedbackSample_ = 0.0f;

  float prevInput_ = 0.0f;

  uint32_t rngState_ = 0x12345678u;
};

}  // namespace onebitdelay
