// Text with SpriteFont. Up and Down, or the mouse wheel, change the size; each size is a
// fresh bake, so text stays sharp at every size instead of being scaled. The size is saved
// with Storage and comes back next run.

#include <coldx/coldx.h>

#include <cstdio>
#include <cstdlib>

using namespace coldx;

namespace {

constexpr int MinSize = 8;
constexpr int MaxSize = 96;
constexpr const char* SettingsFile = "settings.txt";

} // namespace

class HelloText : public Game {
public:
    HelloText() {
        Settings().title = "hello_text";
        Settings().company = "coldx";
    }

protected:
    void LoadContent() override {
        if (auto saved = Storage::ReadFile(SettingsFile)) {
            const int value = std::atoi(saved->c_str());
            if (value >= MinSize && value <= MaxSize) size = value;
        }
        Bake();
        // A 1x1 white texture, tinted and stretched for the measurement box.
        const Color white = Color::White;
        pixel = Texture2D::FromPixels(Graphics(), 1, 1, &white);
    }

    void Update(const GameTime& time) override {
        if (Keyboard::GetState().IsKeyDown(Key::Escape)) Exit();
        int change = 0;
        if (Keyboard::WasPressed(Key::Up)) change += 2;
        if (Keyboard::WasPressed(Key::Down)) change -= 2;
        const float wheel = Mouse::ScrollWheelDelta();
        if (wheel > 0) change += 2;
        if (wheel < 0) change -= 2;
        if (change != 0) Resize(size + change);
        angle += time.Delta() * 0.5f;
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color(24, 26, 32));
        const char* text = "The quick brown fox jumps over the lazy dog.\n"
                           "Line two. Latin-1: caf\xc3\xa9, na\xc3\xafve, \xc2\xa3" "5, \xc2\xbd off.\n"
                           "Kerning: AV To Ty Wa";
        const Vector2 at(40, 40);
        const Vector2 measured = font.MeasureString(text);

        char label[80];
        std::snprintf(label, sizeof(label), "%d px  (Up / Down or the wheel to change; it is saved)", size);

        batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        batch.Draw(pixel, Rectangle(static_cast<int>(at.x), static_cast<int>(at.y), static_cast<int>(measured.x),
                                    static_cast<int>(measured.y)),
                   Color(60, 70, 90));
        batch.DrawString(font, text, at, Color::White);
        batch.DrawString(font, label, Vector2(40, at.y + measured.y + 20), Color(255, 210, 120));
        batch.End();

        // Rotation and scale need smoothing; point sampling would make them jagged.
        batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::LinearClamp);
        const char* spinning = "turning about its center";
        const Vector2 spinSize = font.MeasureString(spinning);
        batch.DrawString(font, spinning, Vector2(640, 480), Color(140, 220, 255), angle, spinSize * 0.5f, 1.0f,
                         SpriteEffects::None, 0.0f);
        batch.End();
    }

private:
    void Resize(int newSize) {
        newSize = newSize < MinSize ? MinSize : (newSize > MaxSize ? MaxSize : newSize);
        if (newSize == size) return;
        size = newSize;
        Bake();
        Storage::WriteFile(SettingsFile, std::to_string(size));
    }

    void Bake() {
        font = SpriteFont::FromFile(Graphics(), "content/LiberationSans-Regular.ttf", static_cast<float>(size));
    }

    SpriteBatch batch{Graphics()};
    SpriteFont font;
    Texture2D pixel;
    int size = 18;
    float angle = 0.0f;
};

int main() {
    HelloText game;
    return game.Run();
}
