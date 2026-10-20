// Text with SpriteFont. Up and Down change the size; each size is a fresh bake, so text
// stays sharp at every size instead of being scaled.

#include <coldx/coldx.h>

#include <cstdio>

using namespace coldx;

class HelloText : public Game {
public:
    HelloText() {
        Settings().title = "hello_text";
    }

protected:
    void LoadContent() override {
        Bake();
        // A 1x1 white texture, tinted and stretched for the measurement box.
        const Color white = Color::White;
        pixel = Texture2D::FromPixels(Graphics(), 1, 1, &white);
    }

    void Update(const GameTime& time) override {
        const KeyboardState keys = Keyboard::GetState();
        if (keys.IsKeyDown(Key::Escape)) Exit();
        if (Pressed(keys, Key::Up) && size < 96) {
            size += 2;
            Bake();
        }
        if (Pressed(keys, Key::Down) && size > 8) {
            size -= 2;
            Bake();
        }
        previous = keys;
        angle += time.Delta() * 0.5f;
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color(24, 26, 32));
        const char* text = "The quick brown fox jumps over the lazy dog.\n"
                           "Line two. Latin-1: caf\xc3\xa9, na\xc3\xafve, \xc2\xa3" "5, \xc2\xbd off.\n"
                           "Kerning: AV To Ty Wa";
        const Vector2 at(40, 40);
        const Vector2 measured = font.MeasureString(text);

        char label[64];
        std::snprintf(label, sizeof(label), "%d px  (Up / Down to change)", size);

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
    void Bake() {
        font = SpriteFont::FromFile(Graphics(), "content/LiberationSans-Regular.ttf", static_cast<float>(size));
    }

    bool Pressed(const KeyboardState& keys, Key key) const {
        return keys.IsKeyDown(key) && previous.IsKeyUp(key);
    }

    SpriteBatch batch{Graphics()};
    SpriteFont font;
    Texture2D pixel;
    KeyboardState previous;
    int size = 18;
    float angle = 0.0f;
};

int main() {
    HelloText game;
    return game.Run();
}
