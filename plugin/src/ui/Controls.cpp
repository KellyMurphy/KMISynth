#include "Controls.h"

#include <cstring>

#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "Views.h"
#include "kmi/Modulation.h"
#include "kmi/Wavetables.h"

namespace kmiui {

// ---- Parameter helpers -----------------------------------------------------------------------------
juce::RangedAudioParameter* param(UiContext& ctx, const juce::String& id) { return ctx.apvts.getParameter(id); }

float getParam(UiContext& ctx, const juce::String& id)
{
    auto* raw = ctx.apvts.getRawParameterValue(id);
    return raw ? raw->load() : 0.0f;
}

void setParam(UiContext& ctx, const juce::String& id, float plain)
{
    if (auto* p = param(ctx, id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(plain));
        p->endChangeGesture();
    }
}

// ---- Waveform menus ------------------------------------------------------------------------------------
namespace {
// A small picture of one cycle for the waveform menu.
std::unique_ptr<juce::Drawable> waveThumbnail(const std::vector<float>& cycle)
{
    constexpr float w = 34.0f, h = 14.0f;
    juce::Path path;
    if (cycle.empty())
        path.startNewSubPath(0.0f, h * 0.5f), path.lineTo(w, h * 0.5f);
    else
        for (int k = 0; k <= 48; ++k)
        {
            const float ph = static_cast<float>(k) / 48.0f;
            const float v = cycle[std::min(cycle.size() - 1, static_cast<size_t>(ph * static_cast<float>(cycle.size())))];
            const juce::Point<float> pt(ph * w, h * 0.5f - v * h * 0.45f);
            if (k == 0) path.startNewSubPath(pt);
            else path.lineTo(pt);
        }
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(path);
    d->setFill(juce::FillType());
    d->setStrokeFill(theme::accent);
    d->setStrokeType(juce::PathStrokeType(1.2f));
    return d;
}
} // namespace

WaveMenuBinder::WaveMenuBinder(UiContext& ctx, juce::RangedAudioParameter& p, juce::ComboBox& combo)
    : ctx_(ctx), combo_(combo),
      attachment_(p, [this](float v) { combo_.setSelectedId(juce::roundToInt(v) + 1, juce::dontSendNotification); }, nullptr)
{
    combo_.setJustificationType(juce::Justification::centredLeft);
    combo_.onChange = [this] {
        if (combo_.getSelectedId() > 0) attachment_.setValueAsCompleteGesture(static_cast<float>(combo_.getSelectedId() - 1));
    };
    rebuild();
    startTimerHz(4);
}

void WaveMenuBinder::timerCallback()
{
    if (ctx_.proc.userWaveVersion() != version_) rebuild();
}

void WaveMenuBinder::rebuild()
{
    using Bank = kmi::WavetableBank;
    version_ = ctx_.proc.userWaveVersion();
    const auto& bank = Bank::instance();
    combo_.clear(juce::dontSendNotification);
    auto* menu = combo_.getRootMenu();
    juce::String group;
    for (int pos = 0; pos < bank.menuSize(); ++pos)
    {
        const int w = bank.waveAtMenuPosition(pos);
        if (group != bank.waveGroup(w))
        {
            group = bank.waveGroup(w);
            menu->addSectionHeader(group);
        }
        juce::String name = bank.waveName(w);
        if (Bank::isUserWave(w))
        {
            const int slot = w - Bank::kUserWaveFirst;
            name << (ctx_.proc.hasUserWave(slot) ? ": " + ctx_.proc.userWaveName(slot) : juce::String(" (empty)"));
        }
        juce::PopupMenu::Item item(name);
        item.itemID = w + 1;
        item.image = waveThumbnail(WavePanel::waveCycle(ctx_, w));
        menu->addItem(std::move(item));
    }
    attachment_.sendInitialUpdate();
}

int destForParam(const juce::String& id)
{
    using namespace kmi::dst;
    auto num = [&](const char* prefix, int maxN) -> int {
        if (!id.startsWith(prefix)) return -1;
        const int n = id.substring(static_cast<int>(std::strlen(prefix))).getIntValue();
        return n >= 1 && n <= maxN ? n - 1 : -1;
    };
    const juce::String suffix = id.fromFirstOccurrenceOf("_", false, false);

    if (const int o = num("osc", kmi::kNumOscs); o >= 0 && !suffix.startsWith("env_"))
    {
        if (suffix == "level") return osc(o, OscLevel);
        if (suffix == "envlevel") return osc(o, OscEnvLevel);
        if (suffix == "scan") return OscScanBase + o;
        if (suffix == "semi" || suffix == "ratio" || suffix == "freq") return osc(o, OscPitch);
        if (suffix == "cents" || suffix == "ratiofine" || suffix == "freqfine") return osc(o, OscFine);
        if (suffix == "feedback") return osc(o, OscFeedback);
        return -1;
    }
    if (const int e = num("env", kmi::kNumEnvs); e >= 0)
    {
        if (suffix == "attack") return env(e, EnvAttack);
        if (suffix == "decay") return env(e, EnvDecay);
        if (suffix == "sustain") return env(e, EnvSustain);
        if (suffix == "release") return env(e, EnvRelease);
        if (suffix == "level") return env(e, EnvLevel);
        return -1;
    }
    if (const int l = num("lfo", kmi::kNumLfos); l >= 0)
    {
        if (suffix == "rate") return lfo(l, LfoRate);
        if (suffix == "level") return lfo(l, LfoLevel);
        return -1;
    }
    static const std::pair<const char*, int> fixed[] = {
        { "dfilt_cutoff", DFiltBase + DfCutoff }, { "dfilt_res", DFiltBase + DfResonance },
        { "dfilt_morph", DFiltBase + DfMorph }, { "dfilt_drive", DFiltBase + DfDrive },
        { "dfilt_envamt", DFiltBase + DfEnvAmt }, { "dfilt_lfoamt", DFiltBase + DfLfoAmt },
        { "dfilt_level", DFiltBase + DfLevel }, { "afilt_cutoff", AFiltBase + AfCutoff },
        { "afilt_res", AFiltBase + AfResonance }, { "afilt_predrive", AFiltBase + AfPreDrive },
        { "afilt_envamt", AFiltBase + AfEnvAmt }, { "afilt_lfoamt", AFiltBase + AfLfoAmt },
        { "vca_level", VcaBase + VcaLevel }, { "vca_lfoamt", VcaBase + VcaLfoAmt },
        { "voice_detune", VoiceBase + VoiceUnisonDetune }, { "voice_vibamt", VoiceBase + VoiceVibratoAmt },
        { "voice_glidetime", VoiceBase + VoiceGlideTime }, { "algo_morph", AlgoMorph },
        { "delay_time", FxBase + DelayTime }, { "delay_feedback", FxBase + DelayFeedback },
        { "delay_wettone", FxBase + DelayWetTone }, { "delay_fbtone", FxBase + DelayFeedbackTone },
        { "delay_mix", FxBase + DelayMix }, { "reverb_time", FxBase + ReverbTime },
        { "reverb_tone", FxBase + ReverbTone }, { "reverb_hidamp", FxBase + ReverbHiDamp },
        { "reverb_lodamp", FxBase + ReverbLoDamp }, { "reverb_mix", FxBase + ReverbMix },
    };
    for (const auto& [name, d] : fixed)
        if (id == name) return d;
    if (id.startsWith("prefx_") && id.endsWith("_mix")) return FxBase + PreFxMix;
    if (id.startsWith("postfx_") && id.endsWith("_mix")) return FxBase + PostFxMix;
    return -1;
}

void addModRoute(UiContext& ctx, int source, int dest)
{
    for (int k = 1; k <= kmi::kNumModSlots; ++k)
    {
        const juce::String p = "mod" + juce::String(k) + "_";
        if (static_cast<int>(getParam(ctx, p + "src")) != kmi::src::None) continue;
        setParam(ctx, p + "src", static_cast<float>(source));
        setParam(ctx, p + "dst", static_cast<float>(dest));
        setParam(ctx, p + "depth", 32.0f);
        return;
    }
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Mod Matrix full",
                                           "All 32 mod routes are in use. Clear one on the Mod Matrix page.");
}

// ---- ParamKnob -----------------------------------------------------------------------------------------
namespace {
void styleTextBox(juce::Slider& s)
{
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxTextColourId, theme::text);
    s.setColour(juce::Slider::textBoxHighlightColourId, theme::accent.withAlpha(0.4f));
}

float g_clipboard = 0.0f;
bool g_hasClipboard = false;
const juce::String kKnobHelp = "\nDouble-click: reset    Ctrl-drag: fine    Right-click: options";
} // namespace

class ParamKnob::KnobSlider final : public juce::Slider
{
public:
    KnobSlider(UiContext& c, juce::String id) : ctx_(c), id_(std::move(id)) {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }
        setMouseDragSensitivity(e.mods.isCtrlDown() || e.mods.isCommandDown() ? 1500 : 250);
        juce::Slider::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!e.mods.isPopupMenu()) juce::Slider::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!e.mods.isPopupMenu()) juce::Slider::mouseUp(e);
    }

private:
    void showMenu()
    {
        auto* p = param(ctx_, id_);
        if (!p) return;
        juce::PopupMenu menu;
        const int mapped = ctx_.proc.midiCcFor(id_);
        menu.addSectionHeader(p->getName(64) + (mapped >= 0 ? "  (CC " + juce::String(mapped) + ")" : juce::String()));
        menu.addItem(1, "Reset to default");
        menu.addItem(2, "Copy value");
        menu.addItem(3, "Paste value", g_hasClipboard);
        menu.addSeparator();
        const bool learning = ctx_.proc.midiLearnTarget() == id_;
        menu.addItem(4, learning ? "Cancel MIDI Learn" : "MIDI Learn (move a controller)");
        const int cc = ctx_.proc.midiCcFor(id_);
        if (cc >= 0) menu.addItem(5, "Forget MIDI CC " + juce::String(cc));
        const int maps = ctx_.proc.numMidiMappings();
        if (maps > 0) menu.addItem(6, "Clear all MIDI mappings (" + juce::String(maps) + ")");
        const int dest = destForParam(id_);
        if (dest >= 0)
        {
            juce::PopupMenu sources;
            const auto& names = kmi::modSourceNames();
            const int picks[] = { kmi::src::Env1, kmi::src::Env1 + 1, kmi::src::Env1 + 2, kmi::src::Env1 + 3,
                                  kmi::src::Env1 + 4, kmi::src::Lfo1, kmi::src::Lfo1 + 1, kmi::src::Lfo1 + 2,
                                  kmi::src::Lfo1 + 3, kmi::src::Lfo1 + 4, kmi::src::ModWheel, kmi::src::Velocity,
                                  kmi::src::PolyAT, kmi::src::ChannelAT, kmi::src::Timbre, kmi::src::Keytrack, kmi::src::RandomPerNote,
                                  kmi::src::Macro1, kmi::src::Macro1 + 1, kmi::src::Macro1 + 2, kmi::src::Macro1 + 3,
                                  kmi::src::Macro1 + 4, kmi::src::Macro1 + 5, kmi::src::Macro1 + 6, kmi::src::Macro1 + 7 };
            for (int s : picks) sources.addItem(1000 + s, names[static_cast<size_t>(s)]);
            menu.addSeparator();
            menu.addSubMenu("Modulate with", sources);
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [safe = juce::Component::SafePointer<KnobSlider>(this), dest](int result) {
                               if (!safe || result == 0) return;
                               auto& self = *safe;
                               auto* prm = param(self.ctx_, self.id_);
                               if (!prm) return;
                               if (result == 1) setParam(self.ctx_, self.id_, prm->convertFrom0to1(prm->getDefaultValue()));
                               else if (result == 2) { g_clipboard = getParam(self.ctx_, self.id_); g_hasClipboard = true; }
                               else if (result == 3) setParam(self.ctx_, self.id_, g_clipboard);
                               else if (result == 4)
                               {
                                   if (self.ctx_.proc.midiLearnTarget() == self.id_) self.ctx_.proc.cancelMidiLearn();
                                   else self.ctx_.proc.startMidiLearn(self.id_);
                               }
                               else if (result == 5) self.ctx_.proc.clearMidiCc(self.id_);
                               else if (result == 6) self.ctx_.proc.clearAllMidiCcs();
                               else if (result >= 1000) addModRoute(self.ctx_, result - 1000, dest);
                           });
    }

    UiContext& ctx_;
    juce::String id_;
};

ParamKnob::ParamKnob(UiContext& ctx, const juce::String& id, const juce::String& label, juce::Colour colour)
    : ctx_(ctx), id_(id), slider_(std::make_unique<KnobSlider>(ctx, id))
{
    auto* p = param(ctx, id);
    jassert(p != nullptr);
    label_.setText(label.isNotEmpty() ? label : (p ? p->getName(32) : id), juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::Font(juce::FontOptions(11.5f)));
    label_.setMinimumHorizontalScale(0.6f);
    label_.setColour(juce::Label::textColourId, theme::textDim);
    label_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label_);

    styleTextBox(*slider_);
    slider_->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 14);
    slider_->setColour(juce::Slider::rotarySliderFillColourId, colour);
    addAndMakeVisible(*slider_);
    if (p)
    {
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ctx.apvts, id, *slider_);
        slider_->setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
        slider_->setTooltip(p->getName(64) + kKnobHelp);
    }
    dest_ = destForParam(id);
    if (dest_ >= 0) startTimerHz(20);
}

ParamKnob::~ParamKnob() = default;

namespace {
bool isBipolarSource(int s)
{
    using namespace kmi::src;
    return (s >= Lfo1 && s < Lfo1 + kmi::kNumLfos) || s == PitchBend || s == VoiceMod || s == RandomPerNote;
}

// Knob position (0..1) of the parameter's current value plus a modulation offset `d` at `dest`, using the
// same arithmetic as the engine for that destination.
float modulatedProportion(UiContext& ctx, const juce::String& id, int dest, float d)
{
    using namespace kmi::dst;
    auto* p = param(ctx, id);
    float scale = 1.0f, offset = 0.0f;
    if (!p) return 0.0f;
    if (isFx(dest) || !kmiplug::ParameterBinding::patchMapping(id, scale, offset) || scale == 0.0f)
        return juce::jlimit(0.0f, 1.0f, p->getValue() + d);
    float pv = getParam(ctx, id) * scale + offset;
    const juce::String suffix = id.fromFirstOccurrenceOf("_", false, false);
    const bool envTime = dest >= EnvBase && dest < LfoBase &&
                         ((dest - EnvBase) % kEnvParams == EnvAttack || (dest - EnvBase) % kEnvParams == EnvDecay ||
                          (dest - EnvBase) % kEnvParams == EnvRelease);
    const bool lfoRate = dest >= LfoBase && dest < VoiceBase && (dest - LfoBase) % kLfoParams == LfoRate;
    const bool oscParam = dest >= OscBase && dest < AllOscBase;
    if (envTime || lfoRate) pv *= std::exp2(5.0f * d);
    else if (oscParam && (dest - OscBase) % kOscParams == OscPitch) pv = suffix == "semi" ? pv + 12.8f * d : pv * std::exp2(12.8f * d / 12.0f);
    else if (oscParam && (dest - OscBase) % kOscParams == OscFine) pv = suffix == "cents" ? pv + 50.0f * d : pv;
    else if (dest == AlgoMorph) pv += 7.0f * d;
    else if (dest == DFiltBase + DfLevel) pv += 2.0f * d;
    else if (dest == VoiceBase + VoiceVibratoAmt) pv += 12.0f * d;
    else pv += d;
    const auto range = p->getNormalisableRange();
    return p->convertTo0to1(juce::jlimit(range.start, range.end, (pv - offset) / scale));
}
} // namespace

void ParamKnob::timerCallback()
{
    if (dest_ < 0) return;
    const kmi::Patch& patch = ctx_.patch;
    float lo = 0.0f, hi = 0.0f;
    bool routed = false;
    for (const auto& s : patch.mod)
    {
        if (s.source == kmi::src::None || s.dest != dest_ || s.depth == 0.0f) continue;
        routed = true;
        if (isBipolarSource(s.source))
        {
            lo -= std::abs(s.depth);
            hi += std::abs(s.depth);
        }
        else if (s.depth > 0.0f) hi += s.depth;
        else lo += s.depth;
    }
    for (const auto& m : patch.macro)
        for (const auto& r : m.routes)
            if (r.dest == dest_ && r.depth != 0.0f)
            {
                routed = true;
                lo += m.value * r.depth; // a macro is a static offset at its current value
                hi += m.value * r.depth;
            }

    auto& props = slider_->getProperties();
    if (!routed)
    {
        if (props.contains("kmiModLo"))
        {
            props.remove("kmiModLo");
            props.remove("kmiModHi");
            props.remove("kmiModLive");
            slider_->repaint();
        }
        return;
    }
    const float pLo = modulatedProportion(ctx_, id_, dest_, lo), pHi = modulatedProportion(ctx_, id_, dest_, hi);
    const float live = ctx_.proc.hasLiveMods() ? modulatedProportion(ctx_, id_, dest_, ctx_.proc.liveMod(dest_)) : -1.0f;
    auto changed = [&](const char* key, float v) {
        return !props.contains(key) || std::abs(static_cast<float>(props[key]) - v) > 1.0e-3f;
    };
    if (changed("kmiModLo", pLo) || changed("kmiModHi", pHi) || changed("kmiModLive", live))
    {
        props.set("kmiModLo", pLo);
        props.set("kmiModHi", pHi);
        props.set("kmiModLive", live);
        slider_->repaint();
    }
}

void ParamKnob::resized()
{
    auto r = getLocalBounds().reduced(2, 1);
    label_.setBounds(r.removeFromTop(14));
    slider_->setBounds(r);
}

// ---- MIDI learn menu (choices, toggles) ------------------------------------------------------------------
void showMidiLearnMenu(UiContext& ctx, const juce::String& id, juce::Component& target)
{
    auto* p = param(ctx, id);
    if (!p) return;
    const int cc = ctx.proc.midiCcFor(id);
    const bool learning = ctx.proc.midiLearnTarget() == id;
    juce::PopupMenu menu;
    menu.addSectionHeader(p->getName(64) + (cc >= 0 ? "  (CC " + juce::String(cc) + ")" : juce::String()));
    menu.addItem(1, "Reset to default");
    menu.addSeparator();
    menu.addItem(2, learning ? "Cancel MIDI Learn" : "MIDI Learn (move a controller)");
    if (cc >= 0) menu.addItem(3, "Forget MIDI CC " + juce::String(cc));
    const int maps = ctx.proc.numMidiMappings();
    if (maps > 0) menu.addItem(4, "Clear all MIDI mappings (" + juce::String(maps) + ")");
    // Result 0 also arrives when the control is deleted with the menu open: check it before touching ctx.
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target), [&ctx, id](int result) {
        if (result == 0) return;
        auto* prm = param(ctx, id);
        if (!prm) return;
        if (result == 1) setParam(ctx, id, prm->convertFrom0to1(prm->getDefaultValue()));
        else if (result == 2)
        {
            if (ctx.proc.midiLearnTarget() == id) ctx.proc.cancelMidiLearn();
            else ctx.proc.startMidiLearn(id);
        }
        else if (result == 3) ctx.proc.clearMidiCc(id);
        else if (result == 4) ctx.proc.clearAllMidiCcs();
    });
}

// ---- ParamChoice / ParamToggle / ParamBar / ParamCombo -----------------------------------------------------
namespace {
void fillCombo(juce::ComboBox& combo, juce::RangedAudioParameter* p)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(p)) combo.addItemList(c->choices, 1);
    combo.setJustificationType(juce::Justification::centredLeft);
}
} // namespace

ParamChoice::ParamChoice(UiContext& ctx, const juce::String& id, const juce::String& label)
{
    auto* p = param(ctx, id);
    jassert(p != nullptr);
    label_.setText(label.isNotEmpty() ? label : (p ? p->getName(32) : id), juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::Font(juce::FontOptions(11.5f)));
    label_.setMinimumHorizontalScale(0.6f);
    label_.setColour(juce::Label::textColourId, theme::textDim);
    addAndMakeVisible(label_);
    combo_.setLearn(ctx, id);
    addAndMakeVisible(combo_);
    if (!p) return;
    combo_.setTooltip(p->getName(64));
    if (id.startsWith("osc") && id.endsWith("_wave")) // oscillator waveforms (not the LFO wave)
    {
        waveMenu_ = std::make_unique<WaveMenuBinder>(ctx, *p, combo_);
        return;
    }
    fillCombo(combo_, p);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(ctx.apvts, id, combo_);
}

void ParamChoice::resized()
{
    auto r = getLocalBounds().reduced(3, 1);
    label_.setBounds(r.removeFromTop(14));
    combo_.setBounds(r.withSizeKeepingCentre(r.getWidth(), juce::jmin(24, r.getHeight())));
}

ParamToggle::ParamToggle(UiContext& ctx, const juce::String& id, const juce::String& label)
{
    auto* p = param(ctx, id);
    jassert(p != nullptr);
    button_.setButtonText(label.isNotEmpty() ? label : (p ? p->getName(32) : id));
    button_.setLearn(ctx, id);
    addAndMakeVisible(button_);
    if (p)
    {
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(ctx.apvts, id, button_);
        button_.setTooltip(p->getName(64));
    }
}

void ParamToggle::resized()
{
    button_.setBounds(getLocalBounds().reduced(2).withSizeKeepingCentre(getWidth() - 4, 24));
}

ParamBar::ParamBar(UiContext& ctx, const juce::String& id)
{
    styleTextBox(slider_);
    slider_.setSliderStyle(juce::Slider::LinearBar);
    slider_.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    slider_.setColour(juce::Slider::trackColourId, theme::accent.withAlpha(0.55f));
    addAndMakeVisible(slider_);
    if (auto* p = param(ctx, id))
    {
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(ctx.apvts, id, slider_);
        slider_.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
        slider_.setTooltip(p->getName(64) + "\nDouble-click: reset");
    }
}

ParamCombo::ParamCombo(UiContext& ctx, const juce::String& id)
{
    auto* p = param(ctx, id);
    combo_.setLearn(ctx, id);
    addAndMakeVisible(combo_);
    if (!p) return;
    combo_.setTooltip(p->getName(64));
    if (id.startsWith("osc") && id.endsWith("_wave")) // oscillator waveforms (not the LFO wave)
    {
        waveMenu_ = std::make_unique<WaveMenuBinder>(ctx, *p, combo_);
        return;
    }
    fillCombo(combo_, p);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(ctx.apvts, id, combo_);
}

std::unique_ptr<juce::Component> makeControl(UiContext& ctx, const juce::String& id, const juce::String& label,
                                             juce::Colour colour)
{
    auto* p = param(ctx, id);
    if (dynamic_cast<juce::AudioParameterChoice*>(p)) return std::make_unique<ParamChoice>(ctx, id, label);
    if (dynamic_cast<juce::AudioParameterBool*>(p)) return std::make_unique<ParamToggle>(ctx, id, label);
    return std::make_unique<ParamKnob>(ctx, id, label, colour);
}

// ---- Section -----------------------------------------------------------------------------------------------
Section::Section(juce::String title, int columns, juce::Colour accent)
    : title_(std::move(title)), columns_(juce::jmax(1, columns)), accent_(accent)
{
}

bool Section::fits(int col, int row, int cs, int rs) const
{
    if (col + cs > columns_) return false;
    for (int r = row; r < row + rs; ++r)
    {
        if (r >= static_cast<int>(used_.size())) continue;
        for (int c = col; c < col + cs; ++c)
            if (used_[static_cast<size_t>(r)][static_cast<size_t>(c)]) return false;
    }
    return true;
}

void Section::occupy(int col, int row, int cs, int rs)
{
    while (static_cast<int>(used_.size()) < row + rs) used_.emplace_back(static_cast<size_t>(columns_), false);
    for (int r = row; r < row + rs; ++r)
        for (int c = col; c < col + cs; ++c) used_[static_cast<size_t>(r)][static_cast<size_t>(c)] = true;
    rows_ = juce::jmax(rows_, row + rs);
}

juce::Component& Section::add(std::unique_ptr<juce::Component> c, int colSpan, int rowSpan)
{
    colSpan = juce::jlimit(1, columns_, colSpan);
    rowSpan = juce::jmax(1, rowSpan);
    for (int row = 0;; ++row)
        for (int col = 0; col < columns_; ++col)
            if (fits(col, row, colSpan, rowSpan))
            {
                occupy(col, row, colSpan, rowSpan);
                addAndMakeVisible(*c);
                items_.push_back({ std::move(c), col, row, colSpan, rowSpan });
                return *items_.back().comp;
            }
}

void Section::addParams(UiContext& ctx, std::initializer_list<juce::String> ids)
{
    for (const auto& id : ids) add(makeControl(ctx, id, {}, accent_));
}

void Section::addParam(UiContext& ctx, const juce::String& id, const juce::String& label)
{
    add(makeControl(ctx, id, label, accent_));
}

void Section::skip(int cells)
{
    for (int i = 0; i < cells; ++i) add(std::make_unique<juce::Component>());
}

void Section::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(theme::panel);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(theme::panelEdge);
    g.drawRoundedRectangle(r, 6.0f, 1.0f);
    g.setColour(accent_);
    g.fillRoundedRectangle(r.getX() + 8.0f, r.getY() + 8.0f, 3.0f, 10.0f, 1.5f);
    g.setColour(theme::text);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(title_, juce::Rectangle<float>(r.getX() + 16.0f, r.getY() + 3.0f, r.getWidth() - 20.0f, 20.0f),
               juce::Justification::centredLeft);
}

void Section::resized()
{
    for (auto& it : items_)
        it.comp->setBounds(kPad + it.col * kCellW, kTitleH + it.row * kCellH, it.colSpan * kCellW, it.rowSpan * kCellH);
}

// ---- Selector ---------------------------------------------------------------------------------------------------
Selector::Selector(const juce::StringArray& labels, int selected, std::function<void(int)> onSelect,
                   std::function<juce::Colour(int)> colourFor)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons_.add(new juce::TextButton(labels[i]));
        b->setRadioGroupId(0x5e1);
        b->setClickingTogglesState(true);
        b->setToggleState(i == selected, juce::dontSendNotification);
        if (colourFor) b->setColour(juce::TextButton::buttonOnColourId, colourFor(i).darker(0.35f));
        b->onClick = [onSelect, i, b] {
            if (b->getToggleState() && onSelect) onSelect(i);
        };
        addAndMakeVisible(b);
    }
}

void Selector::setSelected(int index)
{
    for (int i = 0; i < buttons_.size(); ++i) buttons_[i]->setToggleState(i == index, juce::dontSendNotification);
}

void Selector::resized()
{
    auto r = getLocalBounds();
    const int w = buttons_.isEmpty() ? 0 : juce::jmin(110, r.getWidth() / buttons_.size());
    for (auto* b : buttons_) b->setBounds(r.removeFromLeft(w).reduced(2, 0));
}

// ---- Page ---------------------------------------------------------------------------------------------------------
Page::~Page()
{
    stopTimer();
    cancelPendingUpdate();
}

void Page::start()
{
    rebuild();
    startTimerHz(8);
}

void Page::timerCallback()
{
    if (layoutKey() != key_) triggerAsyncUpdate();
}

void Page::handleAsyncUpdate() { rebuild(); }

void Page::rebuild()
{
    key_ = layoutKey();
    headers_.clear();
    sections_.clear();
    build();
    resized();
}

Section& Page::addSection(std::unique_ptr<Section> s)
{
    addAndMakeVisible(*s);
    sections_.push_back(std::move(s));
    return *sections_.back();
}

void Page::addHeader(std::unique_ptr<juce::Component> c, int height)
{
    addAndMakeVisible(*c);
    headers_.emplace_back(std::move(c), height);
}

void Page::resized()
{
    auto area = getLocalBounds().reduced(8);
    for (auto& [comp, h] : headers_)
    {
        comp->setBounds(area.removeFromTop(h));
        area.removeFromTop(6);
    }
    int x = area.getX(), y = area.getY(), rowH = 0;
    for (auto& s : sections_)
    {
        const int w = s->preferredWidth(), h = s->preferredHeight();
        if (x > area.getX() && x + w > area.getRight())
        {
            x = area.getX();
            y += rowH + 6;
            rowH = 0;
        }
        s->setBounds(x, y, w, h);
        x += w + 6;
        rowH = juce::jmax(rowH, h);
    }
}

} // namespace kmiui
