#pragma once

#include <memory>
#include <utility>

#include <coldx/core/game_time.h>

namespace coldx {

class Game;
class GameComponentCollection;
class GraphicsDevice;

// A piece of a game with its own Update: a camera, a spawner, a debug overlay. Add it to
// Game::Components() and the game calls it every Update, after its own, in UpdateOrder.
class GameComponent {
public:
    explicit GameComponent(Game& game) : game_(game) {}
    virtual ~GameComponent() = default;
    GameComponent(const GameComponent&) = delete;
    GameComponent& operator=(const GameComponent&) = delete;

    // Once, before the first Update: when the game initializes, or when added later.
    virtual void Initialize() {}
    virtual void Update(const GameTime&) {}

    bool Enabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    // Lower updates first; equal orders keep the order they were added in.
    int UpdateOrder() const { return updateOrder_; }
    void SetUpdateOrder(int order);

    Game& GetGame() const { return game_; }

private:
    friend class GameComponentCollection;
    friend class DrawableGameComponent;
    // Which components draw, without dynamic_cast: coldx builds with RTTI off.
    virtual DrawableGameComponent* AsDrawable() { return nullptr; }
    Game& game_;
    GameComponentCollection* owner_ = nullptr;
    bool enabled_ = true;
    int updateOrder_ = 0;
};

// A component that also draws, after the game's own Draw, in DrawOrder. Its LoadContent
// runs right after its Initialize; UnloadContent when it is removed or the game ends.
class DrawableGameComponent : public GameComponent {
public:
    using GameComponent::GameComponent;

    virtual void LoadContent() {}
    virtual void UnloadContent() {}
    virtual void Draw(const GameTime&) {}

    bool Visible() const { return visible_; }
    void SetVisible(bool visible) { visible_ = visible; }
    // Lower draws first, so higher draws on top.
    int DrawOrder() const { return drawOrder_; }
    void SetDrawOrder(int order);

    GraphicsDevice& Graphics() const;

private:
    DrawableGameComponent* AsDrawable() final { return this; }
    bool visible_ = true;
    int drawOrder_ = 0;
};

// The game's components. Owns them. Adding, removing or reordering during an Update or
// Draw takes effect after that pass, so the pass in progress is never disturbed.
class GameComponentCollection {
public:
    GameComponentCollection();
    ~GameComponentCollection();
    GameComponentCollection(const GameComponentCollection&) = delete;
    GameComponentCollection& operator=(const GameComponentCollection&) = delete;

    // Takes ownership and returns the component. Initialized at once if the game is running.
    template <class T>
    T& Add(std::unique_ptr<T> component) {
        T& result = *component;
        Insert(std::unique_ptr<GameComponent>(std::move(component)));
        return result;
    }

    // Constructs a T with args (usually the game first) and adds it.
    template <class T, class... Args>
    T& Emplace(Args&&... args) {
        return Add(std::make_unique<T>(std::forward<Args>(args)...));
    }

    // Unloads and destroys it, after the current pass if one is running. Does nothing if
    // the component is not in this collection.
    void Remove(GameComponent& component);

    int Count() const;

private:
    friend class Game;
    friend class GameComponent;
    friend class DrawableGameComponent;
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void Insert(std::unique_ptr<GameComponent> component);
    void MarkUnsorted();
    // Driven by Game::Run.
    void InitializeAll();
    void UpdateAll(const GameTime& time);
    void DrawAll(const GameTime& time);
    void UnloadAll();
};

} // namespace coldx
