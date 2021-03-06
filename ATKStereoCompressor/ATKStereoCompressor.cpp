#include <cmath>

#include "ATKStereoCompressor.h"
#include "IPlug_include_in_plug_src.h"
#include "IControl.h"
#include "controls.h"
#include "resource.h"

const int kNumPrograms = 1;

enum EParams
{
  kMiddleside = 0, // do we process data with middle/side or plain L/R ?
  kLinkChannels,
  kActivateChannel1,
  kActivateChannel2,
  kAttack1,
  kRelease1,
  kThreshold1,
  kRatio1,
  kSoftness1,
  kMakeup1,
  kAttack2,
  kRelease2,
  kThreshold2,
  kRatio2,
  kSoftness2,
  kMakeup2,
  kDryWet,
  kNumParams
};

enum ELayout
{
  kWidth = GUI_WIDTH,
  kHeight = GUI_HEIGHT,

  kMiddlesideX = 25,
  kMiddlesideY = 36,
  kLinkChannelsX = 25,
  kLinkChannelsY = 121,

  kActivateChannel1X = 94,
  kActivateChannel1Y = 36,
  kActivateChannel2X = 94,
  kActivateChannel2Y = 121,

  kAttack1X = 163,
  kAttack1Y = 26,
  kRelease1X = 232,
  kRelease1Y = 26,
  kThreshold1X = 301,
  kThreshold1Y = 26,
  kRatio1X = 370,
  kRatio1Y = 26,
  kSoftness1X = 439,
  kSoftness1Y = 26,
  kMakeup1X = 508,
  kMakeup1Y = 26,

  kAttack2X = 163,
  kAttack2Y = 111,
  kRelease2X = 232,
  kRelease2Y = 111,
  kThreshold2X = 301,
  kThreshold2Y = 111,
  kRatio2X = 370,
  kRatio2Y = 111,
  kSoftness2X = 439,
  kSoftness2Y = 111,
  kMakeup2X = 508,
  kMakeup2Y = 111,

  kDryWetX = 577,
  kDryWetY = 68,

  kKnobFrames = 43
};

ATKStereoCompressor::ATKStereoCompressor(IPlugInstanceInfo instanceInfo)
  :	IPLUG_CTOR(kNumParams, kNumPrograms, instanceInfo),
  inLFilter(NULL, 1, 0, false), inRFilter(NULL, 1, 0, false), volumesplitFilter(2), applyGainFilter(2), volumemergeFilter(2), drywetFilter(2), outLFilter(NULL, 1, 0, false), outRFilter(NULL, 1, 0, false)
{
  TRACE;

  //arguments are: name, defaultVal, minVal, maxVal, step, label
  GetParam(kMiddleside)->InitBool("Middle/Side processing", 0, "");
  GetParam(kLinkChannels)->InitBool("Link channels", 0, "");
  GetParam(kActivateChannel1)->InitBool("Enable Channel 1", 1, "");
  GetParam(kActivateChannel2)->InitBool("Enable Channel 2", 1, "");

  GetParam(kAttack1)->InitDouble("Attack ch1", 10., 1., 100.0, 0.1, "ms");
  GetParam(kAttack1)->SetShape(2.);
  GetParam(kRelease1)->InitDouble("Release ch1", 10, 1., 100.0, 0.1, "ms");
  GetParam(kRelease1)->SetShape(2.);
  GetParam(kThreshold1)->InitDouble("Threshold ch1", 0., -40., 0.0, 0.1, "dB"); // threshold is actually power
  GetParam(kThreshold1)->SetShape(2.);
  GetParam(kRatio1)->InitDouble("Ratio ch1", 2., 1, 100, 1, "-");
  GetParam(kRatio1)->SetShape(2.);
  GetParam(kSoftness1)->InitDouble("Softness ch1", -2, -4, 0, 0.1, "-");
  GetParam(kSoftness1)->SetShape(2.);
  GetParam(kMakeup1)->InitDouble("Makeup Gain ch1", 0, 0, 40, 0.1, "-"); // Makeup is expressed in amplitude
  GetParam(kMakeup1)->SetShape(2.);
  GetParam(kAttack2)->InitDouble("Attack ch2", 10., 1., 100.0, 0.1, "ms");
  GetParam(kAttack2)->SetShape(2.);
  GetParam(kRelease2)->InitDouble("Release ch2", 10, 1., 100.0, 0.1, "ms");
  GetParam(kRelease2)->SetShape(2.);
  GetParam(kThreshold2)->InitDouble("Threshold ch2", 0., -40., 0.0, 0.1, "dB"); // threshold is actually power
  GetParam(kThreshold2)->SetShape(2.);
  GetParam(kRatio2)->InitDouble("Ratio ch2", 2., 1, 100, 1, "-");
  GetParam(kRatio2)->SetShape(2.);
  GetParam(kSoftness2)->InitDouble("Softness ch2", -2, -4, 0, 0.1, "-");
  GetParam(kSoftness2)->SetShape(2.);
  GetParam(kMakeup2)->InitDouble("Makeup Gain ch2", 0, 0, 40, 0.1, "-"); // Makeup is expressed in amplitude
  GetParam(kMakeup2)->SetShape(2.);
  GetParam(kDryWet)->InitDouble("Dry/Wet", 1, 0, 1, 0.01, "-");
  GetParam(kDryWet)->SetShape(1.);

  IGraphics* pGraphics = MakeGraphics(this, kWidth, kHeight);
  pGraphics->AttachBackground(STEREO_COMPRESSOR_ID, STEREO_COMPRESSOR_FN);

  IBitmap knob = pGraphics->LoadIBitmap(KNOB_ID, KNOB_FN, kKnobFrames);
  IBitmap knob1 = pGraphics->LoadIBitmap(KNOB1_ID, KNOB1_FN, kKnobFrames);
  IBitmap myswitch = pGraphics->LoadIBitmap(SWITCH_ID, SWITCH_FN, 2);
  IText text = IText(10, 0, 0, IText::kStyleBold);
  
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kAttack1X, kAttack1Y, kAttack1X + 43, kAttack1Y + 43 + 21), kAttack1, &knob, &text, "ms"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kRelease1X, kRelease1Y, kRelease1X + 43, kRelease1Y + 43 + 21), kRelease1, &knob, &text, "ms"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kThreshold1X, kThreshold1Y, kThreshold1X + 43, kThreshold1Y + 43 + 21), kThreshold1, &knob, &text, "dB"));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kRatio1X, kRatio1Y, kRatio1X + 43, kRatio1Y + 43 + 21), kRatio1, &knob, &text, ""));
  pGraphics->AttachControl(new IKnobMultiControl(this, kSoftness1X, kSoftness1Y, kSoftness1, &knob));
  pGraphics->AttachControl(new IKnobMultiControlText(this, IRECT(kMakeup1X, kMakeup1Y, kMakeup1X + 43, kMakeup1Y + 43 + 21), kMakeup1, &knob, &text, "dB"));

  attack2 = new IKnobMultiControlText(this, IRECT(kAttack2X, kAttack2Y, kAttack2X + 43, kAttack2Y + 43 + 21), kAttack2, &knob, &text, "ms");
  pGraphics->AttachControl(attack2);
  release2 = new IKnobMultiControlText(this, IRECT(kRelease2X, kRelease2Y, kRelease2X + 43, kRelease2Y + 43 + 21), kRelease2, &knob, &text, "ms");
  pGraphics->AttachControl(release2);
  threshold2 = new IKnobMultiControlText(this, IRECT(kThreshold2X, kThreshold2Y, kThreshold2X + 43, kThreshold2Y + 43 + 21), kThreshold2, &knob, &text, "dB");
  pGraphics->AttachControl(threshold2);
  ratio2 = new IKnobMultiControlText(this, IRECT(kRatio2X, kRatio2Y, kRatio2X + 43, kRatio2Y + 43 + 21), kRatio2, &knob, &text, "");
  pGraphics->AttachControl(ratio2);
  softness2 = new IKnobMultiControl(this, kSoftness2X, kSoftness2Y, kSoftness2, &knob);
  pGraphics->AttachControl(softness2);
  makeup2 = new IKnobMultiControlText(this, IRECT(kMakeup2X, kMakeup2Y, kMakeup2X + 43, kMakeup2Y + 43 + 21), kMakeup2, &knob, &text, "dB");
  pGraphics->AttachControl(makeup2);

  pGraphics->AttachControl(new IKnobMultiControl(this, kDryWetX, kDryWetY, kDryWet, &knob1));

  pGraphics->AttachControl(new ISwitchControl(this, kMiddlesideX, kMiddlesideY, kMiddleside, &myswitch));
  pGraphics->AttachControl(new ISwitchControl(this, kLinkChannelsX, kLinkChannelsY, kLinkChannels, &myswitch));
  pGraphics->AttachControl(new ISwitchControl(this, kActivateChannel1X, kActivateChannel1Y, kActivateChannel1, &myswitch));
  pGraphics->AttachControl(new ISwitchControl(this, kActivateChannel2X, kActivateChannel2Y, kActivateChannel2, &myswitch));

  AttachGraphics(pGraphics);

  //MakePreset("preset 1", ... );
  MakeDefaultPreset((char *) "-", kNumPrograms);
  
  volumesplitFilter.set_volume(std::sqrt(.5));
  volumemergeFilter.set_volume(std::sqrt(.5));
  endpoint.add_filter(&outLFilter);
  endpoint.add_filter(&outRFilter);

  powerFilter1.set_input_port(0, &inLFilter, 0);
  gainCompressorFilter1.set_input_port(0, &powerFilter1, 0);
  attackReleaseFilter1.set_input_port(0, &gainCompressorFilter1, 0);
  applyGainFilter.set_input_port(0, &attackReleaseFilter1, 0);
  applyGainFilter.set_input_port(1, &inLFilter, 0);
  makeupFilter1.set_input_port(0, &applyGainFilter, 0);
  drywetFilter.set_input_port(0, &makeupFilter1, 0);
  drywetFilter.set_input_port(1, &inLFilter, 0);
  outLFilter.set_input_port(0, &drywetFilter, 0);
  
  powerFilter2.set_input_port(0, &inRFilter, 0);
  gainCompressorFilter2.set_input_port(0, &powerFilter2, 0);
  attackReleaseFilter2.set_input_port(0, &gainCompressorFilter2, 0);
  applyGainFilter.set_input_port(2, &attackReleaseFilter2, 0);
  applyGainFilter.set_input_port(3, &inRFilter, 0);
  makeupFilter2.set_input_port(0, &applyGainFilter, 1);
  drywetFilter.set_input_port(2, &makeupFilter2, 0);
  drywetFilter.set_input_port(3, &inRFilter, 0);
  outRFilter.set_input_port(0, &drywetFilter, 1);

  middlesidesplitFilter.set_input_port(0, &inLFilter, 0);
  middlesidesplitFilter.set_input_port(1, &inRFilter, 0);
  volumesplitFilter.set_input_port(0, &middlesidesplitFilter, 0);
  volumesplitFilter.set_input_port(1, &middlesidesplitFilter, 1);
  middlesidemergeFilter.set_input_port(0, &makeupFilter1, 0);
  middlesidemergeFilter.set_input_port(1, &makeupFilter2, 0);
  volumemergeFilter.set_input_port(0, &middlesidemergeFilter, 0);
  volumemergeFilter.set_input_port(1, &middlesidemergeFilter, 1);
  sumFilter.set_input_port(0, &powerFilter1, 0);
  sumFilter.set_input_port(1, &powerFilter2, 0);

  powerFilter1.set_memory(0);
  powerFilter2.set_memory(0);

  Reset();
}

ATKStereoCompressor::~ATKStereoCompressor() {}

void ATKStereoCompressor::ProcessDoubleReplacing(double** inputs, double** outputs, int nFrames)
{
  // Mutex is already locked for us.

  inLFilter.set_pointer(inputs[0], nFrames);
  outLFilter.set_pointer(outputs[0], nFrames);
  inRFilter.set_pointer(inputs[1], nFrames);
  outRFilter.set_pointer(outputs[1], nFrames);
  endpoint.process(nFrames);
}

void ATKStereoCompressor::Reset()
{
  TRACE;
  IMutexLock lock(this);
  
  int sampling_rate = GetSampleRate();
  
  if (sampling_rate != endpoint.get_input_sampling_rate())
  {
    inLFilter.set_input_sampling_rate(sampling_rate);
    inLFilter.set_output_sampling_rate(sampling_rate);
    inRFilter.set_input_sampling_rate(sampling_rate);
    inRFilter.set_output_sampling_rate(sampling_rate);
    outLFilter.set_input_sampling_rate(sampling_rate);
    outLFilter.set_output_sampling_rate(sampling_rate);
    outRFilter.set_input_sampling_rate(sampling_rate);
    outRFilter.set_output_sampling_rate(sampling_rate);

    middlesidesplitFilter.set_input_sampling_rate(sampling_rate);
    middlesidesplitFilter.set_output_sampling_rate(sampling_rate);
    volumesplitFilter.set_input_sampling_rate(sampling_rate);
    volumesplitFilter.set_output_sampling_rate(sampling_rate);
    middlesidemergeFilter.set_input_sampling_rate(sampling_rate);
    middlesidemergeFilter.set_output_sampling_rate(sampling_rate);
    volumemergeFilter.set_input_sampling_rate(sampling_rate);
    volumemergeFilter.set_output_sampling_rate(sampling_rate);
    sumFilter.set_input_sampling_rate(sampling_rate);
    sumFilter.set_output_sampling_rate(sampling_rate);

    powerFilter1.set_input_sampling_rate(sampling_rate);
    powerFilter1.set_output_sampling_rate(sampling_rate);
    attackReleaseFilter1.set_input_sampling_rate(sampling_rate);
    attackReleaseFilter1.set_output_sampling_rate(sampling_rate);
    gainCompressorFilter1.set_input_sampling_rate(sampling_rate);
    gainCompressorFilter1.set_output_sampling_rate(sampling_rate);
    makeupFilter1.set_input_sampling_rate(sampling_rate);
    makeupFilter1.set_output_sampling_rate(sampling_rate);

    powerFilter2.set_input_sampling_rate(sampling_rate);
    powerFilter2.set_output_sampling_rate(sampling_rate);
    attackReleaseFilter2.set_input_sampling_rate(sampling_rate);
    attackReleaseFilter2.set_output_sampling_rate(sampling_rate);
    gainCompressorFilter2.set_input_sampling_rate(sampling_rate);
    gainCompressorFilter2.set_output_sampling_rate(sampling_rate);
    makeupFilter2.set_input_sampling_rate(sampling_rate);
    makeupFilter2.set_output_sampling_rate(sampling_rate);

    applyGainFilter.set_input_sampling_rate(sampling_rate);
    applyGainFilter.set_output_sampling_rate(sampling_rate);
    drywetFilter.set_input_sampling_rate(sampling_rate);
    drywetFilter.set_output_sampling_rate(sampling_rate);
    endpoint.set_input_sampling_rate(sampling_rate);
    endpoint.set_output_sampling_rate(sampling_rate);

    attackReleaseFilter1.set_release(std::exp(-1e3 / (GetParam(kAttack1)->Value() * sampling_rate))); // in ms
    attackReleaseFilter1.set_attack(std::exp(-1e3 / (GetParam(kRelease1)->Value() * sampling_rate))); // in ms
    attackReleaseFilter2.set_release(std::exp(-1e3 / (GetParam(kAttack2)->Value() * sampling_rate))); // in ms
    attackReleaseFilter2.set_attack(std::exp(-1e3 / (GetParam(kRelease2)->Value() * sampling_rate))); // in ms
  }
}

void ATKStereoCompressor::OnParamChange(int paramIdx)
{
  IMutexLock lock(this);

  switch (paramIdx)
  {
    case kMiddleside:
      if (GetParam(kMiddleside)->Bool())
      {
        powerFilter1.set_input_port(0, &volumesplitFilter, 0);
        powerFilter2.set_input_port(0, &volumesplitFilter, 1);
        drywetFilter.set_input_port(0, &volumemergeFilter, 0);
        drywetFilter.set_input_port(2, &volumemergeFilter, 1);
        applyGainFilter.set_input_port(1, &volumesplitFilter, 0);
        applyGainFilter.set_input_port(3, &volumesplitFilter, 1);
        if (GetParam(kActivateChannel1)->Bool())
        {
          middlesidemergeFilter.set_input_port(0, &makeupFilter1, 0);
        }
        else
        {
          middlesidemergeFilter.set_input_port(0, &volumesplitFilter, 0);
        }
        if (GetParam(kActivateChannel2)->Bool())
        {
          middlesidemergeFilter.set_input_port(1, &makeupFilter2, 0);
        }
        else
        {
          middlesidemergeFilter.set_input_port(1, &volumesplitFilter, 1);
        }
      }
      else
      {
        powerFilter1.set_input_port(0, &inLFilter, 0);
        powerFilter2.set_input_port(0, &inRFilter, 0);
        applyGainFilter.set_input_port(1, &inLFilter, 0);
        applyGainFilter.set_input_port(3, &inRFilter, 0);
        if (GetParam(kActivateChannel1)->Bool())
        {
          drywetFilter.set_input_port(0, &makeupFilter1, 0);
        }
        else
        {
          drywetFilter.set_input_port(0, &inLFilter, 0);
        }
        if (GetParam(kActivateChannel2)->Bool())
        {
          drywetFilter.set_input_port(2, &makeupFilter2, 0);
        }
        else
        {
          drywetFilter.set_input_port(2, &inRFilter, 0);
        }
      }
      break;
    case kLinkChannels:
      if (GetParam(kLinkChannels)->Bool())
      {
        gainCompressorFilter1.set_input_port(0, &sumFilter, 0);
        applyGainFilter.set_input_port(2, &attackReleaseFilter1, 0);
        makeupFilter2.set_volume_db(GetParam(kMakeup1)->Value());
        
        attack2->GrayOut(true);
        release2->GrayOut(true);
        threshold2->GrayOut(true);
        ratio2->GrayOut(true);
        softness2->GrayOut(true);
        makeup2->GrayOut(true);
      }
      else
      {
        gainCompressorFilter1.set_input_port(0, &powerFilter1, 0);
        applyGainFilter.set_input_port(2, &attackReleaseFilter2, 0);
        makeupFilter2.set_volume_db(GetParam(kMakeup2)->Value());
        
        attack2->GrayOut(false);
        release2->GrayOut(false);
        threshold2->GrayOut(false);
        ratio2->GrayOut(false);
        softness2->GrayOut(false);
        makeup2->GrayOut(false);
      }
      break;
    case kActivateChannel1:
      if (GetParam(kActivateChannel1)->Bool())
      {
        middlesidemergeFilter.set_input_port(0, &makeupFilter1, 0);
        if (GetParam(kMiddleside)->Bool())
        {
          drywetFilter.set_input_port(0, &volumemergeFilter, 0);
        }
        else
        {
          drywetFilter.set_input_port(0, &makeupFilter1, 0);
        }
      }
      else
      {
        middlesidemergeFilter.set_input_port(0, &volumesplitFilter, 0);
        if (GetParam(kMiddleside)->Bool())
        {
          drywetFilter.set_input_port(0, &volumemergeFilter, 0);
        }
        else
        {
          drywetFilter.set_input_port(0, &inLFilter, 0);
        }
      }
      break;
    case kActivateChannel2:
      if (GetParam(kActivateChannel2)->Bool())
      {
        middlesidemergeFilter.set_input_port(1, &makeupFilter2, 0);
        if (GetParam(kMiddleside)->Bool())
        {
          drywetFilter.set_input_port(2, &volumemergeFilter, 1);
        }
        else
        {
          drywetFilter.set_input_port(2, &makeupFilter2, 0);
        }
      }
      else
      {
        middlesidemergeFilter.set_input_port(1, &volumesplitFilter, 1);
        if (GetParam(kMiddleside)->Bool())
        {
          drywetFilter.set_input_port(2, &volumemergeFilter, 1);
        }
        else
        {
          drywetFilter.set_input_port(2, &inRFilter, 0);
        }
      }
      break;
      
    case kThreshold1:
      gainCompressorFilter1.set_threshold(std::pow(10, GetParam(kThreshold1)->Value() / 10));
      break;
    case kRatio1:
      gainCompressorFilter1.set_ratio(GetParam(kRatio1)->Value());
      break;
    case kSoftness1:
      gainCompressorFilter1.set_softness(std::pow(10, GetParam(kSoftness1)->Value()));
      break;
    case kAttack1:
      attackReleaseFilter1.set_release(std::exp(-1 / (GetParam(kAttack1)->Value() * 1e-3 * GetSampleRate()))); // in ms
      break;
    case kRelease1:
      attackReleaseFilter1.set_attack(std::exp(-1 / (GetParam(kRelease1)->Value() * 1e-3 * GetSampleRate()))); // in ms
      break;
    case kMakeup1:
      makeupFilter1.set_volume_db(GetParam(kMakeup1)->Value());
      if (GetParam(kLinkChannels)->Bool())
      {
        makeupFilter2.set_volume_db(GetParam(kMakeup1)->Value());
      }
      break;
    case kThreshold2:
      gainCompressorFilter2.set_threshold(std::pow(10, GetParam(kThreshold2)->Value() / 10));
      break;
    case kRatio2:
      gainCompressorFilter2.set_ratio(GetParam(kRatio2)->Value());
      break;
    case kSoftness2:
      gainCompressorFilter2.set_softness(std::pow(10, GetParam(kSoftness2)->Value()));
      break;
    case kAttack2:
      attackReleaseFilter2.set_release(std::exp(-1 / (GetParam(kAttack2)->Value() * 1e-3 * GetSampleRate()))); // in ms
      break;
    case kRelease2:
      attackReleaseFilter2.set_attack(std::exp(-1 / (GetParam(kRelease2)->Value() * 1e-3 * GetSampleRate()))); // in ms
      break;
    case kMakeup2:
      makeupFilter2.set_volume_db(GetParam(kMakeup2)->Value());
      break;
    case kDryWet:
      drywetFilter.set_dry(GetParam(kDryWet)->Value());
      break;

    default:
      break;
  }
}
