// A game whose content is encrypted and fingerprinted by coldx_add_content. It reads its
// package with the key the generated source installed, then exits: 0 if everything came
// back as packed. The text it expects is hidden too, so it is in neither file.

#include <coldx/coldx.h>
#include <coldx/protect/hidden.h>

#include <string>

using namespace coldx;

class ProtectedGame : public Game {
public:
    int result = 1;

protected:
    void LoadContent() override {
        Content().SetLooseFiles(LooseFiles::Never);
        const Texture2D sheet = Content().Load<Texture2D>("sheet");
        const auto notes = Content().ReadBytes("notes.txt");
        const bool ok = sheet.Width() == 144 && sheet.Height() == 128 && notes &&
                        std::string(notes->begin(), notes->end()) == COLDX_HIDDEN("Secret notes only the game should read.\n");
        Log("protected_game: %s", ok ? "content read" : "content NOT read");
        result = ok ? 0 : 1;
    }
    void Update(const GameTime&) override { Exit(); }
};

int main() {
    ProtectedGame game;
    game.Settings().console = ConsoleMode::Never;
    if (game.Run() != 0) return 1;
    return game.result;
}
