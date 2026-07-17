#include "main.h"
#include "input_manager.h"
#include "mathhelper.h"

#include "game.h"
#include "note_manager.h"
#include "status_manager.h"
#include "player.h"
#include "rainbow_note.h"
#include "sound.h"
#include "debug_params.h"
#include "gamecamera.h"

namespace
{
	float NormalizeAngleDelta(float angle)
	{
		while (angle > 180.0f) angle -= 360.0f;
		while (angle < -180.0f) angle += 360.0f;
		return angle;
	}
}

void Player::Init(NoteManager* nm, StatusManager* sm)
{
	m_Scale = { 0.017f,0.017f,0.017f };

	m_pNoteManager   = nm;
	m_pStatusManager = sm;
	m_GravityFace = FACE::FACE_FLOOR;
	m_LaneIndex = LANE_CENTER;
	m_TargetLaneIndex = LANE_CENTER;
	m_IsMoving = false;
	m_MoveTimer = 0.0f;
	m_MoveDuration = 0.15f;

	m_IsGravityMoving = false;
	m_GravityTimer = 0.0f;
	m_GravityDuration = 0.3f;
	m_WasHoldingRope = false;
	m_IsPostRopeSnapping = false;

	m_Position = { 0.0f,-2.5f,0.0f };
	m_StartPos = m_Position;
	m_TargetPos = m_Position;
	m_Rotation = { 0.0f,180.0f,0.0f };
	m_GravityStartPos = m_Position;
	m_GravityStartRot = m_Rotation;

	m_IsEffectSlashActive = false;
	m_IsOverridePlaying = false;
	m_IsHoldingHoldNote = false;
	m_DamageFlashRemaining = 0.0f;
	m_DamageFlashElapsed = 0.0f;
	m_pEffectSlash = new SplitBilBoard(
		4, 4,
		{ 0.0f, 0.5f, 0.0f },
		{ 3.0f, 3.0f },
		{ 90.0f, 0.0f, 0.0f },
		"asset\\texture\\effect_slash_ver01.png",
		true
	);
	if (m_pEffectSlash)
	{
		m_pEffectSlash->SetWallFadeEnabled(false);
		m_pEffectSlash->SetLoop(false);
		m_pEffectSlash->SetFPS(30.0f);
		m_pEffectSlash->SetAnimationEnabled(false);
		m_pEffectSlash->SetBillboardMode(false);
	}

	// キャラクター用3点照明の初期化
	m_ThreePointLight.Init();

	SetAnimationBlendDuration(0.2);
	if (GetAnimationCount() > 0)
	{
		PlayAnimationByIndex(0, true);
		UpdateAnimation(dt);
	}

	m_pSwordSe = LoadMP3("asset/sound/se/sword.mp3");
	m_pEnemyHitSe = LoadMP3("asset/sound/se/enemyHit.wav");
	m_pKaihiSe = LoadMP3("asset/sound/se/kaihi.wav");
}

void Player::Reset()
{
	m_GravityFace = FACE::FACE_FLOOR;
	m_LaneIndex = LANE_CENTER;
	m_TargetLaneIndex = LANE_CENTER;
	m_IsMoving = false;
	m_MoveTimer = 0.0f;

	m_IsGravityMoving = false;
	m_GravityTimer = 0.0f;
	m_WasHoldingRope = false;
	m_IsPostRopeSnapping = false;

	m_Position = { 0.0f,-2.5f,0.0f };
	m_StartPos = m_Position;
	m_TargetPos = m_Position;
	m_Rotation = { 0.0f,180.0f,0.0f };
	m_GravityStartPos = m_Position;
	m_GravityStartRot = m_Rotation;

	m_IsEffectSlashActive = false;
	m_IsOverridePlaying = false;
	m_IsHoldingHoldNote = false;
	m_DamageFlashRemaining = 0.0f;
	m_DamageFlashElapsed = 0.0f;
	if (m_pEffectSlash)
	{
		m_pEffectSlash->SetAnimationEnabled(false);
	}
}

void Player::Update()
{
	// pad変数とGetGamePad()は不要になったため削除しました
	if (m_DamageFlashRemaining > 0.0f)
	{
		m_DamageFlashRemaining -= dt;
		m_DamageFlashElapsed += dt;
		if (m_DamageFlashRemaining <= 0.0f)
		{
			m_DamageFlashRemaining = 0.0f;
			m_DamageFlashElapsed = 0.0f;
		}
	}

	auto processJudge = [this](JUDGE result, bool isHold)
	{
		if (result == JUDGE_NONE)
			return;

		const int hpBeforeJudge = m_pStatusManager->GetHP();
		if (isHold)
			m_pStatusManager->OnJudgeHold(result);
		else
			m_pStatusManager->OnJudge(result);

		// HPが実際に減った時だけ、点滅とカメラ振動を同時に開始する。
		if (m_pStatusManager->GetHP() < hpBeforeJudge)
		{
			m_DamageFlashRemaining = D_PARAMS.damageFlashDuration;
			m_DamageFlashElapsed = 0.0f;
			GameCamera::StartDamageShake();
		}
	};

	RopeHoldNote* holdingRope = m_pNoteManager->GetHoldingRope();

	if (holdingRope)
	{
		// ロープ曲線に沿って位置・向きを更新
		float t = holdingRope->GetHoldProgress();
		XMFLOAT2 xy = holdingRope->GetCurveXY(t);
		m_Position.x = xy.x;
		m_Position.y = xy.y;

		// tが属するセグメント（面ペア）ごとに向きを補間する（全体の最短経路ではなく、
		// 経由する面を実際に一つずつ辿ることで2/4回転以上のカーブでも正しい向きになる）
		int segFaceA, segFaceB; float segLocalT;
		holdingRope->GetSegmentInfo(t, segFaceA, segFaceB, segLocalT);

		m_GravityFace     = (segLocalT >= 0.5f) ? segFaceB : segFaceA;
		m_TargetFace      = holdingRope->GetEndFace();
		m_LaneIndex       = LANE_CENTER;
		m_TargetLaneIndex = LANE_CENTER;
		m_IsMoving        = false;
		m_IsGravityMoving = false;

		// 現在のセグメント内（隣接面同士＝常に90°差）で回転を補間
		float rotStart = CalcFaceTargetRot(segFaceA).z;
		float rotEnd   = CalcFaceTargetRot(segFaceB).z;
		float diff = NormalizeAngleDelta(rotEnd - rotStart);
		m_Rotation.z = rotStart + diff * segLocalT;
	}
	else
	{
		if (m_WasHoldingRope)
		{
			m_TargetFace      = m_GravityFace;
			m_LaneIndex       = LANE_CENTER;
			m_TargetLaneIndex = LANE_CENTER;

			m_GravityStartPos = m_Position;
			m_GravityStartRot = m_Rotation;
			m_TargetPos       = CalcFaceTargetPos(m_GravityFace, m_LaneIndex);
			m_TargetRot       = CalcFaceTargetRot(m_GravityFace);

			// 回転を最短経路で補間するため差分を[-180, 180]に正規化
			float diff = NormalizeAngleDelta(m_TargetRot.z - m_GravityStartRot.z);
			m_TargetRot.z = m_GravityStartRot.z + diff;

			m_GravityTimer       = 0.0f;
			m_IsPostRopeSnapping = true; // 入力はブロックしない座標補正のみ
			m_IsMoving           = false; // 通常移動はキャンセル
		}

		//lane移動入力
		if (IsGamePlaying())
		{
			if (m_GravityFace == FACE::FACE_FLOOR || m_GravityFace == FACE::FACE_CEILING)
			{
				if (Input_IsActionTrigger(INPUT_ACTION_MOVE_LEFT))
					MoveLeft();
				else if (Input_IsActionTrigger(INPUT_ACTION_MOVE_RIGHT))
					MoveRight();
			}
			else
			{
				if (Input_IsActionTrigger(INPUT_ACTION_MOVE_UP))
					MoveRight();
				else if (Input_IsActionTrigger(INPUT_ACTION_MOVE_DOWN))
					MoveLeft();
			}
		}

		//移動補間
		if (m_IsMoving)
		{
			m_MoveTimer += dt;
			float t = m_MoveTimer / m_MoveDuration;
			if (t >= 1.0f)
			{
				t = 1.0f;
				m_LaneIndex = m_TargetLaneIndex;
				m_IsMoving = false;
			}
			float eased = 1.0f - (1.0f - t) * (1.0f - t);
			m_Position.x = m_StartPos.x + (m_TargetPos.x - m_StartPos.x) * eased;
			m_Position.y = m_StartPos.y + (m_TargetPos.y - m_StartPos.y) * eased;
		}

		//重力変更入力
		if (IsGamePlaying())
		{
			if (!m_IsGravityMoving)
			{
				if (Input_IsActionTrigger(INPUT_ACTION_GRAVITY_UP))
					ChangeGravity(FACE_CEILING);
				else if (Input_IsActionTrigger(INPUT_ACTION_GRAVITY_DOWN))
					ChangeGravity(FACE_FLOOR);
				else if (Input_IsActionTrigger(INPUT_ACTION_GRAVITY_LEFT))
					ChangeGravity(FACE_LEFT_WALL);
				else if (Input_IsActionTrigger(INPUT_ACTION_GRAVITY_RIGHT))
					ChangeGravity(FACE_RIGHT_WALL);
			}
		}

		//重力移動補間（通常の重力移動 or ロープ終端からの座標補正）
		if (m_IsGravityMoving || m_IsPostRopeSnapping)
		{
			m_GravityTimer += dt;
			float t = m_GravityTimer / m_GravityDuration;
			if (t >= 1.0f)
			{
				t = 1.0f;
				m_GravityFace = m_TargetFace;
				m_IsGravityMoving = false;
				m_IsPostRopeSnapping = false;
			}
			float eased = 1.0f - (1.0f - t) * (1.0f - t);
			m_Position.x = m_GravityStartPos.x + (m_TargetPos.x - m_GravityStartPos.x) * eased;
			m_Position.y = m_GravityStartPos.y + (m_TargetPos.y - m_GravityStartPos.y) * eased;
			m_Rotation.z = m_GravityStartRot.z + (m_TargetRot.z - m_GravityStartRot.z) * eased;
		}

	}

	UpdateAnimation(dt);

	if (!IsAnimationPlaying())
	{
		if (m_AnimState.currentAnimName.find("jump_left") != std::string::npos ||
			m_AnimState.currentAnimName.find("jump_right") != std::string::npos)
		{
			StopAnimation();
			PlayAnimationByName("run", true);
		}
	}

	if (m_IsOverridePlaying && !IsOverrideAnimationActive())
	{
		StopOverrideAnimation();
		m_IsOverridePlaying = false;
	}

	if (m_pEffectSlash && m_IsEffectSlashActive)
	{
		m_pEffectSlash->Update();
		if (!m_pEffectSlash->IsAnimationEnabled())
		{
			m_IsEffectSlashActive = false;
		}
	}

	if (IsGamePlaying())
	{
		//ノーツヒット入力
		bool isPressed  = Input_IsActionTrigger(INPUT_ACTION_ATTACK);
		bool isHolding  = Input_IsActionDown(INPUT_ACTION_ATTACK);

		int judgeFace = m_IsGravityMoving ? m_TargetFace : m_GravityFace;
		bool isHoldingHoldNote = isHolding && m_pNoteManager->IsHoldingActiveHoldNote(m_LaneIndex, judgeFace);

		if (isHoldingHoldNote)
		{
			if (!m_IsHoldingHoldNote)
			{
				m_IsHoldingHoldNote = true;

				// 剣振りモーションをループ再生
				std::vector<std::string> overrideBones = { "BJnt_R_shoulder", "BJnt_sword" };
				PlayOverrideAnimation("attack", overrideBones, true); // loop = true
				SetOverridePlaySpeed(2.0);
				m_IsOverridePlaying = true;

				// エフェクトをループ再生
				if (m_pEffectSlash)
				{
					m_pEffectSlash->SetLoop(true);
					m_pEffectSlash->SetFPS(60.0f); // 2倍のペース (30fps -> 60fps)
					m_pEffectSlash->SetTextureIndex(0);
					m_pEffectSlash->SetAnimationEnabled(true);
					m_IsEffectSlashActive = true;
				}
			}

			// エフェクトの位置・回転を毎フレーム更新（移動中などのため）
			if (m_pEffectSlash && m_IsEffectSlashActive)
			{
				float upX = 0.0f;
				float upY = 0.0f;
				switch (judgeFace)
				{
				case FACE_FLOOR:      upX =  0.0f; upY =  1.0f; break; // 床では上方向
				case FACE_CEILING:    upX =  0.0f; upY = -1.0f; break; // 天井では下方向
				case FACE_LEFT_WALL:  upX =  1.0f; upY =  0.0f; break; // 左壁では右方向(内側)
				case FACE_RIGHT_WALL: upX = -1.0f; upY =  0.0f; break; // 右壁では左方向(内側)
				}
				m_pEffectSlash->SetPos({ m_Position.x + 1.0f * upX, m_Position.y + 1.0f * upY, m_Position.z });

				XMFLOAT3 rot = { 90.0f, 180.0f, 0.0f };
				switch (judgeFace)
				{
				case FACE_FLOOR:
					rot = { 90.0f, 180.0f, 0.0f };
					break;
				case FACE_CEILING:
					rot = { -90.0f, 180.0f, 180.0f };
					break;
				case FACE_LEFT_WALL:
					rot = { 0.0f, 270.0f, 90.0f }; // 左壁に沿わせるためのオイラー角
					break;
				case FACE_RIGHT_WALL:
					rot = { 0.0f, 90.0f, -90.0f }; // 右壁に沿わせるためのオイラー角
					break;
				}
				m_pEffectSlash->SetRotation(rot);
			}
		}
		else
		{
			if (m_IsHoldingHoldNote)
			{
				m_IsHoldingHoldNote = false;

				if (m_pEffectSlash)
				{
					m_pEffectSlash->SetLoop(false);
					m_pEffectSlash->SetFPS(30.0f); // 等倍に戻す
				}

				StopOverrideAnimation();
				SetOverridePlaySpeed(1.0);
				m_IsOverridePlaying = false;
			}
		}

		if (isPressed && !isHoldingHoldNote)
		{
			bool isJumping = (m_AnimState.currentAnimName.find("jump_left") != std::string::npos ||
							  m_AnimState.currentAnimName.find("jump_right") != std::string::npos);
			if (!isJumping)
			{
				PlayAnimationByName("run", true);
			}

			std::vector<std::string> overrideBones = { "BJnt_R_shoulder", "BJnt_sword" };
			PlayOverrideAnimation("attack", overrideBones, false);
			m_IsOverridePlaying = true;

			if (m_pEffectSlash)
			{
				m_pEffectSlash->SetTextureIndex(0);
				m_pEffectSlash->SetAnimationEnabled(true);
				m_IsEffectSlashActive = true;

				float upX = 0.0f;
				float upY = 0.0f;
				switch (judgeFace)
				{
				case FACE_FLOOR:      upX =  0.0f; upY =  1.0f; break; // 床では上方向
				case FACE_CEILING:    upX =  0.0f; upY = -1.0f; break; // 天井では下方向
				case FACE_LEFT_WALL:  upX =  1.0f; upY =  0.0f; break; // 左壁では右方向(内側)
				case FACE_RIGHT_WALL: upX = -1.0f; upY =  0.0f; break; // 右壁では左方向(内側)
				}
				m_pEffectSlash->SetPos({ m_Position.x + 1.0f * upX, m_Position.y + 1.0f * upY, m_Position.z });

				XMFLOAT3 rot = { 90.0f, 180.0f, 0.0f };
				switch (judgeFace)
				{
				case FACE_FLOOR:
					rot = { 90.0f, 180.0f, 0.0f };
					break;
				case FACE_CEILING:
					rot = { -90.0f, 180.0f, 180.0f };
					break;
				case FACE_LEFT_WALL:
					rot = { 0.0f, 270.0f, 90.0f }; // 左壁に沿わせるためのオイラー角
					break;
				case FACE_RIGHT_WALL:
					rot = { 0.0f, 90.0f, -90.0f }; // 右壁に沿わせるためのオイラー角
					break;
				}
				m_pEffectSlash->SetRotation(rot);
			}

			// 押した瞬間（KeyTrigger）：Enemy・Hold(最初の一撃) 判定
			JUDGE result = m_pNoteManager->Judge(m_LaneIndex, judgeFace);
			processJudge(result, false);

			if (result == JUDGE_HIT)
			{
				if (m_pEnemyHitSe) PlaySound(m_pEnemyHitSe, false);
			}
			else
			{
				// 攻撃がヒットしなかった（空振った）時のみ sword 音を再生
				if (m_pSwordSe)
				{
					PlaySound(m_pSwordSe, false, 0.25f);
				}
			}
		}

		if (isHolding)
		{
			// 押している間（KeyDown、トリガーの瞬間も含む）：
			// HoldNote 継続判定 / RopeHoldNote の活性化・継続判定
			int judgeFace = m_IsGravityMoving ? m_TargetFace : m_GravityFace;
			// 押した瞬間(isPressed)のみ活性化(始点タッチ)を許可するために、isPressed を渡す
			JUDGE result = m_pNoteManager->JudgeHold(m_LaneIndex, judgeFace, isPressed);
			processJudge(result, true);

			if (result == JUDGE_HIT)
			{
				if (m_pEnemyHitSe) PlaySound(m_pEnemyHitSe, false);
			}
		}
		else
		{
			// ボタンを離した瞬間：RopeHoldNote の途中離し判定
			int judgeFace = m_IsGravityMoving ? m_TargetFace : m_GravityFace;
			JUDGE result = m_pNoteManager->OnButtonRelease(m_LaneIndex, judgeFace);
			processJudge(result, false);
		}


		// RopeHoldNote 完了など、非同期で積まれた判定を処理
		while (m_pNoteManager->HasPendingJudge())
			processJudge(m_pNoteManager->PopPendingJudge(), false);

		// Orb: スコア・コンボは変化させずHP回復/取り逃し数のみ反映
		while (m_pNoteManager->HasPendingOrbEvent())
		{
			ORB_EVENT ev = m_pNoteManager->PopPendingOrbEvent();
			if (ev == ORB_EVENT_HIT)
				m_pStatusManager->OnOrbHit();
			else
				m_pStatusManager->OnOrbMiss();
		}

		// Barrier: 回避イベント処理
		while (m_pNoteManager->HasPendingBarrierEvent())
		{
			BARRIER_EVENT ev = m_pNoteManager->PopPendingBarrierEvent();
			if (ev == BARRIER_EVENT_KAIHI)
			{
				if (m_pKaihiSe) PlaySound(m_pKaihiSe, false);
			}
		}
	}
	else
	{
		// 非プレイ中：クリア時の最終判定は Game_Update 側で FINISHED 遷移前に反映済み。
		// ここに残っている分はゲームオーバー等で捨ててよいキューだけなので破棄する。
		while (m_pNoteManager->HasPendingJudge())
			m_pNoteManager->PopPendingJudge();
		while (m_pNoteManager->HasPendingOrbEvent())
			m_pNoteManager->PopPendingOrbEvent();
		while (m_pNoteManager->HasPendingBarrierEvent())
			m_pNoteManager->PopPendingBarrierEvent();
	}

	m_WasHoldingRope = (holdingRope != nullptr);
}

void Player::Draw()
{
	// プレイヤーを描く直前に3点照明を適用する。
	// PBRシェーダー(S_PBR)のみが参照するため、Playerだけがこの照明で描かれる。
	m_ThreePointLight.Apply(m_Position, m_Rotation);
	const float flashInterval = (D_PARAMS.damageFlashInterval > 0.0f)
		? D_PARAMS.damageFlashInterval
		: dt;
	const bool useDamageColor = m_DamageFlashRemaining > 0.0f &&
		(static_cast<int>(m_DamageFlashElapsed / flashInterval) % 2 == 0);
	if (useDamageColor)
	{
		const float* color = D_PARAMS.damageFlashColor;
		SetMaterialOverrideColor({ color[0], color[1], color[2], color[3] });
	}
	else
	{
		ResetMaterialOverride();
	}

	UpdateBoneMatrices();
	AnimSprite3D::Draw();
	ResetMaterialOverride();

	if (m_pEffectSlash && m_IsEffectSlashActive)
	{
		m_pEffectSlash->Draw();
	}
}

void Player::Finalize()
{
	SAFE_DELETE(m_pEffectSlash);

	UnloadSound(m_pSwordSe);     m_pSwordSe = nullptr;
	UnloadSound(m_pEnemyHitSe);  m_pEnemyHitSe = nullptr;
	UnloadSound(m_pKaihiSe);     m_pKaihiSe = nullptr;
}

void Player::MoveLeft()
{
	if (m_IsMoving || m_IsGravityMoving) return;
	int newLane = Clamp(m_LaneIndex - 1, (int)LANE_LEFT, (int)LANE_RIGHT);
	if (newLane == m_LaneIndex) return;

	m_pNoteManager->CheckAndHitBarrier(m_LaneIndex, m_GravityFace, newLane, m_GravityFace);

	m_TargetLaneIndex = newLane;
	m_StartPos = m_Position;
	m_TargetPos = CalcLaneTargetPos(m_TargetLaneIndex);
	m_MoveTimer = 0.0f;
	m_IsMoving = true;
	StopAnimation();
	PlayAnimationByName("jump_left", false);
}

void Player::MoveRight()
{
	if (m_IsMoving || m_IsGravityMoving) return;
	int newLane = Clamp(m_LaneIndex + 1, (int)LANE_LEFT, (int)LANE_RIGHT);
	if (newLane == m_LaneIndex) return;

	m_pNoteManager->CheckAndHitBarrier(m_LaneIndex, m_GravityFace, newLane, m_GravityFace);

	m_TargetLaneIndex = newLane;
	m_StartPos = m_Position;
	m_TargetPos = CalcLaneTargetPos(m_TargetLaneIndex);
	m_MoveTimer = 0.0f;
	m_IsMoving = true;
	StopAnimation();
	PlayAnimationByName("jump_right", false);
}

void Player::ChangeGravity(int targetFace)
{
	if (targetFace == m_GravityFace) return;

	m_pNoteManager->CheckAndHitBarrier(m_LaneIndex, m_GravityFace, LANE_CENTER, targetFace);

	m_TargetFace = targetFace;

	// 重力変更時の移動位置を固定で2番目のレーン（中央）にする
	m_LaneIndex = LANE_CENTER;
	m_TargetLaneIndex = LANE_CENTER;

	m_GravityStartPos = m_Position;
	m_GravityStartRot = m_Rotation;
	m_TargetPos = CalcFaceTargetPos(targetFace, m_LaneIndex);
	m_TargetRot = CalcFaceTargetRot(targetFace);

	// 回転を最短経路で補間するため差分を[-180, 180]に正規化
	float diff = NormalizeAngleDelta(m_TargetRot.z - m_GravityStartRot.z);
	m_TargetRot.z = m_GravityStartRot.z + diff;

	m_GravityTimer = 0.0f;
	m_IsGravityMoving = true;
	m_IsPostRopeSnapping = false; // ロープ終端の座標補正より優先
	m_IsMoving = false; // レーン移動はキャンセル
}

int Player::CalcNearestLane()
{
	/*float axisVal = (m_GravityFace == FACE_FLOOR || m_GravityFace == FACE_CEILING)
		? m_Position.x : m_Position.y;
	int nearest = (int)roundf(axisVal / LANE_WIDTH);
	return Clamp(nearest, (int)LANE_LEFT, (int)LANE_RIGHT);*/

	switch (m_GravityFace)
	{
	case FACE::FACE_FLOOR:
	case FACE::FACE_LEFT_WALL:
		return LANE::LANE_LEFT;

	case FACE::FACE_CEILING:
	case FACE::FACE_RIGHT_WALL:
		return LANE::LANE_RIGHT;

	default:
		return LANE::LANE_CENTER;
	}
}

XMFLOAT3 Player::CalcFaceTargetPos(int face, int laneIndex)
{
	const float TUNNEL_HALF = 2.5f;
	XMFLOAT3 pos = m_Position;
	float laneVal = laneIndex * LANE_WIDTH;
	switch (face)
	{
	case FACE_FLOOR:      pos.x = laneVal;       pos.y = -TUNNEL_HALF; break;
	case FACE_CEILING:    pos.x = laneVal;       pos.y =  TUNNEL_HALF; break;
	case FACE_LEFT_WALL:  pos.x = -TUNNEL_HALF;  pos.y = laneVal;      break;
	case FACE_RIGHT_WALL: pos.x =  TUNNEL_HALF;  pos.y = laneVal;      break;
	}
	return pos;
}

XMFLOAT3 Player::CalcFaceTargetRot(int face)
{
	XMFLOAT3 rot = m_Rotation;
	switch (face)
	{
	case FACE_FLOOR:      rot.z =   0.0f; break;
	case FACE_LEFT_WALL:  rot.z =  90.0f; break;
	case FACE_CEILING:    rot.z = 180.0f; break;
	case FACE_RIGHT_WALL: rot.z = -90.0f; break;
	}
	return rot;
}

XMFLOAT3 Player::CalcLaneTargetPos(int laneIndex)
{
	XMFLOAT3 pos = m_Position;
	switch (m_GravityFace)
	{
	case FACE_FLOOR:
	case FACE_CEILING:
		pos.x = laneIndex * LANE_WIDTH;
		break;
	case FACE_LEFT_WALL:
	case FACE_RIGHT_WALL:
		pos.y = laneIndex * LANE_WIDTH;
		break;
	}
	return pos;
}
