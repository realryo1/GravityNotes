#include "debugcamera.h"
#include "sprite.h"
#include "texture.h"
#include "keyboard.h"
#include "fade.h"
#include "debug_ostream.h"
#include "define.h"

#include "renderer.h"
#include "font.h"
#include "mouse.h"
#include "sound.h"
#include "ClickFont.h"
#include "movie.h"
#include "camera.h"
#include <cmath>

using namespace DirectX;

static XMFLOAT3 g_DebugCameraPos = { 0.0f, 0.0f, -5.0f };
static float g_DebugCameraYaw = 0.0f;
static float g_DebugCameraPitch = 0.0f;

void DebugCamera_Initialize(void)
{
	Camera_Initialize();
	LockMouse();
	g_DebugCameraPos = { 0.0f, 2.0f, 0.0f };
	g_DebugCameraYaw = 0.0f;
	g_DebugCameraPitch = 0.0f;
}

void DebugCamera_Update(void)
{
	const float SPEED = 0.1f;
	Mouse_State mouseState;
	Mouse_GetState(&mouseState);

	g_DebugCameraYaw += static_cast<float>(mouseState.x) * 0.1f;
	g_DebugCameraPitch += static_cast<float>(mouseState.y) * 0.1f;

	if (g_DebugCameraPitch > 89.0f) g_DebugCameraPitch = 89.0f;
	if (g_DebugCameraPitch < -89.0f) g_DebugCameraPitch = -89.0f;

	float yawRad = XMConvertToRadians(g_DebugCameraYaw);
	float pitchRad = XMConvertToRadians(g_DebugCameraPitch);

	XMVECTOR forward = XMVectorSet(sinf(yawRad), 0.0f, cosf(yawRad), 0.0f);
	XMVECTOR right = XMVectorSet(cosf(yawRad), 0.0f, -sinf(yawRad), 0.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	
	XMVECTOR moveDir = XMVectorZero();

	if (Keyboard_IsKeyDown(KK_W)) moveDir = XMVectorAdd(moveDir, forward);
	if (Keyboard_IsKeyDown(KK_S)) moveDir = XMVectorSubtract(moveDir, forward);
	if (Keyboard_IsKeyDown(KK_D)) moveDir = XMVectorAdd(moveDir, right);
	if (Keyboard_IsKeyDown(KK_A)) moveDir = XMVectorSubtract(moveDir, right);

	if (Keyboard_IsKeyDown(KK_SPACE)) moveDir = XMVectorAdd(moveDir, up);
	if (Keyboard_IsKeyDown(KK_LEFTSHIFT) || Keyboard_IsKeyDown(KK_RIGHTSHIFT)) moveDir = XMVectorSubtract(moveDir, up);

	if (!XMVector3Equal(moveDir, XMVectorZero()))
	{
		moveDir = XMVector3Normalize(moveDir);
		moveDir = XMVectorScale(moveDir, SPEED);
		
		XMVECTOR pos = XMLoadFloat3(&g_DebugCameraPos);
		pos = XMVectorAdd(pos, moveDir);
		XMStoreFloat3(&g_DebugCameraPos, pos);
	}

	XMVECTOR lookDir = XMVectorSet(
		sinf(yawRad) * cosf(pitchRad),
		-sinf(pitchRad),
		cosf(yawRad) * cosf(pitchRad),
		0.0f
	);

	XMVECTOR posVec = XMLoadFloat3(&g_DebugCameraPos);
	XMVECTOR atVec = XMVectorAdd(posVec, lookDir);
	
	XMFLOAT3 atPos;
	XMStoreFloat3(&atPos, atVec);

	if (GetCamera()) {
		GetCamera()->UpdateView(g_DebugCameraPos, atPos);
	}

	if (Keyboard_IsKeyDownTrigger(KK_ESCAPE))
	{
		StartFade(SCENE_TITLE);
	}

	// カメラ位置のシェーダー送信は削除
}

void DebugCamera_Draw(void)
{
	// No longer draws a 2D sprite
}

void DebugCamera_Finalize(void)
{
}
