# SAMPSmoothNativeChat

A complete replacement for the native SA-MP chat with a custom renderer built on pure DirectX 9.
Features smooth spring-based scrolling, timestamps, `{RRGGBB}` colored messages,
DXUT scrollbar synchronization, and JSON-based configuration — without ImGui or DXUT.

![Platform](https://img.shields.io/badge/platform-Windows%20x86-blue)
![Game](https://img.shields.io/badge/game-GTA%20SA%201.0-green)
![SAMP](https://img.shields.io/badge/SA--MP-0.3.7%20R5-yellow)

---

## Features

- Complete replacement of the native SA-MP chat with a custom renderer (`ID3DXFont` + `ID3DXSprite`)
- Rendering in `EndScene` — no ImGui, DXUT, or extra dependencies
- Smooth spring-damper scroll physics (`position / velocity / target`)
- Auto-stick to the bottom when new messages arrive
- Correct behavior while reading history (lines don't "jump")
- Support for `{RRGGBB}` color codes inside messages
- `[HH:MM]` timestamps for each message
- Synchronization with the native DXUT scrollbar
- Mouse-draggable scrollbar with chat synchronization
- Feedback-loop protection (grace period after dragging)
- Gradient semi-transparent background when the input box is open
- JSON-based configuration — no rebuild required
- Colored logger with levels, millisecond timestamps, and thread IDs
- Clean architecture: physics / buffer / renderer / scrollbar are separated

---

## Compatibility

| Component | Support |
|---|---|
| Game | GTA San Andreas 1.0 US (HUD/rendering via D3D9) |
| Multiplayer | SA-MP 0.3.7 **R5** (R5-1 / R5-2) |
| Architecture | Windows x86 (32-bit) |
| DirectX | DirectX 9.0c |
| Compiler | MSVC (Visual Studio 2019/2022), C++17 |
| Build | Debug / Release, `/MT` or `/MD` |
| ImGui / DXUT | Not used |

> ⚠️ SA-MP function addresses are specific to **R5**. The plugin will not work
> on other versions (R1, 0.3.DL, 0.3.8, etc.) without updating the addresses
> in `ChatHooks.cpp` and `dllmain.cpp`.

---

## Installation

1. Build the project in Visual Studio (x86, Release).
2. Copy the resulting `.asi` file to the game folder:
   ```text
   GTA San Andreas/scripts/SAMPSmoothNativeChat.asi
   ```
3. Copy `config.json` next to the `.asi` file:
   ```text
   GTA San Andreas/scripts/config.json
   ```
4. Make sure an ASI Loader is installed (e.g., `vorbisFile.dll` / `Ultimate ASI Loader`).
5. Launch the game and connect to a server.

When the first message appears in the chat, the native DXUT scrollbar
will show up, synchronized with your scroll physics.

---

## Configuration

All parameters are configured in `config.json` next to the `.asi` file:

```json
{
    "chat": {
        "x": 45,
        "y": 40,
        "width": 758,
        "height": 200,
        "line_height": 20,
        "font_size": 18,
        "max_messages": 300
    },
    "scroll": {
        "lines_per_click": 3.0,
        "stiffness": 220.0,
        "friction": 32.0
    },
    "timestamps": {
        "enabled": true,
        "color": "#969696"
    },
    "background": {
        "enabled": true,
        "top_alpha": 16,
        "bottom_alpha": 144
    },
    "scrollbar": {
        "enabled": true
    },
    "log": {
        "level": "info"
    }
}
```

### Parameter groups

| Group | Purpose |
|---|---|
| `chat` | Position, size, font, message history limit |
| `scroll` | Scroll speed and spring physics |
| `timestamps` | Timestamp enable/disable and color |
| `background` | Chat background and transparency gradient |
| `scrollbar` | DXUT scrollbar synchronization enable/disable |
| `log` | Logging level |

- If `config.json` is missing or corrupted, default values are used.
- Changes take effect after restarting the game.

---

## Architecture

```text
DllMain
└── InitializationThread (waits for CChatInfo)
    ├── InstallD3DHooks (EndScene, Reset)
    └── InstallChatHooks (AddEntry, Render, Scroll)

EndScene (every frame)
├── ScrollPhysics.Update(dt)      — spring physics
├── RenderChat()
│   ├── background (ID3DXSprite, gradient)
│   └── text (ID3DXFont, {RRGGBB} segments)
└── IScrollbarView.SetPosition()  — DXUT scrollbar sync
```

### Modules

| Module | File | Purpose |
|---|---|---|
| Config | `Config.h`, `ConfigLoader.cpp` | JSON configuration |
| Physics | `ScrollPhysics.h` | Spring-based scrolling |
| Parser | `ChatTextParser.h` | `{RRGGBB}` → color segments |
| Buffer | `ChatBuffer.h` | Custom message history |
| SA-MP hooks | `ChatHooks.cpp` | `AddEntry`, `Render`, `Scroll` |
| D3D hooks | `D3DHooks.cpp` | `EndScene`, `Reset` |
| Renderer | `ChatRenderer.cpp` | Background + text + clipping |
| Scrollbar | `ScrollbarView.h`, `NativeDxutScrollbarView.h` | Abstraction + DXUT |

---

## Used addresses (SA-MP 0.3.7 R5)

| Address (RVA) | Purpose |
|---|---|
| `0x26EB80` | `CChat*` (pointer to the chat structure) |
| `0x26EB84` | `CInput*` (pointer to the input box) |
| `0x673C0` | `CChat::Scroll(int)` |
| `0x67470` | `CChat::RenderEntry(...)` |
| `0x67BE0` | `CChat::AddEntry(...)` |
| `0x67120` | `CChat::RecalcFontSize()` |
| `0x67940` | `CChat::Render()` |
| `0x887B0` | `CDXUTScrollBar::Scroll(int)` |

All addresses are relative (`samp.dll base + RVA`).

---

## Building

1. Open the solution in Visual Studio 2019/2022.
2. Select the **x86** platform.
3. Make sure the following are linked:
   - `d3d9.lib`
   - `d3dx9.lib`
   - `MinHook.x86.lib`
4. Build **Release | x86**.
5. The resulting `.asi` file will be in the output folder.

### Dependencies

- [MinHook](https://github.com/TsudaKageyu/minhook) — x86
- [nlohmann/json](https://github.com/nlohmann/json) — header-only
- DirectX SDK (for `d3dx9.lib`)

---

## Logging

The plugin writes colored logs to the console:

```text
[12:27:41.123] [12345] [SAMP ] AddEntry: type=4, prefix='Tupak_Rahmed', text='Hello'
[12:27:41.130] [12345] [D3D  ] EndScene hook installed
[12:27:41.135] [12345] [OK   ] Graphics created
[12:27:41.140] [12345] [HOOK ] CChat::AddEntry installed
```

Levels: `TRACE`, `DEBUG`, `INFO`, `OK`, `WARN`, `ERROR`, `HOOK`, `D3D`, `SAMP`.

---

## Known limitations

- Addresses are tied to SA-MP 0.3.7 R5 — other versions require an address update.
- The native chat renderer is fully disabled — CLEO mods that draw on top
  of the chat may conflict.
- The chat background only appears when the input box is open (by design).
- Duplicate messages from the server are not filtered (can be added if needed).

---

## License

MIT — use, modify, and distribute freely.

---

## Credits

- [MinHook](https://github.com/TsudaKageyu/minhook) — for the convenient hooking API
- [nlohmann/json](https://github.com/nlohmann/json) — for the JSON parser
- SA-MP reverse engineering community — for knowledge bases and addresses