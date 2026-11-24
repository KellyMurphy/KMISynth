#pragma once

#include <memory>

#include "Controls.h"

namespace kmiui {

juce::StringArray pageNames();
std::unique_ptr<juce::Component> createPage(int index, UiContext& ctx);

} // namespace kmiui
