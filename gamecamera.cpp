#include "camera.h"
#include "keyboard.h"
#include "mouse.h"
#include "scene.h"
#include "fade.h"

#include "gamecamera.h"
#include "game.h"
#include "player.h"
#include "debug_params.h"

#include <cmath>

namespace
{
	// ダメージ時の揺れ幅と長さ。短く減衰させ、ノーツの視認性を保つ。
	constexpr float DAMAGE_SHAKE_DURATION = 0.2f;
	constexpr float DAMAGE_SHAKE_STRENGTH = 0.15f;
	constexpr float DAMAGE_SHAKE_FREQUENCY = 38.0f;
}

GameCamera* GameCamera::s_Instance=nullptr;

bool cameraIndex;

void GameCamera::Init() {
	s_Instance = new GameCamera();
	Camera_Initialize();

	// 初期面（FLOOR）の目標位置・角度を取得して即座に適用
	auto& p = D_PARAMS;
	s_Instance->m_Pos = {
		0.0f + p.cameraOffsets[0].posX,
		0.0f + p.cameraOffsets[0].posY,
		-8.0f + p.cameraOffsets[0].posZ
	};
	s_Instance->m_yaw = 0.0f;
	s_Instance->m_pitch = 0.0f;

	// カメラ角度の補間設定を初期化
	float initialYaw = 0.0f + p.cameraOffsets[0].yaw;
	float initialPitch = 0.0f + p.cameraOffsets[0].pitch;
	s_Instance->m_CurrentYaw = initialYaw;
	s_Instance->m_CurrentPitch = initialPitch;
	s_Instance->m_TargetYaw = initialYaw;
	s_Instance->m_TargetPitch = initialPitch;
	s_Instance->m_AngleLerpSpeed = 0.05f; // ゆっくり補間（値が小さいほど滑らか）
	s_Instance->m_DamageShakeRemaining = 0.0f;
	s_Instance->m_DamageShakeElapsed = 0.0f;

	cameraIndex = true;
}

void GameCamera::Update(Player* player) {
	//if (Keyboard_IsKeyDownTrigger(KK_D4))cameraIndex= !cameraIndex;

	if (!cameraIndex)
	{
		const float SPEED = 0.1f;
		Mouse_State mouseState;
		Mouse_GetState(&mouseState);

		s_Instance->m_yaw += static_cast<float>(mouseState.dx) * 0.1f;
		s_Instance->m_pitch += static_cast<float>(mouseState.dy) * 0.1f;

		if (s_Instance->m_pitch > 89.0f) s_Instance->m_pitch = 89.0f;
		if (s_Instance->m_pitch < -89.0f) s_Instance->m_pitch = -89.0f;

		float yawRad = XMConvertToRadians(s_Instance->m_yaw);
		float pitchRad = XMConvertToRadians(s_Instance->m_pitch);

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

			XMVECTOR pos = XMLoadFloat3(&s_Instance->m_Pos);
			pos = XMVectorAdd(pos, moveDir);
			XMStoreFloat3(&s_Instance->m_Pos, pos);
		}

		XMVECTOR lookDir = XMVectorSet(
			sinf(yawRad) * cosf(pitchRad),
			-sinf(pitchRad),
			cosf(yawRad) * cosf(pitchRad),
			0.0f
		);

		XMVECTOR posVec = XMLoadFloat3(&s_Instance->m_Pos);
		XMVECTOR atVec = XMVectorAdd(posVec, lookDir);

		XMFLOAT3 atPos;
		XMStoreFloat3(&atPos, atVec);

		if (GetCamera()) {
			XMFLOAT3 viewPos = s_Instance->m_Pos;
			s_Instance->ApplyDamageShake(viewPos, atPos);
			GetCamera()->UpdateView(viewPos, atPos);
		}
	}
	else {
		// プレイヤーの重力面に応じてカメラの目標角度を更新
		// 重力移動中なら目標の重力面を使用（即座にカメラが反応する）
		int targetGravityFace = player->IsGravityMoving() ? player->GetTargetFace() : player->GetGravityFace();
		s_Instance->UpdateCameraAngleByGravity(targetGravityFace);

		// 現在の角度を目標角度に向けてゆっくり補間
		s_Instance->m_CurrentYaw += (s_Instance->m_TargetYaw - s_Instance->m_CurrentYaw) * s_Instance->m_AngleLerpSpeed;
		s_Instance->m_CurrentPitch += (s_Instance->m_TargetPitch - s_Instance->m_CurrentPitch) * s_Instance->m_AngleLerpSpeed;

		// カメラ位置の補間先（真ん中から少し後ろ＋面ごとのオフセット）
		auto& p = D_PARAMS;
		XMFLOAT3 targetPos = {
			0.0f + p.cameraOffsets[targetGravityFace].posX,
			0.0f + p.cameraOffsets[targetGravityFace].posY,
			-8.0f + p.cameraOffsets[targetGravityFace].posZ
		};

		// 位置もゆっくり補間
		s_Instance->m_Pos.x += (targetPos.x - s_Instance->m_Pos.x) * s_Instance->m_AngleLerpSpeed;
		s_Instance->m_Pos.y += (targetPos.y - s_Instance->m_Pos.y) * s_Instance->m_AngleLerpSpeed;
		s_Instance->m_Pos.z += (targetPos.z - s_Instance->m_Pos.z) * s_Instance->m_AngleLerpSpeed;

		// 固定された中央位置を基準に注視点を計算（プレイヤーの位置は使わない）
		XMFLOAT3 centerPos = { 0.0f, 0.0f, 0.0f };

		// 角度をラジアンに変換
		float yawRad = XMConvertToRadians(s_Instance->m_CurrentYaw);
		float pitchRad = XMConvertToRadians(s_Instance->m_CurrentPitch);

		// 中央位置から15度分オフセットした注視点を計算
		float offsetDistance = 1.0f; // オフセットの強さ
		XMFLOAT3 atPos;
		atPos.x = centerPos.x + offsetDistance * sinf(yawRad);
		atPos.y = centerPos.y + offsetDistance * sinf(pitchRad);
		atPos.z = centerPos.z;

		s_Instance->m_AtPos = atPos;

		if (GetCamera()) {
			XMFLOAT3 viewPos = s_Instance->m_Pos;
			XMFLOAT3 viewAt = s_Instance->m_AtPos;
			s_Instance->ApplyDamageShake(viewPos, viewAt);
			GetCamera()->UpdateView(viewPos, viewAt);
		}
	}
}

void GameCamera::StartDamageShake()
{
	if (!s_Instance)
		return;

	s_Instance->m_DamageShakeRemaining = DAMAGE_SHAKE_DURATION;
	s_Instance->m_DamageShakeElapsed = 0.0f;
}

void GameCamera::ApplyDamageShake(XMFLOAT3& cameraPos, XMFLOAT3& targetPos)
{
	if (m_DamageShakeRemaining <= 0.0f)
		return;

	m_DamageShakeRemaining -= dt;
	m_DamageShakeElapsed += dt;
	if (m_DamageShakeRemaining < 0.0f)
		m_DamageShakeRemaining = 0.0f;

	const float fade = m_DamageShakeRemaining / DAMAGE_SHAKE_DURATION;
	const float phase = m_DamageShakeElapsed * DAMAGE_SHAKE_FREQUENCY;
	const float shakeX = std::sin(phase) * DAMAGE_SHAKE_STRENGTH * fade;
	const float shakeY = std::cos(phase * 1.37f) * DAMAGE_SHAKE_STRENGTH * 0.7f * fade;

	// カメラの右・上ベクトルを使い、重力面に関係なく画面方向へ揺らす。
	XMVECTOR pos = XMLoadFloat3(&cameraPos);
	XMVECTOR target = XMLoadFloat3(&targetPos);
	XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward));
	XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
	XMVECTOR offset = XMVectorAdd(XMVectorScale(right, shakeX), XMVectorScale(up, shakeY));

	XMStoreFloat3(&cameraPos, XMVectorAdd(pos, offset));
	XMStoreFloat3(&targetPos, XMVectorAdd(target, offset));
}

void GameCamera::Draw() {

}

void GameCamera::Finalize() {
	Camera_Finalize();
}

void GameCamera::UpdateCameraAngleByGravity(int gravityFace)
{
	auto& p = D_PARAMS;
	// すべての面でベースの角度は 0.0f とする
	switch (gravityFace)
	{
	case FACE_FLOOR:
		m_TargetYaw = 0.0f + p.cameraOffsets[FACE_FLOOR].yaw;
		m_TargetPitch = 0.0f + p.cameraOffsets[FACE_FLOOR].pitch;
		break;

	case FACE_CEILING:
		m_TargetYaw = 0.0f + p.cameraOffsets[FACE_CEILING].yaw;
		m_TargetPitch = 0.0f + p.cameraOffsets[FACE_CEILING].pitch;
		break;

	case FACE_LEFT_WALL:
		m_TargetYaw = 0.0f + p.cameraOffsets[FACE_LEFT_WALL].yaw;
		m_TargetPitch = 0.0f + p.cameraOffsets[FACE_LEFT_WALL].pitch;
		break;

	case FACE_RIGHT_WALL:
		m_TargetYaw = 0.0f + p.cameraOffsets[FACE_RIGHT_WALL].yaw;
		m_TargetPitch = 0.0f + p.cameraOffsets[FACE_RIGHT_WALL].pitch;
		break;
	}
}
