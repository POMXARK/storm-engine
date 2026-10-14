#ifdef STORM_SOUND_MINIAUDIO

#include "audio_backend.h"

#include <memory>
#include <string>
#include <vector>

#define STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c>

#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c>

namespace storm::audio
{

namespace
{

// FMOD defaults for 3D sounds
constexpr float kDefaultMinDistance = 1.0f;
constexpr float kDefaultMaxDistance = 10000.0f;
constexpr int kDefaultPriority = 128;

// engine space is left-handed (+Z forward), miniaudio is right-handed (-Z forward)
ma_vec3f ToMa(const Vector3 &v)
{
    return ma_vec3f{v.x, v.y, -v.z};
}

Vector3 FromMa(const ma_vec3f &v)
{
    return Vector3{v.x, v.y, -v.z};
}

Result Check(ma_result result)
{
    return result == MA_SUCCESS ? Result::Ok : Result::Failed;
}

} // namespace

struct SoundData
{
    std::string path;
    SoundKind kind{};
    bool looped = false;
    // samples are decoded once into the resource manager and played through copies of this prototype
    ma_sound prototype{};
    bool has_prototype = false;
};

namespace
{

struct ChannelSlot
{
    ma_sound sound{};
    SoundData *source = nullptr;
    uint32_t generation = 1;
    bool active = false;
    bool paused = false;
    int priority = kDefaultPriority;
};

struct Backend
{
    ma_engine engine{};
    ma_sound_group master{};
    std::vector<std::unique_ptr<ChannelSlot>> slots;
    std::vector<uint32_t> free_slots;
    std::vector<std::unique_ptr<SoundData>> sounds;
    size_t max_channels = 0;

    ChannelSlot *Get(uint32_t index, uint32_t generation)
    {
        if (generation == 0 || index >= slots.size())
        {
            return nullptr;
        }
        auto *slot = slots[index].get();
        return slot->active && slot->generation == generation ? slot : nullptr;
    }

    void Release(uint32_t index)
    {
        auto &slot = *slots[index];
        if (!slot.active)
        {
            return;
        }
        ma_sound_uninit(&slot.sound);
        slot.active = false;
        slot.source = nullptr;
        // never hand out generation 0, it marks an empty Channel
        if (++slot.generation == 0)
        {
            slot.generation = 1;
        }
        free_slots.push_back(index);
    }
};

Backend *backend = nullptr;

ChannelSlot *Lookup(uint32_t index, uint32_t generation)
{
    return backend ? backend->Get(index, generation) : nullptr;
}

} // namespace

const char *ResultString(Result result)
{
    switch (result)
    {
    case Result::Ok:
        return "ok";
    case Result::InvalidHandle:
        return "invalid channel handle";
    default:
        return "miniaudio call failed";
    }
}

// Channel ---------------------------------------------------------------------------------------------------------

#define GET_SLOT_OR_RETURN()                                                                                           \
    auto *slot = Lookup(index_, generation_);                                                                          \
    if (slot == nullptr)                                                                                               \
    {                                                                                                                  \
        return Result::InvalidHandle;                                                                                  \
    }

Result Channel::stop() const
{
    GET_SLOT_OR_RETURN();
    ma_sound_stop(&slot->sound);
    backend->Release(index_);
    return Result::Ok;
}

Result Channel::getPaused(bool &paused) const
{
    GET_SLOT_OR_RETURN();
    paused = slot->paused;
    return Result::Ok;
}

Result Channel::setPaused(bool paused) const
{
    GET_SLOT_OR_RETURN();
    slot->paused = paused;
    return Check(paused ? ma_sound_stop(&slot->sound) : ma_sound_start(&slot->sound));
}

Result Channel::isPlaying(bool &playing) const
{
    GET_SLOT_OR_RETURN();
    // like FMOD: a paused channel still counts as playing
    playing = !ma_sound_at_end(&slot->sound);
    return Result::Ok;
}

Result Channel::isVirtual(bool &is_virtual) const
{
    GET_SLOT_OR_RETURN();
    is_virtual = false;
    return Result::Ok;
}

Result Channel::getAudibility(float &audibility) const
{
    GET_SLOT_OR_RETURN();
    audibility = ma_sound_get_volume(&slot->sound);
    if (ma_sound_is_spatialization_enabled(&slot->sound))
    {
        const auto listener = ma_engine_listener_get_position(&backend->engine, 0);
        const auto pos = ma_sound_get_position(&slot->sound);
        const float dx = pos.x - listener.x;
        const float dy = pos.y - listener.y;
        const float dz = pos.z - listener.z;
        const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        const float min_distance = ma_sound_get_min_distance(&slot->sound);
        const float max_distance = ma_sound_get_max_distance(&slot->sound);
        float gain = 1.0f;
        if (distance >= max_distance)
        {
            gain = 0.0f;
        }
        else if (distance > min_distance && max_distance > min_distance)
        {
            gain = 1.0f - (distance - min_distance) / (max_distance - min_distance);
        }
        audibility *= gain;
    }
    return Result::Ok;
}

Result Channel::setVolume(float volume) const
{
    GET_SLOT_OR_RETURN();
    ma_sound_set_volume(&slot->sound, volume);
    return Result::Ok;
}

Result Channel::getVolume(float &volume) const
{
    GET_SLOT_OR_RETURN();
    volume = ma_sound_get_volume(&slot->sound);
    return Result::Ok;
}

Result Channel::setPitch(float pitch) const
{
    GET_SLOT_OR_RETURN();
    ma_sound_set_pitch(&slot->sound, pitch);
    return Result::Ok;
}

Result Channel::setPriority(int priority) const
{
    GET_SLOT_OR_RETURN();
    slot->priority = priority;
    return Result::Ok;
}

Result Channel::getPriority(int &priority) const
{
    GET_SLOT_OR_RETURN();
    priority = slot->priority;
    return Result::Ok;
}

Result Channel::setPositionMs(uint32_t position) const
{
    GET_SLOT_OR_RETURN();
    ma_uint32 sample_rate = 0;
    const auto result = ma_sound_get_data_format(&slot->sound, nullptr, nullptr, &sample_rate, nullptr, 0);
    if (result != MA_SUCCESS || sample_rate == 0)
    {
        return Result::Failed;
    }
    return Check(ma_sound_seek_to_pcm_frame(&slot->sound, static_cast<ma_uint64>(position) * sample_rate / 1000));
}

Result Channel::getPositionMs(uint32_t &position) const
{
    GET_SLOT_OR_RETURN();
    ma_uint32 sample_rate = 0;
    ma_uint64 cursor = 0;
    if (ma_sound_get_data_format(&slot->sound, nullptr, nullptr, &sample_rate, nullptr, 0) != MA_SUCCESS ||
        sample_rate == 0 || ma_sound_get_cursor_in_pcm_frames(&slot->sound, &cursor) != MA_SUCCESS)
    {
        position = 0;
        return Result::Failed;
    }
    position = static_cast<uint32_t>(cursor * 1000 / sample_rate);
    return Result::Ok;
}

Result Channel::getLengthPcm(uint32_t &length) const
{
    GET_SLOT_OR_RETURN();
    ma_uint64 frames = 0;
    const auto result = ma_sound_get_length_in_pcm_frames(&slot->sound, &frames);
    length = static_cast<uint32_t>(frames);
    return Check(result);
}

Result Channel::setLooped(bool looped) const
{
    GET_SLOT_OR_RETURN();
    ma_sound_set_looping(&slot->sound, looped ? MA_TRUE : MA_FALSE);
    return Result::Ok;
}

Result Channel::getLooped(bool &looped) const
{
    GET_SLOT_OR_RETURN();
    looped = ma_sound_is_looping(&slot->sound);
    return Result::Ok;
}

Result Channel::setLoopPoints(uint32_t start_pcm, uint32_t end_pcm) const
{
    GET_SLOT_OR_RETURN();
    return Check(
        ma_data_source_set_loop_point_in_pcm_frames(ma_sound_get_data_source(&slot->sound), start_pcm, end_pcm));
}

Result Channel::set3DMinMaxDistance(float min_distance, float max_distance) const
{
    GET_SLOT_OR_RETURN();
    // same validation as FMOD: invalid ranges are rejected and the previous values are kept
    if (min_distance < 0.0f || max_distance < min_distance)
    {
        return Result::Failed;
    }
    ma_sound_set_min_distance(&slot->sound, min_distance);
    ma_sound_set_max_distance(&slot->sound, max_distance);
    return Result::Ok;
}

Result Channel::get3DMinMaxDistance(float &min_distance, float &max_distance) const
{
    GET_SLOT_OR_RETURN();
    min_distance = ma_sound_get_min_distance(&slot->sound);
    max_distance = ma_sound_get_max_distance(&slot->sound);
    return Result::Ok;
}

Result Channel::set3DPosition(const Vector3 &position) const
{
    GET_SLOT_OR_RETURN();
    const auto p = ToMa(position);
    ma_sound_set_position(&slot->sound, p.x, p.y, p.z);
    return Result::Ok;
}

Result Channel::get3DPosition(Vector3 &position) const
{
    GET_SLOT_OR_RETURN();
    position = FromMa(ma_sound_get_position(&slot->sound));
    return Result::Ok;
}

#undef GET_SLOT_OR_RETURN

// System ----------------------------------------------------------------------------------------------------------

System::~System()
{
    shutdown();
}

bool System::init(int max_channels)
{
    if (backend != nullptr)
    {
        return false;
    }

    auto instance = std::make_unique<Backend>();
    auto config = ma_engine_config_init();
    config.listenerCount = 1;
    if (ma_engine_init(&config, &instance->engine) != MA_SUCCESS)
    {
        return false;
    }
    if (ma_sound_group_init(&instance->engine, 0, nullptr, &instance->master) != MA_SUCCESS)
    {
        ma_engine_uninit(&instance->engine);
        return false;
    }
    instance->max_channels = max_channels > 0 ? static_cast<size_t>(max_channels) : 1;

    backend = instance.release();
    initialized_ = true;
    return true;
}

void System::shutdown()
{
    if (!initialized_ || backend == nullptr)
    {
        return;
    }
    for (uint32_t i = 0; i < backend->slots.size(); i++)
    {
        backend->Release(i);
    }
    for (auto &sound : backend->sounds)
    {
        if (sound->has_prototype)
        {
            ma_sound_uninit(&sound->prototype);
        }
    }
    backend->sounds.clear();
    ma_sound_group_uninit(&backend->master);
    ma_engine_uninit(&backend->engine);
    delete backend;
    backend = nullptr;
    initialized_ = false;
}

void System::update()
{
    if (backend == nullptr)
    {
        return;
    }
    // like FMOD: finished channels are reclaimed and their handles become invalid
    for (uint32_t i = 0; i < backend->slots.size(); i++)
    {
        auto &slot = *backend->slots[i];
        if (slot.active && !slot.paused && ma_sound_at_end(&slot.sound))
        {
            backend->Release(i);
        }
    }
}

const char *System::name() const
{
    return "miniaudio " MA_VERSION_STRING;
}

Sound System::createSound(const char *path, SoundKind kind, bool looped)
{
    if (backend == nullptr || path == nullptr)
    {
        return nullptr;
    }

    auto data = std::make_unique<SoundData>();
    data->path = path;
    data->kind = kind;
    data->looped = looped;

    if (kind != SoundKind::Stream2D)
    {
        constexpr ma_uint32 flags = MA_SOUND_FLAG_DECODE;
        if (ma_sound_init_from_file(&backend->engine, path, flags, &backend->master, nullptr, &data->prototype) !=
            MA_SUCCESS)
        {
            return nullptr;
        }
        data->has_prototype = true;
    }
    else
    {
        // fail early like FMOD::System::createStream does when the file cannot be opened
        ma_decoder decoder;
        if (ma_decoder_init_file(path, nullptr, &decoder) != MA_SUCCESS)
        {
            return nullptr;
        }
        ma_decoder_uninit(&decoder);
    }

    backend->sounds.push_back(std::move(data));
    return backend->sounds.back().get();
}

void System::releaseSound(Sound sound)
{
    if (backend == nullptr || sound == nullptr)
    {
        return;
    }
    for (uint32_t i = 0; i < backend->slots.size(); i++)
    {
        if (backend->slots[i]->active && backend->slots[i]->source == sound)
        {
            backend->Release(i);
        }
    }
    if (sound->has_prototype)
    {
        ma_sound_uninit(&sound->prototype);
    }
    std::erase_if(backend->sounds, [sound](const auto &ptr) { return ptr.get() == sound; });
}

Channel System::playSound(Sound sound, bool paused)
{
    if (backend == nullptr || sound == nullptr)
    {
        return {};
    }

    uint32_t index;
    if (!backend->free_slots.empty())
    {
        index = backend->free_slots.back();
        backend->free_slots.pop_back();
    }
    else
    {
        if (backend->slots.size() >= backend->max_channels)
        {
            return {};
        }
        index = static_cast<uint32_t>(backend->slots.size());
        backend->slots.push_back(std::make_unique<ChannelSlot>());
    }

    auto &slot = *backend->slots[index];
    ma_uint32 flags = 0;
    if (sound->kind != SoundKind::Sample3D)
    {
        flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    }

    ma_result result;
    if (sound->kind == SoundKind::Stream2D)
    {
        result = ma_sound_init_from_file(&backend->engine, sound->path.c_str(), flags | MA_SOUND_FLAG_STREAM,
                                         &backend->master, nullptr, &slot.sound);
    }
    else
    {
        result = ma_sound_init_copy(&backend->engine, &sound->prototype, flags, &backend->master, &slot.sound);
    }
    if (result != MA_SUCCESS)
    {
        backend->free_slots.push_back(index);
        return {};
    }

    if (sound->kind == SoundKind::Sample3D)
    {
        ma_sound_set_attenuation_model(&slot.sound, ma_attenuation_model_linear);
        ma_sound_set_min_distance(&slot.sound, kDefaultMinDistance);
        ma_sound_set_max_distance(&slot.sound, kDefaultMaxDistance);
        ma_sound_set_doppler_factor(&slot.sound, 0.0f);
    }
    ma_sound_set_looping(&slot.sound, sound->looped ? MA_TRUE : MA_FALSE);

    slot.source = sound;
    slot.active = true;
    slot.paused = paused;
    slot.priority = kDefaultPriority;
    if (!paused)
    {
        ma_sound_start(&slot.sound);
    }
    return Channel(index, slot.generation);
}

void System::setListener(const Vector3 *position, const Vector3 *forward, const Vector3 *up)
{
    if (backend == nullptr)
    {
        return;
    }
    if (position)
    {
        const auto p = ToMa(*position);
        ma_engine_listener_set_position(&backend->engine, 0, p.x, p.y, p.z);
    }
    if (forward)
    {
        const auto f = ToMa(*forward);
        ma_engine_listener_set_direction(&backend->engine, 0, f.x, f.y, f.z);
    }
    if (up)
    {
        const auto u = ToMa(*up);
        ma_engine_listener_set_world_up(&backend->engine, 0, u.x, u.y, u.z);
    }
}

void System::getListener(Vector3 &position, Vector3 &forward, Vector3 &up) const
{
    if (backend == nullptr)
    {
        return;
    }
    position = FromMa(ma_engine_listener_get_position(&backend->engine, 0));
    forward = FromMa(ma_engine_listener_get_direction(&backend->engine, 0));
    up = FromMa(ma_engine_listener_get_world_up(&backend->engine, 0));
}

void System::setActiveWithFade(bool active, float seconds)
{
    if (backend == nullptr)
    {
        return;
    }
    const auto ms = static_cast<ma_uint64>(seconds * 1000.0f);
    if (active)
    {
        ma_engine_start(&backend->engine);
        ma_sound_group_set_fade_in_milliseconds(&backend->master, 0.0f, 1.0f, ms);
    }
    else
    {
        // the device keeps running silently so the fade-out is audible
        ma_sound_group_set_fade_in_milliseconds(&backend->master, -1.0f, 0.0f, ms);
    }
}

Stats System::getStats() const
{
    // miniaudio does not track CPU usage or allocations
    return {};
}

} // namespace storm::audio

#endif // STORM_SOUND_MINIAUDIO
