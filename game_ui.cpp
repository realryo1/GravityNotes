#include "game_ui.h"
#include "define.h"
#include <string>
#include "sound.h"
#include "game.h"

// --- 仮置き定数（後で調整してください） ---
//コンボ表示
static constexpr float DIGIT_W        = 45.0f;   // 1桁の幅
static constexpr float DIGIT_H        = 60.0f;   // 1桁の高さ
static constexpr float DIGIT_SPACING  = 35.0f;   // 桁間隔
static constexpr float DIGIT_ANCHOR_X = 150.0f;   // 1桁目（一の位）の固定X座標
static constexpr float COMBO_CENTER_Y = 45.0f;  // 数字のY座標
static constexpr float LABEL_X        = DIGIT_ANCHOR_X + DIGIT_W * 0.5f + 8.0f;
static constexpr float LABEL_Y        = COMBO_CENTER_Y;

// Hit / Miss 表示
static constexpr float JUDGE_X           = 150.0f;                 // 表示中心X
static constexpr float JUDGE_Y           = 120.0f;                 // 表示中心Y
static constexpr float JUDGE_W           = 150.0f;                 // スプライト幅
static constexpr float JUDGE_H           = 150.0f;                 // スプライト高さ
static constexpr float JUDGE_DISPLAY_SEC = 0.6f;                   // 表示秒数
static constexpr float HANTEI_UI_X       = 180.0f;                 // 判定表示位置のXマージン
static constexpr float HANTEI_UI_Y       = 180.0f;                 // 判定表示位置のYマージン
static constexpr float LANE_OFFSET_X     = 180.0f;                  // 横方向のレーンずれ幅
static constexpr float LANE_OFFSET_Y     = 150.0f;                  // 縦方向のレーンずれ幅

// HP バー
static constexpr float HP_BAR_LEFT  = SCREEN_WIDTH  * 0.78f;  // バー左端X
static constexpr float HP_BAR_Y     = SCREEN_HEIGHT * 0.06f;  // バー中心Y
static constexpr float HP_BAR_MAX_W = 250.0f;                 // バー最大幅
static constexpr float HP_BAR_H     = 24.0f;                  // バー高さ
static constexpr float HP_TEXT_X    = HP_BAR_LEFT;            // テキスト左端X
static constexpr float HP_TEXT_Y    = HP_BAR_Y - 22.0f;       // テキストY（バー上）

void GameUI::Init()
{
    for (int i = 0; i < COMBO_MAX_DIGITS; ++i)
    {
        m_pComboDigits[i] = new FadableSplitSprite(
            { DIGIT_ANCHOR_X, COMBO_CENTER_Y },
            { DIGIT_W, DIGIT_H },
            0.0f,
            { 1.0f, 1.0f, 1.0f, 1.0f },
            BLENDSTATE_ALFA,
            L"asset\\texture\\Ingame_Number_UI.png",
            5, 2
        );
        // 初期は全桁を画面外へ
        m_pComboDigits[i]->SetPos({ -999.0f, -999.0f });
    }

    m_pComboLabel = new FontRenderer(
        { LABEL_X, LABEL_Y },
        31.0f, 0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        "COMBO",
        TA_START
    );

    // 初期コンボ(0)を表示
    UpdateComboDigits(0);

    // HP バー背景（暗いグレー、幅固定）
    float bgCX = HP_BAR_LEFT + HP_BAR_MAX_W * 0.5f;
    m_pHPBarBg = new Sprite2D(
        { bgCX, HP_BAR_Y },
        { HP_BAR_MAX_W, HP_BAR_H },
        0.0f,
        { 0.2f, 0.2f, 0.2f, 1.0f },
        BLENDSTATE_NONE,
        L"asset\\texture\\fade.png"
    );
    // HP バー前景（緑、HP 比率で幅が変わる）
    m_pHPBarFg = new Sprite2D(
        { bgCX, HP_BAR_Y },
        { HP_BAR_MAX_W, HP_BAR_H },
        0.0f,
        { 0.2f, 1.0f, 0.3f, 1.0f },
        BLENDSTATE_NONE,
        L"asset\\texture\\fade.png"
    );
    // Hit / Miss / Kaihi スプライト（初期は非表示、タイマーで制御）
    m_pHitSprite = new Sprite2D(
        { JUDGE_X, JUDGE_Y }, { JUDGE_W, JUDGE_H }, 0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f }, BLENDSTATE_ALFA,
        L"asset\\texture\\Ingame_Rank_hit_UI.png"
    );
    m_pMissSprite = new Sprite2D(
        { JUDGE_X, JUDGE_Y }, { JUDGE_W, JUDGE_H }, 0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f }, BLENDSTATE_ALFA,
        L"asset\\texture\\Ingame_Rank_miss_UI.png"
    );
    m_pKaihiSprite = new Sprite2D(
        { JUDGE_X, JUDGE_Y }, { JUDGE_W, JUDGE_H }, 0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f }, BLENDSTATE_ALFA,
        L"asset\\texture\\Ingame_Rank_just_UI.png"
    );

    // HP テキスト
    m_pHPText = new FontRenderer(
        { HP_TEXT_X, HP_TEXT_Y },
        20.0f, 0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        "HP 1000/ 1000",
        TA_START
    );

    // 演出用スプライトの生成
    m_pFadeOverlay = new Sprite2D(
        { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
        { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT },
        0.0f,
        { 0.0f, 0.0f, 0.0f, 0.0f }, // 初期値は透明
        BLENDSTATE_ALFA,
        L"asset\\texture\\fade.png"
    );

    m_pClearSprite = new Sprite2D(
        { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
        { 800.0f, 800.0f },
        0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        BLENDSTATE_ALFA,
        L"asset\\texture\\Result_Text_Clear_UI.png"
    );

    m_pAllHitSprite = new Sprite2D(
        { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - 150.0f }, // 150上
        { 800.0f, 800.0f },
        0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        BLENDSTATE_ALFA,
        L"asset\\texture\\Result_Text_AllHit_UI.png"
    );

    m_pGameOverSprite = new Sprite2D(
        { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
        { 800.0f, 800.0f },
        0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        BLENDSTATE_ALFA,
        L"asset\\texture\\Result_Text_GameOver_UI.png"
    );

    m_ShowEndOverlay = false;
    m_ShowLogos      = false;
    m_IsDead         = false;
    m_IsAllHit       = false;
    m_FadeTimer      = 0.0f;
    m_FadeDuration   = 1.0f;
    m_LogoAnimTimer  = 0.0f;
    m_LogoAnimDuration = 0.35f;

    // 風切りエフェクトの初期化
    m_pWindCutSprite = new FadableSplitSprite(
        { SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f },
        { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT },
        0.0f,
        { 1.0f, 1.0f, 1.0f, 1.0f },
        BLENDSTATE_ALFA,
        L"asset\\texture\\effect_windCut_ver01.png",
        5, 6
    );
    m_WindCutAnimTimer = 0.0f;
    m_WindCutAlpha     = 0.0f;
    m_IsHoldingRainbow = false;

    // SEロード
    m_pStageClearSe = LoadMP3("asset/sound/se/StageClear.wav");
    m_pGameOverSe   = LoadMP3("asset/sound/se/GameOver.wav");
    m_pAllHitSe     = LoadMP3("asset/sound/se/AllHit.wav");
}

void GameUI::Reset()
{
    m_LastCombo   = -1;
    UpdateComboDigits(0);

    m_LastHP   = -1;
    UpdateHP(1000, 1000);

    m_JudgeTimer   = 0.0f;
    m_CurrentJudge = JUDGE_NONE;

    if (m_pHitSprite)
    {
        m_pHitSprite->SetPos({ JUDGE_X, JUDGE_Y });
        m_pHitSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
    if (m_pMissSprite)
    {
        m_pMissSprite->SetPos({ JUDGE_X, JUDGE_Y });
        m_pMissSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
    if (m_pKaihiSprite)
    {
        m_pKaihiSprite->SetPos({ JUDGE_X, JUDGE_Y });
        m_pKaihiSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    m_ShowEndOverlay = false;
    m_ShowLogos      = false;
    m_IsDead         = false;
    m_IsAllHit       = false;

    m_FadeTimer    = 0.0f;
    m_LogoAnimTimer    = 0.0f;

    m_WindCutAnimTimer = 0.0f;
    m_WindCutAlpha     = 0.0f;
    m_IsHoldingRainbow = false;

    if (m_pStageClearSe != nullptr) {
        StopSound(m_pStageClearSe);
    }
    if (m_pGameOverSe != nullptr) {
        StopSound(m_pGameOverSe);
    }
    if (m_pAllHitSe != nullptr) {
        StopSound(m_pAllHitSe);
    }
}

void GameUI::Update(const StatusManager* pStatus, bool isHoldingRainbow, int gravityFace, int laneIndex)
{
    m_IsHoldingRainbow = isHoldingRainbow;

    if (m_IsHoldingRainbow)
    {
        m_WindCutAlpha += (1.0f / 0.24f) * dt; // 約0.24秒でフェードイン
        if (m_WindCutAlpha > 1.0f) m_WindCutAlpha = 1.0f;
    }
    else
    {
        m_WindCutAlpha -= (1.0f / 0.30f) * dt; // 約0.30秒でフェードアウト
        if (m_WindCutAlpha < 0.0f) m_WindCutAlpha = 0.0f;
    }

    if (m_WindCutAlpha > 0.0f)
    {
        m_WindCutAnimTimer += dt;
        int textureNumber = static_cast<int>(m_WindCutAnimTimer * 30.0f) % 30;
        if (m_pWindCutSprite)
        {
            m_pWindCutSprite->SetTextureNumber(textureNumber);
            m_pWindCutSprite->SetColor({ 1.0f, 1.0f, 1.0f, m_WindCutAlpha });
        }
    }
    else
    {
        m_WindCutAnimTimer = 0.0f;
    }

    int combo = pStatus->GetCombo();
    if (combo != m_LastCombo)
    {
        UpdateComboDigits(combo);
        m_LastCombo = combo;
    }

    int hp    = pStatus->GetHP();
    int maxHP = pStatus->GetMaxHP();
    if (hp != m_LastHP)
    {
        UpdateHP(hp, maxHP);
        m_LastHP = hp;
    }

    if (m_JudgeTimer > 0.0f)
    {
        m_JudgeTimer -= dt;
        if (m_JudgeTimer < 0.0f) m_JudgeTimer = 0.0f;

        float elapsed = JUDGE_DISPLAY_SEC - m_JudgeTimer;
        float animDuration = 0.25f; // ぽこんアニメーションの秒数

        float s = 1.0f;     // スケール倍率
        float alpha = 1.0f; // アルファ値

        if (elapsed < animDuration)
        {
            float t = elapsed / animDuration;
            alpha = sqrtf(t); // すばやくフェードイン
            if (alpha > 1.0f) alpha = 1.0f;

            // Ease-Out Back イージング
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            float tm1 = t - 1.0f;
            s = 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
        }

        XMFLOAT2 pos = { JUDGE_X, JUDGE_Y };
        XMFLOAT2 size = { JUDGE_W * s, JUDGE_H * s };
        XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, alpha };

        if (m_pHitSprite)
        {
            m_pHitSprite->SetPos(pos);
            m_pHitSprite->SetSize(size);
            m_pHitSprite->SetColor(color);
        }
        if (m_pMissSprite)
        {
            m_pMissSprite->SetPos(pos);
            m_pMissSprite->SetSize(size);
            m_pMissSprite->SetColor(color);
        }
        if (m_pKaihiSprite)
        {
            m_pKaihiSprite->SetPos(pos);
            m_pKaihiSprite->SetSize(size);
            m_pKaihiSprite->SetColor(color);
        }
    }

    // 終了時の透過フェードイン更新
    if (m_ShowEndOverlay && m_FadeTimer < m_FadeDuration)
    {
        m_FadeTimer += dt;
        if (m_FadeTimer > m_FadeDuration)
            m_FadeTimer = m_FadeDuration;

        float alpha = (m_FadeTimer / m_FadeDuration) * 0.5f; // 最大0.5
        if (m_pFadeOverlay)
            m_pFadeOverlay->SetColor({ 0.0f, 0.0f, 0.0f, alpha });
    }

    // ロゴ「ぽこん！」ポップアニメーション更新
    if (m_ShowLogos && m_LogoAnimTimer < m_LogoAnimDuration)
    {
        m_LogoAnimTimer += dt;
        if (m_LogoAnimTimer > m_LogoAnimDuration)
            m_LogoAnimTimer = m_LogoAnimDuration;

        float t = m_LogoAnimTimer / m_LogoAnimDuration; // 0.0 -> 1.0

        // 急激に表示されるよう、立ち上がりを早くする (tの平方根を使用)
        float alpha = sqrtf(t);
        if (alpha > 1.0f) alpha = 1.0f;

        // Ease-Out Back イージング (ぽこんと弾む)
        float s = 1.0f;
        if (t < 1.0f)
        {
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            float tm1 = t - 1.0f;
            s = 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
        }

        float baseSize = 800.0f;
        float currentSize = baseSize * s;

        if (m_pClearSprite)
        {
            m_pClearSprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
            m_pClearSprite->SetSize({ currentSize, currentSize });
        }
        if (m_pAllHitSprite)
        {
            m_pAllHitSprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
            m_pAllHitSprite->SetSize({ currentSize, currentSize });
        }
        if (m_pGameOverSprite)
        {
            m_pGameOverSprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
            m_pGameOverSprite->SetSize({ currentSize, currentSize });
        }
    }
}

void GameUI::UpdateComboDigits(int combo)
{
    std::string s = std::to_string(combo);
    int numDigits = static_cast<int>(s.size());

    for (int i = 0; i < COMBO_MAX_DIGITS; ++i)
    {
        // 右詰めで有効桁を割り当てる
        int digitIndex = i - (COMBO_MAX_DIGITS - numDigits);

        if (digitIndex < 0)
        {
            // 使わない桁は画面外へ
            m_pComboDigits[i]->SetPos({ -999.0f, -999.0f });
        }
        else
        {
            m_pComboDigits[i]->SetTextureNumber(s[digitIndex] - '0');

            // 一の位を DIGIT_ANCHOR_X に固定し、桁が増えるほど左へ並べる
            float x = DIGIT_ANCHOR_X + (i - (COMBO_MAX_DIGITS - 1)) * DIGIT_SPACING;
            m_pComboDigits[i]->SetPos({ x, COMBO_CENTER_Y });
        }
    }
}

void GameUI::NotifyJudge(JUDGE judge)
{
    if (judge == JUDGE_NONE) return;
    m_CurrentJudge = judge;
    m_JudgeTimer   = JUDGE_DISPLAY_SEC;
}

void GameUI::UpdateHP(int hp, int maxHP)
{
    if (hp < 0) hp = 0;
    float ratio = (maxHP > 0) ? static_cast<float>(hp) / maxHP : 0.0f;
    float fgW   = HP_BAR_MAX_W * ratio;
    float fgCX  = HP_BAR_LEFT + fgW * 0.5f;
    m_pHPBarFg->SetSize({ fgW, HP_BAR_H });
    m_pHPBarFg->SetPos({ fgCX, HP_BAR_Y });
    m_pHPText->SetText("HP " + std::to_string(hp) + " / " + std::to_string(maxHP));
}

void GameUI::Draw()
{
    // 風切りエフェクト（背景側としてUIの背後に描画する）
    if (m_WindCutAlpha > 0.0f && m_pWindCutSprite)
    {
        m_pWindCutSprite->Draw();
    }

    // HP
    m_pHPBarBg->Draw();
    m_pHPBarFg->Draw();
    m_pHPText->Draw();

    // Hit / Miss / Kaihi
    if (m_JudgeTimer > 0.0f)
    {
        if (m_CurrentJudge == JUDGE_KAIHI) m_pKaihiSprite->Draw();
        else if (m_CurrentJudge == JUDGE_MISS || m_CurrentJudge == JUDGE_PASS_MISS || m_CurrentJudge == JUDGE_HOLD_MISS) m_pMissSprite->Draw();
        else m_pHitSprite->Draw();
    }

    // コンボ
    m_pComboLabel->Draw();
    for (int i = 0; i < COMBO_MAX_DIGITS; ++i)
    {
        m_pComboDigits[i]->Draw();
    }

    // 結果オーバーレイの描画
    if (m_ShowEndOverlay)
    {
        if (m_pFadeOverlay) m_pFadeOverlay->Draw();

        if (m_ShowLogos)
        {
            if (m_IsDead)
            {
                if (m_pGameOverSprite) m_pGameOverSprite->Draw();
            }
            else if (m_IsAllHit)
            {
                // All Hit の場合は AllHit と StageClear (ClearSprite) の両方を表示
                if (m_pAllHitSprite) m_pAllHitSprite->Draw();
                if (m_pClearSprite)  m_pClearSprite->Draw();
            }
            else
            {
                if (m_pClearSprite) m_pClearSprite->Draw();
            }
        }
    }
}

void GameUI::Finalize()
{
    for (int i = 0; i < COMBO_MAX_DIGITS; ++i)
    {
        delete m_pComboDigits[i];
        m_pComboDigits[i] = nullptr;
    }
    delete m_pComboLabel;
    m_pComboLabel = nullptr;

    delete m_pHPBarBg;   m_pHPBarBg   = nullptr;
    delete m_pHPBarFg;   m_pHPBarFg   = nullptr;
    delete m_pHPText;    m_pHPText    = nullptr;
    delete m_pHitSprite; m_pHitSprite = nullptr;
    delete m_pMissSprite;m_pMissSprite= nullptr;
    delete m_pKaihiSprite;m_pKaihiSprite= nullptr;

    delete m_pFadeOverlay;   m_pFadeOverlay   = nullptr;
    delete m_pClearSprite;   m_pClearSprite   = nullptr;
    delete m_pAllHitSprite;  m_pAllHitSprite  = nullptr;
    delete m_pGameOverSprite;m_pGameOverSprite= nullptr;

    delete m_pWindCutSprite; m_pWindCutSprite = nullptr;

    // SE解放
    UnloadSound(m_pStageClearSe);  m_pStageClearSe  = nullptr;
    UnloadSound(m_pGameOverSe);    m_pGameOverSe    = nullptr;
    UnloadSound(m_pAllHitSe);      m_pAllHitSe      = nullptr;
}

void GameUI::StartEndSequence(bool isDead, bool isAllHit)
{
    m_ShowEndOverlay = true;
    m_ShowLogos      = false;
    m_IsDead         = isDead;
    m_IsAllHit       = isAllHit;
    m_FadeTimer      = 0.0f;
    if (m_pFadeOverlay)
    {
        m_pFadeOverlay->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
    }
}

void GameUI::ShowResultLogos()
{
    m_ShowLogos = true;
    m_LogoAnimTimer = 0.0f; // アニメーションタイマーリセット
    m_FadeTimer = m_FadeDuration; // フェードを強制完了状態にする
    if (m_pFadeOverlay)
    {
        m_pFadeOverlay->SetColor({ 0.0f, 0.0f, 0.0f, 0.5f });
    }

    // クリア表示の瞬間に適切なSEを再生
    if (m_IsDead)
    {
        if (m_pGameOverSe) PlaySound(m_pGameOverSe, false);
    }
    else if (m_IsAllHit)
    {
        if (m_pAllHitSe) PlaySound(m_pAllHitSe, false);
    }
    else
    {
        if (m_pStageClearSe) PlaySound(m_pStageClearSe, false);
    }

    // 初期状態を透明＆縮小状態に設定してアニメーション開始に備える
    if (m_pClearSprite)
    {
        m_pClearSprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        m_pClearSprite->SetSize({ 0.0f, 0.0f });
    }
    if (m_pAllHitSprite)
    {
        m_pAllHitSprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        m_pAllHitSprite->SetSize({ 0.0f, 0.0f });
    }
    if (m_pGameOverSprite)
    {
        m_pGameOverSprite->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
        m_pGameOverSprite->SetSize({ 0.0f, 0.0f });
    }
}
