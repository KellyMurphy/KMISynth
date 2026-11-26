#include "PresetBrowser.h"

#include "../PluginProcessor.h"
#include "../CommunityPatches.h"
#include "Theme.h"
#include "kmi/Presets.h"

namespace kmiui {

namespace {
juce::String relativeFolder(const juce::File& dir, const juce::File& root)
{
    if (dir == root) return "User";
    return "User/" + dir.getRelativePathFrom(root).replaceCharacter('\\', '/');
}

juce::StringArray splitTags(const juce::String& tags)
{
    juce::StringArray a;
    a.addTokens(tags, ",", "");
    a.trim();
    a.removeEmptyStrings();
    return a;
}
} // namespace

std::vector<PresetEntry> scanPresets(const juce::File& userFolder)
{
    std::vector<PresetEntry> out;
    for (int i = 0; i < kmi::numFactoryPresets(); ++i)
    {
        const auto& info = kmi::factoryPresetInfo(i);
        PresetEntry e;
        e.name = info.name;
        e.folder = juce::String("Factory/") + info.category;
        e.tags = kmi::factoryPresetTags(i);
        e.description = info.description;
        e.factory = i;
        out.push_back(e);
    }
    if (userFolder.isDirectory())
    {
        auto files = userFolder.findChildFiles(juce::File::findFiles, true, "*" + KMISynthProcessor::presetExtension());
        files.sort();
        for (const auto& f : files)
        {
            PresetEntry e;
            e.name = f.getFileNameWithoutExtension();
            e.folder = relativeFolder(f.getParentDirectory(), userFolder);
            e.tags = KMISynthProcessor::readPresetFileTags(f);
            e.file = f;
            out.push_back(e);
        }
    }
    return out;
}

bool presetMatches(const PresetEntry& e, const juce::String& query, const juce::String& tag)
{
    if (tag.isNotEmpty() && !splitTags(e.tags).contains(tag, true)) return false;
    juce::StringArray words;
    words.addTokens(query, " ", "\"");
    words.removeEmptyStrings();
    const juce::String hay = (e.name + " " + e.folder + " " + e.tags + " " + e.description).toLowerCase();
    for (const auto& w : words)
        if (!hay.contains(w.toLowerCase())) return false;
    return true;
}

// ---- Folder list --------------------------------------------------------------------------------------------
class PresetBrowser::FolderModel final : public juce::ListBoxModel
{
public:
    explicit FolderModel(PresetBrowser& b) : b_(b) {}
    int getNumRows() override { return b_.folders_.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= b_.folders_.size()) return;
        const juce::String f = b_.folders_[row];
        if (selected) g.fillAll(theme::accent.withAlpha(0.35f));
        const int depth = f.retainCharacters("/").length();
        const juce::String label = depth == 0 ? f : f.fromLastOccurrenceOf("/", false, false);
        g.setColour(depth == 0 ? theme::text : theme::textDim.brighter(0.3f));
        g.setFont(juce::Font(juce::FontOptions(13.5f, depth == 0 ? juce::Font::bold : juce::Font::plain)));
        g.drawText(label, 10 + depth * 14, 0, w - 12 - depth * 14, h, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged(int row) override
    {
        if (row < 0 || row >= b_.folders_.size()) return;
        b_.folder_ = b_.folders_[row];
        b_.applyFilter();
    }

private:
    PresetBrowser& b_;
};

// ---- Browser ------------------------------------------------------------------------------------------------
PresetBrowser::PresetBrowser(KMISynthProcessor& proc, std::function<void()> onLoaded, std::function<void()> onClose)
    : proc_(proc), onLoaded_(std::move(onLoaded)), onClose_(std::move(onClose))
{
    setWantsKeyboardFocus(true);
    search_.setTextToShowWhenEmpty("Search name, category, tag or description...", theme::textDim);
    search_.onTextChange = [this] { applyFilter(); };
    search_.onReturnKey = [this] { load(presetList_.getSelectedRow()); };
    search_.onEscapeKey = [this] { if (onClose_) onClose_(); };
    addAndMakeVisible(search_);

    tagFilter_.onChange = [this] { applyFilter(); };
    tagFilter_.setTooltip("Only presets with this tag");
    addAndMakeVisible(tagFilter_);

    newFolder_.onClick = [this] { newFolder(); };
    newFolder_.setTooltip("Create a folder for your presets (inside the selected user folder)");
    refresh_.onClick = [this] { refresh(); };
    close_.onClick = [this] { if (onClose_) onClose_(); };
    community_.onClick = [this] { getCommunityPatches(); };
    community_.setTooltip("Download or update the community patch library from GitHub (into User/Community)");
    for (auto* b : { &newFolder_, &community_, &refresh_, &close_ }) addAndMakeVisible(*b);

    folderModel_ = std::make_unique<FolderModel>(*this);
    folderList_.setModel(folderModel_.get());
    folderList_.setRowHeight(24);
    presetList_.setModel(this);
    presetList_.setRowHeight(26);
    for (auto* l : { &folderList_, &presetList_ })
    {
        l->setColour(juce::ListBox::backgroundColourId, theme::background);
        l->setColour(juce::ListBox::outlineColourId, theme::panelEdge);
        l->setOutlineThickness(1);
        addAndMakeVisible(*l);
    }
    info_.setColour(juce::Label::textColourId, theme::textDim);
    info_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(info_);
    refresh();
}

// Downloads the community library off the message thread, then reports back on it.
class PresetBrowser::CommunityThread final : public juce::Thread
{
public:
    CommunityThread(PresetBrowser& owner, juce::File dest) : juce::Thread("KMISynth community patches"), owner_(&owner), dest_(std::move(dest)) {}
    ~CommunityThread() override { stopThread(25000); }
    void run() override
    {
        result = kmiplug::downloadCommunityPatches(dest_, [this] { return threadShouldExit(); });
        juce::MessageManager::callAsync([safe = owner_] {
            if (safe) safe->communityFinished();
        });
    }
    kmiplug::CommunityResult result;

private:
    juce::Component::SafePointer<PresetBrowser> owner_;
    juce::File dest_;
};

void PresetBrowser::getCommunityPatches()
{
    if (communityThread_ != nullptr) return; // already running
    const auto dest = kmiplug::communityFolder(KMISynthProcessor::userPresetFolder());
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Community patches")
            .withMessage("Download the latest patches from the community library on GitHub?\n\n" +
                         kmiplug::communityLibraryPage().toString(false) + "\n\nThey go into " + dest.getFullPathName() +
                         " and appear under User > Community. New patches are added and updated ones replaced; "
                         "files you add to that folder yourself are kept.\n\nPatches are shared by users under CC BY 4.0.")
            .withButton("Download")
            .withButton("Open Website")
            .withButton("Cancel"),
        [safe = juce::Component::SafePointer<PresetBrowser>(this), dest](int result) {
            if (!safe) return;
            if (result == 2)
            {
                kmiplug::communityLibraryPage().launchInDefaultBrowser();
                return;
            }
            if (result != 1) return;
            safe->community_.setEnabled(false);
            safe->info_.setText("Downloading community patches...", juce::dontSendNotification);
            safe->communityThread_ = std::make_unique<CommunityThread>(*safe, dest);
            safe->communityThread_->startThread();
        });
}

void PresetBrowser::communityFinished()
{
    if (communityThread_ == nullptr) return;
    communityThread_->stopThread(1000);
    const auto r = communityThread_->result;
    communityThread_.reset();
    community_.setEnabled(true);
    juce::String msg;
    if (!r.ok) msg = "The download failed: " + r.error;
    else
    {
        msg = juce::String(r.added) + " new, " + juce::String(r.updated) + " updated, " + juce::String(r.unchanged) + " unchanged.";
        if (r.skipped > 0) msg << "\n" << r.skipped << " file(s) in the library were not valid patches and were skipped.";
        if (r.added + r.updated + r.unchanged > 0) folder_ = "User/Community";
    }
    refresh();
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(r.ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon)
                                     .withTitle("Community patches")
                                     .withMessage(msg)
                                     .withButton("OK"),
                                 nullptr);
}

PresetBrowser::~PresetBrowser()
{
    communityThread_.reset(); // waits for (or cancels) a download in progress
    folderList_.setModel(nullptr);
    presetList_.setModel(nullptr);
}

void PresetBrowser::refresh()
{
    const auto root = KMISynthProcessor::userPresetFolder();
    all_ = scanPresets(root);

    folders_.clear();
    folders_.add("All");
    folders_.add("Factory");
    for (const auto& e : all_)
        if (e.factory >= 0) folders_.addIfNotAlreadyThere(e.folder);
    folders_.add("User");
    auto dirs = root.findChildFiles(juce::File::findDirectories, true);
    dirs.sort();
    for (const auto& d : dirs) folders_.addIfNotAlreadyThere(relativeFolder(d, root));
    if (!folders_.contains(folder_)) folder_ = "All";
    folderList_.updateContent();
    folderList_.selectRow(folders_.indexOf(folder_), true, true);

    juce::StringArray tags;
    for (const auto& e : all_)
        for (const auto& t : splitTags(e.tags)) tags.addIfNotAlreadyThere(t.toLowerCase());
    tags.sort(true);
    const juce::String current = tagFilter_.getSelectedId() > 1 ? tagFilter_.getText() : juce::String();
    tagFilter_.clear(juce::dontSendNotification);
    tagFilter_.addItem("All tags", 1);
    for (int i = 0; i < tags.size(); ++i) tagFilter_.addItem(tags[i], i + 2);
    const int keep = tags.indexOf(current);
    tagFilter_.setSelectedId(keep >= 0 ? keep + 2 : 1, juce::dontSendNotification);
    applyFilter();
}

void PresetBrowser::applyFilter()
{
    const juce::String tag = tagFilter_.getSelectedId() > 1 ? tagFilter_.getText() : juce::String();
    visible_.clear();
    for (int i = 0; i < static_cast<int>(all_.size()); ++i)
    {
        const auto& e = all_[static_cast<size_t>(i)];
        const bool inFolder = folder_ == "All" || e.folder == folder_ || e.folder.startsWith(folder_ + "/");
        if (inFolder && presetMatches(e, search_.getText(), tag)) visible_.push_back(i);
    }
    presetList_.updateContent();
    presetList_.repaint();
    // Keep the loaded preset selected when it is visible, otherwise select the first match.
    int sel = visible_.empty() ? -1 : 0;
    for (int r = 0; r < static_cast<int>(visible_.size()); ++r)
    {
        const auto& e = all_[static_cast<size_t>(visible_[static_cast<size_t>(r)])];
        if ((e.factory >= 0 && e.factory == proc_.factoryPresetIndex()) ||
            (e.factory < 0 && proc_.factoryPresetIndex() < 0 && e.name == proc_.presetName()))
            sel = r;
    }
    if (sel >= 0) presetList_.selectRow(sel, true, true);
    else info_.setText(juce::String(static_cast<int>(visible_.size())) + " presets", juce::dontSendNotification);
}

void PresetBrowser::selectedRowsChanged(int row)
{
    if (row < 0 || row >= static_cast<int>(visible_.size()))
    {
        info_.setText(juce::String(static_cast<int>(visible_.size())) + " presets", juce::dontSendNotification);
        return;
    }
    const auto& e = all_[static_cast<size_t>(visible_[static_cast<size_t>(row)])];
    juce::String text = e.description.isNotEmpty() ? e.description : (e.factory < 0 ? e.file.getFullPathName() : juce::String());
    info_.setText(juce::String(static_cast<int>(visible_.size())) + " presets    " + e.name + ": " + text, juce::dontSendNotification);
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= static_cast<int>(visible_.size())) return;
    const auto& e = all_[static_cast<size_t>(visible_[static_cast<size_t>(row)])];
    const bool loaded = (e.factory >= 0 && e.factory == proc_.factoryPresetIndex()) ||
                        (e.factory < 0 && proc_.factoryPresetIndex() < 0 && e.name == proc_.presetName());
    if (selected) g.fillAll(theme::accent.withAlpha(0.35f));
    else if (row % 2) g.fillAll(theme::panel.withAlpha(0.5f));
    const int nameW = w * 34 / 100, folderW = w * 22 / 100;
    g.setColour(loaded ? theme::accentWarm : theme::text);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.drawText(e.name, 10, 0, nameW - 12, h, juce::Justification::centredLeft, true);
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.setColour(theme::textDim.brighter(0.2f));
    g.drawText(e.folder, nameW, 0, folderW - 6, h, juce::Justification::centredLeft, true);
    g.setColour(theme::textDim);
    g.drawText(e.tags, nameW + folderW, 0, w - nameW - folderW - 8, h, juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showUserMenu(row);
}

void PresetBrowser::load(int row)
{
    if (row < 0 || row >= static_cast<int>(visible_.size())) return;
    const auto& e = all_[static_cast<size_t>(visible_[static_cast<size_t>(row)])];
    if (e.factory >= 0) proc_.loadFactoryPreset(e.factory);
    else if (!proc_.loadPreset(e.file)) return;
    presetList_.repaint();
    if (onLoaded_) onLoaded_();
    if (onClose_) onClose_(); // a loaded preset closes the browser
}

void PresetBrowser::showUserMenu(int row)
{
    if (row < 0 || row >= static_cast<int>(visible_.size())) return;
    const PresetEntry e = all_[static_cast<size_t>(visible_[static_cast<size_t>(row)])];
    juce::PopupMenu menu;
    menu.addSectionHeader(e.name);
    menu.addItem(1, "Load");
    const bool user = e.factory < 0;
    menu.addItem(2, "Edit tags...", user);
    menu.addItem(3, "Show in Explorer", user);
    menu.addSeparator();
    menu.addItem(4, "Delete...", user);
    menu.showMenuAsync(juce::PopupMenu::Options(), [safe = juce::Component::SafePointer<PresetBrowser>(this), e, row](int r) {
        if (!safe || r == 0) return;
        if (r == 1) safe->load(row);
        else if (r == 2)
        {
            auto* w = new juce::AlertWindow("Tags for " + e.name, "Comma-separated, for example: bass, fm, dark",
                                            juce::MessageBoxIconType::NoIcon);
            w->addTextEditor("tags", e.tags);
            w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, e](int result) {
                                   if (result != 1 || !safe) return;
                                   const auto tags = w->getTextEditorContents("tags");
                                   KMISynthProcessor::writePresetFileTags(e.file, tags);
                                   if (safe->proc_.factoryPresetIndex() < 0 && safe->proc_.presetName() == e.name)
                                       safe->proc_.setPresetTags(tags);
                                   safe->refresh();
                               }),
                               true);
        }
        else if (r == 3) e.file.revealToUser();
        else if (r == 4)
        {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                             .withIconType(juce::MessageBoxIconType::WarningIcon)
                                             .withTitle("Delete preset")
                                             .withMessage("Move \"" + e.name + "\" to the Recycle Bin?")
                                             .withButton("Delete")
                                             .withButton("Cancel"),
                                         [safe, e](int result) {
                                             if (result != 1 || !safe) return;
                                             e.file.moveToTrash();
                                             safe->refresh();
                                         });
        }
    });
}

void PresetBrowser::newFolder()
{
    const auto root = KMISynthProcessor::userPresetFolder();
    juce::File parent = root;
    if (folder_.startsWith("User/")) parent = root.getChildFile(folder_.fromFirstOccurrenceOf("User/", false, false));
    auto* w = new juce::AlertWindow("New preset folder", "Inside: " + parent.getFullPathName(), juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", "New Folder");
    w->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<PresetBrowser>(this), w, parent, root](int result) {
                           if (result != 1 || !safe) return;
                           const auto name = juce::File::createLegalFileName(w->getTextEditorContents("name").trim());
                           if (name.isEmpty()) return;
                           const auto dir = parent.getChildFile(name);
                           dir.createDirectory();
                           safe->folder_ = relativeFolder(dir, root);
                           safe->refresh();
                       }),
                       true);
}

void PresetBrowser::focusSearch()
{
    search_.grabKeyboardFocus();
    search_.selectAll();
}

bool PresetBrowser::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose_) onClose_();
        return true;
    }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const int n = static_cast<int>(visible_.size());
        if (n == 0) return true;
        const int cur = presetList_.getSelectedRow();
        const int next = juce::jlimit(0, n - 1, cur + (key == juce::KeyPress::upKey ? -1 : 1));
        presetList_.selectRow(next);
        return true;
    }
    return false;
}

void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(theme::panel);
    g.setColour(theme::panelEdge);
    g.drawRect(getLocalBounds(), 1);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(30);
    close_.setBounds(top.removeFromRight(80));
    top.removeFromRight(6);
    refresh_.setBounds(top.removeFromRight(80));
    top.removeFromRight(6);
    community_.setBounds(top.removeFromRight(110));
    top.removeFromRight(6);
    newFolder_.setBounds(top.removeFromRight(110));
    top.removeFromRight(12);
    tagFilter_.setBounds(top.removeFromRight(180));
    top.removeFromRight(8);
    search_.setBounds(top);
    r.removeFromTop(8);
    info_.setBounds(r.removeFromBottom(26));
    r.removeFromBottom(4);
    folderList_.setBounds(r.removeFromLeft(230));
    r.removeFromLeft(8);
    presetList_.setBounds(r);
}

} // namespace kmiui
