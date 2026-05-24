#include "scene.h"
#include "game.h"
#include "renderer.h"
#include "keyboard.h"
#include "texture.h"
#include "title.h"
#include "result.h"
#include "debug_model_scene.h"
#include "debug_lighting_scene.h"
#include "debugscene/debugscore.h"
#include "define.h"
using namespace DirectX;

static SCENE scene = SCENE_TITLE;

void Init(void)
{
	switch (scene)
	{	
	case SCENE_TITLE:
		Title_Initialize();
		break;
	case SCENE_GAME:
		Game_Initialize();
		break;
	case SCENE_RESULT:
		Result_Initialize();
		break;	
	case SCENE_DEBUG_MODEL:
		DebugModelScene_Initialize();
		break;
	case SCENE_DEBUG_LIGHTING:
		DebugLightingScene_Initialize();
		break;
	default:
		break;
	}
}

void Update(void)
{
	switch (scene)
	{
	case SCENE_TITLE:
		Title_Update();
		break;
	case SCENE_GAME:
		Game_Update();
		break;
	case SCENE_RESULT:
		Result_Update();
		break;
	case SCENE_DEBUG_MODEL:
		DebugModelScene_Update();
		break;
	case SCENE_DEBUG_LIGHTING:
		DebugLightingScene_Update();
		break;
	case SCENE_DEBUG_SCORE:
		Debugscore_Update();
		break;
	default:
		break;
	}
}

void Draw(void)
{
	switch (scene)
	{
	case SCENE_TITLE:
		Title_Draw();
		break;
	case SCENE_GAME:
		Game_Draw();
		break;
	case SCENE_RESULT:
		Result_Draw();
		break;
	case SCENE_DEBUG_MODEL:
		DebugModelScene_Draw();
		break;
	case SCENE_DEBUG_LIGHTING:
		DebugLightingScene_Draw();
		break;
	case SCENE_DEBUG_SCORE:
		Debugscore_Draw();
		break;
	default:
		break;
	}
}

void Finalize(void)
{
	switch (scene)
	{
	case SCENE_TITLE:
		Title_Finalize();
		break;
	case SCENE_GAME:
		Game_Finalize();
		break;
	case SCENE_RESULT:
		Result_Finalize();
		break;
	case SCENE_DEBUG_MODEL:
		DebugModelScene_Finalize();
		break;
	case SCENE_DEBUG_LIGHTING:
		DebugLightingScene_Finalize();
		break;
	case SCENE_DEBUG_SCORE:
		Debugscore_Finalize();
		break;
	default:
		break;
	}
}

void SetScene(SCENE id)
{
	Finalize();

	scene = id;

	Init();
}

SCENE GetScene(void)
{
	return scene;
}
