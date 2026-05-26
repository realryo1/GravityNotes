#include "debug_lighting_scene.h"
#include "light.h"
#include "sprite2d.h"
#include "texture.h"
#include "keyboard.h"
#include "fade.h"
#include "debug_ostream.h"
#include "define.h"
#include "renderer.h"
#include "font.h"
#include "mouse.h"
#include "sound.h"
#include "sprite3d.h"
#include "ClickFont.h"
#include "camera.h"
#include "movie.h"
#include "debugcamera.h"
#include <string>
#include <cmath>

using namespace DirectX;

static Sprite3D* g_pKirbyModel = nullptr;
static PointLight* g_pMainLight = nullptr;
static AmbientLight g_ambientLight(XMFLOAT4(0.08f, 0.08f, 0.08f, 1.0f));
Movie* g_pMovie;


void DebugLightingScene_Initialize(void)
{
	g_pKirbyModel = new Sprite3D(
		{ 0.0f, 0.0f, 5.0f },	//位置
		{ 1.0f, 1.0f, 1.0f },	//スケール
		{ 0.0f, 0.0f, 0.0f },	//回転（度）
		"asset\\model\\cube.fbx", //モデルパス
		S_LAMBERT
	);

	g_pMainLight = new PointLight(
		TRUE,
		{ 0.0f, 5.0f, -5.0f, 1.0f },
		{ 0.0f, -1.0f, 0.5f, 0.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		20.0f,
		1.0f
	);

	g_pMainLight->Apply(g_ambientLight);

	DebugCamera_Initialize();
	if (GetCamera())
	{
		GetCamera()->SetTargetPos(g_pKirbyModel->GetPos());
		SetCameraPosition(GetCamera()->GetPos());
	}

	g_pMovie = new Movie(
		{ 100.0f ,100.0f },
		200.0f,
		0.0f,
		{ 1.0f,1.0f,1.0f, 1.0f },
		BLENDSTATE_NONE,
		L"asset\\movie\\nullmovie.mp4"
	);


	LockMouse();
}

void DebugLightingScene_Update(void)
{
	DebugCamera_Update();

	if (GetCamera())
	{
		GetCamera()->SetTargetPos(g_pKirbyModel->GetPos());
		SetCameraPosition(GetCamera()->GetPos());
	}

	g_pMainLight->Apply(g_ambientLight);

	g_pMovie->Update();

}

void DebugLightingScene_Draw(void)
{
	SetDepthEnable(true);

	g_pKirbyModel->Draw();

	SetDepthEnable(false);
	g_pMovie->Draw();

}

void DebugLightingScene_Finalize(void)
{
	delete g_pKirbyModel;
	g_pKirbyModel = nullptr;

	delete g_pMainLight;
	g_pMainLight = nullptr;

	delete g_pMovie;
	g_pMovie = nullptr;

	DebugCamera_Finalize();
}
