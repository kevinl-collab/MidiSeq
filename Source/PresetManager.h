#pragma once
#include "ProjectSerializer.h"
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

/** File side of presets: factory list + user presets in
    ~/Library/Application Support/<Company>/<Plugin>/Presets (one .mseq file each), and the global custom-scales file next to them. */
class PresetManager
{
public:
    struct Info
    {
        juce::String name;
        bool factory = false;
        int factoryIndex = -1;
        juce::File file;
    };

    static juce::File rootDirectory();        // .../<Company>/<Plugin>
    static juce::File presetsDirectory();     // .../Presets (created on demand)
    static juce::File scalesFile();           // .../UserScales.xml

    static std::vector<Info> list();          // factory presets first, then user presets A-Z

    /** Saves `tree` (root MSPRESET) as "<name>.mseq", overwriting. */
    static bool writePreset (const juce::String& name, const juce::ValueTree& tree);
    static juce::ValueTree readPreset (const juce::File&);
    static bool deletePreset (const Info&);

    static std::vector<ser::NamedScale> loadUserScales();
    static bool saveUserScales (const std::vector<ser::NamedScale>&);
};
