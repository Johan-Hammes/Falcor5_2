

unsigned int phaseX = 32;
unsigned int phaseY = 128;
float phaseData[128*32]{
	3668.555f, 2926.465f, 2326.993f, 1833.008f, 1431.855f, 1106.898f, 845.660f,  641.583f,	484.958f,  363.384f,	271.562f,  204.452f,	152.287f,  111.649f,	82.991f,	  61.797f,	44.586f,	  33.028f,	24.622f,
	17.957f,	  13.524f,	9.959f,	  7.328f,	5.577f,	  4.328f,	3.424f,	  2.769f,	2.291f,	  1.943f,	1.689f,	  1.504f,	1.368f,	  3663.044f, 2922.342f, 2323.913f, 1830.716f, 1430.159f, 1105.647f,
	844.741f,  640.909f,	484.465f,  363.024f,	271.298f,  204.260f,	152.147f,  111.547f,	82.915f,	  61.742f,	44.546f,	  32.999f,	24.600f,	  17.941f,	13.512f,	  9.950f,	7.322f,	  5.573f,	4.324f,
	3.422f,	  2.767f,	2.290f,	  1.942f,	1.689f,	  1.503f,	1.368f,	  3648.796f, 2911.680f, 2315.946f, 1824.790f, 1425.770f, 1102.412f, 842.364f,	639.167f,  483.190f,	362.091f,  270.617f,	203.762f,
	151.783f,  111.281f,	82.721f,	  61.600f,	44.442f,	  32.923f,	24.545f,	  17.901f,	13.483f,	  9.929f,	7.306f,	  5.561f,	4.316f,	  3.416f,	2.762f,	  2.287f,	1.940f,	  1.687f,	1.502f,
	1.367f,	  3610.093f, 2882.712f, 2294.296f, 1808.683f, 1413.843f, 1093.618f, 835.902f,	634.430f,  479.724f,	359.557f,  268.766f,	202.409f,  150.795f,	110.559f,  82.193f,	61.214f,	  44.160f,	32.717f,
	24.394f,	  17.791f,	13.402f,	  9.870f,	7.263f,	  5.530f,	4.293f,	  3.399f,	2.750f,	  2.278f,	1.933f,	  1.682f,	1.499f,	  1.365f,	3528.110f, 2821.327f, 2248.400f, 1774.531f, 1388.550f,
	1074.965f, 822.195f,	624.383f,  472.370f,	354.181f,  264.838f,	199.539f,  148.698f,	109.027f,  81.074f,	60.396f,	  43.562f,	32.279f,	  24.074f,	17.557f,	  13.231f,	9.744f,	  7.172f,	5.463f,
	4.244f,	  3.363f,	2.724f,	  2.258f,	1.919f,	  1.672f,	1.491f,	  1.359f,	3379.209f, 2709.758f, 2164.933f, 1712.390f, 1342.513f, 1041.006f, 797.237f,  606.088f,	458.980f,  344.392f,	257.686f,
	194.314f,  144.880f,	106.237f,  79.035f,	58.906f,	  42.473f,	31.483f,	  23.492f,	17.131f,	  12.919f,	9.517f,	  7.005f,	5.341f,	  4.154f,	3.298f,	  2.676f,	2.223f,	  1.894f,	1.653f,
	1.477f,	  1.349f,	3142.792f, 2532.403f, 2032.107f, 1613.414f, 1269.140f, 986.858f,	757.432f,  576.906f,	437.621f,  328.778f,	246.278f,  185.980f,	138.792f,  101.790f,	75.785f,	  56.530f,	40.736f,
	30.213f,	  22.563f,	16.452f,	  12.423f,	9.153f,	  6.739f,	5.147f,	  4.012f,	3.194f,	  2.600f,	2.168f,	  1.853f,	1.623f,	  1.456f,	1.333f,	  2808.024f, 2280.777f, 1843.314f, 1472.542f,
	1164.600f, 909.656f,	700.657f,  535.275f,	407.148f,  306.503f,	230.006f,  174.094f,	130.109f,  95.447f,	71.151f,	  53.143f,	38.261f,	  28.403f,	21.240f,	  15.484f,	11.715f,	  8.636f,	6.361f,
	4.870f,	  3.810f,	3.045f,	  2.491f,	2.088f,	  1.795f,	1.581f,	  1.425f,	1.310f,	  2381.387f, 1959.087f, 1601.254f, 1291.512f, 1030.038f, 810.172f,  627.448f,	481.577f,  367.840f,	277.771f,
	209.021f,  158.767f,	118.915f,  87.272f,	65.179f,	  48.778f,	35.071f,	  26.072f,	19.535f,	  14.237f,	10.803f,	  7.969f,	5.873f,	  4.513f,	3.549f,	  2.855f,	2.352f,	  1.986f,	1.720f,
	1.526f,	  1.385f,	1.281f,	  1893.123f, 1589.066f, 1321.544f, 1081.578f, 873.577f,	694.291f,  542.084f,	418.933f,  321.976f,	244.252f,  184.546f,	140.896f,  105.867f,	77.746f,	  58.221f,	43.695f,
	31.356f,	  23.356f,	17.550f,	  12.786f,	9.742f,	  7.192f,	5.305f,	  4.097f,	3.245f,	  2.632f,	2.189f,	  1.867f,	1.633f,	  1.462f,	1.338f,	  1.247f,	1393.695f, 1207.470f, 1030.947f,
	862.219f,  709.404f,	572.347f,  452.103f,	352.849f,  273.579f,	208.887f,  158.734f,	122.058f,  92.119f,	67.714f,	  50.896f,	38.345f,	  27.448f,	20.500f,	  15.462f,	11.259f,	  8.626f,	6.376f,
	4.708f,	  3.661f,	2.926f,	  2.399f,	2.018f,	  1.742f,	1.542f,	  1.396f,	1.289f,	  1.211f,	941.196f,  856.994f,	760.813f,  656.412f,	554.318f,  456.612f,	366.471f,  289.873f,	227.436f,
	175.178f,  134.147f,	104.126f,  79.042f,	58.178f,	  43.938f,	33.265f,	  23.739f,	17.791f,	  13.482f,	9.812f,	  7.568f,	5.603f,	  4.143f,	3.247f,	  2.623f,	2.177f,	  1.856f,	1.624f,
	1.455f,	  1.332f,	1.243f,	  1.177f,	581.426f,  571.757f,	536.500f,  482.899f,	422.101f,  357.182f,	292.568f,  235.400f,	187.487f,  146.004f,	112.890f,  88.639f,	67.760f,	  49.963f,	37.951f,
	28.898f,	  20.553f,	15.465f,	  11.783f,	8.571f,	  6.660f,	4.939f,	  3.658f,	2.893f,	  2.364f,	1.988f,	  1.718f,	1.522f,	  1.381f,	1.278f,	  1.203f,	1.148f,	  332.216f,	365.841f,
	369.003f,  350.084f,	319.049f,  278.712f,	233.808f,  191.920f,	155.547f,  122.687f,	95.927f,	  76.300f,	58.789f,	  43.444f,	33.207f,	  25.442f,	18.035f,	  13.628f,	10.442f,	  7.592f,	5.945f,
	4.417f,	  3.276f,	2.613f,	  2.159f,	1.838f,	  1.608f,	1.443f,	  1.322f,	1.235f,	  1.172f,	1.125f,	  181.973f,	232.124f,  254.032f,	255.317f,  243.442f,	220.023f,  189.351f,	158.817f,
	131.156f,  104.885f,	83.003f,	  66.921f,	51.987f,	  38.518f,	29.631f,	  22.842f,	16.145f,	  12.251f,	9.438f,	  6.859f,	5.410f,	  4.026f,	2.991f,	  2.405f,	2.007f,	  1.727f,	1.527f,
	1.383f,	  1.279f,	1.203f,	  1.148f,	1.108f,	  102.546f,	151.499f,  178.618f,	189.661f,  188.998f,	176.624f,  155.940f,	133.710f,  112.564f,	91.310f,	  73.171f,	59.803f,	  46.842f,	34.807f,
	26.946f,	  20.895f,	14.733f,	  11.225f,	8.691f,	  6.315f,	5.013f,	  3.737f,	2.779f,	  2.250f,	1.894f,	  1.644f,	1.467f,	  1.339f,	1.247f,	  1.180f,	1.131f,	  1.096f,	64.924f,
	104.079f,  129.134f,	143.659f,  149.057f,	143.752f,  130.126f,	114.077f,  97.920f,	80.600f,	  65.429f,	54.211f,	  42.814f,	31.916f,	  24.863f,	19.388f,	  13.645f,	10.436f,	  8.117f,	5.898f,
	4.709f,	  3.515f,	2.617f,	  2.132f,	1.808f,	  1.581f,	1.421f,	  1.305f,	1.222f,	  1.162f,	1.118f,	  1.086f,	47.564f,	  75.023f,	95.370f,	  110.232f,	118.648f,  117.871f,	109.361f,
	98.061f,	  85.859f,	71.751f,	  59.038f,	49.600f,	  39.500f,	29.550f,	  23.164f,	18.164f,	  12.763f,	9.797f,	  7.654f,	5.561f,	  4.464f,	3.337f,	  2.488f,	2.038f,	  1.739f,	1.531f,
	1.384f,	  1.278f,	1.202f,	  1.147f,	1.107f,	  1.078f,	37.756f,	  55.373f,	71.042f,	  84.972f,	94.683f,	  96.805f,	92.086f,	  84.529f,	75.549f,	  64.150f,	53.546f,	  45.636f,	36.657f,
	27.530f,	  21.718f,	17.123f,	  12.017f,	9.259f,	  7.264f,	5.278f,	  4.258f,	3.187f,	  2.379f,	1.958f,	  1.681f,	1.489f,	  1.353f,	1.256f,	  1.186f,	1.135f,	  1.099f,	1.072f,
	30.134f,	  40.964f,	52.982f,	  65.564f,	75.530f,	  79.418f,	77.502f,	  72.905f,	66.572f,	  57.489f,	48.724f,	  42.151f,	34.160f,	  25.763f,	20.457f,	  16.219f,	11.371f,	  8.793f,	6.927f,
	5.035f,	  4.081f,	3.058f,	  2.285f,	1.890f,	  1.631f,	1.452f,	  1.326f,	1.236f,	  1.172f,	1.125f,	  1.091f,	1.066f,	  23.954f,	30.605f,	  39.934f,	50.950f,	  60.418f,	65.188f,
	65.256f,	  62.944f,	58.749f,	  51.636f,	44.476f,	  39.072f,	31.953f,	  24.210f,	19.353f,	  15.428f,	10.809f,	  8.389f,	6.635f,	  4.824f,	3.927f,	  2.947f,	2.205f,	  1.832f,	1.588f,
	1.421f,	  1.303f,	1.220f,	  1.159f,	1.116f,	  1.084f,	1.062f,	  19.680f,	23.835f,	  31.068f,	40.331f,	  48.742f,	53.707f,	  55.078f,	54.460f,	  51.950f,	46.496f,	  40.730f,	36.345f,
	29.999f,	  22.843f,	18.384f,	  14.736f,	10.320f,	  8.039f,	6.382f,	  4.642f,	3.795f,	  2.851f,	2.135f,	  1.781f,	1.552f,	  1.394f,	1.284f,	  1.205f,	1.149f,	  1.108f,	1.079f,
	1.057f,	  17.047f,	19.720f,	  25.246f,	32.691f,	  39.741f,	44.449f,	  46.607f,	47.207f,	  46.001f,	41.944f,	  37.394f,	33.902f,	  28.245f,	21.622f,	  17.522f,	14.122f,	  9.888f,	7.730f,
	6.160f,	  4.482f,	3.679f,	  2.768f,	2.075f,	  1.737f,	1.520f,	  1.371f,	1.267f,	  1.193f,	1.140f,	  1.102f,	1.074f,	  1.054f,	15.003f,	  16.906f,	21.118f,	  26.904f,	32.574f,
	36.824f,	  39.438f,	40.900f,	  40.702f,	37.836f,	  34.360f,	31.662f,	  26.633f,	20.506f,	  16.736f,	13.562f,	  9.498f,	7.452f,	  5.960f,	4.339f,	  3.576f,	2.693f,	  2.021f,	1.698f,
	1.491f,	  1.350f,	1.252f,	  1.182f,	1.132f,	  1.096f,	1.070f,	  1.051f,	12.968f,	  14.557f,	17.816f,	  22.229f,	26.682f,	  30.440f,	33.301f,	  35.357f,	35.930f,	  34.082f,	31.562f,
	29.576f,	  25.125f,	19.466f,	  16.004f,	13.042f,	  9.137f,	7.196f,	  5.776f,	4.207f,	  3.480f,	2.624f,	  1.972f,	1.662f,	  1.465f,	1.331f,	  1.238f,	1.172f,	  1.125f,	1.090f,
	1.066f,	  1.048f,	11.302f,	  12.673f,	15.170f,	  18.458f,	21.889f,	  25.166f,	28.111f,	  30.527f,	31.657f,	  30.666f,	28.989f,	  27.634f,	23.713f,	  18.496f,	15.323f,	  12.559f,	8.803f,
	6.959f,	  5.606f,	4.086f,	  3.393f,	2.561f,	  1.927f,	1.630f,	  1.442f,	1.314f,	  1.226f,	1.163f,	  1.118f,	1.086f,	  1.062f,	1.045f,	  10.290f,	11.347f,	  13.161f,	15.521f,
	18.107f,	  20.924f,	23.811f,	  26.382f,	27.874f,	  27.585f,	26.637f,	  25.832f,	22.394f,	  17.593f,	14.691f,	  12.109f,	8.496f,	  6.742f,	5.451f,	  3.976f,	3.313f,	  2.504f,	1.886f,
	1.600f,	  1.420f,	1.299f,	  1.214f,	1.154f,	  1.112f,	1.081f,	  1.059f,	1.043f,	  9.667f,	10.391f,	  11.637f,	13.272f,	  15.190f,	17.582f,	  20.300f,	22.855f,	  24.544f,	24.818f,
	24.491f,	  24.160f,	21.160f,	  16.752f,	14.101f,	  11.690f,	8.211f,	  6.542f,	5.308f,	  3.875f,	3.239f,	  2.452f,	1.849f,	  1.574f,	1.401f,	  1.285f,	1.204f,	  1.147f,	1.106f,
	1.077f,	  1.056f,	1.041f,	  8.846f,	9.426f,	  10.326f,	11.479f,	  12.921f,	14.946f,	  17.428f,	19.846f,	  21.605f,	22.323f,	  22.522f,	22.594f,	  19.993f,	15.959f,	  13.545f,	11.294f,
	7.945f,	  6.355f,	5.174f,	  3.781f,	3.171f,	  2.403f,	1.815f,	  1.549f,	1.383f,	  1.272f,	1.194f,	  1.140f,	1.101f,	  1.074f,	1.053f,	  1.039f,	8.143f,	  8.684f,	9.374f,
	10.196f,	  11.276f,	12.960f,	  15.143f,	17.323f,	  19.046f,	20.098f,	  20.728f,	21.135f,	  18.894f,	15.213f,	  13.021f,	10.921f,	  7.697f,	6.180f,	  5.050f,	3.693f,	  3.108f,	2.359f,
	1.783f,	  1.526f,	1.367f,	  1.260f,	1.186f,	  1.134f,	1.097f,	  1.070f,	1.051f,	  1.037f,	7.859f,	  8.313f,	8.813f,	  9.371f,	10.154f,	  11.506f,	13.346f,	  15.221f,	16.831f,
	18.122f,	  19.096f,	19.774f,	  17.856f,	14.510f,	  12.527f,	10.568f,	  7.463f,	6.018f,	  4.934f,	3.612f,	  3.049f,	2.317f,	  1.754f,	1.506f,	  1.352f,	1.249f,	  1.178f,	1.128f,
	1.093f,	  1.067f,	1.049f,	  1.036f,	7.499f,	  7.869f,	8.243f,	  8.641f,	9.229f,	  10.313f,	11.830f,	  13.395f,	14.864f,	  16.330f,	17.579f,	  18.477f,	16.853f,	  13.831f,	12.048f,
	10.224f,	  7.238f,	5.861f,	  4.822f,	3.534f,	  2.993f,	2.278f,	  1.727f,	1.486f,	  1.338f,	1.239f,	  1.171f,	1.123f,	  1.089f,	1.064f,	  1.047f,	1.034f,	  7.020f,	7.296f,
	7.589f,	  7.912f,	8.393f,	  9.278f,	10.512f,	  11.789f,	13.111f,	  14.701f,	16.165f,	  17.235f,	15.881f,	  13.171f,	11.580f,	  9.887f,	7.019f,	  5.708f,	4.713f,	  3.459f,	2.938f,
	2.239f,	  1.700f,	1.467f,	  1.324f,	1.229f,	  1.163f,	1.117f,	  1.085f,	1.061f,	  1.045f,	1.033f,	  6.574f,	6.747f,	  6.980f,	7.269f,	  7.689f,	8.414f,	  9.398f,	10.411f,
	11.582f,	  13.245f,	14.862f,	  16.056f,	14.946f,	  12.535f,	11.126f,	  9.559f,	6.807f,	  5.560f,	4.607f,	  3.386f,	2.885f,	  2.202f,	1.675f,	  1.449f,	1.311f,	  1.220f,	1.157f,
	1.113f,	  1.081f,	1.059f,	  1.043f,	1.031f,	  6.276f,	6.371f,	  6.556f,	6.814f,	  7.176f,	7.752f,	  8.503f,	9.271f,	  10.289f,	11.975f,	  13.680f,	14.951f,	  14.054f,	11.926f,
	10.689f,	  9.241f,	6.603f,	  5.419f,	4.506f,	  3.317f,	2.834f,	  2.167f,	1.651f,	  1.432f,	1.299f,	  1.211f,	1.150f,	  1.108f,	1.078f,	  1.056f,	1.041f,	  1.030f,	6.060f,
	6.136f,	  6.296f,	6.520f,	  6.816f,	7.249f,	  7.789f,	8.339f,	  9.209f,	10.873f,	  12.611f,	13.914f,	  13.204f,	11.343f,	  10.267f,	8.931f,	  6.407f,	5.283f,	  4.409f,	3.250f,
	2.786f,	  2.134f,	1.629f,	  1.416f,	1.288f,	  1.203f,	1.144f,	  1.104f,	1.075f,	  1.054f,	1.039f,	  1.029f,	5.761f,	  5.876f,	6.039f,	  6.238f,	6.475f,	  6.789f,	7.162f,
	7.540f,	  8.284f,	9.899f,	  11.628f,	12.928f,	  12.384f,	10.778f,	  9.853f,	8.626f,	  6.214f,	5.149f,	  4.313f,	3.185f,	  2.738f,	2.101f,	  1.607f,	1.401f,	  1.277f,	1.195f,
	1.139f,	  1.099f,	1.072f,	  1.052f,	1.038f,	  1.027f,	5.547f,	  5.648f,	5.779f,	  5.933f,	6.108f,	  6.327f,	6.579f,	  6.836f,	7.482f,	  9.031f,	10.716f,	  11.987f,	11.589f,
	10.226f,	  9.446f,	8.323f,	  6.024f,	5.017f,	  4.219f,	3.121f,	  2.691f,	2.069f,	  1.586f,	1.386f,	  1.266f,	1.187f,	  1.133f,	1.095f,	  1.069f,	1.050f,	  1.036f,	1.026f,
	5.324f,	  5.372f,	5.453f,	  5.559f,	5.684f,	  5.839f,	6.017f,	  6.203f,	6.780f,	  8.251f,	9.866f,	  11.086f,	10.819f,	  9.686f,	9.044f,	  8.021f,	5.835f,	  4.886f,	4.124f,
	3.057f,	  2.644f,	2.037f,	  1.565f,	1.371f,	  1.255f,	1.179f,	  1.128f,	1.091f,	  1.066f,	1.048f,	  1.035f,	1.025f,	  4.984f,	5.021f,	  5.081f,	5.157f,	  5.249f,	5.367f,
	5.505f,	  5.653f,	6.179f,	  7.559f,	9.080f,	  10.231f,	10.078f,	  9.162f,	8.648f,	  7.722f,	5.649f,	  4.756f,	4.031f,	  2.994f,	2.597f,	  2.005f,	1.544f,	  1.357f,	1.245f,
	1.172f,	  1.122f,	1.088f,	  1.063f,	1.046f,	  1.033f,	1.024f,	  4.751f,	4.783f,	  4.821f,	4.864f,	  4.918f,	5.001f,	  5.106f,	5.227f,	  5.701f,	6.970f,	  8.371f,	9.433f,
	9.376f,	  8.660f,	8.264f,	  7.428f,	5.468f,	  4.630f,	3.939f,	  2.932f,	2.552f,	  1.975f,	1.524f,	  1.343f,	1.235f,	  1.165f,	1.117f,	  1.084f,	1.060f,	  1.044f,	1.032f,
	1.023f,	  4.501f,	4.537f,	  4.567f,	4.594f,	  4.630f,	4.694f,	  4.782f,	4.886f,	  5.311f,	6.457f,	  7.724f,	8.685f,	  8.709f,	8.177f,	  7.888f,	7.139f,	  5.289f,	4.505f,
	3.849f,	  2.872f,	2.507f,	  1.945f,	1.505f,	  1.330f,	1.226f,	  1.158f,	1.112f,	  1.080f,	1.058f,	  1.042f,	1.030f,	  1.022f,	4.227f,	  4.257f,	4.285f,	  4.313f,	4.350f,
	4.413f,	  4.498f,	4.596f,	  4.979f,	5.998f,	  7.124f,	7.978f,	  8.072f,	7.709f,	  7.519f,	6.851f,	  5.113f,	4.381f,	  3.759f,	2.812f,	  2.462f,	1.915f,	  1.486f,	1.317f,
	1.217f,	  1.152f,	1.108f,	  1.077f,	1.055f,	  1.040f,	1.029f,	  1.021f,	4.078f,	  4.073f,	4.078f,	  4.093f,	4.122f,	  4.180f,	4.261f,	  4.355f,	4.695f,	  5.587f,	6.569f,
	7.315f,	  7.467f,	7.259f,	  7.158f,	6.567f,	  4.939f,	4.259f,	  3.669f,	2.752f,	  2.418f,	1.885f,	  1.468f,	1.304f,	  1.208f,	1.145f,	  1.103f,	1.074f,	  1.053f,	1.038f,
	1.028f,	  1.020f,	3.808f,	  3.816f,	3.827f,	  3.840f,	3.867f,	  3.927f,	4.013f,	  4.114f,	4.420f,	  5.194f,	6.040f,	  6.684f,	6.887f,	  6.820f,	6.800f,	  6.283f,	4.764f,
	4.136f,	  3.579f,	2.692f,	  2.373f,	1.855f,	  1.449f,	1.291f,	  1.199f,	1.139f,	  1.098f,	1.070f,	  1.051f,	1.037f,	  1.026f,	1.019f,	  3.659f,	3.645f,	  3.630f,	3.618f,
	3.625f,	  3.676f,	3.767f,	  3.875f,	4.153f,	  4.818f,	5.539f,	  6.086f,	6.336f,	  6.394f,	6.448f,	  6.000f,	4.591f,	  4.013f,	3.488f,	  2.632f,	2.327f,	  1.825f,	1.431f,
	1.279f,	  1.190f,	1.132f,	  1.094f,	1.067f,	  1.048f,	1.035f,	  1.025f,	1.018f,	  3.332f,	3.334f,	  3.325f,	3.310f,	  3.314f,	3.373f,	  3.481f,	3.607f,	  3.870f,	4.441f,
	5.052f,	  5.516f,	5.807f,	  5.979f,	6.099f,	  5.717f,	4.418f,	  3.890f,	3.397f,	  2.571f,	2.281f,	  1.795f,	1.412f,	  1.266f,	1.181f,	  1.126f,	1.089f,	  1.064f,	1.046f,
	1.033f,	  1.024f,	1.017f,	  3.182f,	3.163f,	  3.132f,	3.096f,	  3.083f,	3.135f,	  3.246f,	3.378f,	  3.617f,	4.098f,	  4.606f,	4.991f,	  5.315f,	5.584f,	  5.760f,	5.440f,
	4.247f,	  3.768f,	3.306f,	  2.511f,	2.236f,	  1.765f,	1.394f,	  1.254f,	1.172f,	  1.120f,	1.085f,	  1.060f,	1.043f,	  1.031f,	1.023f,	  1.017f,	2.948f,	  2.945f,	2.924f,
	2.892f,	  2.880f,	2.932f,	  3.043f,	3.174f,	  3.386f,	3.783f,	  4.196f,	4.510f,	  4.859f,	5.209f,	  5.432f,	5.168f,	  4.080f,	3.647f,	  3.216f,	2.451f,	  2.190f,	1.736f,
	1.377f,	  1.241f,	1.164f,	  1.114f,	1.080f,	  1.057f,	1.041f,	  1.030f,	1.022f,	  1.016f,	2.815f,	  2.803f,	2.779f,	  2.749f,	2.739f,	  2.787f,	2.888f,	  3.007f,	3.186f,
	3.502f,	  3.827f,	4.075f,	  4.440f,	4.856f,	  5.116f,	4.902f,	  3.917f,	3.529f,	  3.127f,	2.392f,	  2.145f,	1.706f,	  1.359f,	1.230f,	  1.155f,	1.108f,	  1.076f,	1.054f,
	1.039f,	  1.028f,	1.020f,	  1.015f,	2.605f,	  2.601f,	2.589f,	  2.574f,	2.578f,	  2.630f,	2.727f,	  2.838f,	2.988f,	  3.235f,	3.484f,	  3.675f,	4.052f,	  4.519f,	4.808f,
	4.641f,	  3.756f,	3.411f,	  3.037f,	2.334f,	  2.100f,	1.677f,	  1.342f,	1.218f,	  1.147f,	1.102f,	  1.072f,	1.051f,	  1.037f,	1.027f,	  1.019f,	1.014f,	  2.465f,	2.452f,
	2.436f,	  2.420f,	2.423f,	  2.472f,	2.564f,	  2.668f,	2.795f,	  2.983f,	3.168f,	  3.310f,	3.694f,	  4.200f,	4.510f,	  4.386f,	3.598f,	  3.295f,	2.949f,	  2.275f,	2.055f,
	1.648f,	  1.325f,	1.207f,	  1.140f,	1.097f,	  1.068f,	1.049f,	  1.035f,	1.025f,	  1.018f,	1.013f,	  2.273f,	2.270f,	  2.257f,	2.238f,	  2.237f,	2.285f,	  2.377f,	2.481f,
	2.595f,	  2.738f,	2.872f,	  2.974f,	3.361f,	  3.895f,	4.221f,	  4.135f,	3.442f,	  3.179f,	2.860f,	  2.217f,	2.009f,	  1.619f,	1.309f,	  1.196f,	1.132f,	  1.091f,	1.064f,
	1.046f,	  1.033f,	1.024f,	  1.017f,	1.012f,	  2.108f,	2.104f,	  2.086f,	2.061f,	  2.054f,	2.099f,	  2.192f,	2.298f,	  2.403f,	2.509f,	  2.600f,	2.671f,	  3.056f,	3.607f,
	3.942f,	  3.891f,	3.289f,	  3.065f,	2.772f,	  2.158f,	1.964f,	  1.590f,	1.292f,	  1.185f,	1.124f,	  1.086f,	1.060f,	  1.043f,	1.031f,	  1.022f,	1.016f,	  1.012f,	1.952f,
	1.947f,	  1.929f,	1.904f,	  1.896f,	1.940f,	  2.032f,	2.137f,	  2.230f,	2.303f,	  2.358f,	2.401f,	  2.780f,	3.337f,	  3.675f,	3.655f,	  3.140f,	2.952f,	  2.684f,	2.101f,
	1.918f,	  1.561f,	1.276f,	  1.174f,	1.117f,	  1.081f,	1.057f,	  1.040f,	1.029f,	  1.021f,	1.015f,	  1.011f,	1.802f,	  1.800f,	1.788f,	  1.771f,	1.771f,	  1.815f,	1.902f,
	1.999f,	  2.077f,	2.120f,	  2.143f,	2.162f,	  2.530f,	3.085f,	  3.420f,	3.427f,	  2.995f,	2.842f,	  2.598f,	2.044f,	  1.873f,	1.532f,	  1.260f,	1.164f,	  1.110f,	1.076f,
	1.053f,	  1.038f,	1.027f,	  1.019f,	1.014f,	  1.010f,	1.670f,	  1.666f,	1.657f,	  1.648f,	1.654f,	  1.699f,	1.778f,	  1.865f,	1.929f,	  1.948f,	1.947f,	  1.947f,	2.302f,
	2.847f,	  3.176f,	3.207f,	  2.854f,	2.733f,	  2.512f,	1.987f,	  1.828f,	1.504f,	  1.245f,	1.153f,	  1.103f,	1.071f,	  1.050f,	1.035f,	  1.025f,	1.018f,	  1.013f,	1.010f,
	1.520f,	  1.523f,	1.521f,	  1.517f,	1.526f,	  1.569f,	1.642f,	  1.722f,	1.775f,	  1.779f,	1.762f,	  1.750f,	2.090f,	  2.621f,	2.941f,	  2.993f,	2.716f,	  2.625f,	2.426f,
	1.930f,	  1.783f,	1.475f,	  1.229f,	1.143f,	  1.096f,	1.066f,	  1.046f,	1.033f,	  1.023f,	1.017f,	  1.012f,	1.009f,	  1.406f,	1.406f,	  1.401f,	1.394f,	  1.398f,	1.436f,
	1.503f,	  1.576f,	1.622f,	  1.616f,	1.589f,	  1.570f,	1.896f,	  2.407f,	2.716f,	  2.787f,	2.580f,	  2.518f,	2.341f,	  1.874f,	1.738f,	  1.447f,	1.214f,	  1.133f,	1.089f,
	1.061f,	  1.043f,	1.030f,	  1.022f,	1.016f,	  1.011f,	1.008f,	  1.287f,	1.289f,	  1.285f,	1.279f,	  1.282f,	1.315f,	  1.376f,	1.442f,	  1.481f,	1.467f,	  1.434f,	1.410f,
	1.719f,	  2.208f,	2.503f,	  2.590f,	2.449f,	  2.414f,	2.257f,	  1.818f,	1.693f,	  1.419f,	1.199f,	  1.123f,	1.082f,	  1.056f,	1.039f,	  1.028f,	1.020f,	  1.014f,	1.010f,
	1.008f,	  1.187f,	1.189f,	  1.187f,	1.183f,	  1.189f,	1.219f,	  1.272f,	1.329f,	  1.360f,	1.338f,	  1.299f,	1.270f,	  1.560f,	2.023f,	  2.302f,	2.402f,	  2.323f,	2.312f,
	2.174f,	  1.763f,	1.648f,	  1.391f,	1.184f,	  1.114f,	1.076f,	  1.052f,	1.036f,	  1.026f,	1.018f,	  1.013f,	1.010f,	  1.007f,	1.083f,	  1.080f,	1.080f,	  1.082f,	1.093f,
	1.124f,	  1.173f,	1.224f,	  1.249f,	1.222f,	  1.179f,	1.147f,	  1.417f,	1.851f,	  2.112f,	2.224f,	  2.201f,	2.212f,	  2.092f,	1.709f,	  1.603f,	1.363f,	  1.170f,	1.105f,
	1.069f,	  1.047f,	1.033f,	  1.023f,	1.017f,	  1.012f,	1.009f,	  1.006f,	0.983f,	  0.987f,	0.990f,	  0.994f,	1.005f,	  1.034f,	1.080f,	  1.127f,	1.149f,	  1.119f,	1.072f,
	1.038f,	  1.288f,	1.692f,	  1.934f,	2.055f,	  2.084f,	2.115f,	  2.011f,	1.655f,	  1.559f,	1.336f,	  1.156f,	1.096f,	  1.063f,	1.043f,	  1.030f,	1.021f,	  1.015f,	1.011f,
	1.008f,	  1.006f,	0.883f,	  0.886f,	0.890f,	  0.896f,	0.909f,	  0.939f,	0.984f,	  1.031f,	1.053f,	  1.022f,	0.975f,	  0.941f,	1.170f,	  1.543f,	1.767f,	  1.896f,	1.970f,
	2.020f,	  1.932f,	1.602f,	  1.515f,	1.309f,	  1.142f,	1.087f,	  1.057f,	1.039f,	  1.027f,	1.019f,	  1.014f,	1.010f,	  1.007f,	1.005f,	  0.802f,	0.807f,	  0.813f,	0.820f,
	0.834f,	  0.863f,	0.906f,	  0.950f,	0.969f,	  0.937f,	0.890f,	  0.855f,	1.064f,	  1.406f,	1.612f,	  1.746f,	1.862f,	  1.927f,	1.854f,	  1.550f,	1.472f,	  1.282f,	1.129f,
	1.078f,	  1.051f,	1.035f,	  1.024f,	1.017f,	  1.012f,	1.009f,	  1.006f,	1.005f,	  0.717f,	0.725f,	  0.734f,	0.745f,	  0.762f,	0.791f,	  0.832f,	0.872f,	  0.889f,	0.858f,
	0.812f,	  0.778f,	0.968f,	  1.279f,	1.467f,	  1.605f,	1.758f,	  1.836f,	1.778f,	  1.499f,	1.429f,	  1.256f,	1.116f,	  1.070f,	1.046f,	  1.031f,	1.022f,	  1.015f,	1.011f,
	1.008f,	  1.006f,	1.004f,	  0.646f,	0.654f,	  0.664f,	0.676f,	  0.693f,	0.722f,	  0.759f,	0.796f,	  0.811f,	0.782f,	  0.739f,	0.708f,	  0.880f,	1.162f,	  1.332f,	1.473f,
	1.658f,	  1.748f,	1.702f,	  1.448f,	1.386f,	  1.230f,	1.103f,	  1.062f,	1.040f,	  1.027f,	1.019f,	  1.013f,	1.010f,	  1.007f,	1.005f,	  1.004f,	0.580f,	  0.591f,	0.602f,
	0.614f,	  0.631f,	0.656f,	  0.690f,	0.724f,	  0.737f,	0.711f,	  0.672f,	0.644f,	  0.799f,	1.054f,	  1.207f,	1.350f,	  1.562f,	1.663f,	  1.629f,	1.398f,	  1.344f,	1.204f,
	1.090f,	  1.054f,	1.035f,	  1.024f,	1.016f,	  1.012f,	1.008f,	  1.006f,	1.004f,	  1.003f,	0.518f,	  0.527f,	0.539f,	  0.553f,	0.570f,	  0.595f,	0.627f,	  0.658f,	0.670f,
	0.647f,	  0.613f,	0.587f,	  0.726f,	0.955f,	  1.092f,	1.236f,	  1.471f,	1.580f,	  1.557f,	1.350f,	  1.303f,	1.179f,	  1.078f,	1.046f,	  1.030f,	1.020f,	  1.014f,	1.010f,
	1.007f,	  1.005f,	1.004f,	  1.003f,	0.460f,	  0.472f,	0.486f,	  0.502f,	0.521f,	  0.545f,	0.575f,	  0.603f,	0.614f,	  0.592f,	0.561f,	  0.538f,	0.661f,	  0.865f,	0.988f,
	1.131f,	  1.384f,	1.500f,	  1.487f,	1.302f,	  1.262f,	1.154f,	  1.066f,	1.039f,	  1.025f,	1.017f,	  1.012f,	1.008f,	  1.006f,	1.004f,	  1.003f,	1.002f,	  0.412f,	0.423f,
	0.437f,	  0.452f,	0.471f,	  0.495f,	0.523f,	  0.550f,	0.561f,	  0.542f,	0.514f,	  0.492f,	0.602f,	  0.783f,	0.892f,	  1.034f,	1.302f,	  1.423f,	1.419f,	  1.255f,	1.222f,
	1.129f,	  1.054f,	1.032f,	  1.020f,	1.013f,	  1.009f,	1.006f,	  1.005f,	1.003f,	  1.002f,	1.002f,	  0.371f,	0.380f,	  0.392f,	0.407f,	  0.424f,	0.447f,	  0.474f,	0.501f,
	0.512f,	  0.496f,	0.470f,	  0.451f,	0.548f,	  0.708f,	0.805f,	  0.945f,	1.224f,	  1.349f,	1.353f,	  1.210f,	1.183f,	  1.106f,	1.043f,	  1.025f,	1.016f,	  1.010f,	1.007f,
	1.005f,	  1.003f,	1.002f,	  1.002f,	1.001f,	  0.328f,	0.338f,	  0.351f,	0.367f,	  0.384f,	0.406f,	  0.433f,	0.458f,	  0.469f,	0.454f,	  0.431f,	0.413f,	  0.499f,	0.641f,
	0.726f,	  0.863f,	1.150f,	  1.277f,	1.289f,	  1.165f,	1.144f,	  1.082f,	1.032f,	  1.018f,	1.011f,	  1.007f,	1.005f,	  1.003f,	1.002f,	  1.002f,	1.001f,	  1.001f,	0.289f,
	0.299f,	  0.313f,	0.330f,	  0.348f,	0.370f,	  0.395f,	0.418f,	  0.429f,	0.415f,	  0.394f,	0.379f,	  0.454f,	0.579f,	  0.654f,	0.789f,	  1.080f,	1.208f,	  1.227f,	1.122f,
	1.106f,	  1.060f,	1.022f,	  1.011f,	1.007f,	  1.004f,	1.003f,	  1.002f,	1.001f,	  1.001f,	1.001f,	  1.000f,	0.253f,	  0.265f,	0.280f,	  0.296f,	0.315f,	  0.336f,	0.359f,
	0.380f,	  0.390f,	0.378f,	  0.360f,	0.346f,	  0.413f,	0.523f,	  0.589f,	0.720f,	  1.014f,	1.142f,	  1.167f,	1.081f,	  1.070f,	1.038f,	  1.012f,	1.005f,	  1.003f,	1.001f,
	1.001f,	  1.000f,	1.000f,	  1.000f,	1.000f,	  1.000f,	0.224f,	  0.235f,	0.249f,	  0.264f,	0.281f,	  0.302f,	0.324f,	  0.344f,	0.354f,	  0.344f,	0.328f,	  0.316f,	0.375f,
	0.472f,	  0.531f,	0.658f,	  0.951f,	1.079f,	  1.109f,	1.040f,	  1.034f,	1.016f,	  1.002f,	0.999f,	  0.999f,	0.999f,	  0.999f,	0.999f,	  0.999f,	0.999f,	  1.000f,	1.000f,
	0.198f,	  0.209f,	0.222f,	  0.237f,	0.254f,	  0.273f,	0.294f,	  0.313f,	0.323f,	  0.314f,	0.300f,	  0.290f,	0.341f,	  0.427f,	0.479f,	  0.602f,	0.893f,	  1.018f,	1.053f,
	1.001f,	  1.000f,	0.995f,	  0.992f,	0.993f,	  0.995f,	0.996f,	  0.997f,	0.998f,	  0.998f,	0.999f,	  0.999f,	0.999f,	  0.175f,	0.185f,	  0.198f,	0.213f,	  0.230f,	0.249f,
	0.269f,	  0.287f,	0.296f,	  0.288f,	0.275f,	  0.266f,	0.311f,	  0.387f,	0.432f,	  0.551f,	0.837f,	  0.961f,	1.000f,	  0.964f,	0.967f,	  0.976f,	0.983f,	  0.988f,	0.991f,
	0.994f,	  0.995f,	0.997f,	  0.998f,	0.998f,	  0.999f,	0.999f,	  0.154f,	0.164f,	  0.177f,	0.192f,	  0.208f,	0.226f,	  0.245f,	0.262f,	  0.270f,	0.264f,	  0.252f,	0.244f,
	0.284f,	  0.351f,	0.391f,	  0.505f,	0.785f,	  0.906f,	0.949f,	  0.928f,	0.935f,	  0.956f,	0.975f,	  0.983f,	0.988f,	  0.991f,	0.994f,	  0.995f,	0.997f,	  0.998f,	0.998f,
	0.999f,	  0.136f,	0.145f,	  0.157f,	0.171f,	  0.186f,	0.203f,	  0.221f,	0.238f,	  0.246f,	0.241f,	  0.231f,	0.223f,	  0.259f,	0.318f,	  0.354f,	0.463f,	  0.736f,	0.853f,
	0.900f,	  0.893f,	0.904f,	  0.938f,	0.966f,	  0.978f,	0.984f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.997f,	0.998f,	  0.998f,	0.119f,	  0.128f,	0.140f,	  0.153f,	0.167f,
	0.183f,	  0.201f,	0.217f,	  0.225f,	0.221f,	  0.212f,	0.205f,	  0.237f,	0.289f,	  0.320f,	0.426f,	  0.690f,	0.804f,	  0.853f,	0.860f,	  0.875f,	0.920f,	  0.959f,	0.973f,
	0.981f,	  0.987f,	0.991f,	  0.993f,	0.995f,	  0.997f,	0.997f,	  0.998f,	0.104f,	  0.113f,	0.124f,	  0.137f,	0.151f,	  0.166f,	0.183f,	  0.199f,	0.207f,	  0.203f,	0.195f,
	0.189f,	  0.217f,	0.263f,	  0.291f,	0.391f,	  0.647f,	0.757f,	  0.809f,	0.829f,	  0.847f,	0.904f,	  0.951f,	0.968f,	  0.978f,	0.985f,	  0.989f,	0.992f,	  0.994f,	0.996f,
	0.997f,	  0.998f,	0.090f,	  0.099f,	0.110f,	  0.122f,	0.135f,	  0.150f,	0.167f,	  0.181f,	0.190f,	  0.187f,	0.180f,	  0.174f,	0.199f,	  0.240f,	0.265f,	  0.360f,	0.607f,
	0.713f,	  0.767f,	0.800f,	  0.820f,	0.888f,	  0.944f,	0.964f,	  0.976f,	0.983f,	  0.988f,	0.991f,	  0.994f,	0.996f,	  0.997f,	0.998f,	  0.078f,	0.086f,	  0.097f,	0.108f,
	0.121f,	  0.135f,	0.150f,	  0.165f,	0.173f,	  0.171f,	0.165f,	  0.160f,	0.182f,	  0.219f,	0.241f,	  0.332f,	0.569f,	  0.671f,	0.728f,	  0.772f,	0.796f,	  0.873f,	0.937f,
	0.960f,	  0.973f,	0.981f,	  0.987f,	0.991f,	  0.993f,	0.995f,	  0.996f,	0.997f,	  0.067f,	0.075f,	  0.085f,	0.096f,	  0.108f,	0.121f,	  0.136f,	0.149f,	  0.157f,	0.156f,
	0.151f,	  0.148f,	0.167f,	  0.200f,	0.220f,	  0.307f,	0.534f,	  0.631f,	0.690f,	  0.746f,	0.772f,	  0.859f,	0.931f,	  0.956f,	0.971f,	  0.980f,	0.986f,	  0.990f,	0.993f,
	0.995f,	  0.996f,	0.997f,	  0.059f,	0.067f,	  0.077f,	0.087f,	  0.098f,	0.110f,	  0.124f,	0.137f,	  0.145f,	0.144f,	  0.140f,	0.137f,	  0.155f,	0.184f,	  0.201f,	0.284f,
	0.502f,	  0.594f,	0.655f,	  0.722f,	0.751f,	  0.846f,	0.926f,	  0.953f,	0.968f,	  0.978f,	0.985f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.997f,	0.052f,	  0.060f,	0.068f,
	0.078f,	  0.088f,	0.101f,	  0.114f,	0.127f,	  0.135f,	0.134f,	  0.131f,	0.128f,	  0.144f,	0.169f,	  0.184f,	0.263f,	  0.471f,	0.560f,	  0.623f,	0.699f,	  0.731f,	0.835f,
	0.920f,	  0.950f,	0.966f,	  0.977f,	0.984f,	  0.988f,	0.992f,	  0.994f,	0.996f,	  0.997f,	0.046f,	  0.053f,	0.062f,	  0.070f,	0.080f,	  0.092f,	0.106f,	  0.119f,	0.127f,
	0.126f,	  0.123f,	0.120f,	  0.134f,	0.156f,	  0.170f,	0.244f,	  0.443f,	0.528f,	  0.593f,	0.679f,	  0.713f,	0.824f,	  0.915f,	0.947f,	  0.964f,	0.975f,	  0.983f,	0.988f,
	0.991f,	  0.994f,	0.995f,	  0.997f,	0.040f,	  0.047f,	0.054f,	  0.062f,	0.072f,	  0.083f,	0.097f,	  0.110f,	0.118f,	  0.118f,	0.115f,	  0.113f,	0.125f,	  0.145f,	0.157f,
	0.227f,	  0.417f,	0.498f,	  0.564f,	0.660f,	  0.696f,	0.814f,	  0.911f,	0.944f,	  0.963f,	0.974f,	  0.982f,	0.987f,	  0.991f,	0.993f,	  0.995f,	0.996f,	  0.036f,	0.042f,
	0.049f,	  0.057f,	0.066f,	  0.077f,	0.089f,	  0.101f,	0.109f,	  0.110f,	0.108f,	  0.106f,	0.117f,	  0.134f,	0.145f,	  0.212f,	0.394f,	  0.471f,	0.538f,	  0.643f,	0.682f,
	0.805f,	  0.907f,	0.942f,	  0.961f,	0.973f,	  0.981f,	0.987f,	  0.990f,	0.993f,	  0.995f,	0.996f,	  0.033f,	0.038f,	  0.045f,	0.052f,	  0.061f,	0.071f,	  0.083f,	0.094f,
	0.101f,	  0.102f,	0.101f,	  0.099f,	0.109f,	  0.124f,	0.134f,	  0.198f,	0.371f,	  0.445f,	0.514f,	  0.628f,	0.669f,	  0.797f,	0.904f,	  0.940f,	0.960f,	  0.972f,	0.980f,
	0.986f,	  0.990f,	0.993f,	  0.995f,	0.996f,	  0.031f,	0.036f,	  0.042f,	0.049f,	  0.056f,	0.066f,	  0.077f,	0.088f,	  0.095f,	0.096f,	  0.095f,	0.093f,	  0.102f,	0.116f,
	0.124f,	  0.185f,	0.351f,	  0.422f,	0.493f,	  0.615f,	0.658f,	  0.791f,	0.901f,	  0.938f,	0.958f,	  0.971f,	0.980f,	  0.986f,	0.990f,	  0.993f,	0.995f,	  0.996f,	0.029f,
	0.033f,	  0.039f,	0.045f,	  0.052f,	0.062f,	  0.073f,	0.084f,	  0.091f,	0.092f,	  0.091f,	0.089f,	  0.096f,	0.109f,	  0.116f,	0.174f,	  0.333f,	0.400f,	  0.473f,	0.603f,
	0.648f,	  0.785f,	0.898f,	  0.936f,	0.957f,	  0.971f,	0.979f,	  0.985f,	0.989f,	  0.992f,	0.995f,	  0.996f,	0.028f,	  0.032f,	0.037f,	  0.043f,	0.050f,	  0.060f,	0.071f,
	0.082f,	  0.089f,	0.090f,	  0.087f,	0.085f,	  0.092f,	0.102f,	  0.109f,	0.164f,	  0.317f,	0.381f,	  0.455f,	0.594f,	  0.641f,	0.781f,	  0.896f,	0.935f,	  0.956f,	0.970f,
	0.979f,	  0.985f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.027f,	  0.030f,	0.035f,	  0.041f,	0.048f,	  0.058f,	0.069f,	  0.079f,	0.086f,	  0.086f,	0.084f,	  0.082f,	0.087f,
	0.097f,	  0.103f,	0.156f,	  0.302f,	0.364f,	  0.439f,	0.586f,	  0.635f,	0.777f,	  0.894f,	0.934f,	  0.956f,	0.969f,	  0.979f,	0.985f,	  0.989f,	0.992f,	  0.994f,	0.996f,
	0.025f,	  0.029f,	0.033f,	  0.039f,	0.046f,	  0.055f,	0.066f,	  0.076f,	0.082f,	  0.082f,	0.080f,	  0.078f,	0.083f,	  0.092f,	0.097f,	  0.148f,	0.288f,	  0.348f,	0.425f,
	0.580f,	  0.631f,	0.775f,	  0.893f,	0.933f,	  0.955f,	0.969f,	  0.978f,	0.985f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.023f,	0.027f,	  0.032f,	0.037f,	  0.044f,	0.053f,
	0.063f,	  0.073f,	0.079f,	  0.080f,	0.077f,	  0.076f,	0.080f,	  0.088f,	0.092f,	  0.141f,	0.276f,	  0.334f,	0.413f,	  0.575f,	0.629f,	  0.773f,	0.892f,	  0.932f,	0.955f,
	0.969f,	  0.978f,	0.984f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.023f,	0.026f,	  0.031f,	0.037f,	  0.044f,	0.052f,	  0.063f,	0.073f,	  0.079f,	0.079f,	  0.076f,	0.074f,
	0.078f,	  0.085f,	0.089f,	  0.135f,	0.266f,	  0.321f,	0.402f,	  0.573f,	0.628f,	  0.773f,	0.891f,	  0.932f,	0.954f,	  0.969f,	0.978f,	  0.984f,	0.989f,	  0.992f,	0.994f,
	0.996f,	  0.023f,	0.026f,	  0.031f,	0.037f,	  0.043f,	0.052f,	  0.062f,	0.072f,	  0.078f,	0.078f,	  0.076f,	0.074f,	  0.076f,	0.082f,	  0.086f,	0.131f,	  0.257f,	0.310f,
	0.393f,	  0.572f,	0.629f,	  0.774f,	0.891f,	  0.932f,	0.954f,	  0.968f,	0.978f,	  0.984f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.024f,	  0.027f,	0.032f,	  0.037f,	0.042f,
	0.051f,	  0.061f,	0.071f,	  0.077f,	0.077f,	  0.075f,	0.072f,	  0.075f,	0.080f,	  0.083f,	0.126f,	  0.249f,	0.300f,	  0.385f,	0.572f,	  0.632f,	0.775f,	  0.891f,	0.932f,
	0.954f,	  0.968f,	0.978f,	  0.984f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.024f,	  0.028f,	0.033f,	  0.037f,	0.043f,	  0.051f,	0.061f,	  0.071f,	0.077f,	  0.077f,	0.074f,
	0.071f,	  0.073f,	0.078f,	  0.081f,	0.123f,	  0.242f,	0.292f,	  0.379f,	0.574f,	  0.636f,	0.777f,	  0.892f,	0.932f,	  0.954f,	0.968f,	  0.978f,	0.984f,	  0.989f,	0.992f,
	0.994f,	  0.996f,	0.027f,	  0.031f,	0.035f,	  0.039f,	0.045f,	  0.052f,	0.062f,	  0.072f,	0.078f,	  0.078f,	0.074f,	  0.071f,	0.073f,	  0.077f,	0.079f,	  0.120f,	0.236f,
	0.285f,	  0.374f,	0.577f,	  0.641f,	0.781f,	  0.893f,	0.932f,	  0.955f,	0.969f,	  0.978f,	0.984f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.033f,	0.036f,	  0.039f,	0.042f,
	0.047f,	  0.055f,	0.065f,	  0.075f,	0.081f,	  0.079f,	0.075f,	  0.071f,	0.072f,	  0.076f,	0.078f,	  0.117f,	0.231f,	  0.279f,	0.371f,	  0.581f,	0.648f,	  0.784f,	0.894f,
	0.933f,	  0.955f,	0.969f,	  0.978f,	0.984f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.040f,	0.041f,	  0.044f,	0.046f,	  0.051f,	0.058f,	  0.068f,	0.078f,	  0.084f,	0.081f,
	0.076f,	  0.072f,	0.072f,	  0.075f,	0.076f,	  0.115f,	0.227f,	  0.274f,	0.368f,	  0.587f,	0.656f,	  0.789f,	0.895f,	  0.934f,	0.955f,	  0.969f,	0.978f,	  0.984f,	0.989f,
	0.992f,	  0.994f,	0.996f,	  0.046f,	0.047f,	  0.048f,	0.051f,	  0.055f,	0.061f,	  0.071f,	0.080f,	  0.086f,	0.083f,	  0.077f,	0.072f,	  0.072f,	0.074f,	  0.076f,	0.114f,
	0.224f,	  0.270f,	0.367f,	  0.594f,	0.665f,	  0.794f,	0.897f,	  0.935f,	0.956f,	  0.969f,	0.978f,	  0.985f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.049f,	  0.049f,	0.051f,
	0.053f,	  0.057f,	0.064f,	  0.074f,	0.083f,	  0.088f,	0.085f,	  0.078f,	0.073f,	  0.073f,	0.074f,	  0.075f,	0.113f,	  0.221f,	0.267f,	  0.366f,	0.601f,	  0.675f,	0.800f,
	0.899f,	  0.936f,	0.957f,	  0.970f,	0.979f,	  0.985f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.044f,	  0.046f,	0.049f,	  0.054f,	0.059f,	  0.068f,	0.078f,	  0.088f,	0.093f,
	0.089f,	  0.082f,	0.076f,	  0.075f,	0.075f,	  0.075f,	0.112f,	  0.219f,	0.264f,	  0.366f,	0.610f,	  0.686f,	0.807f,	  0.901f,	0.937f,	  0.957f,	0.970f,	  0.979f,	0.985f,
	0.989f,	  0.992f,	0.994f,	  0.996f,	0.041f,	  0.045f,	0.050f,	  0.057f,	0.065f,	  0.074f,	0.085f,	  0.096f,	0.101f,	  0.096f,	0.087f,	  0.081f,	0.078f,	  0.077f,	0.076f,
	0.112f,	  0.218f,	0.263f,	  0.367f,	0.620f,	  0.698f,	0.814f,	  0.904f,	0.938f,	  0.958f,	0.971f,	  0.979f,	0.985f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.044f,	0.050f,
	0.056f,	  0.064f,	0.073f,	  0.083f,	0.094f,	  0.104f,	0.109f,	  0.103f,	0.093f,	  0.085f,	0.081f,	  0.079f,	0.077f,	  0.113f,	0.218f,	  0.262f,	0.369f,	  0.630f,	0.710f,
	0.821f,	  0.906f,	0.939f,	  0.959f,	0.971f,	  0.980f,	0.986f,	  0.990f,	0.992f,	  0.995f,	0.996f,	  0.057f,	0.064f,	  0.071f,	0.080f,	  0.088f,	0.098f,	  0.108f,	0.117f,
	0.120f,	  0.112f,	0.100f,	  0.091f,	0.085f,	  0.081f,	0.079f,	  0.114f,	0.218f,	  0.262f,	0.372f,	  0.640f,	0.723f,	  0.829f,	0.909f,	  0.941f,	0.960f,	  0.972f,	0.980f,
	0.986f,	  0.990f,	0.993f,	  0.995f,	0.996f,	  0.083f,	0.090f,	  0.097f,	0.103f,	  0.110f,	0.118f,	  0.126f,	0.134f,	  0.135f,	0.124f,	  0.109f,	0.098f,	  0.090f,	0.085f,
	0.081f,	  0.115f,	0.219f,	  0.263f,	0.375f,	  0.652f,	0.737f,	  0.837f,	0.912f,	  0.943f,	0.961f,	  0.973f,	0.981f,	  0.986f,	0.990f,	  0.993f,	0.995f,	  0.996f,	0.133f,
	0.135f,	  0.138f,	0.142f,	  0.145f,	0.150f,	  0.154f,	0.158f,	  0.155f,	0.140f,	  0.121f,	0.107f,	  0.097f,	0.089f,	  0.084f,	0.117f,	  0.221f,	0.264f,	  0.379f,	0.663f,
	0.751f,	  0.845f,	0.915f,	  0.944f,	0.962f,	  0.973f,	0.981f,	  0.986f,	0.990f,	  0.993f,	0.995f,	  0.996f,	0.203f,	  0.197f,	0.193f,	  0.191f,	0.189f,	  0.188f,	0.187f,
	0.185f,	  0.178f,	0.157f,	  0.134f,	0.117f,	  0.104f,	0.095f,	  0.088f,	0.120f,	  0.224f,	0.266f,	  0.383f,	0.675f,	  0.765f,	0.853f,	  0.918f,	0.946f,	  0.963f,	0.974f,
	0.982f,	  0.987f,	0.990f,	  0.993f,	0.995f,	  0.996f,	0.258f,	  0.247f,	0.238f,	  0.231f,	0.225f,	  0.220f,	0.214f,	  0.208f,	0.197f,	  0.173f,	0.146f,	  0.126f,	0.111f,
	0.100f,	  0.092f,	0.124f,	  0.226f,	0.268f,	  0.388f,	0.687f,	  0.779f,	0.862f,	  0.921f,	0.948f,	  0.964f,	0.975f,	  0.982f,	0.987f,	  0.991f,	0.993f,	  0.995f,	0.996f,
	0.262f,	  0.256f,	0.251f,	  0.247f,	0.243f,	  0.238f,	0.232f,	  0.225f,	0.213f,	  0.186f,	0.157f,	  0.135f,	0.118f,	  0.105f,	0.096f,	  0.127f,	0.229f,	  0.270f,	0.392f,
	0.699f,	  0.793f,	0.870f,	  0.924f,	0.949f,	  0.965f,	0.975f,	  0.982f,	0.987f,	  0.991f,	0.993f,	  0.995f,	0.997f,	  0.217f,	0.223f,	  0.229f,	0.234f,	  0.237f,	0.238f,
	0.236f,	  0.232f,	0.221f,	  0.194f,	0.164f,	  0.141f,	0.123f,	  0.109f,	0.100f,	  0.130f,	0.231f,	  0.273f,	0.397f,	  0.711f,	0.806f,	  0.878f,	0.927f,	  0.951f,	0.966f,
	0.976f,	  0.983f,	0.988f,	  0.991f,	0.994f,	  0.995f,	0.997f,	  0.172f,	0.185f,	  0.198f,	0.212f,	  0.223f,	0.230f,	  0.233f,	0.233f,	  0.225f,	0.198f,	  0.168f,	0.146f,
	0.127f,	  0.113f,	0.103f,	  0.133f,	0.234f,	  0.275f,	0.401f,	  0.722f,	0.820f,	  0.886f,	0.930f,	  0.952f,	0.967f,	  0.977f,	0.983f,	  0.988f,	0.991f,	  0.994f,	0.995f,
	0.997f,	  0.151f,	0.162f,	  0.176f,	0.192f,	  0.206f,	0.217f,	  0.224f,	0.228f,	  0.223f,	0.198f,	  0.170f,	0.148f,	  0.130f,	0.115f,	  0.106f,	0.136f,	  0.236f,	0.277f,
	0.405f,	  0.733f,	0.832f,	  0.893f,	0.932f,	  0.954f,	0.968f,	  0.977f,	0.984f,	  0.988f,	0.992f,	  0.994f,	0.996f,	  0.997f,	0.152f,	  0.157f,	0.167f,	  0.180f,	0.194f,
	0.207f,	  0.217f,	0.226f,	  0.223f,	0.200f,	  0.171f,	0.149f,	  0.132f,	0.118f,	  0.109f,	0.139f,	  0.238f,	0.279f,	  0.410f,	0.743f,	  0.844f,	0.900f,	  0.935f,	0.955f,
	0.969f,	  0.978f,	0.984f,	  0.989f,	0.992f,	  0.994f,	0.996f,	  0.997f,	0.146f,	  0.151f,	0.159f,	  0.171f,	0.184f,	  0.199f,	0.215f,	  0.229f,	0.230f,	  0.206f,	0.175f,
	0.151f,	  0.134f,	0.121f,	  0.113f,	0.143f,	  0.241f,	0.281f,	  0.414f,	0.753f,	  0.856f,	0.907f,	  0.937f,	0.957f,	  0.970f,	0.978f,	  0.984f,	0.989f,	  0.992f,	0.994f,
	0.996f,	  0.997f,	0.140f,	  0.148f,	0.158f,	  0.169f,	0.183f,	  0.201f,	0.223f,	  0.244f,	0.249f,	  0.221f,	0.184f,	  0.156f,	0.137f,	  0.126f,	0.118f,	  0.147f,	0.244f,
	0.284f,	  0.418f,	0.762f,	  0.866f,	0.913f,	  0.940f,	0.958f,	  0.970f,	0.979f,	  0.985f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.997f,	  0.138f,	0.148f,	  0.160f,	0.173f,
	0.188f,	  0.211f,	0.239f,	  0.267f,	0.274f,	  0.241f,	0.197f,	  0.163f,	0.142f,	  0.131f,	0.123f,	  0.152f,	0.248f,	  0.287f,	0.422f,	  0.771f,	0.876f,	  0.919f,	0.942f,
	0.959f,	  0.971f,	0.979f,	  0.985f,	0.989f,	  0.992f,	0.994f,	  0.996f,	0.997f,	  0.140f,	0.153f,	  0.170f,	0.190f,	  0.212f,	0.238f,	  0.267f,	0.294f,	  0.299f,	0.260f,
	0.209f,	  0.170f,	0.147f,	  0.137f,	0.129f,	  0.157f,	0.252f,	  0.290f,	0.426f,	  0.778f,	0.885f,	  0.924f,	0.944f,	  0.960f,	0.972f,	  0.980f,	0.986f,	  0.990f,	0.992f,
	0.995f,	  0.996f,	0.997f,	  0.143f,	0.162f,	  0.190f,	0.225f,	  0.258f,	0.285f,	  0.305f,	0.318f,	  0.311f,	0.266f,	  0.213f,	0.172f,	  0.150f,	0.140f,	  0.133f,	0.161f,
	0.255f,	  0.292f,	0.429f,	  0.785f,	0.892f,	  0.929f,	0.945f,	  0.961f,	0.972f,	  0.980f,	0.986f,	  0.990f,	0.993f,	  0.995f,	0.996f,	  0.997f,	0.156f,	  0.183f,	0.221f,
	0.267f,	  0.307f,	0.328f,	  0.330f,	0.322f,	  0.301f,	0.254f,	  0.205f,	0.168f,	  0.149f,	0.141f,	  0.135f,	0.164f,	  0.256f,	0.294f,	  0.432f,	0.790f,	  0.898f,	0.932f,
	0.947f,	  0.962f,	0.973f,	  0.981f,	0.986f,	  0.990f,	0.993f,	  0.995f,	0.996f,	  0.997f,	0.195f,	  0.240f,	0.283f,	  0.326f,	0.356f,	  0.363f,	0.348f,	  0.323f,	0.292f,
	0.245f,	  0.200f,	0.167f,	  0.150f,	0.143f,	  0.138f,	0.166f,	  0.258f,	0.295f,	  0.433f,	0.794f,	  0.903f,	0.935f,	  0.948f,	0.962f,	  0.973f,	0.981f,	  0.986f,	0.990f,
	0.993f,	  0.995f,	0.996f,	  0.997f,	0.313f,	  0.346f,	0.378f,	  0.409f,	0.429f,	  0.425f,	0.399f,	  0.365f,	0.325f,	  0.271f,	0.221f,	  0.183f,	0.162f,	  0.152f,	0.145f,
	0.172f,	  0.261f,	0.298f,	  0.436f,	0.798f,	  0.906f,	0.937f,	  0.949f,	0.963f,	  0.974f,	0.981f,	  0.986f,	0.990f,	  0.993f,	0.995f,	  0.996f,	0.997f,	  0.468f,	0.486f,
	0.507f,	  0.529f,	0.540f,	  0.527f,	0.490f,	  0.445f,	0.393f,	  0.325f,	0.260f,	  0.211f,	0.183f,	  0.167f,	0.156f,	  0.179f,	0.267f,	  0.302f,	0.439f,	  0.801f,	0.910f,
	0.939f,	  0.950f,	0.964f,	  0.974f,	0.981f,	  0.987f,	0.990f,	  0.993f,	0.995f,	  0.996f,	0.997f,


};