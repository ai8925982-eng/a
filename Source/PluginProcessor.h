#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <mutex>
#include "Core/Advisor.h"

class MixCheckProcessor : public juce::AudioProcessor
{
public:
    MixCheckProcessor();
    ~MixCheckProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Mix Check AI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- file analysis (background thread) ----
    void analyzeFile (const juce::File& file);
    void cancelAnalysis();
    bool isAnalyzing() const { return analyzing.load(); }
    float getProgress() const { return progress.load(); }
    juce::String getLastError() const;

    // ---- live capture (from DAW playback / standalone input) ----
    void startCapture();
    void stopCaptureAndAnalyze();
    bool isCapturing() const { return capturing.load(); }
    double getCaptureSeconds() const { return liveSeconds.load(); }
    float getLevelDb() const { return levelDb.load(); }

    // ---- results ----
    struct Snapshot { mc::Result result; juce::String name; bool has = false; };
    Snapshot getSnapshot() const;
    int getResultVersion() const { return version.load(); }

    // ---- parameters (mode / profile / language) ----
    int getChoice (const char* id) const;
    void setChoice (const char* id, int index);

    juce::AudioProcessorValueTreeState apvts;

private:
    class FileJob;
    class LiveThread;
    friend class FileJob;
    friend class LiveThread;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void publish (const mc::Result&, const juce::String& name);
    void drainLive();
    void setError (const juce::String& e);

    juce::AudioFormatManager formats;

    std::unique_ptr<FileJob> job;
    std::atomic<bool> analyzing { false };
    std::atomic<float> progress { 0.f };
    mutable std::mutex resultLock;
    Snapshot snapshot;
    juce::String lastError;
    std::atomic<int> version { 0 };

    // live capture
    static constexpr int kFifoSize = 1 << 19;
    juce::AbstractFifo fifo { kFifoSize };
    std::vector<float> fifoL, fifoR;
    mc::Analyzer liveAnalyzer;
    juce::CriticalSection liveLock;
    std::atomic<bool> capturing { false };
    std::atomic<double> liveSeconds { 0.0 };
    std::atomic<float> levelDb { -100.f };
    std::atomic<double> sampleRate { 44100.0 };
    std::unique_ptr<LiveThread> liveThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixCheckProcessor)
};
