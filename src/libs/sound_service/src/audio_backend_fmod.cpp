#ifdef STORM_SOUND_FMOD

#include "audio_backend.h"

#include <cstdio>
#include <vector>

#include <fmod.hpp>
#include <fmod_errors.h>

namespace storm::audio
{

namespace
{

constexpr float kDistanceFactor = 1.0f;

FMOD::System *fmod_system = nullptr;
FMOD::ChannelGroup *master_group = nullptr;

// FMOD channel handles stay safe to call after the channel ended (calls return FMOD_ERR_INVALID_HANDLE),
// the table only gives them a stable integer identity for the Channel type
struct ChannelEntry
{
    FMOD::Channel *channel = nullptr;
    uint32_t generation = 1;
};
std::vector<ChannelEntry> channels;
std::vector<uint32_t> free_entries;

Result Convert(FMOD_RESULT result)
{
    switch (result)
    {
    case FMOD_OK:
        return Result::Ok;
    case FMOD_ERR_INVALID_HANDLE:
    case FMOD_ERR_CHANNEL_STOLEN:
        return Result::InvalidHandle;
    default:
        return Result::Failed;
    }
}

FMOD::Channel *Lookup(uint32_t index, uint32_t generation)
{
    if (generation == 0 || index >= channels.size() || channels[index].generation != generation)
    {
        return nullptr;
    }
    return channels[index].channel;
}

FMOD_VECTOR ToFmod(const Vector3 &v)
{
    return FMOD_VECTOR{v.x, v.y, v.z};
}

Vector3 FromFmod(const FMOD_VECTOR &v)
{
    return Vector3{v.x, v.y, v.z};
}

void ReleaseEntry(uint32_t index)
{
    auto &entry = channels[index];
    entry.channel = nullptr;
    if (++entry.generation == 0)
    {
        entry.generation = 1;
    }
    free_entries.push_back(index);
}

} // namespace

struct SoundData
{
    FMOD::Sound *sound = nullptr;
};

const char *ResultString(Result result)
{
    switch (result)
    {
    case Result::Ok:
        return "ok";
    case Result::InvalidHandle:
        return "invalid channel handle";
    default:
        return "FMOD call failed";
    }
}

// Channel ---------------------------------------------------------------------------------------------------------

#define GET_CHANNEL_OR_RETURN()                                                                                        \
    auto *channel = Lookup(index_, generation_);                                                                       \
    if (channel == nullptr)                                                                                            \
    {                                                                                                                  \
        return Result::InvalidHandle;                                                                                  \
    }

Result Channel::stop() const
{
    GET_CHANNEL_OR_RETURN();
    const auto result = Convert(channel->stop());
    ReleaseEntry(index_);
    return result;
}

Result Channel::getPaused(bool &paused) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->getPaused(&paused));
}

Result Channel::setPaused(bool paused) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->setPaused(paused));
}

Result Channel::isPlaying(bool &playing) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->isPlaying(&playing));
}

Result Channel::isVirtual(bool &is_virtual) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->isVirtual(&is_virtual));
}

Result Channel::getAudibility(float &audibility) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->getAudibility(&audibility));
}

Result Channel::setVolume(float volume) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->setVolume(volume));
}

Result Channel::getVolume(float &volume) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->getVolume(&volume));
}

Result Channel::setPitch(float pitch) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->setPitch(pitch));
}

Result Channel::setPriority(int priority) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->setPriority(priority));
}

Result Channel::getPriority(int &priority) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->getPriority(&priority));
}

Result Channel::setPositionMs(uint32_t position) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->setPosition(position, FMOD_TIMEUNIT_MS));
}

Result Channel::getPositionMs(uint32_t &position) const
{
    GET_CHANNEL_OR_RETURN();
    unsigned int value = 0;
    const auto result = channel->getPosition(&value, FMOD_TIMEUNIT_MS);
    position = value;
    return Convert(result);
}

Result Channel::getLengthPcm(uint32_t &length) const
{
    GET_CHANNEL_OR_RETURN();
    FMOD::Sound *sound = nullptr;
    auto result = channel->getCurrentSound(&sound);
    if (result != FMOD_OK || sound == nullptr)
    {
        return Convert(result);
    }
    unsigned int value = 0;
    result = sound->getLength(&value, FMOD_TIMEUNIT_PCM);
    length = value;
    return Convert(result);
}

Result Channel::setLooped(bool looped) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->setMode(looped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
}

Result Channel::getLooped(bool &looped) const
{
    GET_CHANNEL_OR_RETURN();
    FMOD_MODE mode = 0;
    const auto result = channel->getMode(&mode);
    looped = (mode & FMOD_LOOP_NORMAL) != 0;
    return Convert(result);
}

Result Channel::setLoopPoints(uint32_t start_pcm, uint32_t end_pcm) const
{
    GET_CHANNEL_OR_RETURN();
    auto result = channel->setLoopPoints(start_pcm, FMOD_TIMEUNIT_PCM, end_pcm, FMOD_TIMEUNIT_PCM);
    if (result == FMOD_OK)
    {
        result = channel->setLoopCount(3);
    }
    return Convert(result);
}

Result Channel::set3DMinMaxDistance(float min_distance, float max_distance) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->set3DMinMaxDistance(min_distance * kDistanceFactor, max_distance * kDistanceFactor));
}

Result Channel::get3DMinMaxDistance(float &min_distance, float &max_distance) const
{
    GET_CHANNEL_OR_RETURN();
    return Convert(channel->get3DMinMaxDistance(&min_distance, &max_distance));
}

Result Channel::set3DPosition(const Vector3 &position) const
{
    GET_CHANNEL_OR_RETURN();
    const auto pos = ToFmod(position);
    const FMOD_VECTOR velocity{0.0f, 0.0f, 0.0f};
    return Convert(channel->set3DAttributes(&pos, &velocity));
}

Result Channel::get3DPosition(Vector3 &position) const
{
    GET_CHANNEL_OR_RETURN();
    FMOD_VECTOR pos{};
    FMOD_VECTOR velocity{};
    const auto result = channel->get3DAttributes(&pos, &velocity);
    position = FromFmod(pos);
    return Convert(result);
}

#undef GET_CHANNEL_OR_RETURN

// System ----------------------------------------------------------------------------------------------------------

System::~System()
{
    shutdown();
}

bool System::init(int max_channels)
{
    if (fmod_system != nullptr)
    {
        return false;
    }
    if (FMOD::System_Create(&fmod_system) != FMOD_OK)
    {
        return false;
    }

    unsigned version = 0;
    fmod_system->getVersion(&version);
    if (version < FMOD_VERSION)
    {
        std::fprintf(stderr, "Error : You are using old FMOD version %08x !\n", version);
        fmod_system->release();
        fmod_system = nullptr;
        return false;
    }

    fmod_system->setSoftwareChannels(max_channels);
    fmod_system->setOutput(FMOD_OUTPUTTYPE_AUTODETECT);
    if (fmod_system->init(max_channels, FMOD_INIT_NORMAL, nullptr) != FMOD_OK)
    {
        fmod_system->release();
        fmod_system = nullptr;
        return false;
    }
    fmod_system->set3DSettings(1.0, kDistanceFactor, 1.0f);
    fmod_system->getMasterChannelGroup(&master_group);

    initialized_ = true;
    return true;
}

void System::shutdown()
{
    if (!initialized_ || fmod_system == nullptr)
    {
        return;
    }
    fmod_system->close();
    fmod_system->release();
    fmod_system = nullptr;
    master_group = nullptr;
    channels.clear();
    free_entries.clear();
    initialized_ = false;
}

void System::update()
{
    if (fmod_system == nullptr)
    {
        return;
    }
    fmod_system->update();

    // forget channels FMOD has already invalidated, so the table does not grow forever
    for (uint32_t i = 0; i < channels.size(); i++)
    {
        bool playing;
        if (channels[i].channel != nullptr && channels[i].channel->isPlaying(&playing) != FMOD_OK)
        {
            ReleaseEntry(i);
        }
    }
}

const char *System::name() const
{
    static char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "FMOD %08x", FMOD_VERSION);
    return buffer;
}

Sound System::createSound(const char *path, SoundKind kind, bool looped)
{
    if (fmod_system == nullptr)
    {
        return nullptr;
    }

    FMOD::Sound *sound = nullptr;
    FMOD_RESULT result;
    if (kind == SoundKind::Stream2D)
    {
        const FMOD_MODE mode = FMOD_CREATESAMPLE | FMOD_2D | (looped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
        result = fmod_system->createStream(path, mode, nullptr, &sound);
    }
    else
    {
        FMOD_MODE mode = FMOD_DEFAULT;
        mode |= kind == SoundKind::Sample3D ? (FMOD_3D | FMOD_3D_LINEARROLLOFF) : FMOD_2D;
        result = fmod_system->createSound(path, mode, nullptr, &sound);
    }
    if (result != FMOD_OK || sound == nullptr)
    {
        std::fprintf(stderr, "FMOD: cannot load '%s': %s\n", path, FMOD_ErrorString(result));
        return nullptr;
    }
    return new SoundData{sound};
}

void System::releaseSound(Sound sound)
{
    if (sound == nullptr)
    {
        return;
    }
    if (sound->sound)
    {
        sound->sound->release();
    }
    delete sound;
}

Channel System::playSound(Sound sound, bool paused)
{
    if (fmod_system == nullptr || sound == nullptr)
    {
        return {};
    }
    FMOD::Channel *channel = nullptr;
    if (fmod_system->playSound(sound->sound, nullptr, paused, &channel) != FMOD_OK || channel == nullptr)
    {
        return {};
    }

    uint32_t index;
    if (!free_entries.empty())
    {
        index = free_entries.back();
        free_entries.pop_back();
    }
    else
    {
        index = static_cast<uint32_t>(channels.size());
        channels.push_back({});
    }
    channels[index].channel = channel;
    return Channel(index, channels[index].generation);
}

void System::setListener(const Vector3 *position, const Vector3 *forward, const Vector3 *up)
{
    if (fmod_system == nullptr)
    {
        return;
    }
    FMOD_VECTOR pos, fwd, top;
    if (position)
        pos = ToFmod(*position);
    if (forward)
        fwd = ToFmod(*forward);
    if (up)
        top = ToFmod(*up);
    fmod_system->set3DListenerAttributes(0, position ? &pos : nullptr, nullptr, forward ? &fwd : nullptr,
                                         up ? &top : nullptr);
}

void System::getListener(Vector3 &position, Vector3 &forward, Vector3 &up) const
{
    if (fmod_system == nullptr)
    {
        return;
    }
    FMOD_VECTOR pos{}, vel{}, fwd{}, top{};
    fmod_system->get3DListenerAttributes(0, &pos, &vel, &fwd, &top);
    position = FromFmod(pos);
    forward = FromFmod(fwd);
    up = FromFmod(top);
}

void System::setActiveWithFade(bool active, float seconds)
{
    if (fmod_system == nullptr || master_group == nullptr)
    {
        return;
    }

    if (active)
    {
        fmod_system->mixerResume();
    }

    unsigned long long parent_clock;
    int rate;
    fmod_system->getSoftwareFormat(&rate, nullptr, nullptr);
    master_group->getDSPClock(nullptr, &parent_clock);

    const auto dsp_clock_start = parent_clock;
    const auto dsp_clock_end = static_cast<unsigned long long>(parent_clock + seconds * rate);

    if (active)
    {
        master_group->setDelay(dsp_clock_start, 0, false);
    }

    for (const auto &entry : channels)
    {
        if (entry.channel == nullptr)
            continue;

        entry.channel->addFadePoint(dsp_clock_start, active ? 0.0f : 1.0f);
        entry.channel->addFadePoint(dsp_clock_end, active ? 1.0f : 0.0f);
    }

    if (!active)
    {
        master_group->setDelay(dsp_clock_start, dsp_clock_end, false);
        fmod_system->mixerSuspend();
    }
}

Stats System::getStats() const
{
    Stats stats;
    if (fmod_system == nullptr)
    {
        return stats;
    }
    FMOD_CPU_USAGE usage;
    fmod_system->getCPUUsage(&usage);
    stats.cpu_percent = usage.dsp + usage.stream + usage.geometry + usage.update + usage.convolution1 +
                        usage.convolution2;

    int current = 0;
    int peak = 0;
    FMOD::Memory_GetStats(&current, &peak);
    stats.memory = current;
    stats.memory_peak = peak;
    return stats;
}

} // namespace storm::audio

#endif // STORM_SOUND_FMOD
