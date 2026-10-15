// The script function of the scenes: int LoadScene(string name) puts the models of
// resource/scenes/<name>.scene into the game and gives how many there are, -1 for no scene.

#include "storm/scene/scene.hpp"

#include "core.h"
#include "s_import_func.h"
#include "script_libriary.h"
#include "v_s_stack.h"
#include "vma.hpp"

namespace storm::scene
{
namespace
{

class SceneLibrary : public SCRIPT_LIBRIARY
{
  public:
    bool Init() override;
};

CREATE_SCRIPTLIBRIARY(SceneLibrary)

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

} // namespace
} // namespace storm::scene
