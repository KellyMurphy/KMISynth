#include "Views.h"

#include "../PluginProcessor.h"
#include "kmi/Dsp.h"
#include "kmi/Lfo.h"
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
namespace {
// Segment widths grow with the square root of the time, so short and long stages both stay visible.
float segW(float ms) { return std::sqrt(juce::jmax(0.0f, ms)) + 2.0f; }
float segMs(float w) { return juce::square(juce::jmax(0.0f, w - 2.0f)); }

struct EnvSlow
{
    float a, d, dl; // time stretch of attack/hold, decay/release and delay in Slow mode
};
EnvSlow envSlow(const kmi::EnvParams& e)
{
    return e.slow ? EnvSlow{ 600.0f / 36.0f, 15.0f, 1.875f } : EnvSlow{ 1.0f, 1.0f, 1.0f };
}

// Where each stage ends on screen. sx: pixels per width unit (fixedSx > 0 holds the scale steady during a drag).
struct EnvGeometry
{
    float sx, x0, xDelay, xAttack, xHold, xDecay, xSustain, xRelease;
    float wd, wa, wh, wdc, ws, wr;
};
EnvGeometry envGeometry(juce::Rectangle<float> r, const kmi::EnvParams& e, float fixedSx)
{
    const auto k = envSlow(e);
    EnvGeometry g{};
    g.wd = e.delayMs > 0.0f ? segW(e.delayMs * k.dl) : 0.0f;
    g.wa = segW(e.attackMs * k.a);
    g.wh = e.holdMs > 0.0f ? segW(e.holdMs * k.a) : 0.0f;
    g.wdc = segW(e.decayMs * k.d);
    g.wr = segW(e.releaseMs * k.d);
    g.ws = (g.wd + g.wa + g.wh + g.wdc + g.wr) * 0.25f + 10.0f;
    const float total = g.wd + g.wa + g.wh + g.wdc + g.ws + g.wr;
    g.sx = fixedSx > 0.0f ? fixedSx : r.getWidth() / total;
    g.x0 = r.getX();
    g.xDelay = g.x0 + g.wd * g.sx;
    g.xAttack = g.xDelay + g.wa * g.sx;
    g.xHold = g.xAttack + g.wh * g.sx;
    g.xDecay = g.xHold + g.wdc * g.sx;
    g.xSustain = g.xDecay + g.ws * g.sx;
    g.xRelease = g.xSustain + g.wr * g.sx;
    return g;
}

// Sets a parameter from an engine (patch) value, inside a gesture the caller opened.
void setFromPatch(UiContext& ctx, const juce::String& id, float v)
{
    auto* p = param(ctx, id);
    if (p == nullptr) return;
    float scale = 1.0f, offset = 0.0f;
    if (!kmiplug::ParameterBinding::patchMapping(id, scale, offset) || scale == 0.0f)
    {
        scale = 1.0f;
        offset = 0.0f;
    }
    p->setValueNotifyingHost(p->convertTo0to1((v - offset) / scale));
}

void gesture(UiContext& ctx, const juce::StringArray& ids, bool begin)
{
    for (const auto& id : ids)
        if (auto* p = param(ctx, id)) begin ? p->beginChangeGesture() : p->endChangeGesture();
}

void drawHandle(juce::Graphics& g, juce::Point<float> p, juce::Colour colour, bool hot)
{
    const float rad = hot ? 6.5f : 5.0f;
    g.setColour(hot ? colour : theme::panel);
    g.fillEllipse(p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
    g.setColour(hot ? theme::text : colour);
    g.drawEllipse(p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f, 1.8f);
}
} // namespace

void drawEnvelope(juce::Graphics& g, juce::Rectangle<float> r, const kmi::EnvParams& e, juce::Colour colour, bool fill)
{
    const auto geo = envGeometry(r, e, 0.0f);
    const float S = juce::jlimit(0.0f, 1.0f, e.sustain), L = juce::jlimit(0.0f, 1.0f, e.level);
    auto Y = [&](float v) { return r.getBottom() - v * L * r.getHeight(); };

    juce::Path p;
    p.startNewSubPath(geo.x0, Y(0.0f));
    p.lineTo(geo.xDelay, Y(0.0f));
    const int n = 24;
    for (int i = 1; i <= n; ++i)
        p.lineTo(geo.xDelay + geo.wa * geo.sx * i / n, Y(kmi::shapeCurve(static_cast<float>(i) / n, e.attackCurve)));
    p.lineTo(geo.xHold, Y(1.0f));
    for (int i = 1; i <= n; ++i)
        p.lineTo(geo.xHold + geo.wdc * geo.sx * i / n,
                 Y(S + (1.0f - S) * (1.0f - kmi::shapeCurve(static_cast<float>(i) / n, e.decayCurve))));
    p.lineTo(geo.xSustain, Y(S));
    for (int i = 1; i <= n; ++i)
        p.lineTo(geo.xSustain + geo.wr * geo.sx * i / n, Y(S * (1.0f - kmi::shapeCurve(static_cast<float>(i) / n, e.releaseCurve))));

    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(r.expanded(8.0f).toNearestInt());
    if (fill)
    {
        juce::Path area(p);
        area.lineTo(juce::jmax(geo.xRelease, r.getRight()), r.getBottom());
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

EnvelopeView::EnvelopeView(std::function<const kmi::EnvParams&()> get, juce::Colour colour, UiContext* ctx, juce::String prefix)
    : get_(std::move(get)), colour_(colour), ctx_(ctx), prefix_(std::move(prefix))
{
    startTimerHz(15);
    if (editable())
        setTooltip("Drag the handles: delay, attack peak, hold, decay and sustain level, release." +
                   juce::String(hasLevel() ? " Drag the attack peak up or down for the envelope's Level." : ""));
}

bool EnvelopeView::editable() const { return ctx_ != nullptr && prefix_.isNotEmpty(); }
bool EnvelopeView::hasLevel() const { return editable() && param(*ctx_, prefix_ + "level") != nullptr; }

juce::Rectangle<float> EnvelopeView::plotArea() const { return getLocalBounds().toFloat().reduced(4.0f).reduced(8.0f); }

std::array<juce::Point<float>, 5> EnvelopeView::handlePositions() const
{
    const auto& e = get_();
    const auto r = plotArea();
    const auto geo = envGeometry(r, e, 0.0f);
    const float L = juce::jlimit(0.0f, 1.0f, e.level), S = juce::jlimit(0.0f, 1.0f, e.sustain);
    const float top = r.getBottom() - L * r.getHeight(), sus = r.getBottom() - S * L * r.getHeight();
    // A zero-length hold sits just right of the attack peak so both can be grabbed.
    const float holdX = e.holdMs > 0.0f ? geo.xHold : geo.xAttack + 12.0f;
    return { juce::Point<float>(geo.xDelay, r.getBottom()), { geo.xAttack, top }, { holdX, top }, { geo.xDecay, sus },
             { geo.xRelease, r.getBottom() } };
}

int EnvelopeView::handleAt(juce::Point<float> p) const
{
    if (!editable() || !isEnabled()) return -1;
    const auto h = handlePositions();
    int best = -1;
    float bestD = 10.0f;
    for (int i = 0; i < static_cast<int>(h.size()); ++i)
    {
        const float d = h[static_cast<size_t>(i)].getDistanceFrom(p);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void EnvelopeView::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, r);
    drawEnvelope(g, plotArea(), get_(), colour_, true);
    if (!editable() || !isEnabled()) return;
    const auto h = handlePositions();
    for (int i = 0; i < static_cast<int>(h.size()); ++i)
        drawHandle(g, h[static_cast<size_t>(i)], colour_, i == hover_ || i == drag_);
}

void EnvelopeView::mouseMove(const juce::MouseEvent& e)
{
    const int h = handleAt(e.position);
    if (h != hover_)
    {
        hover_ = h;
        setMouseCursor(h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EnvelopeView::mouseExit(const juce::MouseEvent&)
{
    if (hover_ >= 0 && drag_ < 0)
    {
        hover_ = -1;
        repaint();
    }
}

namespace {
const char* const kEnvStageIds[5] = { "delay", "attack", "hold", "decay", "release" };
}

void EnvelopeView::mouseDown(const juce::MouseEvent& e)
{
    drag_ = handleAt(e.position);
    if (drag_ < 0) return;
    const auto& env = get_();
    const auto geo = envGeometry(plotArea(), env, 0.0f);
    grabOffset_ = e.position - handlePositions()[static_cast<size_t>(drag_)];
    dragStartX_ = e.position.x;
    dragStartW_ = geo.wr;
    dragStartSx_ = geo.sx;
    juce::StringArray ids{ prefix_ + kEnvStageIds[drag_] };
    if (drag_ == 3) ids.add(prefix_ + "sustain");
    if (drag_ == 1 && hasLevel()) ids.add(prefix_ + "level");
    gesture(*ctx_, ids, true);
    startTimerHz(60); // redraw the new shape as fast as the patch snapshot follows the drag
}

void EnvelopeView::mouseDrag(const juce::MouseEvent& e)
{
    if (drag_ < 0) return;
    const auto& env = get_();
    const auto k = envSlow(env);
    const float stretch[5] = { k.dl, k.a, k.a, k.d, k.d };
    const auto r = plotArea();
    const auto pos = e.position - grabOffset_;
    const auto geo = envGeometry(r, env, 0.0f);
    float w;
    if (drag_ == 4)
    {
        // The release always ends at the right edge (the graph refits), so it follows the mouse's movement.
        w = dragStartW_ + (e.position.x - dragStartX_) / dragStartSx_;
    }
    else
    {
        // The graph refits as the stage changes, so solve for the width that puts the stage's end under the
        // mouse: with A the widths before it and C the other four stages, the end sits at fraction
        // (A + w) / (1.25 (C + w) + 10) of the width (the sustain plateau is a quarter of the stages plus 10).
        const float seg[5] = { geo.wd, geo.wa, geo.wh, geo.wdc, geo.wr };
        float A = 0.0f, C = 0.0f;
        for (int i = 0; i < 5; ++i)
        {
            if (i < drag_) A += seg[i];
            if (i != drag_) C += seg[i];
        }
        const float f = juce::jlimit(0.0f, 0.79f, (pos.x - r.getX()) / r.getWidth());
        w = (f * (1.25f * C + 10.0f) - A) / (1.0f - 1.25f * f);
    }
    setFromPatch(*ctx_, prefix_ + kEnvStageIds[drag_], segMs(w) / stretch[drag_]);

    const float up = juce::jlimit(0.0f, 1.0f, (r.getBottom() - pos.y) / r.getHeight());
    if (drag_ == 3)
    {
        const float L = juce::jlimit(0.0f, 1.0f, env.level);
        if (L > 0.0f) setFromPatch(*ctx_, prefix_ + "sustain", juce::jlimit(0.0f, 1.0f, up / L));
    }
    if (drag_ == 1 && hasLevel()) setFromPatch(*ctx_, prefix_ + "level", up);
    repaint();
}

void EnvelopeView::mouseUp(const juce::MouseEvent&)
{
    if (drag_ < 0) return;
    juce::StringArray ids{ prefix_ + kEnvStageIds[drag_] };
    if (drag_ == 3) ids.add(prefix_ + "sustain");
    if (drag_ == 1 && hasLevel()) ids.add(prefix_ + "level");
    gesture(*ctx_, ids, false);
    drag_ = -1;
    startTimerHz(15);
    repaint();
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

// ---- LFO view ----------------------------------------------------------------------------------------------
namespace {
constexpr int kLfoCycles = 3;
float lfoHz(const kmi::LfoParams& p)
{
    const float hz = p.bpmSync ? 1.0f / kmi::syncDivisionSeconds(p.syncDiv, 120.0) : p.rateHz;
    return juce::jmax(0.01f, hz);
}
const char* const kLfoHandleIds[4] = { "level", "phase", "delay", "fade" };
} // namespace

LfoView::LfoView(UiContext& ctx, int lfoIndex, juce::Colour colour) : ctx_(ctx), lfo_(lfoIndex), colour_(colour)
{
    startTimerHz(10);
    setTooltip("Drag the handles: Level (right edge), Phase (where the wave starts), Delay (bottom marker), "
               "Fade In (top marker).");
}

juce::Rectangle<float> LfoView::plotArea() const { return getLocalBounds().toFloat().reduced(4.0f).reduced(14.0f, 14.0f); }

std::array<juce::Point<float>, 4> LfoView::handlePositions() const
{
    const auto& p = ctx_.patch.lfo[static_cast<size_t>(lfo_)];
    const auto r = plotArea();
    const float hz = lfoHz(p), perCycle = r.getWidth() / kLfoCycles, mid = r.getCentreY();
    auto X = [&](float cycles) { return juce::jmin(r.getRight(), r.getX() + cycles * perCycle); };
    const float delayC = p.delayMs * 0.001f * hz, fadeC = p.fadeMs * 0.001f * hz;
    return { juce::Point<float>(r.getRight(), mid - juce::jlimit(0.0f, 1.0f, p.level) * r.getHeight() * 0.5f),
             { X(delayC), startY_ }, { X(delayC), r.getBottom() }, { X(delayC + fadeC), r.getY() } };
}

int LfoView::handleAt(juce::Point<float> pt) const
{
    if (!isEnabled()) return -1;
    const auto h = handlePositions();
    int best = -1;
    float bestD = 10.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float d = h[static_cast<size_t>(i)].getDistanceFrom(pt);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void LfoView::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat().reduced(4.0f);
    drawPanelBackground(g, outer);
    const auto r = plotArea();
    const float mid = r.getCentreY();
    g.setColour(theme::panelEdge);
    g.drawHorizontalLine(juce::roundToInt(mid), r.getX(), r.getRight());
    for (int c = 1; c < kLfoCycles; ++c)
    {
        const float x = r.getX() + r.getWidth() * static_cast<float>(c) / kLfoCycles;
        g.setColour(theme::panelEdge.withAlpha(0.6f));
        g.drawVerticalLine(juce::roundToInt(x), r.getY(), r.getBottom());
    }

    // Run a copy of the LFO offline: 256 samples per cycle, so smoothing, delay and fade keep their real
    // proportions to the rate (synced rates at 120 BPM).
    constexpr int kPerCycle = 256, kN = kPerCycle * kLfoCycles;
    const auto& p = ctx_.patch.lfo[static_cast<size_t>(lfo_)];
    const auto steps = ctx_.proc.getLfoSteps(lfo_);
    const float sr = lfoHz(p) * kPerCycle;
    kmi::Lfo lfo;
    lfo.reset(1234u);
    lfo.update(p, steps.data(), 1.0f, 0.0f, sr, 120.0);
    lfo.retrigger(p, sr);

    // The wave's own starting value (after any delay) places the Phase handle.
    kmi::LfoParams noDelay = p;
    noDelay.delayMs = 0.0f;
    noDelay.fadeMs = 0.0f;
    kmi::Lfo probe;
    probe.reset(1234u);
    probe.update(noDelay, steps.data(), 1.0f, 0.0f, sr, 120.0);
    probe.retrigger(noDelay, sr);
    startY_ = mid - juce::jlimit(-1.0f, 1.0f, probe.tick()) * r.getHeight() * 0.5f;

    juce::Path line;
    for (int n = 0; n < kN; ++n)
    {
        const float v = juce::jlimit(-1.0f, 1.0f, lfo.tick());
        const float x = r.getX() + r.getWidth() * static_cast<float>(n) / static_cast<float>(kN - 1);
        const float y = mid - v * r.getHeight() * 0.5f;
        if (n == 0) line.startNewSubPath(x, y);
        else line.lineTo(x, y);
    }
    juce::Path fill(line);
    fill.lineTo(r.getRight(), mid);
    fill.lineTo(r.getX(), mid);
    fill.closeSubPath();
    g.setColour(colour_.withAlpha(0.18f));
    g.fillPath(fill);
    g.setColour(colour_);
    g.strokePath(line, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(theme::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(juce::String(kLfoCycles) + " cycles from note-on", r.reduced(2.0f), juce::Justification::topRight);
    if (!isEnabled()) return;

    // Level guide, delay and fade markers, then the handles.
    const auto h = handlePositions();
    const float dashes[] = { 4.0f, 4.0f };
    g.setColour(colour_.withAlpha(0.5f));
    g.drawDashedLine(juce::Line<float>(r.getX(), h[0].y, r.getRight(), h[0].y), dashes, 2, 1.0f);
    g.drawDashedLine(juce::Line<float>(h[2].x, r.getY(), h[2].x, r.getBottom()), dashes, 2, 1.0f);
    if (h[3].x > h[2].x + 0.5f) g.drawDashedLine(juce::Line<float>(h[3].x, r.getY(), h[3].x, r.getBottom()), dashes, 2, 1.0f);
    for (int i = 0; i < 4; ++i) drawHandle(g, h[static_cast<size_t>(i)], colour_, i == hover_ || i == drag_);
}

void LfoView::mouseMove(const juce::MouseEvent& e)
{
    const int h = handleAt(e.position);
    if (h != hover_)
    {
        hover_ = h;
        setMouseCursor(h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void LfoView::mouseExit(const juce::MouseEvent&)
{
    if (hover_ >= 0 && drag_ < 0)
    {
        hover_ = -1;
        repaint();
    }
}

void LfoView::mouseDown(const juce::MouseEvent& e)
{
    drag_ = handleAt(e.position);
    if (drag_ < 0) return;
    dragStartX_ = e.position.x;
    dragStartPhase_ = ctx_.patch.lfo[static_cast<size_t>(lfo_)].phaseDeg;
    gesture(ctx_, { "lfo" + juce::String(lfo_ + 1) + "_" + kLfoHandleIds[drag_] }, true);
    startTimerHz(60);
}

void LfoView::mouseDrag(const juce::MouseEvent& e)
{
    if (drag_ < 0) return;
    const auto& p = ctx_.patch.lfo[static_cast<size_t>(lfo_)];
    const auto r = plotArea();
    const float perCycle = r.getWidth() / kLfoCycles, hz = lfoHz(p);
    const juce::String id = "lfo" + juce::String(lfo_ + 1) + "_" + kLfoHandleIds[drag_];
    const float cycles = juce::jmax(0.0f, (e.position.x - r.getX()) / perCycle);
    switch (drag_)
    {
        case 0: setFromPatch(ctx_, id, juce::jlimit(0.0f, 1.0f, (r.getCentreY() - e.position.y) / (r.getHeight() * 0.5f))); break;
        case 1:
        {
            // Dragging right slides the wave right (an earlier start phase).
            float ph = dragStartPhase_ - (e.position.x - dragStartX_) / perCycle * 360.0f;
            ph = std::fmod(ph, 360.0f);
            if (ph < 0.0f) ph += 360.0f;
            setFromPatch(ctx_, id, ph);
            break;
        }
        case 2: setFromPatch(ctx_, id, cycles / hz * 1000.0f); break;
        default:
        {
            const float delayC = p.delayMs * 0.001f * hz;
            setFromPatch(ctx_, id, juce::jmax(0.0f, cycles - delayC) / hz * 1000.0f);
            break;
        }
    }
    repaint();
}

void LfoView::mouseUp(const juce::MouseEvent&)
{
    if (drag_ < 0) return;
    gesture(ctx_, { "lfo" + juce::String(lfo_ + 1) + "_" + kLfoHandleIds[drag_] }, false);
    drag_ = -1;
    startTimerHz(10);
    repaint();
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
