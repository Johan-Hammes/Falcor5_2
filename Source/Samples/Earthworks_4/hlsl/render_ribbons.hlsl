
#include "groundcover_defines.hlsli"
#include "groundcover_functions.hlsli"		




/*

struct ribbonVertex
{
    3x16bit position        += 32 meter in mm res
    10 bit material
    6 bit stuff - maybe shadow material     // 64

    3x10 bit up
    1 bit start
    1 bit free              // 32

    4x10 bit cone           // get this down to 32

    //??? radius
    // depth inside sphere 8 bits at least




    TINY  - keep inside int4 for all data
    3 x 16bit position             // 15 should be enough
    16 bit material             // some bits free 10 should be enough

    (9, 8, 10, 5) yaw pitch, radius mm up to 1m dws 2m width, start bit, 4 bits free            // up

    (9, 8, 7, 8) yaw pitch, cone, depth         // lighting


    // packing normal vectors
    cubemap face 3 , x 6, y 6   16 bits roughly 1.4 degree acuracy

    sincos hemisphere 8 bits, 7 bits        so extremely similar numbes, look at unpack
    yaw, pitch, cone (8, 7, 9)  1.4 degrees, 0.4 degree cone
    yaw, pitch, cone (9, 8, 7)  0.7 degrees, 0.8 degree cone rougly
        sincos(yaw, z, x);
        sincos(pitch, y, pln);
        float3(pln * x, y, pln * z);
        // 0.02454369260617

    yaw pitch roll? -> Matrix
    

    float3 pos;
    int A;          // start bit, leaves 31 bits unused
    float3 right;
    int B;          // material
    float4 lighting;
    float4 extra;
};

*/



SamplerState gSampler;
Texture2D gAlbedo : register(t0);
TextureCube gEnv : register(t1);

struct ribbonTextures
{
    Texture2D<float4> T[4096];
};
ParameterBlock<ribbonTextures>
textures;


StructuredBuffer<sprite_material>   materials;
StructuredBuffer<ribbonVertex>      instanceBuffer;
StructuredBuffer<xformed_PLANT>     instances;


cbuffer gConstantBuffer
{
    float4x4 viewproj;
    float3 eye;
};



struct VSOut
{
    float4 rootPos : ANCHOR;
    float3 N : NORMAL;
    float3 R : TANGENT;
    float3 U : BINORMAL;
    float4 right : TEXCOORD;
    uint4 flags : TEXCOORD1;
    float3 world : TEXCOORD2;
    float4 other : TEXCOORD3;
    float4 lighting : TEXCOORD4;
    float4 extra : TEXCOORD5;
};

struct GSOut
{
    float4 pos : SV_POSITION;
    float3 N : NORMAL;
    float3 R : TANGENT;
    float3 U : BINORMAL;
    float3 texCoords : TEXCOORD;
    uint4 flags : TEXCOORD1;
    float3 world : TEXCOORD2;
    float4 lighting : TEXCOORD3;
    float4 extra : TEXCOORD4;
};


struct GSOutCompute
{
    float4 pos : SV_POSITION;
    float4 N : NORMAL;
    float4 R : TANGENT;
    float4 U : BINORMAL;
    float4 uv : TEXCOORD;
    uint4 flags : TEXCOORD1;
    float3 world : TEXCOORD2;
    float4 lighting : TEXCOORD3;    //128 bytes
    float4 extra : TEXCOORD4;
};


inline float3 rot_xz(const float3 v, const float2 r)
{
    return float3((v.x * r.y) + (v.z * r.x), v.y, (-v.x * r.x) + (v.z * r.y));
}

inline float3 yawPitch_8bit(int yaw, int pitch)
{
    float plane, x, y, z;
    sincos(yaw * 0.02454, z, x);
    sincos((pitch - 64) * 0.02454, y, plane);
    return float3(plane * x, y, plane * z);

}

VSOut vsMain(uint vId : SV_VertexID, uint iId : SV_InstanceID)
{
    VSOut output = (VSOut)0;

    //const xformed_PLANT plant = instances[iId];
    const uint P = 0;// plant.index * 128;
    const ribbonVertex R = instanceBuffer[vId + P];

    xformed_PLANT plant;;
    plant.position = float3((iId % 10) * 20, 0, ((int) (iId / 10)) * 20) - float3(100, 0, 100);
    plant.rotation.x = 0;
    plant.rotation.y = 1;
    //sincos(iId, plant.rotation.x, plant.rotation.y);
    plant.scale = 1;

    output.rootPos = float4(plant.position + (rot_xz(R.pos, plant.rotation) * plant.scale), 1);
    
    bool absolute = (R.B >> 16) & 0x1;
    float3 up;
    float3 dir = normalize(eye - output.rootPos.xyz);
    if (absolute)
    {
        output.right = float4(rot_xz(R.right, plant.rotation), 0);
        up = normalize(cross(dir, output.right.xyz));

        output.N = normalize(cross(output.right.xyz, up));
        output.U = normalize(up);
        output.R = normalize(output.right.xyz);
        output.flags.z = 0;

    }
    else
    {
        float3 up = rot_xz(R.right, plant.rotation);
        float3 right = normalize(cross(up, dir)) ;
        output.right = float4(right, 0) * plant.scale * length(R.right);  // rather pack that diameter somewhere

        output.U = normalize(up);
        output.R = right;
        output.N = cross(right, output.U);
        output.flags.z = 1;

    }
    
    output.flags.x = R.A;
    output.flags.y = (R.B & 0xff);
    
    output.other.x = R.pos.y * 4;

    

    output.world = -dir;//    float3(dir.x, dir.z, -dir.y);

    output.lighting =  R.lighting; // FIXME rotate this still
    output.extra = R.extra;

    return output;
}



[maxvertexcount(4)]
void gsMain(line VSOut L[2], inout TriangleStream<GSOut> OutputStream)
{
    GSOut v;

    //OutputStream.RestartStrip();
    if (L[1].flags.x == 1)
    {
        v.texCoords = float3(0, 1.0, L[0].other.x);
        v.N = L[0].N;
        v.U = L[0].U;
        v.R = L[0].R;
        v.lighting = L[0].lighting;
        v.extra = L[0].extra;
        v.world = L[0].world;
        v.flags = L[0].flags;
//        if (L[0].flags.z == 0)
 //       {
  //          v.texCoords = float3(0.5, 1.0, L[0].other.x);
    //    }
        v.pos = mul(L[0].rootPos - L[0].right, viewproj);
        OutputStream.Append(v);

        v.pos = mul(L[0].rootPos + L[0].right, viewproj);
        v.texCoords.x = 1;
//        if (L[0].flags.z == 0)
  //      {
    //        v.texCoords = float3(1.0, 0.5, L[0].other.x);
      //  }
        OutputStream.Append(v);


        v.N = L[1].N;
        v.U = L[1].U;
        v.R = L[1].R;
        v.lighting = L[1].lighting;
        v.extra = L[1].extra;
        v.pos = mul(L[1].rootPos - L[1].right, viewproj);
        v.texCoords = float3(0, 0.0, L[1].other.x);
        v.world = L[1].world;
//        if (L[0].flags.z == 0)
  //      {
    //        v.texCoords = float3(0.0, 0.5, L[0].other.x);
      //  }
        OutputStream.Append(v);

        v.pos = mul(L[1].rootPos + L[1].right, viewproj);
        v.texCoords.x = 1;
//        if (L[0].flags.z == 0)
  //      {
    //        v.texCoords = float3(0.5, 0.0, L[0].other.x);
      //  }
        OutputStream.Append(v);
    }
    
}



float4 psMain(GSOut vOut, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    //return 1;
    
    
        int material = vOut.flags.y;
    if (material == 2)
    {
        material = 1;
    }

    float4 albedoAlpha = textures.T[material * 2 + 0].Sample(gSampler, vOut.texCoords.xy);
    clip(albedoAlpha.a - 0.7);

    float3 N = vOut.N;
    if (material == 0)
    {
        float3 normalTex = ((textures.T[material * 2 + 1].Sample(gSampler, vOut.texCoords.xy).rgb) * 2.0) - 1.0;
        N = (normalTex.r * vOut.R) + (normalTex.g * vOut.U) + (normalTex.b * vOut.N);
        //N = normalTex;
       //return float4(N, 1);
    }
    // consider testing backface and even changing material for that
    
    
    
    float3 R = reflect(vOut.world, N);


    float3 sunDir = normalize(float3(1.0, 0.7, -0.8));
    //float3 sunDir = normalize(float3(0.0, 0.0, 1.0));

    float A = dot(N, sunDir);
    float A_backface = saturate(-A);
    A = saturate(A*1);
    float S = pow(saturate(dot(sunDir, R)), 15);
    float AO = (saturate(1 - vOut.extra.x * 0.3) + 0.8) / 1.8;

    //float Z = dot(normalize(vOut.lighting.xyz) + 0.0 * sunDir * vOut.lighting.w, sunDir);
    //float shadow =     saturate(saturate(3 * Z) - saturate(1 * (1. - vOut.lighting.w)));

    //return float4(saturate(vOut.lighting.w), 0, saturate(-vOut.lighting.w), 1);
    float Z = dot(vOut.lighting.xyz, sunDir) + vOut.lighting.w * 2;
    float shadow = saturate(saturate(Z * 5) - saturate(vOut.extra.x * 0.07 / Z));
    //shadow = shadow * shadow * shadow;
    
    float3 sunColor = float3(1.2, 1.0, 0.7);
    float B = 0.97;
    float3 D = float3(0.5, 0.5, 0.6);
    if (material == 0)
    {
        S = 0;
        B = 0;
        D = 1.0;
    }
    // diffuse
    //shadow = 1;
    float3 final = (albedoAlpha.rgb * A) * shadow * D * sunColor * saturate(1-S);

    // back light
    //final += B * shadow * saturate(-dot(vOut.world, sunDir)) * albedoAlpha.rgb * float3(0.9, 0.7, 0.3);
    final += B * shadow * pow(saturate(dot(vOut.world, sunDir)), 2) * albedoAlpha.rgb * float3(0.9, 0.7, 0.3);

    // specular
    final += shadow * S * 0.2;

    //env
    float3 dir = N;
    dir.z *= -1;
    final += gEnv.SampleLevel(gSampler, dir, 0).rgb * albedoAlpha.rgb * saturate(AO) * 0.7;

    
    //final = albedoAlpha.rgb * D;


    
    albedoAlpha.a = 1;
    return float4(final, albedoAlpha.a);
}
