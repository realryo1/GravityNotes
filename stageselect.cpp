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

// Quản lý số lượng màn chơi / bài hát
const int MAX_STAGES = 5;
static int g_SelectedStage = 0;                  // Màn chơi hiện tại đang phát
static int g_NextStage = 0;                      // Màn chơi tạm thời (chờ chuyển cảnh xong)

// Mảng chứa đường dẫn ảnh đĩa than riêng cho từng Stage (Bạn có thể custom ảnh nếu muốn)
static const wchar_t* g_StageTextures[MAX_STAGES] = {
	L"asset\\texture\\vinmain.png", // Stage 1
	L"asset\\texture\\vinmain1.png", // Stage 2
	L"asset\\texture\\vinmain2.png", // Stage 3
	L"asset\\texture\\vinmain3.png", // Stage 4
	L"asset\\texture\\vinmain4.png"  // Stage 5
};

// Các thực thể đồ họa (Con trỏ Sprite)
static Sprite2D* g_pBackground = nullptr;         // Ảnh nền máy hát đĩa
static Sprite2D* g_pMainVinyl = nullptr;          // Đĩa xoay chính ở giữa
static Sprite2D* g_pToneArm = nullptr;            // Kim đọc đĩa (Tonearm)
static Sprite2D* g_pStageDisks[MAX_STAGES] = { nullptr };     // Hàng đĩa nhỏ bên trái
static ClickFont* g_pStageButtons[MAX_STAGES] = { nullptr };  // Chữ/Nút bấm cho từng đĩa bên trái

// Biến điều khiển chuyển động (Animations)
static float g_VinylRotation = 0.0f;              // Góc xoay hiện tại của đĩa tính theo độ
static float g_ToneArmAngle = 0.0f;              // Góc của kim (25 độ là trên đĩa, 0 độ là ở ngoài rìa)
static float g_DiscSpeed = 0.5f;                  // Tốc độ xoay hiện tại (Dùng để giảm tốc mượt mà)

// ==========================================
// HÀM KHỞI TẠO (INITIALIZE)
// ==========================================
void StageSelect_Initialize(void)
{
	// 1. Khởi tạo hình nền (Tỷ lệ Full HD hoặc 2K)
	g_pBackground = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
		{ 954.0f, 717.0f }, // Tự động co giãn theo SCREEN nếu bạn để độ phân giải khác
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_NONE,
		L"asset\\texture\\1.png"
	);

	// 2. Khởi tạo hàng đĩa nhỏ xếp dọc ở góc bên trái
	for (int i = 0; i < MAX_STAGES; i++) {
		float posX = 90.0f;
		float posY = 70.0f + (i * 130.0f); // Mỗi đĩa cách nhau 150 pixel theo chiều dọc

		g_pStageDisks[i] = new Sprite2D(
			{ posX, posY },
			{ 110.0f, 110.0f }, // Thu nhỏ đĩa list lại một chút nhìn sẽ tinh tế hơn
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			BLENDSTATE_ALFA,
			g_StageTextures[i] // SỬA TẠI ĐÂY: Lấy đúng texture riêng biệt của từng Stage trong danh sách
		);

		// Khởi tạo text hiển thị đè lên đĩa nhỏ để nhận diện chuột click
		g_pStageButtons[i] = new ClickFont(
			{ posX, posY },
			20.0f,
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			{ 1.0f, 0.8f, 0.2f, 1.0f },
			"Stage"
		);
	}

	// 3. Khởi tạo đĩa chính nằm vừa vặn vào mâm xoay của hình nền
	g_pMainVinyl = new Sprite2D(
		{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f }, // Đã căn chỉnh tâm mâm xoay
		{ 800.0f, 800.0f },                                               // Kích thước vừa khít viền mâm
		0.0f,
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,                                                 // Chế độ hòa trộn chống viền đen vuông
		g_StageTextures[g_SelectedStage]
	);

	// 4. Khởi tạo kim đọc đĩa (Nằm đè lên phía trên bên phải của đĩa chính)
	g_pToneArm = new Sprite2D(
		{ SCREEN_WIDTH / 2.0f + 210.0f, SCREEN_HEIGHT / 2.0f - 290.0f },
		{ 534.0f, 490.0f },
		75.0f, // Mặc định ban đầu góc 25 độ (đang đặt trên mặt đĩa)
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		BLENDSTATE_ALFA,
		L"asset\\texture\\tonearm1.png"
	);

	// Thiết lập lại các thông số trạng thái ban đầu ổn định
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

		// Bấm mũi tên LÊN
		if (Keyboard_IsKeyDownTrigger(KK_UP)) {
			g_NextStage = g_SelectedStage - 1;
			if (g_NextStage < 0) g_NextStage = MAX_STAGES - 1;
			isInputPressed = true;
		}
		// Bấm mũi tên XUỐNG
		else if (Keyboard_IsKeyDownTrigger(KK_DOWN)) {
			g_NextStage = g_SelectedStage + 1;
			if (g_NextStage >= MAX_STAGES) g_NextStage = 0;
			isInputPressed = true;
		}

		// Kích hoạt chuỗi hành động nhấc kim nếu có tương tác đổi bài
		if (isInputPressed) {
			g_CurrentState = STATE_LIFTING_ARM;
			// PlaySound(SOUND_NEEDLE_LIFT); // Có thể bật tiếng nhấc kim cơ học tại đây
		}
	}

	// --- PHẦN 2: XỬ LÝ MÁY TRẠNG THÁI CHUYỂN ĐỘNG (STATE MACHINE) ---
	switch (g_CurrentState)
	{
	case STATE_PLAYING:
		g_DiscSpeed = 0.5f;     // Tốc độ đĩa quay đều ổn định
		g_ToneArmAngle = 0.0f; // Giữ nguyên vị trí kim trên đĩa
		break;

	case STATE_LIFTING_ARM:
		// Kim dịch chuyển mượt mà từ trong đĩa ra ngoài rìa (giảm góc về 0 độ)
		if (g_ToneArmAngle < 25.0f) {
			g_ToneArmAngle += 1.0f; // Tốc độ di chuyển của kim
		}

		// Đĩa không dừng đột ngột mà hãm phanh chậm dần đều do quán tính ma sát
		if (g_DiscSpeed > 0.0f) {
			g_DiscSpeed -= 0.02f; // Trừ dần tốc độ sau mỗi frame
			if (g_DiscSpeed < 0.0f) g_DiscSpeed = 0.0f;
		}

		// Điều kiện chuyển trạng thái: Kim đã ra hẳn ngoài biên AND Đĩa đã đứng im hoàn toàn
		if (g_ToneArmAngle >= 25.0f && g_DiscSpeed <= 0.0f) {
			g_CurrentState = STATE_CHANGING_DISC;
		}
		break;

	case STATE_CHANGING_DISC:
		// Cập nhật chỉ số bài hát chính thức
		g_SelectedStage = g_NextStage;

		// Xóa thực thể ảnh đĩa cũ để giải phóng bộ nhớ
		if (g_pMainVinyl != nullptr) {
			SAFE_DELETE(g_pMainVinyl);
		}

		// Nạp đĩa mới của bài hát vừa chọn vào mâm xoay
		g_pMainVinyl = new Sprite2D(
			{ (SCREEN_WIDTH / 2.0f) - 62.0f, (SCREEN_HEIGHT / 2.0f) + 2.0f },
			{ 800.0f, 800.0f },
			g_VinylRotation, // Giữ nguyên góc quay dở dang để đĩa không bị giật texture
			{ 1.0f, 1.0f, 1.0f, 1.0f },
			BLENDSTATE_ALFA,
			g_StageTextures[g_SelectedStage]
		);

		// Ngay sau khi đổi đĩa xong, chuyển trạng thái hạ kim xuống đĩa mới
		g_CurrentState = STATE_DROPPING_ARM;
		// PlaySound(SOUND_NEEDLE_DROP); // Phát âm thanh nổ lách tách / cạch khi kim chạm đĩa
		break;

	case STATE_DROPPING_ARM:
		// Kim dịch chuyển từ từ từ ngoài biên vào trong mặt đĩa mới (tăng góc lên 25 độ)
		if (g_ToneArmAngle > 0.0f) {
			g_ToneArmAngle -= 1.0f;
		}

		// Khi kim vừa chạm đúng vị trí 25 độ trên đĩa, chuyển về trạng thái PLAYING tuần hoàn
		if (g_ToneArmAngle <= 0.0f) {
			g_CurrentState = STATE_PLAYING;
		}
		break;
	}

	// --- PHẦN 3: ÁP DỤNG THÔNG SỐ VÀO BIẾN ĐỒ HỌA DIRECTX ---
	// Tính toán góc xoay liên tục cho đĩa dựa trên g_DiscSpeed của khung hình đó
	if (g_DiscSpeed > 0.0f) {
		g_VinylRotation += g_DiscSpeed;
		if (g_VinylRotation >= 360.0f) g_VinylRotation -= 360.0f;
	}

	// Thực thi xoay đĩa chính
	if (g_pMainVinyl != nullptr) {
		g_pMainVinyl->SetRotation(g_VinylRotation);
	}

	// Thực thi xoay/dịch chuyển kim đĩa
	if (g_pToneArm != nullptr) {
		g_pToneArm->SetRotation(g_ToneArmAngle);
	}

	// --- PHẦN 4: XỬ LÝ HÀNG ĐĨA NHỎ BÊN TRÁI ---
	for (int i = 0; i < MAX_STAGES; i++)
	{
		g_pStageButtons[i]->Update();

		// Chiếc đĩa nhỏ nào đang được chọn phát sẽ tự động quay để tạo điểm nhấn thị giác
		if (i == g_SelectedStage) {
			g_pStageDisks[i]->SetRotation(g_VinylRotation * 2.0f); // Quay nhanh hơn đĩa chính một chút nhìn cho rõ
		}
		else {
			g_pStageDisks[i]->SetRotation(0.0f); // Các đĩa khác đứng im thẳng đứng
		}

		// Tích hợp hỗ trợ Chuột: Nếu click chuột vào đĩa bên trái lúc đang PLAYING ổn định
		if (g_pStageButtons[i]->IsClick() && g_CurrentState == STATE_PLAYING && g_SelectedStage != i)
		{
			g_NextStage = i;
			g_CurrentState = STATE_LIFTING_ARM; // Kích hoạt chuỗi nhấc kim y hệt bàn phím
		}
	}

	// --- PHẦN 5: XÁC NHẬN VÀO GAME (ENTER / SPACE) ---
	// Chỉ cho phép chuyển cảnh vào game khi đĩa đang phát nhạc ổn định, tránh lỗi bấm xen ngang lúc đang chuyển đĩa
	if (g_CurrentState == STATE_PLAYING) {
		if (Keyboard_IsKeyDownTrigger(KK_BACK) || Keyboard_IsKeyDownTrigger(KK_ENTER)) {
			SetSceneFade(SCENE_GAME); // Chuyển sang màn hình chơi game chính thức
		}
	}
}

// ==========================================
// HÀM VẼ ĐỒ HỌA (DRAW)
// ==========================================
void StageSelect_Draw(void)
{
	// Quy tắc vẽ: Lớp nào nằm dưới vẽ trước, lớp nào nằm trên vẽ đè lên sau
	g_pBackground->Draw(); // 1. Vẽ nền gỗ/bàn máy hát dưới cùng

	g_pMainVinyl->Draw(); // 4. Vẽ đĩa xoay chính ở giữa màn hình

	g_pToneArm->Draw();   // 3. Vẽ kim đọc đĩa đè lên

	// 2. Vẽ toàn bộ danh sách đĩa nhỏ và nút chữ tương ứng ở bên trái
	for (int i = 0; i < MAX_STAGES; i++) {
		g_pStageDisks[i]->Draw();
		g_pStageButtons[i]->Draw();
	}
	
	

}
// ==========================================
// HÀM GIẢI PHÓNG BỘ NHỚ (FINALIZE)
// ==========================================
void StageSelect_Finalize(void)
{
	// Giải phóng các con trỏ đơn lẻ để tránh Memory Leak
	SAFE_DELETE(g_pBackground);
	SAFE_DELETE(g_pMainVinyl);
	SAFE_DELETE(g_pToneArm);

	// Giải phóng các mảng đối tượng vòng lặp của hàng đĩa bên trái
	for (int i = 0; i < MAX_STAGES; i++) {
		SAFE_DELETE(g_pStageDisks[i]);
		SAFE_DELETE(g_pStageButtons[i]);
	}
}