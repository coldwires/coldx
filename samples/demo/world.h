#pragma once

// What the demo's components share: the loaded content, the run being played, the high
// score table, and which screen is up.

#include <coldx/coldx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "sheet.h"

namespace demo {

using namespace coldx;

constexpr int Width = 640;
constexpr int Height = 360;

inline std::mt19937& Random() {
    static std::mt19937 random(12345);
    return random;
}

inline float Uniform(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(Random());
}

enum class Screen { Title, Play, Paused, NameEntry, Scores, Credits };

// levels/waves.wave, a format of the demo's own read by a RegisterDataLoader: one line
// per wave, "score spawnSeconds speed meteorChance gemChance". The wave in force is the
// last one whose score has been reached.
struct Wave {
    int score = 0;
    float spawnSeconds = 0.8f;
    float speed = 100.0f;
    float meteorChance = 0.25f;
    float gemChance = 0.1f;
};

struct Waves {
    std::vector<Wave> list;

    static Waves Parse(const std::vector<std::uint8_t>& bytes) {
        Waves w;
        std::istringstream in(std::string(bytes.begin(), bytes.end()));
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            Wave v;
            std::istringstream fields(line);
            if (fields >> v.score >> v.spawnSeconds >> v.speed >> v.meteorChance >> v.gemChance) w.list.push_back(v);
        }
        return w;
    }

    Wave At(int score) const {
        Wave current;
        for (const Wave& v : list)
            if (score >= v.score) current = v;
        return current;
    }
};

// The five best runs, saved with Storage as "name score" lines.
struct HighScores {
    struct Entry {
        std::string name;
        int score = 0;
    };
    std::vector<Entry> entries;
    static constexpr const char* File = "scores.txt";

    void Load() {
        entries.clear();
        const auto saved = Storage::ReadFile(File);
        if (!saved) return;
        std::istringstream in(*saved);
        Entry e;
        while (in >> e.name >> e.score) entries.push_back(e);
    }

    bool Qualifies(int score) const { return score > 0 && (entries.size() < 5 || score > entries.back().score); }

    void Add(const std::string& name, int score) {
        entries.push_back({name, score});
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.score > b.score; });
        if (entries.size() > 5) entries.resize(5);
        std::string text;
        for (const Entry& e : entries) text += e.name + " " + std::to_string(e.score) + "\n";
        if (Storage::WriteFile(File, text)) Log("demo: high score saved, %s %d", name.c_str(), score);
    }

    int Best() const { return entries.empty() ? 0 : entries.front().score; }
};

// A tile's opaque pixels, read back from the sheet once, for pixel-exact collisions.
struct Mask {
    int width = 0, height = 0;
    std::vector<bool> solid;

    static Mask From(const Texture2D& sheet, const Rectangle& tile) {
        Mask m;
        std::vector<Color> pixels(static_cast<size_t>(tile.width * tile.height));
        if (!sheet.GetData(tile, pixels.data())) return m;
        m.width = tile.width;
        m.height = tile.height;
        for (const Color& c : pixels) m.solid.push_back(c.a > 64);
        return m;
    }

    // Whether this mask at a overlaps other at b (top-left corners, whole pixels).
    bool Overlaps(Point a, const Mask& other, Point b) const {
        const Rectangle ra(a.x, a.y, width, height), rb(b.x, b.y, other.width, other.height);
        if (!ra.Intersects(rb)) return false;
        const int x0 = std::max(ra.x, rb.x), x1 = std::min(ra.x + ra.width, rb.x + rb.width);
        const int y0 = std::max(ra.y, rb.y), y1 = std::min(ra.y + ra.height, rb.y + rb.height);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                if (solid[static_cast<size_t>((y - a.y) * width + (x - a.x))] &&
                    other.solid[static_cast<size_t>((y - b.y) * other.width + (x - b.x))])
                    return true;
        return false;
    }
};

struct World {
    // Content.
    Texture2D sheet;
    Texture2D pixel;   // 1x1 white
    Texture2D nebula;  // 64x64, tiled across the background with PointWrap
    RenderTarget2D field;  // the playfield, drawn into and then shown through an effect
    SpriteFont font;
    SpriteFont big;
    SoundEffect pickup;
    SoundEffect hit;
    SoundEffect hum;   // made from samples at load, not from a file
    SoundEffectInstance engine;
    Music music;
    Effect gray, flash, wave;
    Waves waves;
    Mask shipMask, meteorMask;
    HighScores scores;

    // The run.
    Screen screen = Screen::Title;
    Vector2 player, previousPlayer;
    int score = 0;
    int lives = 3;
    float shake = 0.0f;     // seconds of screen shake left
    float flashLeft = 0.0f;  // seconds of the ship's hit flash left
    float rumbleLeft = 0.0f;
    float grayAmount = 0.0f;  // eased towards 1 while paused or over
    std::string toast;        // a line shown briefly at the bottom
    float toastLeft = 0.0f;
    bool debug = false;       // F3: where the image sits in the window, and timings

    bool Playing() const { return screen == Screen::Play; }

    void Say(std::string text) {
        Log("demo: %s", text.c_str());
        toast = std::move(text);
        toastLeft = 2.5f;
    }

    // Shake moves everything by whole pixels: a fractional camera would undo the snapping.
    Matrix ShakeTransform() const {
        if (shake <= 0.0f) return Matrix::Identity();
        const float strength = 6.0f * shake;
        return Matrix::CreateTranslation(std::round(Uniform(-strength, strength)), std::round(Uniform(-strength, strength)));
    }
};

// What a pad calls its bottom face button, for prompts: "A", "Cross", or "B" on a Nintendo pad.
inline const char* ConfirmLabel() {
    switch (GamePad::FaceButtonLabel(0, Buttons::A)) {
    case ButtonLabel::Cross: return "Cross";
    case ButtonLabel::B: return "B";
    default: return "A";
    }
}

} // namespace demo
