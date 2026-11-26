// About Patch: the current patch's name, author, description and tags in a panel over the editor. The About...
// button opens it as a read-only view with an Edit button; Save... opens it ready to edit before the file is chosen.
#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

class KMISynthProcessor;

namespace kmiui {

class AboutPatchPanel final : public juce::Component
{
public:
    enum class Mode
    {
        View, // read-only, Edit / Close; Edit switches to editing (Apply / Cancel)
        Save  // editing from the start, Save... / Cancel: applies the details, then the caller picks the file
    };

    AboutPatchPanel(KMISynthProcessor& proc, Mode mode, std::function<void(bool accepted)> onDone);

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusFirstField();

private:
    void finish(bool accept);
    void setEditing(bool editing);
    void loadFields();

    KMISynthProcessor& proc_;
    Mode mode_;
    std::function<void(bool)> onDone_;
    juce::Label title_, nameLabel_, name_, authorLabel_, descLabel_, tagsLabel_, info_;
    juce::TextEditor author_, description_, tags_;
    juce::ToggleButton rememberAuthor_{ "Use this name for my new patches" };
    juce::TextButton ok_, cancel_{ "Cancel" };
    juce::String authorHint_, descHint_, tagsHint_;
    bool editing_ = false;
};

} // namespace kmiui
