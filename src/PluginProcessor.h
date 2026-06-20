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
    float getDynGainDb (int band) const noexcept { return dynGainDisplay[(size_t) band].load(); }

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
    int  getEditorWidth() const noexcept         { return editorWidth; }
    void setEditorWidth (int w) noexcept         { editorWidth = w; }
    juce::String getCurrentSlot() const          { return abSlot; }
    void toggleABSlot (const juce::String& slot); // swaps the live params with the stored A/B snapshot

    // --- Undo/redo (whole-parameter snapshots; message-thread only) -----------
    void beginUndoTransaction();                          // capture the pre-edit state
    void commitUndoTransaction();                         // push it iff the edit changed anything
    void recordUndoableEdit (std::function<void()> edit); // begin + edit + commit, for discrete edits
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

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
        std::atomic<float>* channel = nullptr;
        std::atomic<float>* dynOn    = nullptr;
        std::atomic<float>* dynThresh = nullptr;
        std::atomic<float>* dynRange  = nullptr;
        std::atomic<float>* dynAttack = nullptr;
        std::atomic<float>* dynRelease = nullptr;
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
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, numBands>          rangeSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>                                outputSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>                        autoGainSm;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    double baseSampleRate = 48000.0;
    double smootherRate   = 48000.0;
    bool   lastHq         = false;
    bool   lastMs         = false;                  // global-domain change detector
    std::array<int, numBands> lastChannel { };      // per-band lane change detector
    std::atomic<float> autoGainDb { 0.0f };
    std::array<std::atomic<float>, numBands> dynGainDisplay { };

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
    int              editorWidth { 1100 };       // persisted UI size (message-thread)
    juce::String     abSlot { "A" };
    juce::ValueTree  abStored { "ABOther" }; // snapshot of the inactive slot

    // Undo/redo: stacks of full parameter snapshots (shares the A/B capture/restore).
    juce::ValueTree              pendingUndo;
    std::vector<juce::ValueTree> undoStack, redoStack;
    static constexpr int         maxUndo = 64;
    void            snapshotInto (juce::ValueTree& dest) const; // fill dest with every param's value
    juce::ValueTree captureParams() const;
    void            applyParams (const juce::ValueTree& snapshot);

    static constexpr int controlBlock = 32;  // coeff refresh granularity (samples)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersEqAudioProcessor)
};

} // namespace zeq
