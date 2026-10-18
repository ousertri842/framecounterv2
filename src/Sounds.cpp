#include "Sounds.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>

#include <unordered_map>

using namespace geode::prelude;

namespace fwl::sounds {

namespace {
    std::unordered_map<std::string, FMOD::Sound*> s_cache;

    FMOD::Sound* load(std::filesystem::path const& path) {
        auto key = utils::string::pathToString(path);
        if (auto it = s_cache.find(key); it != s_cache.end()) return it->second;

        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine || !engine->m_system) return nullptr;

        FMOD::Sound* sound = nullptr;
        if (engine->m_system->createSound(key.c_str(), FMOD_DEFAULT, nullptr, &sound) != FMOD_OK) {
            log::warn("Could not load sound {}", key);
            sound = nullptr;
        }
        s_cache[key] = sound;
        return sound;
    }

    std::filesystem::path bundled(std::string const& name) {
        return Mod::get()->getResourcesDir() / name;
    }

    std::filesystem::path custom(std::string const& key) {
        auto value = Mod::get()->getSettingValue<std::filesystem::path>(key);
        std::error_code ec;
        if (!value.empty() && std::filesystem::exists(value, ec)) return value;
        return {};
    }

    std::filesystem::path pick(int frames, bool release, bool unknown) {
        if (unknown) return bundled("fwl-unknown.wav");
        if (release) return bundled("fwl-release.wav");
        if (frames >= 1 && frames <= 5) {
            if (auto path = custom(fmt::format("sound-{}", frames)); !path.empty()) return path;
        }
        if (frames >= 1 && frames <= 9) return bundled(fmt::format("fwl-{}.wav", frames));
        if (auto path = custom("sound-wide"); !path.empty()) return path;
        return bundled("fwl-wide.wav");
    }
}

void play(int frames, bool release, bool unknown) {
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine || !engine->m_system) return;

    auto sound = load(pick(frames, release, unknown));
    if (!sound) return;

    FMOD::Channel* channel = nullptr;
    if (engine->m_system->playSound(sound, nullptr, false, &channel) == FMOD_OK && channel) {
        channel->setVolume(engine->m_sfxVolume);
    }
}

void clearCache() {
    for (auto& [_, sound] : s_cache) {
        if (sound) sound->release();
    }
    s_cache.clear();
}

} // namespace fwl::sounds
