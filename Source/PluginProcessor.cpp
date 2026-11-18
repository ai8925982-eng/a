#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// Background file analysis
// ============================================================================
class MixCheckProcessor::FileJob : public juce::Thread
{
public:
    FileJob (MixCheckProcessor& o, const juce::File& f) : juce::Thread ("MixCheck file analysis"), owner (o), file (f) {}
    ~FileJob() override { stopThread (5000); }

    void run() override
    {
        std::unique_ptr<juce::AudioFormatReader> reader (owner.formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
        {
            owner.setError (juce::String::fromUTF8 ("Cannot read this file / ไม่สามารถอ่านไฟล์นี้ได้"));
            owner.analyzing = false;
            return;
        }

        mc::Analyzer analyzer;
        analyzer.prepare (reader->sampleRate);
        analyzer.setMono (reader->numChannels == 1);

        constexpr int block = 32768;
        juce::AudioBuffer<float> buf (2, block);
        const juce::int64 total = reader->lengthInSamples;
        juce::int64 pos = 0;

        while (pos < total && ! threadShouldExit())
        {
            const int n = (int) std::min<juce::int64> (block, total - pos);
            buf.clear();
            // For a mono file the left channel is copied to both destination channels.
            reader->read (&buf, 0, n, pos, true, true);
            analyzer.process (buf.getReadPointer (0), buf.getReadPointer (1), n);
            pos += n;
            owner.progress = (float) ((double) pos / (double) total);
        }

        if (! threadShouldExit())
        {
            const auto result = analyzer.finish();
            if (! result.valid)
                owner.setError (juce::String::fromUTF8 ("File is silent or too short (min. 0.5 s) / ไฟล์เงียบหรือสั้นเกินไป (อย่างน้อย 0.5 วินาที)"));
            else
            {
                owner.setError ({});
                owner.publish (result, file.getFileName());
            }
        }
        owner.analyzing = false;
    }

private:
    MixCheckProcessor& owner;
    juce::File file;
};

// ============================================================================
// Live capture drain thread
// ============================================================================
class MixCheckProcessor::LiveThread : public juce::Thread
{
public:
    explicit LiveThread (MixCheckProcessor& o) : juce::Thread ("MixCheck live analysis"), owner (o) {}
    void run() override
    {
        while (! threadShouldExit())
        {
            owner.drainLive();
            wait (25);
        }
    }
private:
    MixCheckProcessor& owner;
};

// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MixCheckProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Full Song", "Vocal Only" }, 0));
    StringArray profileNames { "AI Auto" };
    for (auto& pr : mc::profiles()) profileNames.add (pr.en);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "profile", 1 }, "Standard", profileNames, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lang", 1 }, "Language", StringArray { "Thai", "English" }, 0));
    return { p.begin(), p.end() };
}

MixCheckProcessor::MixCheckProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MIXCHECK", createLayout())
{
    formats.registerBasicFormats();
    fifoL.assign ((size_t) kFifoSize, 0.f);
    fifoR.assign ((size_t) kFifoSize, 0.f);
    liveThread = std::make_unique<LiveThread> (*this);
    liveThread->startThread();
}

MixCheckProcessor::~MixCheckProcessor()
{
    capturing = false;
    liveThread->stopThread (2000);
    if (job != nullptr) job->stopThread (5000);
}

void MixCheckProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
}

bool MixCheckProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && out == l.getMainInputChannelSet();
}

void MixCheckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = buffer.getNumChannels();
    if (chans == 0 || n == 0) return;

    // meter (audio passes through untouched)
    const float peak = buffer.getMagnitude (0, n);
    levelDb.store (juce::Decibels::gainToDecibels (peak, -100.f));

    if (! capturing.load()) return;

    int s1, z1, s2, z2;
    fifo.prepareToWrite (n, s1, z1, s2, z2);
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (chans > 1 ? 1 : 0);
    if (z1 > 0) { std::copy (l, l + z1, fifoL.data() + s1); std::copy (r, r + z1, fifoR.data() + s1); }
    if (z2 > 0) { std::copy (l + z1, l + z1 + z2, fifoL.data() + s2); std::copy (r + z1, r + z1 + z2, fifoR.data() + s2); }
    fifo.finishedWrite (z1 + z2);
}

void MixCheckProcessor::drainLive()
{
    const juce::ScopedLock sl (liveLock);
    const int ready = fifo.getNumReady();
    if (ready <= 0) return;
    int s1, z1, s2, z2;
    fifo.prepareToRead (ready, s1, z1, s2, z2);
    if (z1 > 0) liveAnalyzer.process (fifoL.data() + s1, fifoR.data() + s1, z1);
    if (z2 > 0) liveAnalyzer.process (fifoL.data() + s2, fifoR.data() + s2, z2);
    fifo.finishedRead (z1 + z2);
    liveSeconds.store (liveSeconds.load() + (double) (z1 + z2) / sampleRate.load());
}

void MixCheckProcessor::startCapture()
{
    if (analyzing.load()) return;
    capturing = false;
    {
        const juce::ScopedLock sl (liveLock);
        fifo.reset();
        liveAnalyzer.prepare (sampleRate.load());
        liveAnalyzer.setMono (false);
        liveSeconds = 0.0;
    }
    capturing = true;
}

void MixCheckProcessor::stopCaptureAndAnalyze()
{
    if (! capturing.load()) return;
    capturing = false;
    juce::Thread::sleep (60); // let the audio thread finish its current block
    drainLive();

    mc::Result r;
    {
        const juce::ScopedLock sl (liveLock);
        r = liveAnalyzer.finish();
    }
    if (r.valid)
    {
        setError ({});
        publish (r, juce::String::fromUTF8 ("Live capture / บันทึกสด"));
    }
    else
        setError (juce::String::fromUTF8 ("Nothing audible was captured (min. 0.5 s) / ไม่ได้ยินสัญญาณที่บันทึก (อย่างน้อย 0.5 วินาที)"));
}

// ---------------------------------------------------------------- files
void MixCheckProcessor::analyzeFile (const juce::File& file)
{
    if (capturing.load()) { capturing = false; }
    cancelAnalysis();
    setError ({});
    progress = 0.f;
    analyzing = true;
    job = std::make_unique<FileJob> (*this, file);
    job->startThread();
}

void MixCheckProcessor::cancelAnalysis()
{
    if (job != nullptr) { job->stopThread (5000); job.reset(); }
    analyzing = false;
}

void MixCheckProcessor::publish (const mc::Result& r, const juce::String& name)
{
    {
        std::lock_guard<std::mutex> lg (resultLock);
        snapshot.result = r; snapshot.name = name; snapshot.has = true;
    }
    ++version;
}

MixCheckProcessor::Snapshot MixCheckProcessor::getSnapshot() const
{
    std::lock_guard<std::mutex> lg (resultLock);
    return snapshot;
}

void MixCheckProcessor::setError (const juce::String& e)
{
    std::lock_guard<std::mutex> lg (resultLock);
    lastError = e;
}

juce::String MixCheckProcessor::getLastError() const
{
    std::lock_guard<std::mutex> lg (resultLock);
    return lastError;
}

// ---------------------------------------------------------------- params
int MixCheckProcessor::getChoice (const char* id) const
{
    return juce::roundToInt (apvts.getRawParameterValue (id)->load());
}

void MixCheckProcessor::setChoice (const char* id, int index)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void MixCheckProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void MixCheckProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MixCheckProcessor::createEditor() { return new MixCheckEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixCheckProcessor(); }
