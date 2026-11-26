// Preset browser: factory and user presets with search, tag filter and folders.
#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

class KMISynthProcessor;

namespace kmiui {

struct PresetEntry
{
    juce::String name, folder, tags, description;
    int factory = -1;     // factory preset index, or -1 for a user file
    juce::File file;
};

// All presets: factory ones (folder "Factory/<category>") and .kmip files below the user folder
// (folder "User" or "User/<subfolder>").
std::vector<PresetEntry> scanPresets(const juce::File& userFolder);

// True if the entry matches every word of `query` (in name, folder, tags or description) and has `tag`
// (empty = any). Case-insensitive.
bool presetMatches(const PresetEntry& e, const juce::String& query, const juce::String& tag);

class PresetBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    PresetBrowser(KMISynthProcessor& proc, std::function<void()> onLoaded, std::function<void()> onClose);
    ~PresetBrowser() override;

    void refresh();          // rescan the disk
    void focusSearch();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    class FolderModel;

    // Presets list (ListBoxModel)
    int getNumRows() override { return static_cast<int>(visible_.size()); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { load(row); }
    void returnKeyPressed(int row) override { load(row); }
    void selectedRowsChanged(int row) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

    void applyFilter();
    void load(int row);
    void showUserMenu(int row);
    void newFolder();
    void getCommunityPatches(); // confirm, then download in the background
    void communityFinished();

    KMISynthProcessor& proc_;
    std::function<void()> onLoaded_, onClose_;
    std::vector<PresetEntry> all_;
    std::vector<int> visible_;
    juce::StringArray folders_;
    juce::String folder_ = "All";

    juce::TextEditor search_;
    juce::ComboBox tagFilter_;
    juce::TextButton newFolder_{ "New Folder..." }, community_{ "Community..." }, refresh_{ "Refresh" }, close_{ "Close" };
    class CommunityThread;
    std::unique_ptr<CommunityThread> communityThread_;
    juce::ListBox folderList_, presetList_;
    std::unique_ptr<FolderModel> folderModel_;
    juce::Label info_;
};

} // namespace kmiui
