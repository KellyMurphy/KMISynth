#include "Controls.h"

#include <cstring>

#include "kmi/Modulation.h"

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
        menu.addSectionHeader(p->getName(64));
        menu.addItem(1, "Reset to default");
        menu.addItem(2, "Copy value");
        menu.addItem(3, "Paste value", g_hasClipboard);
        const int dest = destForParam(id_);
        if (dest >= 0)
        {
            juce::PopupMenu sources;
            const auto& names = kmi::modSourceNames();
            const int picks[] = { kmi::src::Env1, kmi::src::Env1 + 1, kmi::src::Env1 + 2, kmi::src::Env1 + 3,
                                  kmi::src::Env1 + 4, kmi::src::Lfo1, kmi::src::Lfo1 + 1, kmi::src::Lfo1 + 2,
                                  kmi::src::Lfo1 + 3, kmi::src::Lfo1 + 4, kmi::src::ModWheel, kmi::src::Velocity,
                                  kmi::src::PolyAT, kmi::src::ChannelAT, kmi::src::Keytrack, kmi::src::RandomPerNote,
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
}

ParamKnob::~ParamKnob() = default;

void ParamKnob::resized()
{
    auto r = getLocalBounds().reduced(2, 1);
    label_.setBounds(r.removeFromTop(14));
    slider_->setBounds(r);
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
    fillCombo(combo_, p);
    addAndMakeVisible(combo_);
    if (p)
    {
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(ctx.apvts, id, combo_);
        combo_.setTooltip(p->getName(64));
    }
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
    fillCombo(combo_, p);
    addAndMakeVisible(combo_);
    if (p)
    {
        attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(ctx.apvts, id, combo_);
        combo_.setTooltip(p->getName(64));
    }
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
