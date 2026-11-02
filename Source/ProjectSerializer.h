#pragma once
#include "Engine/Pattern.h"
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

/** Saving / loading of the full project as a juce::ValueTree. Properties equal to their default are omitted, so a
    mostly-empty project stays small. The tree carries a version number so older sessions can be migrated. */
namespace ser
{
constexpr int kStateVersion = 2;   // 1 = parameters only (stages 1-5 builds); 2 = + project, scenes, scales, preset name

struct NamedScale
{
    juce::String name;
    uint16_t mask = 0;
};

juce::ValueTree projectToTree (const ms::Project&, const juce::String& presetName);

/** Returns false if `tree` isn't a PROJECT node. Unknown / missing properties fall back to defaults. */
bool treeToProject (const juce::ValueTree&, ms::Project& out, juce::String* presetName = nullptr);

juce::ValueTree scalesToTree (const std::vector<NamedScale>&);
std::vector<NamedScale> treeToScales (const juce::ValueTree&);

/** Brings a tree written by an older version up to kStateVersion (in place). */
void migrate (juce::ValueTree& root);
} // namespace ser
