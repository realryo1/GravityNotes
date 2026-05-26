#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

//=== DebugScore 関連定数 ===
#define DEBUGSCORE_INITIAL_HIGHSPEED (5.0f)  // ハイスピードの初期値
#define DEBUGSCORE_HIGHSPEED_STEP    (0.1f)  // ハイスピードの変更ステップ
#define DEBUGSCORE_HIGHSPEED_MIN     (0.1f)  // ハイスピードの最小値
#define DEBUGSCORE_HIGHSPEED_MAX     (4.0f)  // ハイスピードの最大値

void Debugscore_Initialize(void);
void Debugscore_Update(void);
void Debugscore_Draw(void);
void Debugscore_Finalize(void);
