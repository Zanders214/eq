#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>
#include <memory>
namespace zeq
{

namespace
{
    // Visit every host-automatable parameter id (used for A/B snapshots).
    template <typename Fn>
    void forEachParamId (Fn&& fn)
    {
        for (int i = 0; i < numBands; ++i)
        {
            fn (ids::type (i));  fn (ids::freq (i)); fn (ids::gain (i));
            fn (ids::q (i));     fn (ids::slope (i)); fn (ids::on (i));
            fn (ids::solo (i));  fn (ids::channel (i));
            fn (ids::dynOn (i)); fn (ids::dynThresh (i)); fn (ids::dynRange (i));
            fn (ids::dynAttack (i)); fn (ids::dynRelease (i));
        }
        fn (ids::output); fn (ids::mode); fn (ids::hq); fn (ids::autogain); fn (ids::matchamount);
    }
}

ZandersEqAudioProcessor::ZandersEqAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", makeLayout())
{
    for (int i = 0; i < numBands; ++i)
    {
        bandParams[(size_t) i].type  = apvts.getRawParameterValue (ids::type (i));
        bandParams[(size_t) i].freq  = apvts.getRawParameterValue (ids::freq (i));
        bandParams[(size_t) i].gain  = apvts.getRawParameterValue (ids::gain (i));
        bandParams[(size_t) i].q     = apvts.getRawParameterValue (ids::q (i));
        bandParams[(size_t) i].slope = apvts.getRawParameterValue (ids::slope (i));
        bandParams[(size_t) i].on    = apvts.getRawParameterValue (ids::on (i));
        bandParams[(size_t) i].solo  = apvts.getRawParameterValue (ids::solo (i));
        bandParams[(size_t) i].channel = apvts.getRawParameterValue (ids::channel (i));
        bandParams[(size_t) i].dynOn    = apvts.getRawParameterValue (ids::dynOn (i));
        bandParams[(size_t) i].dynThresh = apvts.getRawParameterValue (ids::dynThresh (i));
        bandParams[(size_t) i].dynRange  = apvts.getRawParameterValue (ids::dynRange (i));
        bandParams[(size_t) i].dynAttack = apvts.getRawParameterValue (ids::dynAttack (i));
        bandParams[(size_t) i].dynRelease = apvts.getRawParameterValue (ids::dynRelease (i));
        bandParams[(size_t) i].dynDir   = apvts.getRawParameterValue (ids::dynDir (i));
    }
    outputParam   = apvts.getRawParameterValue (ids::output);
    modeParam     = apvts.getRawParameterValue (ids::mode);
    hqParam       = apvts.getRawParameterValue (ids::hq);
    autogainParam = apvts.getRawParameterValue (ids::autogain);
}

void ZandersEqAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseSampleRate = sampleRate;
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing ((size_t) samplesPerBlock);

    smootherRate = baseSampleRate;
    const double ramp = 0.025;
    for (int i = 0; i < numBands; ++i)
    {
        freqSm[(size_t) i].reset (smootherRate, ramp);
        gainSm[(size_t) i].reset (smootherRate, ramp);
        qSm  [(size_t) i].reset (smootherRate, ramp);
        rangeSm[(size_t) i].reset (smootherRate, ramp);
        freqSm[(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].freq->load());
        gainSm[(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].gain->load());
        qSm  [(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].q->load());
        rangeSm[(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].dynRange->load());
        bands[(size_t) i].reset();
        bands[(size_t) i].active = false;
        lastChannel[(size_t) i] = -1;
        dynGainDisplay[(size_t) i].store (0.0f);
    }
    lastMs = false;
    outputSm.reset (baseSampleRate, ramp);
    outputSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    autoGainSm.reset (baseSampleRate, 0.08);   // slower ramp so the trim doesn't pump
    autoGainSm.setCurrentAndTargetValue (1.0f);

    lastHq = hqParam->load() > 0.5f;
    setLatencySamples (lastHq ? (int) std::round (oversampler->getLatencyInSamples()) : 0);
}

bool ZandersEqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;

    // Optional sidechain (input bus 1) — allow disabled, mono or stereo.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled()
            && sc != juce::AudioChannelSet::mono()
            && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

namespace
{
    // Sum a JUCE bus to mono per sample and push each averaged sample into a FIFO.
    void pushBusToFifo (const juce::AudioBuffer<float>& bus, AnalyzerFifo& fifo, int nSamples, int channelsForAvg) noexcept
    {
        const int chans = bus.getNumChannels();
        for (int s = 0; s < nSamples; ++s)
        {
            float m = 0.0f;
            for (int c = 0; c < chans; ++c)
                m += bus.getSample (c, s);
            fifo.push (m / (float) channelsForAvg);
        }
    }

    // Sum of squares across every channel of a JUCE bus (loudness measure).
    double sumSquares (const juce::AudioBuffer<float>& bus, int nSamples) noexcept
    {
        double acc = 0.0;
        for (int c = 0; c < bus.getNumChannels(); ++c)
        {
            const float* d = bus.getReadPointer (c);
            for (int s = 0; s < nSamples; ++s)
                acc += (double) d[s] * d[s];
        }
        return acc;
    }
}

void ZandersEqAudioProcessor::captureMatchTaps (juce::AudioBuffer<float>& buffer,
                                                const juce::AudioBuffer<float>& mainBus,
                                                int nSamples, int nCh, bool scEnabled) noexcept
{
    pushBusToFifo (mainBus, captureSrc, nSamples, nCh);
    if (! scEnabled)
        return;

    auto scBus = getBusBuffer (buffer, true, 1);
    const int scCh = juce::jmax (1, scBus.getNumChannels());
    pushBusToFifo (scBus, captureRef, nSamples, scCh);
}

void ZandersEqAudioProcessor::resetSmoothersForRate (double procRate) noexcept
{
    const double ramp = 0.025;
    for (int i = 0; i < numBands; ++i)
    {
        freqSm[(size_t) i].reset (procRate, ramp);
        gainSm[(size_t) i].reset (procRate, ramp);
        qSm  [(size_t) i].reset (procRate, ramp);
        rangeSm[(size_t) i].reset (procRate, ramp);
    }
}

void ZandersEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, true, 0);   // main in/out, processed in place
    const int nSamples = mainBus.getNumSamples();
    const int nCh      = juce::jmax (1, mainBus.getNumChannels());

    // EQ-match capture: source from the main bus (pre-EQ), reference from the sidechain.
    const bool scEnabled = getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
    sidechainOn.store (scEnabled);
    if (capturing.load())
        captureMatchTaps (buffer, mainBus, nSamples, nCh, scEnabled);

    const bool hq = hqParam->load() > 0.5f;
    const double procRate = baseSampleRate * (hq ? 2.0 : 1.0);

    if (hq != lastHq)
    {
        setLatencySamples (hq ? (int) std::round (oversampler->getLatencyInSamples()) : 0);
        lastHq = hq;
    }
    if (procRate != smootherRate)
    {
        resetSmoothersForRate (procRate);
        smootherRate = procRate;   // output/auto-gain stay at base rate (applied post-downsample)
    }

    for (int i = 0; i < numBands; ++i)
    {
        freqSm[(size_t) i].setTargetValue (bandParams[(size_t) i].freq->load());
        gainSm[(size_t) i].setTargetValue (bandParams[(size_t) i].gain->load());
        qSm  [(size_t) i].setTargetValue (bandParams[(size_t) i].q->load());
        rangeSm[(size_t) i].setTargetValue (bandParams[(size_t) i].dynRange->load());
    }
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    // Input level (pre-EQ), for auto-gain loudness matching.
    const bool autogain = autogainParam->load() > 0.5f;
    const double inSumSq = autogain ? sumSquares (mainBus, nSamples) : 0.0;

    // Filtering only (no gain) — at base rate or 2x inside the oversampler.
    if (hq)
    {
        juce::dsp::AudioBlock<float> block (mainBus);
        auto up = oversampler->processSamplesUp (block);
        std::array<float*, 2> chans { nullptr, nullptr };
        // Compare as int (channel count is tiny): an explicit jmin<size_t> would force JUCE's
        // SIMD jmin overload to instantiate SIMDRegister<size_t>, which is undefined on macOS
        // arm64 where size_t (unsigned long) != uint64_t (unsigned long long).
        const int n = juce::jmin (2, (int) up.getNumChannels());
        for (int ch = 0; ch < n; ++ch)
            chans[(size_t) ch] = up.getChannelPointer ((size_t) ch);
        processEq (chans.data(), n, (int) up.getNumSamples(), procRate);
        oversampler->processSamplesDown (block);
    }
    else
    {
        processEq (mainBus.getArrayOfWritePointers(), mainBus.getNumChannels(), nSamples, procRate);
    }

    // Auto-gain: trim so post-EQ loudness matches the input (clamped to +-12 dB).
    if (autogain)
    {
        const double outSumSq = sumSquares (mainBus, nSamples);
        if (outSumSq > 1.0e-9 && inSumSq > 1.0e-9)
        {
            const float trim = juce::jlimit (0.25f, 4.0f, (float) std::sqrt (inSumSq / outSumSq));
            autoGainSm.setTargetValue (trim);
        }
    }
    else
    {
        autoGainSm.setTargetValue (1.0f);
    }

    // Output stage: user gain x auto-gain trim, per-sample (click-free), base rate.
    for (int s = 0; s < nSamples; ++s)
    {
        const float g = outputSm.getNextValue() * autoGainSm.getNextValue();
        for (int c = 0; c < nCh; ++c)
            mainBus.getWritePointer (c)[s] *= g;
    }
    autoGainDb.store (juce::Decibels::gainToDecibels (autoGainSm.getCurrentValue()));

    // Feed the analyzer with the post-EQ (output) signal, summed to mono.
    pushBusToFifo (mainBus, analyzer, nSamples, nCh);
}

void ZandersEqAudioProcessor::processEq (float* const* channels, int numChannels,
                                         int numSamples, double sr) noexcept ZEQ_RT_NONBLOCKING
{
    const bool stereo = numChannels >= 2;
    const bool ms     = stereo && modeParam->load() > 0.5f;

    bool anySolo = false;
    for (int i = 0; i < numBands; ++i)
        anySolo = anySolo || (bandParams[(size_t) i].solo->load() > 0.5f);

    // Global-domain flip (L/R <-> M/S) changes what every state set carries — reset all.
    if (ms != lastMs)
    {
        for (int i = 0; i < numBands; ++i)
            bands[(size_t) i].reset();
        lastMs = ms;
    }

    std::array<int, numBands>  lane {};   // resolved per-band lane for this sub-block
    std::array<bool, numBands> dyn {};    // per-band dynamics active this sub-block

    int pos = 0;
    while (pos < numSamples)
    {
        const int len = juce::jmin (controlBlock, numSamples - pos);

        updateBandCoeffsForBlock (len, sr, anySolo, lane, dyn);

        if (stereo)
            applyBandsStereo (channels, pos, len, ms, lane, dyn);
        else
            applyBandsMono (channels, pos, len, dyn);

        pos += len;
    }
}

// Advance the smoothers and refresh each band's coefficients/dynamics for one control sub-block.
void ZandersEqAudioProcessor::updateBandCoeffsForBlock (int len, double sr, bool anySolo,
                                                        std::array<int, numBands>& lane,
                                                        std::array<bool, numBands>& dyn) noexcept ZEQ_RT_NONBLOCKING
{
    for (int i = 0; i < numBands; ++i)
    {
        const float f = freqSm[(size_t) i].getNextValue();
        const float g = gainSm[(size_t) i].getNextValue();
        const float qq = qSm[(size_t) i].getNextValue();
        const float rng = rangeSm[(size_t) i].getNextValue();
        if (len > 1) { freqSm[(size_t) i].skip (len - 1); gainSm[(size_t) i].skip (len - 1); qSm[(size_t) i].skip (len - 1); rangeSm[(size_t) i].skip (len - 1); }

        const auto type   = static_cast<FilterType> ((int) bandParams[(size_t) i].type->load());
        const int  slope  = slopeIndexToValue ((int) bandParams[(size_t) i].slope->load());
        const bool on     = bandParams[(size_t) i].on->load() > 0.5f;
        const bool solo   = bandParams[(size_t) i].solo->load() > 0.5f;
        const bool active = on && (! anySolo || solo);
        const int  chMode = (int) bandParams[(size_t) i].channel->load();
        lane[(size_t) i]  = chMode;

        if (const bool laneChanged = chMode != lastChannel[(size_t) i];
            active != bands[(size_t) i].active || laneChanged)
        {
            if (! active || laneChanged)
                bands[(size_t) i].reset();
            bands[(size_t) i].active = active;
        }
        lastChannel[(size_t) i] = chMode;

        // Dynamic EQ (bell/shelf only): fold the detector-driven offset into gain.
        const bool dynActive = active && bandParams[(size_t) i].dynOn->load() > 0.5f && ! sitsOnZeroLine (type);
        dyn[(size_t) i] = dynActive;
        float effGain = g;
        if (dynActive)
        {
            const double levelDb = juce::Decibels::gainToDecibels (bands[(size_t) i].env + 1.0e-9f);
            const auto dir = bandParams[(size_t) i].dynDir->load() > 0.5f ? DynDirection::under
                                                                          : DynDirection::over;
            const double offs = dynamicGainDb (levelDb, (double) bandParams[(size_t) i].dynThresh->load(),
                                               (double) rng, kDynKnee, dir);
            effGain = g + (float) offs;
            bands[(size_t) i].dynGainDb = (float) offs;
            bands[(size_t) i].updateDetector (f, juce::jlimit (0.5f, 4.0f, qq),
                                              (double) bandParams[(size_t) i].dynAttack->load(),
                                              (double) bandParams[(size_t) i].dynRelease->load(), sr);
            dynGainDisplay[(size_t) i].store ((float) offs);
        }
        else
        {
            bands[(size_t) i].dynGainDb = 0.0f;
            bands[(size_t) i].env = 0.0f;   // restart clean when dynamics are re-enabled
            dynGainDisplay[(size_t) i].store (0.0f);
        }

        if (active)
            bands[(size_t) i].updateCoeffs (type, f, effGain, qq, slope, sr);
    }
}

// Stereo (or M/S) path: filter the canonical lane pair through every active band.
void ZandersEqAudioProcessor::applyBandsStereo (float* const* channels, int pos, int len, bool ms,
                                                const std::array<int, numBands>& lane,
                                                const std::array<bool, numBands>& dyn) noexcept ZEQ_RT_NONBLOCKING
{
    float* L = channels[0];
    float* R = channels[1];
    for (int s = pos; s < pos + len; ++s)
    {
        // detector keys off the pre-EQ input (mono), independent of band order/domain
        const float det = 0.5f * (L[s] + R[s]);
        // canonical lane pair for the whole chain: L/R, or M/S (encoded once)
        float a = ms ? 0.5f * (L[s] + R[s]) : L[s];
        float b = ms ? 0.5f * (L[s] - R[s]) : R[s];
        for (int i = 0; i < numBands; ++i)
        {
            if (! bands[(size_t) i].active)
                continue;
            if (dyn[(size_t) i])
                bands[(size_t) i].pushDetector (det);
            applyBand (bands[(size_t) i], lane[(size_t) i], a, b);
        }
        if (ms) { L[s] = a + b; R[s] = a - b; }
        else    { L[s] = a;     R[s] = b;     }
    }
}

// Mono path: lane-agnostic, filter the single channel through state set 0.
void ZandersEqAudioProcessor::applyBandsMono (float* const* channels, int pos, int len,
                                              const std::array<bool, numBands>& dyn) noexcept ZEQ_RT_NONBLOCKING
{
    float* M = channels[0];
    for (int s = pos; s < pos + len; ++s)
    {
        const float in = M[s];   // detect on the pre-EQ input
        float x = M[s];
        for (int i = 0; i < numBands; ++i)
        {
            if (! bands[(size_t) i].active)
                continue;
            if (dyn[(size_t) i])
                bands[(size_t) i].pushDetector (in);
            x = bands[(size_t) i].processSample (0, x);
        }
        M[s] = x;
    }
}

juce::AudioProcessorEditor* ZandersEqAudioProcessor::createEditor()
{
    return std::make_unique<ZandersEqEditor> (*this).release();
}

// Fill `dest` with the normalised value of every host-automatable parameter.
void ZandersEqAudioProcessor::snapshotInto (juce::ValueTree& dest) const
{
    forEachParamId ([this, &dest] (const juce::String& id)
    {
        if (const auto* p = apvts.getParameter (id))
            dest.setProperty (id, p->getValue(), nullptr);
    });
}

juce::ValueTree ZandersEqAudioProcessor::captureParams() const
{
    juce::ValueTree t ("ParamSnapshot");
    snapshotInto (t);
    return t;
}

void ZandersEqAudioProcessor::applyParams (const juce::ValueTree& snapshot) const
{
    forEachParamId ([this, &snapshot] (const juce::String& id)
    {
        if (snapshot.hasProperty (id))
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost ((float) snapshot.getProperty (id));
    });
}

void ZandersEqAudioProcessor::toggleABSlot (const juce::String& slot)
{
    if (slot == abSlot)
        return;

    juce::ValueTree current ("ABOther");
    snapshotInto (current);          // remember the live params for this slot
    applyParams (abStored);          // load the other slot's params
    abStored = current;
    abSlot = slot;
}

void ZandersEqAudioProcessor::beginUndoTransaction()
{
    pendingUndo = captureParams();
}

void ZandersEqAudioProcessor::commitUndoTransaction()
{
    if (! pendingUndo.isValid())
        return;
    if (! captureParams().isEquivalentTo (pendingUndo))   // ignore no-op gestures (e.g. a click)
    {
        undoStack.push_back (pendingUndo);
        if ((int) undoStack.size() > maxUndo)
            undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    pendingUndo = {};
}

void ZandersEqAudioProcessor::recordUndoableEdit (const std::function<void()>& edit)
{
    beginUndoTransaction();
    if (edit) edit();
    commitUndoTransaction();
}

void ZandersEqAudioProcessor::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (captureParams());
    const auto snapshot = undoStack.back();
    undoStack.pop_back();
    applyParams (snapshot);
}

void ZandersEqAudioProcessor::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (captureParams());
    const auto snapshot = redoStack.back();
    redoStack.pop_back();
    applyParams (snapshot);
}

// --- User presets ----------------------------------------------------------
juce::File ZandersEqAudioProcessor::userPresetsDir() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ZandersAudio").getChildFile ("ZandersEQ").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

bool ZandersEqAudioProcessor::savePresetToFile (const juce::File& file) const
{
    if (auto xml = captureParams().createXml())
        return xml->writeTo (file);
    return false;
}

bool ZandersEqAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid())
        return false;
    recordUndoableEdit ([this, &tree] { applyParams (tree); });   // loading a preset is one undo step
    return true;
}

bool ZandersEqAudioProcessor::saveUserPreset (const juce::String& name) const
{
    const auto clean = juce::File::createLegalFileName (name).trim();
    if (clean.isEmpty())
        return false;
    return savePresetToFile (userPresetsDir().getChildFile (clean + presetExtension()));
}

bool ZandersEqAudioProcessor::deleteUserPreset (const juce::File& file) const
{
    return file.existsAsFile() && file.deleteFile();
}

juce::Array<juce::File> ZandersEqAudioProcessor::listUserPresets() const
{
    auto files = userPresetsDir().findChildFiles (juce::File::findFiles, false, "*" + presetExtension());
    struct Cmp { static int compareElements (const juce::File& a, const juce::File& b)
                 { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()); } };
    Cmp cmp;
    files.sort (cmp);
    return files;
}

void ZandersEqAudioProcessor::storeCaptureCurve (CaptureSlot s, const float* power, int n)
{
    auto& dst = (s == CaptureSlot::source) ? srcCurve : refCurve;
    const int count = juce::jmin (n, kMatchBins);
    std::fill (dst.begin(), dst.end(), 0.0f);
    std::copy (power, power + count, dst.begin());
    (s == CaptureSlot::source ? hasSrc : hasRef) = true;
}

void ZandersEqAudioProcessor::clearCaptureCurves()
{
    refCurve.fill (0.0f);
    srcCurve.fill (0.0f);
    hasSrc = false;
    hasRef = false;
}

void ZandersEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("selectedBand", selectedBand.load(), nullptr);
    state.setProperty ("editorWidth", editorWidth, nullptr);
    state.setProperty ("abSlot", abSlot, nullptr);
    state.removeChild (state.getChildWithName ("ABOther"), nullptr);
    state.appendChild (abStored.createCopy(), nullptr);

    // EQ-match captured curves (raw float power per bin) as binary blobs.
    if (hasRef)
        state.setProperty ("refCurve", juce::MemoryBlock (refCurve.data(), sizeof (refCurve)), nullptr);
    if (hasSrc)
        state.setProperty ("srcCurve", juce::MemoryBlock (srcCurve.data(), sizeof (srcCurve)), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ZandersEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes) // NOSONAR(cpp:S5008) const void* signature is mandated by the juce::AudioProcessor::setStateInformation override
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || tree.getType() != apvts.state.getType())
        return;

    selectedBand.store (juce::jlimit (0, numBands - 1, (int) tree.getProperty ("selectedBand", 3)));
    editorWidth = juce::jlimit (660, 1870, (int) tree.getProperty ("editorWidth", 1100));
    abSlot = tree.getProperty ("abSlot", "A").toString();

    auto stored = tree.getChildWithName ("ABOther");
    abStored = stored.isValid() ? stored.createCopy() : juce::ValueTree ("ABOther");

    // Restore EQ-match curves if present and the right size.
    clearCaptureCurves();
    auto loadCurve = [&] (const char* prop, std::array<float, kMatchBins>& dst, bool& flag)
    {
        if (auto* mb = tree.getProperty (prop).getBinaryData())
            if (mb->getSize() == sizeof (dst))
            {
                std::memcpy (dst.data(), mb->getData(), sizeof (dst));
                flag = true;
            }
    };
    loadCurve ("refCurve", refCurve, hasRef);
    loadCurve ("srcCurve", srcCurve, hasSrc);

    apvts.replaceState (tree);
}

} // namespace zeq

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return std::make_unique<zeq::ZandersEqAudioProcessor>().release();
}
