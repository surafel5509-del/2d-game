#pragma once
// ============================================================
//  Nebula Engine - ecs/registry.hpp
//  EnTT-style sparse-set ECS registry.
//   * Dense pools per component type (cache-friendly iteration)
//   * Sparse map entity -> dense index (O(1) lookup)
//   * Swap-and-pop removal, type-erased destruction via IPool
// ============================================================
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace nebula {

using Entity = uint32_t;
inline constexpr Entity kNullEntity = 0u;

namespace detail {
struct TypeCounter { static size_t next() { static size_t n = 0; return n++; } };
template <typename T> inline size_t typeId() { static size_t id = TypeCounter::next(); return id; }

struct IDataPool {
    virtual ~IDataPool() = default;
    virtual void removeIfPresent(Entity e) = 0;
    virtual void clear() = 0;
    virtual size_t size() const = 0;
};

template <typename T>
class DataPool final : public IDataPool {
public:
    T& add(Entity e, const T& value) {
        auto it = sparse_.find(e);
        if (it != sparse_.end()) {
            dense_[it->second] = value;
            return dense_[it->second];
        }
        const size_t idx = dense_.size();
        dense_.push_back(value);
        denseEntities_.push_back(e);
        sparse_.emplace(e, idx);
        return dense_.back();
    }

    T& get(Entity e) {
        auto it = sparse_.find(e);
        if (it == sparse_.end()) throw std::runtime_error("component not found");
        return dense_[it->second];
    }

    const T& get(Entity e) const {
        auto it = sparse_.find(e);
        if (it == sparse_.end()) throw std::runtime_error("component not found");
        return dense_[it->second];
    }

    bool has(Entity e) const { return sparse_.find(e) != sparse_.end(); }

    void remove(Entity e) {
        auto it = sparse_.find(e);
        if (it == sparse_.end()) return;
        const size_t idx = it->second;
        const size_t last = dense_.size() - 1;
        if (idx != last) {
            dense_[idx] = std::move(dense_[last]);
            denseEntities_[idx] = denseEntities_[last];
            sparse_[denseEntities_[idx]] = idx;
        }
        dense_.pop_back();
        denseEntities_.pop_back();
        sparse_.erase(it);
    }

    // IDataPool
    void removeIfPresent(Entity e) override { remove(e); }
    void clear() override { dense_.clear(); denseEntities_.clear(); sparse_.clear(); }
    size_t size() const override { return dense_.size(); }

    // Dense access for iteration / views.
    std::vector<T>& dense() { return dense_; }
    const std::vector<T>& dense() const { return dense_; }
    std::vector<Entity>& denseEntities() { return denseEntities_; }
    const std::vector<Entity>& denseEntities() const { return denseEntities_; }

private:
    std::vector<T> dense_;
    std::vector<Entity> denseEntities_;
    std::unordered_map<Entity, size_t> sparse_;
};
} // namespace detail

class Registry {
public:
    Registry() { poolSlots_.resize(64); }
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    Entity create() {
        const Entity e = nextEntity_++;
        alive_.insert(e);
        return e;
    }

    bool alive(Entity e) const { return alive_.find(e) != alive_.end(); }

    void destroy(Entity e) {
        if (!alive(e)) return;
        for (auto& slot : poolSlots_) {
            if (slot) slot->removeIfPresent(e);
        }
        alive_.erase(e);
    }

    void clear() {
        for (auto& slot : poolSlots_) if (slot) slot->clear();
        alive_.clear();
        // Keep id counter monotonic so stale ids never alias new entities.
    }

    size_t entityCount() const { return alive_.size(); }

    template <typename T, typename... Args>
    T& add(Entity e, Args&&... args) {
        return pool<T>().add(e, T(std::forward<Args>(args)...));
    }

    template <typename T>
    T& get(Entity e) { return pool<T>().get(e); }

    template <typename T>
    const T& get(Entity e) const {
        auto* p = poolIfExists<T>();
        if (!p) throw std::runtime_error("component pool not found");
        return p->get(e);
    }

    template <typename T>
    bool has(Entity e) const {
        auto* p = poolIfExists<T>();
        return p && p->has(e);
    }

    template <typename T>
    void remove(Entity e) { pool<T>().remove(e); }

    template <typename T>
    size_t componentCount() const {
        auto* p = poolIfExists<T>();
        return p ? p->size() : 0;
    }

    // Dense iteration over a single component type.
    template <typename T, typename Fn>
    void each(Fn&& fn) {
        auto* p = poolIfExists<T>();
        if (!p) return;
        auto& entities = p->denseEntities();
        auto& dense = p->dense();
        for (size_t i = 0; i < dense.size(); ++i) fn(entities[i], dense[i]);
    }

    // Const single-component iteration.
    template <typename T, typename Fn>
    void each(Fn&& fn) const {
        auto* p = poolIfExists<T>();
        if (!p) return;
        const auto& entities = p->denseEntities();
        const auto& dense = p->dense();
        for (size_t i = 0; i < dense.size(); ++i) fn(entities[i], dense[i]);
    }

    // Pair iteration: iterates the smaller pool, sparse-lookups the other.
    template <typename A, typename B, typename Fn>
    void eachPair(Fn&& fn) {
        auto* pa = poolIfExists<A>();
        auto* pb = poolIfExists<B>();
        if (!pa || !pb) return;
        if (pa->size() > pb->size()) {
            eachSwapOrder<A, B>(std::forward<Fn>(fn));
            return;
        }
        auto& entities = pa->denseEntities();
        auto& denseA = pa->dense();
        for (size_t i = 0; i < denseA.size(); ++i) {
            if (pb->has(entities[i])) fn(entities[i], denseA[i], pb->get(entities[i]));
        }
    }

    template <typename T>
    std::vector<Entity> entities() const {
        auto* p = poolIfExists<T>();
        return p ? p->denseEntities() : std::vector<Entity>{};
    }

private:
    template <typename A, typename B, typename Fn>
    void eachSwapOrder(Fn&& fn) {
        auto* pa = poolIfExists<A>();
        auto* pb = poolIfExists<B>();
        auto& entities = pb->denseEntities();
        auto& denseB = pb->dense();
        for (size_t i = 0; i < denseB.size(); ++i) {
            if (pa->has(entities[i])) fn(entities[i], pa->get(entities[i]), denseB[i]);
        }
    }

    template <typename T>
    detail::DataPool<T>& pool() {
        const size_t id = detail::typeId<T>();
        ensureCapacity(id);
        auto& slot = poolSlots_[id];
        if (!slot) slot = std::make_unique<detail::DataPool<T>>();
        return static_cast<detail::DataPool<T>&>(*slot);
    }

    template <typename T>
    detail::DataPool<T>* poolIfExists() const {
        const size_t id = detail::typeId<T>();
        if (id >= poolSlots_.size() || !poolSlots_[id]) return nullptr;
        return static_cast<detail::DataPool<T>*>(poolSlots_[id].get());
    }

    void ensureCapacity(size_t id) {
        if (id >= poolSlots_.size()) poolSlots_.resize(id + 16);
    }

    Entity nextEntity_ = 1;
    std::unordered_set<Entity> alive_;
    std::vector<std::unique_ptr<detail::IDataPool>> poolSlots_;
};

} // namespace nebula
