// =========================================================
// fade.h フェード制御
// =========================================================
#ifndef _FADE_H_
#define _FADE_H_

#include "sprite2d.h"
#include "scene.h"

// =========================================================
// 列挙体宣言
// =========================================================
enum FADESTAT
{
	FADE_NONE = 0,
	FADE_OUT,		// 暗くなる
	FADE_IN,		// 明るくなる
	FADE_MAX,		// 真っ暗で待機
	FADE_WAIT_LOAD	// 完全白で1フレーム待機後にロード
};

// =========================================================
// Spriteを継承したFadeクラス
// =========================================================
class Fade : public Sprite2D
{
private:
	FADESTAT m_State;
	SCENE m_NextScene;

public:
	// コンストラクタ・デストラクタ
	Fade();
	~Fade();

	// 更新処理
	void Update();

	// フェード開始
	void StartFade(SCENE next = SCENE_NONE);

	// フェードイン開始
	void StartFadeIn();

	// ゲッター
	FADESTAT GetState() const;
};

// =========================================================
// モジュール関数（グローバル関数）
// =========================================================
void Fade_Initialize(void);
void Fade_Update(void);
void Fade_Draw(void);
void Fade_Finalize(void);

void StartFade(SCENE ns = SCENE_NONE);
void Fade_StartIn(void);
FADESTAT GetFadeState(void);

#endif