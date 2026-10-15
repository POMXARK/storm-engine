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
// puts what the scene does not make itself (the player, an NPC with its AI); or a sound source, a marker
// that plays a sound there over and over (the surf, a fire, a tavern); or a trigger zone, a marker with a
// radius and a height the script asks who is in (CheckSceneTriggers)
struct SceneObject
{
    std::string name;
    // [[marker]] of the file: no model, only the place; GetSceneMarker gives it to the script.
    // [[sound]] is a marker too, with its sound, and [[trigger]], with its zone
    bool marker = false;
    // the sound of a source: a file of resource/sounds ("demo\waves.wav") or an alias of resource/ini/aliases;
    // empty for a plain marker
    std::string sound;
    // its loudness, 0-1, and the distances in metres: full up to the first, silent from the second
    float volume = 1.f;
    float distance[2] = {2.f, 30.f};
    // the zone of a trigger: a vertical cylinder from the place up, 0 radius for anything else
    float radius = 0.f;
    float height = 2.f;
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
    // the sound playing for a source (an id of the sound service), 0 when none
    int32_t voice = 0;
    // a model out of the layer REALIZE or a sound stopped by the script (SceneObjectShow); not in the file
    bool hidden = false;

    [[nodiscard]] bool IsSound() const
    {
        return marker && !sound.empty();
    }
    [[nodiscard]] bool IsTrigger() const
    {
        return marker && sound.empty() && radius > 0.f;
    }
    // a place for the script and nothing else
    [[nodiscard]] bool IsPlainMarker() const
    {
        return marker && !IsSound() && !IsTrigger();
    }
    // whether a point is in the zone of a trigger
    [[nodiscard]] bool Contains(float x, float y, float z) const;
    // the same in the file (the entity and the id aside)
    [[nodiscard]] bool SameAs(const SceneObject &other) const;
    // made the same: what Spawn takes, the place aside
    [[nodiscard]] bool MadeAs(const SceneObject &other) const;
};

// The scene loaded now, by LoadScene or by the editor
struct Scene
{
    // the name in resource/scenes without .scene, empty for a new one
    std::string name;
    std::vector<SceneObject> objects;
    // the objects as LoadScene made them, before the scripts changed anything: the editor, opened while
    // the game runs, edits them and gives them back on Stop
    std::vector<SceneObject> loaded;
};

Scene &CurrentScene();

// resource/scenes/<name>.scene
std::filesystem::path ScenePath(std::string_view name);

// The text of a .scene file and back; nullopt and the reason in error for a broken one
std::optional<std::vector<SceneObject>> ParseScene(std::string_view text, std::string &error);
std::string FormatScene(const std::vector<SceneObject> &objects);

std::optional<std::vector<SceneObject>> ReadScene(std::string_view name, std::string &error);
bool WriteScene(std::string_view name, const std::vector<SceneObject> &objects);

// The MODELR of an object in the layers EXECUTE and REALIZE, at its place, or the sound of a source playing
// looped at its place; false when the model or the sound is not loaded and for a plain marker
bool Spawn(SceneObject &object);
void Despawn(SceneObject &object);
// the matrix of the model to the object, and back (the place of a sound only goes to it)
void ReadPlace(SceneObject &object);
void WritePlace(const SceneObject &object);

// The marker of the current scene by its name, nullptr when there is none
const SceneObject *FindMarker(std::string_view name);
// Any object of the current scene by its name
SceneObject *FindObject(std::string_view name);

// What the script changes in an object while the game runs: the action of an animated model, whether a
// model is drawn or a sound plays, the place (the turn about y only, as a character's); false when the
// object can not take it
bool PlayAction(SceneObject &object, const std::string &action);
bool Show(SceneObject &object, bool show);
void Move(SceneObject &object, float x, float y, float z, float ay);

// Who (any name the script gives: a character, a ship) is at x, y, z now: posts SceneTriggerEnter and
// SceneTriggerExit (string trigger, string who) for each zone of the current scene it came into or left
// since the last call for it, and gives in how many zones it is
size_t CheckTriggers(const std::string &who, float x, float y, float z);

// The scene from its file instead of the current one; the number of the models made, nullopt when the file
// is not there or is broken
std::optional<size_t> LoadScene(std::string_view name);
// the models of the current scene gone, the scene empty and new
void ClearScene();

} // namespace storm::scene
