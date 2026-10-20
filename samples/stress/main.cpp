// Many moving, rotating sprites with vsync off, logging frames per second. The sprite count
// is the first argument; 10,000 by default.

#include <coldx/coldx.h>

#include <cstdlib>
#include <random>
#include <vector>

using namespace coldx;

namespace {

struct Mover {
    Vector2 position;
    Vector2 velocity;
    float rotation;
    float spin;
    Color tint;
};

constexpr int SpriteSize = 16;

} // namespace

class Stress : public Game {
public:
    explicit Stress(int count) : count_(count) {
        Settings().title = "stress";
        Settings().vsync = false;
        Settings().fixedTimeStep = false;
    }

protected:
    void LoadContent() override {
        // A white ring with a soft edge, so overlap and rotation are visible.
        std::vector<Color> pixels(SpriteSize * SpriteSize);
        for (int y = 0; y < SpriteSize; ++y) {
            for (int x = 0; x < SpriteSize; ++x) {
                const float dx = x + 0.5f - SpriteSize / 2.0f;
                const float dy = y + 0.5f - SpriteSize / 2.0f;
                const float d = dx * dx + dy * dy;
                const bool ring = d < 64.0f && d > 16.0f;
                pixels[y * SpriteSize + x] = ring ? Color::White : (x == SpriteSize / 2 ? Color::White * 0.5f : Color::Transparent);
            }
        }
        texture_ = Texture2D::FromPixels(Graphics(), SpriteSize, SpriteSize, pixels.data());

        std::mt19937 random(1234);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const float w = static_cast<float>(Settings().width);
        const float h = static_cast<float>(Settings().height);
        movers_.resize(static_cast<size_t>(count_));
        for (Mover& m : movers_) {
            m.position = Vector2(unit(random) * w, unit(random) * h);
            m.velocity = Vector2(unit(random) - 0.5f, unit(random) - 0.5f) * 400.0f;
            m.rotation = unit(random) * MathHelper::TwoPi;
            m.spin = (unit(random) - 0.5f) * 6.0f;
            m.tint = Color(static_cast<std::uint8_t>(80 + unit(random) * 175), static_cast<std::uint8_t>(80 + unit(random) * 175),
                           static_cast<std::uint8_t>(80 + unit(random) * 175));
        }
        Log("stress: %d sprites", count_);
    }

    void Update(const GameTime& time) override {
        if (Keyboard::GetState().IsKeyDown(Key::Escape)) Exit();
        const float dt = time.Delta();
        const float w = static_cast<float>(Graphics().BackBufferWidth());
        const float h = static_cast<float>(Graphics().BackBufferHeight());
        for (Mover& m : movers_) {
            m.position += m.velocity * dt;
            m.rotation += m.spin * dt;
            if (m.position.x < 0 || m.position.x > w) m.velocity.x = -m.velocity.x;
            if (m.position.y < 0 || m.position.y > h) m.velocity.y = -m.velocity.y;
        }

        frames_++;
        elapsed_ += time.Delta();
        if (elapsed_ >= 1.0) {
            Log("stress: %.1f fps, %d sprites", frames_ / elapsed_, count_);
            frames_ = 0;
            elapsed_ = 0.0;
        }
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color(16, 16, 24));
        const Vector2 origin(SpriteSize / 2.0f, SpriteSize / 2.0f);
        batch_.Begin();
        for (const Mover& m : movers_)
            batch_.Draw(texture_, m.position, std::nullopt, m.tint, m.rotation, origin, 1.0f, SpriteEffects::None, 0.0f);
        batch_.End();
    }

private:
    int count_;
    SpriteBatch batch_{Graphics()};
    Texture2D texture_;
    std::vector<Mover> movers_;
    int frames_ = 0;
    double elapsed_ = 0.0;
};

int main(int argc, char** argv) {
    const int count = argc > 1 ? std::atoi(argv[1]) : 10000;
    Stress game(count > 0 ? count : 10000);
    return game.Run();
}
