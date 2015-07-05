#include <cmath>

#include "ATKSideChainCompressor.h"
#include "IPlug_include_in_plug_src.h"
#include "IControl.h"
#include "controls.h"
#include "resource.h"

const int kNumPrograms = 2;

enum EParams
{
  kLF = 0,
  kHF,
  kAttack,
  kRelease,
  kThreshold,
  kSlope,
  kSoftness,
  kMakeup,
  kDryWet,
  kNumParams
};

enum ELayout
{
  kWidth = GUI_WIDTH,
  kHeight = GUI_HEIGHT,

  kLFX = 25,
  kLFY = 40,
  kHFX = 128,
  kHFY = 40,
  kAttackX = 231,
  kAttackY = 40,
  kReleaseX = 334,
  kReleaseY = 40,
  kThresholdX = 437,
  kThresholdY = 40,
  kSlopeX = 540,
  kSlopeY = 40,
  kSoftnessX = 643,
  kSoftnessY = 40,
  kMakeupX = 746,
  kMakeupY = 40,
  kDryWetX = 849,
  kDryWetY = 40,

  kKnobFrames = 20,
  kKnobFrames1 = 19
};

ATKSideChainCompressor::ATKSideChainCompressor(IPlugInstanceInfo instanceInfo)
  :	IPLUG_CTOR(kNumParams, kNumPrograms, instanceInfo),
    inFilter(NULL, 1, 0, false), inSideChainFilter(NULL, 1, 0, false), outFilter(NULL, 1, 0, false)
{
  TRACE;

  //arguments are: name, defaultVal, minVal, maxVal, step, label
  GetParam(kLF)->InitDouble("Low cut", 100., 20., 1000.0, 1, "Hz");
  GetParam(kLF)->SetShape(2.);
  GetParam(kHF)->InitDouble("High cut", 10000, 100., 2000.0, 1, "Hz");
  GetParam(kHF)->SetShape(2.);
  GetParam(kAttack)->InitDouble("Attack", 10., 1., 100.0, 0.1, "ms");
  GetParam(kAttack)->SetShape(2.);
  GetParam(kRelease)->InitDouble("Release", 10, 1., 100.0, 0.1, "ms");
  GetParam(kRelease)->SetShape(2.);
  GetParam(kThreshold)->InitDouble("Threshold", 0., -40., 0.0, 0.1, "dB"); // threshold is actually power
  GetParam(kThreshold)->SetShape(2.);
  GetParam(kSlope)->InitDouble("Slope", 2., .1, 100, .1, "-");
  GetParam(kSlope)->SetShape(2.);
  GetParam(kSoftness)->InitDouble("Softness", -2, -4, 0, 0.1, "-");
  GetParam(kSoftness)->SetShape(2.);
  GetParam(kMakeup)->InitDouble("Makeup Gain", 0, 0, 40, 0.1, "-"); // Makeup is expressed in amplitude
  GetParam(kMakeup)->SetShape(2.);
  GetParam(kDryWet)->InitDouble("Dry/Wet", 1, 0, 1, 0.01, "-");
  GetParam(kDryWet)->SetShape(1.);

  IGraphics* pGraphics = MakeGraphics(this, kWidth, kHeight);
  pGraphics->AttachBackground(COMPRESSOR_ID, COMPRESSOR_FN);

  IBitmap knob = pGraphics->LoadIBitmap(KNOB_ID, KNOB_FN, kKnobFrames);
  IBitmap knob1 = pGraphics->LoadIBitmap(KNOB1_ID, KNOB1_FN, kKnobFrames1);
  IColor color = IColor(255, 255, 255, 255);
  IText text = IText(10, &color, nullptr, IText::kStyleBold);

  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kLFX, kLFY, kLFX + 78, kLFY + 78 + 21), kLF, &knob, &text, "Hz"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kHFX, kHFY, kHFX + 78, kHFY + 78 + 21), kHF, &knob, &text, "Hz"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kAttackX, kAttackY, kAttackX + 78, kAttackY + 78 + 21), kAttack, &knob, &text, "ms"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kReleaseX, kReleaseY, kReleaseX + 78, kReleaseY + 78 + 21), kRelease, &knob, &text, "ms"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kThresholdX, kThresholdY, kThresholdX + 78, kThresholdY + 78 + 21), kThreshold, &knob, &text, "dB"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kSlopeX, kSlopeY, kSlopeX + 78, kSlopeY + 78 + 21), kSlope, &knob, &text, ""));
  pGraphics->AttachControl(new IKnobMultiControl(this, kSoftnessX, kSoftnessY, kSoftness, &knob));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kMakeupX, kMakeupY, kMakeupX + 78, kMakeupY + 78 + 21), kMakeup, &knob, &text, "dB"));
  pGraphics->AttachControl(new IKnobMultiControl(this, kDryWetX, kDryWetY, kDryWet, &knob1));

  AttachGraphics(pGraphics);

  if (GetAPI() == kAPIVST2) // for VST2 we name individual outputs
  {
    SetInputLabel(0, "main input");
    SetInputLabel(1, "sc input");
    SetOutputLabel(0, "output");
  }
  else // for AU and VST3 we name buses
  {
    SetInputBusLabel(0, "main input");
    SetInputBusLabel(1, "sc input");
    SetOutputBusLabel(0, "output");
  }

  MakePreset("Serial Compression", 10., 10., 0., 2., -2., 0., 0.);
  MakePreset("Parallel Compression", 10., 10., 0., 2., -2., 0., 0.5);
  
  iirFilter.set_order(4);
  iirFilter.set_input_port(0, &inSideChainFilter, 0);
  powerFilter.set_input_port(0, &iirFilter, 0);
  gainCompressorFilter.set_input_port(0, &powerFilter, 0);
  attackReleaseFilter.set_input_port(0, &gainCompressorFilter, 0);
  applyGainFilter.set_input_port(0, &attackReleaseFilter, 0);
  applyGainFilter.set_input_port(1, &inFilter, 0);
  volumeFilter.set_input_port(0, &applyGainFilter, 0);
  drywetFilter.set_input_port(0, &volumeFilter, 0);
  drywetFilter.set_input_port(1, &inFilter, 0);
  outFilter.set_input_port(0, &drywetFilter, 0);
  
  Reset();
}

ATKSideChainCompressor::~ATKSideChainCompressor() {}

void ATKSideChainCompressor::ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames)
{
  // Mutex is already locked for us.

  inFilter.set_pointer(inputs[0], nFrames);
  if (IsInChannelConnected(1))
  {
    inSideChainFilter.set_pointer(inputs[1], nFrames);
  }
  else
  {
    inSideChainFilter.set_pointer(inputs[0], nFrames);
  }
  outFilter.set_pointer(outputs[0], nFrames);
  outFilter.process(nFrames);
}

void ATKSideChainCompressor::Reset()
{
  TRACE;
  IMutexLock lock(this);
  
  int sampling_rate = GetSampleRate();
  
  inFilter.set_input_sampling_rate(sampling_rate);
  inFilter.set_output_sampling_rate(sampling_rate);
  inSideChainFilter.set_input_sampling_rate(sampling_rate);
  inSideChainFilter.set_output_sampling_rate(sampling_rate);
  iirFilter.set_input_sampling_rate(sampling_rate);
  iirFilter.set_output_sampling_rate(sampling_rate);
  powerFilter.set_input_sampling_rate(sampling_rate);
  powerFilter.set_output_sampling_rate(sampling_rate);
  attackReleaseFilter.set_input_sampling_rate(sampling_rate);
  attackReleaseFilter.set_output_sampling_rate(sampling_rate);
  gainCompressorFilter.set_input_sampling_rate(sampling_rate);
  gainCompressorFilter.set_output_sampling_rate(sampling_rate);
  applyGainFilter.set_input_sampling_rate(sampling_rate);
  applyGainFilter.set_output_sampling_rate(sampling_rate);
  volumeFilter.set_input_sampling_rate(sampling_rate);
  volumeFilter.set_output_sampling_rate(sampling_rate);
  drywetFilter.set_input_sampling_rate(sampling_rate);
  drywetFilter.set_output_sampling_rate(sampling_rate);
  outFilter.set_input_sampling_rate(sampling_rate);
  outFilter.set_output_sampling_rate(sampling_rate);

  auto lf = GetParam(kLF)->Value();
  auto hf = GetParam(kHF)->Value();
  iirFilter.set_cut_frequencies(std::min(lf, hf), std::max(lf, hf));
  powerFilter.set_memory(std::exp(-1 / 1e-3 * GetSampleRate()));
  attackReleaseFilter.set_release(std::exp(-1 / (GetParam(kAttack)->Value() * 1e-3 * GetSampleRate()))); // in ms
  attackReleaseFilter.set_attack(std::exp(-1 / (GetParam(kRelease)->Value() * 1e-3 * GetSampleRate()))); // in ms
}

void ATKSideChainCompressor::OnParamChange(int paramIdx)
{
  IMutexLock lock(this);

  switch (paramIdx)
  {
  case kLF:
  case kHF:
  {
    auto lf = GetParam(kLF)->Value();
    auto hf = GetParam(kHF)->Value();
    iirFilter.set_cut_frequencies(std::min(lf, hf), std::max(lf, hf));
    break;
  }
  case kThreshold:
    gainCompressorFilter.set_threshold(std::pow(10, GetParam(kThreshold)->Value() / 10));
    break;
  case kSlope:
    gainCompressorFilter.set_ratio(GetParam(kSlope)->Value());
    break;
  case kSoftness:
    gainCompressorFilter.set_softness(std::pow(10, GetParam(kSoftness)->Value()));
    break;
  case kAttack:
    attackReleaseFilter.set_release(std::exp(-1 / (GetParam(kAttack)->Value() * 1e-3 * GetSampleRate()))); // in ms
    break;
  case kRelease:
    attackReleaseFilter.set_attack(std::exp(-1 / (GetParam(kRelease)->Value() * 1e-3 * GetSampleRate()))); // in ms
    break;
  case kMakeup:
    volumeFilter.set_volume_db(GetParam(kMakeup)->Value());
    break;
  case kDryWet:
    drywetFilter.set_dry(GetParam(kDryWet)->Value());
    break;

  default:
    break;
  }
}
