#include "storm/scene/scene.hpp"

#include "core.h"
#include "geometry.h"
#include "model.h"
#include "shared/layers.h"
#include "shared/messages.h"
#include "v_sound_service.h"

#include <fmt/format.h>
#include <toml++/toml.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace storm::scene
{
namespace
{

// the priority of the models of a scene in their layers, as the demos put theirs
constexpr priority_t kPriority = 10;

// 4 digits are a tenth of a millimetre: the file stays readable and the same place gives the same text
double Round(float value)
{
    const double rounded = std::round(static_cast<double>(value) * 10000.0) / 10000.0;
    return rounded == 0.0 ? 0.0 : rounded;
}

std::string Number(float value)
{
    std::string text = fmt::format("{}", Round(value));
    // TOML wants a float to look like one
    if (text.find_first_of(".e") == std::string::npos)
    {
        text += ".0";
    }
    return text;
}

std::string Numbers(const float *values, size_t count, const char *separator = ", ")
{
    std::string text = "[ ";
    for (size_t i = 0; i < count; ++i)
    {
        text += (i > 0 ? separator : "") + Number(values[i]);
    }
    return text + " ]";
}

// a literal string keeps the backslashes of the paths as they are
std::string Quote(const std::string &text)
{
    if (text.find_first_of("'\n\r") == std::string::npos)
    {
        return "'" + text + "'";
    }
    std::ostringstream out;
    out << toml::value<std::string>(text);
    return out.str();
}

bool ReadFloats(const toml::node_view<const toml::node> &node, float *values, size_t count)
{
    const toml::array *array = node.as_array();
    if (array == nullptr || array->size() != count)
    {
        return false;
    }
    for (size_t i = 0; i < count; ++i)
    {
        const std::optional<double> value = (*array)[i].value<double>();
        if (!value)
        {
            return false;
        }
        values[i] = static_cast<float>(*value);
    }
    return true;
}

MODEL *Model(entid_t entity)
{
    return dynamic_cast<MODEL *>(core.GetEntityPointerSafe(entity));
}

VSoundService *Sounds()
{
    return core.GetServiceX<VSoundService>();
}

// the zones each who of CheckTriggers was in at its last call, by the names of the triggers
std::map<std::string, std::set<std::string>> &Inside()
{
    static std::map<std::string, std::set<std::string>> inside;
    return inside;
}

// what an object is in the file: [[model]], [[marker]], [[sound]], [[trigger]]
enum Kind
{
    kModel,
    kMarker,
    kSound,
    kTrigger,
};

Kind KindOf(const SceneObject &object)
{
    return !object.marker ? kModel : object.IsSound() ? kSound : object.IsTrigger() ? kTrigger : kMarker;
}

void PlaySound(SceneObject &object)
{
    VSoundService *sounds = Sounds();
    if (sounds == nullptr)
    {
        return;
    }
    VSoundService::SoundPlayOptions options;
    options.type = PCM_3D;
    options.startPosition = CVECTOR(object.matrix[12], object.matrix[13], object.matrix[14]);
    options.minDistance = object.distance[0];
    options.maxDistance = object.distance[1];
    options.volume = object.volume;
    options.looped = true;
    object.voice = sounds->SoundPlay(object.sound, options);
    if (object.voice == 0)
    {
        core.Trace("Scene: no sound %s for \"%s\"", object.sound.c_str(), object.name.c_str());
    }
}

void StopSound(SceneObject &object)
{
    if (object.voice != 0)
    {
        if (VSoundService *sounds = Sounds())
        {
            sounds->SoundStop(object.voice);
        }
        object.voice = 0;
    }
}

// what the file keeps of an object but the name and the place
bool SameKeys(const SceneObject &a, const SceneObject &b)
{
    return a.marker == b.marker && a.geometry == b.geometry && a.technique == b.technique && a.textures == b.textures &&
           a.animation == b.animation && a.action == b.action && a.sound == b.sound &&
           Round(a.volume) == Round(b.volume) && Round(a.distance[0]) == Round(b.distance[0]) &&
           Round(a.distance[1]) == Round(b.distance[1]) && Round(a.radius) == Round(b.radius) &&
           Round(a.height) == Round(b.height);
}

} // namespace

bool SceneObject::MadeAs(const SceneObject &other) const
{
    return SameKeys(*this, other) && hidden == other.hidden;
}

bool SceneObject::Contains(float x, float y, float z) const
{
    const float dx = x - matrix[12];
    const float dz = z - matrix[14];
    return IsTrigger() && dx * dx + dz * dz <= radius * radius && y >= matrix[13] && y <= matrix[13] + height;
}

bool SceneObject::SameAs(const SceneObject &other) const
{
    // hidden is the script's, not the file's
    if (name != other.name || !SameKeys(*this, other))
    {
        return false;
    }
    // what the file keeps of the place
    for (size_t i = 0; i < 16; ++i)
    {
        if (Round(matrix[i]) != Round(other.matrix[i]))
        {
            return false;
        }
    }
    return true;
}

Scene &CurrentScene()
{
    static Scene scene;
    return scene;
}

std::filesystem::path ScenePath(std::string_view name)
{
    return std::filesystem::path("resource") / "scenes" / (std::string(name) + ".scene");
}

std::optional<std::vector<SceneObject>> ParseScene(std::string_view text, std::string &error)
{
    toml::table table;
    try
    {
        table = toml::parse(text);
    }
    catch (const toml::parse_error &e)
    {
        std::ostringstream out;
        out << e.description() << " at " << e.source().begin;
        error = out.str();
        return std::nullopt;
    }
    std::vector<SceneObject> objects;
    // the models first, then the markers, the sounds, the triggers: the order the file is written in
    for (const char *kind : {"model", "marker", "sound", "trigger"})
    {
        const bool marker = std::string_view(kind) != "model";
        const bool sound = std::string_view(kind) == "sound";
        const bool trigger = std::string_view(kind) == "trigger";
        const toml::array *items = table[kind].as_array();
        if (items == nullptr)
        {
            continue;
        }
        for (const toml::node &node : *items)
        {
            const toml::table *item = node.as_table();
            if (item == nullptr)
            {
                error = fmt::format("[[{}]] is not a table", kind);
                return std::nullopt;
            }
            const toml::node_view<const toml::node> view(item);
            SceneObject object;
            object.marker = marker;
            object.name = view["name"].value_or(std::string());
            if (marker && object.name.empty())
            {
                error = fmt::format("a {} has no name", kind);
                return std::nullopt;
            }
            if (sound)
            {
                object.sound = view["sound"].value_or(std::string());
                object.volume = static_cast<float>(view["volume"].value_or(1.0));
                if (object.sound.empty())
                {
                    error = "sound \"" + object.name + "\" has no sound";
                    return std::nullopt;
                }
                if (view["distance"] && !ReadFloats(view["distance"], object.distance, 2))
                {
                    error = "sound \"" + object.name + "\": distance is 2 numbers";
                    return std::nullopt;
                }
            }
            if (trigger)
            {
                object.radius = static_cast<float>(view["radius"].value_or(0.0));
                object.height = static_cast<float>(view["height"].value_or(2.0));
                if (object.radius <= 0.f || object.height <= 0.f)
                {
                    error = "trigger \"" + object.name + "\" has no radius or height";
                    return std::nullopt;
                }
            }
            if (!marker)
            {
                object.geometry = view["geometry"].value_or(std::string());
                object.technique = view["technique"].value_or(std::string());
                object.textures = view["textures"].value_or(std::string());
                object.animation = view["animation"].value_or(std::string());
                object.action = view["action"].value_or(std::string());
                if (object.geometry.empty())
                {
                    error = "model \"" + object.name + "\" has no geometry";
                    return std::nullopt;
                }
            }
            float position[3] = {};
            float axes[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            if ((view["position"] && !ReadFloats(view["position"], position, 3)) ||
                (view["axes"] && !ReadFloats(view["axes"], axes, 9)))
            {
                error = fmt::format("{} \"{}\": position is 3 numbers, axes 9", kind, object.name);
                return std::nullopt;
            }
            for (size_t row = 0; row < 3; ++row)
            {
                std::copy(axes + row * 3, axes + row * 3 + 3, object.matrix + row * 4);
            }
            std::copy(position, position + 3, object.matrix + 12);
            objects.push_back(std::move(object));
        }
    }
    return objects;
}

std::string FormatScene(const std::vector<SceneObject> &objects)
{
    // written by hand, not by toml++: the keys go in the order a reader wants and the floats stay short
    std::string text = "# A scene of Storm Engine: the models and their places, made in the window Scene of the editor.\n"
                       "# A script loads it with LoadScene(\"<name>\"). position is in metres, axes are the rows x, y, z of\n"
                       "# the turn. A marker is a place the script asks for by name (GetSceneMarker). A sound plays\n"
                       "# looped where it is: a file of resource/sounds or an alias, full up to the first distance, silent\n"
                       "# from the second. A trigger is a zone, a cylinder from its place up: the script tells who is\n"
                       "# where (CheckSceneTriggers) and gets SceneTriggerEnter and SceneTriggerExit.\n";
    for (const Kind kind : {kModel, kMarker, kSound, kTrigger})
    {
        for (const SceneObject &object : objects)
        {
            const bool markers = kind != kModel;
            if (KindOf(object) != kind)
            {
                continue;
            }
            const float axes[9] = {object.matrix[0], object.matrix[1], object.matrix[2],
                                   object.matrix[4], object.matrix[5], object.matrix[6],
                                   object.matrix[8], object.matrix[9], object.matrix[10]};
            text += kind == kTrigger ? "\n[[trigger]]\n"
                    : kind == kSound ? "\n[[sound]]\n"
                    : markers        ? "\n[[marker]]\n"
                                     : "\n[[model]]\n";
            text += "name = " + Quote(object.name) + "\n";
            if (kind == kTrigger)
            {
                text += "radius = " + Number(object.radius) + "\n";
                text += "height = " + Number(object.height) + "\n";
                text += "position = " + Numbers(object.matrix + 12, 3) + "\n";
                continue;
            }
            if (kind == kSound)
            {
                text += "sound = " + Quote(object.sound) + "\n";
                text += "volume = " + Number(object.volume) + "\n";
                text += "distance = " + Numbers(object.distance, 2) + "\n";
                text += "position = " + Numbers(object.matrix + 12, 3) + "\n";
                continue;
            }
            if (!markers)
            {
                text += "geometry = " + Quote(object.geometry) + "\n";
                if (!object.technique.empty())
                {
                    text += "technique = " + Quote(object.technique) + "\n";
                }
                if (!object.textures.empty())
                {
                    text += "textures = " + Quote(object.textures) + "\n";
                }
                if (!object.animation.empty())
                {
                    text += "animation = " + Quote(object.animation) + "\n";
                }
                if (!object.action.empty())
                {
                    text += "action = " + Quote(object.action) + "\n";
                }
            }
            text += "position = " + Numbers(object.matrix + 12, 3) + "\n";
            text += "axes = " + Numbers(axes, 9) + "\n";
        }
    }
    return text;
}

std::optional<std::vector<SceneObject>> ReadScene(std::string_view name, std::string &error)
{
    std::ifstream in(ScenePath(name), std::ios::binary);
    if (!in)
    {
        error = "no file " + ScenePath(name).generic_string();
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return ParseScene(text.str(), error);
}

bool WriteScene(std::string_view name, const std::vector<SceneObject> &objects)
{
    const std::filesystem::path path = ScenePath(name);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary);
    out << FormatScene(objects);
    return static_cast<bool>(out);
}

bool Spawn(SceneObject &object)
{
    object.entity = invalid_entity;
    object.voice = 0;
    if (object.IsSound())
    {
        if (!object.hidden)
        {
            PlaySound(object);
        }
        return object.voice != 0;
    }
    if (object.marker)
    {
        return false;
    }
    object.entity = core.CreateEntity("MODELR");
    if (object.entity == invalid_entity)
    {
        return false;
    }
    // the textures load with the model, from the folder set for that moment (as a character does it)
    auto *geometry = core.GetServiceX<VGEOMETRY>();
    if (geometry != nullptr && !object.textures.empty())
    {
        geometry->SetTexturePath((object.textures + "\\").c_str());
    }
    // a model that does not load erases its entity itself
    const bool loaded = core.Send_Message(object.entity, "ls", MSG_MODEL_LOAD_GEO, object.geometry.c_str()) != 0;
    if (geometry != nullptr && !object.textures.empty())
    {
        geometry->SetTexturePath("");
    }
    if (!loaded)
    {
        core.Trace("Scene: no model %s for \"%s\"", object.geometry.c_str(), object.name.c_str());
        object.entity = invalid_entity;
        return false;
    }
    if (!object.technique.empty())
    {
        core.Send_Message(object.entity, "ls", MSG_MODEL_SET_TECHNIQUE, object.technique.c_str());
    }
    if (!object.animation.empty())
    {
        // the model keeps going without its skeleton, in its pose of the file
        if (core.Send_Message(object.entity, "ls", MSG_MODEL_LOAD_ANI, object.animation.c_str()) == 0)
        {
            core.Trace("Scene: no animation %s for \"%s\"", object.animation.c_str(), object.name.c_str());
        }
        else if (!object.action.empty() &&
                 core.Send_Message(object.entity, "lls", MSG_MODEL_PLAY_ACTION, 0, object.action.c_str()) == 0)
        {
            core.Trace("Scene: no action %s for \"%s\"", object.action.c_str(), object.name.c_str());
        }
    }
    core.AddToLayer(EXECUTE, object.entity, kPriority);
    if (!object.hidden)
    {
        core.AddToLayer(REALIZE, object.entity, kPriority);
    }
    WritePlace(object);
    return true;
}

void Despawn(SceneObject &object)
{
    StopSound(object);
    if (core.GetEntityPointerSafe(object.entity) != nullptr)
    {
        core.EraseEntity(object.entity);
    }
    object.entity = invalid_entity;
}

void ReadPlace(SceneObject &object)
{
    if (MODEL *model = Model(object.entity))
    {
        std::copy(model->mtx.matrix, model->mtx.matrix + 16, object.matrix);
    }
}

void WritePlace(const SceneObject &object)
{
    if (object.voice != 0)
    {
        if (VSoundService *sounds = Sounds())
        {
            const float position[3] = {object.matrix[12], object.matrix[13], object.matrix[14]};
            sounds->SoundSet3DParam(object.voice, SM_POSITION, position);
        }
    }
    if (MODEL *model = Model(object.entity))
    {
        std::copy(object.matrix, object.matrix + 16, model->mtx.matrix);
    }
}

const SceneObject *FindMarker(std::string_view name)
{
    for (const SceneObject &object : CurrentScene().objects)
    {
        if (object.IsPlainMarker() && object.name == name)
        {
            return &object;
        }
    }
    return nullptr;
}

SceneObject *FindObject(std::string_view name)
{
    for (SceneObject &object : CurrentScene().objects)
    {
        if (object.name == name)
        {
            return &object;
        }
    }
    return nullptr;
}

bool PlayAction(SceneObject &object, const std::string &action)
{
    if (Model(object.entity) == nullptr || object.animation.empty() ||
        core.Send_Message(object.entity, "lls", MSG_MODEL_PLAY_ACTION, 0, action.c_str()) == 0)
    {
        return false;
    }
    // the editor sees a changed object and makes it anew on Stop, with the action of the file
    object.action = action;
    return true;
}

bool Show(SceneObject &object, bool show)
{
    if (object.IsSound())
    {
        if (show && object.voice == 0)
        {
            PlaySound(object);
        }
        else if (!show)
        {
            StopSound(object);
        }
    }
    else if (Model(object.entity) != nullptr)
    {
        if (show && object.hidden)
        {
            core.AddToLayer(REALIZE, object.entity, kPriority);
        }
        else if (!show && !object.hidden)
        {
            core.RemoveFromLayer(REALIZE, object.entity);
        }
    }
    else
    {
        return false;
    }
    object.hidden = !show;
    return true;
}

void Move(SceneObject &object, float x, float y, float z, float ay)
{
    // the axis z looks along (sin ay, 0, cos ay), as GetSceneMarker gives it
    const float s = std::sin(ay);
    const float c = std::cos(ay);
    const float matrix[16] = {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, x, y, z, 1};
    std::copy(matrix, matrix + 16, object.matrix);
    WritePlace(object);
}

size_t CheckTriggers(const std::string &who, float x, float y, float z)
{
    std::set<std::string> &was = Inside()[who];
    std::set<std::string> now;
    for (const SceneObject &object : CurrentScene().objects)
    {
        if (object.Contains(x, y, z))
        {
            now.insert(object.name);
        }
    }
    // posted, not sent: the handlers run after the script that asked, not inside its call
    for (const std::string &name : was)
    {
        if (now.count(name) == 0)
        {
            core.PostEvent("SceneTriggerExit", 0, "ss", name.c_str(), who.c_str());
        }
    }
    for (const std::string &name : now)
    {
        if (was.count(name) == 0)
        {
            core.PostEvent("SceneTriggerEnter", 0, "ss", name.c_str(), who.c_str());
        }
    }
    was = std::move(now);
    return was.size();
}

std::optional<size_t> LoadScene(std::string_view name)
{
    std::string error;
    auto objects = ReadScene(name, error);
    if (!objects)
    {
        core.Trace("Scene: %s: %s", std::string(name).c_str(), error.c_str());
        return std::nullopt;
    }
    ClearScene();
    Scene &scene = CurrentScene();
    scene.name = name;
    scene.objects = std::move(*objects);
    size_t made = 0;
    size_t markers = 0;
    size_t sounds = 0;
    size_t playing = 0;
    size_t triggers = 0;
    uint32_t id = 0;
    for (SceneObject &object : scene.objects)
    {
        object.id = ++id;
        const bool spawned = Spawn(object);
        made += spawned && !object.marker ? 1 : 0;
        playing += spawned && object.IsSound() ? 1 : 0;
        sounds += object.IsSound() ? 1 : 0;
        markers += object.IsPlainMarker() ? 1 : 0;
        triggers += object.IsTrigger() ? 1 : 0;
    }
    scene.loaded = scene.objects;
    core.Trace("Scene: %s, %zu models of %zu, %zu markers, %zu sounds of %zu, %zu triggers", std::string(name).c_str(),
               made, scene.objects.size() - markers - sounds - triggers, markers, playing, sounds, triggers);
    return made;
}

void ClearScene()
{
    Scene &scene = CurrentScene();
    for (SceneObject &object : scene.objects)
    {
        Despawn(object);
    }
    scene = {};
    Inside().clear();
}

} // namespace storm::scene
