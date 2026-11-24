// Per-instance user wavetables: 8 slots, each a single cycle loaded by the user (for example from a WAV
// file) and band-limited into mips like the built-in waves. Wave indices WavetableBank::kUserWaveFirst..+7
// read these slots; an empty slot is silent.
//
// Threading: set/clear/cycle/name are message-thread only. table() is real-time safe and lock-free. A
// replaced table is kept alive until the audio thread has finished at least two blocks since the swap.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "kmi/Wavetables.h"

namespace kmi {

class UserWavetables
{
public:
    static constexpr int kSlots = WavetableBank::kNumUserWaves;

    // Loads one cycle of any length (resampled to WavetableBank::kTableSize). Returns false, leaving the slot
    // unchanged, if the cycle is empty or silent.
    bool set(int slot, const std::vector<float>& cycle, const std::string& name);
    void clear(int slot);
    void clearAll();

    bool has(int slot) const noexcept { return valid(slot) && !slots_[static_cast<size_t>(slot)].cycle.empty(); }
    // The stored cycle (kTableSize samples, as loaded, before band-limiting) and its display name.
    const std::vector<float>& cycle(int slot) const noexcept;
    const std::string& name(int slot) const noexcept;

    // Audio thread: the slot's mip table, or nullptr if empty.
    const float* table(int slot, int mip) const noexcept
    {
        if (!valid(slot)) return nullptr;
        const float* d = live_[static_cast<size_t>(slot)].load(std::memory_order_acquire);
        return d ? d + mip * WavetableBank::kMipFloats : nullptr;
    }

    // Audio thread: call once per processed block (lets retired tables be freed).
    void audioTick() noexcept { ticks_.fetch_add(1, std::memory_order_release); }

    // Resamples any single cycle to kTableSize samples (linear interpolation).
    static std::vector<float> resample(const std::vector<float>& cycle);

private:
    static bool valid(int slot) noexcept { return slot >= 0 && slot < kSlots; }
    void publish(int slot, std::unique_ptr<float[]> mips);
    void collect();

    struct Slot
    {
        std::vector<float> cycle;
        std::string name;
        std::unique_ptr<float[]> mips;
    };
    struct Retired
    {
        std::unique_ptr<float[]> mips;
        uint64_t tick;
    };
    std::array<std::atomic<const float*>, kSlots> live_{};
    std::array<Slot, kSlots> slots_;
    std::vector<Retired> retired_;
    std::atomic<uint64_t> ticks_{ 0 };
};

} // namespace kmi
