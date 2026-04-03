#include "PresetManager.h"
#include "PluginProcessor.h"

const juce::String PresetManager::kExtension = ".npp";

// ── Factory preset definitions ───────────────────────────────────────────────
const PresetManager::FactoryData PresetManager::kFactory[PresetManager::kNumFactory] =
{
    // ── Clean: sparkly clean with a touch of room reverb ──────────────
    { "Clean", {
        {"drive", 1.0f}, {"odTone", 6000.0f}, {"level", 0.85f},
        {"delayTime", 20.0f}, {"delayFeedback", 0.0f}, {"delayTone", 6000.0f}, {"delayMix", 0.0f},
        {"reverbRoom", 0.25f}, {"reverbPreDelay", 15.0f}, {"reverbDamp", 0.6f}, {"reverbMix", 0.12f},
        {"overdriveOn", 0.0f}, {"delayOn", 0.0f}, {"reverbOn", 1.0f}
    }},
    // ── Crunch: edge-of-breakup with slapback ───────────────────────────
    { "Crunch", {
        {"drive", 5.0f}, {"odTone", 5000.0f}, {"level", 0.75f},
        {"delayTime", 120.0f}, {"delayFeedback", 0.25f}, {"delayTone", 5000.0f}, {"delayMix", 0.18f},
        {"reverbRoom", 0.4f}, {"reverbPreDelay", 25.0f}, {"reverbDamp", 0.5f}, {"reverbMix", 0.22f},
        {"overdriveOn", 1.0f}, {"delayOn", 1.0f}, {"reverbOn", 1.0f}
    }},
    // ── Blues OD: warm, singing sustain with lush delay ──────────────────
    { "Blues OD", {
        {"drive", 8.0f}, {"odTone", 3500.0f}, {"level", 0.70f},
        {"delayTime", 380.0f}, {"delayFeedback", 0.40f}, {"delayTone", 4000.0f}, {"delayMix", 0.28f},
        {"reverbRoom", 0.5f}, {"reverbPreDelay", 30.0f}, {"reverbDamp", 0.45f}, {"reverbMix", 0.28f},
        {"overdriveOn", 1.0f}, {"delayOn", 1.0f}, {"reverbOn", 1.0f}
    }},
    // ── Heavy: tight high-gain, dark tone, minimal ambience ─────────────
    { "Heavy", {
        {"drive", 18.0f}, {"odTone", 2500.0f}, {"level", 0.60f},
        {"delayTime", 80.0f}, {"delayFeedback", 0.20f}, {"delayTone", 3500.0f}, {"delayMix", 0.12f},
        {"reverbRoom", 0.45f}, {"reverbPreDelay", 15.0f}, {"reverbDamp", 0.65f}, {"reverbMix", 0.18f},
        {"overdriveOn", 1.0f}, {"delayOn", 1.0f}, {"reverbOn", 1.0f}
    }},
    // ── Space Echo: ambient, dreamy, long dark repeats ───────────────────
    { "Space Echo", {
        {"drive", 3.0f}, {"odTone", 5500.0f}, {"level", 0.78f},
        {"delayTime", 620.0f}, {"delayFeedback", 0.70f}, {"delayTone", 3000.0f}, {"delayMix", 0.55f},
        {"reverbRoom", 0.80f}, {"reverbPreDelay", 40.0f}, {"reverbDamp", 0.30f}, {"reverbMix", 0.45f},
        {"overdriveOn", 1.0f}, {"delayOn", 1.0f}, {"reverbOn", 1.0f}
    }},
};

// ── Constructor ──────────────────────────────────────────────────────────────
PresetManager::PresetManager (NewProjectAudioProcessor& p) : proc (p)
{
    // Scan default folder for user presets on startup
    auto folder = defaultFolder();
    folder.createDirectory();
    scanDirectory (folder);
}

// ── List accessors ───────────────────────────────────────────────────────────
int PresetManager::getNumPresets() const
{
    return kNumFactory + userPresets.size();
}

juce::String PresetManager::getPresetName (int index) const
{
    if (index < kNumFactory)
        return kFactory[index].name;
    const int ui = index - kNumFactory;
    if (ui < userPresets.size())
        return userPresets[ui].name;
    return {};
}

// ── Navigation ───────────────────────────────────────────────────────────────
void PresetManager::loadPreset (int index)
{
    if (index < 0 || index >= getNumPresets()) return;
    currentIndex = index;

    if (isFactory (index))
        applyFactory (index);
    else
        applyFile (userPresets[index - kNumFactory].file);
}

void PresetManager::nextPreset()
{
    loadPreset ((currentIndex + 1) % getNumPresets());
}

void PresetManager::prevPreset()
{
    int n = getNumPresets();
    loadPreset ((currentIndex + n - 1) % n);
}

// ── Save ─────────────────────────────────────────────────────────────────────
void PresetManager::saveCurrentPreset (std::function<void()> onDone)
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Save Preset", defaultFolder(), "*" + kExtension);

    chooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser, onDone] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result == juce::File{}) return;

            auto file = result.withFileExtension (kExtension);

            auto state = proc.apvts.copyState();
            std::unique_ptr<juce::XmlElement> xml (state.createXml());
            if (xml) xml->writeTo (file);

            // Refresh list so the new preset appears
            scanDirectory (defaultFolder());

            // Select the newly saved preset
            for (int i = 0; i < userPresets.size(); ++i)
            {
                if (userPresets[i].file == file)
                {
                    currentIndex = kNumFactory + i;
                    break;
                }
            }

            if (onDone) onDone();
        });
}

// ── Delete ───────────────────────────────────────────────────────────────────
void PresetManager::deletePreset (int index)
{
    if (isFactory (index)) return;
    int ui = index - kNumFactory;
    if (ui < 0 || ui >= userPresets.size()) return;

    userPresets[ui].file.deleteFile();
    userPresets.remove (ui);

    if (currentIndex >= getNumPresets())
        currentIndex = getNumPresets() - 1;
}

// ── Rename ───────────────────────────────────────────────────────────────────
void PresetManager::renamePreset (int index, const juce::String& newName)
{
    if (isFactory (index)) return;
    int ui = index - kNumFactory;
    if (ui < 0 || ui >= userPresets.size()) return;

    auto newFile = userPresets[ui].file.getSiblingFile (
        newName + kExtension);
    if (userPresets[ui].file.moveFileTo (newFile))
    {
        userPresets.getReference (ui).name = newName;
        userPresets.getReference (ui).file = newFile;
    }
}

// ── Scan directory ───────────────────────────────────────────────────────────
void PresetManager::scanDirectory (const juce::File& dir)
{
    userPresets.clear();
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*" + kExtension))
    {
        UserPreset p;
        p.name = f.getFileNameWithoutExtension();
        p.file = f;
        userPresets.add (p);
    }
    std::sort (userPresets.begin(), userPresets.end(),
        [] (const UserPreset& a, const UserPreset& b) {
            return a.name.compareIgnoreCase (b.name) < 0;
        });
}

// ── Private helpers ──────────────────────────────────────────────────────────
juce::File PresetManager::defaultFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("NewProject")
               .getChildFile ("Presets");
}

void PresetManager::applyFactory (int factoryIndex)
{
    const auto& f = kFactory[factoryIndex];
    for (auto& pv : f.params)
    {
        if (auto* p = proc.apvts.getParameter (pv.id))
            p->setValueNotifyingHost (p->convertTo0to1 (pv.value));
    }
}

void PresetManager::applyFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (proc.apvts.state.getType()))
            proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::File PresetManager::userPresetFile (int index) const
{
    return userPresets[index - kNumFactory].file;
}
