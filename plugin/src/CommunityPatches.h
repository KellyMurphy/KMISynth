// Community patch library: downloads https://github.com/KellyMurphy/KMISynth-UserPatches and installs its patches
// into the user preset folder (Community/<Category>/<Name>.kmip), where the preset browser lists them.
#pragma once

#include <functional>

#include <juce_core/juce_core.h>

namespace kmiplug {

struct CommunityResult
{
    bool ok = false;
    juce::String error;                     // why it failed (ok == false)
    int added = 0, updated = 0, unchanged = 0;
    int skipped = 0;                        // files in the download that are not valid patches
};

// The library's web page and the ZIP of its main branch.
juce::URL communityLibraryPage();
juce::URL communityLibraryZip();

// The folder community patches go into: <userPresetFolder>/Community.
juce::File communityFolder(const juce::File& userPresetFolder);

// Downloads the library and installs it into `dest` (blocking: call from a background thread). shouldStop is
// polled during the download so the caller can cancel.
CommunityResult downloadCommunityPatches(const juce::File& dest, const std::function<bool()>& shouldStop);

// Installs the patches from a downloaded ZIP of the repository. Only Patches/<Category>/<Name>.kmip entries are
// taken, and only if the names are legal, the size is sane and the file is KMISynth patch XML; anything else is
// counted as skipped. New files are added, changed ones replaced; files already in `dest` that are not in the
// library are left alone.
CommunityResult installCommunityZip(const juce::MemoryBlock& zip, const juce::File& dest);

} // namespace kmiplug
