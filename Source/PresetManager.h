#pragma once
#include <JuceHeader.h>

namespace forge
{
/** Factory presets (compiled in) followed by user presets (.forgepreset XML files).

    Lives in the processor, so the current preset survives closing the editor and is saved
    with the session. Tracks whether the sound was edited since the preset was loaded. */
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr const char* extension = ".forgepreset";

    struct Entry
    {
        juce::String name;
        bool factory = false;
        int factoryIndex = -1;
        juce::File file;
    };

    /** extraLoader is called with a user preset's <Extra> element (e.g. a user IR path). */
    PresetManager (juce::AudioProcessorValueTreeState& state,
                   std::function<void (const juce::XmlElement*)> extraLoader,
                   std::function<void (juce::XmlElement&)> extraSaver);
    ~PresetManager() override;

    const std::vector<Entry>& getEntries() const noexcept { return entries; }
    int  getCurrentIndex() const noexcept                 { return currentIndex; }
    juce::String getCurrentName() const                   { return currentName; }
    bool isDirty() const noexcept                         { return dirty.load(); }
    bool isCurrentFactory() const;

    void load (int index);
    void loadNext();
    void loadPrevious();
    void resetToDefaults();

    bool userPresetExists (const juce::String& name) const;
    bool saveUserPreset (const juce::String& name);
    bool renameCurrent (const juce::String& newName);
    bool deleteCurrent();

    void refresh();
    juce::File getUserFolder() const;

    /** Session persistence of which preset is loaded. */
    void writeState (juce::ValueTree& state) const;
    void readState (const juce::ValueTree& state);

    static juce::String sanitiseName (const juce::String& name);

private:
    void parameterChanged (const juce::String&, float) override;
    void applyValues (const std::map<juce::String, float>& values);
    void selectByName (const juce::String& name, bool factory);

    juce::AudioProcessorValueTreeState& apvts;
    std::function<void (const juce::XmlElement*)> loadExtra;
    std::function<void (juce::XmlElement&)> saveExtra;

    std::vector<Entry> entries;
    int currentIndex = 0;
    juce::String currentName;
    std::atomic<bool> dirty { false };
    std::atomic<bool> applying { false };
};
} // namespace forge
