#include "kmi/UserWavetables.h"

#include <cmath>

namespace kmi {

std::vector<float> UserWavetables::resample(const std::vector<float>& cycle)
{
    constexpr int N = WavetableBank::kTableSize;
    std::vector<float> out(N, 0.0f);
    if (cycle.empty()) return out;
    const auto len = static_cast<double>(cycle.size());
    for (int n = 0; n < N; ++n)
    {
        const double pos = static_cast<double>(n) * len / N;
        const auto i = static_cast<size_t>(pos);
        const double frac = pos - static_cast<double>(i);
        const float a = cycle[i % cycle.size()], b = cycle[(i + 1) % cycle.size()];
        out[static_cast<size_t>(n)] = static_cast<float>(a + frac * (b - a));
    }
    return out;
}

bool UserWavetables::set(int slot, const std::vector<float>& cycle, const std::string& name)
{
    if (!valid(slot) || cycle.empty()) return false;
    std::vector<float> c = cycle.size() == WavetableBank::kTableSize ? cycle : resample(cycle);
    auto mips = std::make_unique<float[]>(WavetableBank::kTableFloats);
    if (!WavetableBank::buildMips(c.data(), mips.get())) return false;
    Slot& s = slots_[static_cast<size_t>(slot)];
    s.cycle = std::move(c);
    s.name = name;
    publish(slot, std::move(mips));
    return true;
}

void UserWavetables::clear(int slot)
{
    if (!valid(slot)) return;
    Slot& s = slots_[static_cast<size_t>(slot)];
    s.cycle.clear();
    s.name.clear();
    publish(slot, nullptr);
}

void UserWavetables::clearAll()
{
    for (int i = 0; i < kSlots; ++i) clear(i);
}

const std::vector<float>& UserWavetables::cycle(int slot) const noexcept
{
    static const std::vector<float> empty;
    return valid(slot) ? slots_[static_cast<size_t>(slot)].cycle : empty;
}

const std::string& UserWavetables::name(int slot) const noexcept
{
    static const std::string empty;
    return valid(slot) ? slots_[static_cast<size_t>(slot)].name : empty;
}

void UserWavetables::publish(int slot, std::unique_ptr<float[]> mips)
{
    collect();
    Slot& s = slots_[static_cast<size_t>(slot)];
    live_[static_cast<size_t>(slot)].store(mips.get(), std::memory_order_release);
    if (s.mips) retired_.push_back({ std::move(s.mips), ticks_.load(std::memory_order_acquire) });
    s.mips = std::move(mips);
}

void UserWavetables::collect()
{
    const uint64_t now = ticks_.load(std::memory_order_acquire);
    for (size_t i = 0; i < retired_.size();)
    {
        if (now >= retired_[i].tick + 2)
        {
            retired_[i] = std::move(retired_.back());
            retired_.pop_back();
        }
        else ++i;
    }
}

} // namespace kmi
