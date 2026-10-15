// The script functions of the scenes: int LoadScene(string name) puts the models of
// resource/scenes/<name>.scene into the game and gives how many there are, -1 for no scene;
// int GetSceneMarker(string name, ref place) gives a marker of the scene loaded as place.x, .y, .z and
// .ay (the turn about y, as MSG_CHARACTER_TELEPORT_AY takes it), 0 when there is no such marker.
// GetSceneMarker is a library of its own, SceneMarkers: a library wants every function it sets declared
// in the script (compiler.cpp, SetScriptFunction), so a script with LoadScene alone keeps working.

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
