#pragma once

#include <directxmath.h>
using namespace DirectX;


class Transform3D
{
protected:
	XMFLOAT3 m_Position;
	XMFLOAT3 m_Rotation;
	XMFLOAT3 m_Scale;
public:
	Transform3D(XMFLOAT3 p = XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3 r = XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3 s = XMFLOAT3(0.0f, 0.0f, 0.0f)) :
		m_Position(p), m_Rotation(r), m_Scale(s)
	{
	}
	XMFLOAT3 GetPos(void) const {
		return m_Position;
	}
	XMFLOAT3 GetRot(void) const {
		return m_Rotation;
	}
	XMFLOAT3 GetScale(void) const {
		return m_Scale;
	}
	void SetPos(XMFLOAT3 p) {
		m_Position = p;
	}
	void SetRot(XMFLOAT3 r) {
		m_Rotation = r;
	}
	void SetSize(XMFLOAT3 s) {
		m_Scale = s;
	}
	XMFLOAT3 AddPos(XMFLOAT3 p) {
		m_Position.x += p.x;
		m_Position.y += p.y;
		m_Position.z += p.z;
		return m_Position;
	}
	XMFLOAT3 AddRot(XMFLOAT3 r) {
		m_Rotation.x += r.x;
		m_Rotation.y += r.y;
		m_Rotation.z += r.z;
		return m_Rotation;
	}
	XMFLOAT3 AddScale(XMFLOAT3 s) {
		m_Scale.x += s.x;
		m_Scale.y += s.y;
		m_Scale.z += s.z;
		return m_Scale;
	}

	// Position X,Y,Z個別ゲッター/セッター/アッダー
	float GetPosX(void) const { return m_Position.x; }
	float GetPosY(void) const { return m_Position.y; }
	float GetPosZ(void) const { return m_Position.z; }
	void SetPosX(float x) { m_Position.x = x; }
	void SetPosY(float y) { m_Position.y = y; }
	void SetPosZ(float z) { m_Position.z = z; }
	float AddPosX(float x) { m_Position.x += x; return m_Position.x; }
	float AddPosY(float y) { m_Position.y += y; return m_Position.y; }
	float AddPosZ(float z) { m_Position.z += z; return m_Position.z; }

	// Rotation X,Y,Z個別ゲッター/セッター/アッダー
	float GetRotX(void) const { return m_Rotation.x; }
	float GetRotY(void) const { return m_Rotation.y; }
	float GetRotZ(void) const { return m_Rotation.z; }
	void SetRotX(float x) { m_Rotation.x = x; }
	void SetRotY(float y) { m_Rotation.y = y; }
	void SetRotZ(float z) { m_Rotation.z = z; }
	float AddRotX(float x) { m_Rotation.x += x; return m_Rotation.x; }
	float AddRotY(float y) { m_Rotation.y += y; return m_Rotation.y; }
	float AddRotZ(float z) { m_Rotation.z += z; return m_Rotation.z; }

	// Scale X,Y,Z個別ゲッター/セッター/アッダー
	float GetScaleX(void) const { return m_Scale.x; }
	float GetScaleY(void) const { return m_Scale.y; }
	float GetScaleZ(void) const { return m_Scale.z; }
	void SetScaleX(float x) { m_Scale.x = x; }
	void SetScaleY(float y) { m_Scale.y = y; }
	void SetScaleZ(float z) { m_Scale.z = z; }
	float AddScaleX(float x) { m_Scale.x += x; return m_Scale.x; }
	float AddScaleY(float y) { m_Scale.y += y; return m_Scale.y; }
	float AddScaleZ(float z) { m_Scale.z += z; return m_Scale.z; }
};

// 2D 用の Transform クラス
class Transform2D
{
protected:
	XMFLOAT2 m_Position;
	float    m_Rotation; // degrees
	XMFLOAT2 m_Scale;
public:
	Transform2D(XMFLOAT2 p = XMFLOAT2(0.0f, 0.0f), float r = 0.0f, XMFLOAT2 s = XMFLOAT2(1.0f, 1.0f)) :
		m_Position(p), m_Rotation(r), m_Scale(s)
	{
	}

	// getters
	XMFLOAT2 GetPos() const { return m_Position; }
	float GetRot() const { return m_Rotation; }
	XMFLOAT2 GetScale() const { return m_Scale; }

	// setters
	void SetPos(const XMFLOAT2& p) { m_Position = p; }
	void SetRot(float r) { m_Rotation = r; }
	void SetSize(const XMFLOAT2& s) { m_Scale = s; }
	void SetRotation(float r) { m_Rotation = r; }

	// adders
	XMFLOAT2 AddPos(const XMFLOAT2& p) { m_Position.x += p.x; m_Position.y += p.y; return m_Position; }
	float AddRot(float r) { m_Rotation += r; return m_Rotation; }
	XMFLOAT2 AddScale(const XMFLOAT2& s) { m_Scale.x += s.x; m_Scale.y += s.y; return m_Scale; }

	// Position X,Y個別ゲッター/セッター/アッダー
	float GetPosX(void) const { return m_Position.x; }
	float GetPosY(void) const { return m_Position.y; }
	void SetPosX(float x) { m_Position.x = x; }
	void SetPosY(float y) { m_Position.y = y; }
	float AddPosX(float x) { m_Position.x += x; return m_Position.x; }
	float AddPosY(float y) { m_Position.y += y; return m_Position.y; }

	// Scale X,Y個別ゲッター/セッター/アッダー
	float GetScaleX(void) const { return m_Scale.x; }
	float GetScaleY(void) const { return m_Scale.y; }
	void SetScaleX(float x) { m_Scale.x = x; }
	void SetScaleY(float y) { m_Scale.y = y; }
	float AddScaleX(float x) { m_Scale.x += x; return m_Scale.x; }
	float AddScaleY(float y) { m_Scale.y += y; return m_Scale.y; }
};
