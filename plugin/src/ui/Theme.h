#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kmiui::theme {

inline const juce::Colour background{ 0xff14161b };
inline const juce::Colour panel{ 0xff1d2027 };
inline const juce::Colour panelEdge{ 0xff2d313b };
inline const juce::Colour control{ 0xff262a33 };
inline const juce::Colour text{ 0xffdde1e8 };
inline const juce::Colour textDim{ 0xff8b919d };
inline const juce::Colour accent{ 0xff4fc3f7 };
inline const juce::Colour accentWarm{ 0xffffb74d };
inline const juce::Colour track{ 0xff383d48 };

// One colour per oscillator, used consistently across every page.
inline juce::Colour osc(int index)
{
    static const juce::uint32 c[8] = { 0xff4fc3f7, 0xff81c784, 0xffffb74d, 0xffe57373,
                                       0xffba68c8, 0xfff06292, 0xff4db6ac, 0xfffff176 };
    return juce::Colour(c[index & 7]);
}

} // namespace kmiui::theme
