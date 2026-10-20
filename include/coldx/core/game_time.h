#pragma once

namespace coldx {

class GameTime {
public:
    constexpr GameTime() = default;
    constexpr GameTime(float delta, double total, bool runningSlowly)
        : delta_(delta), total_(total), runningSlowly_(runningSlowly) {}

    // Seconds since the previous call. With a fixed timestep this is always the step length.
    constexpr float Delta() const { return delta_; }
    // Seconds of game time since Run started. Time dropped by the catch-up cap is not counted.
    constexpr double Total() const { return total_; }
    // True when the loop needed more than one update to catch up this frame.
    constexpr bool IsRunningSlowly() const { return runningSlowly_; }

private:
    float delta_ = 0.0f;
    double total_ = 0.0;
    bool runningSlowly_ = false;
};

} // namespace coldx
