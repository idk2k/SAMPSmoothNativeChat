#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>

// ---------------------------------------------------------------------------
// ÷ветной логгер дл€ консоли Windows.
//
// ”ровни:
//   TRACE  Ч серый, максимально подробный
//   DEBUG  Ч голубой, отладочна€ информаци€
//   INFO   Ч белый, обычные событи€
//   OK     Ч зелЄный, успешные операции
//   WARN   Ч жЄлтый, предупреждени€
//   ERROR  Ч красный, ошибки
//   HOOK   Ч пурпурный, установка/сн€тие хуков
//   D3D    Ч голубой, DirectX-событи€
//   SAMP   Ч жЄлтый, событи€ SA-MP
// ---------------------------------------------------------------------------

class Logger {
public:
    enum class Level {
        Trace,
        Debug,
        Info,
        Ok,
        Warn,
        Error,
        Hook,
        D3D,
        SAMP
    };

    static void Init() {
        // ¬ключаем ANSI escape-последовательности в консоли.
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

        DWORD mode = 0;
        if (GetConsoleMode(console, &mode)) {
            SetConsoleMode(console,
                mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        // ¬ключаем UTF-8 вывод (дл€ кириллицы в консоли).
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }

    static void Trace(const std::string& text) { Write(Level::Trace, text); }
    static void Debug(const std::string& text) { Write(Level::Debug, text); }
    static void Info(const std::string& text) { Write(Level::Info, text); }
    static void Ok(const std::string& text) { Write(Level::Ok, text); }
    static void Warn(const std::string& text) { Write(Level::Warn, text); }
    static void Error(const std::string& text) { Write(Level::Error, text); }
    static void Hook(const std::string& text) { Write(Level::Hook, text); }
    static void D3D(const std::string& text) { Write(Level::D3D, text); }
    static void SAMP(const std::string& text) { Write(Level::SAMP, text); }

    // ”казатель в hex.
    static void Pointer(const std::string& name, uintptr_t value) {
        std::ostringstream stream;
        stream << name << " = 0x"
            << std::hex << std::uppercase << value;
        Info(stream.str());
    }

    // HRESULT-результат.
    static void HResult(const std::string& name, HRESULT result) {
        if (SUCCEEDED(result)) {
            Ok(name + " succeeded (0x" + ToHex(static_cast<uint32_t>(result)) + ")");
        }
        else {
            Error(name + " failed (0x" + ToHex(static_cast<uint32_t>(result)) + ")");
        }
    }

    // MH_STATUS-результат.
    static void MhStatus(const std::string& name, int status) {
        if (status == 0) { // MH_OK
            Ok(name + " succeeded (MH_OK)");
        }
        else {
            Error(name + " failed (MH_STATUS=" + std::to_string(status) + ")");
        }
    }

    // Hex-дамп пам€ти (дл€ отладки патчей/структур).
    static void HexDump(const std::string& name,
        const void* data,
        size_t size)
    {
        if (!data || size == 0) {
            Warn(name + ": empty dump");
            return;
        }

        const auto* bytes = static_cast<const uint8_t*>(data);

        std::ostringstream stream;
        stream << name << " [" << size << " bytes]: ";

        for (size_t i = 0; i < size; ++i) {
            stream << std::hex << std::uppercase << std::setw(2)
                << std::setfill('0')
                << static_cast<int>(bytes[i]) << ' ';
        }

        Debug(stream.str());
    }

private:
    static std::string ToHex(uint32_t value) {
        std::ostringstream stream;
        stream << std::hex << std::uppercase << value;
        return stream.str();
    }

    static const char* LevelTag(Level level) {
        switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Ok:    return "OK   ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        case Level::Hook:  return "HOOK ";
        case Level::D3D:   return "D3D  ";
        case Level::SAMP:  return "SAMP ";
        }
        return "?????";
    }

    static const char* LevelColor(Level level) {
        switch (level) {
        case Level::Trace: return "\x1B[90m"; // серый
        case Level::Debug: return "\x1B[96m"; // голубой
        case Level::Info:  return "\x1B[97m"; // белый
        case Level::Ok:    return "\x1B[92m"; // зелЄный
        case Level::Warn:  return "\x1B[93m"; // жЄлтый
        case Level::Error: return "\x1B[91m"; // красный
        case Level::Hook:  return "\x1B[95m"; // пурпурный
        case Level::D3D:   return "\x1B[94m"; // синий
        case Level::SAMP:  return "\x1B[93m"; // жЄлтый
        }
        return "\x1B[97m";
    }

    static void Write(Level level, const std::string& text) {
        SYSTEMTIME time{};
        GetLocalTime(&time);

        std::ostringstream stream;
        stream << std::setfill('0')
            << std::setw(2) << time.wHour << ":"
            << std::setw(2) << time.wMinute << ":"
            << std::setw(2) << time.wSecond << "."
            << std::setw(3) << time.wMilliseconds;

        std::ostringstream line;
        line << "\x1B[90m[" << stream.str() << "]"
            << " [" << GetCurrentThreadId() << "]"
            << LevelColor(level)
            << " [" << LevelTag(level) << "] "
            << text
            << "\x1B[0m";

        std::cout << line.str() << std::endl;
    }
};