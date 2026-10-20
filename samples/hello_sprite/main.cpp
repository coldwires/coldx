#include <coldx/coldx.h>

using namespace coldx;

class HelloSprite : public Game {
public:
    HelloSprite() {
        Settings().title = "hello_sprite";
    }

protected:
    void LoadContent() override {
        sprite = Texture2D::FromFile(Graphics(), "content/sprite.png");
    }

    void Update(const GameTime&) override {
        if (Keyboard::GetState().IsKeyDown(Key::Escape)) Exit();
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color::CornflowerBlue);
        batch.Begin();
        batch.Draw(sprite, Vector2(100, 100), Color::White);
        batch.Draw(sprite, Vector2(200, 100), Color::Red);
        batch.Draw(sprite, Vector2(300, 100), Color::White * 0.5f);
        batch.Draw(sprite, Vector2(400, 100), Rectangle(16, 0, 16, 16), Color::White);
        batch.Draw(sprite, Rectangle(100, 200, 128, 128), Color::White);
        batch.End();
    }

private:
    SpriteBatch batch{Graphics()};
    Texture2D sprite;
};

int main() {
    HelloSprite game;
    return game.Run();
}
