#pragma once

// The playfield. Everything here draws into World::field, a render target, which the
// Compositor then shows on the back buffer: through the grayscale effect while paused
// or after a game over.

#include "world.h"

namespace demo {

// A tiled nebula scrolling slowly (PointWrap past the texture's edges), under a pixel
// starfield: each star a 1x1 or 2x2 dot, brighter and faster nearer (additive).
class Background : public DrawableGameComponent {
public:
    Background(Game& game, World& world) : DrawableGameComponent(game), world_(world) { SetDrawOrder(0); }

    void Initialize() override {
        stars_.resize(400);
        for (Star& s : stars_) Respawn(s, Uniform(0.0f, Height));
    }

    void Update(const GameTime& time) override {
        if (world_.screen == Screen::Paused) return;
        scroll_ += 6.0f * time.Delta();
        for (Star& s : stars_) {
            s.y += s.speed * time.Delta();
            if (s.y > Height) Respawn(s, -2.0f);
        }
    }

    void Draw(const GameTime&) override {
        // A source rectangle larger than the texture, offset by the scroll: PointWrap tiles it.
        batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointWrap);
        batch_.Draw(world_.nebula, Vector2(0, 0), Rectangle(0, -static_cast<int>(scroll_), Width, Height), Color::White);
        batch_.End();
        // One texture for every star, so Texture sorting changes nothing. It is the
        // cheapest mode when a batch mixes many.
        batch_.Begin(SpriteSortMode::Texture, BlendState::Additive);
        for (const Star& s : stars_)
            batch_.Draw(world_.pixel, Rectangle(static_cast<int>(s.x), static_cast<int>(std::floor(s.y)), s.size, s.size),
                        Color::White * s.brightness);
        batch_.End();
    }

private:
    struct Star {
        float x, y, speed, brightness;
        int size;
    };

    void Respawn(Star& s, float y) {
        s.x = std::floor(Uniform(0.0f, Width));
        s.y = y;
        s.speed = Uniform(5.0f, 40.0f);
        s.size = s.speed > 30.0f ? 2 : 1;
        s.brightness = 0.2f + s.speed / 50.0f;
    }

    World& world_;
    std::vector<Star> stars_;
    float scroll_ = 0.0f;
    SpriteBatch batch_{Graphics()};
};

// Stars and gems to catch, meteors to dodge, the ship, and the sparks of a catch.
// Positions are interpolated between updates (GameTime::Alpha), so motion is smooth on a
// display faster than the 60 Hz update.
class Fallers : public DrawableGameComponent {
public:
    Fallers(Game& game, World& world) : DrawableGameComponent(game), world_(world) { SetDrawOrder(1); }

    void Reset() {
        items_.clear();
        sparks_.clear();
        spawnIn_ = 0.5f;
    }

    // Ends the run at once: the F9 debug key, for trying the game-over screens.
    void Crash() {
        world_.lives = 1;
        Hit();
    }

    void Update(const GameTime& time) override {
        if (!world_.Playing()) return;
        const float dt = time.Delta();
        const Wave wave = world_.waves.At(world_.score);
        spawnIn_ -= dt;
        if (spawnIn_ <= 0.0f) {
            spawnIn_ = wave.spawnSeconds;
            const float roll = Uniform(0.0f, 1.0f);
            const Kind kind = roll < wave.meteorChance ? Kind::Meteor : (roll < wave.meteorChance + wave.gemChance ? Kind::Gem : Kind::Star);
            const Vector2 at(Uniform(16.0f, Width - 16.0f), -16.0f);
            items_.push_back({kind, at, at, wave.speed * Uniform(0.8f, 1.25f), Uniform(0.0f, 1.0f) < 0.5f});
        }
        for (Item& it : items_) {
            it.previous = it.position;
            it.position.y += it.speed * dt;
        }
        for (Spark& s : sparks_) {
            s.previous = s.position;
            s.position += s.velocity * dt;
            s.velocity.y += 120.0f * dt;
            s.life -= dt;
        }
        std::erase_if(sparks_, [](const Spark& s) { return s.life <= 0.0f; });

        const Point ship = TopLeft(world_.player);
        for (Item& it : items_) {
            if (it.kind == Kind::Meteor) {
                // Pixel-exact: only where the ship's and the meteor's opaque pixels meet.
                if (!world_.shipMask.Overlaps(ship, world_.meteorMask, TopLeft(it.position))) continue;
                it.position.y = Height + 100.0f;  // gone
                Hit();
                if (!world_.Playing()) return;
                continue;
            }
            const Vector2 d = it.position - world_.player;
            if (d.x * d.x + d.y * d.y > 16.0f * 16.0f) continue;
            it.position.y = Height + 100.0f;
            const int points = it.kind == Kind::Gem ? 5 : 1;
            world_.score += points;
            // Higher for a gem and as the score climbs, panned to where it was caught.
            const float pan = (world_.player.x / Width) * 2.0f - 1.0f;
            world_.pickup.Play(0.6f, std::min(1.0f, (points == 5 ? 0.5f : 0.0f) + world_.score * 0.005f), pan * 0.7f);
            for (int i = 0; i < (points == 5 ? 14 : 6); ++i) {
                const float angle = Uniform(0.0f, MathHelper::TwoPi);
                const Vector2 v(std::cos(angle) * Uniform(30.0f, 90.0f), std::sin(angle) * Uniform(30.0f, 90.0f) - 40.0f);
                sparks_.push_back({world_.player, world_.player, v, 0.6f});
            }
        }
        std::erase_if(items_, [](const Item& it) { return it.position.y > Height + 20.0f; });
    }

    void Draw(const GameTime& time) override {
        if (world_.screen == Screen::Title || world_.screen == Screen::Credits) return;
        const float a = world_.Playing() ? time.Alpha() : 1.0f;
        auto at = [a](Vector2 previous, Vector2 current) { return previous + (current - previous) * a; };
        const Matrix shake = world_.ShakeTransform();

        // Back to front by layer depth: meteors (0.6) under stars (0.4) under gems (0.2),
        // whatever order they were spawned in. Every other one is drawn mirrored.
        batch_.Begin(SpriteSortMode::BackToFront, BlendState::AlphaBlend, SamplerState::Default, Effect(), shake);
        for (const Item& it : items_) {
            const char* tile = it.kind == Kind::Meteor ? "meteor" : (it.kind == Kind::Gem ? "gem" : "star");
            const float depth = it.kind == Kind::Meteor ? 0.6f : (it.kind == Kind::Gem ? 0.2f : 0.4f);
            batch_.Draw(world_.sheet, at(it.previous, it.position), sheet::Get(tile), Color::White, 0.0f, Vector2(16, 16), 1.0f,
                        it.mirrored ? SpriteEffects::FlipHorizontally : SpriteEffects::None, depth);
        }
        batch_.End();

        // The ship alone, through the flash effect: white for a moment after a hit.
        if (world_.lives > 0 && (world_.Playing() || world_.screen == Screen::Paused)) {
            world_.flash.Set("Color", Color::White);
            world_.flash.Set("Amount", MathHelper::Clamp(world_.flashLeft / 0.3f, 0.0f, 1.0f));
            batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::Default, world_.flash, shake);
            batch_.Draw(world_.sheet, at(world_.previousPlayer, world_.player), sheet::Get("ship"), Color::White, 0.0f,
                        Vector2(16, 16), 1.0f, SpriteEffects::None, 0.0f);
            batch_.End();
        }

        batch_.Begin(SpriteSortMode::Deferred, BlendState::Additive, SamplerState::Default, Effect(), shake);
        for (const Spark& s : sparks_)
            batch_.Draw(world_.sheet, at(s.previous, s.position), sheet::Get("spark"), Color(255, 220, 140) * (s.life / 0.6f), 0.0f,
                        Vector2(16, 16), 0.5f, SpriteEffects::None, 0.0f);
        batch_.End();
    }

    // Where each faller is, for the radar.
    template <class F>
    void ForEach(F&& f) const {
        for (const Item& it : items_) f(it.position, it.kind == Kind::Meteor);
    }

private:
    enum class Kind { Star, Gem, Meteor };
    struct Item {
        Kind kind;
        Vector2 previous, position;
        float speed;
        bool mirrored;
    };
    struct Spark {
        Vector2 previous, position, velocity;
        float life;
    };

    static Point TopLeft(Vector2 centre) {
        return Point(static_cast<int>(std::round(centre.x)) - 16, static_cast<int>(std::round(centre.y)) - 16);
    }

    void Hit() {
        world_.hit.Play(0.9f);
        world_.shake = 0.4f;
        world_.flashLeft = 0.3f;
        if (GamePad::SetVibration(0, 0.6f, 0.9f)) world_.rumbleLeft = 0.25f;
        if (--world_.lives <= 0) {
            Log("demo: game over, score %d", world_.score);
            world_.screen = world_.scores.Qualifies(world_.score) ? Screen::NameEntry : Screen::Scores;
            MediaPlayer::FadeOut(2.0f);
        }
    }

    World& world_;
    std::vector<Item> items_;
    std::vector<Spark> sparks_;
    float spawnIn_ = 0.5f;
    SpriteBatch batch_{Graphics()};
};

// Shows the playfield on the back buffer, drained of colour while paused or over, and
// the radar: a 64x36 texture rewritten with SetData every frame.
class Compositor : public DrawableGameComponent {
public:
    Compositor(Game& game, World& world, const Fallers& fallers)
        : DrawableGameComponent(game), world_(world), fallers_(fallers) {
        SetDrawOrder(5);
    }

    void LoadContent() override {
        radarPixels_.assign(RadarWidth * RadarHeight, Color::Transparent);
        radar_ = Texture2D::FromPixels(Graphics(), RadarWidth, RadarHeight, radarPixels_.data());
    }

    void Update(const GameTime& time) override {
        const bool drained = world_.screen == Screen::Paused || world_.screen == Screen::NameEntry || world_.screen == Screen::Scores;
        world_.grayAmount = MathHelper::Clamp(world_.grayAmount + (drained ? 2.0f : -3.0f) * time.Delta(), 0.0f, 1.0f);
    }

    void Draw(const GameTime&) override {
        Graphics().SetRenderTarget(nullptr);
        Graphics().Clear(Color::Black);
        world_.gray.Set("Amount", world_.grayAmount);
        // Opaque: the playfield covers the whole screen, so nothing under it needs blending.
        batch_.Begin(SpriteSortMode::Deferred, BlendState::Opaque, SamplerState::Default, world_.gray);
        batch_.Draw(world_.field, Vector2(0, 0), Color::White);
        batch_.End();

        if (!world_.Playing() && world_.screen != Screen::Paused) return;
        // Straight (not premultiplied) pixels, so NonPremultiplied blending.
        std::fill(radarPixels_.begin(), radarPixels_.end(), Color(20, 30, 60, 140));
        auto plot = [this](Vector2 p, Color c) {
            const int x = static_cast<int>(p.x / Width * RadarWidth), y = static_cast<int>(p.y / Height * RadarHeight);
            if (x >= 0 && x < RadarWidth && y >= 0 && y < RadarHeight) radarPixels_[static_cast<size_t>(y * RadarWidth + x)] = c;
        };
        fallers_.ForEach([&](Vector2 p, bool meteor) { plot(p, meteor ? Color(255, 80, 60, 255) : Color(255, 230, 120, 255)); });
        plot(world_.player, Color(120, 220, 255, 255));
        radar_.SetData(radarPixels_.data());
        batch_.Begin(SpriteSortMode::Deferred, BlendState::NonPremultiplied);
        batch_.Draw(radar_, Rectangle(Width - RadarWidth * 2 - 8, 26, RadarWidth * 2, RadarHeight * 2), Color::White);
        batch_.End();
    }

private:
    static constexpr int RadarWidth = 64;
    static constexpr int RadarHeight = 36;
    World& world_;
    const Fallers& fallers_;
    Texture2D radar_;
    std::vector<Color> radarPixels_;
    SpriteBatch batch_{Graphics()};
};

} // namespace demo
