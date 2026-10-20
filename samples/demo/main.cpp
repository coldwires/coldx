// A small game that uses most of coldx: catch stars and gems, dodge meteors.
//
// Move with the arrows or WASD, a gamepad stick, or hold the left mouse button. R restarts
// after a game over, M toggles the music, F11 fullscreen, Escape quits. The best score is
// saved with Storage.
//
// Shows: components (a starfield of thousands of sprites, the falling objects, the HUD),
// ContentManager (sheet, sounds, music, font by name), additive blending for sparks, a
// SpriteBatch transform for screen shake, keyboard, mouse and gamepad input, sound effects
// with pitch, streaming music, a resizable window.

#include <coldx/coldx.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "sheet.h"

using namespace coldx;

namespace {

std::mt19937 random(12345);

float Uniform(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(random);
}

// The parts of the game the components share.
struct World {
    Texture2D sheet;
    SpriteFont font;
    SpriteFont big;
    SoundEffect pickup;
    SoundEffect hit;
    Vector2 player;
    int score = 0;
    int best = 0;
    int lives = 3;
    float shake = 0.0f;  // seconds of screen shake left
    bool over = false;

    Matrix ShakeTransform() const {
        if (shake <= 0.0f) return Matrix::Identity();
        const float strength = 8.0f * shake;
        return Matrix::CreateTranslation(Uniform(-strength, strength), Uniform(-strength, strength));
    }
};

// Thousands of drifting background stars: many sprites, one texture, one GPU draw.
class Starfield : public DrawableGameComponent {
public:
    Starfield(Game& game, World& world) : DrawableGameComponent(game), world_(world) { SetDrawOrder(0); }

    void Initialize() override {
        stars_.resize(3000);
        for (Star& s : stars_) Respawn(s, Uniform(0.0f, 1.0f));
    }

    void Update(const GameTime& time) override {
        const float h = static_cast<float>(Graphics().BackBufferHeight());
        for (Star& s : stars_) {
            s.y += s.speed * time.Delta() / h;
            if (s.y > 1.0f) Respawn(s, 0.0f);
        }
    }

    void Draw(const GameTime&) override {
        const float w = static_cast<float>(Graphics().BackBufferWidth());
        const float h = static_cast<float>(Graphics().BackBufferHeight());
        const Rectangle spark = sheet::Get("spark");
        batch_.Begin(SpriteSortMode::Deferred, BlendState::Additive, SamplerState::LinearClamp);
        for (const Star& s : stars_) {
            const Color tint = Color::White * s.brightness;
            batch_.Draw(world_.sheet, Vector2(s.x * w, s.y * h), spark, tint, 0.0f, Vector2(16, 16), s.size,
                        SpriteEffects::None, 0.0f);
        }
        batch_.End();
    }

private:
    struct Star {
        float x, y, speed, size, brightness;
    };

    void Respawn(Star& s, float y) {
        s.x = Uniform(0.0f, 1.0f);
        s.y = y;
        s.speed = Uniform(10.0f, 80.0f);
        s.size = s.speed / 300.0f;
        s.brightness = s.speed / 100.0f;
    }

    World& world_;
    std::vector<Star> stars_;
    SpriteBatch batch_{Graphics()};
};

// Stars and gems to catch, meteors to dodge, falling faster as the score rises; and the
// ship, drawn with them so the screen shake moves both.
class Fallers : public DrawableGameComponent {
public:
    Fallers(Game& game, World& world) : DrawableGameComponent(game), world_(world) { SetDrawOrder(1); }

    void Reset() { items_.clear(); }

    void Update(const GameTime& time) override {
        if (world_.over) return;
        const float dt = time.Delta();
        const float w = static_cast<float>(Graphics().BackBufferWidth());
        const float h = static_cast<float>(Graphics().BackBufferHeight());
        spawnIn_ -= dt;
        if (spawnIn_ <= 0.0f) {
            spawnIn_ = std::max(0.15f, 0.8f - world_.score * 0.01f);
            const float roll = Uniform(0.0f, 1.0f);
            const Kind kind = roll < 0.25f ? Kind::Meteor : (roll < 0.35f ? Kind::Gem : Kind::Star);
            items_.push_back({kind, Vector2(Uniform(20.0f, w - 20.0f), -20.0f), Uniform(150.0f, 250.0f) + world_.score * 3.0f,
                              0.0f, Uniform(-3.0f, 3.0f)});
        }
        for (Item& it : items_) {
            it.position.y += it.speed * dt;
            it.angle += it.spin * dt;
        }
        // Catch or crash: anything within 24 px of the ship.
        for (Item& it : items_) {
            const Vector2 d = it.position - world_.player;
            if (d.x * d.x + d.y * d.y > 24.0f * 24.0f) continue;
            it.position.y = h + 100.0f;  // gone
            if (it.kind == Kind::Meteor) {
                world_.hit.Play(0.9f);
                world_.shake = 0.4f;
                if (--world_.lives <= 0) {
                    world_.over = true;
                    Log("demo: game over, score %d", world_.score);
                }
            } else {
                const int points = it.kind == Kind::Gem ? 5 : 1;
                world_.score += points;
                // Higher pitch for a gem, and a little higher as the score climbs.
                world_.pickup.Play(0.6f, std::min(1.0f, (points == 5 ? 0.5f : 0.0f) + world_.score * 0.005f));
            }
        }
        std::erase_if(items_, [h](const Item& it) { return it.position.y > h + 40.0f; });
    }

    void Draw(const GameTime&) override {
        batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp, world_.ShakeTransform());
        for (const Item& it : items_) {
            const char* tile = it.kind == Kind::Meteor ? "meteor" : (it.kind == Kind::Gem ? "gem" : "star");
            batch_.Draw(world_.sheet, it.position, sheet::Get(tile), Color::White, it.angle, Vector2(16, 16), 1.5f,
                        SpriteEffects::None, 0.0f);
        }
        if (!world_.over)
            batch_.Draw(world_.sheet, world_.player, sheet::Get("ship"), Color::White, 0.0f, Vector2(16, 16), 1.5f,
                        SpriteEffects::None, 0.0f);
        batch_.End();
    }

private:
    enum class Kind { Star, Gem, Meteor };
    struct Item {
        Kind kind;
        Vector2 position;
        float speed, angle, spin;
    };

    World& world_;
    std::vector<Item> items_;
    float spawnIn_ = 0.5f;
    SpriteBatch batch_{Graphics()};
};

// Score, lives and help, drawn on top of everything.
class Hud : public DrawableGameComponent {
public:
    Hud(Game& game, World& world) : DrawableGameComponent(game), world_(world) { SetDrawOrder(10); }

    void Update(const GameTime& time) override {
        ++frames_;
        elapsed_ += time.Delta();
        if (elapsed_ >= 0.5) {
            fps_ = frames_ / elapsed_;
            frames_ = 0;
            elapsed_ = 0.0;
        }
    }

    void Draw(const GameTime&) override {
        const float w = static_cast<float>(Graphics().BackBufferWidth());
        const float h = static_cast<float>(Graphics().BackBufferHeight());
        char line[128];
        batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        std::snprintf(line, sizeof(line), "Score %d   Best %d", world_.score, world_.best);
        batch_.DrawString(world_.font, line, Vector2(20, 16), Color::White);
        for (int i = 0; i < world_.lives; ++i)
            batch_.Draw(world_.sheet, Vector2(w - 44.0f - i * 36.0f, 12), sheet::Get("heart"), Color::White);
        std::snprintf(line, sizeof(line), "%.0f fps   arrows/WASD/stick/mouse   M music   F11 fullscreen", fps_);
        batch_.DrawString(world_.font, line, Vector2(20, h - 36), Color::White * 0.6f);
        if (world_.over) {
            const char* over = "Game over - R to play again";
            const Vector2 size = world_.big.MeasureString(over);
            batch_.DrawString(world_.big, over, Vector2((w - size.x) / 2, (h - size.y) / 2), Color(255, 210, 120));
        }
        batch_.End();
    }

private:
    World& world_;
    SpriteBatch batch_{Graphics()};
    int frames_ = 0;
    double elapsed_ = 0.0;
    double fps_ = 0.0;
};

} // namespace

class Demo : public Game {
public:
    Demo() {
        Settings().title = "coldx demo";
        Settings().company = "coldx";
        Settings().resizable = true;
        Components().Emplace<Starfield>(*this, world_);
        fallers_ = &Components().Emplace<Fallers>(*this, world_);
        Components().Emplace<Hud>(*this, world_);
    }

protected:
    void LoadContent() override {
        world_.sheet = Content().Load<Texture2D>("sheet");
        world_.font = Content().LoadFont("LiberationSans-Regular", 18.0f);
        world_.big = Content().LoadFont("LiberationSans-Regular", 40.0f);
        world_.pickup = Content().Load<SoundEffect>("pickup");
        world_.hit = Content().Load<SoundEffect>("hit");
        if (auto saved = Storage::ReadFile("best.txt")) world_.best = std::atoi(saved->c_str());
        MediaPlayer::SetVolume(0.5f);
        MediaPlayer::Play(Content().Load<Music>("loop"));
        Restart();
    }

    void Update(const GameTime& time) override {
        if (Keyboard::WasPressed(Key::Escape)) Exit();
        if (Keyboard::WasPressed(Key::F11)) ToggleFullscreen();
        if (Keyboard::WasPressed(Key::M)) {
            if (MediaPlayer::IsPlaying())
                MediaPlayer::Stop();
            else
                MediaPlayer::Play(Content().Load<Music>("loop"));
        }
        world_.shake = std::max(0.0f, world_.shake - time.Delta());
        if (world_.over) {
            if (world_.score > world_.best) {
                world_.best = world_.score;
                Storage::WriteFile("best.txt", std::to_string(world_.best));
            }
            if (Keyboard::WasPressed(Key::R) || GamePad::WasPressed(0, Buttons::Start)) Restart();
            return;
        }
        MovePlayer(time.Delta());
    }

    void Draw(const GameTime&) override {
        // Everything else is drawn by the components, after this, in draw order: the
        // starfield, then the falling objects and ship, then the HUD.
        Graphics().Clear(Color(8, 10, 20));
    }

    void OnResize(int width, int height) override {
        world_.player.x = std::min(world_.player.x, static_cast<float>(width) - 20.0f);
        world_.player.y = static_cast<float>(height) - 60.0f;
    }

private:
    void Restart() {
        world_.score = 0;
        world_.lives = 3;
        world_.over = false;
        world_.player = Vector2(Graphics().BackBufferWidth() / 2.0f, Graphics().BackBufferHeight() - 60.0f);
        fallers_->Reset();
    }

    void MovePlayer(float dt) {
        Vector2 move;
        const KeyboardState keys = Keyboard::GetState();
        if (keys.IsKeyDown(Key::Left) || keys.IsKeyDown(Key::A)) move.x -= 1;
        if (keys.IsKeyDown(Key::Right) || keys.IsKeyDown(Key::D)) move.x += 1;
        move.x += GamePad::GetState(0).leftStick.x;
        world_.player.x += move.x * 420.0f * dt;
        const MouseState mouse = Mouse::GetState();
        if (mouse.left) world_.player.x += (mouse.x - world_.player.x) * std::min(1.0f, 12.0f * dt);
        world_.player.x = std::clamp(world_.player.x, 20.0f, Graphics().BackBufferWidth() - 20.0f);
    }

    World world_;
    Fallers* fallers_ = nullptr;
};

int main() {
    Demo game;
    return game.Run();
}
