#include "PluginEditor.h"

#include "kmi/Modulation.h"
#include "kmi/Presets.h"
#include "ui/Pages.h"
#include "ui/Theme.h"

namespace {
constexpr int kInitItem = 10000; // preset menu id for Init (factory presets use index + 1)
constexpr int kClearMidiItem = 10001;

juce::File presetFolder()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("KMISynth").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}
} // namespace

void KMISynthEditor::Surface::paint(juce::Graphics& g)
{
    g.fillAll(kmiui::theme::background);
    g.setColour(kmiui::theme::panel);
    g.fillRect(0, 0, getWidth(), 46);
    g.setColour(kmiui::theme::panelEdge);
    g.drawHorizontalLine(46, 0.0f, static_cast<float>(getWidth()));
}

KMISynthEditor::KMISynthEditor(KMISynthProcessor& p)
    : AudioProcessorEditor(p), proc_(p), uiPatch_(kmi::makeInitPatch()), ctx_{ p, p.state(), uiPatch_ },
      keyboard_(p.keyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lnf_);
    uiBinding_.bind(p.state(), uiPatch_);
    uiBinding_.apply();

    addAndMakeVisible(surface_);

    title_.setText("KMISynth", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    title_.setColour(juce::Label::textColourId, kmiui::theme::accent);
    status_.setJustificationType(juce::Justification::centredRight);
    status_.setColour(juce::Label::textColourId, kmiui::theme::textDim);
    for (juce::Component* c : { static_cast<juce::Component*>(&title_), static_cast<juce::Component*>(&presetButton_),
                                static_cast<juce::Component*>(&prevButton_), static_cast<juce::Component*>(&nextButton_),
                                static_cast<juce::Component*>(&status_), static_cast<juce::Component*>(&initButton_),
                                static_cast<juce::Component*>(&loadButton_), static_cast<juce::Component*>(&saveButton_),
                                static_cast<juce::Component*>(&undoButton_), static_cast<juce::Component*>(&redoButton_),
                                static_cast<juce::Component*>(&keyboard_) })
        surface_.addAndMakeVisible(c);

    initButton_.setTooltip("Reset every parameter to the Init patch");
    loadButton_.setTooltip("Load a .kmip preset");
    saveButton_.setTooltip("Save the current sound as a .kmip preset");
    presetButton_.setTooltip("Browse the factory presets");
    prevButton_.setTooltip("Previous factory preset");
    nextButton_.setTooltip("Next factory preset");
    presetButton_.onClick = [this] { showPresetMenu(); };
    prevButton_.onClick = [this] { stepFactoryPreset(-1); };
    nextButton_.onClick = [this] { stepFactoryPreset(1); };
    initButton_.onClick = [this] {
        proc_.initPatch();
        afterPresetChange();
    };
    loadButton_.onClick = [this] { loadPreset(); };
    saveButton_.onClick = [this] { savePreset(); };
    undoButton_.onClick = [this] { proc_.undoManager().undo(); };
    redoButton_.onClick = [this] { proc_.undoManager().redo(); };

    keyboard_.setLowestVisibleKey(36);
    keyboard_.setKeyWidth(18.0f);
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, kmiui::theme::accent.withAlpha(0.6f));
    keyboard_.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kmiui::theme::accent.withAlpha(0.25f));

    tabs_ = std::make_unique<kmiui::Selector>(kmiui::pageNames(), pageIndex_, [this](int i) {
        // Defer: the click comes from a button inside the tab bar, page swaps are safe on the next tick.
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<KMISynthEditor>(this), i] {
            if (safe) safe->showPage(i);
        });
    });
    surface_.addAndMakeVisible(*tabs_);
    showPage(0);

    setResizable(true, true);
    setResizeLimits(kDesignW * 3 / 4, kDesignH * 3 / 4, kDesignW * 2, kDesignH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(static_cast<double>(kDesignW) / kDesignH);
    setSize(kDesignW, kDesignH);
    startTimerHz(30);

    // Developer aid: KMI_UI_SNAPSHOT_DIR=<folder> renders every page to PNG (used to review the UI).
    const auto snapDir = juce::SystemStats::getEnvironmentVariable("KMI_UI_SNAPSHOT_DIR", {});
    if (snapDir.isNotEmpty())
    {
        verifyFactoryPresets(juce::File(snapDir));
        verifyMidiLearn(juce::File(snapDir));
        snapshotPages(juce::File(snapDir), 0);
    }
}

void KMISynthEditor::snapshotPages(juce::File dir, int index)
{
    dir.createDirectory();
    const int numPages = kmiui::pageNames().size();
    // Second pass (indices numPages..): non-default states that switch in extra views.
    static const int variantPages[] = { 0, 1, 5, 8 };
    if (index == numPages)
    {
        kmiui::setParam(ctx_, "algo_index", 13.0f);      // Double Diamond
        kmiui::setParam(ctx_, "osc2_level", 90.0f);
        kmiui::setParam(ctx_, "osc5_feedback", 40.0f);
        kmiui::setParam(ctx_, "lfo1_wave", static_cast<float>(kmi::LfoWave::Step));
        kmiui::setParam(ctx_, "prefx_type", static_cast<float>(kmi::FxType::Chorus));
        kmiui::setParam(ctx_, "postfx_type", static_cast<float>(kmi::FxType::Distort));
        kmiui::setParam(ctx_, "mod1_src", static_cast<float>(kmi::src::Lfo1));
        kmiui::setParam(ctx_, "mod1_dst", static_cast<float>(kmi::dst::DFiltBase + kmi::dst::DfCutoff));
        kmiui::setParam(ctx_, "mod1_depth", -40.0f);
        auto steps = proc_.getLfoSteps(0);
        for (int i = 0; i < 16; ++i) steps[static_cast<size_t>(i)] = std::sin(i * 0.7f) * 0.8f;
        proc_.setLfoSteps(0, steps);
    }
    if (index >= numPages + static_cast<int>(std::size(variantPages))) return;
    const int page = index < numPages ? index : variantPages[index - numPages];
    juce::Timer::callAfterDelay(index == numPages ? 200 : 0, [safe = juce::Component::SafePointer<KMISynthEditor>(this), page] {
        if (safe) safe->showPage(page);
    });
    juce::Timer::callAfterDelay(index == numPages ? 900 : 600, [safe = juce::Component::SafePointer<KMISynthEditor>(this), dir, index] {
        if (!safe) return;
        const auto image = safe->surface_.createComponentSnapshot(safe->surface_.getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream out(dir.getChildFile(juce::String(index).paddedLeft('0', 2) + ".png"));
        if (out.openedOk())
        {
            out.setPosition(0);
            out.truncate();
            png.writeImageToStream(image, out);
        }
        safe->snapshotPages(dir, index + 1);
    });
}

KMISynthEditor::~KMISynthEditor()
{
    stopTimer();
    page_.reset();
    setLookAndFeel(nullptr);
}

void KMISynthEditor::showPage(int index)
{
    pageIndex_ = index;
    if (tabs_) tabs_->setSelected(index);
    page_.reset();
    page_ = kmiui::createPage(index, ctx_);
    surface_.addAndMakeVisible(*page_);
    layoutSurface();
}

void KMISynthEditor::layoutSurface()
{
    auto r = juce::Rectangle<int>(0, 0, kDesignW, kDesignH);
    auto header = r.removeFromTop(46).reduced(10, 7);
    title_.setBounds(header.removeFromLeft(120));
    prevButton_.setBounds(header.removeFromLeft(26));
    header.removeFromLeft(3);
    presetButton_.setBounds(header.removeFromLeft(230));
    header.removeFromLeft(3);
    nextButton_.setBounds(header.removeFromLeft(26));
    header.removeFromLeft(10);
    for (auto* b : { &initButton_, &loadButton_, &saveButton_ })
    {
        b->setBounds(header.removeFromLeft(72));
        header.removeFromLeft(6);
    }
    header.removeFromLeft(14);
    for (auto* b : { &undoButton_, &redoButton_ })
    {
        b->setBounds(header.removeFromLeft(60));
        header.removeFromLeft(6);
    }
    status_.setBounds(header);

    keyboard_.setBounds(r.removeFromBottom(70).reduced(8, 4));
    r.removeFromTop(6);
    tabs_->setBounds(r.removeFromTop(30).reduced(8, 0));
    if (page_) page_->setBounds(r);
}

void KMISynthEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kDesignW);
    surface_.setBounds(0, 0, kDesignW, kDesignH);
    surface_.setTransform(juce::AffineTransform::scale(scale));
    layoutSurface();
}

void KMISynthEditor::timerCallback()
{
    uiBinding_.apply(); // refresh the view snapshot of all parameters

    const int fi = proc_.factoryPresetIndex();
    const juce::String label = (fi >= 0 ? juce::String(kmi::factoryPresetInfo(fi).category) + " / " : juce::String()) +
                               proc_.presetName() + juce::String::fromUTF8("  \xe2\x96\xbe");
    if (presetButton_.getButtonText() != label) presetButton_.setButtonText(label);
    const juce::String learning = proc_.midiLearnTarget();
    if (learning.isNotEmpty())
    {
        auto* p = proc_.state().getParameter(learning);
        status_.setColour(juce::Label::textColourId, kmiui::theme::accentWarm);
        status_.setText("MIDI Learn: move a controller for " + (p ? p->getName(40) : learning) + "  (Esc cancels)",
                        juce::dontSendNotification);
    }
    else
    {
        status_.setColour(juce::Label::textColourId, kmiui::theme::textDim);
        const int maps = proc_.numMidiMappings();
        status_.setText((maps > 0 ? "MIDI " + juce::String(maps) + "    " : juce::String()) + "Voices " +
                            juce::String(proc_.activeVoices()) + " / " + juce::String(kmi::kMaxVoices) + "    " +
                            juce::String(juce::roundToInt(100.0f * getWidth() / kDesignW)) + "%",
                        juce::dontSendNotification);
    }
    undoButton_.setEnabled(proc_.undoManager().canUndo());
    redoButton_.setEnabled(proc_.undoManager().canRedo());

    // Group edits into undo steps: start a new transaction after the mouse has been idle.
    if (juce::Desktop::getInstance().getMainMouseSource().isDragging()) transactionTicks_ = 0;
    else if (++transactionTicks_ == 10) proc_.undoManager().beginNewTransaction();
}

void KMISynthEditor::loadPreset()
{
    chooser_ = std::make_unique<juce::FileChooser>("Load KMISynth preset", presetFolder(), "*" + KMISynthProcessor::presetExtension());
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<KMISynthEditor>(this)](const juce::FileChooser& fc) {
                              if (!safe) return;
                              const auto file = fc.getResult();
                              if (file == juce::File()) return;
                              if (!safe->proc_.loadPreset(file))
                                  juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Load failed",
                                                                         "Not a KMISynth preset: " + file.getFullPathName());
                              safe->afterPresetChange();
                          });
}

void KMISynthEditor::savePreset()
{
    chooser_ = std::make_unique<juce::FileChooser>("Save KMISynth preset",
                                                   presetFolder().getChildFile(proc_.presetName() + KMISynthProcessor::presetExtension()),
                                                   "*" + KMISynthProcessor::presetExtension());
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<KMISynthEditor>(this)](const juce::FileChooser& fc) {
                              if (!safe) return;
                              auto file = fc.getResult();
                              if (file == juce::File()) return;
                              file = file.withFileExtension(KMISynthProcessor::presetExtension());
                              if (!safe->proc_.savePreset(file))
                                  juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save failed",
                                                                         "Could not write " + file.getFullPathName());
                          });
}

void KMISynthEditor::afterPresetChange()
{
    uiBinding_.apply();
    showPage(pageIndex_); // rebuild: views that show non-parameter data (step LFOs, custom matrix) re-read it
}

void KMISynthEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const int cur = proc_.factoryPresetIndex();
    juce::StringArray categories;
    for (int i = 0; i < kmi::numFactoryPresets(); ++i) categories.addIfNotAlreadyThere(kmi::factoryPresetInfo(i).category);
    for (const auto& cat : categories)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < kmi::numFactoryPresets(); ++i)
        {
            const auto& info = kmi::factoryPresetInfo(i);
            if (cat == info.category) sub.addItem(i + 1, info.name, true, i == cur);
        }
        menu.addSubMenu(cat, sub, true, nullptr, cur >= 0 && cat == juce::String(kmi::factoryPresetInfo(cur).category));
    }
    menu.addSeparator();
    menu.addItem(kInitItem, "Init");
    const int maps = proc_.numMidiMappings();
    menu.addItem(kClearMidiItem, "Clear all MIDI mappings" + (maps > 0 ? " (" + juce::String(maps) + ")" : juce::String()), maps > 0);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton_),
                       [safe = juce::Component::SafePointer<KMISynthEditor>(this)](int result) {
                           if (!safe || result == 0) return;
                           if (result == kClearMidiItem)
                           {
                               safe->proc_.clearAllMidiCcs();
                               return;
                           }
                           if (result == kInitItem) safe->proc_.initPatch();
                           else safe->proc_.loadFactoryPreset(result - 1);
                           safe->afterPresetChange();
                       });
}

// Developer aid (snapshot mode): exercise MIDI learn on a separate, headless processor instance.
// Results go to midi-check.txt.
void KMISynthEditor::verifyMidiLearn(juce::File dir)
{
    juce::StringArray log;
    bool allOk = true;
    auto check = [&](bool ok, const juce::String& what) {
        log.add((ok ? "OK   " : "FAIL ") + what);
        allOk = allOk && ok;
    };
    KMISynthProcessor p;
    p.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> buf(2, 256);
    auto sendCc = [&](KMISynthProcessor& proc, int cc, int value) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, value), 10);
        buf.clear();
        proc.processBlock(buf, midi);
    };
    auto value = [](KMISynthProcessor& proc, const char* id) { return proc.state().getParameter(id)->getValue(); };

    sendCc(p, 21, 100);
    check(p.numMidiMappings() == 0, "an unmapped CC changes nothing");
    p.startMidiLearn("dfilt_cutoff");
    check(p.midiLearnTarget() == "dfilt_cutoff", "learn armed");
    sendCc(p, 21, 100);
    check(p.midiCcFor("dfilt_cutoff") == 21 && p.midiLearnTarget().isEmpty(), "CC 21 learned and learn disarmed");
    check(std::abs(value(p, "dfilt_cutoff") - 100.0f / 127.0f) < 1e-3f, "the learning CC also sets the value");
    sendCc(p, 21, 0);
    check(value(p, "dfilt_cutoff") < 1e-3f, "CC 21 drives the cutoff");

    p.startMidiLearn("dfilt_cutoff");
    sendCc(p, 22, 64);
    check(p.midiCcFor("dfilt_cutoff") == 22 && p.numMidiMappings() == 1, "relearning moves the parameter to CC 22");
    p.startMidiLearn("dfilt_res");
    sendCc(p, 22, 127);
    check(p.midiCcFor("dfilt_res") == 22 && p.midiCcFor("dfilt_cutoff") < 0, "a CC drives one parameter only");
    p.startMidiLearn("arp_on");
    sendCc(p, 23, 127);
    check(p.midiCcFor("arp_on") == 23 && value(p, "arp_on") > 0.5f, "toggles can be learned");
    p.startMidiLearn("macro1_value");
    p.cancelMidiLearn();
    sendCc(p, 24, 127);
    check(p.midiCcFor("macro1_value") < 0, "cancel");

    juce::MemoryBlock session;
    p.getStateInformation(session);
    KMISynthProcessor q;
    q.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    check(q.midiCcFor("dfilt_res") == 22 && q.midiCcFor("arp_on") == 23 && q.numMidiMappings() == 2, "saved with the session");

    q.loadFactoryPreset(3);
    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kmi-midi-check.kmip");
    q.savePreset(tmp);
    q.loadPreset(tmp);
    tmp.deleteFile();
    check(q.numMidiMappings() == 2, "preset load and save keep the controller map");
    q.prepareToPlay(48000.0, 256);
    sendCc(q, 22, 0);
    check(value(q, "dfilt_res") < 1e-3f, "restored map drives the parameter");
    q.clearMidiCc("dfilt_res");
    check(q.numMidiMappings() == 1 && q.midiCcFor("dfilt_res") < 0, "forget one");
    q.clearAllMidiCcs();
    check(q.numMidiMappings() == 0, "clear all");

    log.add(allOk ? "ALL OK" : "FAILURES");
    dir.createDirectory();
    dir.getChildFile("midi-check.txt").replaceWithText(log.joinIntoString("\n") + "\n");
}

bool KMISynthEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && proc_.midiLearnTarget().isNotEmpty())
    {
        proc_.cancelMidiLearn();
        return true;
    }
    return false;
}

void KMISynthEditor::stepFactoryPreset(int delta)
{
    const int n = kmi::numFactoryPresets();
    const int current = proc_.factoryPresetIndex();
    const int next = current < 0 ? (delta > 0 ? 0 : n - 1) : (current + delta + n) % n;
    proc_.loadFactoryPreset(next);
    afterPresetChange();
}

// Developer aid (snapshot mode): load every factory preset and check that the parameters reproduce
// the engine's definition exactly. Results go to presets-check.txt; the Init patch is restored after.
void KMISynthEditor::verifyFactoryPresets(juce::File dir)
{
    dir.createDirectory();
    juce::String report;
    int failures = 0;
    for (int i = 0; i < kmi::numFactoryPresets(); ++i)
    {
        proc_.loadFactoryPreset(i);
        const auto bad = kmiplug::ParameterBinding::mismatches(proc_.state(), kmi::makeFactoryPreset(i));
        report << kmi::factoryPresetInfo(i).name << ": " << (bad.isEmpty() ? "OK" : juce::String(bad.size()) + " mismatches") << "\n";
        for (const auto& b : bad) report << "    " << b << "\n";
        failures += bad.size();
    }
    report << (failures == 0 ? "ALL OK\n" : "FAILURES: " + juce::String(failures) + "\n");
    dir.getChildFile("presets-check.txt").replaceWithText(report);
    proc_.initPatch();
    uiBinding_.apply();
}
