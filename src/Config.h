#pragma once
#include <d3d9.h>
#include <string>

struct ChatConfig {
    int x = 45;
    int y = 40;
    int width = 758;
    int height = 200;
    int line_height = 20;
    int font_size = 18;
    size_t max_messages = 300;
};

struct ScrollConfig {
    float lines_per_click = 3.0f;
    float stiffness = 220.0f;
    float friction = 32.0f;
};

struct TimestampsConfig {
    bool enabled = true;
    D3DCOLOR color = 0xFF969696;
};

struct BackgroundConfig {
    bool enabled = true;
    BYTE top_alpha = 16;
    BYTE bottom_alpha = 144;
};

struct ScrollbarConfig {
    bool enabled = true;
};

struct LogConfig {
    std::string level = "info";
};

struct Config {
    ChatConfig chat;
    ScrollConfig scroll;
    TimestampsConfig timestamps;
    BackgroundConfig background;
    ScrollbarConfig scrollbar;
    LogConfig log;
};

extern Config g_config;

bool LoadConfig(const std::string& path);
bool SaveConfig(const std::string& path);
