#pragma once

// Thin audio backend layer used by SoundService.
// The API mirrors the subset of FMOD Core that the engine used, so the service logic stays backend-agnostic.
// Implementations: audio_backend_fmod.cpp (STORM_SOUND_FMOD), audio_backend_miniaudio.cpp (STORM_SOUND_MINIAUDIO).
// Coordinates are in engine space (left-handed, +Z forward), backends convert if needed.

#include <cstddef>
#include <cstdint>

namespace storm::audio
{

enum class Result
{
    Ok,
    InvalidHandle, // channel has finished, was stopped or stolen
    Failed,
};

const char *ResultString(Result result);

struct Vector3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class SoundKind
{
    Sample2D, // fully decoded, no spatialization
    Sample3D, // fully decoded, linear distance rolloff
    Stream2D, // streamed from disk (music)
};

struct SoundData;
using Sound = SoundData *;

class Channel
{
  public:
    Channel() = default;

    bool operator==(const Channel &) const = default;
    [[nodiscard]] bool valid() const
    {
        return generation_ != 0;
    }
    [[nodiscard]] uint32_t index() const
    {
        return index_;
    }

    // stops playback and invalidates the channel
    Result stop() const;

    Result getPaused(bool &paused) const;
    Result setPaused(bool paused) const;
    Result isPlaying(bool &playing) const;
    Result isVirtual(bool &is_virtual) const;
    Result getAudibility(float &audibility) const;

    Result setVolume(float volume) const;
    Result getVolume(float &volume) const;
    Result setPitch(float pitch) const;
    Result setPriority(int priority) const;
    Result getPriority(int &priority) const;

    Result setPositionMs(uint32_t position) const;
    Result getPositionMs(uint32_t &position) const;
    Result getLengthPcm(uint32_t &length) const;

    Result setLooped(bool looped) const;
    Result getLooped(bool &looped) const;
    Result setLoopPoints(uint32_t start_pcm, uint32_t end_pcm) const;

    Result set3DMinMaxDistance(float min_distance, float max_distance) const;
    Result get3DMinMaxDistance(float &min_distance, float &max_distance) const;
    Result set3DPosition(const Vector3 &position) const;
    Result get3DPosition(Vector3 &position) const;

  private:
    friend class System;
    Channel(uint32_t index, uint32_t generation) : index_(index), generation_(generation)
    {
    }

    uint32_t index_ = 0;
    uint32_t generation_ = 0; // 0 means "no channel"
};

struct Stats
{
    float cpu_percent = 0.0f;
    size_t memory = 0;
    size_t memory_peak = 0;
};

// Only one System instance may exist at a time.
class System
{
  public:
    System() = default;
    System(const System &) = delete;
    System &operator=(const System &) = delete;
    ~System();

    bool init(int max_channels);
    void shutdown();
    void update();
    [[nodiscard]] const char *name() const;

    // returns nullptr on failure
    Sound createSound(const char *path, SoundKind kind, bool looped = false);
    // releases the sound and stops all channels playing it
    void releaseSound(Sound sound);

    // returns an invalid channel on failure
    Channel playSound(Sound sound, bool paused);

    // any argument may be nullptr
    void setListener(const Vector3 *position, const Vector3 *forward, const Vector3 *up);
    void getListener(Vector3 &position, Vector3 &forward, Vector3 &up) const;

    // fade every playing channel out (and suspend the mixer) or resume and fade back in
    void setActiveWithFade(bool active, float seconds);

    [[nodiscard]] Stats getStats() const;

  private:
    bool initialized_ = false;
};

} // namespace storm::audio
