#include <coldx/core/game.h>
#include <coldx/core/game_component.h>

#include <algorithm>
#include <vector>

namespace coldx {

void GameComponent::SetUpdateOrder(int order) {
    if (order == updateOrder_) return;
    updateOrder_ = order;
    if (owner_) owner_->MarkUnsorted();
}

void DrawableGameComponent::SetDrawOrder(int order) {
    if (order == drawOrder_) return;
    drawOrder_ = order;
    if (owner_) owner_->MarkUnsorted();
}

GraphicsDevice& DrawableGameComponent::Graphics() const {
    return GetGame().Graphics();
}

struct GameComponentCollection::Impl {
    struct Entry {
        std::unique_ptr<GameComponent> component;
        DrawableGameComponent* drawable;  // null for plain components
        long long sequence;               // insertion order, for stable ordering
        bool initialized = false;
        bool removed = false;
    };
    std::vector<Entry> entries;
    std::vector<Entry> pending;  // added during a pass
    long long nextSequence = 0;
    bool running = false;        // InitializeAll has happened
    int passDepth = 0;
    bool unsorted = false;

    std::vector<GameComponent*> updateOrder;
    std::vector<DrawableGameComponent*> drawOrder;

    static void Start(Entry& e) {
        if (e.initialized) return;
        e.initialized = true;
        e.component->Initialize();
        if (e.drawable) e.drawable->LoadContent();
    }

    static void Finish(Entry& e) {
        if (e.initialized && e.drawable) e.drawable->UnloadContent();
        e.initialized = false;
    }

    // Applies what a pass deferred: drop removed entries, take in pending ones, re-sort.
    void Settle() {
        if (passDepth > 0) return;
        const auto removedBegin = std::remove_if(entries.begin(), entries.end(), [](const Entry& e) { return e.removed; });
        if (removedBegin != entries.end()) {
            entries.erase(removedBegin, entries.end());
            unsorted = true;
        }
        if (!pending.empty()) {
            for (Entry& e : pending) entries.push_back(std::move(e));
            pending.clear();
            unsorted = true;
        }
        if (!unsorted) return;
        updateOrder.clear();
        drawOrder.clear();
        std::vector<Entry*> sorted;
        for (Entry& e : entries) sorted.push_back(&e);
        std::stable_sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b) {
            return a->component->UpdateOrder() != b->component->UpdateOrder()
                       ? a->component->UpdateOrder() < b->component->UpdateOrder()
                       : a->sequence < b->sequence;
        });
        for (Entry* e : sorted) updateOrder.push_back(e->component.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b) {
            const int da = a->drawable ? a->drawable->DrawOrder() : 0;
            const int db = b->drawable ? b->drawable->DrawOrder() : 0;
            return da != db ? da < db : a->sequence < b->sequence;
        });
        for (Entry* e : sorted)
            if (e->drawable) drawOrder.push_back(e->drawable);
        unsorted = false;
    }

    Entry* Find(const GameComponent& c) {
        for (Entry& e : entries)
            if (e.component.get() == &c) return &e;
        for (Entry& e : pending)
            if (e.component.get() == &c) return &e;
        return nullptr;
    }
};

GameComponentCollection::GameComponentCollection() : impl_(std::make_unique<Impl>()) {}

GameComponentCollection::~GameComponentCollection() = default;

void GameComponentCollection::Insert(std::unique_ptr<GameComponent> component) {
    if (!component) return;
    Impl& c = *impl_;
    component->owner_ = this;
    DrawableGameComponent* drawable = component->AsDrawable();
    Impl::Entry entry{std::move(component), drawable, c.nextSequence++};
    if (c.running) Impl::Start(entry);
    if (c.passDepth > 0) {
        c.pending.push_back(std::move(entry));
    } else {
        c.entries.push_back(std::move(entry));
        c.unsorted = true;
        c.Settle();
    }
}

void GameComponentCollection::Remove(GameComponent& component) {
    Impl& c = *impl_;
    Impl::Entry* e = c.Find(component);
    if (!e || e->removed) return;
    Impl::Finish(*e);
    e->removed = true;
    std::erase_if(c.pending, [](const Impl::Entry& p) { return p.removed; });
    c.Settle();
}

int GameComponentCollection::Count() const {
    int n = 0;
    for (const Impl::Entry& e : impl_->entries) n += e.removed ? 0 : 1;
    return n + static_cast<int>(impl_->pending.size());
}

void GameComponentCollection::MarkUnsorted() {
    impl_->unsorted = true;
    impl_->Settle();
}

void GameComponentCollection::InitializeAll() {
    Impl& c = *impl_;
    c.running = true;
    ++c.passDepth;
    // By index: a component's Initialize may add more, which go to pending and start there.
    for (size_t i = 0; i < c.entries.size(); ++i)
        if (!c.entries[i].removed) Impl::Start(c.entries[i]);
    --c.passDepth;
    c.Settle();
}

void GameComponentCollection::UpdateAll(const GameTime& time) {
    Impl& c = *impl_;
    ++c.passDepth;
    for (GameComponent* component : c.updateOrder) {
        const Impl::Entry* e = c.Find(*component);
        if (e && !e->removed && component->Enabled()) component->Update(time);
    }
    --c.passDepth;
    c.Settle();
}

void GameComponentCollection::DrawAll(const GameTime& time) {
    Impl& c = *impl_;
    ++c.passDepth;
    for (DrawableGameComponent* component : c.drawOrder) {
        const Impl::Entry* e = c.Find(*component);
        if (e && !e->removed && component->Visible()) component->Draw(time);
    }
    --c.passDepth;
    c.Settle();
}

void GameComponentCollection::UnloadAll() {
    Impl& c = *impl_;
    for (Impl::Entry& e : c.entries) Impl::Finish(e);
    for (Impl::Entry& e : c.pending) Impl::Finish(e);
    c.running = false;
}

} // namespace coldx
