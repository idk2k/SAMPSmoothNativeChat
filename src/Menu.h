#pragma once

#include <d3d9.h>

namespace menu {
void Init(IDirect3DDevice9* device);
void Render(IDirect3DDevice9* device);
bool IsInitialized();
} // namespace menu