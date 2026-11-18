#pragma once
#include "PluginProcessor.h"

class MixCheckEditor : public juce::AudioProcessorEditor
{
public:
    explicit MixCheckEditor (MixCheckProcessor&);
    ~MixCheckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixCheckEditor)
};
