// A small game that uses most of coldx: catch stars and gems, dodge meteors.
//
// Move with the arrows or WASD, a gamepad stick, or hold the left mouse button. R restarts
// after a game over, M toggles the music, F11 fullscreen, Escape quits. The best score is
// saved with Storage.
//
// Pixel-perfect, the way a pixel-art game on coldx should be: Settings().pixelArt and a
// fixed 640x360 back buffer give integer scaling into the window (2x at 1280x720, 4x at
// 2560x1440), point sampling and pixel snapping in every SpriteBatch. On top: sprites at
// 1x and never rotated, a pixel font drawn sharp, and screen shake by whole pixels.
//
// Shows: components (a starfield of hundreds of pixel stars, the falling objects, the HUD),
// ContentManager (sheet, sounds, music, font by name, all from content.rime), additive
// blending for the stars, a SpriteBatch transform for screen shake, keyboard, mouse and
// gamepad input, sound effects with pitch, streaming music, a resizable window.

#include <coldx/coldx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "sheet.h"

using namespace coldx;

namespace {

constexpr int Width = 640;
constexpr int Height = 360;

std::mt19937 random(12345);

float Uniform(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(random);
}

// The parts of the game the components share.
struct World {
    Texture2D sheet;
    Texture2D pixel;  // 1x1 white, for the stars
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

    // Shake moves everything by whole pixels: a fractional camera would undo the snapping.
    Matrix ShakeTransform() const {
        if (shake <= 0.0f) return Matrix::Identity();
        const float strength = 6.0f * shake;
        return Matrix::CreateTranslation(std::round(Uniform(-strength, strength)), std::round(Uniform(-strength, strength)));
    }
};

// A pixel starfield: each star a 1x1 or 2x2 dot, brighter and faster nearer.
class Starfield : public DrawableGameComponent {
public:
    Starfield(Game& game, World& world) : DrawableGameComponent(game), world_(world) { SetDrawOrder(0); }

    void Initialize() override {
        stars_.resize(400);
        for (Star& s : stars_) Respawn(s, Uniform(0.0f, Height));
    }

    void Update(const GameTime& time) override {
        for (Star& s : stars_) {
            s.y += s.speed * time.Delta();
            if (s.y > Height) Respawn(s, -2.0f);
        }
    }

    void Draw(const GameTime&) override {
        batch_.Begin(SpriteSortMode::Deferred, BlendState::Additive);
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
        spawnIn_ -= dt;
        if (spawnIn_ <= 0.0f) {
            spawnIn_ = std::max(0.15f, 0.8f - world_.score * 0.01f);
            const float roll = Uniform(0.0f, 1.0f);
            const Kind kind = roll < 0.25f ? Kind::Meteor : (roll < 0.35f ? Kind::Gem : Kind::Star);
            items_.push_back({kind, Vector2(Uniform(16.0f, Width - 16.0f), -16.0f), Uniform(75.0f, 125.0f) + world_.score * 1.5f});
        }
        for (Item& it : items_) it.position.y += it.speed * dt;
        // Catch or crash: anything within 16 px of the ship.
        for (Item& it : items_) {
            const Vector2 d = it.position - world_.player;
            if (d.x * d.x + d.y * d.y > 16.0f * 16.0f) continue;
            it.position.y = Height + 100.0f;  // gone
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
        std::erase_if(items_, [](const Item& it) { return it.position.y > Height + 20.0f; });
    }

    void Draw(const GameTime&) override {
        // Sprites at 1x, origin at the tile's centre: the snapping puts each on whole pixels.
        batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::Default, Effect(),
                     world_.ShakeTransform());
        for (const Item& it : items_) {
            const char* tile = it.kind == Kind::Meteor ? "meteor" : (it.kind == Kind::Gem ? "gem" : "star");
            batch_.Draw(world_.sheet, it.position, sheet::Get(tile), Color::White, 0.0f, Vector2(16, 16), 1.0f,
                        SpriteEffects::None, 0.0f);
        }
        if (!world_.over)
            batch_.Draw(world_.sheet, world_.player, sheet::Get("ship"), Color::White, 0.0f, Vector2(16, 16), 1.0f,
                        SpriteEffects::None, 0.0f);
        batch_.End();
    }

private:
    enum class Kind { Star, Gem, Meteor };
    struct Item {
        Kind kind;
        Vector2 position;
        float speed;
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
        char line[128];
        batch_.Begin();
        std::snprintf(line, sizeof(line), "Score %d   Best %d", world_.score, world_.best);
        batch_.DrawString(world_.font, line, Vector2(8, 6), Color::White);
        for (int i = 0; i < world_.lives; ++i)
            batch_.Draw(world_.sheet, Vector2(Width - 36.0f - i * 30.0f, 2), sheet::Get("heart"), Color::White);
        std::snprintf(line, sizeof(line), "%.0f fps  arrows/WASD/stick/mouse  M music  F11 fullscreen", fps_);
        batch_.DrawString(world_.font, line, Vector2(8, Height - 18), Color::White * 0.6f);
        if (world_.over) {
            const char* over = "Game over - R to play again";
            const Vector2 size = world_.big.MeasureString(over);
            batch_.DrawString(world_.big, over, Vector2((Width - size.x) / 2, (Height - size.y) / 2), Color(255, 210, 120));
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
        // The game is 640x360; the window shows it at whole multiples, bars filling the rest.
        Settings().pixelArt = true;
        Settings().virtualWidth = Width;
        Settings().virtualHeight = Height;
        Components().Emplace<Starfield>(*this, world_);
        fallers_ = &Components().Emplace<Fallers>(*this, world_);
        Components().Emplace<Hud>(*this, world_);
    }

protected:
    void LoadContent() override {
        world_.sheet = Content().Load<Texture2D>("sheet");
        const Color white = Color::White;
        world_.pixel = Texture2D::FromPixels(Graphics(), 1, 1, &white);
        // Silkscreen is drawn on a grid of 125 font units, 8 to the em; 10.24 px (ascender
        // to descender) puts one of its pixels on one screen pixel, 20.48 on two.
        world_.font = Content().LoadFont("silkscreen/Silkscreen-Regular", 10.24f, FontSmoothing::Sharp);
        world_.big = Content().LoadFont("silkscreen/Silkscreen-Regular", 20.48f, FontSmoothing::Sharp);
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

private:
    void Restart() {
        world_.score = 0;
        world_.lives = 3;
        world_.over = false;
        world_.player = Vector2(Width / 2.0f, Height - 30.0f);
        fallers_->Reset();
    }

    void MovePlayer(float dt) {
        Vector2 move;
        const KeyboardState keys = Keyboard::GetState();
        if (keys.IsKeyDown(Key::Left) || keys.IsKeyDown(Key::A)) move.x -= 1;
        if (keys.IsKeyDown(Key::Right) || keys.IsKeyDown(Key::D)) move.x += 1;
        move.x += GamePad::GetState(0).leftStick.x;
        world_.player.x += move.x * 210.0f * dt;
        // The mouse is in the game's 640x360 pixels, whatever the window's size.
        const MouseState mouse = Mouse::GetState();
        if (mouse.left) world_.player.x += (mouse.x - world_.player.x) * std::min(1.0f, 12.0f * dt);
        world_.player.x = std::clamp(world_.player.x, 16.0f, Width - 16.0f);
    }

    World world_;
    Fallers* fallers_ = nullptr;
};

int main() {
    Demo game;
    return game.Run();
}
