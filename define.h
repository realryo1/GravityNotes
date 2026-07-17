#pragma once

//==============================================================================
// プロジェクト全体の定数定義ファイル
//==============================================================================

#define _CRT_SECURE_NO_WARNINGS

#include "version.h"

#define CLASS_NAME L"GravityNote"
#define WINDOW_CAPTION L"GravityNote " VERSION_STR

#define SCREEN_WIDTH (1280.0f)	// UI要素の配置に使う（いままで通り）
#define SCREEN_HEIGHT (720.0f)	// UI要素の配置に使う（いままで通り）
#define DRAW_SCREEN_WIDTH  (3840.0f)   // （最終的な描画解像度）　実際の配置には使わない！！！
#define DRAW_SCREEN_HEIGHT (2160.0f)   // （最終的な描画解像度）　実際の配置には使わない！！！
#define DRAW_SCALE_X (DRAW_SCREEN_WIDTH  / SCREEN_WIDTH)   // 描画倍率X
#define DRAW_SCALE_Y (DRAW_SCREEN_HEIGHT / SCREEN_HEIGHT)  // 描画倍率Y
#define WIN32_LEAN_AND_MEAN	//32bitアプリには不要な情報を無視
#define FPS (60)


#if defined(_DEBUG)
//=== デバッグ関連定数 ===

#else
//==================================
//          ここは触らない
//==================================

//==================================
//          ここは触らない
//==================================
#endif


//=== Sound 関連定数 ===
#define SOUND_BGM_VOLUME (0.3f)
#define SOUND_SE_VOLUME  (0.4f)

//=== Camera 関連定数 ===
#define PITCH_LIMIT_LOOK_UP    (25.0f)   // 上を見る限界（カメラが下がる限界）: 床埋まり防止
#define PITCH_LIMIT_LOOK_DOWN  (-60.0f)  // 下を見る限界（カメラが上がる限界）: 天井埋まり防止
#define MOUSE_SENSITIVITY (0.15f)
#define CAMERA_DISTANCE (6.0f)  // カメラ距離
#define CAMERA_OFFSET_Y (1.5f)  // 注視点のオフセット