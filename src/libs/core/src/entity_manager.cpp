#include "entity_manager.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <ranges>
#include <type_traits>

#include "storm/editor/engine_editor.hpp"
#include "storm/editor/storm_imgui.hpp"
#include "storm/layers.hpp"
#include "string_compare.hpp"
#include "vma.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <imgui.h>
#include <imgui_internal.h>

namespace
{

using invalid_entity_idx_t = std::integral_constant<size_t, std::numeric_limits<size_t>::max()>;

const char *ClassName(uint32_t hash)
{
    for (const auto &c : __STORM_CLASSES_REGISTRY)
    {
        if (c->GetHash() == hash)
        {
            return c->GetName();
        }
    }
    return nullptr;
}

// One value of an attribute: a number drags, the rest is text, set on Enter. The entity hears of
// the change as of a script assignment, through AttributeChanged.
void ShowAttributeValue(Entity &entity, ATTRIBUTES &attr, const char *label)
{
    const std::string &value = attr.GetValue();
    char *end = nullptr;
    float number = std::strtof(value.c_str(), &end);
    const bool numeric = !value.empty() && end != nullptr && *end == '\0';
    bool changed = false;
    const bool integral_value = numeric && value.find_first_of(".eE") == std::string::npos;
    if (integral_value && ImStristr(label, nullptr, "color", nullptr) != nullptr)
    {
        // a colour of the scripts is argb() packed into an int
        const auto argb = static_cast<uint32_t>(std::strtoll(value.c_str(), nullptr, 10));
        float rgba[4] = {((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f,
                         (argb >> 24) / 255.0f};
        if (ImGui::ColorEdit4(label, rgba, ImGuiColorEditFlags_AlphaPreviewHalf))
        {
            const auto channel = [&](int i) { return static_cast<uint32_t>(std::lround(rgba[i] * 255.0f)); };
            const uint32_t packed = channel(3) << 24 | channel(0) << 16 | channel(1) << 8 | channel(2);
            attr.SetValue(std::to_string(static_cast<int32_t>(packed)));
            changed = true;
        }
    }
    else if (numeric)
    {
        const bool integral = value.find_first_of(".eE") == std::string::npos;
        const float speed = integral ? 1.0f : std::max(0.001f, std::abs(number) * 0.01f);
        if (ImGui::DragFloat(label, &number, speed, 0.0f, 0.0f, integral ? "%.0f" : "%.3f"))
        {
            attr.SetValue(integral ? std::to_string(std::lround(number)) : fmt::format("{}", number));
            changed = true;
        }
    }
    else
    {
        std::string text = value;
        if (ImGui::InputText(label, &text, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            attr.SetValue(text);
            changed = true;
        }
    }
    if (changed)
    {
        entity.AttributeChanged(&attr);
    }
}

// The attributes of an entity as a tree; a filter keeps the branches with a matching name
void ShowAttributes(Entity &entity, ATTRIBUTES &attr, const ImGuiTextFilter &filter)
{
    for (uint32_t i = 0; i < attr.GetAttributesNum(); ++i)
    {
        ATTRIBUTES *child = attr.GetAttributeClass(i);
        if (child == nullptr)
        {
            continue;
        }
        ImGui::PushID(child);
        const char *name = child->GetThisName();
        if (child->GetAttributesNum() > 0)
        {
            const ImGuiTreeNodeFlags flags = filter.IsActive() ? ImGuiTreeNodeFlags_DefaultOpen : 0;
            if (ImGui::TreeNodeEx("##node", flags, "%s", name))
            {
                // a branch made by a script assignment to its leaves keeps an empty value
                if (child->HasValue() && !child->GetValue().empty())
                {
                    ShowAttributeValue(entity, *child, "value");
                }
                ShowAttributes(entity, *child, filter);
                ImGui::TreePop();
            }
        }
        else if (filter.PassFilter(name))
        {
            if (child->HasValue())
            {
                ShowAttributeValue(entity, *child, name);
            }
            else
            {
                ImGui::TextDisabled("%s", name);
            }
        }
        ImGui::PopID();
    }
}

} // namespace

void EntityManager::EraseAndFree(EntityInternalData &data)
{
    for (layer_index_t i = 0; i < kMaxLayerNum; ++i)
    {
        if (data.mask >> i & 1)
        {
            RemoveFromLayer(i, data);
        }
    }
    delete data.ptr;
}

void EntityManager::MarkDeleted(EntityInternalData &data)
{
    data.state = kExists;
    const auto index = static_cast<entid_index_t>(data.id);
    deletedIndices_.push(index);
}

hash_t EntityManager::GetClassCode(const entid_t id) const
{
    const auto idx = GetEntityDataIdx(id);
    if (!EntIdxValid(idx))
    {
        return {};
    }

    return entities_[idx].hash;
}

void EntityManager::AddToLayer(const layer_index_t index, EntityInternalData &data, const priority_t priority)
{
    assert(index < kMaxLayerNum);

    // set info about layer
    data.mask |= 1 << index;
    data.priorities[index] = priority;

    auto &layer = layers_[index];
    auto &priorities = layer.priorities;
    auto &entity_ids = layer.entity_ids;

    // duplicate assert
    assert(std::ranges::find(entity_ids, data.id) == std::end(entity_ids));

    const auto targetIdx = std::distance(std::begin(priorities), std::ranges::upper_bound(priorities, priority));

    priorities.insert(std::begin(priorities) + targetIdx, priority);
    entity_ids.insert(std::begin(entity_ids) + targetIdx, data.id);
}

void EntityManager::RemoveFromLayer(const layer_index_t index, EntityInternalData &data)
{
    assert(index < kMaxLayerNum);

    auto &mask = data.mask;

    const auto priority = data.priorities[index];
    const auto id = data.id;

    auto &layer = layers_[index];
    auto &priorities = layer.priorities;
    auto &entity_ids = layer.entity_ids;

    const auto ssize = std::ssize(priorities);

    const auto lowerIdx = std::distance(std::begin(priorities), std::ranges::lower_bound(priorities, priority));
    assert(lowerIdx < ssize);

    // look through this priority only
    for (auto i = lowerIdx; i < ssize && priorities[i] == priority; ++i)
    {
        if (entity_ids[i] == id)
        {
            priorities.erase(std::begin(priorities) + i);
            entity_ids.erase(std::begin(entity_ids) + i);
            break;
        }
    }

    // clear layer flag
    mask &= ~(1 << index);
}

entid_t EntityManager::CreateEntity(const char *name, ATTRIBUTES *attr)
{
    // Find VMA
    const auto hash = MakeHashValue(name);
    if (hash == 0)
    {
        throw std::runtime_error("CreateEntity null hash");
    }
    VMA *pClass = nullptr;
    for (const auto &c : __STORM_CLASSES_REGISTRY)
    {
        if (c->GetHash() == hash && storm::iEquals(name, c->GetName()))
        {
            pClass = c;
            break;
        }
    }
    if (pClass == nullptr)
    {
        spdlog::error("Invalid entity name '{}'", name);
        return invalid_entity;
    }

    // Create entity
    auto *ptr = static_cast<Entity *>(pClass->CreateClass());
    if (ptr == nullptr)
    {
        spdlog::error("CreateClass returned nullptr for '{}'", name);
        return invalid_entity;
    }

    // Init entity
    // set id first
    const auto id = InsertEntity(ptr, hash);
    ptr->data_.id = id;
    ptr->AttributesPointer = attr;

    // add to cache
    cache_.UpdateAdd(hash, id);

    // then init
    if (!ptr->Init())
    {
        // remove from cache
        cache_.UpdateErase(hash, id);

        // delete if fail
        const auto index = static_cast<entid_index_t>(id);
        MarkDeleted(entities_[index]);
        spdlog::error("Failed to initialize entity for '{}'", name);
        return invalid_entity;
    }

    return id;
}

void EntityManager::EraseEntity(const entid_t id)
{
    const auto idx = GetEntityDataIdx(id);
    if (!EntIdxValid(idx))
    {
        return;
    }

    auto &data = entities_[idx];

    // check if already erased
    if (data.state != kValid)
    {
        return;
    }

    // check id mismatch
    if (data.ptr->GetId() != id)
    {
        return;
    }

    // remove from cache
    cache_.UpdateErase(data.hash, id);
    MarkDeleted(data);
}

void EntityManager::EraseAll()
{
    for (auto it = std::begin(entities_); it != std::end(entities_); ++it)
    {
        it->state = kNotExists;
        delete it->ptr;
    }

    // clear containers
    for (auto &layer : layers_)
    {
        layer.entity_ids.clear();
        layer.priorities.clear();
    }
    entities_.clear();
    freeIndices_.clear();
    deletedIndices_.clear();

    // clear cache
    cache_.Clear();

    previousStamp_ = 0;
}

entptr_t EntityManager::GetEntityPointer(const entid_t id) const
{
    const auto idx = GetEntityDataIdx(id);
    if (!EntIdxValid(idx))
    {
        return nullptr;
    }

    return entities_[idx].ptr;
}

entity_container_cref EntityManager::GetEntityIds(const layer_type_t type) const
{
    // TODO: introduce cache or invalidation-proof iterator?
    static std::vector<entid_t> result;
    result.reserve(entities_.size());
    result.clear();

    for (auto it = std::begin(layers_); it != std::end(layers_); ++it)
    {
        if (it->frozen)
        {
            continue;
        }

        if (it->type == type)
        {
            std::ranges::copy(it->entity_ids, std::back_inserter(result));
        }
    }

    // TODO: Investigate:
    // Some entities are referenced in multiple layers of same type
    // therefore there are getting processed multiple times.
    // This negatively affects any logic that make use of delta time.
    // Removing duplicates would resolve such issues, but
    // 1) it will increase the complexity drastically
    // 2) it's not obvious what duplicate to remove as they might
    // have different priorities inside corresponding layers.
    return result;
}

entity_container_cref EntityManager::GetEntityIds(const char *name) const
{
    return GetEntityIds(MakeHashValue(name));
}

entity_container_cref EntityManager::GetEntityIds(const uint32_t hash) const
{
    static const std::vector<entid_t> null;

    // if contains, it is already valid
    // otherwise let's fill the cache in
    bool empty = !cache_.Contains(hash);
    if (empty)
    {
        for (auto it = std::begin(entities_); it != std::end(entities_); ++it)
        {
            if (it->hash == hash && it->state == kValid)
            {
                cache_.Add(hash, it->id);
                empty = false;
            }
        }
    }

    return empty ? null : cache_.Get(hash);
}

entity_container_cref EntityManager::GetEntityIds(const layer_index_t index) const
{
    assert(index < kMaxLayerNum);

    return layers_[index].entity_ids;
}

entid_t EntityManager::GetEntityId(const char *name) const
{
    return GetEntityId(MakeHashValue(name));
}

bool EntityManager::IsEntityValid(entid_t id) const
{
    const auto index = GetEntityDataIdx(id);
    return EntIdxValid(index) && entities_[index].state == kValid;
}

entid_t EntityManager::GetEntityId(const uint32_t hash) const
{
    for (auto it = std::begin(entities_); it != std::end(entities_); ++it)
    {
        if (it->hash == hash && it->state == kValid)
        {
            return it->id;
        }
    }

    return invalid_entity;
}

bool EntityManager::EntIdxValid(const size_t idx)
{
    return idx != invalid_entity_idx_t{};
}

entid_t EntityManager::CalculateEntityId(const size_t idx)
{
    // assert for idx exceeding 2^32
    assert(static_cast<size_t>(static_cast<uint32_t>(idx)) == idx);

    const auto stamp = ++previousStamp_;
    const auto id = static_cast<entid_t>(stamp) << 32 | idx;

    return id;
}

layer_type_t EntityManager::GetLayerType(const layer_index_t index) const
{
    assert(index < kMaxLayerNum);

    return layers_[index].type;
}

void EntityManager::SetLayerType(const layer_index_t index, const layer_type_t type)
{
    assert(index < kMaxLayerNum);

    layers_[index].type = type;
}

void EntityManager::SetLayerFrozen(const layer_index_t index, const bool freeze)
{
    assert(index < kMaxLayerNum);

    layers_[index].frozen = freeze;
}

bool EntityManager::IsLayerFrozen(const layer_index_t index) const
{
    assert(index < kMaxLayerNum);

    return layers_[index].frozen;
}

void EntityManager::AddToLayer(layer_index_t layer_idx, entid_t entity_id, priority_t priority)
{

    if (const auto entity_idx = GetEntityDataIdx(entity_id); EntIdxValid(entity_idx))
    {
        if (!(entities_[entity_idx].mask >> layer_idx & 1))
        {
            AddToLayer(layer_idx, entities_[entity_idx], priority);
        }
    }
}

void EntityManager::RemoveFromLayer(layer_index_t layer_idx, entid_t entity_id)
{

    if (const auto entity_idx = GetEntityDataIdx(entity_id); EntIdxValid(entity_idx))
    {
        if (entities_[entity_idx].mask >> layer_idx & 1)
        {
            RemoveFromLayer(layer_idx, entities_[entity_idx]);
        }
    }
}

void EntityManager::NewLifecycle()
{
    // deletedSize will increment while erasing entities. this is safe for std::array iteration
    while (!deletedIndices_.empty())
    {
        const auto entity_idx = deletedIndices_.top();
        assert(entity_idx < entities_.size());
        deletedIndices_.pop();

        auto &data = entities_[entity_idx];
        // release
        EraseAndFree(data);
        // erase entity data
        data = {};
        // push free index to stack instead of shifting
        freeIndices_.push(entity_idx);
    }
}

size_t EntityManager::GetEntityDataIdx(const entid_t id) const
{
    if (id == invalid_entity)
    {
        return invalid_entity_idx_t{};
    }

    // check bounds
    const auto index = static_cast<entid_index_t>(id);
    if (index >= std::size(entities_))
    {
        return invalid_entity_idx_t{};
    }

    // check validity
    auto &data = entities_[index];
    // This check was intentionally removed to workaround some rare corner cases that are not handled properly.
    //
    // Here's an example of such a corner case that leads to crash due to missing sanity checks:
    // Location entity is being destroyed (marked as invalid) from scripts. Therefore in the next frame its
    // destructor is called, invoking Supervisor destructor which in turn destroys (marks as invalid) all the
    // Player entities.
    // Here's how it looks like:
    // + - exists and operates; ! - invalidated (marked as invalid); x - deleted
    //               |  Frame 0 | Frame 1 | Frame 2 | Frame 3 |
    // Location      |     +    |    !    |    x    |    x    |
    // Supervisor    |     +    |    +    |    x    |    x    |
    // Players[s]    |     +    |    +    |    !    |    x    |
    //
    // As it's shown, while Player[s] exists Location is always marked as invalid and doesn't return its pointer.
    // At this point using an existing pointer to Location without check for validity would work (despite the
    // inadmissibility of such); but calling GetEntityPointer legally returns nullptr due to `valid` flag unset.
    // This case is exacerbated by the fact that in this corner case calls to `Player` are done via PostEvent.
    //
    // Although in this case it would be more consistent with introducing sanity checks over the code,
    // such issues are very difficult to catch. And since this workaround doesn't introduce any harm except for
    // encouraging inconsistency code, it makes sense to left it as a lifebuoy for the existing issues.
    // if (data.valid == false)
    // {
    //     return invalid_entity_idx_t{};
    // }
    if (data.state == kNotExists)
    {
        return invalid_entity_idx_t{};
    }

    // check if already replaced by another entity
    if (data.ptr->GetId() != id)
    {
        return invalid_entity_idx_t{};
    }

    return index;
}

entid_t EntityManager::InsertEntity(entptr_t ptr, hash_t hash)
{
    entid_t id;
    if (freeIndices_.empty())
    {
        const entid_index_t index = std::size(entities_);
        id = CalculateEntityId(index);
        entities_.emplace_back(EntityInternalData{{}, {}, kValid, ptr, id, hash});
    }
    else
    {
        const entid_index_t index = freeIndices_.top();
        id = CalculateEntityId(index);
        freeIndices_.pop();
        entities_[index] = {{}, {}, kValid, ptr, id, hash};
    }

    return id;
}

void EntityManager::ForEachEntity(const std::function<void(entptr_t)> &f)
{
    std::ranges::for_each(entities_, [&](const EntityInternalData &data) {
        if (data.state == kValid)
        {
            if (const auto entity_ptr = GetEntityPointer(data.id))
            {
                f(entity_ptr);
            }
        }
    });
}

void EntityManager::ShowEditor(bool &active)
{
    if (ImGui::Begin("Entities", &active, 0))
    {
        ImGui::TextFmt("There are currently {} entities", entities_.size() - freeIndices_.size());

        static entid_t selected_entity = invalid_entity;
        static ImGuiTextFilter class_filter;
        class_filter.Draw("Class");
        // the selection outlives the run by its class: the first entity of it is picked again
        const std::string selected_class = storm::editor::EngineEditor::GetSetting("Entities.Class");
        if (GetEntityPointer(selected_entity) == nullptr && !selected_class.empty())
        {
            for (const auto &data : entities_)
            {
                const char *name = data.state == kValid ? ClassName(data.hash) : nullptr;
                if (name != nullptr && selected_class == name)
                {
                    selected_entity = data.id;
                    break;
                }
            }
        }

        if (ImGui::BeginTable("Entities", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable) )
        {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableHeadersRow();

            std::ranges::for_each(entities_, [&](const EntityInternalData &data) {
                const char *class_name = data.state == kValid ? ClassName(data.hash) : nullptr;
                if (data.state == kValid && class_filter.PassFilter(class_name != nullptr ? class_name : ""))
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    std::string label = std::to_string(data.id);
                    if (ImGui::Selectable(label.c_str(), data.id == selected_entity, ImGuiSelectableFlags_SpanAllColumns) )
                    {
                        selected_entity = data.id;
                        storm::editor::EngineEditor::SetSetting("Entities.Class", class_name != nullptr ? class_name : "");
                    }
                    ImGui::TableNextColumn();

                    if (class_name != nullptr)
                    {
                        ImGui::Text("%s", class_name);
                    }
                }
            });

            ImGui::EndTable();
        }

        if (ImGui::Begin("Entity Properties") )
        {
            Entity *entity = GetEntityPointer(selected_entity);
            if (entity != nullptr)
            {
                // Show layer info
                const auto entity_idx = GetEntityDataIdx(entity->GetId() );
                EntityInternalData &data = entities_[entity_idx];
                if (ImGui::TreeNode("Layers") ) {
                    if (ImGui::BeginTable("layer_selection", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders) ) {
                        size_t i = 0;
                        for (const auto &layer : layers_) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            std::string label = fmt::format("{}", storm::GetLayerName(i) );
                            bool selected = ( (data.mask >> i) & 1) == 1;
                            if (ImGui::Selectable(label.c_str(), &selected) ) {
                                if (selected) {
                                    AddToLayer(i, selected_entity, data.priorities[i]);
                                }
                                else {
                                    RemoveFromLayer(i, selected_entity);
                                }
                            }
                            ImGui::TableNextColumn();
                            ImGui::InputScalar("##priority", ImGuiDataType_U32, &data.priorities[i]);
                            ++i;
                        }
                        ImGui::EndTable();
                    }
                }
                if (const char *class_name = ClassName(data.hash))
                {
                    ImGui::TextFmt("{} {}", class_name, entity->GetId());
                }
                // the attributes scripts give the entity, edited live
                if (entity->AttributesPointer != nullptr && ImGui::TreeNodeEx("Attributes", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    // the filter is kept between runs too, it opens the branches it matches
                    static ImGuiTextFilter attribute_filter(storm::editor::EngineEditor::GetSetting("Entities.Filter").c_str());
                    if (attribute_filter.Draw("Name"))
                    {
                        storm::editor::EngineEditor::SetSetting("Entities.Filter", attribute_filter.InputBuf);
                    }
                    ShowAttributes(*entity, *entity->AttributesPointer, attribute_filter);
                    ImGui::TreePop();
                }
                // Show custom editor
                entity->ShowEditor();
            }
        }
        ImGui::End();
    }
    // End goes with every Begin, also when the window is collapsed and Begin is false
    ImGui::End();
}
