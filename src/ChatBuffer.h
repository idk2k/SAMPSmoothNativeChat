#pragma once
#include <string>
#include <vector>
#include <deque>
#include <d3d9.h>

#include "ChatTextParser.h"

struct ChatMessage {
    std::vector<TextSegment> segments;
    D3DCOLOR base_color = 0xFFFFFFFF;
    std::wstring timestamp; // например "[12:34]"
};

class ChatBuffer {
public:
    std::deque<ChatMessage> messages;
    size_t max_messages = 300;

    void Append(
        const std::wstring& text,
        D3DCOLOR color,
        const std::wstring& timestamp)
    {
        ChatMessage message;
        message.base_color = color;
        message.timestamp = timestamp;
        message.segments = ParseColorCodes(text, color);

        messages.push_back(std::move(message));

        while (messages.size() > max_messages) {
            messages.pop_front();
        }
    }

    void Clear() {
        messages.clear();
    }
};