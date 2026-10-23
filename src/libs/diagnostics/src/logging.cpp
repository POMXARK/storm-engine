#include "logging.hpp"

#include <cstdlib>

#include <spdlog/spdlog.h>

#include "spdlog_sinks/syncable_sink.hpp"
#include "storm/engine_settings.hpp"

namespace
{

constexpr auto kLogExtension = ".log";

}

namespace storm::logging
{

// TODO: loggers with periodic flush shall be thread safe; we should measure performance diff and decide what to do
logger_ptr getOrCreateLogger(const std::string &name, const spdlog::level::level_enum level,
                                                  const bool truncate)
{
    auto logger = spdlog::get(name);
    if (logger)
    {
        return logger;
    }

    auto path = GetEngineSettings().GetEnginePath(EngineSettingsPathType::Logs) / name;
    path += kLogExtension;

    logger = spdlog::create<sinks::syncable_sink>(name, path.string(), truncate);
    logger->set_level(level);
    // STORM_LOG_FLUSH=1: every message reaches the file at once, so a crash leaves the last lines (opt-in, slower)
    if (const char *flush = std::getenv("STORM_LOG_FLUSH"); flush != nullptr && flush[0] == '1')
        logger->flush_on(spdlog::level::trace);

    return logger;
}

} // namespace storm::logging