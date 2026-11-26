#include "AboutPatch.h"

#include "../PluginProcessor.h"
#include "Theme.h"

namespace kmiui {

namespace {
void styleLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, theme::textDim);
    l.setFont(juce::Font(juce::FontOptions(12.0f)));
    l.setJustificationType(juce::Justification::centredLeft);
}

void styleEditor(juce::TextEditor& e, const juce::String& text, const juce::String& hint)
{
    e.setText(text, false);
    e.setTextToShowWhenEmpty(hint, theme::textDim.withAlpha(0.7f));
    e.setFont(juce::Font(juce::FontOptions(14.0f)));
    e.setColour(juce::TextEditor::backgroundColourId, theme::control);
    e.setColour(juce::TextEditor::outlineColourId, theme::panelEdge);
    e.setColour(juce::TextEditor::focusedOutlineColourId, theme::accent);
    e.setColour(juce::TextEditor::textColourId, theme::text);
    e.setIndents(8, 6);
}
} // namespace

AboutPatchPanel::AboutPatchPanel(KMISynthProcessor& proc, Mode mode, std::function<void(bool)> onDone)
    : proc_(proc), mode_(mode), onDone_(std::move(onDone))
{
    setWantsKeyboardFocus(true);
    title_.setText(mode == Mode::Save ? "Save Patch" : "About Patch", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    title_.setColour(juce::Label::textColourId, theme::text);

    styleLabel(nameLabel_, "Name");
    name_.setText(proc.presetName() + (mode == Mode::Save ? "   (the file name you choose next)" : ""), juce::dontSendNotification);
    name_.setColour(juce::Label::textColourId, theme::text);
    name_.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

    authorHint_ = "Your name or handle";
    descHint_ = "What the sound is and how to play it, e.g. \"Warm FM keys; velocity adds bark, the mod wheel adds tine.\"";
    tagsHint_ = "Comma-separated, e.g. keys, fm, soft";
    styleLabel(authorLabel_, "Author");
    styleEditor(author_, {}, authorHint_);
    author_.setInputRestrictions(64);
    rememberAuthor_.setToggleState(KMISynthProcessor::defaultAuthor().isEmpty(), juce::dontSendNotification);
    rememberAuthor_.setTooltip("Fill this name in automatically when you save a new patch");

    styleLabel(descLabel_, "Description");
    styleEditor(description_, {}, descHint_);
    description_.setMultiLine(true, true);
    description_.setReturnKeyStartsNewLine(true);
    description_.setInputRestrictions(400);

    styleLabel(tagsLabel_, "Tags");
    styleEditor(tags_, {}, tagsHint_);
    tags_.setInputRestrictions(160);

    juce::String meta;
    if (proc.presetCreated().isNotEmpty())
        meta << "First saved " << juce::Time::fromISO8601(proc.presetCreated()).toString(true, false) << ".  ";
    meta << "Shared patches need a description and at least one tag; they are saved inside the .kmip file.";
    styleLabel(info_, meta);
    info_.setFont(juce::Font(juce::FontOptions(11.5f)));

    ok_.setColour(juce::TextButton::buttonColourId, theme::accent.darker(0.4f));
    ok_.onClick = [this] {
        if (editing_) finish(true);
        else setEditing(true);
    };
    cancel_.onClick = [this] { finish(false); };

    for (juce::Component* c : { static_cast<juce::Component*>(&title_), static_cast<juce::Component*>(&nameLabel_),
                                static_cast<juce::Component*>(&name_), static_cast<juce::Component*>(&authorLabel_),
                                static_cast<juce::Component*>(&author_), static_cast<juce::Component*>(&rememberAuthor_),
                                static_cast<juce::Component*>(&descLabel_), static_cast<juce::Component*>(&description_),
                                static_cast<juce::Component*>(&tagsLabel_), static_cast<juce::Component*>(&tags_),
                                static_cast<juce::Component*>(&info_), static_cast<juce::Component*>(&ok_),
                                static_cast<juce::Component*>(&cancel_) })
        addAndMakeVisible(c);
    setEditing(mode == Mode::Save);
}

// The patch's current details; when editing a patch with no author, the remembered name is offered.
void AboutPatchPanel::loadFields()
{
    const bool fresh = proc_.presetAuthor().isEmpty() || (proc_.factoryPresetIndex() >= 0 && mode_ == Mode::Save);
    author_.setText(fresh && editing_ ? KMISynthProcessor::defaultAuthor() : proc_.presetAuthor(), false);
    description_.setText(proc_.presetDescription(), false);
    tags_.setText(proc_.presetTags(), false);
}

void AboutPatchPanel::setEditing(bool editing)
{
    editing_ = editing;
    loadFields();
    const juce::String hints[3] = { authorHint_, descHint_, tagsHint_ };
    juce::TextEditor* eds[3] = { &author_, &description_, &tags_ };
    for (int i = 0; i < 3; ++i)
    {
        auto* e = eds[i];
        e->setReadOnly(!editing);
        e->setCaretVisible(editing);
        e->setColour(juce::TextEditor::backgroundColourId, editing ? theme::control : juce::Colours::transparentBlack);
        e->setColour(juce::TextEditor::outlineColourId, editing ? theme::panelEdge : juce::Colours::transparentBlack);
        e->setColour(juce::TextEditor::focusedOutlineColourId, editing ? theme::accent : juce::Colours::transparentBlack);
        e->setTextToShowWhenEmpty(editing ? hints[i] : juce::String(), theme::textDim.withAlpha(0.7f));
        e->setIndents(editing ? 8 : 0, 6);
    }
    if (!editing)
    {
        if (author_.isEmpty()) author_.setText("-", false);
        if (description_.isEmpty()) description_.setText("No description yet. Click Edit to add one.", false);
        if (tags_.isEmpty()) tags_.setText("-", false);
    }
    rememberAuthor_.setVisible(editing);
    title_.setText(mode_ == Mode::Save ? "Save Patch" : editing ? "Edit Patch Info" : "About Patch", juce::dontSendNotification);
    ok_.setButtonText(mode_ == Mode::Save ? "Save..." : editing ? "Apply" : "Edit");
    cancel_.setButtonText(editing ? "Cancel" : "Close");
    if (editing) focusFirstField();
    repaint();
}

void AboutPatchPanel::focusFirstField()
{
    if (!isShowing()) return;
    if (author_.isEmpty()) author_.grabKeyboardFocus();
    else description_.grabKeyboardFocus();
}

void AboutPatchPanel::finish(bool accept)
{
    if (accept && editing_)
    {
        proc_.setPresetAuthor(author_.getText());
        proc_.setPresetDescription(description_.getText());
        proc_.setPresetTags(tags_.getText().trim());
        if (rememberAuthor_.getToggleState() && author_.getText().trim().isNotEmpty())
            KMISynthProcessor::setDefaultAuthor(author_.getText());
    }
    // Opened as a view: Apply or Cancel goes back to the (updated) view; Close and Save finish.
    if (mode_ == Mode::View && editing_)
    {
        setEditing(false);
        return;
    }
    if (onDone_) onDone_(accept);
}

bool AboutPatchPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (mode_ == Mode::View && editing_) setEditing(false);
        else if (onDone_) onDone_(false);
        return true;
    }
    return false;
}

void AboutPatchPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.55f)); // dim the editor behind
    auto card = getLocalBounds().toFloat().withSizeKeepingCentre(620.0f, 460.0f);
    g.setColour(theme::panel);
    g.fillRoundedRectangle(card, 10.0f);
    g.setColour(theme::panelEdge);
    g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);
}

void AboutPatchPanel::resized()
{
    auto r = getLocalBounds().withSizeKeepingCentre(620, 460).reduced(24, 20);
    title_.setBounds(r.removeFromTop(28));
    r.removeFromTop(10);
    auto row = [&](juce::Label& label, int h) {
        auto line = r.removeFromTop(h);
        label.setBounds(line.removeFromLeft(96).withHeight(30));
        r.removeFromTop(10);
        return line;
    };
    name_.setBounds(row(nameLabel_, 26));
    auto a = row(authorLabel_, 30);
    author_.setBounds(a.removeFromLeft(220));
    a.removeFromLeft(12);
    rememberAuthor_.setBounds(a);
    description_.setBounds(row(descLabel_, 130));
    tags_.setBounds(row(tagsLabel_, 30));
    info_.setBounds(r.removeFromTop(36));
    auto buttons = r.removeFromBottom(32);
    cancel_.setBounds(buttons.removeFromRight(100));
    buttons.removeFromRight(10);
    ok_.setBounds(buttons.removeFromRight(110));
}

} // namespace kmiui
