

struct TF_material
{
		int	materialType;
		float2  uvScale;
		float	worldSize;
		
		float2  uvScaleClampAlpha;
		uint	debugAlpha;
		float	buf_____06;

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
		float	buf_____04;

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

		float4	ecotopeMasks[15];
};

