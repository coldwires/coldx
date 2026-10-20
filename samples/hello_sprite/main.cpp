#include <coldx/coldx.h>

using namespace coldx;

class HelloSprite : public Game {
public:
    HelloSprite() {
        Settings().title = "hello_sprite";
    }

protected:
    void Update(const GameTime&) override {
        if (Keyboard::GetState().IsKeyDown(Key::Escape)) Exit();
    }

    void Draw(const GameTime&) override {
        Graphics().Clear(Color::CornflowerBlue);
    }
};

int main() {
    HelloSprite game;
    return game.Run();
}
