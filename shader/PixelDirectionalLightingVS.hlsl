#include "Common.hlsl" //必須インクルード

void main(in VS_IN In, out PS_IN Out)
{
    // 頂点変換
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    Out.Position = mul(In.Position, wvp); // 投影空間への変換

    // 法線の回転のみ行い、ピクセルシェーダーへ渡す(ライティング計算はしない)
    In.Normal.w = 0.0f;
    float4 worldNormal = mul(In.Normal, World);
    Out.Normal = worldNormal;

    Out.Diffuse = In.Diffuse * MaterialDiffuse;	//モデルそのもののMaterialカラーを乗算（Mayaで言うハイパーシェード）
    Out.TexCoord = In.TexCoord;
    Out.WorldPosition = mul(In.Position, World);
}
