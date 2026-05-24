#pragma execution_character_set("utf-8")
/*==============================================================================
   デバッグモデルビューアシーン [debug_model_scene.cpp]
   asset\model フォルダ内の .fbx / .glb を自動列挙し、グリッド状に配置して表示する。
   Ghost と家具憑依機能のみを残し、モデルに近づくとモデル名と
   block_definitions.json への登録状況を描画する。
==============================================================================*/

#include <d3d11.h>
#include <DirectXMath.h>
#include <windows.h>
#include <string>
#include <vector>
#include <cmath>
#include <fstream>
#include <map>
#include "debug_model_scene.h"
#include "renderer.h"
#include "light.h"
#include "camera.h"
#include "model.h"
#include "sprite3d.h"
#include "sprite2d.h"
#include "billboard.h"
#include "font.h"
#include "keyboard.h"
#include "mouse.h"
#include "define.h"
#include "texture.h"
#include "debugcamera.h"

using namespace DirectX;

// ======================================================
// 内部構造体
// ======================================================

// 展示モデル1体分の情報
struct DebugModelEntry
{
	std::string fileName;       // "bathtub.fbx" など
	std::string filePath;       // "asset\\model\\bathtub.fbx"
	XMFLOAT3    worldPos;       // 配置ワールド座標
	MODEL*      pModel;         // 読み込んだモデル
};

// ======================================================
// 静的変数
// ======================================================
static std::vector<DebugModelEntry> g_Entries;
static AmbientLight* g_pAmbientLight = nullptr;
static PointLight* g_pFloorLight = nullptr;

// 近接表示用フォント
static FontRenderer* g_pModelNameFont = nullptr;
static FontRenderer* g_pSubInfoFont = nullptr;
static FontRenderer* g_pControlHintFont = nullptr;

// 床描画用
static Billboard* g_pFloorBillboard = nullptr;

// 配置パラメータ
static const float MODEL_SPACING = 5.0f;  // モデル同士の間隔
static const float LABEL_RANGE = 8.0f;  // この距離以内で名前を表示

// 原点キューブ表示用
static MODEL* g_pCubeModel = nullptr;
static bool   g_ShowOriginCubes = false;

// リロード用フラグ
static bool g_ReloadRequested = false;

// ======================================================
// asset\model フォルダを列挙して .fbx / .glb を集める
// ======================================================
static void EnumerateModels()
{
	// 検索する拡張子パターン一覧
	const char* patterns[] = {
		"asset\\model\\*.fbx",
		"asset\\model\\*.glb",
	};

	for (const char* pattern : patterns)
	{
		WIN32_FIND_DATAA fd;
		HANDLE hFind = FindFirstFileA(pattern, &fd);
		if (hFind == INVALID_HANDLE_VALUE) continue;

		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

			DebugModelEntry entry;
			entry.fileName = fd.cFileName;
			entry.filePath = "asset\\model\\" + entry.fileName;
			entry.worldPos = { 0, 0, 0 };
		entry.pModel = nullptr;

			// block_definitions.json にあるか確認

			g_Entries.push_back(entry);
		} while (FindNextFileA(hFind, &fd));

		FindClose(hFind);
	}
}

// ======================================================
// モデルの再読み込み（家具とキャッシュを破棄して再構築）
// ======================================================
static void ReloadAllModels()
{
	// ゴーストの現在位置を保持

	// 家具を全て破棄（内部で各モデルのModelRelease/GlbModel::Releaseも呼ばれる）
	for (int i = 0; i < (int)g_Entries.size(); i++)
	{
		if (g_Entries[i].pModel) { ModelRelease(g_Entries[i].pModel); }
	}

	// 原点キューブモデルを解放
	if (g_pCubeModel) { ModelRelease(g_pCubeModel); g_pCubeModel = nullptr; }

	// モデル一覧を再列挙
	g_Entries.clear();
	EnumerateModels();

	// グリッド配置 + 家具再生成
	int cols = 6;
	for (int i = 0; i < (int)g_Entries.size(); i++)
	{
		int row = i / cols;
		int col = i % cols;
		float x = (col - cols / 2.0f + 0.5f) * MODEL_SPACING;
		float z = (float)row * MODEL_SPACING;
		float y = 0.0f;

		g_Entries[i].worldPos = { x, y, z };
		g_Entries[i].pModel = ModelLoad(g_Entries[i].filePath.c_str());
	}

	// 原点キューブモデルを再読み込み
	g_pCubeModel = ModelLoad("asset\\model\\cube.fbx");

	// ゴーストの位置を復元

}

// ======================================================
// Initialize
// ======================================================
void DebugModelScene_Initialize(void)
{
	// ライト
	g_pAmbientLight = new AmbientLight(XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f));

	// 床用バッファ・テクスチャの作成
	g_pFloorBillboard = new Billboard(XMFLOAT3(0.0f, -0.5f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(90.0f, 0.0f, 0.0f), "asset\\texture\\tex.png", false);
	g_pFloorBillboard->SetBillboardMode(false);

	// 原点表示用キューブモデルの読み込み
	g_pCubeModel = ModelLoad("asset\\model\\cube.fbx");
	g_ShowOriginCubes = false;

	// モデル列挙
	EnumerateModels();

	// グリッド配置: 1列あたりのモデル数
	int cols = 6;
	for (int i = 0; i < (int)g_Entries.size(); i++)
	{
		int row = i / cols;
		int col = i % cols;
		float x = (col - cols / 2.0f + 0.5f) * MODEL_SPACING;
		float z = (float)row * MODEL_SPACING;
		float y = 0.0f;

		g_Entries[i].worldPos = { x, y, z };
		g_Entries[i].pModel = ModelLoad(g_Entries[i].filePath.c_str());
	}

	// プレイヤー初期化 (内部でCamera_InitializeやLockMouseが呼ばれる)
	DebugCamera_Initialize();

	// フォント
	g_pModelNameFont = new FontRenderer({ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 140.0f }, 36.0f, 0.0f, { 1,1,1,1 }, "");
	g_pSubInfoFont = new FontRenderer({ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 90.0f }, 28.0f, 0.0f, { 0.8f,0.8f,0.2f,1 }, "");
	g_pControlHintFont = new FontRenderer({ SCREEN_WIDTH / 2.0f,                  30.0f }, 22.0f, 0.0f, { 0.6f,0.6f,0.6f,1 }, "WASD:Move  Mouse:Look  SPACE:Possess  E:Release  B:OriginCube  R:Reload");
	g_pControlHintFont->PreCacheGlyphs();

}

// ======================================================
// Update
// ======================================================
void DebugModelScene_Update(void)
{

	// Bキーで原点キューブ表示切り替え
	if (Keyboard_IsKeyDownTrigger(KK_B))
	{
		g_ShowOriginCubes = !g_ShowOriginCubes;
	}

	// Rキーで全モデルを再読み込み
	if (Keyboard_IsKeyDownTrigger(KK_R))
	{
		ReloadAllModels();
	}

	// プレイヤー(カメラ)の更新
	DebugCamera_Update();
	SetCameraPosition(GetCamera()->GetPos());

	float nearestDist = FLT_MAX;
	int nearestIdx = -1;

	if (nearestIdx >= 0 && nearestDist < LABEL_RANGE)
	{
		const DebugModelEntry& e = g_Entries[nearestIdx];

		// モデル名（ファイル名）
		g_pModelNameFont->SetText(e.fileName);
		g_pModelNameFont->PreCacheGlyphs();
	}
	else
	{
		g_pModelNameFont->SetText("");
		g_pSubInfoFont->SetText("");
	}
}

// ======================================================
// Draw
// ======================================================
void DebugModelScene_Draw(void)
{
	// 3D 描画
	SetDepthEnable(true);

	// --- 床の描画 ---
	if (g_pFloorBillboard)
	{
		// モデル配置範囲に合わせて床タイルを敷く
		int cols = 6;
		int rows = ((int)g_Entries.size() + cols - 1) / cols;
		float halfW = (cols / 2.0f) * MODEL_SPACING + MODEL_SPACING;
		float maxZ = (float)(rows) * MODEL_SPACING + MODEL_SPACING;
		float minZ = -MODEL_SPACING;

		float tileSize = 1.0f;
		int tilesX = (int)((halfW * 2.0f) / tileSize) + 2;
		int tilesZ = (int)((maxZ - minZ) / tileSize) + 2;
		float startX = -halfW;
		float startZ = minZ;

		for (int iz = 0; iz < tilesZ; iz++)
		{
			for (int ix = 0; ix < tilesX; ix++)
			{
				float x = startX + ix * tileSize + tileSize * 0.5f;
				float z = startZ + iz * tileSize + tileSize * 0.5f;
				float y = -0.5f;

				g_pFloorBillboard->SetPos({ x, y, z });
				g_pFloorBillboard->SetSize({ tileSize, tileSize });
				g_pFloorBillboard->Draw();
			}
		}
	}

	// 家具描画（展示モデル＋ビルボードアイコン含む）
	for (int i = 0; i < (int)g_Entries.size(); i++)
	{
		if (g_Entries[i].pModel)
		{
			ModelDraw(g_Entries[i].pModel, g_Entries[i].worldPos, {0,0,0}, {1,1,1});
		}
	}

	// 原点キューブ描画
	if (g_ShowOriginCubes && g_pCubeModel)
	{
		for (int i = 0; i < (int)g_Entries.size(); i++)
		{
			ModelDraw(
				g_pCubeModel,
				g_Entries[i].worldPos,
				{ 0.0f, 0.0f, 0.0f },
				{ 1.0f, 1.0f, 1.0f }
			);
		}
	}
	// 2D 描画
	SetDepthEnable(false);
	Sprite_BeginDraw2D();

	if (g_pModelNameFont)   g_pModelNameFont->Draw();
	if (g_pSubInfoFont)     g_pSubInfoFont->Draw();
	if (g_pControlHintFont) g_pControlHintFont->Draw();

	// プレイヤー描画追加
	DebugCamera_Draw();

	// ポーズメニュー描画（2D描画内で最後に描く）

	Sprite_EndDraw2D();
}

// ======================================================
// Finalize
// ======================================================
void DebugModelScene_Finalize(void)
{
	for (int i = 0; i < (int)g_Entries.size(); i++)
	{
		if (g_Entries[i].pModel) { ModelRelease(g_Entries[i].pModel); }
	}
	g_Entries.clear();
	DebugCamera_Finalize();
	Camera_Finalize();

	if (g_pAmbientLight) { delete g_pAmbientLight; g_pAmbientLight = nullptr; }
	if (g_pFloorLight) { delete g_pFloorLight;   g_pFloorLight = nullptr; }

	// 原点キューブモデル解放
	if (g_pCubeModel) { ModelRelease(g_pCubeModel); g_pCubeModel = nullptr; }

	// 床リソース解放
	if (g_pFloorBillboard) { delete g_pFloorBillboard; g_pFloorBillboard = nullptr; }

	if (g_pModelNameFont) { delete g_pModelNameFont;   g_pModelNameFont = nullptr; }
	if (g_pSubInfoFont) { delete g_pSubInfoFont;     g_pSubInfoFont = nullptr; }
	if (g_pControlHintFont) { delete g_pControlHintFont; g_pControlHintFont = nullptr; }
}
