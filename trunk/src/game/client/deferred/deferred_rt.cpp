
#include "cbase.h"
#include "deferred/deferred_shared_common.h"

#include "materialsystem/itexture.h"

static CTextureReference g_tex_Normals;
static CTextureReference g_tex_Depth;
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
static CTextureReference g_tex_LightCtrl;
#endif
static CTextureReference g_tex_Lightaccum;

static CTextureReference g_tex_VolumePrepass;
static CTextureReference g_tex_VolumetricsBuffer[ 2 ];

static CTextureReference g_tex_ShadowColor_Ortho[ MAX_SHADOW_ORTHO ];
static CTextureReference g_tex_ShadowDepth_Ortho[ MAX_SHADOW_ORTHO ];
static CTextureReference g_tex_ShadowRad_Albedo_Ortho[ MAX_SHADOW_ORTHO ];
static CTextureReference g_tex_ShadowRad_Normal_Ortho[ MAX_SHADOW_ORTHO ];
static CTextureReference g_tex_ShadowColor_Proj[ MAX_SHADOW_PROJ ];
static CTextureReference g_tex_ShadowDepth_Proj[ MAX_SHADOW_PROJ ];
static CTextureReference g_tex_ShadowColor_DP[ MAX_SHADOW_DP ];
static CTextureReference g_tex_ShadowDepth_DP[ MAX_SHADOW_DP ];

static CTextureReference g_tex_RadiosityBuffer[ 2 ];
static CTextureReference g_tex_RadiosityNormal[ 2 ];

static CTextureReference g_tex_ProjectableVGUI[ NUM_PROJECTABLE_VGUI ];

static float g_flDepthScalar = 65536.0f;

float GetDepthMapDepthResolution( float zDelta )
{
	return zDelta / g_flDepthScalar;
}

void DefRTsOnModeChanged()
{
	InitDeferredRTs();
}

void InitDeferredRTs( bool bInitial )
{
	if ( !bInitial )
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly(); // HAHAHAHA. No.

	int screen_w, screen_h;
	int dummy = 128;

	materials->GetBackBufferDimensions( screen_w, screen_h );

	const ImageFormat fmt_gbuffer0 =
#if DEFCFG_LIGHTCTRL_PACKING
		IMAGE_FORMAT_RGBA8888;
#else
		IMAGE_FORMAT_RGB888;

	const ImageFormat fmt_gbuffer2 = IMAGE_FORMAT_RGBA8888;
#endif
	const ImageFormat fmt_gbuffer1 = IMAGE_FORMAT_R32F;
	const ImageFormat fmt_lightAccum =
#if DEFCFG_LIGHTACCUM_COMPRESSED
		IMAGE_FORMAT_RGBA8888;
#else
		IMAGE_FORMAT_RGBA16161616F;
#endif
	const ImageFormat fmt_volumAccum = IMAGE_FORMAT_RGB888;
	const ImageFormat fmt_projVGUI = IMAGE_FORMAT_RGB888;

	const bool bShadowUseColor = 
#ifdef SHADOWMAPPING_USE_COLOR
		true;
#else
		false;
#endif

	const ImageFormat fmt_depth = GetDeferredManager()->GetShadowDepthFormat();
	const ImageFormat fmt_depthColor = bShadowUseColor ? IMAGE_FORMAT_R32F
		: g_pMaterialSystemHardwareConfig->GetNullTextureFormat();
	const ImageFormat fmt_radAlbedo = IMAGE_FORMAT_RGB888;
	const ImageFormat fmt_radNormal = IMAGE_FORMAT_RGB888;
	const ImageFormat fmt_radBuffer = IMAGE_FORMAT_RGB888;

	if ( fmt_depth == IMAGE_FORMAT_D16_SHADOW )
		g_flDepthScalar = pow( 2.0, 16 );
	else if ( fmt_depth == IMAGE_FORMAT_D24X8_SHADOW )
		g_flDepthScalar = pow( 2.0, 24 );

	AssertMsg( fmt_depth == IMAGE_FORMAT_D16_SHADOW || fmt_depth == IMAGE_FORMAT_D24X8_SHADOW, "Unexpected depth format" );

	unsigned int gbufferFlags =			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int lightAccumFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int volumAccumFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int depthFlags =			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int shadowColorFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int projVGUIFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int radAlbedoNormalFlags =	TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;
	unsigned int radBufferFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET;
	unsigned int radNormalFlags =		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_RENDERTARGET | TEXTUREFLAGS_POINTSAMPLE;

	materials->BeginRenderTargetAllocation();

	shadowData_general_t generalShadowData;

	if ( bInitial )
	{
		g_tex_Normals.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER0,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_gbuffer0,
			MATERIAL_RT_DEPTH_SHARED,
			gbufferFlags, 0 ) );

		g_tex_Depth.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER1,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_gbuffer1,
			MATERIAL_RT_DEPTH_NONE,
			gbufferFlags, 0 ) );

#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
		g_tex_LightCtrl.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_GBUFFER2,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_gbuffer2,
			MATERIAL_RT_DEPTH_NONE,
			gbufferFlags, 0 ) );
#endif

		g_tex_Lightaccum.Init( materials->CreateNamedRenderTargetTextureEx2( DEFRTNAME_LIGHTACCUM,
			dummy, dummy,
			RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP,
			fmt_lightAccum,
			MATERIAL_RT_DEPTH_NONE,
			lightAccumFlags, 0 ) );

		for ( int i = 0; i < 2; i++ )
			g_tex_VolumetricsBuffer[ i ].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_VOLUMACCUM, i ),
				dummy, dummy,
				RT_SIZE_HDR,
				fmt_volumAccum,
				MATERIAL_RT_DEPTH_NONE,
				volumAccumFlags, 0 ) );

		g_tex_VolumePrepass.Init( materials->CreateNamedRenderTargetTextureEx2(
			DEFRTNAME_VOLUMPREPASS,
			dummy, dummy,
			RT_SIZE_HDR,
			fmt_gbuffer1,
			MATERIAL_RT_DEPTH_NONE,
			gbufferFlags, 0 ) );

		for ( int i = 0; i < MAX_SHADOW_ORTHO; i++ )
		{
#if CSM_USE_COMPOSITED_TARGET
			int iResolution_x = CSM_COMP_RES_X;
			int iResolution_y = CSM_COMP_RES_Y;
#else
			const cascade_t &c = GetCascadeInfo( i );
			int iResolution_x = c.iResolution;
			int iResolution_y = c.iResolution;
#endif

			g_tex_ShadowDepth_Ortho[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_ORTHO, i ),
				iResolution_x, iResolution_y,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );

			g_tex_ShadowColor_Ortho[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_ORTHO, i ),
				iResolution_x, iResolution_y,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );

#if DEFCFG_ENABLE_RADIOSITY
			g_tex_ShadowRad_Albedo_Ortho[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWRAD_ALBEDO_ORTHO, i ),
				iResolution_x, iResolution_y,
				RT_SIZE_NO_CHANGE,
				fmt_radAlbedo,
				MATERIAL_RT_DEPTH_NONE,
				radAlbedoNormalFlags, 0 ) );

			g_tex_ShadowRad_Normal_Ortho[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWRAD_NORMAL_ORTHO, i ),
				iResolution_x, iResolution_y,
				RT_SIZE_NO_CHANGE,
				fmt_radNormal,
				MATERIAL_RT_DEPTH_NONE,
				radAlbedoNormalFlags, 0 ) );
#endif

			Assert( iResolution_y == g_tex_ShadowDepth_Ortho[i]->GetActualHeight() );
			Assert( iResolution_y == g_tex_ShadowColor_Ortho[i]->GetActualHeight() );
			Assert( iResolution_x == g_tex_ShadowDepth_Ortho[i]->GetActualWidth() );
			Assert( iResolution_x == g_tex_ShadowColor_Ortho[i]->GetActualWidth() );
		}

		for ( int i = 0; i < NUM_PROJECTABLE_VGUI; i++ )
		{
			g_tex_ProjectableVGUI[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_PROJECTABLE_VGUI, i ),
				PROJECTABLE_VGUI_RES, PROJECTABLE_VGUI_RES,
				RT_SIZE_NO_CHANGE,
				fmt_projVGUI,
				MATERIAL_RT_DEPTH_NONE,
				projVGUIFlags, 0 ) );
		}

#if DEFCFG_ENABLE_RADIOSITY
		AssertMsg( sqrt( (double)(RADIOSITY_BUFFER_SAMPLES * RADIOSITY_BUFFER_SAMPLES * RADIOSITY_BUFFER_SAMPLES) ) == RADIOSITY_BUFFER_RES,
			"Sample algorithm relies on count and size to be correct..." );
		AssertMsg( sqrt( (double)RADIOSITY_BUFFER_SAMPLES ) == RADIOSITY_BUFFER_GRIDS_PER_AXIS,
			"Sample algorithm relies on grid layout to be defined based on sample count..." );

		for ( int i = 0; i < 2; i++ )
		{
			g_tex_RadiosityBuffer[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_RADIOSITY_BUFFER, i ),
				RADIOSITY_BUFFER_RES, RADIOSITY_BUFFER_RES,
				RT_SIZE_NO_CHANGE,
				fmt_radBuffer,
				MATERIAL_RT_DEPTH_NONE,
				radBufferFlags, 0 ) );

			g_tex_RadiosityNormal[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_RADIOSITY_NORMAL, i ),
				RADIOSITY_BUFFER_RES, RADIOSITY_BUFFER_RES,
				RT_SIZE_NO_CHANGE,
				fmt_radNormal,
				MATERIAL_RT_DEPTH_NONE,
				radNormalFlags, 0 ) );
		}
#endif
	}

	for ( int i = 0; i < MAX_SHADOW_PROJ; i++ )
	{
		int res = GetShadowResolution_Spot();
		generalShadowData.iPROJ_Res = res;
		bool bFirst = i == 0;

		if ( !bShadowUseColor || bFirst )
			g_tex_ShadowDepth_Proj[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_PROJ, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );
		else
			g_tex_ShadowDepth_Proj[i].Init( g_tex_ShadowDepth_Proj[0] );

		if ( bShadowUseColor || bFirst )
			g_tex_ShadowColor_Proj[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_PROJ, i ),
				res, res,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );
		else
			g_tex_ShadowColor_Proj[i].Init( g_tex_ShadowColor_Proj[0] );

		Assert( res == g_tex_ShadowDepth_Proj[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowColor_Proj[i]->GetActualHeight() );
		Assert( res == g_tex_ShadowDepth_Proj[i]->GetActualWidth() );
		Assert( res == g_tex_ShadowColor_Proj[i]->GetActualWidth() );
	}

	for ( int i = 0; i < MAX_SHADOW_DP; i++ )
	{
		int res_x = GetShadowResolution_Point();
		int res_y = res_x * 2;

		generalShadowData.iDPSM_Res_x = res_x;
		generalShadowData.iDPSM_Res_y = res_y;

		bool bFirst = i == 0;

		if ( !bShadowUseColor || bFirst )
			g_tex_ShadowDepth_DP[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWDEPTH_DP, i ),
				res_x, res_y,
				RT_SIZE_NO_CHANGE,
				fmt_depth,
				MATERIAL_RT_DEPTH_NONE,
				depthFlags, 0 ) );
		else
			g_tex_ShadowDepth_DP[i].Init( g_tex_ShadowDepth_DP[0] );

		if ( bShadowUseColor || bFirst )
			g_tex_ShadowColor_DP[i].Init( materials->CreateNamedRenderTargetTextureEx2(
				VarArgs( "%s%02i", DEFRTNAME_SHADOWCOLOR_DP, i ),
				res_x, res_y,
				RT_SIZE_NO_CHANGE,
				fmt_depthColor,
				MATERIAL_RT_DEPTH_NONE,
				shadowColorFlags, 0 ) );
		else
			g_tex_ShadowColor_DP[i].Init( g_tex_ShadowColor_DP[0] );

		Assert( res_y == g_tex_ShadowDepth_DP[i]->GetActualHeight() );
		Assert( res_y == g_tex_ShadowColor_DP[i]->GetActualHeight() );
		Assert( res_x == g_tex_ShadowDepth_DP[i]->GetActualWidth() );
		Assert( res_x == g_tex_ShadowColor_DP[i]->GetActualWidth() );
	}

	materials->EndRenderTargetAllocation();

	GetDeferredExt()->CommitTexture_General( g_tex_Normals, g_tex_Depth,
#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
		g_tex_LightCtrl,
#endif
		g_tex_Lightaccum );

	for ( int i = 0; i < MAX_SHADOW_ORTHO; i++ )
		GetDeferredExt()->CommitTexture_CascadedDepth( i,
			bShadowUseColor ? g_tex_ShadowColor_Ortho[i] : g_tex_ShadowDepth_Ortho[i]
		);

	for ( int i = 0; i < MAX_SHADOW_DP; i++ )
		GetDeferredExt()->CommitTexture_DualParaboloidDepth( i,
			bShadowUseColor ? g_tex_ShadowColor_DP[i] : g_tex_ShadowDepth_DP[i]
		);

	for ( int i = 0; i < MAX_SHADOW_PROJ; i++ )
		GetDeferredExt()->CommitTexture_ProjectedDepth( i,
			bShadowUseColor ? g_tex_ShadowColor_Proj[i] : g_tex_ShadowDepth_Proj[i]
		);

	GetDeferredExt()->CommitTexture_VolumePrePass( g_tex_VolumePrepass );

	GetDeferredExt()->CommitShadowData_General( generalShadowData );

#if DEFCFG_ENABLE_RADIOSITY
	AssertMsg( MAX_SHADOW_ORTHO == 1, "You gotta fix the commit func now.." );
	GetDeferredExt()->CommitTexture_ShadowRadOutput_Ortho( g_tex_ShadowRad_Albedo_Ortho[0],
		g_tex_ShadowRad_Normal_Ortho[0] );
	GetDeferredExt()->CommitTexture_Radiosity( g_tex_RadiosityBuffer[0], g_tex_RadiosityBuffer[1],
		g_tex_RadiosityNormal[0], g_tex_RadiosityNormal[1] );
#endif
}

int GetShadowResolution_Spot()
{
	return deferred_rt_shadowspot_res.GetInt();
}

int GetShadowResolution_Point()
{
	return deferred_rt_shadowpoint_res.GetInt();
}

ITexture *GetDefRT_Normals()
{
	return g_tex_Normals;
}

ITexture *GetDefRT_Depth()
{
	return g_tex_Depth;
}

#if ( DEFCFG_LIGHTCTRL_PACKING == 0 )
ITexture *GetDefRT_LightCtrl()
{
	return g_tex_LightCtrl;
}
#endif

ITexture *GetDefRT_Lightaccum()
{
	return g_tex_Lightaccum;
}

ITexture *GetDefRT_VolumePrepass()
{
	return g_tex_VolumePrepass;
}

ITexture *GetDefRT_VolumetricsBuffer( int index )
{
	return g_tex_VolumetricsBuffer[ index ];
}

ITexture *GetDefRT_RadiosityBuffer( int index )
{
	Assert( index >= 0 && index < 2 );
	return g_tex_RadiosityBuffer[ index ];
}

ITexture *GetDefRT_RadiosityNormal( int index )
{
	Assert( index >= 0 && index < 2 );
	return g_tex_RadiosityNormal[ index ];
}

ITexture *GetShadowColorRT_Ortho( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_ORTHO );
	return g_tex_ShadowColor_Ortho[ index ];
}
ITexture *GetShadowDepthRT_Ortho( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_ORTHO );
	return g_tex_ShadowDepth_Ortho[ index ];
}

ITexture *GetShadowColorRT_Proj( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	return g_tex_ShadowColor_Proj[ index ];
}
ITexture *GetShadowDepthRT_Proj( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	return g_tex_ShadowDepth_Proj[ index ];
}

ITexture *GetShadowColorRT_DP( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	return g_tex_ShadowColor_DP[ index ];
}
ITexture *GetShadowDepthRT_DP( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	return g_tex_ShadowDepth_DP[ index ];
}
ITexture *GetProjectableVguiRT( int index )
{
	Assert( index >= 0 && index < NUM_PROJECTABLE_VGUI );
	return g_tex_ProjectableVGUI[ index ];
}

ITexture *GetRadiosityAlbedoRT_Ortho( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	return g_tex_ShadowRad_Albedo_Ortho[ index ];
}
ITexture *GetRadiosityNormalRT_Ortho( int index )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	return g_tex_ShadowRad_Normal_Ortho[ index ];
}
