#pragma once

// Everything drawn over the playfield, straight onto the back buffer: the HUD, and the
// title, name entry, high score and credits screens.

#include <functional>

#include "world.h"

namespace demo {

// What the screens ask the game to do.
struct Actions {
    std::function<void()> start;  // a new run
    std::function<void()> quit;
};

class Screens : public DrawableGameComponent {
public:
    Screens(Game& game, World& world, Actions actions) : DrawableGameComponent(game), world_(world), actions_(std::move(actions)) {
        SetDrawOrder(10);
    }

    void LoadContent() override {
        // The demo's own text files, read as bytes from the package.
        if (auto credits = GetGame().Content().ReadBytes("credits.txt")) Lines(*credits, credits_);
        if (auto license = GetGame().Content().ReadBytes("silkscreen/LICENSE")) {
            std::vector<std::string> lines;
            Lines(*license, lines);
            if (!lines.empty()) credits_.push_back("Silkscreen: " + lines.front());
        }
    }

    void Update(const GameTime& time) override {
        ++frames_;
        elapsed_ += time.Delta();
        if (elapsed_ >= 0.5) {
            fps_ = frames_ / elapsed_;
            frames_ = 0;
            elapsed_ = 0.0;
        }
        slow_ = time.IsRunningSlowly();
        world_.toastLeft = std::max(0.0f, world_.toastLeft - time.Delta());
        switch (world_.screen) {
        case Screen::Title: UpdateTitle(); break;
        case Screen::NameEntry: UpdateNameEntry(); break;
        case Screen::Scores: UpdateScores(); break;
        case Screen::Credits:
            if (Confirmed() || Keyboard::WasPressed(Key::Escape) || GamePad::WasPressed(0, Buttons::B)) world_.screen = Screen::Title;
            break;
        default: break;
        }
    }

    void Draw(const GameTime& time) override {
        batch_.Begin();
        switch (world_.screen) {
        case Screen::Title: DrawTitle(time); break;
        case Screen::Play: DrawHud(); break;
        case Screen::Paused:
            DrawHud();
            Centered(world_.big, "Paused", 150, Color::White);
            Centered(world_.font, Keyboard::KeyName(Key::P) + " or Start to go on, Escape for the title", 180, Color::White * 0.8f);
            break;
        case Screen::NameEntry: DrawNameEntry(time); break;
        case Screen::Scores: DrawScores(); break;
        case Screen::Credits: DrawCredits(); break;
        }
        if (world_.toastLeft > 0.0f)
            Centered(world_.font, world_.toast, Height - 34, Color(255, 230, 160) * std::min(1.0f, world_.toastLeft));
        if (world_.debug) DrawDebug();
        batch_.End();
        // The title's letters, through the wave effect, in a batch of their own.
        if (world_.screen == Screen::Title) {
            world_.wave.Set("Time", static_cast<float>(time.Total()) * 0.25f);
            world_.wave.Set("Amplitude", 1.0f);
            world_.wave.Set("Frequency", 0.12f);
            batch_.Begin(SpriteSortMode::Deferred, BlendState::AlphaBlend, SamplerState::Default, world_.wave);
            const char* title = "COLDX DEMO";
            const Vector2 size = world_.big.MeasureString(title) * 2.0f;
            batch_.DrawString(world_.big, title, Vector2(std::round((Width - size.x) / 2), 60), Color(120, 220, 255), 0.0f, Vector2(),
                              2.0f, SpriteEffects::None, 0.0f);
            batch_.End();
        }
    }

    // Called when a run ends and the name screen opens.
    void BeginNameEntry() {
        name_.clear();
        TextInput::Start();
        TextInput::SetCaretArea(NameBox());
    }

private:
    static void Lines(const std::vector<std::uint8_t>& bytes, std::vector<std::string>& out) {
        std::string line;
        for (std::uint8_t b : bytes) {
            if (b == '\r') continue;
            if (b == '\n') {
                out.push_back(line);
                line.clear();
            } else {
                line += static_cast<char>(b);
            }
        }
        if (!line.empty()) out.push_back(line);
    }

    static bool Confirmed() {
        return Keyboard::WasPressed(Key::Enter) || Keyboard::WasPressed(Key::Space) || GamePad::WasPressed(0, Buttons::A);
    }

    void Centered(const SpriteFont& font, const std::string& text, float y, Color color) {
        const Vector2 size = font.MeasureString(text);
        batch_.DrawString(font, text, Vector2(std::round((Width - size.x) / 2), y), color);
    }

    // Title: a menu driven by keys, the pad's D-pad, or the mouse.
    static constexpr const char* Items[] = {"Play", "High scores", "Credits", "Quit"};
    static Rectangle ItemBox(int i) { return Rectangle(Width / 2 - 70, 160 + i * 24, 140, 20); }

    void UpdateTitle() {
        const int count = static_cast<int>(std::size(Items));
        if (Keyboard::WasPressed(Key::Down) || GamePad::WasPressed(0, Buttons::DPadDown)) choice_ = (choice_ + 1) % count;
        if (Keyboard::WasPressed(Key::Up) || GamePad::WasPressed(0, Buttons::DPadUp)) choice_ = (choice_ + count - 1) % count;
        const MouseState mouse = Mouse::GetState();
        bool clicked = false;
        for (int i = 0; i < count; ++i) {
            if (!ItemBox(i).Contains(mouse.x, mouse.y)) continue;
            if (mouse.x != lastMouse_.x || mouse.y != lastMouse_.y) choice_ = i;
            clicked = Mouse::WasPressed(MouseButton::Left) && choice_ == i;
        }
        lastMouse_ = Point(mouse.x, mouse.y);
        if (!clicked && !Confirmed()) return;
        switch (choice_) {
        case 0: actions_.start(); break;
        case 1: world_.screen = Screen::Scores; break;
        case 2: world_.screen = Screen::Credits; break;
        default: actions_.quit(); break;
        }
    }

    void DrawTitle(const GameTime&) {
        for (int i = 0; i < static_cast<int>(std::size(Items)); ++i) {
            const Rectangle box = ItemBox(i);
            if (i == choice_) batch_.Draw(world_.pixel, box, Color(60, 110, 200) * 0.6f);
            Centered(world_.font, Items[i], static_cast<float>(box.y + 5), i == choice_ ? Color::White : Color::White * 0.6f);
        }
        const GamePadCapabilities pad = GamePad::GetCapabilities(0);
        const std::string device = pad.isConnected ? "Pad: " + pad.name + (pad.hasRumble ? " (rumble)" : "") : "No pad: keyboard and mouse";
        Centered(world_.font, device, 270, Color::White * 0.5f);
        Centered(world_.font, std::string("Enter or ") + ConfirmLabel() + " to choose. Best " + std::to_string(world_.scores.Best()), 286,
                 Color::White * 0.5f);
        Centered(world_.font, "arrows/WASD/stick/mouse  P pause  M music  wheel volume  F11 fullscreen  F12 screenshot", Height - 16,
                 Color::White * 0.4f);
    }

    void DrawHud() {
        char line[96];
        std::snprintf(line, sizeof(line), "Score %d   Best %d", world_.score, std::max(world_.score, world_.scores.Best()));
        batch_.DrawString(world_.font, line, Vector2(8, 6), Color::White);
        for (int i = 0; i < world_.lives; ++i)
            batch_.Draw(world_.sheet, Vector2(8.0f + i * 30.0f, 18), sheet::Get("heart"), Color::White);
        std::snprintf(line, sizeof(line), "%.0f fps%s", fps_, slow_ ? "  (running slowly)" : "");
        batch_.DrawString(world_.font, line, Vector2(8, Height - 16), Color::White * 0.5f);
    }

    // Name entry: typed text from TextInput (any layout, accents, IME), Backspace, and
    // Ctrl+V from the clipboard. Only characters the font has are kept.
    static Rectangle NameBox() { return Rectangle(Width / 2 - 90, 170, 180, 22); }

    void UpdateNameEntry() {
        std::string typed = TextInput::GetText();
        const KeyboardState keys = Keyboard::GetState();
        if ((keys.IsKeyDown(Key::LeftControl) || keys.IsKeyDown(Key::RightControl)) && Keyboard::WasPressed(Key::V) && Clipboard::HasText())
            typed += Clipboard::GetText();
        for (char c : typed) {
            // ASCII only here, so the scores file stays one word per name.
            if (c == ' ') c = '_';
            if (static_cast<unsigned char>(c) < 0x80 && c > ' ' && name_.size() < 12 && world_.font.HasCharacter(static_cast<char32_t>(c)))
                name_ += c;
        }
        if (Keyboard::WasPressed(Key::Backspace) && !name_.empty()) name_.pop_back();
        if (Keyboard::WasPressed(Key::Enter) || GamePad::WasPressed(0, Buttons::A)) {
            TextInput::Stop();
            world_.scores.Add(name_.empty() ? "pilot" : name_, world_.score);
            world_.screen = Screen::Scores;
        }
    }

    void DrawNameEntry(const GameTime& time) {
        Centered(world_.big, "A new high score: " + std::to_string(world_.score), 120, Color(255, 210, 120));
        Centered(world_.font, "Type your name, Enter to save", 150, Color::White * 0.8f);
        const Rectangle box = NameBox();
        batch_.Draw(world_.pixel, box, Color(20, 30, 60) * 0.8f);
        const bool caret = std::fmod(time.Total(), 1.0) < 0.5;
        batch_.DrawString(world_.font, name_ + (caret ? "_" : ""), Vector2(static_cast<float>(box.x + 6), static_cast<float>(box.y + 6)),
                          Color::White);
    }

    void UpdateScores() {
        if (Keyboard::WasPressed(Key::Delete) && Storage::Exists(HighScores::File)) {
            Storage::Remove(HighScores::File);
            world_.scores.Load();
            world_.Say("high scores cleared");
        }
        if (Confirmed() || Keyboard::WasPressed(Key::Escape) || GamePad::WasPressed(0, Buttons::B)) world_.screen = Screen::Title;
    }

    void DrawScores() {
        Centered(world_.big, "High scores", 70, Color(255, 210, 120));
        float y = 110;
        if (world_.scores.entries.empty()) Centered(world_.font, "None yet", y, Color::White * 0.7f);
        for (const HighScores::Entry& e : world_.scores.entries) {
            char line[64];
            std::snprintf(line, sizeof(line), "%-12s %6d", e.name.c_str(), e.score);
            Centered(world_.font, line, y, Color::White);
            y += world_.font.LineSpacing() + 4;
        }
        Centered(world_.font, "Enter to go back, Delete to clear", 260, Color::White * 0.5f);
        Centered(world_.font, "Saved in " + Storage::Folder(), 276, Color::White * 0.35f);
    }

    void DrawCredits() {
        Centered(world_.big, "Credits", 50, Color(255, 210, 120));
        float y = 90;
        for (const std::string& line : credits_) {
            Centered(world_.font, line, y, Color::White * 0.8f);
            y += world_.font.LineSpacing() + 2;
        }
    }

    // F3: where the image sits in the window, the back buffer, and the frame rate.
    void DrawDebug() {
        const Rectangle shown = Graphics().PresentedArea();
        char line[128];
        std::snprintf(line, sizeof(line), "back buffer %dx%d, shown at %d,%d %dx%d in the window, %.0f fps", Graphics().BackBufferWidth(),
                      Graphics().BackBufferHeight(), shown.x, shown.y, shown.width, shown.height, fps_);
        batch_.Draw(world_.pixel, Rectangle(0, 0, Width, 14), Color::Black * 0.7f);
        batch_.DrawString(world_.font, line, Vector2(4, 2), Color(160, 255, 160));
    }

    World& world_;
    Actions actions_;
    SpriteBatch batch_{Graphics()};
    std::vector<std::string> credits_;
    std::string name_;
    int choice_ = 0;
    Point lastMouse_;
    int frames_ = 0;
    double elapsed_ = 0.0;
    double fps_ = 0.0;
    bool slow_ = false;
};

} // namespace demo
