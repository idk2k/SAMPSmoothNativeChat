#include "Config.h"
#include "Logger.h"
#include "nlohmann/json.hpp"
#include <Windows.h>
#include <fstream>

using json = nlohmann::json;

Config g_config;

// Парсит "#RRGGBB" или "AARRGGBB" в D3DCOLOR.
static D3DCOLOR ParseColor(const std::string& hex, D3DCOLOR fallback) {
    if (hex.size() != 7 && hex.size() != 9)
        return fallback;

    auto to_byte = [](const std::string& s, size_t pos) -> int { return std::stoi(s.substr(pos, 2), nullptr, 16); };

    if (hex.size() == 7) {
        // "#RRGGBB" -> alpha 255
        return 0xFF000000 | (to_byte(hex, 1) << 16) | (to_byte(hex, 3) << 8) | to_byte(hex, 5);
    }

    // "AARRGGBB"
    return (to_byte(hex, 1) << 24) | (to_byte(hex, 3) << 16) | (to_byte(hex, 5) << 8) | to_byte(hex, 7);
}

static void LogLoadedConfig();

bool LoadConfig(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        Logger::Warn("Config not found: " + path + ", using defaults");
        return false;
    }

    try {
        const json j = json::parse(file);

        // chat
        if (j.contains("chat")) {
            const auto& c = j["chat"];
            g_config.chat.x = c.value("x", g_config.chat.x);
            g_config.chat.y = c.value("y", g_config.chat.y);
            g_config.chat.width = c.value("width", g_config.chat.width);
            g_config.chat.height = c.value("height", g_config.chat.height);
            g_config.chat.line_height = c.value("line_height", g_config.chat.line_height);
            g_config.chat.font_size = c.value("font_size", g_config.chat.font_size);
            g_config.chat.max_messages = c.value("max_messages", g_config.chat.max_messages);
        }

        // scroll
        if (j.contains("scroll")) {
            const auto& s = j["scroll"];
            g_config.scroll.lines_per_click = s.value("lines_per_click", g_config.scroll.lines_per_click);
            g_config.scroll.stiffness = s.value("stiffness", g_config.scroll.stiffness);
            g_config.scroll.friction = s.value("friction", g_config.scroll.friction);
        }

        // timestamps
        if (j.contains("timestamps")) {
            const auto& t = j["timestamps"];
            g_config.timestamps.enabled = t.value("enabled", g_config.timestamps.enabled);
            g_config.timestamps.color = ParseColor(t.value("color", "#969696"), g_config.timestamps.color);
        }

        // background
        if (j.contains("background")) {
            const auto& b = j["background"];
            g_config.background.enabled = b.value("enabled", g_config.background.enabled);
            g_config.background.top_alpha = b.value("top_alpha", g_config.background.top_alpha);
            g_config.background.bottom_alpha = b.value("bottom_alpha", g_config.background.bottom_alpha);
        }

        // scrollbar
        if (j.contains("scrollbar")) {
            g_config.scrollbar.enabled = j["scrollbar"].value("enabled", g_config.scrollbar.enabled);
        }

        // log
        if (j.contains("log")) {
            g_config.log.level = j["log"].value("level", g_config.log.level);
        }

        LogLoadedConfig();
        Logger::Ok("Config loaded: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::Error(std::string("Config parse error: ") + e.what());
        return false;
    }
}

static void LogLoadedConfig() {
    Logger::Info("---- Loaded config ----");

    // chat
    Logger::Info("chat.x = " + std::to_string(g_config.chat.x));
    Logger::Info("chat.y = " + std::to_string(g_config.chat.y));
    Logger::Info("chat.width = " + std::to_string(g_config.chat.width));
    Logger::Info("chat.height = " + std::to_string(g_config.chat.height));
    Logger::Info("chat.line_height = " + std::to_string(g_config.chat.line_height));
    Logger::Info("chat.font_size = " + std::to_string(g_config.chat.font_size));
    Logger::Info("chat.max_messages = " + std::to_string(g_config.chat.max_messages));

    // scroll
    Logger::Info("scroll.lines_per_click = " + std::to_string(g_config.scroll.lines_per_click));
    Logger::Info("scroll.stiffness = " + std::to_string(g_config.scroll.stiffness));
    Logger::Info("scroll.friction = " + std::to_string(g_config.scroll.friction));

    // timestamps
    Logger::Info("timestamps.enabled = " + std::string(g_config.timestamps.enabled ? "true" : "false"));

    {
        std::ostringstream color_stream;
        color_stream << std::hex << std::uppercase << g_config.timestamps.color;
        Logger::Info("timestamps.color = 0x" + color_stream.str());
    }

    // background
    Logger::Info("background.enabled = " + std::string(g_config.background.enabled ? "true" : "false"));
    Logger::Info("background.top_alpha = " + std::to_string(g_config.background.top_alpha));
    Logger::Info("background.bottom_alpha = " + std::to_string(g_config.background.bottom_alpha));

    // scrollbar
    Logger::Info("scrollbar.enabled = " + std::string(g_config.scrollbar.enabled ? "true" : "false"));

    // log
    Logger::Info("log.level = " + g_config.log.level);

    Logger::Info("---- End of config ----");
}

bool SaveConfig(const std::string& path) {
    try {
        json j;

        // chat
        j["chat"]["x"] = g_config.chat.x;
        j["chat"]["y"] = g_config.chat.y;
        j["chat"]["width"] = g_config.chat.width;
        j["chat"]["height"] = g_config.chat.height;
        j["chat"]["line_height"] = g_config.chat.line_height;
        j["chat"]["font_size"] = g_config.chat.font_size;
        j["chat"]["max_messages"] = g_config.chat.max_messages;

        // scroll
        j["scroll"]["lines_per_click"] = g_config.scroll.lines_per_click;
        j["scroll"]["stiffness"] = g_config.scroll.stiffness;
        j["scroll"]["friction"] = g_config.scroll.friction;

        // timestamps
        j["timestamps"]["enabled"] = g_config.timestamps.enabled;

        // background
        j["background"]["enabled"] = g_config.background.enabled;
        j["background"]["top_alpha"] = g_config.background.top_alpha;
        j["background"]["bottom_alpha"] = g_config.background.bottom_alpha;

        // scrollbar
        j["scrollbar"]["enabled"] = g_config.scrollbar.enabled;

        // log
        j["log"]["level"] = g_config.log.level;

        std::ofstream file(path);
        if (!file.is_open()) {
            Logger::Error("Failed to open config file for writing: " + path);
            return false;
        }

        file << j.dump(4);

        Logger::Ok("Config saved: " + path);
        return true;
    } catch (const std::exception& e) {
        Logger::Error(std::string("Config save error: ") + e.what());
        return false;
    }
}