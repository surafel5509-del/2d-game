#pragma once
// ============================================================
//  Nebula Engine - scene/scene.hpp
//  A Scene = Registry + Camera + metadata. Scenes serialize to
//  human-readable JSON (.nebula.json) and are the unit the
//  editor edits and the runtime plays.
// ============================================================
#include "engine/ecs/registry.hpp"
#include "engine/render/camera2d.hpp"
#include <functional>
#include <string>
#include <utility>

namespace nebula {

class Scene {
public:
    Scene() : name_("Untitled") {}

    const std::string& name() const { return name_; }
    void setName(const std::string& n) { name_ = n; }

    Registry& registry() { return registry_; }
    const Registry& registry() const { return registry_; }

    Camera2D& camera() { return camera_; }
    const Camera2D& camera() const { return camera_; }

    void clear() {
        registry_.clear();
        camera_ = Camera2D{};
        selected_ = kNullEntity;
    }

    // Editor selection lives on the scene (not the registry).
    Entity selectedEntity() const { return selected_; }
    void setSelectedEntity(Entity e) { selected_ = e; }

    // Serialize to JSON text (stable key order).
    std::string serialize() const;

    // Replace scene contents from JSON text. `textureResolver` maps
    // texture names to runtime ids (return 0 when unknown).
    bool deserialize(const std::string& jsonText,
                     const std::function<int(const std::string&)>& textureResolver);

private:
    std::string name_;
    Registry registry_;
    Camera2D camera_;
    Entity selected_ = kNullEntity;
};

} // namespace nebula
