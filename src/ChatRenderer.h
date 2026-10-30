#pragma once
#include <d3d9.h>

bool CreateGraphics(IDirect3DDevice9* device);
void ReleaseGraphics();
void RenderChat(IDirect3DDevice9* device);