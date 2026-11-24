// ============================================================
//  Nebula Engine - scene/scene.cpp
//  Scene <-> JSON (.nebula.json). Sprites store their logical
//  texture name; runtime ids are re-resolved on load, so scene
//  files stay portable across devices and builds.
// ============================================================
#include "engine/scene/scene.hpp"
#include "engine/ecs/components.hpp"
#include "engine/json/json.hpp"
#include "engine/core/log.hpp"
#include <cmath>

namespace nebula {

std::string Scene::serialize() const {
    using json::Value;
    Value root = Value::object();
    root["format"] = Value("nebula-scene/1");
    root["name"] = Value(name_);

    Value cam = Value::object();
    cam["x"] = Value(camera_.position.x);
    cam["y"] = Value(camera_.position.y);
    cam["zoom"] = Value(camera_.zoom);
    root["camera"] = cam;

    Value entities = Value::array();
    for (Entity e : registry_.entities<NameComponent>()) {
        Value ent = Value::object();
        const auto& t = registry_.get<TransformComponent>(e);

        ent["name"] = Value(registry_.get<NameComponent>(e).name);
        ent["tag"] = Value(registry_.has<TagComponent>(e) ? registry_.get<TagComponent>(e).tag : std::string());

        Value tr = Value::object();
        tr["x"] = Value(t.position.x);
        tr["y"] = Value(t.position.y);
        tr["rotation"] = Value(t.rotation);
        tr["scaleX"] = Value(t.scale.x);
        tr["scaleY"] = Value(t.scale.y);
        ent["transform"] = tr;

        if (registry_.has<SpriteComponent>(e)) {
            const auto& s = registry_.get<SpriteComponent>(e);
            Value spr = Value::object();
            spr["texture"] = Value(s.texture);
            spr["sizeW"] = Value(s.sizePx.x);
            spr["sizeH"] = Value(s.sizePx.y);
            Value tint = Value::array();
            tint.push(Value(s.tint.r)); tint.push(Value(s.tint.g));
            tint.push(Value(s.tint.b)); tint.push(Value(s.tint.a));
            spr["tint"] = tint;
            spr["zOrder"] = Value(s.zOrder);
            spr["srcX"] = Value(s.srcX); spr["srcY"] = Value(s.srcY);
            spr["srcW"] = Value(s.srcW); spr["srcH"] = Value(s.srcH);
            spr["flipX"] = Value(s.flipX); spr["flipY"] = Value(s.flipY);
            ent["sprite"] = spr;
        }

        if (registry_.has<RigidbodyComponent>(e)) {
            const auto& rb = registry_.get<RigidbodyComponent>(e);
            Value r = Value::object();
            r["velocityX"] = Value(rb.velocity.x);
            r["velocityY"] = Value(rb.velocity.y);
            r["gravityScale"] = Value(rb.gravityScale);
            r["restitution"] = Value(rb.restitution);
            r["friction"] = Value(rb.friction);
            r["density"] = Value(rb.density);
            r["mass"] = Value(rb.mass);
            r["isStatic"] = Value(rb.isStatic);
            ent["rigidbody"] = r;
        }

        if (registry_.has<BoxColliderComponent>(e)) {
            const auto& c = registry_.get<BoxColliderComponent>(e);
            Value col = Value::object();
            col["halfW"] = Value(c.halfExtents.x);
            col["halfH"] = Value(c.halfExtents.y);
            col["offsetX"] = Value(c.offset.x);
            col["offsetY"] = Value(c.offset.y);
            col["isTrigger"] = Value(c.isTrigger);
            col["restitution"] = Value(c.restitution);
            col["friction"] = Value(c.friction);
            ent["boxCollider"] = col;
        }

        if (registry_.has<CircleColliderComponent>(e)) {
            const auto& c = registry_.get<CircleColliderComponent>(e);
            Value col = Value::object();
            col["radius"] = Value(c.radius);
            col["offsetX"] = Value(c.offset.x);
            col["offsetY"] = Value(c.offset.y);
            col["isTrigger"] = Value(c.isTrigger);
            col["restitution"] = Value(c.restitution);
            col["friction"] = Value(c.friction);
            ent["circleCollider"] = col;
        }

        entities.push(ent);
    }
    root["entities"] = entities;
    return root.dump(2);
}

bool Scene::deserialize(const std::string& jsonText,
                        const std::function<int(const std::string&)>& textureResolver) {
    using json::Value;
    try {
        Value root = json::parse(jsonText);
        if (root.at("format").asString() != "nebula-scene/1") {
            NEBULA_LOG_E("deserialize: unsupported scene format");
            return false;
        }
        clear();
        name_ = root.at("name").asString().empty() ? "Untitled" : root.at("name").asString();

        if (const Value* cam = root.find("camera")) {
            camera_.position.x = cam->at("x").asFloat();
            camera_.position.y = cam->at("y").asFloat();
            camera_.zoom = cam->at("zoom").asFloat(1.0f);
            if (camera_.zoom <= 0.0f) camera_.zoom = 1.0f;
        }

        const Value* ents = root.find("entities");
        if (!ents || !ents->isArray()) return true;
        for (const Value& ent : ents->asArray()) {
            const Entity e = registry_.create();
            registry_.add<NameComponent>(e, NameComponent{ent.at("name").asString().empty()
                ? "Entity" : ent.at("name").asString()});
            const std::string tag = ent.at("tag").asString();
            if (!tag.empty()) registry_.add<TagComponent>(e, TagComponent{tag});

            TransformComponent t;
            if (const Value* tr = ent.find("transform")) {
                t.position.x = tr->at("x").asFloat();
                t.position.y = tr->at("y").asFloat();
                t.rotation = tr->at("rotation").asFloat();
                t.scale.x = tr->at("scaleX").asFloat(1.0f);
                t.scale.y = tr->at("scaleY").asFloat(1.0f);
            }
            registry_.add<TransformComponent>(e, t);

            if (const Value* spr = ent.find("sprite")) {
                SpriteComponent s;
                s.texture = spr->at("texture").asString().empty() ? "ship" : spr->at("texture").asString();
                s.textureId = textureResolver ? textureResolver(s.texture) : 0;
                s.sizePx.x = spr->at("sizeW").asFloat(96.0f);
                s.sizePx.y = spr->at("sizeH").asFloat(96.0f);
                if (const Value* tint = spr->find("tint")) {
                    const auto& arr = tint->asArray();
                    if (arr.size() == 4) {
                        s.tint = Color(arr[0].asFloat(1), arr[1].asFloat(1), arr[2].asFloat(1), arr[3].asFloat(1));
                    }
                }
                s.zOrder = spr->at("zOrder").asFloat();
                s.srcX = spr->at("srcX").asFloat();
                s.srcY = spr->at("srcY").asFloat();
                s.srcW = spr->at("srcW").asFloat(-1.0f);
                s.srcH = spr->at("srcH").asFloat(-1.0f);
                s.flipX = spr->at("flipX").asBool();
                s.flipY = spr->at("flipY").asBool();
                registry_.add<SpriteComponent>(e, s);
            }

            if (const Value* rbv = ent.find("rigidbody")) {
                RigidbodyComponent rb;
                rb.velocity.x = rbv->at("velocityX").asFloat();
                rb.velocity.y = rbv->at("velocityY").asFloat();
                rb.gravityScale = rbv->at("gravityScale").asFloat(1.0f);
                rb.restitution = rbv->at("restitution").asFloat(0.25f);
                rb.friction = rbv->at("friction").asFloat(0.45f);
                rb.density = rbv->at("density").asFloat(1.0f);
                rb.mass = rbv->at("mass").asFloat(1.0f);
                rb.isStatic = rbv->at("isStatic").asBool();
                registry_.add<RigidbodyComponent>(e, rb);
            }

            if (const Value* col = ent.find("boxCollider")) {
                BoxColliderComponent c;
                c.halfExtents.x = col->at("halfW").asFloat(32.0f);
                c.halfExtents.y = col->at("halfH").asFloat(32.0f);
                c.offset.x = col->at("offsetX").asFloat();
                c.offset.y = col->at("offsetY").asFloat();
                c.isTrigger = col->at("isTrigger").asBool();
                c.restitution = col->at("restitution").asFloat(-1.0f);
                c.friction = col->at("friction").asFloat(-1.0f);
                registry_.add<BoxColliderComponent>(e, c);
            }

            if (const Value* col = ent.find("circleCollider")) {
                CircleColliderComponent c;
                c.radius = col->at("radius").asFloat(32.0f);
                c.offset.x = col->at("offsetX").asFloat();
                c.offset.y = col->at("offsetY").asFloat();
                c.isTrigger = col->at("isTrigger").asBool();
                c.restitution = col->at("restitution").asFloat(-1.0f);
                c.friction = col->at("friction").asFloat(-1.0f);
                registry_.add<CircleColliderComponent>(e, c);
            }
        }
        return true;
    } catch (const std::exception& ex) {
        NEBULA_LOG_E("deserialize failed: %s", ex.what());
        return false;
    }
}

} // namespace nebula
