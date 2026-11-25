#include "PluginEditor.h"

#include "kmi/Modulation.h"
#include "kmi/Presets.h"
#include "ui/Pages.h"
#include "ui/PresetBrowser.h"
#include "ui/Theme.h"

namespace {
juce::File presetFolder() { return KMISynthProcessor::userPresetFolder(); }
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
                                static_cast<juce::Component*>(&randomButton_),
                                static_cast<juce::Component*>(&loadButton_), static_cast<juce::Component*>(&saveButton_),
                                static_cast<juce::Component*>(&undoButton_), static_cast<juce::Component*>(&redoButton_),
                                static_cast<juce::Component*>(&keyboard_) })
        surface_.addAndMakeVisible(c);

    initButton_.setTooltip("Reset every parameter to the Init patch");
    randomButton_.setTooltip("Randomise the patch: amount and which parts may change");
    randomButton_.onClick = [this] { showRandomPanel(); };
    loadButton_.setTooltip("Load a .kmip preset");
    saveButton_.setTooltip("Save the current sound as a .kmip preset");
    presetButton_.setTooltip("Open the preset browser (search, tags, folders)");
    prevButton_.setTooltip("Previous factory preset");
    nextButton_.setTooltip("Next factory preset");
    presetButton_.onClick = [this] { toggleBrowser(); };
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
        verifyPresetSearch(juce::File(snapDir));
        verifyUserWaves(juce::File(snapDir));
        verifyRandomise(juce::File(snapDir));
        verifySequencer(juce::File(snapDir));
        verifyTuning(juce::File(snapDir));
        snapshotPages(juce::File(snapDir), 0);
    }
}

void KMISynthEditor::snapshotPages(juce::File dir, int index)
{
    dir.createDirectory();
    const int numPages = kmiui::pageNames().size();
    // Second pass (indices numPages..): non-default states that switch in extra views.
    static const int variantPages[] = { 0, 1, 2, 5, 8, 10, 11 };
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
        kmiui::setParam(ctx_, "osc1_wave", 3.0f);       // Saw, scanning 6 waves on
        kmiui::setParam(ctx_, "osc1_scanrange", 6.0f);
        kmiui::setParam(ctx_, "osc1_scan", 40.0f);
        {
            // A short pattern for the Sequencer page.
            kmi::SeqData d;
            const int pattern[][3] = { { 0, 48, 8 }, { 0, 60, 4 }, { 0, 67, 4 }, { 2, 63, 2 }, { 3, 65, 2 }, { 4, 60, 4 },
                                       { 4, 67, 4 }, { 6, 70, 2 }, { 8, 51, 8 }, { 8, 63, 6 }, { 10, 58, 2 }, { 11, 62, 2 },
                                       { 12, 60, 12 }, { 12, 67, 12 }, { 14, 72, 2 }, { 15, 70, 2 } };
            for (const auto& n : pattern)
            {
                auto& st = d.steps[0][static_cast<size_t>(n[0])];
                if (st.count < kmi::kSeqNotesPerStep)
                    st.notes[static_cast<size_t>(st.count++)] = { static_cast<uint8_t>(n[1]), static_cast<uint8_t>(70 + 3 * n[0]), static_cast<uint8_t>(n[2]) };
            }
            d.steps[0][6].ratchet = 2;
            d.steps[0][10].probability = 60;
            proc_.setSeqData(d);
            kmiui::setParam(ctx_, "seq_on", 1.0f);
            kmiui::setParam(ctx_, "scale_type", 11.0f);  // A minor pentatonic in Werckmeister III for the Scales page
            kmiui::setParam(ctx_, "scale_root", 9.0f);
            kmiui::setParam(ctx_, "tuning_type", 4.0f);
        }
        auto steps = proc_.getLfoSteps(0);
        for (int i = 0; i < 16; ++i) steps[static_cast<size_t>(i)] = std::sin(i * 0.7f) * 0.8f;
        proc_.setLfoSteps(0, steps);
    }
    if (index == numPages + static_cast<int>(std::size(variantPages)))
    {
        // Last shot: the preset browser, filtered.
        toggleBrowser();
        juce::Timer::callAfterDelay(600, [safe = juce::Component::SafePointer<KMISynthEditor>(this), dir, index] {
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
            safe->toggleBrowser();
        });
        return;
    }
    if (index > numPages + static_cast<int>(std::size(variantPages))) return;
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
    for (auto* b : { &initButton_, &randomButton_, &loadButton_, &saveButton_ })
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
    if (browser_) browser_->setBounds(juce::Rectangle<int>(0, 46, kDesignW, kDesignH - 46 - 70).reduced(8, 6));
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

void KMISynthEditor::showRandomPanel()
{
    auto panel = std::make_unique<kmiui::RandomPanel>([safe = juce::Component::SafePointer<KMISynthEditor>(this)](
                                                          const kmi::RandomiseOptions& o) {
        if (safe) safe->randomise(o);
    });
    juce::CallOutBox::launchAsynchronously(std::move(panel), randomButton_.getScreenBounds(), nullptr);
}

void KMISynthEditor::randomise(const kmi::RandomiseOptions& options)
{
    (void) proc_.state().copyState(); // the "before" values must be in the undoable state first
    proc_.undoManager().beginNewTransaction("Randomise");
    kmi::Patch p = uiPatch_;
    kmi::randomisePatch(p, options, static_cast<uint32_t>(juce::Random::getSystemRandom().nextInt()));
    kmiplug::ParameterBinding::writePatch(proc_.state(), p);
    (void) proc_.state().copyState(); // flush the new values into the undoable state now, inside this transaction
    proc_.undoManager().beginNewTransaction();
}

void KMISynthEditor::toggleBrowser()
{
    if (!browser_)
    {
        browser_ = std::make_unique<kmiui::PresetBrowser>(
            proc_, [this] { afterPresetChange(); },
            [this] { juce::MessageManager::callAsync([safe = juce::Component::SafePointer<KMISynthEditor>(this)] { if (safe) safe->toggleBrowser(); }); });
        surface_.addChildComponent(*browser_);
    }
    if (browser_->isVisible())
    {
        browser_->setVisible(false);
        return;
    }
    browser_->refresh();
    layoutSurface();
    browser_->setVisible(true);
    browser_->toFront(true);
    browser_->focusSearch();
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

// Developer aid (snapshot mode): the browser's search and tag matching. Results go to search-check.txt.
// Developer aid (snapshot mode): user wavetables load from WAV files, survive a preset round trip, and
// Init clears them. Uses separate, headless processors.
void KMISynthEditor::verifyUserWaves(juce::File dir)
{
    juce::StringArray log;
    bool allOk = true;
    auto check = [&](bool ok, const juce::String& what) {
        log.add((ok ? "OK   " : "FAIL ") + what);
        allOk = allOk && ok;
    };
    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kmi-wave-check");
    tmp.deleteRecursively();
    tmp.createDirectory();
    auto writeWav = [&](const juce::String& name, int length, std::function<float(int)> fn) {
        const auto file = tmp.getChildFile(name);
        juce::AudioBuffer<float> b(1, length);
        for (int n = 0; n < length; ++n) b.setSample(0, n, fn(n));
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> out(file.createOutputStream().release());
        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate(48000.0).withNumChannels(1).withBitsPerSample(24);
        if (auto writer = wav.createWriterFor(out, options)) writer->writeFromAudioSampleBuffer(b, 0, length);
        return file;
    };
    const auto saw = writeWav("Short Saw.wav", 600, [](int n) { return -0.8f + 1.6f * static_cast<float>(n) / 600.0f; });
    const auto table = writeWav("Morph Table.wav", 2048 * 4, [](int n) {
        const int frame = n / 2048;
        const float ph = static_cast<float>(n % 2048) / 2048.0f;
        return 0.7f * std::sin(6.2831853f * ph) + 0.1f * static_cast<float>(frame) * std::sin(6.2831853f * ph * static_cast<float>(frame + 2));
    });
    const auto bad = writeWav("Too Long.wav", 10000, [](int n) { return std::sin(static_cast<float>(n) * 0.01f); });

    KMISynthProcessor p;
    p.prepareToPlay(48000.0, 256);
    int used = 0;
    check(p.loadUserWave(0, saw, used).isEmpty() && used == 1, "single cycle loads into User 1");
    check(p.hasUserWave(0) && p.userWaveName(0) == "Short Saw" && p.userWaveCycle(0).size() == 2048, "name and resampled cycle");
    check(p.loadUserWave(2, table, used).isEmpty() && used == 4, "wavetable file fills User 3-6");
    check(p.hasUserWave(5) && !p.hasUserWave(6) && p.userWaveName(3) == "Morph Table 2/4", "frames named in order");
    check(p.loadUserWave(6, bad, used).isNotEmpty() && used == 0 && !p.hasUserWave(6), "a long non-wavetable file is refused");

    // It plays: Osc 1 on User 1.
    p.state().getParameter("osc1_wave")->setValueNotifyingHost(
        p.state().getParameter("osc1_wave")->convertTo0to1(static_cast<float>(kmi::WavetableBank::kUserWaveFirst)));
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    float peak = 0.0f;
    for (int b = 0; b < 20; ++b)
    {
        buf.clear();
        p.processBlock(buf, midi);
        midi.clear();
        peak = std::fmax(peak, buf.getMagnitude(0, 0, 512));
    }
    check(peak > 0.01f, "a User wave plays");

    // The wavetable families: the Wave parameter reaches the last frame, which plays and is saved by index.
    using Bank = kmi::WavetableBank;
    auto* waveParam = dynamic_cast<juce::AudioParameterChoice*>(p.state().getParameter("osc2_wave"));
    check(waveParam != nullptr && waveParam->choices.size() == Bank::kNumWaveIndices &&
              waveParam->choices[Bank::kNumWaveIndices - 1] == "Spectra 16",
          "the Wave parameter lists all " + juce::String(Bank::kNumWaveIndices) + " waves");
    p.state().getParameter("osc1_wave")->setValueNotifyingHost(
        p.state().getParameter("osc1_wave")->convertTo0to1(static_cast<float>(Bank::kNumWaveIndices - 1)));
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.9f), 0);
    peak = 0.0f;
    for (int b = 0; b < 20; ++b)
    {
        buf.clear();
        p.processBlock(buf, midi);
        midi.clear();
        peak = std::fmax(peak, buf.getMagnitude(0, 0, 512));
    }
    check(peak > 0.01f, "a family frame plays");
    const auto famPreset = tmp.getChildFile("Family.kmip");
    KMISynthProcessor fam;
    check(p.savePreset(famPreset) && fam.loadPreset(famPreset) &&
              juce::roundToInt(fam.state().getRawParameterValue("osc1_wave")->load()) == Bank::kNumWaveIndices - 1,
          "a family frame survives a preset");
    p.state().getParameter("osc1_wave")->setValueNotifyingHost(
        p.state().getParameter("osc1_wave")->convertTo0to1(static_cast<float>(Bank::kUserWaveFirst)));

    const auto preset = tmp.getChildFile("With Waves.kmip");
    check(p.savePreset(preset), "preset saved");
    KMISynthProcessor q;
    check(q.loadPreset(preset), "preset loaded");
    bool same = true;
    for (int s = 0; s < 8; ++s)
        same = same && q.hasUserWave(s) == p.hasUserWave(s) && q.userWaveName(s) == p.userWaveName(s) &&
               q.userWaveCycle(s) == p.userWaveCycle(s);
    check(same, "user waves survive the preset round trip");
    juce::MemoryBlock session;
    q.getStateInformation(session);
    KMISynthProcessor r;
    r.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    check(r.hasUserWave(0) && r.userWaveCycle(3) == p.userWaveCycle(3), "user waves survive the session state");
    r.initPatch();
    check(!r.hasUserWave(0) && !r.hasUserWave(2), "Init clears them");
    q.loadFactoryPreset(0);
    check(!q.hasUserWave(0), "factory presets clear them");
    tmp.deleteRecursively();

    log.add(allOk ? "ALL OK" : "FAILURES");
    dir.getChildFile("wave-check.txt").replaceWithText(log.joinIntoString("\n") + "\n");
}

// Developer aid (snapshot mode): a randomise is one undo step that changes the patch and undoes cleanly.
void KMISynthEditor::verifyRandomise(juce::File dir)
{
    juce::StringArray log;
    bool allOk = true;
    auto check = [&](bool ok, const juce::String& what) {
        log.add((ok ? "OK   " : "FAIL ") + what);
        allOk = allOk && ok;
    };
    KMISynthProcessor p;
    p.loadFactoryPreset(0);
    auto snapshot = [&] {
        juce::Array<float> v;
        for (auto* param : p.getParameters()) v.add(param->getValue());
        return v;
    };
    const auto before = snapshot();
    (void) p.state().copyState(); // the "before" values must be in the undoable state first
    p.undoManager().beginNewTransaction("Randomise");
    kmi::Patch patch = kmi::makeFactoryPreset(0);
    kmi::RandomiseOptions all;
    all.amount = 1.0f;
    kmi::randomisePatch(patch, all, 4242u);
    kmiplug::ParameterBinding::writePatch(p.state(), patch);
    (void) p.state().copyState(); // flush the new values into the undoable state now, inside this transaction
    p.undoManager().beginNewTransaction();
    const auto after = snapshot();
    int changed = 0;
    for (int i = 0; i < before.size(); ++i) changed += before[i] != after[i] ? 1 : 0;
    check(changed > 20, "randomise changes many parameters (" + juce::String(changed) + ")");
    check(p.undoManager().canUndo() && p.undoManager().getUndoDescription() == "Randomise", "one undo step named Randomise");
    p.undoManager().undo();
    const auto undone = snapshot();
    int differ = 0;
    float maxDiff = 0.0f;
    for (int i = 0; i < before.size(); ++i)
    {
        const float d = std::abs(undone[i] - before[i]);
        maxDiff = juce::jmax(maxDiff, d);
        differ += d > 1.0e-4f ? 1 : 0;
    }
    check(differ == 0, "undo restores the patch (" + juce::String(differ) + " differ, max " + juce::String(maxDiff, 6) + ")");
    log.add(allOk ? "ALL OK" : "FAILURES");
    dir.getChildFile("random-check.txt").replaceWithText(log.joinIntoString("\n") + "\n");
}

// Developer aid (snapshot mode): sequencer data survives presets and sessions, Key trigger plays, MIDI out
// sends the notes, and recording queues played notes for the editor. Uses headless processors.
void KMISynthEditor::verifySequencer(juce::File dir)
{
    juce::StringArray log;
    bool allOk = true;
    auto check = [&](bool ok, const juce::String& what) {
        log.add((ok ? "OK   " : "FAIL ") + what);
        allOk = allOk && ok;
    };
    auto set = [](KMISynthProcessor& p, const char* id, float plain) {
        auto* prm = p.state().getParameter(id);
        prm->setValueNotifyingHost(prm->convertTo0to1(plain));
    };
    KMISynthProcessor p;
    p.prepareToPlay(48000.0, 512);
    kmi::SeqData d;
    d.steps[0][0].count = 2;
    d.steps[0][0].notes[0] = { 60, 110, 2 };
    d.steps[0][0].notes[1] = { 67, 90, 2 };
    d.steps[0][2] = d.steps[0][0];
    d.steps[0][2].probability = 75;
    d.steps[0][2].ratchet = 2;
    d.steps[0][2].drift = 20;
    d.steps[1][5].count = 1;
    d.steps[1][5].notes[0] = { 36, 127, 8 };
    d.laneOn[3] = true;
    for (int k = 0; k < kmi::kSeqSteps; ++k) d.lanes[3][static_cast<size_t>(k)] = static_cast<float>(k % 8) / 7.0f;
    p.setSeqData(d);
    set(p, "seq_on", 1.0f);
    set(p, "seq_trigger", 1.0f);  // Held Key
    set(p, "seq_midiout", 1.0f);
    set(p, "seq_t1_length", 4.0f);

    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kmi-seq-check");
    tmp.deleteRecursively();
    tmp.createDirectory();
    const auto preset = tmp.getChildFile("Seq.kmip");
    check(p.savePreset(preset), "preset saved");
    KMISynthProcessor q;
    check(q.loadPreset(preset), "preset loaded");
    const auto& e = q.getSeqData();
    check(e.steps == d.steps && e.laneOn == d.laneOn, "steps and lane switches survive the preset");
    bool lanesClose = true;
    for (int k = 0; k < kmi::kSeqSteps; ++k) lanesClose = lanesClose && std::abs(e.lanes[3][static_cast<size_t>(k)] - d.lanes[3][static_cast<size_t>(k)]) < 1e-3f;
    check(lanesClose, "lane values survive (3 decimals)");
    juce::MemoryBlock session;
    q.getStateInformation(session);
    KMISynthProcessor r;
    r.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    check(r.getSeqData().steps == d.steps, "and the session state");
    r.loadFactoryPreset(0);
    check(r.getSeqData().steps[0][0].count == 0, "factory presets clear the sequence");

    // Held key: the sequence plays (transposed) and the notes go out as MIDI.
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 62, 0.9f), 0);
    int ons = 0, onsAt62 = 0;
    float peak = 0.0f;
    for (int b = 0; b < 40; ++b) // ~0.43 s: steps 0 and 2 of a 1/16 grid at 120 BPM
    {
        buf.clear();
        p.processBlock(buf, midi);
        for (const auto m : midi)
            if (m.getMessage().isNoteOn())
            {
                ++ons;
                onsAt62 += m.getMessage().getNoteNumber() == 62 ? 1 : 0;
            }
        midi.clear();
        peak = std::fmax(peak, buf.getMagnitude(0, 0, 512));
    }
    check(peak > 0.01f, "a held key plays the sequence");
    check(ons >= 2 && onsAt62 >= 1, "MIDI out sends the sequence's notes, transposed (" + juce::String(ons) + " note-ons)");

    // Recording: the played note is queued for the editor.
    KMISynthProcessor rec;
    rec.prepareToPlay(48000.0, 512);
    rec.setSequencerRecording(true);
    midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 10);
    buf.clear();
    rec.processBlock(buf, midi);
    KMISynthProcessor::RecordedNote got[4];
    const int n = rec.popRecordedNotes(got, 4);
    check(n == 1 && got[0].note == 70, "recording queues played notes");
    tmp.deleteRecursively();

    log.add(allOk ? "ALL OK" : "FAILURES");
    dir.getChildFile("seq-check.txt").replaceWithText(log.joinIntoString("\n") + "\n");
}

// Developer aid (snapshot mode): Scala files load (with a sibling .kbm), bad files are refused, and the tuning
// survives presets and sessions; Init clears it. Uses headless processors.
void KMISynthEditor::verifyTuning(juce::File dir)
{
    juce::StringArray log;
    bool allOk = true;
    auto check = [&](bool ok, const juce::String& what) {
        log.add((ok ? "OK   " : "FAIL ") + what);
        allOk = allOk && ok;
    };
    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kmi-tuning-check");
    tmp.deleteRecursively();
    tmp.createDirectory();
    const auto scl = tmp.getChildFile("slendro.scl");
    scl.replaceWithText("! test scale\nSlendro-like test\n5\n240.0\n480.0\n720.0\n960.0\n2/1\n");
    tmp.getChildFile("slendro.kbm").replaceWithText("0\n0\n127\n60\n60\n261.625565\n5\n");
    tmp.getChildFile("bad.scl").replaceWithText("bad\nnope\n");

    KMISynthProcessor p;
    check(p.loadScalaFile(tmp.getChildFile("bad.scl")).isNotEmpty() && p.scalaName().isEmpty(), "a bad .scl is refused");
    check(p.loadScalaFile(scl).isEmpty(), "the .scl loads");
    check(p.scalaName() == "Slendro-like test" && p.scalaHasMapping(), "named, with its sibling .kbm");
    check(std::abs(p.scalaTable()[61] - 62.4f) < 1e-3f && std::abs(p.scalaTable()[65] - 72.0f) < 1e-3f, "5 equal steps of 240 cents");

    const auto preset = tmp.getChildFile("Tuned.kmip");
    check(p.savePreset(preset), "preset saved");
    KMISynthProcessor q;
    check(q.loadPreset(preset) && q.scalaName() == p.scalaName() && q.scalaTable() == p.scalaTable(), "tuning survives the preset");
    juce::MemoryBlock session;
    q.getStateInformation(session);
    KMISynthProcessor r;
    r.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    check(r.scalaTable() == p.scalaTable(), "and the session state");
    r.initPatch();
    check(r.scalaName().isEmpty() && r.scalaTable()[61] == 61.0f, "Init clears it");
    tmp.deleteRecursively();

    log.add(allOk ? "ALL OK" : "FAILURES");
    dir.getChildFile("tuning-check.txt").replaceWithText(log.joinIntoString("\n") + "\n");
}

void KMISynthEditor::verifyPresetSearch(juce::File dir)
{
    juce::StringArray log;
    bool allOk = true;
    auto check = [&](bool ok, const juce::String& what) {
        log.add((ok ? "OK   " : "FAIL ") + what);
        allOk = allOk && ok;
    };
    const auto all = kmiui::scanPresets(juce::File());
    auto count = [&](const juce::String& q, const juce::String& tag) {
        int n = 0;
        for (const auto& e : all) n += kmiui::presetMatches(e, q, tag) ? 1 : 0;
        return n;
    };
    check(static_cast<int>(all.size()) == kmi::numFactoryPresets(), "factory presets listed");
    check(count("", "") == kmi::numFactoryPresets(), "empty query matches all");
    check(count("velvet", "") == 1, "name search");
    // Expected counts come from the bank itself, so adding presets never breaks the check.
    int drones = 0, arpTagged = 0;
    for (int i = 0; i < kmi::numFactoryPresets(); ++i)
    {
        drones += juce::String(kmi::factoryPresetInfo(i).category) == "Drone" ? 1 : 0;
        auto tags = juce::StringArray::fromTokens(kmi::factoryPresetTags(i), ",", "");
        tags.trim();
        arpTagged += tags.contains("arp") ? 1 : 0;
    }
    check(drones > 0 && count("FACTORY/DRONE", "") == drones, "folder search, case-insensitive");
    check(count("glass", "") >= 3 && count("glass carillon", "") == 1, "every word must match");
    check(arpTagged > 0 && count("", "arp") == arpTagged, "tag filter");
    check(count("glass stairs", "arp") == 1 && count("glass", "arp") >= 1 && count("glass", "arp") < count("glass", ""), "search plus tag");
    check(count("nothing-like-this", "") == 0, "no match");

    // User presets with tags round-trip through a temporary folder.
    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kmi-browser-check");
    tmp.deleteRecursively();
    tmp.getChildFile("Leads").createDirectory();
    KMISynthProcessor p;
    p.loadFactoryPreset(5);
    p.setPresetTags("bright, mine");
    p.savePreset(tmp.getChildFile("Leads").getChildFile("My Lead.kmip"));
    const auto withUser = kmiui::scanPresets(tmp);
    const kmiui::PresetEntry* mine = nullptr;
    for (const auto& e : withUser)
        if (e.factory < 0) mine = &e;
    check(mine != nullptr && mine->folder == "User/Leads" && mine->tags == "bright, mine", "user preset folder and tags");
    check(mine != nullptr && kmiui::presetMatches(*mine, "mine", ""), "user tags are searchable");
    if (mine)
    {
        KMISynthProcessor::writePresetFileTags(mine->file, "edited");
        check(KMISynthProcessor::readPresetFileTags(mine->file) == "edited", "edit tags in the file");
        KMISynthProcessor q;
        q.loadPreset(mine->file);
        check(q.presetTags() == "edited" && q.presetName() == "My Lead", "loading reads name and tags");
    }
    tmp.deleteRecursively();

    log.add(allOk ? "ALL OK" : "FAILURES");
    dir.createDirectory();
    dir.getChildFile("search-check.txt").replaceWithText(log.joinIntoString("\n") + "\n");
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
