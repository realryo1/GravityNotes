/*定数バッファ C言語から受け取るデータ楊の変数*/
//ワールド行列
cbuffer WorldBuffer : register(b0)
{
    matrix World;
}

//カメラ行列
cbuffer ViewBuffer : register(b1)
{
    matrix View;
}

//プロジェクション行列
cbuffer ProjectionBuffer : register(b2)
{
    matrix Projection;
}

cbuffer MaterialBuffer : register(b3)
{
    float4 MaterialAmbient;
    float4 MaterialDiffuse;
    float4 MaterialSpecular;
    float4 MaterialEmission;
    float MaterialShininess;
    float3 MaterialPadding;
}

//頂点構造体 頂点シェーダーが頂点バッファの情報を受け取るための構造体
struct VS_IN
{
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

//頂点(ピクセル)構造体 頂点シェーダーの出力とピクセルシェーダーの入力を兼ねている
struct PS_IN
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

//ライト構造体 今後使用するライトのデータを受け取る構造体
struct LIGHT
{
    bool Enable;
    bool3 Dummy;
    float4 Direction;
    float4 Diffuse;
    float4 Ambient;
    
    float4 Position;
    float4 PointLightParam;
};

/*その他の定数バッファ*/
//ライトオブジェクト
cbuffer LightBuffer : register(b4)
{
    LIGHT Light;
};

//カメラ座標
cbuffer CameraBuffer : register(b5)
{
    float4 CameraPosition;
};

//汎用パラメーター
cbuffer ParameterBuffer : register(b6)
{
    float4 Parameter;
};