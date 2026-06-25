#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/Biquad.h"
#include "dsp/MatchFit.h"
#include "dsp/RtSafety.h"

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
            if (! blockReady.load())
            {
                std::fill (fftData.begin(), fftData.end(), 0.0f);
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                blockReady.store (true);
            }
            fifoIndex = 0;
        }
        fifo[(size_t) fifoIndex++] = sample;
    }
};

class ZandersEqAudioProcessor : public juce::AudioProcessor // NOSONAR(cpp:S1820,cpp:S1448): core JUCE processor; fields/methods are the plugin's API surface, splitting would ripple across the codebase
{
public:
    ZandersEqAudioProcessor();
    ~ZandersEqAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {} // nothing to free: oversampler/smoothers are reset in prepareToPlay
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock; // un-hide the double-precision overload
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ZandersEQ"; } // NOSONAR(cpp:S5951) const return is mandated by the juce::AudioProcessor::getName override signature
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override { /* single fixed program: nothing to switch */ }
    const juce::String getProgramName (int) override { return {}; } // NOSONAR(cpp:S5951) const return is mandated by the juce::AudioProcessor::getProgramName override signature
    void changeProgramName (int, const juce::String&) override { /* programs are not user-renamable */ }

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
    void recordUndoableEdit (const std::function<void()>& edit); // begin + edit + commit, for discrete edits
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    // --- User presets (full-state snapshot <-> .zeqpreset files) --------------
    juce::File userPresetsDir() const;                       // created if missing
    bool savePresetToFile (const juce::File&) const;         // current params -> XML file
    bool loadPresetFromFile (const juce::File&);             // file -> params (undoable)
    bool saveUserPreset (const juce::String& name) const;    // name -> dir/<name>.zeqpreset
    bool deleteUserPreset (const juce::File&) const;
    juce::Array<juce::File> listUserPresets() const;         // sorted *.zeqpreset
    static juce::String presetExtension() { return ".zeqpreset"; }

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
        std::atomic<float>* dynDir   = nullptr;
    };

    void processEq (float* const* channels, int numChannels, int numSamples, double sr) noexcept ZEQ_RT_NONBLOCKING;

    // processBlock helpers (extracted to keep the audio callback readable).
    void captureMatchTaps (juce::AudioBuffer<float>& buffer,
                           const juce::AudioBuffer<float>& mainBus,
                           int nSamples, int nCh, bool scEnabled) noexcept;
    void resetSmoothersForRate (double procRate) noexcept;

    // processEq helpers.
    void updateBandCoeffsForBlock (int len, double sr, bool anySolo,
                                   std::array<int, numBands>& lane,
                                   std::array<bool, numBands>& dyn) noexcept ZEQ_RT_NONBLOCKING;
    void applyBandsStereo (float* const* channels, int pos, int len, bool ms,
                           const std::array<int, numBands>& lane,
                           const std::array<bool, numBands>& dyn) noexcept ZEQ_RT_NONBLOCKING;
    void applyBandsMono (float* const* channels, int pos, int len,
                         const std::array<bool, numBands>& dyn) noexcept ZEQ_RT_NONBLOCKING;

    juce::AudioProcessorValueTreeState apvts;
    std::array<BandParams, numBands> bandParams;
    std::atomic<float>* outputParam   = nullptr;
    std::atomic<float>* modeParam     = nullptr;
    std::atomic<float>* hqParam        = nullptr;
    std::atomic<float>* autogainParam = nullptr;

    // DSP state
    std::array<BandDsp, numBands>                                bands;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numBands> freqSm;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numBands> qSm;
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
    // Coefficient-cache change detectors: a band's biquad is recomputed only when one of its
    // inputs actually moves (see updateBandCoeffsForBlock). -1 sentinels force the first block
    // to recompute; prepareToPlay primes them.
    std::array<int,  numBands> lastType      { };
    std::array<int,  numBands> lastSlope     { };
    std::array<bool, numBands> lastDynActive { };
    std::array<bool, numBands> lastMoving    { };   // recompute once more after a ramp settles (exact target)
    std::atomic<float> autoGainDb { 0.0f };
    std::array<std::atomic<float>, numBands> dynGainDisplay { };

    AnalyzerFifo analyzer;

    // EQ-match capture: lock-free taps fed pre-EQ (source) / from sidechain (reference)
    // while capturing; averaged curves are message-thread-only state.
    AnalyzerFifo       captureSrc;
    AnalyzerFifo       captureRef;
    std::atomic<bool>  capturing { false };
    std::atomic<bool>  sidechainOn { false };
    std::array<float, kMatchBins> refCurve {};
    std::array<float, kMatchBins> srcCurve {};
    bool hasRef = false;
    bool hasSrc = false;

    // Non-automated, persisted UI state.
    std::atomic<int> selectedBand { 3 };
    int              editorWidth { 1100 };       // persisted UI size (message-thread)
    juce::String     abSlot { "A" };
    juce::ValueTree  abStored { "ABOther" }; // snapshot of the inactive slot

    // Undo/redo: stacks of full parameter snapshots (shares the A/B capture/restore).
    juce::ValueTree              pendingUndo;
    std::vector<juce::ValueTree> undoStack;
    std::vector<juce::ValueTree> redoStack;
    static constexpr int         maxUndo = 64;
    void            snapshotInto (juce::ValueTree& dest) const; // fill dest with every param's value
    juce::ValueTree captureParams() const;
    void            applyParams (const juce::ValueTree& snapshot) const;

    static constexpr int controlBlock = 32;  // coeff refresh granularity (samples)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZandersEqAudioProcessor)
};

} // namespace zeq
