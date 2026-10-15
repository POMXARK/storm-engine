// The script functions of the scenes: int LoadScene(string name) puts the models of
// resource/scenes/<name>.scene into the game and gives how many there are, -1 for no scene;
// int GetSceneMarker(string name, ref place) gives a marker of the scene loaded as place.x, .y, .z and
// .ay (the turn about y, as MSG_CHARACTER_TELEPORT_AY takes it), 0 when there is no such marker.
// GetSceneMarker is a library of its own, SceneMarkers: a library wants every function it sets declared
// in the script (compiler.cpp, SetScriptFunction), so a script with LoadScene alone keeps working.
// SceneObjects changes the objects of the scene loaded while the game runs and tells who is in its zones:
// int SceneObjectAction(string name, string action), int SceneObjectShow(string name, int show),
// int SceneObjectMove(string name, float x, float y, float z, float ay) give 1 when the object took it;
// int CheckSceneTriggers(string who, float x, float y, float z) posts SceneTriggerEnter and SceneTriggerExit
// (string trigger, string who) and gives in how many zones who is.

#include "storm/scene/scene.hpp"

#include "core.h"
#include "s_import_func.h"
#include "script_libriary.h"
#include "v_s_stack.h"
#include "vma.hpp"

#include <cmath>

namespace storm::scene
{
namespace
{

class SceneLibrary : public SCRIPT_LIBRIARY
{
  public:
    bool Init() override;
};

class SceneMarkers : public SCRIPT_LIBRIARY
{
  public:
    bool Init() override;
};

CREATE_SCRIPTLIBRIARY(SceneLibrary)
CREATE_SCRIPTLIBRIARY(SceneMarkers)

class SceneObjects : public SCRIPT_LIBRIARY
{
  public:
    bool Init() override;
};

CREATE_SCRIPTLIBRIARY(SceneObjects)

// the arguments come off the stack last first
const char *PopString(VS_STACK *stack)
{
    auto *data = static_cast<VDATA *>(stack->Pop());
    return data != nullptr ? data->GetString() : nullptr;
}

float PopFloat(VS_STACK *stack)
{
    auto *data = static_cast<VDATA *>(stack->Pop());
    float value = 0.f;
    if (data != nullptr)
    {
        data->Get(value);
    }
    return value;
}

int32_t PopInt(VS_STACK *stack)
{
    auto *data = static_cast<VDATA *>(stack->Pop());
    int32_t value = 0;
    if (data != nullptr)
    {
        data->Get(value);
    }
    return value;
}

uint32_t PushInt(VS_STACK *stack, int32_t value)
{
    VDATA *result = stack->Push();
    if (result == nullptr)
    {
        return IFUNCRESULT_FAILED;
    }
    result->Set(value);
    return IFUNCRESULT_OK;
}

SceneObject *Object(const char *name)
{
    return name != nullptr ? FindObject(name) : nullptr;
}

uint32_t LoadSceneImpl(VS_STACK *stack)
{
    auto *name = static_cast<VDATA *>(stack->Pop());
    const char *text = name != nullptr ? name->GetString() : nullptr;
    const std::optional<size_t> made = text != nullptr ? LoadScene(text) : std::nullopt;
    VDATA *result = stack->Push();
    if (result == nullptr)
    {
        return IFUNCRESULT_FAILED;
    }
    result->Set(made ? static_cast<int32_t>(*made) : -1);
    return IFUNCRESULT_OK;
}

uint32_t GetSceneMarkerImpl(VS_STACK *stack)
{
    auto *place = static_cast<VDATA *>(stack->Pop());
    auto *name = static_cast<VDATA *>(stack->Pop());
    const char *text = name != nullptr ? name->GetString() : nullptr;
    const SceneObject *marker = text != nullptr ? FindMarker(text) : nullptr;
    ATTRIBUTES *attributes = place != nullptr ? place->GetVarPointer()->GetAClass() : nullptr;
    if (marker != nullptr && attributes != nullptr)
    {
        const float *m = marker->matrix;
        attributes->SetAttributeUseFloat("x", m[12]);
        attributes->SetAttributeUseFloat("y", m[13]);
        attributes->SetAttributeUseFloat("z", m[14]);
        // the axis z of the marker is where it looks: (sin ay, 0, cos ay)
        attributes->SetAttributeUseFloat("ay", std::atan2(m[8], m[10]));
    }
    VDATA *result = stack->Push();
    if (result == nullptr)
    {
        return IFUNCRESULT_FAILED;
    }
    result->Set(marker != nullptr && attributes != nullptr ? 1 : 0);
    return IFUNCRESULT_OK;
}

uint32_t SceneObjectActionImpl(VS_STACK *stack)
{
    const char *action = PopString(stack);
    SceneObject *object = Object(PopString(stack));
    return PushInt(stack, object != nullptr && action != nullptr && PlayAction(*object, action) ? 1 : 0);
}

uint32_t SceneObjectShowImpl(VS_STACK *stack)
{
    const int32_t show = PopInt(stack);
    SceneObject *object = Object(PopString(stack));
    return PushInt(stack, object != nullptr && Show(*object, show != 0) ? 1 : 0);
}

uint32_t SceneObjectMoveImpl(VS_STACK *stack)
{
    const float ay = PopFloat(stack);
    const float z = PopFloat(stack);
    const float y = PopFloat(stack);
    const float x = PopFloat(stack);
    SceneObject *object = Object(PopString(stack));
    if (object != nullptr)
    {
        Move(*object, x, y, z, ay);
    }
    return PushInt(stack, object != nullptr ? 1 : 0);
}

uint32_t CheckSceneTriggersImpl(VS_STACK *stack)
{
    const float z = PopFloat(stack);
    const float y = PopFloat(stack);
    const float x = PopFloat(stack);
    const char *who = PopString(stack);
    return PushInt(stack, who != nullptr ? static_cast<int32_t>(CheckTriggers(who, x, y, z)) : 0);
}

void SetFunction(const char *name, int32_t arguments, SIMPORTFUNC address)
{
    IFUNCINFO info{};
    info.nArguments = arguments;
    info.pFuncName = name;
    info.pReturnValueName = "int";
    info.pFuncAddress = address;
    core.SetScriptFunction(&info);
}

bool SceneObjects::Init()
{
    SetFunction("SceneObjectAction", 2, SceneObjectActionImpl);
    SetFunction("SceneObjectShow", 2, SceneObjectShowImpl);
    SetFunction("SceneObjectMove", 5, SceneObjectMoveImpl);
    SetFunction("CheckSceneTriggers", 4, CheckSceneTriggersImpl);
    return true;
}

bool SceneLibrary::Init()
{
    IFUNCINFO info{};
    info.nArguments = 1;
    info.pFuncName = "LoadScene";
    info.pReturnValueName = "int";
    info.pFuncAddress = LoadSceneImpl;
    core.SetScriptFunction(&info);
    return true;
}

bool SceneMarkers::Init()
{
    IFUNCINFO info{};
    info.nArguments = 2;
    info.pFuncName = "GetSceneMarker";
    info.pReturnValueName = "int";
    info.pFuncAddress = GetSceneMarkerImpl;
    core.SetScriptFunction(&info);
    return true;
}

} // namespace
} // namespace storm::scene
