#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/Biquad.h"
#include "dsp/MatchFit.h"

namespace zeq
{

// Lock-free hand-off of the most recent audio block to the editor's FFT analyzer.
// Standard JUCE pattern: the audio thread fills `fifo`; when full it copies into
// `fftData` (guarded by an atomic flag) for the message thread to transform.
struct AnalyzerFifo
{
    static constexpr int order   = 11;          // 2048-point FFT
    static constexpr int fftSize = 1 << order;

    std::array<float, fftSize>     fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoIndex = 0;
    std::atomic<bool> blockReady { false };

    inline void push (float sample) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (! blockReady.load (std::memory_order_acquire))
            {
                std::fill (fftData.begin(), fftData.end(), 0.0f);
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                blockReady.store (true, std::memory_order_release);
            }
            fifoIndex = 0;
        }
        fifo[(size_t) fifoIndex++] = sample;
    }
};

class ZandersEqAudioProcessor : public juce::AudioProcessor
{
public:
    ZandersEqAudioProcessor();
    ~ZandersEqAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ZandersEQ"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int sizeInBytes) override;

    // --- editor access ---------------------------------------------------
    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    AnalyzerFifo& getAnalyzerFifo() noexcept { return analyzer; }
    double getActiveSampleRate() const noexcept { return baseSampleRate; }
    float getAutoGainTrimDb() const noexcept { return autoGainDb.load(); }

    // --- EQ match (capture taps live-fed pre-EQ / sidechain; curves are
    //     message-thread state, never touched by the audio thread) ----------
    enum class CaptureSlot { source, reference };
    AnalyzerFifo& getCaptureFifo (CaptureSlot s) noexcept { return s == CaptureSlot::source ? captureSrc : captureRef; }
    bool isCapturing() const noexcept { return capturing.load(); }
    void setCapturing (bool b) noexcept { capturing.store (b); }
    bool sidechainActive() const noexcept { return sidechainOn.load(); }
    void storeCaptureCurve (CaptureSlot s, const float* power, int n);
    void clearCaptureCurves();
    bool hasReference() const noexcept { return hasRef; }
    bool hasSource()    const noexcept { return hasSrc; }
    bool hasMatchData() const noexcept { return hasRef && hasSrc; }
    const float* getReferenceCurve() const noexcept { return refCurve.data(); }
    const float* getSourceCurve()    const noexcept { return srcCurve.data(); }

    // Editor/UI state that should persist but isn't host-automatable.
    int  getSelectedBand() const noexcept       { return selectedBand.load(); }
    void setSelectedBand (int i) noexcept        { selectedBand.store (juce::jlimit (0, numBands - 1, i)); }
    juce::String getCurrentSlot() const          { return abSlot; }
    void toggleABSlot (const juce::String& slot); // swaps the live params with the stored A/B snapshot

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() { return createParameterLayout(); }

private:
    // Cached atomic pointers for real-time-safe parameter reads.
    struct BandParams
    {
        std::atomic<float>* type  = nullptr;
        std::atomic<float>* freq  = nullptr;
        std::atomic<float>* gain  = nullptr;
        std::atomic<float>* q     = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* on    = nullptr;
        std::atomic<float>* solo  = nullptr;
    };

    void processEq (float* const* channels, int numChannels, int numSamples, double sr) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    std::array<BandParams, numBands> bandParams;
    std::atomic<float>* outputParam   = nullptr;
    std::atomic<float>* modeParam     = nullptr;
    std::atomic<float>* hqParam        = nullptr;
    std::atomic<float>* autogainParam = nullptr;

    // DSP state
    std::array<BandDsp, numBands>                                bands;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numBands> freqSm, qSm;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, numBands>          gainSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>                                outputSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>                        autoGainSm;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    double baseSampleRate = 48000.0;
    double smootherRate   = 48000.0;
    bool   lastHq         = false;
    std::atomic<float> autoGainDb { 0.0f };

    AnalyzerFifo analyzer;

    // EQ-match capture: lock-free taps fed pre-EQ (source) / from sidechain (reference)
    // while capturing; averaged curves are message-thread-only state.
    AnalyzerFifo       captureSrc, captureRef;
    std::atomic<bool>  capturing { false };
    std::atomic<bool>  sidechainOn { false };
    std::array<float, kMatchBins> refCurve {}, srcCurve {};
    bool hasRef = false, hasSrc = false;

    // Non-automated, persisted UI state.
    std::atomic<int> selectedBand { 3 };
    juce::String     abSlot { "A" };
    juce::ValueTree  abStored { "ABOther" }; // snapshot of the inactive slot

    static constexpr int controlBlock = 32;  // coeff refresh granularity (samples)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersEqAudioProcessor)
};

} // namespace zeq
