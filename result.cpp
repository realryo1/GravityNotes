#include "result.h"
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
#include "ClickFont.h"
#include "movie.h"
#include <string>
#include <cmath>

using namespace DirectX;

// ①Spriteのインスタンス、ポインタ用意
static Sprite2D* g_pResultSprite = nullptr;

void Result_Initialize(void)
{
	// ②各種初期化
	g_pResultSprite = new Sprite2D(
		{ SCREEN_WIDTH / 2 - 200.0f, SCREEN_HEIGHT / 2.0f - 100.0f },	//位置
		{ SCREEN_WIDTH * 0.7f, SCREEN_HEIGHT * 0.7f },					//サイズ
		0.0f,															//回転（度）
		{ 1.0f, 1.0f, 1.0f, 1.0f },										//RGBA
		BLENDSTATE_NONE,												//BlendState
		L"asset\\texture\\tex.png"									//テクスチャパス
	);

	UnLockMouse();//マウスアンロック
}

void Result_Update(void)
{
	// ③適当な処理　アニメーションなどもここで
	if (Keyboard_IsKeyDownTrigger(KK_SPACE))
	{
		StartFade(SCENE_TITLE);
	}
}

void Result_Draw(void)
{
	g_pResultSprite->Draw();
}

void Result_Finalize(void)
{
	delete g_pResultSprite;
	g_pResultSprite = nullptr;

}
