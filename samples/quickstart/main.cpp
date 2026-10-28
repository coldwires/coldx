#include <coldx/coldx.h>

using namespace coldx;

class MyGame : public Game {
public:
    MyGame() {
        Settings().title = "quickstart";
        Settings().company = "my studio";  // names the Storage folder
    }

protected:
    void LoadContent() override {
        // Content loads by name from the content folder beside the executable:
        // Content().Load<Texture2D>("player") reads content/player.png. Fonts load per size.
        font = Content().LoadFont("LiberationSans-Regular", 18.0f);
        // Textures can also be made in code. Colors are premultiplied.
        const Color white = Color::White;
        square = Texture2D::FromPixels(Graphics(), 1, 1, &white);
    }

    void Update(const GameTime& time) override {
        if (Keyboard::WasPressed(Key::Escape)) Exit();

        Vector2 move;
        const KeyboardState keys = Keyboard::GetState();
        if (keys.IsKeyDown(Key::Left)) move.x -= 1;
        if (keys.IsKeyDown(Key::Right)) move.x += 1;
        if (keys.IsKeyDown(Key::Up)) move.y -= 1;
        if (keys.IsKeyDown(Key::Down)) move.y += 1;
        // Sticks are up-positive. Screen y grows downward.
        const Vector2 stick = GamePad::GetState(0).leftStick;
        move += Vector2(stick.x, -stick.y);
        position += move * 300.0f * time.Delta();

        if (Mouse::WasPressed(MouseButton::Left)) {
            const MouseState mouse = Mouse::GetState();
            position = Vector2(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
        }
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color::CornflowerBlue);
        batch.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::PointClamp);
        batch.Draw(square, Rectangle(static_cast<int>(position.x), static_cast<int>(position.y), 32, 32),
                   Color::Yellow);
        batch.DrawString(font, "Arrows or a stick move it. Click to place it.", Vector2(20, 20), Color::White);
        batch.End();
    }

private:
    SpriteBatch batch{Graphics()};  // fine before Run, since GPU objects are made on first use
    Texture2D square;
    SpriteFont font;
    Vector2 position{100, 100};
};

int main() {
    MyGame game;
    return game.Run();
}
