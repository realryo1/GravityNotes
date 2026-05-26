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

using namespace DirectX;

namespace
{
	// --- アセットパス ---
	constexpr const char*    kScorePath       = "asset\\score\\shinigstar.json";  // スコアJSONのパス
	constexpr const wchar_t* kNoteTexturePath = L"asset\\texture\\fade.png";      // ノートに使用するテクスチャのパス

	// --- レーン・座標 ---
	constexpr float kLaneCenterX[4] = { 360.0f, 520.0f, 680.0f, 840.0f }; // 各レーン（0〜3）の中心X座標
	constexpr float kJudgementY     = 560.0f; // 判定ラインのY座標
	constexpr float kLaneWidth      = 120.0f; // レーン背景の横幅

	// --- ノートサイズ ---
	constexpr float kNoteWidth  = 132.0f; // ノートの横幅
	constexpr float kNoteHeight =  60.0f; // ノートの縦幅

	// --- スクロール・タイミング ---
	constexpr float kTravelSpeed          = 220.0f; // ノートの移動速度（ピクセル/秒）
	constexpr float kVisibleAhead         =   3.0f; // 判定時刻より何秒先のノートまで表示するか
	constexpr float kVisibleBehind        =  0.75f; // 判定時刻を過ぎてから何秒後まで表示し続けるか
	constexpr float kPlaybackStartBufferSec = 1.5f; // BGM再生開始までのバッファ時間（秒）

	// ノートのイベントデータと描画スプライトをまとめた構造体
	struct NoteSprite
	{
		ScoreEvent event;                 // スコアイベントデータ（ビート・レーン・タイプ・壁）
		std::unique_ptr<Sprite2D> sprite; // 描画用スプライト
	};

	static ScoreData g_ScoreData;                                // 読み込んだスコアデータ
	static std::vector<NoteSprite> g_Notes;                      // 全ノートのスプライトリスト
	static std::vector<std::unique_ptr<Sprite2D>> g_LaneSprites; // レーン背景スプライトのリスト
	static std::unique_ptr<Sprite2D> g_JudgementLine;            // 判定ラインのスプライト
	static SoundData* g_pBgm = nullptr;                          // BGMのサウンドデータポインタ
	static FontRenderer* g_pErrorText = nullptr;                 // 読み込み失敗時のエラーメッセージ
	static float g_ElapsedTime = 0.0f;                           // シーン開始からの経過時間（秒）
	static bool g_BgmStarted = false;                            // BGM再生済みフラグ
	static float g_HighSpeed = DEBUGSCORE_INITIAL_HIGHSPEED;     // ノートの流れる速さの倍率（ハイスピード）

	// ノートのタイプに応じた表示色（RGBA）を返す
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

	// 壁の方向に応じたノートスプライトの回転角度（度）を返す
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
	static std::wstring BuildMusicPath(const std::string& music)
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

	// レーン背景スプライト（4本）と判定ラインスプライトを生成する
	static void BuildSceneSprites()
	{
		g_LaneSprites.clear();
		for (int i = 0; i < 4; ++i)
		{
			auto lane = std::make_unique<Sprite2D>(
				XMFLOAT2(kLaneCenterX[i], kJudgementY - 60.0f),
				XMFLOAT2(kLaneWidth, 460.0f),
				0.0f,
				XMFLOAT4(1.0f, 1.0f, 1.0f, 0.16f),
				BLENDSTATE_ALFA,
				kNoteTexturePath
			);
			g_LaneSprites.push_back(std::move(lane));
		}

		g_JudgementLine = std::make_unique<Sprite2D>(
			XMFLOAT2(SCREEN_WIDTH * 0.5f, kJudgementY),
			XMFLOAT2(640.0f, 8.0f),
			0.0f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f),
			BLENDSTATE_ALFA,
			kNoteTexturePath
		);
	}

	// スコアデータの各イベントに対応するノートスプライトを生成する
	static void BuildNotes()
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

	try
	{
		g_ScoreData = LoadScore(kScorePath);
	}
	catch (const std::exception&)
	{
		// 読み込み失敗時は空のスコアデータのまま、画面中央上にエラーテキストを表示する
		g_pErrorText = new FontRenderer(
			XMFLOAT2(SCREEN_WIDTH * 0.5f, 50.0f),
			30.0f,
			0.0f,
			XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f),
			"譜面の読み込みに失敗しました"
		);
	}

	BuildSceneSprites();
	BuildNotes();

	const std::wstring musicPath = BuildMusicPath(g_ScoreData.music);
	if (!musicPath.empty())
	{
		g_pBgm = LoadMP3(musicPath.c_str());
	}
}

// デバッグスコアシーンの更新処理
// 経過時間を進め、各ノートの表示位置を更新し、BGMの再生タイミングを制御する
void Debugscore_Update(void)
{
	g_ElapsedTime += 1.0f / FPS;

	if (!g_BgmStarted && g_pBgm && g_ElapsedTime >= kPlaybackStartBufferSec)
	{
		PlaySound(g_pBgm, true);
		g_BgmStarted = true;
	}

	const float chartTime = g_ElapsedTime - kPlaybackStartBufferSec;

	for (auto& note : g_Notes)
	{
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
// レーン・判定ライン・表示範囲内のノートを2Dスプライトで描画する
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
		const float noteTime = BeatToSeconds(note.event.beat);
		const float timeDiff = noteTime - chartTime;
		if (timeDiff < -kVisibleBehind || timeDiff > kVisibleAhead)
		{
			continue;
		}

		note.sprite->Draw();
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
	if (g_pErrorText)
	{
		delete g_pErrorText;
		g_pErrorText = nullptr;
	}
	g_Notes.clear();
	g_LaneSprites.clear();
	g_JudgementLine.reset();
	g_ElapsedTime = 0.0f;
	g_BgmStarted = false;
	g_HighSpeed = DEBUGSCORE_INITIAL_HIGHSPEED;
}
