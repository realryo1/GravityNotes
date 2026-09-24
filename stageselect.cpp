#include "stageselect.h"
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
#include "MultiLineFontRenderer.h"
#include "scoresummaryloader.h"
#include "scene.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>

using namespace DirectX;

// ==========================================
// 状態定義およびグローバル変数
// ==========================================

// レコードプレーヤーの一連のアクションを処理するステートマシン
enum VinylState {
	STATE_PLAYING,         // 音楽が安定して再生され、ディスクが均等に回転している状態
	STATE_LIFTING_ARM,     // プレイヤーが曲を変更した：トーンアームが持ち上がり、ディスクが減速している状態
	STATE_CHANGING_DISC,   // アームが元の位置に戻り、ディスクが停止した：1フレームでディスクを入れ替える状態
	STATE_DROPPING_ARM     // ディスク交換完了：トーンアームが新しいディスクへゆっくり降下している状態
};

static VinylState g_CurrentState = STATE_PLAYING; // 初期状態

// 左側のレコードアルバムに対応するステージ数/曲数を管理
static int g_MaxStages = 0;
static int g_VisualStages = 0;                  // 表示用の仮想ディスク総数（最小公倍数で拡張）
static int g_SelectedStage = 0;                  // 現在再生中のステージ
static int g_NextStage = 0;                      // 次のステージ（遷移完了待ち）

// グラフィックオブジェクト（スプライト＆フォントポインタ）
static Sprite2D* g_pResultBG = nullptr;           // 最背面の背景画像
static Sprite2D* g_pBackground = nullptr;         // レコードプレーヤーの背景画像
static Sprite2D* g_pMainVinyl = nullptr;          // 中央のメイン回転ディスク
static Sprite2D* g_pRecordFrame = nullptr;        // メインディスクのフレーム画像
static Sprite2D* g_pToneArm = nullptr;            // トーンアーム（レコードの針）
static std::vector<Sprite2D*> g_pStageDisks;  // 左側の小さなディスクの列
static Sprite2D* g_pStartGameBG = nullptr;         // ゲーム開始ボタン背景
static ClickFont* g_pStartGameText = nullptr;      // ゲーム開始テキスト

// アニメーション制御変数
static float g_VinylRotation = 0.0f;              // ディスクの現在の回転角度（度）
static float g_ToneArmAngle = 0.0f;               // トーンアームの角度（25度はディスク上、0度は外側）
static float g_DiscSpeed = 0.5f;                  // 現在の回転速度（スムーズな減速用）

static float g_ScrollOffset = 0.0f;      // 現在のオフセット（滑らかに補間するためのfloat）
static float g_ScrollTarget = 0.0f;      // 目標のオフセット

static int g_MenuRepeatTimer = 0;        // リピート処理用のタイマー
static int g_PendingDelta = 0;           // アニメーション中に受け付けた入力バッファ（+1=下, -1=上）
static SoundData* g_pMenuMoveSe = nullptr; // メニュー移動SE

// JSONからの譜面データの管理とスコア表示（日本語コード部分より）
static MultiLineFontRenderer* g_pScoreInfoText = nullptr;
static std::vector<ScoreSummary> g_ScoreSummaries;
static int g_SelectedScoreIndex = 0;

static SoundData* g_pCurrentBgmData = nullptr;    // 現在再生中の楽曲 of サウンドデータポインタ
static std::string g_LoadedBgmPath = "";          // 重複ロードを避けるための現在ロード中のサウンドファイルパス

// ==========================================
// ヘルパー関数
// ==========================================
static int GetGCD(int a, int b)
{
	while (b != 0) {
		int r = a % b;
		a = b;
		b = r;
	}
	return a;
}

static int GetLCM(int a, int b)
{
	if (a == 0 || b == 0) return 0;
	return (a * b) / GetGCD(a, b);
}

// UTF-8からワイド文字列への変換
static std::wstring Utf8ToWide(const std::string& utf8Str)
{
	if (utf8Str.empty()) return L"";
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
	if (size_needed <= 0) return L"";
	std::wstring wstr(size_needed - 1, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], size_needed);
	return wstr;
}

// ファイル存在確認ヘルパー
static bool FileExists(const std::string& path)
{
	std::ifstream f(path.c_str());
	return f.good();
}

// サムネイル画像のパス解決（存在しない場合はデフォルト）
static std::wstring GetThumbnailPath(const std::string& thumbnail)
{
	std::string path = "asset\\score\\" + thumbnail;
	if (thumbnail.empty() || !FileExists(path)) {
		path = "asset\\texture\\notfound_thumbnail.png";
	}
	return Utf8ToWide(path);
}

// 現在選択されているJSONファイル名を取得
static std::string GetSelectedJsonName()
{
	if (g_ScoreSummaries.empty()) return "";

	if (g_SelectedScoreIndex < 0) g_SelectedScoreIndex = 0;
	if (g_SelectedScoreIndex >= static_cast<int>(g_ScoreSummaries.size())) {
		g_SelectedScoreIndex = static_cast<int>(g_ScoreSummaries.size()) - 1;
	}
	return g_ScoreSummaries[static_cast<size_t>(g_SelectedScoreIndex)].jsonname;
}

static void UpdateBgmFromSelection()
{
	if (g_ScoreSummaries.empty()) return;

	// 選択された曲のインデックスが範囲内であることを保証
	if (g_SelectedScoreIndex < 0) g_SelectedScoreIndex = 0;
	if (g_SelectedScoreIndex >= static_cast<int>(g_ScoreSummaries.size())) {
		g_SelectedScoreIndex = static_cast<int>(g_ScoreSummaries.size()) - 1;
	}

	const ScoreSummary& summary = g_ScoreSummaries[g_SelectedScoreIndex];
	std::string soundPath = ResolveMusicPath(summary.music);

	// 選択された曲が現在再生中の曲と同じ場合はそのまま
	if (g_LoadedBgmPath == soundPath) return;

	// RAMのオーバーフローを防ぐため、古い曲を停止する（解放はシーン遷移時のキャッシュ一括クリアに任せる）
	if (g_pCurrentBgmData != nullptr) {
		StopSound(g_pCurrentBgmData);
		// UnloadSound(g_pCurrentBgmData);
		g_pCurrentBgmData = nullptr;
	}

	// 物理ディレクトリから .wav ファイルを RAM にロードして再生
	g_pCurrentBgmData = LoadMP3(soundPath);
	if (g_pCurrentBgmData != nullptr) {
		PlaySound(g_pCurrentBgmData, true); // ループ再生
		g_LoadedBgmPath = soundPath;
	}
	else {
		g_LoadedBgmPath = "";
	}
}

// 楽曲の詳細情報を表示するテキストを更新
static void RefreshSelectedScoreText()
{
	if (g_pScoreInfoText == nullptr) return;

	if (g_ScoreSummaries.empty()) {
		g_pScoreInfoText->SetText("No score json found");
		return;
	}

	if (g_SelectedScoreIndex < 0) g_SelectedScoreIndex = 0;
	if (g_SelectedScoreIndex >= static_cast<int>(g_ScoreSummaries.size())) {
		g_SelectedScoreIndex = static_cast<int>(g_ScoreSummaries.size()) - 1;
	}

	const ScoreSummary& summary = g_ScoreSummaries[static_cast<size_t>(g_SelectedScoreIndex)];

	char buf[1024] = {};
	std::snprintf(
		buf,
		sizeof(buf),
		"[%d/%d]\n%s\n%s\n譜面作者: %s\n難易度: %.1f\nBPM: %.1f\n%s",
		g_SelectedScoreIndex + 1,
		static_cast<int>(g_ScoreSummaries.size()),
		summary.musicname.c_str(),
		summary.musicauthor.c_str(),
		summary.scoreauthor.c_str(),
		summary.difficulty,
		summary.bpm,
		summary.jsonname.c_str()
	);
	g_pScoreInfoText->SetText(buf);
}

// 左右矢印キーが押されたときに選択されている曲のインデックスを変更
static void ChangeSelectedScore(int delta)
{
	if (g_ScoreSummaries.empty()) return;

	const int count = static_cast<int>(g_ScoreSummaries.size());
	g_SelectedScoreIndex = (g_SelectedScoreIndex + delta) % count;
	if (g_SelectedScoreIndex < 0) {
		g_SelectedScoreIndex += count;
	}

	RefreshSelectedScoreText();
}

// ==========================================
// 初期化関数 (INITIALIZE)
// ==========================================
void StageSelect_Initialize(void)
{
	// BGMデータ管理用変数の初期化
	g_pCurrentBgmData = nullptr;
	g_LoadedBgmPath = "";

	// JSONシステムから楽曲リストをロード
	g_ScoreSummaries = LoadScoreSummaries();
	g_MaxStages = static_cast<int>(g_ScoreSummaries.size());
	g_VisualStages = GetLCM(8, g_MaxStages);

	const bool loaded = !g_ScoreSummaries.empty();
	hal::dout << "[StageSelect] Score summary reload: "
		<< (loaded ? "SUCCESS" : "FAILED")
		<< " Count=" << g_ScoreSummaries.size()
		<< std::endl;

	// BGMのプリロードを行う（選曲切り替え時のフリーズを防止するため、キャッシュシステムに事前に登録しておく）
	for (const auto& summary : g_ScoreSummaries) {
		if (!summary.music.empty()) {
			std::string soundPath = ResolveMusicPath(summary.music);
			LoadMP3(soundPath);
		}
	}

	g_SelectedStage = 0;
	g_SelectedScoreIndex = 0;
	g_ScrollOffset = 0.0f;
	g_ScrollTarget = 0.0f;
	g_MenuRepeatTimer = 0;
	g_PendingDelta = 0;
	g_pMenuMoveSe = LoadMP3("asset/sound/se/musicMove.wav");

	g_pResultBG = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
		{ SCREEN_WIDTH, SCREEN_HEIGHT },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\Result_BG_kari.png"
	);

	// 1. 背景画像の初期化（テクスチャサイズに合わせて調整）
	g_pBackground = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
		{ SCREEN_WIDTH, SCREEN_HEIGHT },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\stageselect.png"
	);

	// 2. 左隅に縦に並ぶ小さなディスクの列を初期化
	g_pStageDisks.resize(g_VisualStages, nullptr);
	for (int i = 0; i < g_VisualStages; i++) {
		float posX = (SCREEN_WIDTH / 2.0f) - 550.0f;
		float posY = 70.0f + (i * 130.0f); // 各ディスクを縦方向に等間隔で配置

		std::wstring thumbPath = GetThumbnailPath(g_ScoreSummaries[i % g_MaxStages].thumbnail);

		g_pStageDisks[i] = new Sprite2D(
			{ posX, posY },
			{ 110.0f, 110.0f },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			BLENDSTATE_ALFA,
			thumbPath.c_str()
		);
	}

	// 3. 背景のターンテーブルにぴったり収まるメインディスクを初期化
	std::wstring mainThumbPath = L"";
	if (g_MaxStages > 0) {
		mainThumbPath = GetThumbnailPath(g_ScoreSummaries[g_SelectedStage].thumbnail);
	} else {
		mainThumbPath = L"asset\\texture\\notfound_thumbnail.png";
	}
	g_pMainVinyl = new Sprite2D(
		{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f },
		{ 322.0f, 322.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		mainThumbPath.c_str()
	);

	g_pRecordFrame = new Sprite2D(
		{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f },
		{ 700.0f, 700.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\recordframe.png"
	);

	// 4. トーンアーム（レコードの針）を初期化（メインディスクの右上へ重ねて配置）
	g_pToneArm = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f + 210.0f, SCREEN_HEIGHT / 2.0f - 290.0f },
		{ 400.0f, 400.0f },
		75.0f, // デフォルトの初期角度（レコード盤の上に載っている状態）
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\tonearm2.png"
	);

	// 5. 曲情報/スコア表示クラスを初期化（画面右側に配置）
	g_pScoreInfoText = new MultiLineFontRenderer(
		{ SCREEN_WIDTH - 177.0f, SCREEN_HEIGHT / 3.0f - 40.0f }, // 画面外にはみ出さないように位置を調整
		22.0f,
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		"Loading...",
		1.7f
	);

	// ディスク0の最初のBGMを自動的に検索して再生
	RefreshSelectedScoreText();
	UpdateBgmFromSelection();

	// メカニカルな動作の初期状態パラメータを再設定
	g_CurrentState = STATE_PLAYING;
	g_ToneArmAngle = 25.0f;
	g_DiscSpeed = 0.5f;

	g_pStartGameBG = new Sprite2D(
		{ 1068.0f, 612.0f },
		{ 85 * 4, 39 * 4 },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\Result_Button_UI.png"
	);

	g_pStartGameText = new ClickFont(
		{ 1051.0f, 611.0f },				//位置
		40.0f,														//文字サイズ
		0.0f,														//回転（度）
		{ 1.0f, 1.0f, 1.0f, 1.0f },									//通常色
		{ 1.0f, 0.8f, 0.2f, 1.0f },									//ホバー色
		"ゲーム開始"													//テキスト
	);

	UnLockMouse(); // ユーザー操作のためにマウスのロックを解除
}

// ==========================================
// ロジック更新関数 (UPDATE)
// ==========================================
void StageSelect_Update(void)
{
	// --- パート 1: キーボード/マウス of 入力制御 ---
	bool isUpPressed = Input_IsActionDown(INPUT_ACTION_MENU_UP);
	bool isDownPressed = Input_IsActionDown(INPUT_ACTION_MENU_DOWN);

	if (isUpPressed || isDownPressed) {
		g_MenuRepeatTimer++;
	}
	else {
		g_MenuRepeatTimer = 0;
	}

	// パーセンテージによる線形補間（lerp）の代わりに一定速度で移動する。
	// 固定の移動ステップを使用することで、一定のフレーム数で常に正確に目標値（target）に到達し、
	// 均等な動きになり、カクつきの後にジャンプする現象が発生しなくなる。
	// 通常時は25フレームかけてスクロールするが、長押し時は8フレームで完了するように速度を上げる。
	const float SCROLL_STEP = (isUpPressed || isDownPressed) ? (1.0f / 12.0f) : (1.0f / 25.0f);
	if (g_ScrollOffset < g_ScrollTarget) {
		g_ScrollOffset += SCROLL_STEP;
		if (g_ScrollOffset > g_ScrollTarget) g_ScrollOffset = g_ScrollTarget;
	}
	else if (g_ScrollOffset > g_ScrollTarget) {
		g_ScrollOffset -= SCROLL_STEP;
		if (g_ScrollOffset < g_ScrollTarget) g_ScrollOffset = g_ScrollTarget;
	}

	float anchorY = 70.0f;
	float spacing = 130.0f;

	// ディスクが安定して回転している状態（STATE_PLAYING）でのみ曲変更コマンドを受け付ける
	if (g_CurrentState == STATE_PLAYING)
	{
		bool isInputPressed = false;
		int delta = 0;

		// バッファ済みの入力（短押し連打）を優先して消化
		if (g_PendingDelta != 0) {
			delta = g_PendingDelta;
			g_PendingDelta = 0;
			isInputPressed = true;
		}
		// 上下矢印、wasd、DPad、Lスティック上下でレコードのステージを変更
		else if (Input_IsActionTrigger(INPUT_ACTION_MENU_UP)) {
			delta = -1;
			isInputPressed = true;
		}
		else if (Input_IsActionTrigger(INPUT_ACTION_MENU_DOWN)) {
			delta = +1;
			isInputPressed = true;
		}

		if (isInputPressed) {
			if (delta < 0) {
				g_NextStage = g_SelectedStage - 1;
				if (g_NextStage < 0) g_NextStage = g_MaxStages - 1;
				g_ScrollTarget -= 1.0f;  // 1段階上へスライド
			}
			else {
				g_NextStage = g_SelectedStage + 1;
				if (g_NextStage >= g_MaxStages) g_NextStage = 0;
				g_ScrollTarget += 1.0f;  // 1段階下へスライド
			}
			if (g_pMenuMoveSe != nullptr) PlaySound(g_pMenuMoveSe, false);

			// ステージが変更された場合、アームを持ち上げる一連のアクションを開始
			g_CurrentState = STATE_LIFTING_ARM;
			if (g_pCurrentBgmData != nullptr) {
				StopSound(g_pCurrentBgmData);
				g_pCurrentBgmData = nullptr;
			}
			g_LoadedBgmPath = "";
		}
	}
	else
	{
		// アニメーション中（LIFTING / DROPPING）に短押しされた入力をバッファリング
		// 最後に押された方向のみ保持する（複数連打は加算ではなく上書き）
		if (Input_IsActionTrigger(INPUT_ACTION_MENU_UP)) {
			g_PendingDelta = -1;
		}
		else if (Input_IsActionTrigger(INPUT_ACTION_MENU_DOWN)) {
			g_PendingDelta = +1;
		}
	}

	// --- パート 2: 状態遷移（ステートマシン）の処理 ---
	switch (g_CurrentState)
	{
	case STATE_PLAYING:
		g_DiscSpeed = 0.5f;     // ディスクが安定して一定速度で回転
		g_ToneArmAngle = 0.0f;  // ディスク上のトーンアームの位置を維持（アーム角度0度）
		break;

	case STATE_LIFTING_ARM:
		// トーンアームがディスク上から外側へスムーズに移動（角度を25度まで上げる）
		if (g_ToneArmAngle < 25.0f) {
			g_ToneArmAngle += 1.0f; // トーンアームの移動速度
		}

		// 慣性によりディスクが急停止せず、滑らかに減速する
		if (g_DiscSpeed > 0.0f) {
			g_DiscSpeed -= 0.02f;
			if (g_DiscSpeed < 0.0f) g_DiscSpeed = 0.0f;
		}

		// 遷移条件：トーンアームが外側に完全に移動し（>=25度）、ディスクが完全に停止していること
		if (g_ToneArmAngle >= 25.0f && g_DiscSpeed <= 0.0f) {
			g_CurrentState = STATE_CHANGING_DISC;
		}
		break;

	case STATE_CHANGING_DISC:
		// 正式なステージのアルバムインデックスを更新
		g_SelectedStage = g_NextStage;

		// メモリリークを防ぐため、古いディスクのテクスチャオブジェクトを削除
		if (g_pMainVinyl != nullptr) {
			SAFE_DELETE(g_pMainVinyl);
		}

		// 選択されたステージの新しいディスクをメインのターンテーブルにロード
		{
			std::wstring mainThumbPath = L"";
			if (g_MaxStages > 0) {
				mainThumbPath = GetThumbnailPath(g_ScoreSummaries[g_SelectedStage].thumbnail);
			}
			else {
				mainThumbPath = L"asset\\texture\\notfound_thumbnail.png";
			}
			g_pMainVinyl = new Sprite2D(
				{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f },
				{ 322.0f, 322.0f },
				g_VinylRotation, // テクスチャの回転がカクつかないように現在の回転角度を維持
				{ 1.0f, 1.0f, 1.0f, 1.0f },
				BLENDSTATE_ALFA,
				mainThumbPath.c_str()
			);
		}

		// ボタンがまだ押しっぱなしである場合は、アームを降ろさずに長押しによる連続スクロールの入力を待つ
		if (isUpPressed || isDownPressed) {
			bool triggerUp = false;
			bool triggerDown = false;

			if (isUpPressed && g_MenuRepeatTimer >= 30 && (g_MenuRepeatTimer - 30) % 12 == 0) {
				triggerUp = true;
			}
			if (isDownPressed && g_MenuRepeatTimer >= 30 && (g_MenuRepeatTimer - 30) % 12 == 0) {
				triggerDown = true;
			}

			if (triggerUp) {
				g_NextStage = g_SelectedStage - 1;
				if (g_NextStage < 0) g_NextStage = g_MaxStages - 1;
				g_ScrollTarget -= 1.0f;
				if (g_pMenuMoveSe != nullptr) PlaySound(g_pMenuMoveSe, false);
			}
			else if (triggerDown) {
				g_NextStage = g_SelectedStage + 1;
				if (g_NextStage >= g_MaxStages) g_NextStage = 0;
				g_ScrollTarget += 1.0f;
				if (g_pMenuMoveSe != nullptr) PlaySound(g_pMenuMoveSe, false);
			}
		}
		else {
			// ディスク交換直後、トーンアームを新しいディスクに降下させる状態に移行
			g_CurrentState = STATE_DROPPING_ARM;
		}
		break;

	case STATE_DROPPING_ARM:
		// トーンアームが外側から新しいディスクへ徐々に降下（角度を0度へ戻す）
		if (g_ToneArmAngle > 0.0f) {
			g_ToneArmAngle -= 1.0f;
		}

		// トーンアームがディスクに接触したら（<= 0度）、安定再生状態（PLAYING）に戻る
		if (g_ToneArmAngle <= 0.0f) {
			g_CurrentState = STATE_PLAYING;
			// ✅ Update/Refresh を呼び出す前に、g_SelectedStage に合わせて g_SelectedScoreIndex を同期する
			// 切り替えたステージと一致する vinylIndex を持つ最初のスコアエントリを検索
			for (int i = 0; i < static_cast<int>(g_ScoreSummaries.size()); i++) {
				if (g_ScoreSummaries[i].vinylIndex == g_SelectedStage) {
					g_SelectedScoreIndex = i;
					break;
				}
			}

			RefreshSelectedScoreText(); // UIテキストを対応する曲に更新
			UpdateBgmFromSelection();   // 正しいステージの音楽を再生
		}
		break;
	}

	// --- パート 3: DirectXグラフィック変数へのパラメータ適用 ---
	// 現在フレームの g_DiscSpeed に基づいてディスクの連続回転角度を計算
	if (g_DiscSpeed > 0.0f) {
		g_VinylRotation += g_DiscSpeed;
		if (g_VinylRotation >= 360.0f) g_VinylRotation -= 360.0f;
	}

	// メインディスクの回転を更新
	if (g_pMainVinyl != nullptr) {
		g_pMainVinyl->SetRot(g_VinylRotation);
	}
	if (g_pRecordFrame != nullptr) {
		g_pRecordFrame->SetRot(g_VinylRotation);
	}

	// トーンアームの回転/アーム角度を更新
	if (g_pToneArm != nullptr) {
		g_pToneArm->SetRot(g_ToneArmAngle);
	}

	// --- パート 4: 左側の小さなディスク列の処理 ＆ マウスクリック ---
	for (int i = 0; i < g_VisualStages; i++)
	{
		// 位置を計算するために、g_SelectedStageの代わりにg_ScrollOffsetを使用する
		float offset = (float)i - g_ScrollOffset;

		// float形式での円状のラッピング処理
		float minOffset = -1.5f;
		float maxOffset = (float)g_VisualStages - 1.5f;
		while (offset < minOffset)          offset += (float)g_VisualStages;
		while (offset >= maxOffset)         offset -= (float)g_VisualStages;

		float posX = (SCREEN_WIDTH / 2.0f) - 550.0f;
		float posY = anchorY + (offset * spacing);

		g_pStageDisks[i]->SetPos({ posX, posY });

		// 選択中のディスク ＝ オフセットが0に最も近いディスク
		if ((i % g_MaxStages) == g_SelectedStage) {
			g_pStageDisks[i]->SetRotation(g_VinylRotation * 2.0f);
		}
		else {
			g_pStageDisks[i]->SetRotation(0.0f);
		}
	}

	// --- パート 5: ゲーム開始の決定 (ENTER / SPACE) ---
	if (g_pStartGameText != nullptr) {
		g_pStartGameText->Update();
	}

	// ディスクが安定して再生している場合のみゲーム開始への遷移を許可し、SetPlayJson -> SetSceneFade の順序を厳密に遵守する
	if (g_CurrentState == STATE_PLAYING) {
		if ((g_pStartGameText != nullptr && g_pStartGameText->IsClick()) || Input_IsActionTrigger(INPUT_ACTION_DECIDE)) {
			// 本番のステージに遷移する前に、待機中のBGMを解放する
			if (g_pCurrentBgmData != nullptr) {
				StopSound(g_pCurrentBgmData);
				// UnloadSound(g_pCurrentBgmData);
				g_pCurrentBgmData = nullptr;
			}
			g_LoadedBgmPath = "";

			SetPlayJson(GetSelectedJsonName());
			SetSceneFade(SCENE_GAME);
		}
	}
}

// ==========================================
// 描画関数 (DRAW)
// ==========================================
void StageSelect_Draw(void)
{
	if (g_pResultBG != nullptr) g_pResultBG->Draw();
	g_pBackground->Draw(); // 1. 最背面にレコードプレーヤーの背景を描画
	g_pMainVinyl->Draw(); // 3. 画面中央のメインディスクを描画
	if (g_pRecordFrame != nullptr) g_pRecordFrame->Draw();
	g_pToneArm->Draw();   // 4. メインディスクの上に重なるようにトーンアームを描画
	// 2. 左側のすべての小さなディスクを描画（画面外にあるものは描画をスキップ）
	for (int i = 0; i < g_VisualStages; i++) {
		if (g_pStageDisks[i] != nullptr) {
			float posY = g_pStageDisks[i]->GetPosY();
			if (posY >= -60.0f && posY <= SCREEN_HEIGHT + 60.0f) {
				g_pStageDisks[i]->Draw();
			}
		}
	}

	g_pScoreInfoText->Draw(); // 5. 最前面の右上に曲情報とスコアのJSONテキストを描画

	if (g_pStartGameBG != nullptr) g_pStartGameBG->Draw();
	if (g_pStartGameText != nullptr) g_pStartGameText->Draw();
}

// ==========================================
// メモリ解放関数 (FINALIZE)
// ==========================================
void StageSelect_Finalize(void)
{
	// メモリリークを防ぐため、曲を停止してサウンドデータを解放する
	if (g_pCurrentBgmData != nullptr) {
		StopSound(g_pCurrentBgmData);
		UnloadSound(g_pCurrentBgmData);
		g_pCurrentBgmData = nullptr;
	}
	g_LoadedBgmPath = "";
	UnloadSound(g_pMenuMoveSe);
	g_pMenuMoveSe = nullptr;

	SAFE_DELETE(g_pResultBG);
	SAFE_DELETE(g_pBackground);
	SAFE_DELETE(g_pMainVinyl);
	SAFE_DELETE(g_pRecordFrame);
	SAFE_DELETE(g_pToneArm);
	SAFE_DELETE(g_pScoreInfoText);

	for (int i = 0; i < g_VisualStages; i++) {
		SAFE_DELETE(g_pStageDisks[i]);
	}
	g_pStageDisks.clear();

	g_ScoreSummaries.clear();

	SAFE_DELETE(g_pStartGameBG);
	SAFE_DELETE(g_pStartGameText);
}