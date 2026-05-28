#include <d3d11.h>
#include <windows.h>
#include <string>
#include <cmath>
#include <DirectXMath.h>
#include "renderer.h"
#include "debug_ostream.h"
#include "game.h"
#include "texture.h"
#include "keyboard.h"
#include "scene.h"
#include "camera.h"
#include "sprite2d.h"
#include "fade.h"
#include "sound.h"
#include "mouse.h"
#include "model.h"
#include "debugcamera.h"

using namespace DirectX;

static SoundData* g_pBGM = nullptr;

void Game_Initialize(void)
{
	DebugCamera_Initialize();
}

void Game_Update(void)
{
	DebugCamera_Update();
}

void Game_Draw(void)
{
	//3D描画
	{
		SetDepthEnable(true);

		//この中に書く

		SetDepthEnable(false);
	}

	//2D描画
	{
		Sprite_BeginDraw2D();

		//この中に書く

		Sprite_EndDraw2D();
	}
}

void Game_Finalize(void)
{
	DebugCamera_Finalize();
	
	Camera_Finalize();

	if (g_pBGM) {
		StopSound(g_pBGM);
		UnloadSound(g_pBGM);
		g_pBGM = nullptr;
	}
}