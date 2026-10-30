#include "PresetManager.h"
#include "FactoryPresets.h"
#include "Parameters.h"

namespace forge
{
namespace
{
    /** Input gain calibrates the rig to your guitar and interface, so presets never change it. */
    bool isGlobalSetting (const juce::String& id) { return id == ids::inputGain; }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state,
                              std::function<void (const juce::XmlElement*)> extraLoader,
                              std::function<void (juce::XmlElement&)> extraSaver)
    : apvts (state), loadExtra (std::move (extraLoader)), saveExtra (std::move (extraSaver))
{
    for (auto& id : allParameterIds())
        apvts.addParameterListener (id, this);

    // A new instance holds the parameter defaults, so it is the init patch, not a factory preset
    refresh();
    currentIndex = -1;
    currentName = "Default";
}

PresetManager::~PresetManager()
{
    for (auto& id : allParameterIds())
        apvts.removeParameterListener (id, this);
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (! applying.load())
        dirty = true;
}

bool PresetManager::isCurrentFactory() const
{
    return juce::isPositiveAndBelow (currentIndex, (int) entries.size()) && entries[(size_t) currentIndex].factory;
}

juce::File PresetManager::getUserFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile (info::productName)
               .getChildFile ("Presets");
}

juce::String PresetManager::sanitiseName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 64).trim();
}

void PresetManager::refresh()
{
    entries.clear();

    const auto& factory = factoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        entries.push_back ({ factory[(size_t) i].name, true, i, {} });

    auto folder = getUserFolder();
    std::vector<Entry> user;
    if (folder.isDirectory())
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
            user.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });

    std::sort (user.begin(), user.end(), [] (const Entry& a, const Entry& b)
               { return a.name.compareNatural (b.name) < 0; });

    entries.insert (entries.end(), user.begin(), user.end());
}

void PresetManager::applyValues (const std::map<juce::String, float>& values)
{
    applying = true;
    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || isGlobalSetting (ranged->getParameterID()))
            continue;

        const auto it = values.find (ranged->getParameterID());
        const float normalised = it != values.end() ? ranged->convertTo0to1 (it->second)
                                                    : ranged->getDefaultValue();
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (normalised);
        ranged->endChangeGesture();
    }
    applying = false;
    dirty = false;
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size()))
        return;

    const auto& e = entries[(size_t) index];

    if (e.factory)
    {
        std::map<juce::String, float> values;
        for (auto& [id, v] : factoryPresets()[(size_t) e.factoryIndex].values)
            values[id] = (float) v;
        applyValues (values);
        if (loadExtra) loadExtra (nullptr);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr || ! xml->hasTagName ("ForgePreset"))
            return;

        std::map<juce::String, float> values;
        for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
            values[p->getStringAttribute ("id")] = (float) p->getDoubleAttribute ("value");

        applyValues (values);
        if (loadExtra) loadExtra (xml->getChildByName ("Extra"));
    }

    currentIndex = index;
    currentName = e.name;
    dirty = false;
}

void PresetManager::loadNext()
{
    if (! entries.empty())
        load ((currentIndex + 1) % (int) entries.size());
}

void PresetManager::loadPrevious()
{
    if (! entries.empty())
        load (currentIndex <= 0 ? (int) entries.size() - 1 : currentIndex - 1);
}

void PresetManager::resetToDefaults()
{
    applyValues ({});
    if (loadExtra) loadExtra (nullptr);
    currentIndex = -1;
    currentName = "Default";
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    return getUserFolder().getChildFile (sanitiseName (name) + extension).existsAsFile();
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = sanitiseName (rawName);
    if (name.isEmpty())
        return false;

    auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;

    juce::XmlElement xml ("ForgePreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", info::version);

    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr
                                                                         && ! isGlobalSetting (ranged->getParameterID()))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", ranged->getParameterID());
            e->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
        }

    if (saveExtra)
        saveExtra (*xml.createNewChildElement ("Extra"));

    const auto file = folder.getChildFile (name + extension);
    if (! xml.writeTo (file))
        return false;

    refresh();
    selectByName (name, false);
    dirty = false;
    return true;
}

bool PresetManager::renameCurrent (const juce::String& rawName)
{
    const auto name = sanitiseName (rawName);
    if (name.isEmpty() || isCurrentFactory() || ! juce::isPositiveAndBelow (currentIndex, (int) entries.size()))
        return false;

    const auto oldFile = entries[(size_t) currentIndex].file;
    const auto newFile = oldFile.getSiblingFile (name + extension);
    if (newFile.exists() || ! oldFile.moveFileTo (newFile))
        return false;

    refresh();
    selectByName (name, false);
    return true;
}

bool PresetManager::deleteCurrent()
{
    if (isCurrentFactory() || ! juce::isPositiveAndBelow (currentIndex, (int) entries.size()))
        return false;

    if (! entries[(size_t) currentIndex].file.moveToTrash())
        return false;

    refresh();
    currentIndex = juce::jlimit (0, (int) entries.size() - 1, currentIndex - 1);
    currentName = entries[(size_t) currentIndex].name;
    dirty = true;   // the sound on screen no longer matches any saved preset
    return true;
}

void PresetManager::selectByName (const juce::String& name, bool factory)
{
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].factory == factory && entries[(size_t) i].name == name)
        {
            currentIndex = i;
            currentName = name;
            return;
        }
}

void PresetManager::writeState (juce::ValueTree& state) const
{
    state.setProperty ("presetName", currentName, nullptr);
    state.setProperty ("presetFactory", isCurrentFactory(), nullptr);
    state.setProperty ("presetDirty", isDirty(), nullptr);
}

void PresetManager::readState (const juce::ValueTree& state)
{
    refresh();
    const auto name = state.getProperty ("presetName").toString();
    if (name.isNotEmpty())
    {
        currentIndex = -1;
        currentName = name;
        selectByName (name, (bool) state.getProperty ("presetFactory", true));
    }
    dirty = (bool) state.getProperty ("presetDirty", false);
}
} // namespace forge
