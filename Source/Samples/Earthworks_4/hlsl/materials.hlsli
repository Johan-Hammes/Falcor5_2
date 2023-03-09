

#define MATERIAL_TYPE_STANDARD 0
#define MATERIAL_TYPE_MULTILAYER 1
#define MATERIAL_TYPE_PUDDLE 2
#define MATERIAL_TYPE_RUBBER 3

struct TF_material
{
		int	materialType;
		float2  uvScale;
		float	worldSize;
		
		float2  uvScaleClampAlpha;
		uint	debugAlpha;
		float	uvRotation;

		float	useAlpha;
		float	vertexAlphaScale;
		float	baseAlphaScale;
		float	detailAlphaScale;

		uint	baseAlphaTexture;
		float	baseAlphaBrightness;
		float	baseAlphaContrast;
		uint	baseAlphaClampU;

		uint	detailAlphaTexture;
		float	detailAlphaBrightness;
		float	detailAlphaContrast;
		float	useAbsoluteElevation;

		uint	useElevation;
		float	useVertexY;
		float	YOffset;
		uint	baseElevationTexture;

		float	baseElevationScale;
		float	baseElevationOffset;
		uint	detailElevationTexture;
		float	detailElevationScale;

		float	detailElevationOffset;
		float3	buf_____02;

		int		standardMaterialType;
		float3	buf_____03;

		uint	useColour;
		uint	baseAlbedoTexture;
		float	albedoBlend;
		uint	detailAlbedoTexture;

		float3	albedoScale;
		float	uvWorldRotation;

		uint	baseRoughnessTexture;
		float	roughnessBlend;
		uint	detailRoughnessTexture;
		float	roughnessScale;

		float	porosity;
		float3	buf_____05;

		uint	useEcotopes;
		float	permanenceElevation;
		float	permanenceColour;
		float	permanenceEcotopes;

		float	cullA;
		float	cullB;
		float	cullC;
		uint	ecotopeTexture;

        uint    subMaterials[8];

		float4	ecotopeMasks[15];
};


struct _uv
{
    float2 object;      // object space
    float2 side;        // modified object for solving sides of beziers
    float2 world;       // world space scale only - about 1mm presision at 16km should be ok for now
};




#define MATERIAL_EDIT_SELECT 2040
#define MATERIAL_EDIT_GREEN 2041
#define MATERIAL_EDIT_BLUE 2042
#define MATERIAL_EDIT_WHITEDOT 2043
#define MATERIAL_EDIT_REDDOT 2044


#define MATERIAL_BLEND 2045
#define MATERIAL_SOLID 2046
#define MATERIAL_CURVATURE 2047


#if defined(CALLEDFROMHLSL)

void solveUV(const TF_material _mat, const float2 worldPos, const float2 uvIn, inout _uv uv)
{
    uv.side = uvIn;

    float2x2 rotate;
    sincos(_mat.uvRotation, rotate[0][1], rotate[0][0]);
    rotate[1][0] = -rotate[0][1];
    rotate[1][1] = rotate[0][0];
    uv.object = mul(rotate, uv.side * _mat.uvScale);

    sincos(_mat.uvWorldRotation, rotate[0][0], rotate[0][1]);
    rotate[1][0] = -rotate[0][1];
    rotate[1][1] = rotate[0][0];
    uv.world = mul(rotate, worldPos / _mat.worldSize);
}


float solveAlpha(const TF_material _mat, _uv uv, float _vertexAlpha)
{
    float alpha = 1;

    if (_mat.useAlpha)
    {
        if (_mat.baseAlphaClampU)
        {
            float sideAlpha = smoothstep(0, _mat.uvScaleClampAlpha.x, abs(uv.side.x)) * (1 - smoothstep(_mat.uvScaleClampAlpha.y, 1, abs(uv.side.x)));


            //sideAlpha += range * (alphaDetail-0.5);

            if (_mat.baseAlphaScale > 0.05)
            {
                uv.side.xy *= _mat.uvScale;    // without rotate
                uv.side.x = clamp(sideAlpha, 0.1, 0.9);
                float alphaBase = gmyTextures.T[_mat.baseAlphaTexture].Sample(SMP, uv.side).r;

                sideAlpha = lerp(1, saturate((alphaBase + _mat.baseAlphaBrightness) * _mat.baseAlphaContrast), _mat.baseAlphaScale);
            }

            //float range = min(1 - sideAlpha, sideAlpha);
            float alphaDetail = gmyTextures.T[_mat.detailAlphaTexture].Sample(SMP, uv.world).r;
            alpha *= lerp(1, saturate((sideAlpha + alphaDetail + _mat.detailAlphaBrightness) * _mat.detailAlphaContrast), _mat.detailAlphaScale);

            alpha *= sideAlpha;
        }
        else
        {
            float alphaBase = gmyTextures.T[_mat.baseAlphaTexture].Sample(SMP, uv.object).r;
            float alphaDetail = gmyTextures.T[_mat.detailAlphaTexture].Sample(SMP, uv.world).r;

            alpha *= lerp(1, smoothstep(0, 1, _vertexAlpha), _mat.vertexAlphaScale);
            alpha *= lerp(1, saturate((alphaBase + _mat.baseAlphaBrightness) * _mat.baseAlphaContrast), _mat.baseAlphaScale);
            alpha *= lerp(1, saturate((alphaDetail + _mat.detailAlphaBrightness) * _mat.detailAlphaContrast), _mat.detailAlphaScale);
        }
    }

    return alpha;
}


#endif
