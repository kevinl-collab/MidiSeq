#include "PresetManager.h"
#include "Engine/Factory.h"

juce::File PresetManager::rootDirectory()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");   // JUCE reports ~/Library on macOS
   #endif
    return base.getChildFile (JucePlugin_Manufacturer).getChildFile (JucePlugin_Name);
}

juce::File PresetManager::presetsDirectory()
{
    auto dir = rootDirectory().getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::File PresetManager::scalesFile() { return rootDirectory().getChildFile ("UserScales.xml"); }

std::vector<PresetManager::Info> PresetManager::list()
{
    std::vector<Info> out;
    for (int i = 0; i < ms::kNumFactoryPresets; ++i)
        out.push_back ({ ms::kFactoryNames[i], true, i, {} });

    auto files = presetsDirectory().findChildFiles (juce::File::findFiles, false, "*.mseq");
    files.sort();
    for (const auto& f : files)
        out.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });
    return out;
}

bool PresetManager::writePreset (const juce::String& name, const juce::ValueTree& tree)
{
    const auto file = presetsDirectory().getChildFile (juce::File::createLegalFileName (name.trim()) + ".mseq");
    if (auto xml = tree.createXml())
        return xml->writeTo (file);
    return false;
}

juce::ValueTree PresetManager::readPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

bool PresetManager::deletePreset (const Info& info)
{
    return ! info.factory && info.file.existsAsFile() && info.file.deleteFile();
}

std::vector<ser::NamedScale> PresetManager::loadUserScales()
{
    if (auto xml = juce::XmlDocument::parse (scalesFile()))
        return ser::treeToScales (juce::ValueTree::fromXml (*xml));
    return {};
}

bool PresetManager::saveUserScales (const std::vector<ser::NamedScale>& scales)
{
    const auto file = scalesFile();
    file.getParentDirectory().createDirectory();
    if (auto xml = ser::scalesToTree (scales).createXml())
        return xml->writeTo (file);
    return false;
}
