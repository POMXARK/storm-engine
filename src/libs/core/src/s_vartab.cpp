#include "s_vartab.h"

VarInfo::VarInfo() : name(), type(TVOID), value(), elements(0), segment_id(INVALID_SEGMENT_ID)
{
}

VarInfo::VarInfo(const VarInfo &other)
    : name(other.name), type(other.type), value(), elements(other.elements), segment_id(other.segment_id)
{
}

VarInfo &VarInfo::operator=(const VarInfo &other)
{
    if (this != &other)
    {
        name = other.name;
        type = other.type;
        value = {};
        elements = other.elements;
        segment_id = other.segment_id;
    }

    return *this;
}

VarTable::~VarTable()
{
    Release();
}

size_t VarTable::AddVar(const VarInfo &vi)
{
    if (vi.name.empty())
    {
        return INVALID_VAR_CODE;
    }

    auto result = hash_table_.try_emplace(vi.name, vars_.size()); // find or create index
    auto var_index = result.first->second;
    bool is_new = result.second;

    if (is_new) // newly created var, add to table
    {
        vars_.push_back(vi);
    }

    auto &var = vars_[var_index];

    if (!is_new)
    {
        if (var.segment_id != INVALID_SEGMENT_ID) // variable already loaded
        {
            return INVALID_VAR_CODE;
        }

        var = vi; // variable exists, but was unloaded, copy data
    }

    var.value = std::make_unique<DATA>();
    var.value->SetVCompiler(vc_);
    var.value->SetType(var.type, var.elements);
    var.value->SetGlobalVarTableIndex(var_index); // todo change to size_t

    return var_index;
}

const VarInfo *VarTable::GetVar(size_t var_index) const
{
    if (var_index >= vars_.size())
    {
        return nullptr;
    }

    if (vars_[var_index].segment_id == INVALID_SEGMENT_ID)
    {
        return nullptr;
    }

    return &vars_[var_index];
}

const VarInfo *VarTable::GetVarX(size_t var_index) const
{
    if (var_index >= vars_.size())
    {
        return nullptr;
    }

    return &vars_[var_index];
}

void VarTable::InvalidateBySegmentID(uint32_t segment_id)
{
    for (auto &vi : vars_)
    {
        if (vi.segment_id == segment_id)
        {
            vi.segment_id = INVALID_SEGMENT_ID; // hash is not deleted from table
        }
    }
}

std::vector<std::pair<size_t, VarInfo>> VarTable::Detach(uint32_t segment_id)
{
    std::vector<std::pair<size_t, VarInfo>> detached;
    for (size_t n = 0; n < vars_.size(); n++)
    {
        if (vars_[n].segment_id == segment_id)
        {
            VarInfo vi = vars_[n];
            vi.value = std::move(vars_[n].value);
            detached.emplace_back(n, std::move(vi));
        }
    }
    return detached;
}

void VarTable::Attach(std::vector<std::pair<size_t, VarInfo>> &detached, bool restore)
{
    for (auto &[index, vi] : detached)
    {
        auto &var = vars_[index];
        if (restore)
        {
            var = vi;
            var.value = std::move(vi.value);
        }
        // an array keeps its runtime size, SetArraySize may have changed it
        else if (var.segment_id != INVALID_SEGMENT_ID && var.type == vi.type && var.IsArray() == vi.IsArray())
        {
            var.elements = vi.elements;
            var.value = std::move(vi.value);
        }
        else
        {
            retired_.push_back(std::move(vi.value));
        }
    }
    detached.clear();
}

size_t VarTable::FindVar(const std::string &var_name) const
{
    auto result = hash_table_.find(var_name);

    if (result == hash_table_.end())
    {
        return INVALID_VAR_CODE;
    }

    return result->second;
}

bool VarTable::SetElementsNum(size_t var_index, size_t elements_num)
{
    if (var_index >= vars_.size())
    {
        return false;
    }

    if (!vars_[var_index].value->IsArray())
    {
        return false;
    }

    vars_[var_index].elements = elements_num;
    return true;
}

void VarTable::Release()
{
    vars_.clear();
    hash_table_.clear();
    retired_.clear();
}
