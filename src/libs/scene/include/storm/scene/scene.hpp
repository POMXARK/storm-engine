#pragma once

#include "entity.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace storm::scene
{

// A model of a scene: its .gm and its place; or a marker, a named place without a model where the script
// puts what the scene does not make itself (the player, an NPC with its AI)
struct SceneObject
{
    std::string name;
    // [[marker]] of the file: no model, only the place; GetSceneMarker gives it to the script
    bool marker = false;
    // the model as MSG_MODEL_LOAD_GEO takes it: resource/models/<geometry>.gm, "demo\crate"
    std::string geometry;
    // the shader technique of its nodes, empty for the one of the model
    std::string technique;
    // the folder of resource/textures its texture names are in, as a character's are in "characters";
    // empty when the names carry their folder ("demo\crate.tga")
    std::string textures;
    // the skeleton of an animated model as MSG_MODEL_LOAD_ANI takes it: resource/animation/<animation>.ani
    std::string animation;
    // the action its player 0 plays from the start, looped when the .ani says so
    std::string action;
    // the place, D3D layout as MODEL::mtx: the axes x, y, z in the rows 0-2, the position in the row 3
    float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    // the editor tells the objects apart by it across undo; not in the file
    uint32_t id = 0;
    // the MODELR of the object, invalid_entity when it is not made (or the game has erased it)
    entid_t entity = invalid_entity;

    // the same in the file (the entity and the id aside)
    [[nodiscard]] bool SameAs(const SceneObject &other) const;
};

// The scene loaded now, by LoadScene or by the editor
struct Scene
{
    // the name in resource/scenes without .scene, empty for a new one
    std::string name;
    std::vector<SceneObject> objects;
};

Scene &CurrentScene();

// resource/scenes/<name>.scene
std::filesystem::path ScenePath(std::string_view name);

// The text of a .scene file and back; nullopt and the reason in error for a broken one
std::optional<std::vector<SceneObject>> ParseScene(std::string_view text, std::string &error);
std::string FormatScene(const std::vector<SceneObject> &objects);

std::optional<std::vector<SceneObject>> ReadScene(std::string_view name, std::string &error);
bool WriteScene(std::string_view name, const std::vector<SceneObject> &objects);

// The MODELR of an object in the layers EXECUTE and REALIZE, at its place; false when the model is not
// loaded (object.entity stays invalid)
bool Spawn(SceneObject &object);
void Despawn(SceneObject &object);
// the matrix of the model to the object, and back
void ReadPlace(SceneObject &object);
void WritePlace(const SceneObject &object);

// The marker of the current scene by its name, nullptr when there is none
const SceneObject *FindMarker(std::string_view name);

// The scene from its file instead of the current one; the number of the models made, nullopt when the file
// is not there or is broken
std::optional<size_t> LoadScene(std::string_view name);
// the models of the current scene gone, the scene empty and new
void ClearScene();

} // namespace storm::scene
