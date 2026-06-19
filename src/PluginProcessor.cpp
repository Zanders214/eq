#include "PluginProcessor.h"
#include "PluginEditor.h"

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
            fn (ids::solo (i));
        }
        fn (ids::output); fn (ids::mode); fn (ids::hq); fn (ids::autogain);
    }
}

ZandersEqAudioProcessor::ZandersEqAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
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
        freqSm[(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].freq->load());
        gainSm[(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].gain->load());
        qSm  [(size_t) i].setCurrentAndTargetValue (bandParams[(size_t) i].q->load());
        bands[(size_t) i].reset();
        bands[(size_t) i].active = false;
    }
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
    return layouts.getMainInputChannelSet() == out;
}

void ZandersEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const bool hq = hqParam->load() > 0.5f;
    const double procRate = baseSampleRate * (hq ? 2.0 : 1.0);

    if (hq != lastHq)
    {
        setLatencySamples (hq ? (int) std::round (oversampler->getLatencyInSamples()) : 0);
        lastHq = hq;
    }
    if (procRate != smootherRate)
    {
        const double ramp = 0.025;
        for (int i = 0; i < numBands; ++i)
        {
            freqSm[(size_t) i].reset (procRate, ramp);
            gainSm[(size_t) i].reset (procRate, ramp);
            qSm  [(size_t) i].reset (procRate, ramp);
        }
        smootherRate = procRate;   // output/auto-gain stay at base rate (applied post-downsample)
    }

    for (int i = 0; i < numBands; ++i)
    {
        freqSm[(size_t) i].setTargetValue (bandParams[(size_t) i].freq->load());
        gainSm[(size_t) i].setTargetValue (bandParams[(size_t) i].gain->load());
        qSm  [(size_t) i].setTargetValue (bandParams[(size_t) i].q->load());
    }
    outputSm.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    const int nSamples = buffer.getNumSamples();
    const int nCh      = juce::jmax (1, buffer.getNumChannels());

    // Input level (pre-EQ), for auto-gain loudness matching.
    const bool autogain = autogainParam->load() > 0.5f;
    double inSumSq = 0.0;
    if (autogain)
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int s = 0; s < nSamples; ++s)
                inSumSq += (double) d[s] * d[s];
        }

    // Filtering only (no gain) — at base rate or 2x inside the oversampler.
    if (hq)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto up = oversampler->processSamplesUp (block);
        std::array<float*, 2> chans { nullptr, nullptr };
        const int n = (int) juce::jmin<size_t> (2, up.getNumChannels());
        for (int ch = 0; ch < n; ++ch)
            chans[(size_t) ch] = up.getChannelPointer ((size_t) ch);
        processEq (chans.data(), n, (int) up.getNumSamples(), procRate);
        oversampler->processSamplesDown (block);
    }
    else
    {
        processEq (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                   buffer.getNumSamples(), procRate);
    }

    // Auto-gain: trim so post-EQ loudness matches the input (clamped to +-12 dB).
    if (autogain)
    {
        double outSumSq = 0.0;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const float* d = buffer.getReadPointer (c);
            for (int s = 0; s < nSamples; ++s)
                outSumSq += (double) d[s] * d[s];
        }
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
            buffer.getWritePointer (c)[s] *= g;
    }
    autoGainDb.store (juce::Decibels::gainToDecibels (autoGainSm.getCurrentValue()));

    // Feed the analyzer with the post-EQ (output) signal, summed to mono.
    for (int s = 0; s < nSamples; ++s)
    {
        float m = 0.0f;
        for (int c = 0; c < nCh; ++c)
            m += buffer.getSample (c, s);
        analyzer.push (m / (float) nCh);
    }
}

void ZandersEqAudioProcessor::processEq (float* const* channels, int numChannels,
                                         int numSamples, double sr) noexcept
{
    const bool ms = numChannels >= 2 && modeParam->load() > 0.5f;

    bool anySolo = false;
    for (int i = 0; i < numBands; ++i)
        anySolo = anySolo || (bandParams[(size_t) i].solo->load() > 0.5f);

    int pos = 0;
    while (pos < numSamples)
    {
        const int len = juce::jmin (controlBlock, numSamples - pos);

        for (int i = 0; i < numBands; ++i)
        {
            const float f = freqSm[(size_t) i].getNextValue();
            const float g = gainSm[(size_t) i].getNextValue();
            const float qq = qSm[(size_t) i].getNextValue();
            if (len > 1) { freqSm[(size_t) i].skip (len - 1); gainSm[(size_t) i].skip (len - 1); qSm[(size_t) i].skip (len - 1); }

            const auto type   = static_cast<FilterType> ((int) bandParams[(size_t) i].type->load());
            const int  slope  = slopeIndexToValue ((int) bandParams[(size_t) i].slope->load());
            const bool on     = bandParams[(size_t) i].on->load() > 0.5f;
            const bool solo   = bandParams[(size_t) i].solo->load() > 0.5f;
            const bool active = on && (! anySolo || solo);

            if (active != bands[(size_t) i].active)
            {
                if (! active)
                    bands[(size_t) i].reset();
                bands[(size_t) i].active = active;
            }
            if (active)
                bands[(size_t) i].updateCoeffs (type, f, g, qq, slope, sr);
        }

        if (ms)
        {
            float* L = channels[0];
            float* R = channels[1];
            for (int s = pos; s < pos + len; ++s)
            {
                float mid  = 0.5f * (L[s] + R[s]);
                float side = 0.5f * (L[s] - R[s]);
                for (int i = 0; i < numBands; ++i)
                    if (bands[(size_t) i].active)
                    {
                        mid  = bands[(size_t) i].processSample (0, mid);
                        side = bands[(size_t) i].processSample (1, side);
                    }
                L[s] = mid + side;
                R[s] = mid - side;
            }
        }
        else
        {
            for (int s = pos; s < pos + len; ++s)
            {
                for (int c = 0; c < numChannels; ++c)
                {
                    const int stateCh = juce::jmin (c, 1);
                    float x = channels[c][s];
                    for (int i = 0; i < numBands; ++i)
                        if (bands[(size_t) i].active)
                            x = bands[(size_t) i].processSample (stateCh, x);
                    channels[c][s] = x;
                }
            }
        }
        pos += len;
    }
}

juce::AudioProcessorEditor* ZandersEqAudioProcessor::createEditor()
{
    return new ZandersEqEditor (*this);
}

void ZandersEqAudioProcessor::toggleABSlot (const juce::String& slot)
{
    if (slot == abSlot)
        return;

    juce::ValueTree current ("ABOther");
    forEachParamId ([&] (const juce::String& id)
    {
        if (auto* p = apvts.getParameter (id))
            current.setProperty (id, p->getValue(), nullptr);
    });

    forEachParamId ([&] (const juce::String& id)
    {
        if (abStored.hasProperty (id))
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost ((float) abStored.getProperty (id));
    });

    abStored = current;
    abSlot = slot;
}

void ZandersEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("selectedBand", selectedBand.load(), nullptr);
    state.setProperty ("abSlot", abSlot, nullptr);
    state.removeChild (state.getChildWithName ("ABOther"), nullptr);
    state.appendChild (abStored.createCopy(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ZandersEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || tree.getType() != apvts.state.getType())
        return;

    selectedBand.store (juce::jlimit (0, numBands - 1, (int) tree.getProperty ("selectedBand", 3)));
    abSlot = tree.getProperty ("abSlot", "A").toString();

    auto stored = tree.getChildWithName ("ABOther");
    abStored = stored.isValid() ? stored.createCopy() : juce::ValueTree ("ABOther");

    apvts.replaceState (tree);
}

} // namespace zeq

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new zeq::ZandersEqAudioProcessor();
}
