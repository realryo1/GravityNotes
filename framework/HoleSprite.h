// HoleSprite.h
#pragma once

#include <DirectXMath.h>
#include "sprite2d.h"

// 暗幕の「穴あき描画」用Sprite
// 半径・中心などをまとめて管理する
class HoleSprite : public Sprite2D
{
private:
	float m_RadiusPx;
	DirectX::XMFLOAT2 m_CenterPx;
	float m_SoftnessPx;
	bool m_EnableHole;

public:
	HoleSprite(const DirectX::XMFLOAT2& pos,
		const DirectX::XMFLOAT2& size,
		float rotation,
		const DirectX::XMFLOAT4& color,
		BLENDSTATE bstate,
		const wchar_t* texturePath)
		: Sprite2D(pos, size, rotation, color, bstate, texturePath)
		, m_RadiusPx(100.0f)
		, m_CenterPx(pos)
		, m_SoftnessPx(0.0f)
		, m_EnableHole(true)
	{
	}

	void SetHoleEnable(bool enable) { m_EnableHole = enable; }
	bool GetHoleEnable() const { return m_EnableHole; }

	void SetHoleCenterPx(const DirectX::XMFLOAT2& centerPx) { m_CenterPx = centerPx; }
	DirectX::XMFLOAT2 GetHoleCenterPx() const { return m_CenterPx; }

	void SetHoleRadiusPx(float radiusPx) { m_RadiusPx = radiusPx; }
	float GetHoleRadiusPx() const { return m_RadiusPx; }

	void SetHoleSoftnessPx(float softnessPx) { m_SoftnessPx = softnessPx; }
	float GetHoleSoftnessPx() const { return m_SoftnessPx; }

	void Draw()
	{
		if (m_EnableHole && m_RadiusPx > 0.0f)
		{
			Sprite2D::DrawHole(m_CenterPx, m_RadiusPx, m_SoftnessPx);
		}
		else
		{
			Sprite2D::Draw();
		}
	}
};
