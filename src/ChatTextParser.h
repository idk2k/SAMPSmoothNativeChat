#pragma once
#include <d3d9.h>
#include <string>
#include <vector>

struct TextSegment {
    std::wstring text;
    D3DCOLOR color = 0xFFFFFFFF;
};

inline bool IsHexDigit(wchar_t c) {
    return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
}

inline std::vector<TextSegment> ParseColorCodes(const std::wstring& input, D3DCOLOR base_color) {
    std::vector<TextSegment> segments;
    std::wstring current;
    D3DCOLOR current_color = base_color;

    auto flush = [&]() {
        if (!current.empty()) {
            segments.push_back({current, current_color});
            current.clear();
        }
    };

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == L'{' && i + 7 < input.size()) {
            bool valid = true;

            for (int k = 1; k <= 6; ++k) {
                if (!IsHexDigit(input[i + k])) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                flush();

                const std::wstring hex = input.substr(i + 1, 6);
                const unsigned long rgb = std::wcstoul(hex.c_str(), nullptr, 16);

                current_color = (base_color & 0xFF000000) | static_cast<D3DCOLOR>(rgb);

                i += 7;
                continue;
            }
        }

        current += input[i];
    }

    flush();
    return segments;
}