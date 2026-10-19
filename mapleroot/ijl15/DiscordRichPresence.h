#pragma once
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "Discord/discord.h"
#include <csignal>
#include <iostream>
#include <sstream>

class DiscordRichPresence {
public:
    static DiscordRichPresence& getInstance() {
        static DiscordRichPresence instance;
        return instance;
    }

    DiscordRichPresence(const DiscordRichPresence&) = delete;
    DiscordRichPresence& operator=(const DiscordRichPresence&) = delete;

    DiscordRichPresence() {
        liveInstance = this;
        if (!Discord_Initialize(123451241254, DiscordCreateFlags_NoRequireDiscord)) {
            return;
        }
        isInitialized = true;
    }

    ~DiscordRichPresence() {
        stopUpdates();
        liveInstance = nullptr;
    }

    static void shutdownIfCreated() {
        if (liveInstance) liveInstance->stopUpdates();
    }

    void stopUpdates() {
        std::lock_guard<std::mutex> lifecycle(lifecycleMutex);
        running.store(false);
        wake.notify_all();
        if (thread.joinable()) thread.join();
        delete core;
        core = nullptr;
        isInitialized = false;
    }

    void updatePresence(const int jobCode, int level, const char* charName) {
        std::lock_guard<std::mutex> lock(mutex);
        auto [imageKey, displayName] = getJobInfo(jobCode);
        this->jobImageKey = imageKey;
        this->jobDisplayName = displayName;
        this->level = level;
        this->charName = charName;
        this->isLoggedIn = true;
    }

    void setJobCode(const int jobCode) {
        std::lock_guard<std::mutex> lock(mutex);
        auto [imageKey, displayName] = getJobInfo(jobCode);
        this->jobImageKey = imageKey;
        this->jobDisplayName = displayName;
    }

    void setLevel(int level) {
        std::lock_guard<std::mutex> lock(mutex);
        this->level = level;
    }

    void setCharacterName(const char* charName) {
        std::lock_guard<std::mutex> lock(mutex);
        this->charName = charName;
    }

    void setIsLoggedIn(bool isLoggedIn) {
        std::lock_guard<std::mutex> lock(mutex);
        this->isLoggedIn = isLoggedIn;
    }

    void beginUpdates() {
        std::lock_guard<std::mutex> lifecycle(lifecycleMutex);
        if (!isInitialized || running) {
            return;
        }

        running = true;
        level = 0;
        isLoggedIn = false;

        // Set default job to Warrior
        auto [imageKey, displayName] = getJobInfo(100);
        this->jobImageKey = imageKey;
        this->jobDisplayName = displayName;

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto epoch = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

        activity.GetTimestamps().SetStart(epoch);

        activity.SetDetails("Playing Remedy");
        activity.GetAssets().SetLargeImage("logo");
        activity.GetAssets().SetLargeText("Playing Maple Remedy");
        activity.SetName("Exploring the Maple World");
        activity.SetType(discord::ActivityType::Playing);

        core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {});

        std::signal(SIGINT, [](int) { getInstance().running = false; });

        thread = std::thread(&DiscordRichPresence::updateLoop, this);
    }

private:
    inline static DiscordRichPresence* liveInstance = nullptr;
    std::thread thread;
    std::mutex mutex;
    std::mutex lifecycleMutex;
    std::mutex waitMutex;
    std::condition_variable wake;
    std::atomic<bool> running{false};
    std::string jobImageKey;      // snake_case image key
    std::string jobDisplayName;   // formatted job name for text
    int level{};
    bool isLoggedIn{};
    bool isInitialized{};
    std::string charName;
    discord::Core* core{};
    discord::Activity activity{};

    void updateLoop() {
        while (running) {
            Discord_UpdatePresence();
            std::unique_lock<std::mutex> lock(waitMutex);
            wake.wait_for(lock, std::chrono::seconds(15), [this] { return !running.load(); });
        }
    }

    bool Discord_Initialize(discord::ClientId clientId, EDiscordCreateFlags flags) {
        auto result = discord::Core::Create(clientId, flags, &core);
        return result == discord::Result::Ok;
    }

    void Discord_UpdatePresence() {
        core->RunCallbacks();
        std::lock_guard<std::mutex> lock(mutex);

        if (isLoggedIn) {
            std::stringstream ss;
            ss << charName << " (Lv. " << level << ")";

            activity.GetAssets().SetSmallImage(jobImageKey.c_str());        // snake_case image key
            activity.GetAssets().SetSmallText(jobDisplayName.c_str());      // formatted text
            activity.GetAssets().SetLargeImage("logo");
            activity.GetAssets().SetLargeText("Exploring the world!");
            activity.SetDetails(ss.str().c_str());
        }
        else {
            activity.SetName("Remedy");
            activity.SetDetails("Playing Remedy");
            activity.GetAssets().SetSmallImage("");
            activity.GetAssets().SetSmallText("");
            activity.GetAssets().SetLargeImage("logo");
            activity.GetAssets().SetLargeText("Playing Maple Remedy");
        }

        core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {});
    }

    static std::pair<std::string, std::string> getJobInfo(int jobId) {
        switch (jobId) {
        case 0: return { "beginner", "Beginner" };
        case 100: return { "warrior", "Warrior" };
        case 110: return { "fighter", "Fighter" };
        case 111: return { "crusader", "Crusader" };
        case 112: return { "herald", "Herald" };
        case 113: return { "hero", "Hero" };
        case 120: return { "page", "Page" };
        case 121: return { "white_knight", "White Knight" };
        case 122: return { "paladin", "Paladin" };
        case 123: return { "retribute", "Retribute" };
        case 130: return { "spearman", "Spearman" };
        case 131: return { "dragon_knight", "Dragon Knight" };
        case 132: return { "dark_knight", "Dark Knight" };
        case 133: return { "dark_omen", "Dark Omen" };
        case 200: return { "mage", "Mage" };
        case 210: return { "fp_wizard", "F/P Wizard" };
        case 211: return { "fp_mage", "F/P Mage" };
        case 212: return { "fp_archmage", "F/P Archmage" };
        case 213: return { "fire_grandmaster", "Fire Grandmaster" };
        case 220: return { "il_wizard", "I/L Wizard" };
        case 221: return { "il_mage", "I/L Mage" };
        case 222: return { "il_archmage", "I/L Archmage" };
        case 223: return { "ice_grandmaster", "Ice Grandmaster" };
        case 230: return { "cleric", "Cleric" };
        case 231: return { "priest", "Priest" };
        case 232: return { "bishop", "Bishop" };
        case 233: return { "holy_grandmaster", "Holy Grandmaster" };
        case 300: return { "bowman", "Bowman" };
        case 310: return { "hunter", "Hunter" };
        case 311: return { "ranger", "Ranger" };
        case 312: return { "bowmaster", "Bowmaster" };
        case 313: return { "sharpeyes", "Sharp Eyes" };
        case 320: return { "crossbowman", "Crossbowman" };
        case 321: return { "sniper", "Sniper" };
        case 322: return { "marksman", "Marksman" };
        case 323: return { "cursed_bolt", "Cursed Bolt" };
        case 400: return { "thief", "Thief" };
        case 410: return { "assassin", "Assassin" };
        case 411: return { "hermit", "Hermit" };
        case 412: return { "nightlord", "Night Lord" };
        case 413: return { "void", "Void" };
        case 420: return { "bandit", "Bandit" };
        case 421: return { "chief_bandit", "Chief Bandit" };
        case 422: return { "shadower", "Shadower" };
        case 423: return { "inverse", "Inverse" };
        case 500: return { "pirate", "Pirate" };
        case 510: return { "brawler", "Brawler" };
        case 511: return { "marauder", "Marauder" };
        case 512: return { "buccaneer", "Buccaneer" };
        case 513: return { "captain", "Captain" };
        case 520: return { "gunslinger", "Gunslinger" };
        case 521: return { "outlaw", "Outlaw" };
        case 522: return { "corsair", "Corsair" };
        case 523: return { "sharpshot", "Sharpshot" };
        case 700: return { "super_beginner", "Super Beginner" };
        case 710: return { "synth_master", "Synth Master" };
        case 900: return { "game_master", "Game Master" };
        case 910: return { "game_master", "Game Master" };
        case 1000: return { "noblesse", "Noblesse" };
        case 1100: return { "dawn_warrior", "Dawn Warrior" };
        case 1110: return { "dawn_warrior", "Dawn Warrior" };
        case 1111: return { "dawn_warrior", "Dawn Warrior" };
        case 1112: return { "dawn_warrior", "Dawn Warrior" };
        case 1113: return { "sunset", "Sunset" };
        case 1200: return { "blaze_wizard", "Blaze Wizard" };
        case 1210: return { "blaze_wizard", "Blaze Wizard" };
        case 1211: return { "blaze_wizard", "Blaze Wizard" };
        case 1212: return { "blaze_wizard", "Blaze Wizard" };
        case 1213: return { "fiercing_flame", "Fiercing Flame" };
        case 1300: return { "wind_archer", "Wind Archer" };
        case 1310: return { "wind_archer", "Wind Archer" };
        case 1311: return { "wind_archer", "Wind Archer" };
        case 1312: return { "wind_archer", "Wind Archer" };
        case 1313: return { "surreal_breeze", "Surreal Breeze" };
        case 1400: return { "night_walker", "Night Walker" };
        case 1410: return { "night_walker", "Night Walker" };
        case 1411: return { "night_walker", "Night Walker" };
        case 1412: return { "night_walker", "Night Walker" };
        case 1413: return { "night_shade", "Night Shade" };
        case 1500: return { "thunder_breaker", "Thunder Breaker" };
        case 1510: return { "thunder_breaker", "Thunder Breaker" };
        case 1511: return { "thunder_breaker", "Thunder Breaker" };
        case 1512: return { "thunder_breaker", "Thunder Breaker" };
        case 1513: return { "knuckle_breaker", "Knuckle Breaker" };
        case 2000: return { "legend", "Legend" };
        case 2100: return { "aran", "Aran" };
        case 2110: return { "aran", "Aran" };
        case 2111: return { "aran", "Aran" };
        case 2112: return { "aran", "Aran" };
        case 2113: return { "aegis", "Aegis" };
        default: return { "logo", "Logo" };
        }
    }
};
