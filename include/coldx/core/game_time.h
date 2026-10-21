#pragma once

namespace coldx {

class GameTime {
public:
    constexpr GameTime() = default;
    constexpr GameTime(float delta, double total, bool runningSlowly, float alpha = 0.0f)
        : delta_(delta), total_(total), runningSlowly_(runningSlowly), alpha_(alpha) {}

    // Seconds since the previous call. With a fixed timestep this is always the step length.
    constexpr float Delta() const { return delta_; }
    // Seconds of game time since Run started. Time dropped by the catch-up cap is not counted.
    constexpr double Total() const { return total_; }
    // True when the loop needed more than one update to catch up this frame.
    constexpr bool IsRunningSlowly() const { return runningSlowly_; }
    // In Draw with a fixed timestep: how far the frame is from the last update toward the
    // next, 0 to just under 1. Draw at previous + (current - previous) * Alpha() for motion
    // smoother than the update rate (GameSettings::drawBetweenUpdates). 0 in Update.
    constexpr float Alpha() const { return alpha_; }

private:
    float delta_ = 0.0f;
    double total_ = 0.0;
    bool runningSlowly_ = false;
    float alpha_ = 0.0f;
};

} // namespace coldx
