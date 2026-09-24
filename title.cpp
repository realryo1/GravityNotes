#include "title.h"
#include "sprite2d.h"
#include "texture.h"
#include "input_manager.h"
#include "fade.h"
#include "debug_ostream.h"
#include "define.h"
#include "font.h"
#include "mouse.h"
#include "sound.h"
#include "ClickFont.h"
#include "Movie.h"

using namespace DirectX;

// ①インスタンス、ポインタ用意
static Sprite2D* g_pTitleSprite = nullptr;
static ClickFont* g_pChangeSceneText = nullptr;
static ClickFont* g_pDebugSceneText = nullptr;
static Movie* g_pTitleMovie;
static Movie* g_pLogoMovieBB;
static int g_LogoFrameCount = 0;
static SoundData* g_pTitleBGM = nullptr;


void Title_Initialize(void)
{
	g_pTitleMovie = new Movie(
		{ SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 },					//位置
		{ SCREEN_WIDTH },											//サイズ
		0.0f,														//回転（度）
		{ 1.0f,1.0f,1.0f, 1.0f },
		BLENDSTATE_NONE,
		L"asset\\movie\\title_nologo.mp4"
	);

	g_LogoFrameCount = 0;

	g_pLogoMovieBB = new Movie(
		{ SCREEN_WIDTH / 3 * 2 - 10.0f , SCREEN_HEIGHT / 3 - 60.0f},					//位置
		{ 500.0f },											//サイズ
		0.0f,														//回転（度）
		{ 1.0f,1.0f,1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\movie\\LogoAnimationSpriteSheet_30fps.mp4",
		true,
		false,
		false
	);

	g_pChangeSceneText = new ClickFont(
		{ SCREEN_WIDTH / 3.0f, 600.0f },			//位置
		50.0f,														//文字サイズ
		0.0f,														//回転（度）
		{ 1.0f, 1.0f, 1.0f, 1.0f },									//通常色
		{ 1.0f, 0.8f, 0.2f, 1.0f },									//ホバー色
		"Press A"										//テキスト
	);

	UnLockMouse();//マウスアンロック

	g_pTitleBGM = LoadMP3("asset/sound/se/title_kaigan.wav");
	if (g_pTitleBGM)
	{
		PlaySound(g_pTitleBGM, true);
	}
}



void Title_Update(void)
{
	//③処理
	g_pTitleMovie->Update();

	g_LogoFrameCount++;
	if (g_LogoFrameCount >= 80)
	{
		if (g_pLogoMovieBB)
		{
			g_pLogoMovieBB->Play();
		}
	}

	if (g_pLogoMovieBB)
	{
		g_pLogoMovieBB->Update();
	}
	g_pChangeSceneText->Update();

	//ClickFontがクリックされた、または決定ボタンが押された
	if (g_pChangeSceneText->IsClick() || Input_IsActionTrigger(INPUT_ACTION_DECIDE))
	{
		SetSceneFade(SCENE_STAGESELECT);
	}

}

void Title_Draw(void)
{
	//④描画
	g_pTitleMovie->Draw();
	if (g_pLogoMovieBB)
	{
		g_pLogoMovieBB->Draw();
	}
	g_pChangeSceneText->Draw();
}

void Title_Finalize(void)
{
	//⑤解放
	SAFE_DELETE(g_pTitleMovie);
	SAFE_DELETE(g_pLogoMovieBB);
	SAFE_DELETE(g_pChangeSceneText);

	if (g_pTitleBGM)
	{
		StopSound(g_pTitleBGM);
		UnloadSound(g_pTitleBGM);
		g_pTitleBGM = nullptr;
	}
}
