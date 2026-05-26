#pragma execution_character_set("utf-8")
#include "debugscore.h"
#include "scoreloader.h"
#include "sprite2d.h"
#include "texture.h"
#include "keyboard.h"
#include "sound.h"
#include "define.h"
#include "renderer.h"
#include "font.h"
#include <vector>
#include <memory>
#include <string>
#include <exception>
#include <cmath>
#include <limits>
#include <cstdio>

using namespace DirectX;

namespace
{
	// --- アセットパス ---
	constexpr const char* kScorePath = "asset\\score\\shinigstar.json";  // スコアJSONのパス
	constexpr const wchar_t* kNoteTexturePath = L"asset\\texture\\fade.png";      // ノーツに使用するテクスチャのパス

	// --- レーン・座標 ---
	constexpr float kLaneCenterX[4] = { SCREEN_WIDTH / 5 * 1, SCREEN_WIDTH / 5 * 2, SCREEN_WIDTH / 5 * 3, SCREEN_WIDTH / 5 * 4 }; // 各レーン（0〜3）の中心X座標
	constexpr float kLaneCenterY = SCREEN_HEIGHT / 2;// 各レーン（0〜3）の中心Y座標
	constexpr float kJudgementY = SCREEN_HEIGHT / 3 * 2; // 判定ラインのY座標
	constexpr float kLaneWidth = 120.0f; // レーン背景の横幅

	// --- ノーツサイズ ---
	constexpr float kNoteWidth = 132.0f; // ノーツの横幅
	constexpr float kNoteHeight = 60.0f; // ノーツの縦幅

	// --- スクロール・タイミング ---
	constexpr float kTravelSpeed = 220.0f; // ノーツの移動速度（ピクセル/秒）
	constexpr float kVisibleAhead = 3.0f; // 判定時刻より何秒先のノーツまで表示するか
	constexpr float kVisibleBehind = 0.75f; // 判定時刻を過ぎてから何秒後まで表示し続けるか
	constexpr float kPlaybackStartBufferSec = 1.5f; // BGM再生開始までのバッファ時間（秒）
	constexpr float kJudgeWindowSec = 0.12f; // 入力判定レンジ（秒）
	constexpr float kJudgePopupLifetimeSec = 0.55f; // 判定文字の表示時間（秒）
	constexpr float kJudgePopupRiseSpeed = 36.0f; // 判定文字の上昇速度（ピクセル/秒）
	constexpr const wchar_t* kHitSePath = L"asset\\sound\\se\\bell.mp3"; // ヒット時SE
	constexpr const char* kJudgePopupPreCacheChars = "[]+-0123456789ms"; // 判定文字に使う記号・数字を事前キャッシュ

	// のイベントデータと描画スプライトをまとめた構造体
	struct NoteSprite
	{
		ScoreEvent event;                 // 譜面データ（Jsonから読んだもの：scoreloder.h）
		std::unique_ptr<Sprite2D> sprite; // 描画用スプライト
		bool isJudged = false;            // 判定済みノーツは非表示
	};

	struct JudgePopup
	{
		XMFLOAT2 pos;
		std::string text;
		float remainingSec = 0.0f;
	};

	static ScoreData g_ScoreData;                                // 読み込んだスコアデータ
	static std::vector<NoteSprite> g_Notes;                      // 全ノーツのスプライトリスト
	static std::vector<std::unique_ptr<Sprite2D>> g_LaneSprites; // レーン背景スプライトのリスト
	static std::unique_ptr<Sprite2D> g_JudgementLine;            // 判定ラインのスプライト
	static SoundData* g_pBgm = nullptr;                          // BGMのサウンドデータポインタ
	static SoundData* g_pHitSe = nullptr;                        // ヒットSEのサウンドデータポインタ
	static FontRenderer* g_pErrorText = nullptr;                 // 読み込み失敗時のエラーメッセージ
	static FontRenderer* g_pJudgePopupText = nullptr;            // 判定表示用Font（使い回し）
	static std::vector<JudgePopup> g_JudgePopups;                // ヒット時の判定表示
	static float g_ElapsedTime = 0.0f;                           // シーン開始からの経過時間（秒）
	static bool g_BgmStarted = false;                            // BGM再生済みフラグ
	static float g_HighSpeed = DEBUGSCORE_INITIAL_HIGHSPEED;     // ノーツの流れる速さの倍率（ハイスピード）

	// 内部ヘルパーのプロトタイプ宣言
	static float BeatToSeconds(float beat);
	static float LaneToX(int lane);
	static int GetLaneFromKey(Keyboard_Keys key);
	static void AddJudgePopup(float x, float y, int diffMs);
	static int FindNearestNoteIndexInLane(int lane, float chartTime, float& outSignedDiffSec);
	static void TryJudgeLaneHit(int lane, float chartTime);
	static void UpdateJudgePopups(float deltaTime);
	static void QuickRestart();
	static XMFLOAT4 GetTypeColor(ScoreType type);
	static float GetWallRotation(ScoreWall wall);
	static std::wstring CreateMusicPath(const std::string& music);
	static void CreateLane();
	static void CreateNotes();

	// キー入力を固定レーンへ割り当てる（F/G/H/J -> 0/1/2/3）
	static int GetLaneFromKey(Keyboard_Keys key)
	{
		switch (key)
		{
		case KK_F: return 0;
		case KK_G: return 1;
		case KK_H: return 2;
		case KK_J: return 3;
		default:   return -1;
		}
	}

	// 例: [+10ms] / [-24ms] の形式で判定差分を表示する
	static void AddJudgePopup(float x, float y, int diffMs)
	{
		char text[32] = {};
		std::snprintf(text, sizeof(text), "[%+dms]", diffMs);

		JudgePopup popup{};
		popup.pos = XMFLOAT2(x, y - 44.0f);
		popup.text = text;
		popup.remainingSec = kJudgePopupLifetimeSec;
		g_JudgePopups.push_back(std::move(popup));
	}

	// 指定レーン内で、現在時刻に最も近い未判定ノーツを1件だけ選ぶ
	static int FindNearestNoteIndexInLane(int lane, float chartTime, float& outSignedDiffSec)
	{
		int bestIndex = -1;
		float bestAbsDiff = (std::numeric_limits<float>::max)();
		float bestSignedDiff = 0.0f;

		for (size_t i = 0; i < g_Notes.size(); ++i)
		{
			const NoteSprite& note = g_Notes[i];
			if (note.isJudged || note.event.lane != lane)
			{
				continue;
			}

			const float noteTime = BeatToSeconds(note.event.beat);
			const float signedDiff = chartTime - noteTime;
			const float absDiff = std::fabs(signedDiff);
			if (absDiff < bestAbsDiff)
			{
				bestAbsDiff = absDiff;
				bestSignedDiff = signedDiff;
				bestIndex = static_cast<int>(i);
			}
		}

		if (bestIndex >= 0)
		{
			outSignedDiffSec = bestSignedDiff;
		}

		return bestIndex;
	}

	// 判定ウィンドウ内ならヒット成立: ノーツ消去、SE再生、差分表示
	static void TryJudgeLaneHit(int lane, float chartTime)
	{
		float signedDiffSec = 0.0f;
		const int noteIndex = FindNearestNoteIndexInLane(lane, chartTime, signedDiffSec);
		if (noteIndex < 0 || std::fabs(signedDiffSec) > kJudgeWindowSec)
		{
			return;
		}

		NoteSprite& note = g_Notes[noteIndex];
		note.isJudged = true;

		if (g_pHitSe)
		{
			PlaySound(g_pHitSe, false);
		}

		const float noteTime = BeatToSeconds(note.event.beat);
		const float timeDiff = noteTime - chartTime;
		const float x = LaneToX(note.event.lane);
		const float y = kJudgementY - (timeDiff * kTravelSpeed * g_HighSpeed);
		const int diffMs = static_cast<int>(std::round(signedDiffSec * 1000.0f));
		AddJudgePopup(x, y, diffMs);
	}

	// 表示寿命を減らしつつ上方向へ流し、時間切れで削除
	static void UpdateJudgePopups(float deltaTime)
	{
		for (size_t i = 0; i < g_JudgePopups.size();)
		{
			JudgePopup& popup = g_JudgePopups[i];
			popup.remainingSec -= deltaTime;
			popup.pos.y -= kJudgePopupRiseSpeed * deltaTime;

			if (popup.remainingSec <= 0.0f)
			{
				g_JudgePopups.erase(g_JudgePopups.begin() + static_cast<std::vector<JudgePopup>::difference_type>(i));
				continue;
			}

			++i;
		}
	}

	// F1用クイックリスタート: 進行・判定・表示を即時リセット
	static void QuickRestart()
	{
		g_ElapsedTime = 0.0f;
		g_BgmStarted = false;
		g_JudgePopups.clear();

		if (g_pBgm)
		{
			StopSound(g_pBgm);
		}

		for (auto& note : g_Notes)
		{
			note.isJudged = false;
		}
	}

	// ノーツのタイプに応じた表示色（RGBA）を返す
	static XMFLOAT4 GetTypeColor(ScoreType type)
	{
		switch (type)
		{
		case ScoreType::Enemy:    return XMFLOAT4(1.0f, 0.35f, 0.35f, 0.95f);
		case ScoreType::Obstacle: return XMFLOAT4(1.0f, 0.85f, 0.25f, 0.95f);
		case ScoreType::Gravity:  return XMFLOAT4(0.35f, 0.65f, 1.0f, 0.95f);
		case ScoreType::Jump:     return XMFLOAT4(0.35f, 1.0f, 0.55f, 0.95f);
		default:                  return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.95f);
		}
	}

	// 壁の方向に応じたノーツスプライトの回転角度（度）を返す
	//適当実装
	static float GetWallRotation(ScoreWall wall)
	{
		switch (wall)
		{
		case ScoreWall::Up:    return 0.0f;
		case ScoreWall::Right: return 90.0f;
		case ScoreWall::Down:  return 180.0f;
		case ScoreWall::Left:  return 270.0f;
		default:               return 0.0f;
		}
	}

	// ビート値をBPMに基づいて秒数に変換する（BPMが0以下の場合はビート値をそのまま返す）
	static float BeatToSeconds(float beat)
	{
		if (g_ScoreData.bpm <= 0.0f)
		{
			return beat;
		}

		return beat * 60.0f / g_ScoreData.bpm;
	}

	// レーン番号（0〜3）をスクリーン上のX座標に変換する（範囲外はクランプ）
	static float LaneToX(int lane)
	{
		if (lane < 0) lane = 0;
		if (lane > 3) lane = 3;
		return kLaneCenterX[lane];
	}

	// JSONに記述されたmusic文字列を実際に読み込み可能なパスに変換する
	// 既に "asset\" で始まる場合はそのまま、そうでなければ "asset\sound\bgm\" を前置する
	static std::wstring CreateMusicPath(const std::string& music)
	{
		if (music.empty())
		{
			return L"";
		}

		std::wstring wideMusic(music.begin(), music.end());
		if (wideMusic.find(L"asset\\") == 0 || wideMusic.find(L"asset/") == 0)
		{
			return wideMusic;
		}

		return L"asset\\sound\\bgm\\" + wideMusic;
	}

	// レーン背景（4本）と判定ラインの実体を生成する
	static void CreateLane()
	{
		g_LaneSprites.clear();
		for (int i = 0; i < 4; ++i)
		{
			auto lane = std::make_unique<Sprite2D>(
				XMFLOAT2(kLaneCenterX[i], kLaneCenterY),
				XMFLOAT2(kLaneWidth, 600.0f),
				0.0f,
				XMFLOAT4(1.0f, 1.0f, 1.0f, 0.16f),
				BLENDSTATE_ALFA,
				kNoteTexturePath
			);
			g_LaneSprites.push_back(std::move(lane));
		}

		g_JudgementLine = std::make_unique<Sprite2D>(
			XMFLOAT2(SCREEN_WIDTH * 0.5f, kJudgementY),
			XMFLOAT2(1000.0f, 8.0f),
			0.0f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f),
			BLENDSTATE_ALFA,
			kNoteTexturePath
		);
	}

	// スコアデータの各イベントに対応するノーツスプライトを生成する
	static void CreateNotes()
	{
		g_Notes.clear();
		g_Notes.reserve(g_ScoreData.events.size());

		for (const auto& event : g_ScoreData.events)
		{
			auto sprite = std::make_unique<Sprite2D>(
				XMFLOAT2(LaneToX(event.lane), kJudgementY),
				XMFLOAT2(kNoteWidth, kNoteHeight),
				GetWallRotation(event.wall),
				GetTypeColor(event.type),
				BLENDSTATE_ALFA,
				kNoteTexturePath
			);
			g_Notes.push_back(NoteSprite{ event, std::move(sprite) });
		}
	}
}

// デバッグスコアシーンの初期化処理
// スコアデータの読み込み・スプライト生成・BGMロードを行う
void Debugscore_Initialize(void)
{
	g_ElapsedTime = 0.0f;
	g_BgmStarted = false;
	g_JudgePopups.clear();

	try
	{
		g_ScoreData = LoadScore(kScorePath);
	}
	// 読み込み失敗時は空のスコアデータのまま、画面中央上にエラーテキストを表示する
	catch (const std::exception&)
	{
		g_pErrorText = new FontRenderer(
			XMFLOAT2(SCREEN_WIDTH * 0.5f, 50.0f),
			30.0f,
			0.0f,
			XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f),
			"譜面の読み込みに失敗しました"
		);
	}

	CreateLane();
	CreateNotes();

	// 判定表示用フォントは1つだけ作って使い回し、初回ヒット時のスパイクを抑える
	if (!g_pJudgePopupText)
	{
		g_pJudgePopupText = new FontRenderer(
			XMFLOAT2(0.0f, 0.0f),
			24.0f,
			0.0f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			""
		);
		g_pJudgePopupText->SetText(kJudgePopupPreCacheChars);
		g_pJudgePopupText->PreCacheGlyphs();
		g_pJudgePopupText->SetText("");
	}

	const std::wstring musicPath = CreateMusicPath(g_ScoreData.music);
	if (!musicPath.empty())
	{
		g_pBgm = LoadMP3(musicPath.c_str());
	}

	g_pHitSe = LoadMP3(kHitSePath);
}

// デバッグスコアシーンの更新処理
// 経過時間を進め、各ノーツの表示位置を更新し、BGMの再生タイミングを制御する
void Debugscore_Update(void)
{
	const float deltaTime = 1.0f / FPS;

	if (Keyboard_IsKeyDownTrigger(KK_F1))
	{
		QuickRestart();
	}

	g_ElapsedTime += deltaTime;

	if (!g_BgmStarted && g_pBgm && g_ElapsedTime >= kPlaybackStartBufferSec)
	{
		PlaySound(g_pBgm, true);
		g_BgmStarted = true;
	}

	const float chartTime = g_ElapsedTime - kPlaybackStartBufferSec;

	if (Keyboard_IsKeyDownTrigger(KK_F))
	{
		TryJudgeLaneHit(GetLaneFromKey(KK_F), chartTime);
	}
	if (Keyboard_IsKeyDownTrigger(KK_G))
	{
		TryJudgeLaneHit(GetLaneFromKey(KK_G), chartTime);
	}
	if (Keyboard_IsKeyDownTrigger(KK_H))
	{
		TryJudgeLaneHit(GetLaneFromKey(KK_H), chartTime);
	}
	if (Keyboard_IsKeyDownTrigger(KK_J))
	{
		TryJudgeLaneHit(GetLaneFromKey(KK_J), chartTime);
	}

	UpdateJudgePopups(deltaTime);

	for (auto& note : g_Notes)
	{
		if (note.isJudged)
		{
			continue;
		}

		const float noteTime = BeatToSeconds(note.event.beat);
		const float timeDiff = noteTime - chartTime;

		if (timeDiff < -kVisibleBehind || timeDiff > kVisibleAhead)
		{
			continue;
		}

		const float x = LaneToX(note.event.lane);
		const float y = kJudgementY - (timeDiff * kTravelSpeed * g_HighSpeed);
		note.sprite->SetPos(XMFLOAT2(x, y));
	}
}

// デバッグスコアシーンの描画処理
// レーン・判定ライン・表示範囲内のノーツを2Dスプライトで描画する
void Debugscore_Draw(void)
{
	SetDepthEnable(false);
	Sprite_BeginDraw2D();

	for (auto& lane : g_LaneSprites)
	{
		lane->Draw();
	}

	if (g_JudgementLine)
	{
		g_JudgementLine->Draw();
	}

	const float chartTime = g_ElapsedTime - kPlaybackStartBufferSec;

	for (auto& note : g_Notes)
	{
		if (note.isJudged)
		{
			continue;
		}

		const float noteTime = BeatToSeconds(note.event.beat);
		const float timeDiff = noteTime - chartTime;
		if (timeDiff < -kVisibleBehind || timeDiff > kVisibleAhead)
		{
			continue;
		}

		note.sprite->Draw();
	}

	if (g_pJudgePopupText)
	{
		for (auto& popup : g_JudgePopups)
		{
			g_pJudgePopupText->SetPos(popup.pos);
			g_pJudgePopupText->SetText(popup.text);
			g_pJudgePopupText->Draw();
		}
	}

	if (g_pErrorText)
	{
		g_pErrorText->Draw();
	}

	Sprite_EndDraw2D();
	SetDepthEnable(true);
}

// デバッグスコアシーンの終了処理
// BGMのアンロードと全スプライトの解放を行い、状態変数を初期化する
void Debugscore_Finalize(void)
{
	if (g_pBgm)
	{
		UnloadSound(g_pBgm);
		g_pBgm = nullptr;
	}
	if (g_pHitSe)
	{
		UnloadSound(g_pHitSe);
		g_pHitSe = nullptr;
	}
	if (g_pErrorText)
	{
		delete g_pErrorText;
		g_pErrorText = nullptr;
	}
	if (g_pJudgePopupText)
	{
		delete g_pJudgePopupText;
		g_pJudgePopupText = nullptr;
	}
	g_Notes.clear();
	g_JudgePopups.clear();
	g_LaneSprites.clear();
	g_JudgementLine.reset();
	g_ElapsedTime = 0.0f;
	g_BgmStarted = false;
	g_HighSpeed = DEBUGSCORE_INITIAL_HIGHSPEED;
}
