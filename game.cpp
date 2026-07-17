#include "game.h"
#include "define.h"
#include "sprite2d.h"
#include "texture.h"
#include "fade.h"
#include "debug_ostream.h"
#include "font.h"
#include "mouse.h"
#include "keyboard.h"
#include "input_manager.h"
#include "model.h"
#include "debugcamera.h"
#include "debug_ui.h"
#include "debug_params.h"
#include "../framework/imgui/imgui.h"
#include "sound.h"
#include "ClickFont.h"
#include "scene.h"
#include "camera.h"
#include "field.h"
#include "player.h"
#include "gamecamera.h"
#include "note_manager.h"
#include "light_game.h"
#include "status_manager.h"
#include "game_ui.h"
#include "options_manager.h"

using namespace DirectX;

// ゲーム状態の定義
enum class GameState {
	PLAYING,
	FINISHED_WAIT,    // 終了検知後、表示開始までの2秒待機中（何も表示しない）
	FINISHED_DISPLAY, // 2秒経過後、ロゴ等を表示する終了演出中（2秒間表示して自動遷移）
};

static GameState      g_GameState = GameState::PLAYING;
static float          g_FinishTimer = 0.0f;
static float          g_FinishWaitDuration = 3.0f;
static float          g_EscapeHoldTimer = 0.0f;
static bool           g_IsStageSelectRequested = false;
static bool           g_IsPaused = false;

bool IsGamePlaying(void)
{
	return g_GameState == GameState::PLAYING;
}

// ①インスタンス、ポインタ用意
static Sprite2D* g_pGameSprite = nullptr;
static ClickFont* g_pChangeSceneText = nullptr;
static FontRenderer* g_pSelectedJsonText = nullptr;
static Field* g_pField = nullptr;
static Player* g_pPlayer = nullptr;
static NoteManager* g_pNoteManager = nullptr;
static StatusManager* g_pStatusManager = nullptr;
static bool           g_IsMouseCursorVisible = false;
static GameUI* g_pGameUI = nullptr;

// プリロード対象のモデルポインタ保持
static const char* g_PreloadModelPaths[] = {
	"asset/model/knight_02.fbx",
	"asset/model/field_allnormal.fbx",
	"asset/model/field_hasiranashi.fbx",
	"asset/model/Gargoyle.fbx",
	"asset/model/barrier.fbx"
};
static const int G_PRELOAD_MODEL_COUNT = sizeof(g_PreloadModelPaths) / sizeof(g_PreloadModelPaths[0]);
static MODEL* g_pPreloadedModels[G_PRELOAD_MODEL_COUNT] = { nullptr };


void Game_Initialize(void)
{
	//int pad = Gamepad_FindConnectedPlayer();
	//if (pad < 0)return;//デバック時必要なし

	// 主要モデルのプリロード
	for (int i = 0; i < G_PRELOAD_MODEL_COUNT; i++)
	{
		if (!g_pPreloadedModels[i])
		{
			g_pPreloadedModels[i] = ModelLoad(g_PreloadModelPaths[i]);
		}
	}

	// テクスチャのプリロード（スタッター防止）
	LoadTexture(L"asset/texture/30ver.png");
	LoadTexture(L"asset/texture/effect_slash_ver01.png");
	LoadTexture(L"asset/texture/effect_windCut_ver01.png");
	LoadTexture(L"asset/texture/enemy_defeat_particle.png");
	LoadTexture(L"asset/texture/OrbAnimationSpriteSheetBlue.png");
	LoadTexture(L"asset/texture/OrbAnimationSpriteSheetRed.png");
	LoadTexture(L"asset/texture/rainbow_start.png");

	// 各状態の初期化
	g_GameState = GameState::PLAYING;
	g_FinishTimer = 0.0f;
	g_EscapeHoldTimer = 0.0f;
	g_IsStageSelectRequested = false;
	g_IsPaused = false;

	//各種初期化
	GameCamera::Init();
	GameLight::Init();

	g_pField = new Field();
	g_pField->Init();

	g_pStatusManager = new StatusManager();
	g_pStatusManager->Init();

	g_pNoteManager = new NoteManager();

	g_pNoteManager->Init(GetPlayJson());
	//g_pNoteManager->Init("asset/score/shiningstar.json");

	g_pPlayer = new Player();
	g_pPlayer->Init(g_pNoteManager, g_pStatusManager);

	g_pGameUI = new GameUI();
	g_pGameUI->Init();

	UnLockMouse();//マウスアンロック
}

void Game_Update(void)
{
	// ESCキーを3秒間押し続けるとステージ選択へ戻る。
	if (!g_IsStageSelectRequested)
	{
		if (Keyboard_IsKeyDown(KK_ESCAPE))
		{
			g_EscapeHoldTimer += dt;
			if (g_EscapeHoldTimer >= 3.0f)
			{
				g_IsStageSelectRequested = true;
				SetSceneFade(SCENE_STAGESELECT);
			}
		}
		else
		{
			g_EscapeHoldTimer = 0.0f;
		}
	}

	if (g_IsStageSelectRequested)
	{
		return;
	}

	// F3でポーズ／再生トグル（PLAYING中のみ）
	if (g_GameState == GameState::PLAYING && Keyboard_IsKeyDownTrigger(KK_F3))
	{
		g_IsPaused = !g_IsPaused;
		if (g_pNoteManager)
		{
			g_pNoteManager->SetPaused(g_IsPaused);
		}
	}

	if (Input_IsActionTrigger(INPUT_ACTION_DEBUG_F1)) {
		Options_Initialize(); // options.ymlを再ロード
		SetKeepLoadedData(true);
		SetScene(SCENE_GAME);
		SetKeepLoadedData(false);
	}

	if (g_IsPaused)
	{
		return;
	}

	if (g_GameState == GameState::FINISHED_WAIT)
	{
		// 終了演出待機中：余韻を持たせる（黒フェードが徐々に表示される）
		g_FinishTimer += dt;
		if (g_FinishTimer >= g_FinishWaitDuration)
		{
			g_GameState = GameState::FINISHED_DISPLAY;
			g_FinishTimer = 0.0f;

			// UI側にロゴの表示開始を通知
			g_pGameUI->ShowResultLogos();
		}
	}
	else if (g_GameState == GameState::FINISHED_DISPLAY)
	{
		// 終了演出表示中：3秒間ロゴ等を表示し続け、3秒経過したら自動でリザルト画面へ遷移
		g_FinishTimer += dt;
		if (g_FinishTimer >= 3.0f)
		{
			SendResult r = g_pStatusManager->GetResult();
			r.fullCombo = g_pNoteManager->GetScoreData().fullCombo;
			r.fullOrb = g_pNoteManager->GetScoreData().fullOrb;

			// 途中でゲームオーバー等によりノーツをすべて処理しきれずに終了した場合、
			// 処理しきれなかった分のノーツはすべてミスとする
			if (r.hits + r.misses < r.fullCombo)
			{
				r.misses = r.fullCombo - r.hits;
			}

			// 途中でゲームオーバー等によりノーツをすべて処理しきれずに終了した場合、
			// 処理しきれなかった分のノーツはすべてミスとする
			if (r.orbgets + r.orblosses < r.fullOrb)
			{
				r.orblosses = r.fullOrb - r.orbgets;
			}

			SetResult(r);
			SetSceneFade(SCENE_RESULT);
		}
	}

	//3D
	{
		GameCamera::Update(g_pPlayer);
		SetCameraPosition(GetCamera()->GetPos());

		g_pField->Update(g_pNoteManager->GetNoteSpeed());
		g_pPlayer->Update();
		g_pNoteManager->Update(g_pPlayer->GetLaneIndex(), g_pPlayer->GetGravityFace(), g_pPlayer->IsGravityMoving());

	}

	//2D描画
	{
		//③処理
		g_pGameUI->Update(g_pStatusManager, g_pNoteManager->GetHoldingRope() != nullptr, g_pPlayer->GetGravityFace(), g_pPlayer->GetLaneIndex());
		if (g_pStatusManager->HasNewJudge())
			g_pGameUI->NotifyJudge(g_pStatusManager->ConsumeJudge());
		//g_pChangeSceneText->Update();

		//ClickFontがクリックされた
		/*if (g_pChangeSceneText->IsClick())
		{
			SetSceneFade(SCENE_RESULT);
		}*/
	}

	// 状態更新
	if (g_GameState == GameState::PLAYING)
	{
		bool isDead = g_pStatusManager->IsDead();
		bool isFinished = g_pNoteManager->IsFinished();
		if (isDead || isFinished)
		{
			g_GameState = GameState::FINISHED_WAIT;
			g_FinishTimer = 0.0f;

			// ゲームオーバー時は素早く(0.5秒)、クリア時は余韻を持たせて(1.0秒)ロゴを表示する
			g_FinishWaitDuration = isDead ? 0.5f : 1.0f;

			// UI側に終了演出（フェードイン）開始を通知
			bool isAllHit = (!isDead && g_pStatusManager->GetResult().misses == 0);
			g_pGameUI->StartEndSequence(isDead, isAllHit);

			// 6.0秒かけて音をフェードアウト
			g_pNoteManager->StartBgmFadeOut(6.0f);
		}
	}
}

void Game_Draw(void)
{
	//④描画

	// --- 影パス（4面ShadowMap作成）---
	// トンネルの4面それぞれを、その面の内側からライトで照らして影を焼く。
	// キャスターは Player(自分の重力面へ) と Enemy/Orbノーツ(各自の面へ)。受け手は Field。
	{
		XMFLOAT3 pPos = g_pPlayer->GetPos();
		int playerFace = g_pPlayer->GetGravityFace();

		const float FACE_HALF = 2.5f;   // 各面(床/壁/天井)のトンネル半径
		const float lightDist = 12.0f;  // 面から内側へどれだけ離れた所にライトを置くか
		const float centerZ = pPos.z + 6.0f; // 影の中心Z（プレイヤーの少し奥）

		// 影の濃さ(0=真っ黒〜1=影なし。小さいほど濃い)。Player/Enemyで個別に設定できる。
		const float SHADOW_BIAS = 0.003f;
		const float PLAYER_SHADOW_BRIGHTNESS = 0.35f; // ← Playerの影の濃さ
		const float ENEMY_SHADOW_BRIGHTNESS = 0.2f; // ← Enemyの影の濃さ

		XMMATRIX faceView[NUM_SHADOW_FACES];
		XMMATRIX faceProj[NUM_SHADOW_FACES];
		XMMATRIX faceVP[NUM_SHADOW_FACES];
		XMVECTOR camUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // 視線が±X/±Yなのでトンネル軸Zをup

		for (int f = 0; f < NUM_SHADOW_FACES; f++)
		{
			XMVECTOR target, eye;
			switch (f)
			{
			case FACE_FLOOR:      target = XMVectorSet(0.0f, -FACE_HALF, centerZ, 1.0f); eye = XMVectorSet(0.0f, -FACE_HALF + lightDist, centerZ, 1.0f); break;
			case FACE_LEFT_WALL:  target = XMVectorSet(-FACE_HALF, 0.0f, centerZ, 1.0f); eye = XMVectorSet(-FACE_HALF + lightDist, 0.0f, centerZ, 1.0f); break;
			case FACE_CEILING:    target = XMVectorSet(0.0f, FACE_HALF, centerZ, 1.0f); eye = XMVectorSet(0.0f, FACE_HALF - lightDist, centerZ, 1.0f); break;
			case FACE_RIGHT_WALL: target = XMVectorSet(FACE_HALF, 0.0f, centerZ, 1.0f); eye = XMVectorSet(FACE_HALF - lightDist, 0.0f, centerZ, 1.0f); break;
			}
			faceView[f] = XMMatrixLookAtLH(eye, target, camUp);
			// 正射影：幅20(レーン方向) × 奥行50(Z)
			faceProj[f] = XMMatrixOrthographicLH(20.0f, 50.0f, 0.5f, lightDist * 2.0f);
			faceVP[f] = faceView[f] * faceProj[f];
		}

		SetFaceShadowMatrices(faceVP, SHADOW_BIAS, PLAYER_SHADOW_BRIGHTNESS, ENEMY_SHADOW_BRIGHTNESS);

		for (int f = 0; f < NUM_SHADOW_FACES; f++)
		{
			// Playerスライス(0-3)：今いる重力面にだけ影を落とす
			BeginFaceShadowMap(f);
			SetCullState(CULLSTATE_BACK);
			if (playerFace == f)
				g_pPlayer->DrawShadowMap(faceView[f], faceProj[f]);
			SetCullState(CULLSTATE_NONE);

			// ノーツスライス(4-7)：その面にいる Enemy/Orb ノーツの影
			BeginFaceShadowMap(f + NUM_SHADOW_FACES);
			SetCullState(CULLSTATE_BACK);
			g_pNoteManager->DrawShadowMapForFace(f, faceView[f], faceProj[f]);
			SetCullState(CULLSTATE_NONE);
		}
		EndFaceShadowMap();
	}

	//3D
	{
		SetDepthEnable(true);

		g_pField->Draw();
		g_pNoteManager->Draw();
		g_pPlayer->Draw();

		SetDepthEnable(false);
	}

	//2D
	{
		g_pGameUI->Draw();
		//g_pGameSprite->Draw();
		//g_pChangeSceneText->Draw();
		//g_pSelectedJsonText->Draw();
	}
}

void Game_Finalize(void)
{
	//⑤解放
	SAFE_DELETE(g_pGameSprite);
	SAFE_DELETE(g_pSelectedJsonText);
	SAFE_DELETE(g_pChangeSceneText);

	if (g_pField) { g_pField->Finalize();         SAFE_DELETE(g_pField); }
	if (g_pPlayer) { g_pPlayer->Finalize();        SAFE_DELETE(g_pPlayer); }
	if (g_pNoteManager) { g_pNoteManager->Finalize();   SAFE_DELETE(g_pNoteManager); }
	if (g_pStatusManager) { g_pStatusManager->Finalize(); SAFE_DELETE(g_pStatusManager); }
	if (g_pGameUI) { g_pGameUI->Finalize();        SAFE_DELETE(g_pGameUI); }
	GameLight::Finalize();
	GameCamera::Finalize();

	if (!GetKeepLoadedData())
	{
		// プリロードモデルの解放
		for (int i = 0; i < G_PRELOAD_MODEL_COUNT; i++)
		{
			if (g_pPreloadedModels[i])
			{
				ModelRelease(g_pPreloadedModels[i]);
				g_pPreloadedModels[i] = nullptr;
			}
		}
	}
}

void Game_DebugUIDraw(void)
{
#ifdef _DEBUG
	ImGui::Begin("LD Parameters");

	auto& p = D_PARAMS;

	ImGui::SeparatorText("Notes");
	if (ImGui::SliderFloat("Speed", &p.noteSpeed, 1.0f, 60.0f, "%.1f u/s"))
		p.noteSpeed = roundf(p.noteSpeed * 10.0f) / 10.0f;
	if (ImGui::SliderFloat("Hit Distance", &p.hitDistance, 0.5f, 10.0f, "%.2f u"))
		p.hitDistance = roundf(p.hitDistance * 100.0f) / 100.0f;
	if (ImGui::SliderFloat("Rainbow Corner Softness", &p.rainbowCornerSoftness, 0.0f, 1.0f, "%.2f"))
		p.rainbowCornerSoftness = roundf(p.rainbowCornerSoftness * 100.0f) / 100.0f;

	ImGui::SeparatorText("Player");
	if (ImGui::SliderFloat("Lane Width", &p.laneWidth, 0.5f, 5.0f, "%.2f u"))
		p.laneWidth = roundf(p.laneWidth * 100.0f) / 100.0f;
	if (ImGui::SliderFloat("Gravity Time", &p.gravityTransTime, 0.05f, 1.0f, "%.2f s"))
		p.gravityTransTime = roundf(p.gravityTransTime * 100.0f) / 100.0f;

	ImGui::SeparatorText("Camera Offsets");
	const char* faceNames[] = { "Floor (Down)", "Left Wall", "Ceiling (Up)", "Right Wall" };
	for (int i = 0; i < 4; i++)
	{
		if (ImGui::TreeNode(faceNames[i]))
		{
			if (ImGui::SliderFloat("Yaw Offset", &p.cameraOffsets[i].yaw, -90.0f, 90.0f, "%.1f deg"))
				p.cameraOffsets[i].yaw = roundf(p.cameraOffsets[i].yaw * 10.0f) / 10.0f;
			if (ImGui::SliderFloat("Pitch Offset", &p.cameraOffsets[i].pitch, -90.0f, 90.0f, "%.1f deg"))
				p.cameraOffsets[i].pitch = roundf(p.cameraOffsets[i].pitch * 10.0f) / 10.0f;
			if (ImGui::SliderFloat("Pos X Offset", &p.cameraOffsets[i].posX, -10.0f, 10.0f, "%.1f u"))
				p.cameraOffsets[i].posX = roundf(p.cameraOffsets[i].posX * 10.0f) / 10.0f;
			if (ImGui::SliderFloat("Pos Y Offset", &p.cameraOffsets[i].posY, -10.0f, 10.0f, "%.1f u"))
				p.cameraOffsets[i].posY = roundf(p.cameraOffsets[i].posY * 10.0f) / 10.0f;
			if (ImGui::SliderFloat("Pos Z Offset", &p.cameraOffsets[i].posZ, -10.0f, 10.0f, "%.1f u"))
				p.cameraOffsets[i].posZ = roundf(p.cameraOffsets[i].posZ * 10.0f) / 10.0f;
			if (ImGui::Button("Reset"))
			{
				p.cameraOffsets[i].yaw = 0.0f;
				p.cameraOffsets[i].pitch = 0.0f;
				p.cameraOffsets[i].posX = 0.0f;
				p.cameraOffsets[i].posY = 0.0f;
				p.cameraOffsets[i].posZ = 0.0f;
			}
			ImGui::TreePop();
		}
	}

	ImGui::SeparatorText("Configuration");
	if (ImGui::Button("Save Settings"))
	{
		p.Save();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Settings"))
	{
		p.Load();
	}

	ImGui::End();
#endif
}
