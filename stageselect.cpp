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
#include <vector>
#include <string>

using namespace DirectX;

// ==========================================
// CÁC ĐỊNH NGHĨA TRẠNG THÁI VÀ BIẾN TOÀN CỤC
// ==========================================

// Máy trạng thái xử lý chuỗi hành động của máy phát đĩa
enum VinylState {
	STATE_PLAYING,         // Đang phát nhạc ổn định, đĩa xoay đều
	STATE_LIFTING_ARM,     // Người chơi bấm đổi bài: Kim đang nhấc lên, đĩa hãm phanh
	STATE_CHANGING_DISC,   // Kim đã ra ngoài, đĩa dừng: Tiến hành tráo đổi đĩa trong 1 frame
	STATE_DROPPING_ARM     // Đã đổi đĩa xong: Kim đang từ từ hạ xuống đĩa mới
};

static VinylState g_CurrentState = STATE_PLAYING; // Trạng thái ban đầu

// Quản lý số lượng màn chơi / bài hát tương ứng với 5 Album đĩa bên trái
const int MAX_STAGES = 5;
static int g_SelectedStage = 0;                  // Màn chơi hiện tại đang phát
static int g_NextStage = 0;                      // Màn chơi tạm thời (chờ chuyển cảnh xong)

// Mảng chứa đường dẫn ảnh đĩa than riêng cho từng Stage
static const wchar_t* g_StageTextures[MAX_STAGES] = {
	L"asset\\texture\\vinmain.png",  // Stage 1
	L"asset\\texture\\vinmain1.png", // Stage 2
	L"asset\\texture\\vinmain2.png", // Stage 3
	L"asset\\texture\\vinmain3.png", // Stage 4
	L"asset\\texture\\vinmain4.png"  // Stage 5
};

// Các thực thể đồ họa (Con trỏ Sprite & Font)
static Sprite2D* g_pBackground = nullptr;         // Ảnh nền máy hát đĩa
static Sprite2D* g_pMainVinyl = nullptr;          // Đĩa xoay chính ở giữa
static Sprite2D* g_pToneArm = nullptr;            // Kim đọc đĩa (Tonearm)
static Sprite2D* g_pStageDisks[MAX_STAGES] = { nullptr };     // Hàng đĩa nhỏ bên trái
static ClickFont* g_pStageButtons[MAX_STAGES] = { nullptr };  // Chữ/Nút bấm "PLAY" hoặc "Stage" cho từng đĩa bên trái

// Biến điều khiển chuyển động (Animations)
static float g_VinylRotation = 0.0f;              // Góc xoay hiện tại của đĩa tính theo độ
static float g_ToneArmAngle = 0.0f;               // Góc của kim (25 độ là trên đĩa, 0 độ là ở ngoài rìa)
static float g_DiscSpeed = 0.5f;                  // Tốc độ xoay hiện tại (Dùng để giảm tốc mượt mà)

// Quản lý dữ liệu nhạc từ JSON và hiển thị điểm (Từ phần code Tiếng Nhật)
static MultiLineFontRenderer* g_pScoreInfoText = nullptr;
static std::vector<ScoreSummary> g_ScoreSummaries;
static int g_SelectedScoreIndex = 0;

// ==========================================
// CÁC HÀM TRỢ GIÚP LOGIC CHỌN NHẠC (HỆ JSON)
static SoundData* g_pCurrentBgmData = nullptr;    // Con trỏ lưu trữ dữ liệu âm thanh bài hát đang phát hiện tại
static std::string g_LoadedBgmPath = "";          // Lưu đường dẫn file âm thanh hiện tại để tránh load trùng
// ==========================================

// Lấy tên file JSON đang được chọn hiện tại
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
	// Nếu danh sách JSON trống hoặc chỉ số đĩa vượt quá số lượng bài hát thì không xử lý
	if (g_ScoreSummaries.empty() || g_SelectedStage >= static_cast<int>(g_ScoreSummaries.size())) return;

	// BƯỚC 2 & 3: Lấy thuộc tính "music" từ file JSON và lắp ráp đường dẫn
	const ScoreSummary& summary = g_ScoreSummaries[g_SelectedStage];
	std::string soundPath = "asset\\sound\\bgm\\" + summary.music;

	// Nếu bài hát đang chọn trùng với bài đang phát thì giữ nguyên
	if (g_LoadedBgmPath == soundPath) return;

	// Dừng và giải phóng bài hát cũ để tránh tràn bộ nhớ RAM
	if (g_pCurrentBgmData != nullptr) {
		StopSound(g_pCurrentBgmData);
		UnloadSound(g_pCurrentBgmData);
		g_pCurrentBgmData = nullptr;
	}

	// BƯỚC 4: Nạp file .mp3 từ thư mục vật lý lên RAM và phát nhạc
	g_pCurrentBgmData = LoadMP3(soundPath);

	if (g_pCurrentBgmData != nullptr) {
		PlaySound(g_pCurrentBgmData, true); // Phát lặp lại liên tục
		g_LoadedBgmPath = soundPath;
	}
	else {
		g_LoadedBgmPath = "";
	}
}

// Làm mới văn bản hiển thị thông tin chi tiết của bài hát
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
		"[%d/%d]\nMusic: %s\nComposer: %s\nCharter: %s\nDifficulty: %.1f\nBPM: %.1f\nJSON: %s",
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

// Thay đổi index bài hát khi bấm nút mũi tên Trái / Phải
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
// HÀM KHỞI TẠO (INITIALIZE)
// ==========================================
void StageSelect_Initialize(void)
{
	// 1. Khởi tạo hình nền (Kích thước tùy chỉnh theo texture của bạn)
	g_pBackground = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
		{ 954.0f, 717.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\1.png"
	);

	// 2. Khởi tạo hàng đĩa nhỏ xếp dọc ở góc bên trái
	for (int i = 0; i < MAX_STAGES; i++) {
		float posX = 90.0f;
		float posY = 70.0f + (i * 130.0f); // Mỗi đĩa cách nhau theo chiều dọc mượt mà

		g_pStageDisks[i] = new Sprite2D(
			{ posX, posY },
			{ 110.0f, 110.0f },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			BLENDSTATE_ALFA,
			g_StageTextures[i] // Lấy đúng texture riêng biệt của từng Stage trong danh sách
		);

		// Khởi tạo text hiển thị đè lên đĩa nhỏ để nhận diện chuột click
		g_pStageButtons[i] = new ClickFont(
			{ posX, posY },
			20.0f,
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			{ 1.0f, 0.8f, 0.2f, 1.0f },
			"PLAY"
		);
	}

	// 3. Khởi tạo đĩa chính nằm vừa vặn vào mâm xoay của hình nền
	g_pMainVinyl = new Sprite2D(
		{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f },
		{ 800.0f, 800.0f },
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		g_StageTextures[g_SelectedStage]
	);

	// 4. Khởi tạo kim đọc đĩa (Nằm đè lên phía trên bên phải của đĩa chính)
	g_pToneArm = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f + 210.0f, SCREEN_HEIGHT / 2.0f - 290.0f },
		{ 400.0f, 400.0f },
		75.0f, // Mặc định ban đầu góc 25 độ (đang đặt trên mặt đĩa)
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\tonearm2.png"
	);

	// 5. Khởi tạo bộ hiển thị thông tin bài hát / điểm số (Nằm ở góc phải màn hình)
	g_pScoreInfoText = new MultiLineFontRenderer(
		{ SCREEN_WIDTH - 150.0f, SCREEN_HEIGHT - 400.0f }, // Căn chỉnh lại vị trí để không bị lút khỏi màn hình
		28.0f,
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		"Loading...",
		1.35f
	);

	// Khởi tạo các biến quản lý dữ liệu nhạc nền
	g_pCurrentBgmData = nullptr;
	g_LoadedBgmPath = "";

	// Tải danh sách bài hát từ hệ thống JSON
	g_ScoreSummaries = LoadScoreSummaries();
	const bool loaded = !g_ScoreSummaries.empty();
	hal::dout << "[StageSelect] Score summary reload: "
		<< (loaded ? "SUCCESS" : "FAILED")
		<< " Count=" << g_ScoreSummaries.size()
		<< std::endl;

	

	// Tải toàn bộ danh sách các file JSON lên hệ thống
	g_ScoreSummaries = LoadScoreSummaries();
	g_SelectedStage = 0;

	// Tự động tìm và kích hoạt bản BGM đầu tiên của đĩa 0
	RefreshSelectedScoreText();

	UpdateBgmFromSelection();

	// Thiết lập lại các thông số trạng thái cơ học ban đầu ổn định
	g_CurrentState = STATE_PLAYING;
	g_ToneArmAngle = 25.0f;
	g_DiscSpeed = 0.5f;

	UnLockMouse(); // Mở khóa chuột cho người dùng tương tác
}

// ==========================================
// HÀM CẬP NHẬT LOGIC (UPDATE)
// ==========================================
void StageSelect_Update(void)
{
	// --- PHẦN 1: ĐÓN NHẬN TƯƠNG TÁC BÀN PHÍM / CHUỘT ---
	// Chỉ cho phép nhận lệnh đổi bài khi đĩa đang chạy ổn định (STATE_PLAYING)
	if (g_CurrentState == STATE_PLAYING)
	{
		bool isInputPressed = false;

		// Bấm mũi tên LÊN / XUỐNG để đổi Stage đĩa than
		if (Keyboard_IsKeyDownTrigger(KK_UP)) {
			g_NextStage = g_SelectedStage - 1;
			if (g_NextStage < 0) g_NextStage = MAX_STAGES - 1;
			isInputPressed = true;
			ChangeSelectedScore(-1);
		}
		else if (Keyboard_IsKeyDownTrigger(KK_DOWN)) {
			g_NextStage = g_SelectedStage + 1;
			if (g_NextStage >= MAX_STAGES) g_NextStage = 0;
			isInputPressed = true;
			ChangeSelectedScore(1);
		}

		//// Bấm mũi tên TRÁI / PHẢI để đổi bài hát trong danh sách dữ liệu JSON công phá điểm số
		//if (Keyboard_IsKeyDownTrigger(KK_LEFT)) {
		//	ChangeSelectedScore(-1);
		//}
		//if (Keyboard_IsKeyDownTrigger(KK_RIGHT)) {
		//	ChangeSelectedScore(1);
		//}

		// Kích hoạt chuỗi hành động nhấc kim nếu có tương tác đổi Stage đĩa
		if (isInputPressed) {
			g_CurrentState = STATE_LIFTING_ARM;
			if (g_pCurrentBgmData != nullptr) {
				StopSound(g_pCurrentBgmData);
			}
		}
	}

	// --- PHẦN 2: XỬ LÝ MÁY TRẠNG THÁI CHUYỂN ĐỘNG (STATE MACHINE) ---
	switch (g_CurrentState)
	{
	case STATE_PLAYING:
		g_DiscSpeed = 0.5f;     // Tốc độ đĩa quay đều ổn định
		g_ToneArmAngle = 0.0f;  // Giữ nguyên vị trí kim trên đĩa (Góc 0 bám đĩa)
		break;

	case STATE_LIFTING_ARM:
		// Kim dịch chuyển mượt mà từ trong đĩa ra ngoài rìa (tăng góc lên 25 độ để nhấc lên)
		if (g_ToneArmAngle < 25.0f) {
			g_ToneArmAngle += 1.0f; // Tốc độ nhấc kim
		}

		// Đĩa không dừng đột ngột mà hãm phanh chậm dần đều do ma sát quán tính
		if (g_DiscSpeed > 0.0f) {
			g_DiscSpeed -= 0.02f;
			if (g_DiscSpeed < 0.0f) g_DiscSpeed = 0.0f;
		}

		// Điều kiện chuyển trạng thái: Kim đã nhấc ra hẳn biên (>=25độ) AND Đĩa đã đứng im hoàn toàn
		if (g_ToneArmAngle >= 25.0f && g_DiscSpeed <= 0.0f) {
			g_CurrentState = STATE_CHANGING_DISC;
		}
		break;

	case STATE_CHANGING_DISC:
		// Cập nhật chỉ số Album Stage chính thức
		g_SelectedStage = g_NextStage;

		// Xóa thực thể ảnh đĩa cũ để tránh rò rỉ bộ nhớ (Memory Leak)
		if (g_pMainVinyl != nullptr) {
			SAFE_DELETE(g_pMainVinyl);
		}

		// Nạp đĩa mới của Stage vừa chọn vào mâm xoay chính
		g_pMainVinyl = new Sprite2D(
			{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f },
			{ 800.0f, 800.0f },
			g_VinylRotation, // Giữ nguyên góc quay dở dang để đĩa không bị giật khựng texture
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			BLENDSTATE_ALFA,
			g_StageTextures[g_SelectedStage]
		);

		// Ngay sau khi đổi đĩa xong, kích hoạt trạng thái hạ kim xuống đĩa mới
		g_CurrentState = STATE_DROPPING_ARM;
		break;

	case STATE_DROPPING_ARM:
		// Kim dịch chuyển từ từ từ ngoài biên vào trong mặt đĩa mới (giảm góc về 0 độ)
		if (g_ToneArmAngle > 0.0f) {
			g_ToneArmAngle -= 1.0f;
		}

		// Khi kim vừa chạm đúng vị trí mặt đĩa (<= 0 độ), trả về trạng thái PLAYING tuần hoàn ổn định
		if (g_ToneArmAngle <= 0.0f) {
			g_CurrentState = STATE_PLAYING;
		}
		break;
	}

	// --- PHẦN 3: ÁP DỤNG THÔNG SỐ VÀO BIẾN ĐỒ HỌA DIRECTX ---
	// Tính toán góc xoay liên tục cho đĩa dựa trên g_DiscSpeed hiện tại của khung hình
	if (g_DiscSpeed > 0.0f) {
		g_VinylRotation += g_DiscSpeed;
		if (g_VinylRotation >= 360.0f) g_VinylRotation -= 360.0f;
	}

	// Cập nhật xoay cho đĩa chính
	if (g_pMainVinyl != nullptr) {
		g_pMainVinyl->SetRot(g_VinylRotation);
	}

	// Cập nhật xoay/dịch chuyển góc nâng cho kim đĩa
	if (g_pToneArm != nullptr) {
		g_pToneArm->SetRot(g_ToneArmAngle);
	}

	// --- PHẦN 4: XỬ LÝ HÀNG ĐĨA NHỎ BÊN TRÁI & CLICK CHUỘT ---
	for (int i = 0; i < MAX_STAGES; i++)
	{
		g_pStageButtons[i]->Update();

		if (i == g_SelectedStage) {
			g_pStageDisks[i]->SetRotation(g_VinylRotation * 2.0f);
		}
		else {
			g_pStageDisks[i]->SetRotation(0.0f);
		}

		if (g_pStageButtons[i]->IsClick() && g_CurrentState == STATE_PLAYING && g_SelectedStage != i)
		{
			g_NextStage = i;
			g_CurrentState = STATE_LIFTING_ARM;
			if (g_pCurrentBgmData != nullptr) {
				StopSound(g_pCurrentBgmData);
			}
		}
	}

	// --- PHẦN 5: XÁC NHẬN VÀO GAME (ENTER / SPACE) ---
	// Chỉ cho phép chuyển cảnh vào game khi đĩa đang phát nhạc ổn định, tuân thủ nghiêm ngặt thứ tự SetPlayJson -> SetSceneFade
	if (g_CurrentState == STATE_PLAYING) {
		if (Keyboard_IsKeyDownTrigger(KK_BACK) || Keyboard_IsKeyDownTrigger(KK_ENTER)) {
			// Giải phóng nhạc nền phòng chờ trước khi chuyển cảnh sang màn chơi chính thức
			if (g_pCurrentBgmData != nullptr) {
				StopSound(g_pCurrentBgmData);
				UnloadSound(g_pCurrentBgmData);
				g_pCurrentBgmData = nullptr;
			}
			g_LoadedBgmPath = "";

			SetSceneFade(SCENE_GAME);
		}
	}
}

// ==========================================
// HÀM VẼ ĐỒ HỌA (DRAW)
// ==========================================
void StageSelect_Draw(void)
{
	// Quy tắc vẽ layer: Lớp nào nằm dưới vẽ trước, lớp nào nằm trên vẽ đè lên sau
	g_pBackground->Draw(); // 1. Vẽ nền máy hát dưới cùng
	g_pMainVinyl->Draw(); // 3. Vẽ đĩa xoay chính ở giữa màn hình
	g_pToneArm->Draw();   // 4. Vẽ kim đọc đĩa đè lên trên mặt đĩa chính
	// 2. Vẽ toàn bộ danh sách đĩa nhỏ và nút chữ tương ứng ở bên trái
	for (int i = 0; i < MAX_STAGES; i++) {
		g_pStageDisks[i]->Draw();
		g_pStageButtons[i]->Draw();
	}

	
	

	g_pScoreInfoText->Draw(); // 5. Vẽ bảng thông tin bài hát & điểm số JSON lên trên cùng góc phải
}

// ==========================================
// HÀM GIẢI PHÓNG BỘ NHỚ (FINALIZE)
// ==========================================
void StageSelect_Finalize(void)
{
	// Dừng nhạc và giải phóng dữ liệu âm thanh để tránh bị rò rỉ bộ nhớ (Memory Leak)
	if (g_pCurrentBgmData != nullptr) {
		StopSound(g_pCurrentBgmData);
		UnloadSound(g_pCurrentBgmData);
		g_pCurrentBgmData = nullptr;
	}
	g_LoadedBgmPath = "";

	SAFE_DELETE(g_pBackground); 
	SAFE_DELETE(g_pMainVinyl);
	SAFE_DELETE(g_pToneArm);
	SAFE_DELETE(g_pScoreInfoText);

	for (int i = 0; i < MAX_STAGES; i++) {
		SAFE_DELETE(g_pStageDisks[i]);
		SAFE_DELETE(g_pStageButtons[i]);
	}

	g_ScoreSummaries.clear();
}