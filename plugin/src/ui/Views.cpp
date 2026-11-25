#include "Views.h"

#include "../PluginProcessor.h"
#include "kmi/Dsp.h"
#include "kmi/Modulation.h"
#include "kmi/Tuning.h"
#include "kmi/Wavetables.h"

namespace kmiui {

namespace {
const char* modeAbbrev(kmi::OscMode m)
{
    switch (m)
    {
        case kmi::OscMode::PhaseMod: return "PM";
        case kmi::OscMode::FreqMod: return "FM";
        case kmi::OscMode::PWMod: return "PW";
        case kmi::OscMode::HardSync: return "SY";
        case kmi::OscMode::PDSquare: return "PD1";
        case kmi::OscMode::PDSaw: return "PD2";
        case kmi::OscMode::PDSawPulse: return "PD3";
        default: return "";
    }
}

void drawPanelBackground(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour(theme::background.brighter(0.03f));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(theme::panelEdge);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}
} // namespace

// ---- AlgorithmView -----------------------------------------------------------------------------------
AlgorithmView::AlgorithmView(UiContext& ctx, bool interactive) : ctx_(ctx), interactive_(interactive)
{
    startTimerHz(15);
    if (interactive)
        setTooltip("Click a matrix cell to connect a modulator (row) to a target (column).\n"
                   "Click the OUT row to send an oscillator to the output.\n"
                   "Editing a preset or morph algorithm switches the patch to Custom.");
}

kmi::Algorithm AlgorithmView::effective() const
{
    kmi::Patch tmp = ctx_.patch;
    tmp.customAlgorithm = ctx_.proc.getCustomAlgorithm();
    kmi::Algorithm a;
    kmi::resolveAlgorithm(tmp, 0.0f, a);
    for (int i = 1; i < kmi::kNumOscs; ++i)
        if (tmp.osc[static_cast<size_t>(i)].directOut) a.out[i] = juce::jmax(a.out[i], 1.0f);
    return a;
}

kmi::Algorithm AlgorithmView::editableCopy()
{
    if (ctx_.patch.algo.mode == kmi::AlgoMode::Custom) return ctx_.proc.getCustomAlgorithm();
    kmi::Patch tmp = ctx_.patch;
    kmi::Algorithm a;
    kmi::resolveAlgorithm(tmp, 0.0f, a);
    for (auto& row : a.conn)
        for (float& w : row) w = w >= 0.5f ? 1.0f : 0.0f; // snap morphed weights
    for (float& o : a.out) o = o >= 0.5f ? 1.0f : 0.0f;
    a.out[0] = 1.0f;
    setParam(ctx_, "algo_mode", static_cast<float>(kmi::AlgoMode::Custom));
    return a;
}

juce::Rectangle<float> AlgorithmView::matrixArea() const
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    return r.removeFromRight(juce::jmin(r.getWidth() * 0.45f, r.getHeight() * 0.95f));
}

juce::Rectangle<float> AlgorithmView::cellRect(int row, int col) const
{
    auto m = matrixArea();
    const float label = 22.0f;
    const float cell = juce::jmin((m.getWidth() - label) / 8.0f, (m.getHeight() - label - 26.0f) / 9.0f);
    const float x = m.getX() + label + col * cell;
    const float y = m.getY() + label + row * cell + (row == 8 ? 8.0f : 0.0f);
    return { x, y, cell, cell };
}

void AlgorithmView::paint(juce::Graphics& g)
{
    const auto algo = effective();
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    if (interactive_)
    {
        auto m = matrixArea();
        paintDiagram(g, r.withRight(m.getX() - 10.0f), algo);
        paintMatrix(g, m, algo);
    }
    else
    {
        paintDiagram(g, r, algo);
    }
}

void AlgorithmView::paintDiagram(juce::Graphics& g, juce::Rectangle<float> area, const kmi::Algorithm& a)
{
    drawPanelBackground(g, area);
    const auto& p = ctx_.patch;

    // Height of each oscillator above the output: carriers 0, modulators above their targets.
    int depth[kmi::kNumOscs] = {};
    bool used[kmi::kNumOscs] = {};
    for (int i = 0; i < kmi::kNumOscs; ++i) used[i] = a.isCarrier(i) || a.isModulator(i);
    for (int pass = 0; pass < kmi::kNumOscs; ++pass)
        for (int m = 0; m < kmi::kNumOscs; ++m)
            for (int c = 0; c < kmi::kNumOscs; ++c)
                if (a.conn[m][c] > 0.0f && m != c) depth[m] = juce::jmin(6, juce::jmax(depth[m], depth[c] + 1));
    for (int i = 0; i < kmi::kNumOscs; ++i)
        if (a.isCarrier(i) && !a.isModulator(i)) depth[i] = 0;

    int maxDepth = 0;
    for (int i = 0; i < kmi::kNumOscs; ++i)
        if (used[i]) maxDepth = juce::jmax(maxDepth, depth[i]);

    const float outBarH = 14.0f;
    auto inner = area.reduced(10.0f);
    const float rowH = juce::jmin(54.0f, (inner.getHeight() - outBarH - 10.0f) / static_cast<float>(maxDepth + 1));
    const float contentH = rowH * static_cast<float>(maxDepth + 1) + outBarH + 24.0f;
    inner = inner.withSizeKeepingCentre(inner.getWidth(), juce::jmin(inner.getHeight(), contentH));
    const float boxW = juce::jmin(40.0f, inner.getWidth() / 9.0f), boxH = juce::jmin(28.0f, rowH * 0.62f);
    juce::Point<float> pos[kmi::kNumOscs];
    for (int d = 0; d <= maxDepth; ++d)
    {
        int count = 0;
        for (int i = 0; i < kmi::kNumOscs; ++i) count += used[i] && depth[i] == d;
        int k = 0;
        for (int i = 0; i < kmi::kNumOscs; ++i)
        {
            if (!used[i] || depth[i] != d) continue;
            const float x = inner.getX() + inner.getWidth() * (static_cast<float>(k) + 0.5f) / static_cast<float>(count);
            const float y = inner.getBottom() - outBarH - 14.0f - rowH * (static_cast<float>(d) + 0.5f);
            pos[i] = { x, y };
            ++k;
        }
    }

    // Output bar
    const auto outBar = juce::Rectangle<float>(inner.getX(), inner.getBottom() - outBarH, inner.getWidth(), outBarH);
    g.setColour(theme::panelEdge);
    g.fillRoundedRectangle(outBar, 3.0f);
    g.setColour(theme::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("OUT", outBar, juce::Justification::centred);

    // Connections
    for (int m = 0; m < kmi::kNumOscs; ++m)
        for (int c = 0; c < kmi::kNumOscs; ++c)
        {
            const float w = a.conn[m][c];
            if (w <= 0.0f || m == c || !used[m] || !used[c]) continue;
            const auto from = pos[m].translated(0.0f, boxH * 0.5f);
            const auto to = pos[c].translated(0.0f, -boxH * 0.5f);
            g.setColour(theme::osc(m).withAlpha(0.25f + 0.65f * w));
            juce::Path path;
            path.startNewSubPath(from);
            if (to.y <= from.y) // upward edge (feedback loop): curve around the side
                path.cubicTo(from.translated(boxW, rowH * 0.5f), to.translated(boxW, -rowH * 0.5f), to);
            else
                path.lineTo(to);
            g.strokePath(path, juce::PathStrokeType(1.6f));
            const auto dir = (to - path.getPointAlongPath(path.getLength() - 6.0f));
            const float ang = std::atan2(dir.y, dir.x);
            juce::Path head;
            head.addTriangle(0.0f, 0.0f, -7.0f, -3.5f, -7.0f, 3.5f);
            g.fillPath(head, juce::AffineTransform::rotation(ang).translated(to));
        }

    // Carrier outputs
    for (int i = 0; i < kmi::kNumOscs; ++i)
        if (used[i] && a.isCarrier(i))
        {
            g.setColour(theme::osc(i).withAlpha(0.35f + 0.5f * a.out[i]));
            g.drawLine(pos[i].x, pos[i].y + boxH * 0.5f, pos[i].x, outBar.getY(), 1.4f);
        }

    // Oscillator boxes
    for (int i = 0; i < kmi::kNumOscs; ++i)
    {
        if (!used[i]) continue;
        const auto& op = p.osc[static_cast<size_t>(i)];
        const auto box = juce::Rectangle<float>(boxW, boxH).withCentre(pos[i]);
        const float activity = op.mute ? 0.1f : juce::jlimit(0.25f, 1.0f, op.initLevel + std::abs(op.envLevel));
        g.setColour(theme::osc(i).withAlpha(0.18f + 0.55f * activity));
        g.fillRoundedRectangle(box, 4.0f);
        g.setColour(theme::osc(i));
        g.drawRoundedRectangle(box, 4.0f, 1.2f);
        g.setColour(theme::text);
        g.setFont(juce::Font(juce::FontOptions(boxH * 0.5f, juce::Font::bold)));
        g.drawText(juce::String(i + 1), box.withTrimmedBottom(a.isModulator(i) ? boxH * 0.3f : 0.0f),
                   juce::Justification::centred);
        if (a.isModulator(i))
        {
            g.setFont(juce::Font(juce::FontOptions(juce::jmax(8.0f, boxH * 0.3f))));
            g.setColour(theme::text.withAlpha(0.75f));
            g.drawText(modeAbbrev(op.mode), box.withTrimmedTop(boxH * 0.55f), juce::Justification::centred);
        }
        if (op.feedback > 0.0f)
        {
            g.setColour(theme::osc(i).withAlpha(0.4f + 0.6f * op.feedback));
            g.drawEllipse(box.getRight() - 3.0f, box.getY() - 7.0f, 10.0f, 10.0f, 1.3f);
        }
    }

    if (!interactive_) return;
    g.setColour(theme::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    const char* mode = p.algo.mode == kmi::AlgoMode::Custom ? "Custom"
                       : p.algo.mode == kmi::AlgoMode::Morph ? "Morph"
                                                             : "Preset";
    g.drawText(juce::String(mode) + (p.algo.mode == kmi::AlgoMode::Single
                                         ? juce::String(" ") + juce::String(p.algo.index + 1) + "  " +
                                               kmi::presetAlgorithmName(p.algo.index)
                                         : juce::String()),
               area.reduced(8.0f, 4.0f).removeFromTop(16.0f), juce::Justification::topLeft);
}

void AlgorithmView::paintMatrix(juce::Graphics& g, juce::Rectangle<float> area, const kmi::Algorithm& a)
{
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    const auto first = cellRect(0, 0);
    g.setColour(theme::textDim);
    g.drawText("rows modulate columns    OUT = audio output", cellRect(8, 0).translated(0.0f, cellRect(8, 0).getHeight() + 2.0f)
                   .withWidth(area.getWidth()).withHeight(16.0f), juce::Justification::centredLeft);
    for (int i = 0; i < kmi::kNumOscs; ++i)
    {
        const auto col = cellRect(0, i), row = cellRect(i, 0);
        g.setColour(theme::osc(i));
        g.drawText(juce::String(i + 1), col.withY(first.getY() - 16.0f).withHeight(14.0f), juce::Justification::centred);
        g.drawText(juce::String(i + 1), row.withX(area.getX()).withWidth(18.0f), juce::Justification::centred);
    }
    g.setColour(theme::textDim);
    g.drawText("OUT", cellRect(8, 0).withX(area.getX() - 6.0f).withWidth(26.0f), juce::Justification::centred);

    for (int m = 0; m < kmi::kNumOscs; ++m)
        for (int c = 0; c < kmi::kNumOscs; ++c)
        {
            auto cell = cellRect(m, c).reduced(1.5f);
            if (m == c)
            {
                g.setColour(theme::panelEdge.withAlpha(0.4f));
                g.fillRoundedRectangle(cell, 2.0f);
                continue;
            }
            const float w = a.conn[m][c];
            g.setColour(w > 0.0f ? theme::osc(c).withAlpha(0.3f + 0.7f * w) : theme::control);
            g.fillRoundedRectangle(cell, 2.0f);
        }
    for (int i = 0; i < kmi::kNumOscs; ++i)
    {
        auto cell = cellRect(8, i).reduced(1.5f);
        g.setColour(a.out[i] > 0.0f ? theme::osc(i).withAlpha(0.3f + 0.7f * a.out[i]) : theme::control);
        g.fillRoundedRectangle(cell, 2.0f);
        if (a.out[i] > 0.0f)
        {
            g.setColour(theme::text);
            juce::Path arrow;
            const auto c = cell.getCentre();
            arrow.addTriangle(c.x - 4.0f, c.y - 2.0f, c.x + 4.0f, c.y - 2.0f, c.x, c.y + 3.5f);
            g.fillPath(arrow);
        }
    }
}

void AlgorithmView::mouseDown(const juce::MouseEvent& e)
{
    if (!interactive_) return;
    for (int row = 0; row <= 8; ++row)
        for (int col = 0; col < kmi::kNumOscs; ++col)
        {
            if (!cellRect(row, col).contains(e.position)) continue;
            if (row < 8 && row == col) return; // self-modulation is the Feedback parameter
            if (row == 8 && col == 0) return;  // Osc 1 always reaches the output
            const auto shown = effective();
            kmi::Algorithm a = editableCopy();
            if (row < 8)
            {
                a.conn[row][col] = a.conn[row][col] > 0.0f ? 0.0f : 1.0f;
            }
            else
            {
                const bool on = shown.out[col] > 0.0f;
                a.out[col] = on ? 0.0f : 1.0f;
                if (on) setParam(ctx_, "osc" + juce::String(col + 1) + "_directout", 0.0f);
            }
            ctx_.proc.setCustomAlgorithm(a);
            repaint();
            return;
        }
}

// ---- Envelopes -----------------------------------------------------------------------------------------
void drawEnvelope(juce::Graphics& g, juce::Rectangle<float> r, const kmi::EnvParams& e, juce::Colour colour, bool fill)
{
    const float slowA = e.slow ? 600.0f / 36.0f : 1.0f, slowD = e.slow ? 15.0f : 1.0f, slowDl = e.slow ? 1.875f : 1.0f;
    auto w = [](float ms) { return std::sqrt(juce::jmax(0.0f, ms)) + 2.0f; };
    const float wd = e.delayMs > 0.0f ? w(e.delayMs * slowDl) : 0.0f;
    const float wa = w(e.attackMs * slowA), wh = e.holdMs > 0.0f ? w(e.holdMs * slowA) : 0.0f;
    const float wdc = w(e.decayMs * slowD), wr = w(e.releaseMs * slowD);
    const float ws = (wd + wa + wh + wdc + wr) * 0.25f + 10.0f;
    const float total = wd + wa + wh + wdc + ws + wr;
    const float sx = r.getWidth() / total;
    const float S = juce::jlimit(0.0f, 1.0f, e.sustain), L = juce::jlimit(0.0f, 1.0f, e.level);
    auto Y = [&](float v) { return r.getBottom() - v * L * r.getHeight(); };

    juce::Path p;
    float x = r.getX();
    p.startNewSubPath(x, Y(0.0f));
    x += wd * sx;
    p.lineTo(x, Y(0.0f));
    const int n = 24;
    for (int i = 1; i <= n; ++i)
        p.lineTo(x + wa * sx * i / n, Y(kmi::shapeCurve(static_cast<float>(i) / n, e.attackCurve)));
    x += wa * sx;
    x += wh * sx;
    p.lineTo(x, Y(1.0f));
    for (int i = 1; i <= n; ++i)
        p.lineTo(x + wdc * sx * i / n, Y(S + (1.0f - S) * (1.0f - kmi::shapeCurve(static_cast<float>(i) / n, e.decayCurve))));
    x += wdc * sx;
    x += ws * sx;
    p.lineTo(x, Y(S));
    for (int i = 1; i <= n; ++i)
        p.lineTo(x + wr * sx * i / n, Y(S * (1.0f - kmi::shapeCurve(static_cast<float>(i) / n, e.releaseCurve))));

    if (fill)
    {
        juce::Path area(p);
        area.lineTo(r.getRight(), r.getBottom());
        area.closeSubPath();
        g.setColour(colour.withAlpha(0.15f));
        g.fillPath(area);
    }
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(1.6f));
    if (e.loop != 0)
    {
        g.setColour(colour.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(e.loop == kmi::kEnvLoopInfinite ? "loop inf" : "loop x" + juce::String(e.loop), r.reduced(3.0f),
                   juce::Justification::topRight);
    }
}

EnvelopeView::EnvelopeView(std::function<const kmi::EnvParams&()> get, juce::Colour colour)
    : get_(std::move(get)), colour_(colour)
{
    startTimerHz(15);
}

void EnvelopeView::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, r);
    drawEnvelope(g, r.reduced(6.0f), get_(), colour_, true);
}

EnvelopeOverlay::EnvelopeOverlay(UiContext& ctx) : ctx_(ctx) { startTimerHz(15); }

void EnvelopeOverlay::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, r);
    for (int i = kmi::kNumOscs - 1; i >= 0; --i)
    {
        const auto& op = ctx_.patch.osc[static_cast<size_t>(i)];
        if (op.envLevel == 0.0f || op.mute) continue; // the envelope does nothing for this oscillator
        drawEnvelope(g, r.reduced(6.0f), op.env, theme::osc(i), false);
    }
}

// ---- Step editor ------------------------------------------------------------------------------------------
StepEditor::StepEditor(UiContext& ctx, int lfoIndex) : ctx_(ctx), lfo_(lfoIndex)
{
    startTimerHz(15);
    setTooltip("Drag to draw step values. Right-drag to clear steps.");
}

int StepEditor::numSteps() const
{
    return juce::jlimit(2, kmi::kMaxLfoSteps, ctx_.patch.lfo[static_cast<size_t>(lfo_)].steps);
}

void StepEditor::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, r);
    r = r.reduced(6.0f);
    const auto steps = ctx_.proc.getLfoSteps(lfo_);
    const int n = numSteps();
    const float w = r.getWidth() / static_cast<float>(n);
    const float mid = r.getCentreY();
    g.setColour(theme::panelEdge);
    g.drawHorizontalLine(static_cast<int>(mid), r.getX(), r.getRight());
    for (int i = 0; i < n; ++i)
    {
        const float v = juce::jlimit(-1.0f, 1.0f, steps[static_cast<size_t>(i)]);
        const float y = mid - v * r.getHeight() * 0.5f;
        auto bar = juce::Rectangle<float>(r.getX() + i * w + 1.0f, juce::jmin(y, mid), juce::jmax(1.0f, w - 2.0f),
                                          juce::jmax(1.5f, std::abs(y - mid)));
        g.setColour(theme::accent.withAlpha(0.75f));
        g.fillRect(bar);
    }
}

void StepEditor::edit(juce::Point<float> p, bool clearStep)
{
    auto r = getLocalBounds().toFloat().reduced(10.0f);
    const int n = numSteps();
    const int i = juce::jlimit(0, n - 1, static_cast<int>((p.x - r.getX()) / r.getWidth() * n));
    float v = clearStep ? 0.0f : juce::jlimit(-1.0f, 1.0f, (r.getCentreY() - p.y) / (r.getHeight() * 0.5f));
    // Snap near the centre so zero is easy to hit.
    if (std::abs(v) < 0.03f) v = 0.0f;
    auto steps = ctx_.proc.getLfoSteps(lfo_);
    if (steps[static_cast<size_t>(i)] == v) return;
    steps[static_cast<size_t>(i)] = v;
    ctx_.proc.setLfoSteps(lfo_, steps);
    repaint();
}

// ---- Scope ------------------------------------------------------------------------------------------------------
Scope::Scope(UiContext& ctx) : ctx_(ctx) { startTimerHz(30); }

void Scope::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, r);
    r = r.reduced(4.0f);
    constexpr int kRead = 2048, kShow = 768;
    float buf[kRead];
    ctx_.proc.copyScope(buf, kRead);
    int start = 0;
    for (int i = 1; i < kRead - kShow; ++i)
        if (buf[i - 1] <= 0.0f && buf[i] > 0.0f)
        {
            start = i;
            break;
        }
    float peak = 1.0e-3f;
    for (int i = 0; i < kShow; ++i) peak = juce::jmax(peak, std::abs(buf[start + i]));
    const float scale = 0.45f * r.getHeight() / juce::jmax(peak, 0.25f);
    juce::Path p;
    for (int i = 0; i < kShow; ++i)
    {
        const float x = r.getX() + r.getWidth() * static_cast<float>(i) / (kShow - 1);
        const float y = r.getCentreY() - buf[start + i] * scale;
        if (i == 0) p.startNewSubPath(x, y);
        else p.lineTo(x, y);
    }
    g.setColour(theme::accent);
    g.strokePath(p, juce::PathStrokeType(1.3f));
    g.setColour(theme::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("Voices " + juce::String(ctx_.proc.activeVoices()), r.reduced(2.0f), juce::Justification::topRight);
}

// ---- Wave panel ----------------------------------------------------------------------------------------------
WavePanel::WavePanel(UiContext& ctx, int osc, juce::Colour colour) : ctx_(ctx), osc_(osc), colour_(colour)
{
    load_.setTooltip("Load a single-cycle WAV (or a wavetable WAV of 2048-sample frames) into a User wave slot");
    clear_.setTooltip("Empty the selected User wave slot");
    load_.onClick = [this] { loadWav(); };
    clear_.onClick = [this] {
        const int w = ctx_.patch.osc[static_cast<size_t>(osc_)].wave;
        if (kmi::WavetableBank::isUserWave(w)) ctx_.proc.clearUserWave(w - kmi::WavetableBank::kUserWaveFirst);
    };
    addAndMakeVisible(load_);
    addAndMakeVisible(clear_);
    setTooltip("The waveform this oscillator plays (with Wave Scan: the live crossfade)");
    startTimerHz(20);
}

void WavePanel::resized()
{
    auto r = getLocalBounds().reduced(4, 2);
    auto buttons = r.removeFromBottom(20);
    load_.setBounds(buttons.removeFromLeft(buttons.getWidth() * 3 / 5).reduced(1, 0));
    clear_.setBounds(buttons.reduced(1, 0));
}

std::vector<float> WavePanel::waveCycle(UiContext& ctx, int wave)
{
    using Bank = kmi::WavetableBank;
    std::vector<float> c;
    if (Bank::isUserWave(wave))
    {
        c = ctx.proc.userWaveCycle(wave - Bank::kUserWaveFirst);
        float peak = 0.0f;
        for (float v : c) peak = std::fmax(peak, std::fabs(v));
        if (peak > 0.0f)
            for (float& v : c) v /= peak;
        return c;
    }
    const float* t = Bank::instance().table(wave, 0);
    c.assign(t, t + Bank::kTableSize);
    return c;
}

float WavePanel::scanPosition() const
{
    const auto& op = ctx_.patch.osc[static_cast<size_t>(osc_)];
    if (op.scanRange <= 0) return -1.0f;
    const auto& bank = kmi::WavetableBank::instance();
    float scan = op.scan;
    if (ctx_.proc.hasLiveMods())
        scan += ctx_.proc.liveMod(kmi::dst::OscScanBase + osc_) + ctx_.proc.liveMod(kmi::dst::AllOscScan);
    scan = juce::jlimit(0.0f, 1.0f, scan);
    return std::fmin(static_cast<float>(bank.menuPosition(op.wave)) + scan * static_cast<float>(op.scanRange),
                     static_cast<float>(bank.menuSize() - 1));
}

void WavePanel::timerCallback()
{
    const auto& op = ctx_.patch.osc[static_cast<size_t>(osc_)];
    const bool user = kmi::WavetableBank::isUserWave(op.wave);
    clear_.setEnabled(user && ctx_.proc.hasUserWave(op.wave - kmi::WavetableBank::kUserWaveFirst));
    const juce::String key = juce::String(op.wave) + "/" + juce::String(scanPosition(), 3) + "/" +
                             juce::String(ctx_.proc.userWaveVersion()) + "/" + juce::String(op.invert ? 1 : 0);
    if (key != lastKey_)
    {
        lastKey_ = key;
        repaint();
    }
}

void WavePanel::paint(juce::Graphics& g)
{
    using Bank = kmi::WavetableBank;
    const auto& bank = Bank::instance();
    const auto& op = ctx_.patch.osc[static_cast<size_t>(osc_)];
    auto r = getLocalBounds().toFloat().reduced(4.0f, 2.0f);
    r.removeFromBottom(22.0f);
    drawPanelBackground(g, r);
    auto plot = r.reduced(4.0f, 5.0f);
    g.setColour(theme::panelEdge);
    g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()), plot.getX(), plot.getRight());

    const float sign = op.invert ? -1.0f : 1.0f;
    auto drawCycle = [&](const std::vector<float>& c, juce::Colour col, float thickness) {
        if (c.empty()) return;
        juce::Path path;
        const int points = juce::jmax(2, static_cast<int>(plot.getWidth()));
        for (int k = 0; k <= points; ++k)
        {
            const float ph = static_cast<float>(k) / static_cast<float>(points);
            const float v = sign * c[static_cast<size_t>(juce::jmin(static_cast<int>(ph * c.size()), static_cast<int>(c.size()) - 1))];
            const float x = plot.getX() + ph * plot.getWidth();
            const float y = plot.getCentreY() - v * plot.getHeight() * 0.46f;
            if (k == 0) path.startNewSubPath(x, y);
            else path.lineTo(x, y);
        }
        g.setColour(col);
        g.strokePath(path, juce::PathStrokeType(thickness));
    };

    const float pos = scanPosition();
    juce::String label;
    if (pos >= 0.0f)
    {
        const int first = bank.menuPosition(op.wave);
        const int last = juce::jmin(first + op.scanRange, bank.menuSize() - 1);
        drawCycle(waveCycle(ctx_, bank.waveAtMenuPosition(first)), colour_.withAlpha(0.22f), 1.0f);
        drawCycle(waveCycle(ctx_, bank.waveAtMenuPosition(last)), colour_.withAlpha(0.22f), 1.0f);
        const int p0 = static_cast<int>(pos);
        const float mix = pos - static_cast<float>(p0);
        auto a = waveCycle(ctx_, bank.waveAtMenuPosition(p0));
        auto b = waveCycle(ctx_, bank.waveAtMenuPosition(juce::jmin(p0 + 1, bank.menuSize() - 1)));
        if (a.empty()) a.assign(Bank::kTableSize, 0.0f);
        if (b.empty()) b.assign(Bank::kTableSize, 0.0f);
        const size_t n = std::min(a.size(), b.size());
        for (size_t k = 0; k < n; ++k) a[k] += mix * (b[k] - a[k]);
        drawCycle(a, colour_, 1.6f);
        const int nearest = bank.waveAtMenuPosition(juce::roundToInt(pos));
        label = juce::String("Scan: ") + bank.waveName(nearest);
    }
    else
    {
        const auto c = waveCycle(ctx_, op.wave);
        drawCycle(c, colour_, 1.6f);
        if (c.empty()) label = juce::String(bank.waveName(op.wave)) + " is empty: Load WAV...";
    }
    if (Bank::isUserWave(op.wave) && label.isEmpty())
        label = ctx_.proc.userWaveName(op.wave - Bank::kUserWaveFirst);
    if (label.isNotEmpty())
    {
        g.setColour(theme::textDim);
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.drawText(label, plot.toNearestInt().removeFromTop(12), juce::Justification::topLeft, true);
    }
}

void WavePanel::loadWav()
{
    using Bank = kmi::WavetableBank;
    chooser_ = std::make_unique<juce::FileChooser>("Load a single-cycle wave or wavetable",
                                                   juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<WavePanel>(this)](const juce::FileChooser& fc) {
        if (!safe || fc.getResult() == juce::File()) return;
        auto& ctx = safe->ctx_;
        const int osc = safe->osc_;
        const int wave = ctx.patch.osc[static_cast<size_t>(osc)].wave;
        // The selected User slot, else the first empty one, else User 1.
        int slot = Bank::isUserWave(wave) ? wave - Bank::kUserWaveFirst : -1;
        for (int i = 0; i < Bank::kNumUserWaves && slot < 0; ++i)
            if (!ctx.proc.hasUserWave(i)) slot = i;
        if (slot < 0) slot = 0;
        int used = 0;
        const juce::String error = ctx.proc.loadUserWave(slot, fc.getResult(), used);
        if (error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Load WAV", error);
            return;
        }
        const juce::String p = "osc" + juce::String(osc + 1) + "_";
        setParam(ctx, p + "wave", static_cast<float>(Bank::kUserWaveFirst + slot));
        if (used > 1) // a wavetable file: scan across the frames just loaded
        {
            setParam(ctx, p + "scanrange", static_cast<float>(used - 1));
            setParam(ctx, p + "scan", 0.0f);
        }
    });
}

// ---- Scale view ------------------------------------------------------------------------------------------------
ScaleView::ScaleView(UiContext& ctx) : ctx_(ctx) { startTimerHz(10); }

void ScaleView::paint(juce::Graphics& g)
{
    const auto& sp = ctx_.patch.scale;
    int custom = 0;
    for (int k = 0; k < 12; ++k) custom |= sp.custom[static_cast<size_t>(k)] ? 1 << k : 0;
    const int mask = kmi::absoluteMask(sp.scale, sp.root, custom);
    const bool scala = sp.tuning == kmi::scalaTuningIndex();
    const kmi::TuningTable table = scala ? ctx_.proc.scalaTable() : kmi::builtInTuning(sp.tuning, sp.root);

    auto r = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, r);
    r = r.reduced(8.0f, 6.0f);
    const float w = r.getWidth() / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (sp.root + i) % 12;
        const int note = 60 + pc;
        const bool in = (mask & (1 << pc)) != 0;
        const bool black = juce::MidiMessage::isMidiNoteBlack(pc);
        juce::Rectangle<float> key(r.getX() + i * w + 2.0f, r.getY(), w - 4.0f, r.getHeight());
        g.setColour(black ? theme::panel.darker(0.3f) : theme::panel.brighter(0.15f));
        g.fillRoundedRectangle(key, 4.0f);
        if (in)
        {
            g.setColour((i == 0 ? theme::accentWarm : theme::accent).withAlpha(0.75f));
            g.fillRoundedRectangle(key.removeFromTop(key.getHeight() * 0.45f).reduced(3.0f), 3.0f);
        }
        auto text = juce::Rectangle<float>(r.getX() + i * w, r.getY() + r.getHeight() * 0.48f, w, r.getHeight() * 0.5f);
        g.setColour(theme::text);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText(juce::MidiMessage::getMidiNoteName(pc, true, false, 3), text.removeFromTop(18.0f), juce::Justification::centred);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(theme::textDim);
        const float pitch = table[static_cast<size_t>(note)];
        juce::String info;
        if (pitch < 0.0f) info = "silent";
        else if (scala) info = juce::String(pitch, 2);
        else
        {
            const int cents = juce::roundToInt((pitch - static_cast<float>(note)) * 100.0f);
            info = cents == 0 ? juce::String("0 ct") : (cents > 0 ? "+" : "") + juce::String(cents) + " ct";
        }
        g.drawText(info, text.removeFromTop(16.0f), juce::Justification::centred);
    }
}

// ---- Scala panel -----------------------------------------------------------------------------------------------
ScalaPanel::ScalaPanel(UiContext& ctx) : ctx_(ctx)
{
    load_.setTooltip("Load a Scala scale (.scl). A .kbm keyboard mapping with the same name is picked up too, or load one "
                     "separately after the scale.");
    clear_.setTooltip("Forget the Scala tuning");
    load_.onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser>("Load a Scala scale or keyboard mapping",
                                                       juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.scl;*.kbm");
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<ScalaPanel>(this)](const juce::FileChooser& fc) {
            if (!safe || fc.getResult() == juce::File()) return;
            const juce::String error = safe->ctx_.proc.loadScalaFile(fc.getResult());
            if (error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Scala tuning", error);
                return;
            }
            setParam(safe->ctx_, "tuning_type", static_cast<float>(kmi::scalaTuningIndex()));
        });
    };
    clear_.onClick = [this] { ctx_.proc.clearScala(); };
    name_.setColour(juce::Label::textColourId, theme::text);
    name_.setJustificationType(juce::Justification::centredLeft);
    for (auto* c : { static_cast<juce::Component*>(&load_), static_cast<juce::Component*>(&clear_), static_cast<juce::Component*>(&name_) })
        addAndMakeVisible(c);
    timerCallback();
    startTimerHz(4);
}

void ScalaPanel::resized()
{
    auto r = getLocalBounds().reduced(4);
    auto row = r.removeFromTop(26);
    load_.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(6);
    clear_.setBounds(row.removeFromLeft(60));
    r.removeFromTop(6);
    name_.setBounds(r.removeFromTop(36));
}

void ScalaPanel::timerCallback()
{
    const juce::String n = ctx_.proc.scalaName();
    const juce::String text = n.isEmpty() ? juce::String("No Scala file loaded")
                                          : n + (ctx_.proc.scalaHasMapping() ? "  (with keyboard mapping)" : "");
    if (name_.getText() != text) name_.setText(text, juce::dontSendNotification);
    clear_.setEnabled(n.isNotEmpty());
}

} // namespace kmiui
