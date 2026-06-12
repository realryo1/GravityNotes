#include "stageselect.h"
#include "sprite2d.h"
#include "texture.h"
#include "keyboard.h"
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

using namespace DirectX;

// 1. Ảnh nền phòng/bàn máy hát
static Sprite2D* g_pBackground = nullptr;

// 2. Đĩa than chính ở giữa và Kim đọc đĩa (Tonearm)
static Sprite2D* g_pMainVinyl = nullptr;
static Sprite2D* g_pToneArm = nullptr;

// 3. Hàng đĩa bên trái (Dùng mảng để quản lý)
const int MAX_STAGES = 5;
static Sprite2D* g_pStageDisks[MAX_STAGES] = { nullptr };
static ClickFont* g_pStageButtons[MAX_STAGES] = { nullptr };

// 4. Biến logic quản lý
static int g_SelectedStage = 0;      // Màn chơi đang được chọn
static float g_VinylRotation = 0.0f; // Góc xoay của đĩa chính

// ①インスタンス、ポインタ用意
static MultiLineFontRenderer* g_pScoreInfoText = nullptr;
static std::vector<ScoreSummary> g_ScoreSummaries;
static int g_SelectedScoreIndex = 0;

static std::string GetSelectedJsonName()
{
	if (g_ScoreSummaries.empty())
	{
		return "";
	}

	if (g_SelectedScoreIndex < 0)
	{
		g_SelectedScoreIndex = 0;
	}
	if (g_SelectedScoreIndex >= static_cast<int>(g_ScoreSummaries.size()))
	{
		g_SelectedScoreIndex = static_cast<int>(g_ScoreSummaries.size()) - 1;
	}

	return g_ScoreSummaries[static_cast<size_t>(g_SelectedScoreIndex)].jsonname;
}

// 現在選択中の楽曲情報を1曲分だけ整形して表示する
static void RefreshSelectedScoreText()
{
	if (g_pScoreInfoText == nullptr)
	{
		return;
	}

	if (g_ScoreSummaries.empty())
	{
		g_pScoreInfoText->SetText("No score json found");
		return;
	}

	if (g_SelectedScoreIndex < 0)
	{
		g_SelectedScoreIndex = 0;
	}
	if (g_SelectedScoreIndex >= static_cast<int>(g_ScoreSummaries.size()))
	{
		g_SelectedScoreIndex = static_cast<int>(g_ScoreSummaries.size()) - 1;
	}

	const ScoreSummary& summary = g_ScoreSummaries[static_cast<size_t>(g_SelectedScoreIndex)];

	char buf[1024] = {};
	std::snprintf(
		buf,
		sizeof(buf),
		"[%d/%d]\nMusic: %s\n作曲者: %s\n譜面作者: %s\n難易度: %.1f\nBPM: %.1f\nJSON: %s",
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

// 左右入力で選択中インデックスを循環更新し、表示内容を更新する
static void ChangeSelectedScore(int delta)
{
	if (g_ScoreSummaries.empty())
	{
		return;
	}

	const int count = static_cast<int>(g_ScoreSummaries.size());
	g_SelectedScoreIndex = (g_SelectedScoreIndex + delta) % count;
	if (g_SelectedScoreIndex < 0)
	{
		g_SelectedScoreIndex += count;
	}

	RefreshSelectedScoreText();
}

// StageSelectシーンの生成と、譜面概要一覧の再読み込みを行う
void StageSelect_Initialize(void)
{
	// Khởi tạo nền (Độ phân giải 2560 x 1440)
	g_pBackground = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
		{ 740.0f, 556.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_NONE,
		L"asset\\texture\\vinylbg.png"
	);

	// Khởi tạo hàng đĩa bên trái và nút bấm tương ứng
	for (int i = 0; i < MAX_STAGES; i++) {
		float posX = 130.0f;
		float posY = 50.0f + (i * 150.0f); // Tọa độ Y dịch chuyển dần xuống dưới

		g_pStageDisks[i] = new Sprite2D(
			{ posX, posY },
			{ 150.0f, 150.0f },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			BLENDSTATE_NONE,
			L"asset\\texture\\vinmain.png"
		);

		// Đặt chữ/nút đè đúng lên vị trí của chiếc đĩa đó
		g_pStageButtons[i] = new ClickFont(
			{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 4.0f * 3.5 },
			50.0f, // Giảm size chữ một chút cho vừa khít cái đĩa
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			{ 1.0f, 0.8f, 0.2f, 1.0f },
			"PLAY" // Tạm thời đặt tên chung, bạn có thể custom tùy ý
		);
	}

	// Khởi tạo đĩa chính ở GIỮA màn hình
	g_pMainVinyl = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f-85.0f, SCREEN_HEIGHT / 2.0f +2.0f},
		{ 500.0f, 500.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA, // ĐỔI TỪ BLENDSTATE_NONE THÀNH ALPHA
		L"asset\\texture\\vinmain.png"
	);

	// Tương tự cho g_pToneArm và g_pStageDisks[i]
	g_pToneArm = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f + 350.0f, SCREEN_HEIGHT / 2.0f - 300.0f },
		{ 400.0f, 400.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA, // ĐỔI THÀNH ALPHA
		L"asset\\texture\\tonearm.png"
	);

	g_pScoreInfoText = new MultiLineFontRenderer(
		{ SCREEN_WIDTH / 4.0f, SCREEN_HEIGHT / 2 },
		28.0f,
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		"loading...",
		1.35f
	);

	g_ScoreSummaries = LoadScoreSummaries();
	const bool loaded = !g_ScoreSummaries.empty();
	hal::dout << "[StageSelect] score summary reload: "
		<< (loaded ? "success" : "failed")
		<< " count=" << g_ScoreSummaries.size()
		<< std::endl;

	g_SelectedScoreIndex = 0;
	RefreshSelectedScoreText();

	UnLockMouse();//マウスアンロック
}

// 入力処理（左右で選曲）と決定クリック時のシーン遷移を行う
void StageSelect_Update(void)
{
	// 1. Cập nhật trạng thái cho toàn bộ các nút bên trái
	for (int i = 0; i < MAX_STAGES; i++)
	{
		g_pStageButtons[i]->Update();

		// Nếu người chơi click chọn đĩa thứ i
		if (g_pStageButtons[i]->IsClick())
		{
			g_SelectedStage = i; // Ghi nhận màn chơi được chọn

			// Bạn có thể phát âm thanh chuyển đĩa tại đây bằng thư viện sound.h
			// PlaySound(...);

			// Nếu muốn bấm phát ăn ngay vào game:
			SetSceneFade(SCENE_GAME);
		}
	}

	// 2. Làm cho đĩa chính ở giữa xoay tròn mượt mà
	g_VinylRotation += 0.5f; // Tốc độ xoay (tăng/giảm tùy ý)
	if (g_VinylRotation >= 360.0f) {
		g_VinylRotation -= 360.0f;
	}

	// Cập nhật góc quay vào Sprite đĩa chính
	if (g_pMainVinyl != nullptr) {
		g_pMainVinyl->SetRotation(g_VinylRotation);
	//③処理
	/*g_pChangeSceneText->Update();

	if (Keyboard_IsKeyDownTrigger(KK_LEFT))
	{
		ChangeSelectedScore(-1);
	}

	if (Keyboard_IsKeyDownTrigger(KK_RIGHT))
	{
		ChangeSelectedScore(1);
	}*/

	//ClickFontがクリックされた
	//if (g_pChangeSceneText->IsClick())
	//{
	//	SetPlayJson(GetSelectedJsonName());
	//	SetSceneFade(SCENE_GAME);
	}
}

// StageSelectシーンの描画を行う
void StageSelect_Draw(void)
{
	// ④ Vẽ theo thứ tự từ dưới lên trên
	g_pBackground->Draw(); // Vẽ nền trước

	// Vẽ toàn bộ danh sách đĩa và chữ bên trái
	for (int i = 0; i < MAX_STAGES; i++) {
		g_pStageDisks[i]->Draw();
		g_pStageButtons[i]->Draw();
	}

	g_pMainVinyl->Draw(); // Vẽ đĩa xoay chính ở giữa
	g_pToneArm->Draw();   // Vẽ kim đĩa đè lên trên cùng đĩa chính
	//④描画
	g_pScoreInfoText->Draw();
}

// StageSelectシーンで確保したリソースを解放する
void StageSelect_Finalize(void)
{
	// ⑤ Giải phóng toàn bộ bộ nhớ sạch sẽ để tránh rò rỉ (Memory Leak)
	SAFE_DELETE(g_pBackground);
	SAFE_DELETE(g_pMainVinyl);
	SAFE_DELETE(g_pToneArm);

	for (int i = 0; i < MAX_STAGES; i++) {
		SAFE_DELETE(g_pStageDisks[i]);
		SAFE_DELETE(g_pStageButtons[i]);
	}

	SAFE_DELETE(g_pScoreInfoText);
	g_ScoreSummaries.clear();
}
