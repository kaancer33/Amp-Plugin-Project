#pragma once
#include <JuceHeader.h>

class NewProjectAudioProcessor;

// Manages factory presets (built-in) + user presets (XML files on disk).
// Factory presets are always first in the list and cannot be deleted or renamed.
class PresetManager
{
public:
    static constexpr int kNumFactory = 5;
    static const juce::String kExtension; // ".npp"

    explicit PresetManager (NewProjectAudioProcessor& p);

    // ── List ────────────────────────────────────────────────────────────────
    int         getNumPresets()      const;
    juce::String getPresetName (int index) const;
    int         getCurrentIndex()    const { return currentIndex; }
    bool        isFactory (int index) const { return index < kNumFactory; }

    // ── Navigation ──────────────────────────────────────────────────────────
    void loadPreset (int index);
    void nextPreset();
    void prevPreset();

    // ── User preset operations ───────────────────────────────────────────────
    // Prompts a FileChooser; saves current APVTS state; refreshes list.
    void saveCurrentPreset (std::function<void()> onDone);
    void deletePreset (int index);       // only for user presets
    void renamePreset (int index, const juce::String& newName); // only user

    // Scan a directory and add found presets.
    void scanDirectory (const juce::File& dir);

    // ── Factory preset data ──────────────────────────────────────────────────
    struct ParamVal { const char* id; float value; };
    struct FactoryData
    {
        const char* name;
        ParamVal params[14];
    };
    static const FactoryData kFactory[kNumFactory];

private:
    NewProjectAudioProcessor& proc;

    struct UserPreset { juce::String name; juce::File file; };
    juce::Array<UserPreset> userPresets;
    int currentIndex = 0;

    juce::File defaultFolder() const;
    void applyFactory (int factoryIndex);
    void applyFile    (const juce::File& f);
    juce::File userPresetFile (int index) const; // index must be >= kNumFactory
};
