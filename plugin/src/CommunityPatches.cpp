#include "CommunityPatches.h"

namespace kmiplug {

namespace {
constexpr juce::int64 kMaxZipBytes = 200 * 1024 * 1024;  // whole download
constexpr juce::int64 kMaxPatchBytes = 5 * 1024 * 1024;  // one patch (user wavetables are embedded)
constexpr int kMaxPatches = 5000;

// Folder and file names: 1-48 characters of letters, digits, space, - _ ', starting with a letter or digit (the
// library's own rule). This also rules out "..", slashes and anything a file system would treat specially.
bool legalName(const juce::String& s)
{
    if (s.isEmpty() || s.length() > 48 || !juce::CharacterFunctions::isLetterOrDigit(s[0])) return false;
    for (auto c : s)
        if (!juce::CharacterFunctions::isLetterOrDigit(c) && c != ' ' && c != '_' && c != '\'' && c != '-') return false;
    return true;
}

// A KMISynth patch: well-formed XML with a <KMISynth> root and no DOCTYPE/ENTITY declarations.
bool isPatchXml(const juce::MemoryBlock& data)
{
    const auto text = data.toString();
    if (text.containsIgnoreCase("<!DOCTYPE") || text.containsIgnoreCase("<!ENTITY")) return false;
    auto xml = juce::parseXML(text);
    return xml != nullptr && xml->hasTagName("KMISynth");
}
} // namespace

juce::URL communityLibraryPage() { return juce::URL("https://github.com/KellyMurphy/KMISynth-UserPatches"); }
juce::URL communityLibraryZip()
{
    return juce::URL("https://github.com/KellyMurphy/KMISynth-UserPatches/archive/refs/heads/main.zip");
}

juce::File communityFolder(const juce::File& userPresetFolder) { return userPresetFolder.getChildFile("Community"); }

CommunityResult installCommunityZip(const juce::MemoryBlock& zipData, const juce::File& dest)
{
    CommunityResult res;
    juce::MemoryInputStream in(zipData, false);
    juce::ZipFile zip(in);
    if (zip.getNumEntries() == 0)
    {
        res.error = "The download is not a valid ZIP file.";
        return res;
    }
    int seen = 0;
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry(i);
        if (entry == nullptr || entry->isSymbolicLink) continue;
        // <repo>-<branch>/Patches/<Category>/<Name>.kmip, nothing deeper or shallower.
        const auto parts = juce::StringArray::fromTokens(entry->filename.replaceCharacter('\\', '/'), "/", "");
        if (parts.size() < 3 || parts[1] != "Patches" || parts[parts.size() - 1].isEmpty()) continue; // folders, other files
        const auto file = parts[parts.size() - 1];
        if (file == ".gitkeep") continue;
        const bool shapeOk = parts.size() == 4 && file.endsWithIgnoreCase(".kmip");
        const auto category = parts[2], name = file.upToLastOccurrenceOf(".", false, false);
        if (!shapeOk || !legalName(category) || !legalName(name) || entry->uncompressedSize <= 0 ||
            entry->uncompressedSize > kMaxPatchBytes || ++seen > kMaxPatches)
        {
            ++res.skipped;
            continue;
        }
        std::unique_ptr<juce::InputStream> s(zip.createStreamForEntry(i));
        juce::MemoryBlock data;
        if (s == nullptr || s->readIntoMemoryBlock(data, kMaxPatchBytes + 1) > static_cast<size_t>(kMaxPatchBytes) ||
            !isPatchXml(data))
        {
            ++res.skipped;
            continue;
        }
        const auto target = dest.getChildFile(category).getChildFile(name + ".kmip");
        if (!target.isAChildOf(dest)) // belt and braces: legalName already excludes path tricks
        {
            ++res.skipped;
            continue;
        }
        if (target.existsAsFile())
        {
            juce::MemoryBlock old;
            if (target.loadFileAsData(old) && old == data)
            {
                ++res.unchanged;
                continue;
            }
        }
        const bool existed = target.existsAsFile();
        if (!target.getParentDirectory().createDirectory() || !target.replaceWithData(data.getData(), data.getSize()))
        {
            res.error = "Could not write " + target.getFullPathName();
            return res;
        }
        ++(existed ? res.updated : res.added);
    }
    res.ok = true;
    return res;
}

CommunityResult downloadCommunityPatches(const juce::File& dest, const std::function<bool()>& shouldStop)
{
    CommunityResult res;
    int status = 0;
    auto stream = communityLibraryZip().createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                               .withConnectionTimeoutMs(20000)
                                                               .withNumRedirectsToFollow(5)
                                                               .withStatusCode(&status));
    if (stream == nullptr || (status != 0 && status != 200))
    {
        res.error = "Could not reach GitHub" + (status > 0 ? " (HTTP " + juce::String(status) + ")" : juce::String()) +
                    ". Check your internet connection.";
        return res;
    }
    juce::MemoryBlock zip;
    juce::HeapBlock<char> buf(65536);
    while (!stream->isExhausted())
    {
        if (shouldStop && shouldStop())
        {
            res.error = "Cancelled.";
            return res;
        }
        const int n = stream->read(buf.get(), 65536);
        if (n < 0)
        {
            res.error = "The download was interrupted.";
            return res;
        }
        if (n == 0) break;
        zip.append(buf.get(), static_cast<size_t>(n));
        if (static_cast<juce::int64>(zip.getSize()) > kMaxZipBytes)
        {
            res.error = "The download is unexpectedly large; stopped.";
            return res;
        }
    }
    return installCommunityZip(zip, dest);
}

} // namespace kmiplug
