// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
#include "client.h"

#define   TEX_CACHE_MAX_OBJECT   8000000
#define   TEX_CACHE_MAX_WORLD      8000000
#define   TEX_CACHE_MAX_LMAP      8000000
#define   TEX_CACHE_MAX_WALLMASK   2000000
#define   TEX_CACHE_MAX_EFFECT   2000000
#define   TEX_CACHE_MAX_PARTICLE   1000000
#define FOV_H               ((gD3DRect.right - gD3DRect.left) / (float)(MAXX * stretchfactor) * (-PI / 3.6f))
#define FOV_V               ((gD3DRect.bottom - gD3DRect.top) / (float)(MAXY * stretchfactor) * (PI / 6.0f))
#define Z_RANGE               (400000.0f)

d3d_render_packet_new   *gpPacket;

LPDIRECT3D9            gpD3D = NULL;
LPDIRECT3DDEVICE9      gpD3DDevice = NULL;

// Matrices we can precalculate
D3DMATRIX mPlayerHeading; // Player's heading rotated Y.
D3DMATRIX mPlayerPitch; // Player's pitch rotated X.
D3DMATRIX mPlayerPitchFudge; // Player's pitch rotated X, slightly larger for objects?
D3DMATRIX mPlayerHeadingTrans; // Player's heading rotated Y and transposed.
D3DMATRIX mPlayerViewTranslate; // Player's view translated.
D3DMATRIX mPlayerHPMul; // Player's heading and pitch rotated and multiplied.
D3DMATRIX mPlayerHPFudgeMul; // Player's heading and larger pitch rotated and multiplied.
D3DMATRIX mIdentity; // Identity matrix.

// temp dynamic lightmaps
LPDIRECT3DTEXTURE9      gpDLightAmbient = NULL;
LPDIRECT3DTEXTURE9      gpDLightWhite = NULL;
LPDIRECT3DTEXTURE9      gpDLightOrange = NULL;
LPDIRECT3DTEXTURE9      gpNoLookThrough = NULL;
LPDIRECT3DTEXTURE9      gpBackBufferTex[16];
LPDIRECT3DTEXTURE9      gpBackBufferTexFull;
LPDIRECT3DTEXTURE9      gpViewElements[NUM_VIEW_ELEMENTS];

LPDIRECT3DTEXTURE9      gpSkyboxTextures[5][6];

D3DVIEWPORT9         gViewport;
D3DCAPS9            gD3DCaps;

d3d_render_cache      gObjectCache;
d3d_render_cache      gWorldCache;
d3d_render_cache      gWorldCacheStatic;
d3d_render_cache      gLMapCacheStatic;

d3d_render_cache_system   gObjectCacheSystem;
d3d_render_cache_system   gWorldCacheSystem;
d3d_render_cache_system   gLMapCacheSystem;
d3d_render_cache_system   gWorldCacheSystemStatic;
d3d_render_cache_system   gLMapCacheSystemStatic;
d3d_render_cache_system   gWallMaskCacheSystem;
d3d_render_cache_system   gEffectCacheSystem;
d3d_render_cache_system   gParticleCacheSystem;

extern d_light_cache         gDLightCache;
extern d_light_cache         gDLightCacheDynamic;

d3d_render_pool_new      gObjectPool;
d3d_render_pool_new      gWorldPool;
d3d_render_pool_new      gLMapPool;
d3d_render_pool_new      gWorldPoolStatic;
d3d_render_pool_new      gLMapPoolStatic;
d3d_render_pool_new      gWallMaskPool;
d3d_render_pool_new      gEffectPool;
d3d_render_pool_new      gParticlePool;

extern particle_system         gParticleSystemSand;
extern particle_system         gParticleSystemRain;
extern particle_system         gParticleSystemSnow;
extern particle_system         gParticleSystemFireworks;

custom_xyz            playerOldPos;
custom_xyz            playerDeltaPos;

font_3d               gFont;

RECT               gD3DRect;
int                  gD3DEnabled;
BYTE               gViewerLight = 0;
int                  gNumObjects;
int                  gNumVertices;
int                  gNumDPCalls;

unsigned int         gFrame = 0;
int                  gScreenWidth;
int                  gScreenHeight;
int                  gCurBackground;

D3DVERTEXELEMENT9      decl0[] = {
   {0, 0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
   {1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
   D3DDECL_END()
   };

D3DVERTEXELEMENT9      decl1[] = {
   {0, 0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
   {1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
   {2, 0, D3DDECLTYPE_FLOAT2,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
   D3DDECL_END()
   };

D3DVERTEXELEMENT9      decl2[] = {
   {0, 0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
   {1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
   {2, 0, D3DDECLTYPE_FLOAT2,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
   {3, 0, D3DDECLTYPE_FLOAT2,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
   D3DDECL_END()
   };

LPDIRECT3DVERTEXDECLARATION9 decl0dc;
LPDIRECT3DVERTEXDECLARATION9 decl1dc;
LPDIRECT3DVERTEXDECLARATION9 decl2dc;

AREA               gD3DView;
Bool               gD3DRedrawAll = FALSE;
int                  gTemp = 0;
Bool               gWireframe;      // this is really bad, I'm sorry

extern player_info      player;
extern long            viewer_height;
extern Color         base_palette[];
extern PDIB            background;         /* Pointer to background bitmap */
extern ObjectRange      visible_objects[];    /* Where objects are on screen */
extern int            num_visible_objects;
extern Draw3DParams      *p;
extern room_type      current_room;
extern long            shade_amount;
extern int            sector_depths[];
extern d3d_driver_profile   gD3DDriverProfile;
extern BYTE            *gBits;
extern BYTE            *gBufferBits;
extern D3DPRESENT_PARAMETERS   gPresentParam;
extern Vector3D         sun_vect;
extern long            shade_amount;
extern long            stretchfactor;
extern BYTE            light_rows[MAXY/2+1];      // Strength of light as function of screen row
extern ViewElement      ViewElements[];
extern HDC            gBitsDC;

#define SKYBOX_DIMENSIONS   (75000.0f)

float gSkyboxXYZ[] =
{
   // back
   SKYBOX_DIMENSIONS, 37000.0f, -SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, -37000.0f, -SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, -37000.0f, -SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, 37000.0f, -SKYBOX_DIMENSIONS,

   // bottom
   -SKYBOX_DIMENSIONS, -37000.0f, SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, -37000.0f, -SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, -37000.0f, -SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, -37000.0f, SKYBOX_DIMENSIONS,

   // front
   -SKYBOX_DIMENSIONS, 37000.0f, SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, -37000.0f, SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, -37000.0f, SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, 37000.0f, SKYBOX_DIMENSIONS,

   // left
   -SKYBOX_DIMENSIONS, 37000.0f, -SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, -37000.0f, -SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, -37000.0f, SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, 37000.0f, SKYBOX_DIMENSIONS,

   // right
   SKYBOX_DIMENSIONS, 37000.0f, SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, -37000.0f, SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, -37000.0f, -SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, 37000.0f, -SKYBOX_DIMENSIONS,

   // top
   -SKYBOX_DIMENSIONS, 37000.0f, -SKYBOX_DIMENSIONS,
   -SKYBOX_DIMENSIONS, 37000.0f, SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, 37000.0f, SKYBOX_DIMENSIONS,
   SKYBOX_DIMENSIONS, 37000.0f, -SKYBOX_DIMENSIONS,
};

float gSkyboxST[] =
{
   0.001f, 0.001f,
   0.001f, 0.999f,
   0.999f, 0.999f,
   0.999f, 0.001f,

   0.001f, 0.001f,
   0.001f, 0.999f,
   0.999f, 0.999f,
   0.999f, 0.001f,

   0.001f, 0.001f,
   0.001f, 0.999f,
   0.999f, 0.999f,
   0.999f, 0.001f,

   0.001f, 0.001f,
   0.001f, 0.999f,
   0.999f, 0.999f,
   0.999f, 0.001f,

   0.001f, 0.001f,
   0.001f, 0.999f,
   0.999f, 0.999f,
   0.999f, 0.001f,

   0.001f, 0.001f,
   0.001f, 0.999f,
   0.999f, 0.999f,
   0.999f, 0.001f,
};

unsigned char gSkyboxBGRA[] =
{
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,

   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,

   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,

   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,

   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,

   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
   192, 192, 192, 255,
};

void            D3DRenderBackgroundsLoad(char *pFilename, int index);
void            D3DRenderWorldDraw(room_type *room, Draw3DParams *params);
void            D3DRenderNamesDraw3D(d3d_render_cache_system *pCacheSystem, d3d_render_pool_new *pPool,
                  room_type *room, Draw3DParams *params, font_3d *pFont);
void            D3DRenderQuestInfoDraw3D(d3d_render_cache_system *pCacheSystem, d3d_render_pool_new *pPool,
                  room_type *room, Draw3DParams *params, font_3d *pFont);
void            D3DRenderLMapsBuild(void);
void            D3DRenderFontInit(font_3d *pFont, HFONT hFont);
void            D3DRenderSkyboxDraw(d3d_render_pool_new *pPool);
void            D3DRenderBackgroundObjectsDraw(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params);
void            D3DGetBackgroundOverlayPosition(BackgroundOverlay *pOverlay, Draw3DParams *params, Pnt3D *bObj);
Bool            D3DComputePlayerOverlayArea(PDIB pdib, char hotspot, AREA *obj_area);

// new render stuff
void               GeometryUpdate(d3d_render_pool_new *pPool);
void               D3DRenderPoolInit(d3d_render_pool_new *pPool, int size, int packetSize);
void               D3DRenderPoolShutdown(d3d_render_pool_new *pPool);
void               D3DRenderPoolReset(d3d_render_pool_new *pPool, void *pMaterialFunc);
d3d_render_packet_new   *D3DRenderPacketNew(d3d_render_pool_new *pPool);
void               D3DRenderPacketInit(d3d_render_packet_new *pPacket);
d3d_render_chunk_new   *D3DRenderChunkNew(d3d_render_packet_new *pPacket);
void               D3DRenderChunkInit(d3d_render_chunk_new *pChunk);
void               D3DRenderObjectsDraw(d3d_render_pool_new *pPool, room_type *room,
                     Draw3DParams *params, BYTE flags, Bool drawTransparent);
void               D3DRenderOverlaysDraw(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params,
                     BOOL underlays, BYTE flags, Bool drawTransparent);
void               D3DRenderProjectilesDrawNew(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params);
void               D3DRenderPlayerOverlaysDraw(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params);
void               D3DRenderPlayerOverlayOverlaysDraw(d3d_render_pool_new *pPool, list_type overlays,
                     PDIB pDib, room_type *room, Draw3DParams *params, AREA *objArea, BOOL underlays);
void               D3DRenderViewElementsDraw(d3d_render_pool_new *pPool);

void               D3DRenderPacketFloorAdd(BSPnode *pNode, d3d_render_pool_new *pPool, Bool dynamic);
void               D3DRenderPacketCeilingAdd(BSPnode *pNode, d3d_render_pool_new *pPool, Bool dynamic);
void               D3DRenderPacketWallAdd(WallData *pWall, d3d_render_pool_new *pPool, unsigned int type, int side,
                                      Bool dynamic);
void               D3DRenderPacketWallMaskAdd(WallData *pWall, d3d_render_pool_new *pPool, unsigned int type, int side,
                                      Bool dynamic);
void               D3DRenderFloorMaskAdd(BSPnode *pNode, d3d_render_pool_new *pPool,
                                   Bool bDynamic);
void               D3DRenderCeilingMaskAdd(BSPnode *pNode, d3d_render_pool_new *pPool,
                                    Bool bDynamic);

void               D3DGeometryBuildNew(room_type *room);
void               D3DGeometryBuildNewStaticLMaps(room_type *room);
void               D3DPostOverlayEffects(d3d_render_pool_new *pPool);
LPDIRECT3DTEXTURE9 D3DRenderFramebufferTextureCreate(LPDIRECT3DTEXTURE9   pTex0,
                                        LPDIRECT3DTEXTURE9   pTex1,
                                        float width, float height);
void               *D3DRenderMalloc(unsigned int bytes);

// externed stuff
extern int         FindHotspotPdib(PDIB pdib, char hotspot, POINT *point);
extern Bool        ComputePlayerOverlayArea(PDIB pdib, char hotspot, AREA *obj_area);
extern void        UpdateRoom3D(room_type *room, Draw3DParams *params);

inline void *D3DRenderMalloc(unsigned int bytes)
{
   return malloc(bytes);
}

/************************************************************************************
*
*  Initializes Direct3d
*  - Creates D3D object, D3DDevice object, sets up buffers
*
************************************************************************************/
HRESULT D3DRenderInit(HWND hWnd)
{
   D3DDISPLAYMODE   displayMode;      
   HRESULT         hr;
   int            i;
   
   // try to initialize Direct3D9
   if (NULL == (gpD3D = Direct3DCreate9(D3D_SDK_VERSION)))
      return E_FAIL;

   // create D3D9 device and other preparations
   gD3DEnabled = D3DDriverProfileInit();

   /*modeCount = gpD3D->lpVtbl->GetAdapterModeCount(gpD3D, D3DADAPTER_DEFAULT);
   for (i = 0; i < modeCount; i++)
   {
      if (FAILED(gpD3D->lpVtbl->EnumAdapterModes(gpD3D, D3DADAPTER_DEFAULT, i,
         &displayMode2[i])))
      {
      error = E_FAIL;
      assert(0);
      }
   }*/

   if (!gD3DEnabled)
      return FALSE;

   hr = IDirect3D9_GetAdapterDisplayMode(gpD3D, D3DADAPTER_DEFAULT, &displayMode);
   hr = IDirect3DDevice9_GetDeviceCaps(gpD3DDevice, &gD3DCaps);

   /***************************************************************************/
   /*                              VIEWPORT                                   */
   /***************************************************************************/
   
   // define viewport
   gViewport.X         = 0;
   gViewport.Y         = 0;
   gViewport.Width      = gScreenWidth;
   gViewport.Height   = gScreenHeight;
   gViewport.MinZ      = 0.0f;
   gViewport.MaxZ      = 1.0f;

   // set viewport
   hr = IDirect3DDevice9_SetViewport(gpD3DDevice, &gViewport);

   /***************************************************************************/
   /*                       INITIAL RENDERSTATE                               */
   /***************************************************************************/
   
   // Set AA mode
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_MULTISAMPLEANTIALIAS, config.aaMode > 0);
   
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_NONE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_LIGHTING, FALSE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CLIPPING, FALSE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_TRUE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, TRUE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DITHERENABLE, FALSE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_STENCILENABLE, FALSE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAREF, TEMP_ALPHA_REF);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_LASTPIXEL, TRUE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_SOLID);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_COLORWRITEENABLE,
      D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

   //   DX_FUNC_CALL(gpD3DDevice->lpVtbl->SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_WIREFRAME), gD3DError);

   D3DCacheSystemInit(&gLMapCacheSystem);
   D3DCacheSystemInit(&gLMapCacheSystemStatic);
   D3DRenderPoolInit(&gLMapPool, POOL_SIZE, PACKET_SIZE);
   D3DRenderPoolInit(&gLMapPoolStatic, POOL_SIZE, PACKET_SIZE);

   D3DTexCacheInit();

   D3DCacheSystemInit(&gObjectCacheSystem);
   D3DCacheSystemInit(&gWorldCacheSystem);
   D3DCacheSystemInit(&gWorldCacheSystemStatic);
   D3DCacheSystemInit(&gWallMaskCacheSystem);
   D3DCacheSystemInit(&gEffectCacheSystem);
   D3DCacheSystemInit(&gParticleCacheSystem);

   D3DRenderPoolInit(&gObjectPool, POOL_SIZE, PACKET_SIZE);
   D3DRenderPoolInit(&gWorldPool, POOL_SIZE / 2, PACKET_SIZE);
   D3DRenderPoolInit(&gWorldPoolStatic, POOL_SIZE, PACKET_SIZE);
   D3DRenderPoolInit(&gWallMaskPool, POOL_SIZE / 2, PACKET_SIZE);
   D3DRenderPoolInit(&gEffectPool, POOL_SIZE / 8, PACKET_SIZE);
   D3DRenderPoolInit(&gParticlePool, POOL_SIZE, PACKET_SIZE);

   gWorldPool.pMaterialFctn = &D3DMaterialWorldPool;
   gWorldPoolStatic.pMaterialFctn = &D3DMaterialWorldPool;
   gLMapPool.pMaterialFctn = &D3DMaterialLMapDynamicPool;
   gObjectPool.pMaterialFctn = &D3DMaterialObjectPool;
   gLMapPoolStatic.pMaterialFctn = &D3DMaterialLMapDynamicPool;
   gWallMaskPool.pMaterialFctn = &D3DMaterialWallMaskPool;
   gEffectPool.pMaterialFctn = &D3DMaterialEffectPool;
   gParticlePool.pMaterialFctn = &D3DMaterialParticlePool;

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MAGFILTER, gD3DDriverProfile.magFilter);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MINFILTER, gD3DDriverProfile.minFilter);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MAXANISOTROPY, gD3DDriverProfile.maxAnisotropy);
   if (config.mipMaps)
   {
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
   }
   else
   {
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
   }

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MAGFILTER, gD3DDriverProfile.magFilter);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MINFILTER, gD3DDriverProfile.minFilter);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MAXANISOTROPY, gD3DDriverProfile.maxAnisotropy);
      
   /***************************************************************************/
   /*                    VERTEX DECLARATIONS                                  */
   /***************************************************************************/

   IDirect3DDevice9_CreateVertexDeclaration(gpD3DDevice, decl0, &decl0dc);
   IDirect3DDevice9_CreateVertexDeclaration(gpD3DDevice, decl1, &decl1dc);
   IDirect3DDevice9_CreateVertexDeclaration(gpD3DDevice, decl2, &decl2dc);
   
   /***************************************************************************/
   /*                    DEFAULT Z-BIAS/DEPTHBIAS                             */
   /***************************************************************************/

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_SLOPESCALEDEPTHBIAS, F2DW(1.0f));
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW(0.0f));

   /***************************************************************************/
   /*                    VERTEX DECLARATIONS                                  */
   /***************************************************************************/

   D3DRenderLMapsBuild();
   ReleaseCapture();

   if (gD3DDriverProfile.bFogEnable)
   {
      float   start = 0.0f;
      float   end = 50000.8f;
      DWORD   mode = D3DFOG_LINEAR;

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, TRUE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGCOLOR, 0);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGTABLEMODE, mode);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGSTART, *(DWORD *)(&start));
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   /***************************************************************************/
   /*                       FRAMEBUFFER TEXTURES                              */
   /***************************************************************************/

   for (i = 0; i <= 15; i++)
   {
      IDirect3DDevice9_CreateTexture(gpD3DDevice, 256, 256, 1, D3DUSAGE_RENDERTARGET, 
         D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &gpBackBufferTex[i], NULL);
   }

   IDirect3DDevice9_CreateTexture(gpD3DDevice, 1024, 1024, 1, D3DUSAGE_RENDERTARGET, 
      D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &gpBackBufferTexFull, NULL);
   
   /***************************************************************************/
   /*                                FONT                                     */
   /***************************************************************************/

   // This will call D3DRenderFontInit to make sure the font texture is created
   GraphicsResetFont();

   /***************************************************************************/
   /*                                MISC                                     */
   /***************************************************************************/

   playerOldPos.x = 0;
   playerOldPos.y = 0;
   playerOldPos.z = 0;

   // Particle system static info.
   D3DParticleSystemInit();

   return S_OK;
}

/************************************************************************************
*
*  Shutdown Direct3d
*  - ...
*
************************************************************************************/
void D3DRenderShutDown(void)
{
   int   i, j;
   HRESULT hr;

   if (!gD3DDriverProfile.bSoftwareRenderer)
   {
      D3DCacheSystemShutdown(&gLMapCacheSystem);
      D3DCacheSystemShutdown(&gLMapCacheSystemStatic);
      D3DRenderPoolShutdown(&gLMapPool);
      D3DRenderPoolShutdown(&gLMapPoolStatic);

      D3DParticleSystemShutdown();
      D3DTexCacheShutdown();

      D3DCacheSystemShutdown(&gObjectCacheSystem);
      D3DCacheSystemShutdown(&gWorldCacheSystem);
      D3DCacheSystemShutdown(&gWorldCacheSystemStatic);
      D3DCacheSystemShutdown(&gWallMaskCacheSystem);
      D3DCacheSystemShutdown(&gEffectCacheSystem);
      D3DCacheSystemShutdown(&gParticleCacheSystem);

      D3DRenderPoolShutdown(&gObjectPool);
      D3DRenderPoolShutdown(&gWorldPool);
      D3DRenderPoolShutdown(&gWorldPoolStatic);
      D3DRenderPoolShutdown(&gWallMaskPool);
      D3DRenderPoolShutdown(&gEffectPool);
      D3DRenderPoolShutdown(&gParticlePool);

      /***************************************************************************/
      /*                          CUSTOM TEXTURES                                */
      /***************************************************************************/
      
      if (gpDLightWhite)         hr = IDirect3DDevice9_Release(gpDLightWhite);
      if (gpDLightOrange)         hr = IDirect3DDevice9_Release(gpDLightOrange);
      if (gpNoLookThrough)      hr = IDirect3DDevice9_Release(gpNoLookThrough);
      if (gpBackBufferTexFull)   hr = IDirect3DDevice9_Release(gpBackBufferTexFull);
      if (gFont.pTexture)         hr = IDirect3DDevice9_Release(gFont.pTexture);

      gpDLightWhite      = NULL;   
      gpDLightOrange      = NULL;     
      gpNoLookThrough      = NULL;
      gpBackBufferTexFull   = NULL;
      gFont.pTexture      = NULL;
      
      for (i = 0; i < 16; i++)
      {
         if (gpBackBufferTex[i])
         {
            hr = IDirect3DDevice9_Release(gpBackBufferTex[i]);
            gpBackBufferTex[i] = NULL;
         }
      }

      for (j = 0; j < 5; j++)
      {
         for (i = 0; i < 6; i++)
         {
            if (gpSkyboxTextures[j][i])
            {
               hr = IDirect3DDevice9_Release(gpSkyboxTextures[j][i]);
               gpSkyboxTextures[j][i] = NULL;
            }
         }
      }

      for (i = 0; i < 8; i++)
      {
         if (gpViewElements[i])
         {
            hr = IDirect3DDevice9_Release(gpViewElements[i]);
            gpViewElements[i] = NULL;
         }
      }
      
      /***************************************************************************/
      /*                       VERTEX DECLARATIONS                               */
      /***************************************************************************/
      
      if (decl0dc) hr = IDirect3DDevice9_Release(decl0dc);
      if (decl1dc) hr = IDirect3DDevice9_Release(decl1dc);
      if (decl2dc) hr = IDirect3DDevice9_Release(decl2dc);

      decl0dc = NULL;
      decl1dc = NULL;
      decl2dc = NULL;

      /***************************************************************************/
      /*                               DEVICE                                    */
      /***************************************************************************/

      // clean D3D device
      if (gpD3DDevice)
      {
         // check cooperative level
         hr = IDirect3DDevice9_TestCooperativeLevel(gpD3DDevice);
         
         // must reset devices in state DEVICENOTRESET
         // or the release below will leave a mem-leak
         if (hr == D3DERR_DEVICENOTRESET)      
            hr = IDirect3DDevice9_Reset(gpD3DDevice, &gPresentParam);   

         // release device
         hr = IDirect3DDevice9_Release(gpD3DDevice);
      }

      // clean Direct3D
      if (gpD3D)
         hr = IDirect3D9_Release(gpD3D);

      gpD3DDevice   = NULL;
      gpD3D      = NULL;
   }
}

/************************************************************************************
*
*  Main render function
*  - This creates a frame on your monitor
*
************************************************************************************/
void D3DRenderBegin(room_type *room, Draw3DParams *params)
{
   D3DMATRIX   view, proj;
   int         angleHeading, anglePitch;
   long      timeOverall, timeWorld, timeObjects, timeLMaps, timeSkybox;
   long     timeSkybox2, timeInit, timeParticles;
   static ID   tempBkgnd = 0;
   HRESULT      hr;
   Bool      draw_sky = TRUE;
   Bool      draw_world = TRUE;
   Bool      draw_objects = TRUE;
   Bool      draw_particles = TRUE;
   room_contents_node   *pRNode;

   /***************************************************************************/
   /*                             PREPARATIONS                                */
   /***************************************************************************/

   // Check if we have the D3D device.
   // https://msdn.microsoft.com/en-us/library/windows/desktop/bb174472(v=vs.85).aspx
   hr = IDirect3DDevice9_TestCooperativeLevel(gpD3DDevice);

   // Device needs to be reset, but can be used afterwards.
   if (hr == D3DERR_DEVICENOTRESET)
   {
      D3DRenderReset();
      // Test again, in case the reset failed.
      hr = IDirect3DDevice9_TestCooperativeLevel(gpD3DDevice);
   }

   // Just return if not OK (should potentially handle D3DERR_DRIVERINTERNALERROR
   // by switching to software, but drawing nothing is fine for now.
   if (hr != D3D_OK)
      return;


   // If blind, don't draw anything
   if (effects.blind)
   {
      draw_sky = FALSE;
      draw_world = FALSE;
      draw_objects = FALSE;
      draw_particles = FALSE;
   }

   // Load skybox textures if not yet loaded
   if (gpSkyboxTextures[0][0] == NULL)
   {
      D3DRenderBackgroundsLoad("./resource/skya.bsf", 0);
      D3DRenderBackgroundsLoad("./resource/skyb.bsf", 1);
      D3DRenderBackgroundsLoad("./resource/skyc.bsf", 2);
      D3DRenderBackgroundsLoad("./resource/skyd.bsf", 3);
      D3DRenderBackgroundsLoad("./resource/redsky.bsf", 4);
   }

   if (tempBkgnd != current_room.bkgnd)
   {
      D3DRenderBackgroundSet2(current_room.bkgnd);
      tempBkgnd = current_room.bkgnd;
   }

   // view element textures
   if (gFrame == 0)
   {
      for (int i = 0; i < NUM_VIEW_ELEMENTS; ++i)
      {
         gpViewElements[i] = D3DRenderTextureCreateFromResource(ViewElements[i].bits,
            ViewElements[i].width, ViewElements[i].height);
      }
   }

   gFrame++;

   timeOverall = timeWorld = timeObjects = timeLMaps = timeSkybox = timeSkybox2 = timeInit = timeParticles = 0;

   timeOverall = timeInit = timeGetTime();

   gNumObjects = 0;
   gNumVertices = 0;
   gNumDPCalls = 0;

   p = params;

   D3DLightingRenderBegin(room);

   if (gD3DRedrawAll & D3DRENDER_REDRAW_ALL)
   {
      D3DGeometryBuildNew(room);
      gD3DRedrawAll = FALSE;
   }
   else if (gD3DRedrawAll & D3DRENDER_REDRAW_UPDATE)
   {
      GeometryUpdate(&gWorldPoolStatic);
      gD3DRedrawAll = FALSE;
   }
   else if (gD3DRedrawAll & D3DRENDER_REDRAW_STATIC_LIGHTS)
   {
      D3DGeometryBuildNewStaticLMaps(room);
      gD3DRedrawAll = FALSE;
   }
   else if (gD3DDriverProfile.bFogEnable == FALSE)
   {
      GeometryUpdate(&gWorldPoolStatic);
   }

   /***************************************************************************/
   /*                      BEGIN SCENE RENDERING                              */
   /***************************************************************************/
      
   IDirect3DDevice9_Clear(gpD3DDevice, 0, NULL, D3DCLEAR_TARGET |
                          D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0, 0);

   // Shouldn't be needed - palette gets set appropriately later.
   //D3DRenderPaletteSet(0, 0, 0);

   IDirect3DDevice9_BeginScene(gpD3DDevice);

   /***************************************************************************/
   /*                              MATRICES                                   */
   /***************************************************************************/

   MatrixIdentity(&mIdentity);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);

   angleHeading = params->viewer_angle + 3072;
   if (angleHeading >= MAX_ANGLE)
      angleHeading -= MAX_ANGLE;

   anglePitch = PlayerGetHeightOffset();

   // Set some matrices up now so we don't have to calculate them multiple times during rendering.
   MatrixRotateY(&mPlayerHeading, (float)angleHeading * 360.0f / 4096.0f * PI / 180.0f);
   MatrixTranspose(&mPlayerHeadingTrans, &mPlayerHeading);
   MatrixRotateX(&mPlayerPitchFudge, (float)anglePitch * 50.0f / 414.0f * PI / 180.0f);
   MatrixRotateX(&mPlayerPitch, (float)anglePitch * 45.0f / 414.0f * PI / 180.0f);
   MatrixMultiply(&mPlayerHPMul, &mPlayerHeading, &mPlayerPitch);
   MatrixMultiply(&mPlayerHPFudgeMul, &mPlayerHeading, &mPlayerPitchFudge);
   MatrixTranslate(&mPlayerViewTranslate, -(float)params->viewer_x, -(float)params->viewer_height, -(float)params->viewer_y);
   MatrixMultiply(&view, &mPlayerViewTranslate, &mPlayerHPMul);

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &view);

   XformMatrixPerspective(&proj, FOV_H, FOV_V, 100.0f, Z_RANGE);
//   aspectRatio = (float)(gD3DRect.right - gD3DRect.left) / (float)(gD3DRect.bottom - gD3DRect.top);
//   XformMatrixPerspective(&proj, -PI / 4.0f * 0.64f * aspectRatio, PI / 6.0f * 1.55f * (1.0f / aspectRatio), 100.0f, 150000.0f);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &proj);

   /***************************************************************************/
   /*                              PREPARATIONS                               */
   /***************************************************************************/

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_COLORWRITEENABLE,
                                   D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_CW);

   D3DCacheSystemReset(&gObjectCacheSystem);
   D3DCacheSystemReset(&gLMapCacheSystem);
   D3DCacheSystemReset(&gWorldCacheSystem);

   UpdateRoom3D(room, params);

   // Go through all node and mark them as unseen. Used to determine
   // whether to recalculate lighting etc. for this node this frame.
   for (int i = 0; i < room->num_nodes; ++i)
   {
      room->nodes[i].seenFloorThisFrame = False;
      room->nodes[i].seenCeilThisFrame = False;
      if (room->nodes[i].type == BSPinternaltype)
      {
         WallData *pWall;
         for (pWall = room->nodes[i].u.internal.walls_in_plane;
            pWall != NULL; pWall = pWall->next)
         {
            pWall->seen &= ~HR_DRAWMASK;
         }
      }
   }

   playerDeltaPos.x = params->viewer_x - playerOldPos.x;
   playerDeltaPos.y = params->viewer_y - playerOldPos.y;
   playerDeltaPos.z = params->viewer_height - playerOldPos.z;

   playerOldPos.x = params->viewer_x;
   playerOldPos.y = params->viewer_y;
   playerOldPos.z = params->viewer_height;

   timeInit = timeGetTime() - timeInit;

   /***************************************************************************/
   /*                              SKYBOX                                     */
   /***************************************************************************/

   if (draw_sky)
   {
      timeSkybox = timeGetTime();
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_NONE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, FALSE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_FALSE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, TRUE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAREF, TEMP_ALPHA_REF);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHABLENDENABLE, FALSE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, FALSE);

      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      D3DRenderPoolReset(&gWorldPool, &D3DMaterialWorldPool);
      D3DCacheSystemReset(&gWorldCacheSystem);
      D3DRenderSkyboxDraw(&gWorldPool);
      D3DCacheFill(&gWorldCacheSystem, &gWorldPool, 1);
      D3DCacheFlush(&gWorldCacheSystem, &gWorldPool, 1, D3DPT_TRIANGLESTRIP);

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, TRUE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_TRUE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, TRUE);
      timeSkybox = timeGetTime() - timeSkybox;
   }

   // restore the correct view matrix
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &view);

   /***************************************************************************/
   /*                              WORLD                                      */
   /***************************************************************************/
   
   if (draw_world)
   {
      timeWorld = timeGetTime();

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_CW);

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, FALSE, D3DBLEND_ONE, D3DBLEND_ONE);

      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

      IDirect3DDevice9_SetTextureStageState(gpD3DDevice, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
      IDirect3DDevice9_SetTextureStageState(gpD3DDevice, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

      // Set depth bias. Used to be done in material chunk function call, with different bias
      // for ceiling matching sector 0 ceiling (unsure why this was done). Reproduced a full
      // example here:
      //if (pChunk->pSector->ceiling == current_room.sectors[0].ceiling)
      //   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)0));
      // else
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)ZBIAS_WORLD * -0.00001f));

      // This is disabled as it seems to draw sky texture in-between wall
      // textures on some maps. No errant effects noted from disabling this.
      // no look through/sky texture walls
      if (0)
      {
         // for no look through and sky texture, we take all map and procedurally generated
         // polys and render them, drawing only where there is no alpha.  for no look
         // through walls, this means transparent pixels will be drawn, and the "sky" texture
         // we use in the new client is just a 1x1 black texture with no alpha.  all pixels
         // rendered in this fashion also set stencil to one.  afterwards, normal geometry
         // is drawn and any world geometry that passes z test reverts stencil back to zero.
         // finally, skybox is drawn again only where stencil = 1, with z test set to ALWAYS
         // zbias is used to try and cover up as much zfighting as possible.

         // Set alpha test state.
         D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, 254, D3DCMP_LESSEQUAL);
         D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, FALSE, D3DBLEND_ONE, D3DBLEND_ONE);
         D3DRENDER_SET_STENCIL_STATE(gpD3DDevice, TRUE, D3DCMP_ALWAYS, 1, D3DSTENCILOP_REPLACE,
            D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP);

         // like below, we need to render a wireframe of each poly to cover holes
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_WIREFRAME);
         gWireframe = TRUE;
//         D3DCacheFlush(&gWallMaskCacheSystem, &gWallMaskPool, 1, D3DPT_TRIANGLESTRIP);
         gWireframe = FALSE;
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_SOLID);
         D3DCacheFlush(&gWallMaskCacheSystem, &gWallMaskPool, 1, D3DPT_TRIANGLESTRIP);
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_STENCILENABLE, FALSE);
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_CW);
         
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_STENCILENABLE, FALSE);

         // Reset alpha test state.
         D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      }

      D3DRENDER_SET_STENCIL_STATE(gpD3DDevice, TRUE, D3DCMP_ALWAYS, 1, D3DSTENCILOP_ZERO,
         D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP);

      D3DRenderPoolReset(&gWorldPool, &D3DMaterialWorldPool);
      D3DCacheSystemReset(&gWorldCacheSystem);
      D3DRenderWorldDraw(room, params);
      D3DCacheFill(&gWorldCacheSystem, &gWorldPool, 1);

      // this pass is a gigantic hack used to cover up the cracks
      // caused by all the t-junctions in the old geometry.  the entire world is drawn
      // in wireframe, with zwrite disabled.  welcome to my hell
      if (config.drawWireframe)
      {
         gWireframe = TRUE;
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, FALSE);
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_WIREFRAME);
         D3DCacheFlush(&gWorldCacheSystemStatic, &gWorldPoolStatic, 1, D3DPT_TRIANGLESTRIP);
         D3DCacheFlush(&gWorldCacheSystem, &gWorldPool, 1, D3DPT_TRIANGLESTRIP);
         gWireframe = FALSE;
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, TRUE);
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_SOLID);      
      }

      // finally, we actually get around to just drawing the goddam world
      D3DCacheFlush(&gWorldCacheSystemStatic, &gWorldPoolStatic, 1, D3DPT_TRIANGLESTRIP);
      D3DCacheFlush(&gWorldCacheSystem, &gWorldPool, 1, D3DPT_TRIANGLESTRIP);

      timeWorld = timeGetTime() - timeWorld;
   }

   // skybox again, drawing only where stencil = 1
   if (draw_sky)
   {
      timeSkybox2 = timeGetTime();
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_NONE);
      
      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, FALSE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, FALSE, D3DBLEND_ONE, D3DBLEND_ONE);
      D3DRENDER_SET_STENCIL_STATE(gpD3DDevice, TRUE, D3DCMP_LESS, 0, D3DSTENCILOP_KEEP,
         D3DSTENCILOP_KEEP, D3DSTENCILOP_KEEP);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_FALSE);

      if (gD3DDriverProfile.bFogEnable)
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, FALSE);
    
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      D3DRenderPoolReset(&gWorldPool, &D3DMaterialWorldPool);
      D3DCacheSystemReset(&gWorldCacheSystem);
      D3DRenderSkyboxDraw(&gWorldPool);
      D3DCacheFill(&gWorldCacheSystem, &gWorldPool, 1);
      D3DCacheFlush(&gWorldCacheSystem, &gWorldPool, 1, D3DPT_TRIANGLESTRIP);

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_STENCILENABLE, FALSE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_TRUE);

      if (gD3DDriverProfile.bFogEnable)
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, TRUE);
      timeSkybox2 = timeGetTime() - timeSkybox2;
   }

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &view);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_CW);

   /***************************************************************************/
   /*                          POST LIGHTMAPS                                 */
   /***************************************************************************/
   
   if (draw_world && config.dynamicLights)
   {
      timeLMaps = timeGetTime();
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MAGFILTER, gD3DDriverProfile.magFilter);
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MINFILTER, gD3DDriverProfile.minFilter);
      if (config.mipMaps)
         IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
      else
         IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MAXANISOTROPY, gD3DDriverProfile.maxAnisotropy);
      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl2dc);

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_ONE, D3DBLEND_ONE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, FALSE);

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, TRUE);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FILLMODE, D3DFILL_SOLID);
      D3DCacheFlush(&gLMapCacheSystemStatic, &gLMapPoolStatic, 2, D3DPT_TRIANGLESTRIP);
      D3DRenderPoolReset(&gLMapPool, &D3DMaterialLMapDynamicPool);

      // We now loop through each light and traverse the BSP tree to determine
      // which nodes get lit up. Any parts of the tree that are behind the player
      // or whose bounding box does not contain the light are not checked, thus
      // speeding up light drawing significantly.
      //long timeStatic = timeGetTime();
      for (int numLights = 0; numLights < gDLightCache.numLights; numLights++)
      {
         D3DRenderLMapsPostDraw(room->tree, params, &gDLightCache.dLights[numLights]);
      }
      //timeStatic = timeGetTime() - timeStatic;
      //long timeDynamic = timeGetTime();
      for (int numLights = 0; numLights < gDLightCacheDynamic.numLights; numLights++)
      {
         D3DRenderLMapsDynamicPostDraw(room->tree, params, &gDLightCacheDynamic.dLights[numLights]);
      }
      //timeDynamic = timeGetTime() - timeDynamic;
      //debug(("Dynamic light maps = %d, static = %d\n", timeDynamic, timeStatic));
      D3DCacheFill(&gLMapCacheSystem, &gLMapPool, 2);
      D3DCacheFlush(&gLMapCacheSystem, &gLMapPool, 2, D3DPT_TRIANGLESTRIP);
      if (gD3DDriverProfile.bFogEnable)
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGENABLE, TRUE);

      timeLMaps = timeGetTime() - timeLMaps;
   }

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_NONE);

   /***************************************************************************/
   /*                   NAMES / OBJECTS / PLAYEROVERLAYS                      */
   /***************************************************************************/
   
   if (draw_objects)
   {
      timeObjects = timeGetTime();

      /************************** NAMES *********************************/

      if (config.draw_player_names || config.draw_sign_names || config.draw_npc_names)
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

         IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
         IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

         D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
         D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_SRCCOLOR);

         D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectPool);
         D3DCacheSystemReset(&gObjectCacheSystem);
         D3DRenderNamesDraw3D(&gObjectCacheSystem, &gObjectPool, room, params, &gFont);
         D3DRenderQuestInfoDraw3D(&gObjectCacheSystem, &gObjectPool, room, params, &gFont);
         D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 1);
         D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 1, D3DPT_TRIANGLESTRIP);

         IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MAGFILTER, gD3DDriverProfile.magFilter);
         IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MINFILTER, gD3DDriverProfile.minFilter);
      }

      /********************** NORMAL OBJECTS *****************************/
      
      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);

      D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectPool);
      D3DCacheSystemReset(&gObjectCacheSystem);
      //D3DRenderBackgroundObjectsDraw(&gObjectPool, room, params);
      D3DRenderOverlaysDraw(&gObjectPool, room, params, 1, FALSE, FALSE);
      D3DRenderObjectsDraw(&gObjectPool, room, params, FALSE, FALSE);
      D3DRenderOverlaysDraw(&gObjectPool, room, params, 0, FALSE, FALSE);
      D3DRenderProjectilesDrawNew(&gObjectPool, room, params);
      D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 1);
      D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 1, D3DPT_TRIANGLESTRIP);

      /********************** TRANSPARENT OBJECTS ****************************/

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);

      D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectPool);
      D3DCacheSystemReset(&gObjectCacheSystem);
      D3DRenderOverlaysDraw(&gObjectPool, room, params, 1, FALSE, TRUE);
      D3DRenderObjectsDraw(&gObjectPool, room, params, FALSE, TRUE);
      D3DRenderOverlaysDraw(&gObjectPool, room, params, 0, FALSE, TRUE);
      D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 1);
      D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 1, D3DPT_TRIANGLESTRIP);

      /******************** INVISIBLE OBJECTS *****************************/

      D3DRenderFramebufferTextureCreate(gpBackBufferTexFull, gpBackBufferTex[0], 256, 256);

      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &view);
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &proj);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, TRUE);

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl2dc);

      D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectInvisiblePool);
      D3DCacheSystemReset(&gObjectCacheSystem);
      D3DRenderOverlaysDraw(&gObjectPool, room, params, 1, DRAWFX_INVISIBLE, FALSE);
      D3DRenderObjectsDraw(&gObjectPool, room, params, DRAWFX_INVISIBLE, FALSE);
      D3DRenderOverlaysDraw(&gObjectPool, room, params, 0, DRAWFX_INVISIBLE, FALSE);
      D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 2);
      D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 2, D3DPT_TRIANGLESTRIP);

      /********************** PLAYER-OVERLAYS ****************************/

      // get player node
      pRNode = GetRoomObjectById(player.id);

      // player is invisible
      if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl2dc);

         D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectInvisiblePool);
         D3DCacheSystemReset(&gObjectCacheSystem);
         D3DRenderPlayerOverlaysDraw(&gObjectPool, room, params);
         D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 2);
         D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 2, D3DPT_TRIANGLESTRIP);
      }
      else
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

         D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectPool);
         D3DCacheSystemReset(&gObjectCacheSystem);
         D3DRenderPlayerOverlaysDraw(&gObjectPool, room, params);
         D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 1);
         D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 1, D3DPT_TRIANGLESTRIP);
      }

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      timeObjects = timeGetTime() - timeObjects;
   }

   /***************************************************************************/
   /*                        POST OVERLAYS                                    */
   /***************************************************************************/
   
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, D3DTA_CURRENT, D3DTA_TEXTURE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, D3DTA_CURRENT, D3DTA_TEXTURE);

   IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
   IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl0dc);

   // Set up orthographic projection for drawing overlays
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &mIdentity);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &mIdentity);

   // post overlay effects
   if (draw_objects)
   {
      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, 1, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_FALSE);

      D3DCacheSystemReset(&gObjectCacheSystem);
      D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectPool);
      D3DPostOverlayEffects(&gObjectPool);
      D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 1);
      D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 1, D3DPT_TRIANGLESTRIP);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, D3DZB_TRUE);
   }

   /***************************************************************************/
   /*                             PARTICLES                                   */
   /***************************************************************************/

   if (draw_particles)
   {
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &view);
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &proj);

      timeParticles = timeGetTime();

      // Updates emitter positions for all particle systems.
      D3DParticleSystemSetPlayerPos(playerDeltaPos.x, playerDeltaPos.y, playerDeltaPos.z);

      if (effects.sand)
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl0dc);

         D3DParticleSystemUpdateFluid(&gParticleSystemSand, &gParticlePool, &gParticleCacheSystem, params);
      }
      if (effects.raining && config.weather)
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl0dc);

         D3DParticleSystemUpdateFluid(&gParticleSystemRain, &gParticlePool, &gParticleCacheSystem, params);
      }
      if (effects.snowing && config.weather)
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

         D3DParticleSystemUpdateFluid(&gParticleSystemSnow, &gParticlePool, &gParticleCacheSystem, params);
      }
      if (effects.fireworks)
      {
         IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
         IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl0dc);

         D3DParticleSystemUpdateBurst(&gParticleSystemFireworks, &gParticlePool, &gParticleCacheSystem, params);
      }
      timeParticles = timeGetTime() - timeParticles;

      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &mIdentity);
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &mIdentity);
   }

   /***************************************************************************/
   /*                         EFFECTS: BLUR/WAVER                             */
   /***************************************************************************/
   
   // test blur
   if (effects.blur || effects.waver)
//   if (1)
   {
      d3d_render_packet_new   *pPacket;
      d3d_render_chunk_new   *pChunk;
      int                  i, t;
      static int            offset = 0;
      static int            offsetDir = 1;

      t = gFrame & 7;

      if (gFrame & 63)
      {
         offset += offsetDir;

         if ((offset > 31) || (offset < 0))
         {
            offsetDir = -offsetDir;
            offset += offsetDir;
         }
      }

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, FALSE, 1, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, FALSE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      D3DRenderFramebufferTextureCreate(gpBackBufferTexFull, gpBackBufferTex[t], 256, 256);
      D3DCacheSystemReset(&gEffectCacheSystem);
      D3DRenderPoolReset(&gEffectPool, &D3DMaterialBlurPool);

      for (i = 0; i <= 7; i++)
      {
         pPacket = D3DRenderPacketNew(&gEffectPool);
         if (NULL == pPacket)
            return;
         pChunk = D3DRenderChunkNew(pPacket);
         pPacket->pMaterialFctn = D3DMaterialBlurPacket;
         pChunk->pMaterialFctn = D3DMaterialBlurChunk;
         pPacket->pTexture = gpBackBufferTex[i];
         pChunk->numIndices = pChunk->numVertices = 4;
         pChunk->numPrimitives = pChunk->numVertices - 2;
         MatrixIdentity(&pChunk->xForm);

         CHUNK_XYZ_SET(pChunk, 0, D3DRENDER_SCREEN_TO_CLIP_X(-offset, gScreenWidth),
            0, D3DRENDER_SCREEN_TO_CLIP_Y(-offset, gScreenHeight));
         CHUNK_XYZ_SET(pChunk, 1, D3DRENDER_SCREEN_TO_CLIP_X(-offset, gScreenWidth),
            0, D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight + offset, gScreenHeight));
         CHUNK_XYZ_SET(pChunk, 2, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth + offset, gScreenWidth),
            0, D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight + offset, gScreenHeight));
         CHUNK_XYZ_SET(pChunk, 3, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth + offset, gScreenWidth),
            0, D3DRENDER_SCREEN_TO_CLIP_Y(-offset, gScreenHeight));

         CHUNK_ST0_SET(pChunk, 0, 0.0f, 0.0f);
         CHUNK_ST0_SET(pChunk, 1, 0.0f, 1.0f);
         CHUNK_ST0_SET(pChunk, 2, 1.0f, 1.0f);
         CHUNK_ST0_SET(pChunk, 3, 1.0f, 0.0f);

         CHUNK_BGRA_SET(pChunk, 0, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX / 4);
         CHUNK_BGRA_SET(pChunk, 1, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX / 4);
         CHUNK_BGRA_SET(pChunk, 2, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX / 4);
         CHUNK_BGRA_SET(pChunk, 3, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX / 4);

         CHUNK_INDEX_SET(pChunk, 0, 1);
         CHUNK_INDEX_SET(pChunk, 1, 2);
         CHUNK_INDEX_SET(pChunk, 2, 0);
         CHUNK_INDEX_SET(pChunk, 3, 3);
      }

      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &mIdentity);
      IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &mIdentity);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, FALSE);

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, FALSE, 1, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);

      D3DCacheFill(&gEffectCacheSystem, &gEffectPool, 1);
      D3DCacheFlush(&gEffectCacheSystem, &gEffectPool, 1, D3DPT_TRIANGLESTRIP);

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZENABLE, TRUE);
   }

   /***************************************************************************/
   /*                             VIEW ELEMENTS                               */
   /***************************************************************************/

   if (1)
   {
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

      D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
      D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);

      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

      if (config.aaMode > 0)
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_MULTISAMPLEANTIALIAS, FALSE);

      IDirect3DDevice9_SetVertexShader(gpD3DDevice, NULL);
      IDirect3DDevice9_SetVertexDeclaration(gpD3DDevice, decl1dc);

      D3DRenderPoolReset(&gObjectPool, &D3DMaterialObjectPool);
      D3DCacheSystemReset(&gObjectCacheSystem);
      D3DRenderViewElementsDraw(&gObjectPool);
      D3DCacheFill(&gObjectCacheSystem, &gObjectPool, 1);
      D3DCacheFlush(&gObjectCacheSystem, &gObjectPool, 1, D3DPT_TRIANGLESTRIP);

      if (config.aaMode > 0)
         IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_MULTISAMPLEANTIALIAS, TRUE);

      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MAGFILTER, gD3DDriverProfile.magFilter);
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_MINFILTER, gD3DDriverProfile.minFilter);
   }

   /***************************************************************************/
   /*                        END SCENE RENDERING                              */
   /***************************************************************************/
      
   IDirect3DDevice9_EndScene(gpD3DDevice);

   RECT   rect;
   rect.top = 0;
   rect.bottom = gScreenHeight;
   rect.left = 0;
   rect.right = gScreenWidth;

   hr = IDirect3DDevice9_Present(gpD3DDevice, &rect, &gD3DRect, NULL, NULL);

   timeOverall = timeGetTime() - timeOverall;

#ifndef NODPRINTFS
   //   debug(("number of objects = %d\n", gNumObjects));
   if ((gFrame & 255) == 255)
      debug(("number of vertices = %d\nnumber of dp calls = %d\n", gNumVertices, gNumDPCalls));
   //debug(("all = %d lmaps = %d wrld = %d obj = %d  particles = %d sky = %d init = %d\n",
   //   timeOverall, timeLMaps, timeWorld, timeObjects, timeParticles, timeSkybox+timeSkybox2, timeInit));
#endif
}

// Resets the renderer.
void D3DRenderReset()
{
   D3DRenderShutDown();
   gFrame = 0;
   gD3DRedrawAll |= D3DRENDER_REDRAW_ALL;
   D3DRenderInit(hMain);
   // Particle systems were stopped, reset the emitters.
   D3DParticlesInit(false);
}

void D3DRenderWorldDraw(room_type *room, Draw3DParams *params)
{
   BSPnode *pNode = NULL;
   WallData *pWall;

   for (int count = 0; count < room->num_nodes; count++)
   {
      pNode = &room->nodes[count];

      // If the player can't see the bounding box (i.e.behind camera view) skip this node.
      if (IsHidden(params, (long)pNode->bbox.x0, (long)pNode->bbox.y0, (long)pNode->bbox.x1, (long)pNode->bbox.y1))
         continue;

      switch (pNode->type)
      {
         case BSPinternaltype:
            for (pWall = pNode->u.internal.walls_in_plane; pWall != NULL; pWall = pWall->next)
            {
               if (!((pWall->pos_sidedef && pWall->pos_sidedef->flags & WF_HAS_ANIMATED)
                  || (pWall->neg_sidedef && pWall->neg_sidedef->flags & WF_HAS_ANIMATED)
                  || (pWall->pos_sector && pWall->pos_sector->flags & SF_HAS_ANIMATED)
                  || (pWall->neg_sector && pWall->neg_sector->flags & SF_HAS_ANIMATED)))
               {
                  continue;
               }

               /*if ((pWall->pos_sidedef) && (pWall->neg_sidedef))
               {
                  if ((pWall->pos_sidedef->flags & WF_TRANSPARENT) &&
                     (pWall->neg_sidedef->flags & WF_TRANSPARENT))
                     {
                        continue;
                     }
               }*/

               if (!(pWall->pos_sidedef || pWall->neg_sidedef))
                  continue;

               // TODO: this feature doesn't currently work properly, due to walls not being tagged
               // as drawable correctly in drawbsp.c. Code left commented out, as it is a work in progress.

               // D3DRenderWorldDraw now relies on checks done while constructing the list
               // of viewable objects, walls etc. in DrawBSP() in drawbsp.c. Walls that can be seen
               // have the respective boolean value set to TRUE, so all we need to do here is check
               // that value to decide whether to render the wall.  Old code left commented out as 
               // opposed to deleting in case any modification to drawbsp.c is performed which
               // invalidates this method.
               //if (pWall->seen & SR_DRAWNORMAL)
               if (pWall->seen & WF_CANDRAWNORMAL)
               {
                  D3DRenderPacketWallAdd(pWall, &gWorldPool, D3DRENDER_WALL_NORMAL, 1, TRUE);
                  D3DRenderPacketWallAdd(pWall, &gWorldPool, D3DRENDER_WALL_NORMAL, -1, TRUE);
               }

               //if (pWall->seen & SR_DRAWBELOW)
               if (pWall->seen & WF_CANDRAWBELOW)
               {
                  D3DRenderPacketWallAdd(pWall, &gWorldPool, D3DRENDER_WALL_BELOW, 1, TRUE);
                  D3DRenderPacketWallAdd(pWall, &gWorldPool, D3DRENDER_WALL_BELOW, -1, TRUE);
               }

               //if (pWall->seen & SR_DRAWABOVE)
               if (pWall->seen & WF_CANDRAWABOVE)
               {
                  D3DRenderPacketWallAdd(pWall, &gWorldPool, D3DRENDER_WALL_ABOVE, 1, TRUE);
                  D3DRenderPacketWallAdd(pWall, &gWorldPool, D3DRENDER_WALL_ABOVE, -1, TRUE);
               }
            }

         break;

         case BSPleaftype:
            if (pNode->u.leaf.sector->flags & SF_HAS_ANIMATED)
            {
               //if (pNode->drawfloor)
               D3DRenderPacketFloorAdd(pNode, &gWorldPool, TRUE);
               //if (pNode->drawceiling)
               D3DRenderPacketCeilingAdd(pNode, &gWorldPool, TRUE);
            }
         break;

         default:
         break;
      }
   }
}

void D3DGeometryBuildNewStaticLMaps(room_type *room)
{
   BSPnode *pNode = NULL;
   WallData *pWall;

   debug(("D3DGeometryBuildNewStaticLMaps: number of nodes is %i\n", room->num_nodes));

   if (config.dynamicLights)
   {
      D3DCacheSystemReset(&gLMapCacheSystemStatic);
      D3DRenderPoolReset(&gLMapPoolStatic, &D3DMaterialLMapDynamicPool);
      for (int count = 0; count < room->num_nodes; count++)
      {
         for (int numLights = 0; numLights < gDLightCache.numLights; numLights++)
         {
            d_light *light = &gDLightCache.dLights[numLights];

            // If the light doesn't reach this bounding box, skip to the next one.
            pNode = &room->nodes[count];
            if (pNode->bbox.x0 >= light->maxX
               || pNode->bbox.x1 <= light->minX
               || pNode->bbox.y0 >= light->maxY
               || pNode->bbox.y1 <= light->minY)
               continue;
            switch (pNode->type)
            {
            case BSPinternaltype:
               for (pWall = pNode->u.internal.walls_in_plane; pWall != NULL; pWall = pWall->next)
               {
                  if (!(pWall->pos_sidedef || pWall->neg_sidedef))
                     continue;

                  // Skip if no draw flag or not inside light radius.
                  if ((!(pWall->seen & WF_CANDRAWMASK)) || !WallInsideRadius(light, pWall))
                     continue;

                  if (pWall->seen & WF_CANDRAWNORMAL)
                  {
                     D3DRenderLMapPostWallAdd(pWall, &gLMapPoolStatic, D3DRENDER_WALL_NORMAL, 1, light, FALSE);
                     D3DRenderLMapPostWallAdd(pWall, &gLMapPoolStatic, D3DRENDER_WALL_NORMAL, -1, light, FALSE);
                  }

                  if (pWall->seen & WF_CANDRAWBELOW)
                  {
                     D3DRenderLMapPostWallAdd(pWall, &gLMapPoolStatic, D3DRENDER_WALL_BELOW, 1, light, FALSE);
                     D3DRenderLMapPostWallAdd(pWall, &gLMapPoolStatic, D3DRENDER_WALL_BELOW, -1, light, FALSE);
                  }

                  if (pWall->seen & WF_CANDRAWABOVE)
                  {
                     D3DRenderLMapPostWallAdd(pWall, &gLMapPoolStatic, D3DRENDER_WALL_ABOVE, 1, light, FALSE);
                     D3DRenderLMapPostWallAdd(pWall, &gLMapPoolStatic, D3DRENDER_WALL_ABOVE, -1, light, FALSE);
                  }
               }

               break;

            case BSPleaftype:
               D3DRenderLMapPostFloorAdd(pNode, &gLMapPoolStatic, light, FALSE);
               D3DRenderLMapPostCeilingAdd(pNode, &gLMapPoolStatic, light, FALSE);
               break;

            default:
               break;
            }
         }
      }
      D3DCacheFill(&gLMapCacheSystemStatic, &gLMapPoolStatic, 2);
   }
}
void D3DGeometryBuildNew(room_type *room)
{
   BSPnode *pNode = NULL;
   WallData *pWall;

   D3DCacheSystemReset(&gWorldCacheSystemStatic);
   D3DCacheSystemReset(&gWallMaskCacheSystem);

   D3DRenderPoolReset(&gWorldPoolStatic, &D3DMaterialWorldPool);
   D3DRenderPoolReset(&gWallMaskPool, &D3DMaterialWallMaskPool);

   debug(("D3DGeometryBuildNew: number of nodes is %i\n", room->num_nodes));

   for (int count = 0; count < room->num_nodes; count++)
   {
      pNode = &room->nodes[count];

      switch (pNode->type)
      {
         case BSPinternaltype:
            for (pWall = pNode->u.internal.walls_in_plane; pWall != NULL; pWall = pWall->next)
            {
               if (!(pWall->pos_sidedef || pWall->neg_sidedef))
                  continue;

               if (pWall->seen & WF_CANDRAWNORMAL)
               {
                  D3DRenderPacketWallAdd(pWall, &gWorldPoolStatic, D3DRENDER_WALL_NORMAL, 1, FALSE);
                  D3DRenderPacketWallAdd(pWall, &gWorldPoolStatic, D3DRENDER_WALL_NORMAL, -1, FALSE);
                  D3DRenderPacketWallMaskAdd(pWall, &gWallMaskPool, D3DRENDER_WALL_NORMAL, 1, FALSE);
                  D3DRenderPacketWallMaskAdd(pWall, &gWallMaskPool, D3DRENDER_WALL_NORMAL, -1, FALSE);
               }

               if (pWall->seen & WF_CANDRAWBELOW)
               {
                  D3DRenderPacketWallAdd(pWall, &gWorldPoolStatic, D3DRENDER_WALL_BELOW, 1, FALSE);
                  D3DRenderPacketWallAdd(pWall, &gWorldPoolStatic, D3DRENDER_WALL_BELOW, -1, FALSE);
                  D3DRenderPacketWallMaskAdd(pWall, &gWallMaskPool, D3DRENDER_WALL_BELOW, 1, FALSE);
                  D3DRenderPacketWallMaskAdd(pWall, &gWallMaskPool, D3DRENDER_WALL_BELOW, -1, FALSE);
               }

               if (pWall->seen & WF_CANDRAWABOVE)
               {
                  D3DRenderPacketWallAdd(pWall, &gWorldPoolStatic, D3DRENDER_WALL_ABOVE, 1, FALSE);
                  D3DRenderPacketWallAdd(pWall, &gWorldPoolStatic, D3DRENDER_WALL_ABOVE, -1, FALSE);
                  D3DRenderPacketWallMaskAdd(pWall, &gWallMaskPool, D3DRENDER_WALL_ABOVE, 1, FALSE);
                  D3DRenderPacketWallMaskAdd(pWall, &gWallMaskPool, D3DRENDER_WALL_ABOVE, -1, FALSE);
               }
            }

         break;

         case BSPleaftype:
            D3DRenderPacketFloorAdd(pNode, &gWorldPoolStatic, FALSE);
            D3DRenderPacketCeilingAdd(pNode, &gWorldPoolStatic, FALSE);

            // this causes a lot of z fighting, and I don't think it's necessary anywhere
            //if (pNode->u.leaf.sector->floor == NULL)
            //   D3DRenderFloorMaskAdd(pNode, &gWallMaskPool, FALSE);

            if ((pNode->u.leaf.sector->ceiling == NULL) &&
               (pNode->u.leaf.sector->sloped_floor == NULL))
               D3DRenderCeilingMaskAdd(pNode, &gWallMaskPool, FALSE);
         break;

         default:
         break;
      }
   }

   D3DCacheFill(&gWorldCacheSystemStatic, &gWorldPoolStatic, 1);
   D3DCacheFill(&gWallMaskCacheSystem, &gWallMaskPool, 1);

   // Static light maps
   D3DGeometryBuildNewStaticLMaps(room);
}

void GeometryUpdate(d3d_render_pool_new *pPool)
{
   u_int            curPacket, curChunk;
   u_int            i, numPackets;
   d3d_render_cache   *pRenderCache = NULL;
   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;
   Sector               *pSector;
   int                  distX, distY, paletteIndex;
   list_type            list;
   long               lightScale;
   long               lo_end = FINENESS-shade_amount;

   for (list = pPool->renderPacketList; list != pPool->curPacketList->next; list = list->next)
   {
      pPacket = (d3d_render_packet_new *)list->data;

      if (list == pPool->curPacketList)
         numPackets = pPool->curPacket;
      else
         numPackets = pPool->size;

      for (curPacket = 0; curPacket < numPackets; curPacket++, pPacket++)
      {
         for (curChunk = 0; curChunk < pPacket->curChunk; curChunk++)
         {
            pChunk = &pPacket->renderChunks[curChunk];

            // Default lightScale
            lightScale = FINENESS;

            // Chunk is a floor.
            if (pChunk->flags & D3DRENDER_FLOOR)
            {
               pSector = pChunk->pSector;
               if (NULL == pSector)
                  continue;
               if (!shade_amount)
               {
                  if (pSector->sloped_floor != NULL)
                     pSector->sloped_floor->lightscale = FINENESS;
               }
               else if (pSector->sloped_floor != NULL
                  && (pSector->sloped_floor->flags & SLF_DIRECTIONAL))
               {
                  // light scale is based on dot product of surface normal and sun vector
                  lightScale = (long)(pSector->sloped_floor->plane.a * sun_vect.x +
                     pSector->sloped_floor->plane.b * sun_vect.y +
                     pSector->sloped_floor->plane.c * sun_vect.z) >> LOG_FINENESS;

                  lightScale = (lightScale + FINENESS) >> 1; // map to 0 to 1 range

                  lightScale = lo_end + ((lightScale * shade_amount) >> LOG_FINENESS);

                  if (lightScale > FINENESS)
                     lightScale = FINENESS;
                  else if (lightScale < 0)
                     lightScale = 0;

                  pSector->sloped_floor->lightscale = lightScale;
               }
            }
            // Chunk is a ceiling.
            else if (pChunk->flags & D3DRENDER_CEILING)
            {
               pSector = pChunk->pSector;
               if (NULL == pSector)
                  continue;
               if (!shade_amount)
               {
                  if (pSector->sloped_ceiling != NULL)
                     pSector->sloped_ceiling->lightscale = FINENESS;
               }
               else if (pSector->sloped_ceiling != NULL
                  && (pSector->sloped_ceiling->flags & SLF_DIRECTIONAL))
               {
                  // light scale is based on dot product of surface normal and sun vector
                  lightScale = (long)(pSector->sloped_ceiling->plane.a * sun_vect.x +
                     pSector->sloped_ceiling->plane.b * sun_vect.y +
                     pSector->sloped_ceiling->plane.a * sun_vect.z) >> LOG_FINENESS;

                  lightScale = (lightScale + FINENESS) >> 1; // map to 0 to 1 range

                  lightScale = lo_end + ((lightScale * shade_amount) >> LOG_FINENESS);

                  if (lightScale > FINENESS)
                     lightScale = FINENESS;
                  else if (lightScale < 0)
                     lightScale = 0;

                  pSector->sloped_ceiling->lightscale = lightScale;
               }
            }
            // Must be a wall.
            else
            {
               float a = pChunk->plane.a;
               float b = pChunk->plane.b;
               if (pChunk->side > 0)
               {
                  pSector = pChunk->pSectorPos;
               }
               else
               {
                  pSector = pChunk->pSectorNeg;
                  a = -a;
                  b = -b;
               }
               if (NULL == pSector)
                  continue;

               if (!shade_amount)
               {
                  lightScale = FINENESS;
               }
               else
               {
                  lightScale = (long)(a * sun_vect.x + b * sun_vect.y) >> LOG_FINENESS;
                  lightScale = (lightScale + FINENESS) >> 1; // map to 0 to 1 range

                  lightScale = lo_end + ((lightScale * shade_amount) >> LOG_FINENESS);

                  if (lightScale > FINENESS)
                     lightScale = FINENESS;
                  else if (lightScale < 0)
                     lightScale = 0;
               }
            }

            if (pRenderCache != pChunk->pRenderCache)
            {
               if (pRenderCache)
                  CACHE_BGRA_UNLOCK(pRenderCache);

               pRenderCache = pChunk->pRenderCache;
               CACHE_BGRA_LOCK(pRenderCache);
            }

            for (i = 0; i < pChunk->numVertices; i++)
            {
               distX = pChunk->xyz[i].x - player.x;
               distY = pChunk->xyz[i].y - player.y;

               if (gD3DDriverProfile.bFogEnable)
                  paletteIndex = GetLightPaletteIndex(FINENESS, pSector->light, lightScale, 0);
               else
                  paletteIndex = GetLightPaletteIndex(DistanceGet(distX, distY), pSector->light, lightScale, 0);

               pChunk->bgra[i].r = pChunk->bgra[i].g = pChunk->bgra[i].b = paletteIndex * COLOR_AMBIENT / 64;
               pChunk->bgra[i].a = 255;

               D3DCacheBGRASet(pChunk->pRenderCache, pChunk->startIndex + i, pChunk->bgra[i].b,
                  pChunk->bgra[i].g, pChunk->bgra[i].r, pChunk->bgra[i].a);
            }
         }
      }
   }

   if (pRenderCache)
      CACHE_BGRA_UNLOCK(pRenderCache);
}

void D3DRenderPacketFloorAdd(BSPnode *pNode, d3d_render_pool_new *pPool, Bool bDynamic)
{
   Sector      *pSector = pNode->u.leaf.sector;
   PDIB      pDib;
   int         vertex;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   if (pSector->floor)
   {
      pDib = pSector->floor;
   }
   else
      return;

   pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, 0, 0, 0);
   if (NULL == pPacket)
      return;
   pChunk = D3DRenderChunkNew(pPacket);
   assert(pChunk);

   // Update data if we haven't visited this node yet.
   if (!pNode->seenFloorThisFrame)
      D3DRenderFloorUpdate(pNode, pDib, pNode->floor_xyz, pNode->floor_stBase, pNode->floor_bgra);

//   pChunk->numIndices = (pNode->u.leaf.poly.npts - 2) * 3;
   pChunk->numVertices = pNode->u.leaf.poly.npts;
   pChunk->numIndices = pChunk->numVertices;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   pChunk->pSector = pSector;
   pChunk->side = 0;
   pChunk->flags |= D3DRENDER_FLOOR;

   if (pSector->light <= 127)
      pChunk->flags |= D3DRENDER_NOAMBIENT;

   if (pSector->sloped_floor)
   {
      pChunk->plane.a = pSector->sloped_floor->plane.a;
      pChunk->plane.b = pSector->sloped_floor->plane.b;
      pChunk->plane.c = pSector->sloped_floor->plane.c;
   }
   else
   {
      pChunk->plane.a = FINENESS;
      pChunk->plane.b = FINENESS;
      pChunk->plane.c = FINENESS;
   }

   pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

   if (bDynamic)
      pChunk->pMaterialFctn = &D3DMaterialFloorCeilDynamicChunk;
   else
      pChunk->pMaterialFctn = &D3DMaterialFloorCeilStaticChunk;

   for (vertex = 0; vertex < pNode->u.leaf.poly.npts; vertex++)
   {
      pChunk->xyz[vertex].x = pNode->floor_xyz[vertex].x;
      pChunk->xyz[vertex].y = pNode->floor_xyz[vertex].y;
      pChunk->xyz[vertex].z = pNode->floor_xyz[vertex].z;

      pChunk->st0[vertex].s = pNode->floor_stBase[vertex].s;
      pChunk->st0[vertex].t = pNode->floor_stBase[vertex].t;

      pChunk->bgra[vertex].b = pNode->floor_bgra[vertex].b;
      pChunk->bgra[vertex].g = pNode->floor_bgra[vertex].g;
      pChunk->bgra[vertex].r = pNode->floor_bgra[vertex].r;
      pChunk->bgra[vertex].a = pNode->floor_bgra[vertex].a;
   }

   {
      u_int   index;
      int   first, last;

      first = 1;
      last = pChunk->numVertices - 1;

      pChunk->indices[0] = 0;
      pChunk->indices[1] = last--;
      pChunk->indices[2] = first++;

      for (index = 3; index < pChunk->numIndices; first++, last--, index += 2)
      {
         pChunk->indices[index] = last;
         pChunk->indices[index + 1] = first;
      }
   }
}

void D3DRenderPacketCeilingAdd(BSPnode *pNode, d3d_render_pool_new *pPool, Bool bDynamic)
{
   Sector      *pSector = pNode->u.leaf.sector;
   PDIB      pDib;
   int         vertex;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   if (pSector->ceiling)
   {
      pDib = pSector->ceiling;
   }
   else
      return;

   // Update data if we haven't visited this node yet.
   if (!pNode->seenCeilThisFrame)
      D3DRenderCeilingUpdate(pNode, pDib, pNode->ceiling_xyz, pNode->ceiling_stBase, pNode->ceiling_bgra);

   pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, 0, 0, 0);
   if (NULL == pPacket)
      return;
   pChunk = D3DRenderChunkNew(pPacket);
   assert(pChunk);

//   pChunk->numIndices = (pNode->u.leaf.poly.npts - 2) * 3;
   pChunk->numVertices = pNode->u.leaf.poly.npts;
   pChunk->numIndices = pChunk->numVertices;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   pChunk->pSector = pSector;
   pChunk->side = 0;
   pChunk->flags |= D3DRENDER_CEILING;

   if (pSector->light <= 127)
      pChunk->flags |= D3DRENDER_NOAMBIENT;

   if (pSector->sloped_ceiling)
   {
      pChunk->plane.a = pSector->sloped_ceiling->plane.a;
      pChunk->plane.b = pSector->sloped_ceiling->plane.b;
      pChunk->plane.c = pSector->sloped_ceiling->plane.c;
   }
   else
   {
      pChunk->plane.a = FINENESS;
      pChunk->plane.b = FINENESS;
      pChunk->plane.c = FINENESS;
   }

   pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

   if (bDynamic)
      pChunk->pMaterialFctn = &D3DMaterialFloorCeilDynamicChunk;
   else
      pChunk->pMaterialFctn = &D3DMaterialFloorCeilStaticChunk;

   for (vertex = 0; vertex < pNode->u.leaf.poly.npts; vertex++)
   {
      pChunk->xyz[vertex].x = pNode->ceiling_xyz[vertex].x;
      pChunk->xyz[vertex].y = pNode->ceiling_xyz[vertex].y;
      pChunk->xyz[vertex].z = pNode->ceiling_xyz[vertex].z;

      pChunk->st0[vertex].s = pNode->ceiling_stBase[vertex].s;
      pChunk->st0[vertex].t = pNode->ceiling_stBase[vertex].t;

      pChunk->bgra[vertex].b = pNode->ceiling_bgra[vertex].b;
      pChunk->bgra[vertex].g = pNode->ceiling_bgra[vertex].g;
      pChunk->bgra[vertex].r = pNode->ceiling_bgra[vertex].r;
      pChunk->bgra[vertex].a = pNode->ceiling_bgra[vertex].a;
   }

   {
      u_int   index;
      int   first, last;

      first = 1;
      last = pChunk->numVertices - 1;

      pChunk->indices[0] = 0;
      pChunk->indices[1] = first++;
      pChunk->indices[2] = last--;

      for (index = 3; index < pChunk->numIndices; first++, last--, index += 2)
      {
         pChunk->indices[index] = first;
         pChunk->indices[index + 1] = last;
      }
   }
}

void D3DRenderPacketWallAdd(WallData *pWall, d3d_render_pool_new *pPool, unsigned int type, int side,
                        Bool bDynamic)
{
   Sidedef         *pSideDef = NULL;
   unsigned int   flags = 0;
   PDIB         pDib = NULL;
   int            vertex;
   custom_xyz      xyz[4];
   custom_st      stBase[4];
   custom_bgra      bgra[MAX_NPTS];
   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   // Positive if this wall part has already been seen this frame.
   int seenWall = 0;

   // pos and neg sidedefs have their x and y coords reversed
   if (side > 0)
   {
      pSideDef = pWall->pos_sidedef;

      if (NULL == pSideDef)
         return;

      switch (type)
      {
      case D3DRENDER_WALL_NORMAL:
         if (pWall->pos_sidedef->normal_bmap)
         {
            pDib = pWall->pos_sidedef->normal_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->pos_normal_xyz[i];
               bgra[i] = pWall->pos_normal_bgra[i];
               stBase[i] = pWall->pos_normal_stBase[i];
               flags = pWall->pos_normal_d3dFlags;
            }
            seenWall = (pWall->seen & HR_SEENPOSNORMAL);
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_BELOW:
         if (pWall->pos_sidedef->below_bmap)
         {
            pDib = pWall->pos_sidedef->below_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->pos_below_xyz[i];
               bgra[i] = pWall->pos_below_bgra[i];
               stBase[i] = pWall->pos_below_stBase[i];
               flags = pWall->pos_below_d3dFlags;
            }
            seenWall = (pWall->seen & HR_SEENPOSBELOW);
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_ABOVE:
         if (pWall->pos_sidedef->above_bmap)
         {
            pDib = pWall->pos_sidedef->above_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->pos_above_xyz[i];
               bgra[i] = pWall->pos_above_bgra[i];
               stBase[i] = pWall->pos_above_stBase[i];
               flags = pWall->pos_above_d3dFlags;
            }
            seenWall = (pWall->seen & HR_SEENPOSABOVE);
         }
         else
            pDib = NULL;
         break;

      default:
         break;
      }
   }
   else if (side < 0)
   {
      pSideDef = pWall->neg_sidedef;

      if (NULL == pSideDef)
         return;

      switch (type)
      {
      case D3DRENDER_WALL_NORMAL:
         if (pWall->neg_sidedef->normal_bmap)
         {
            pDib = pWall->neg_sidedef->normal_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->neg_normal_xyz[i];
               bgra[i] = pWall->neg_normal_bgra[i];
               stBase[i] = pWall->neg_normal_stBase[i];
               flags = pWall->neg_normal_d3dFlags;
            }
            seenWall = (pWall->seen & HR_SEENNEGNORMAL);
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_BELOW:
         if (pWall->neg_sidedef->below_bmap)
         {
            pDib = pWall->neg_sidedef->below_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->neg_below_xyz[i];
               bgra[i] = pWall->neg_below_bgra[i];
               stBase[i] = pWall->neg_below_stBase[i];
               flags = pWall->neg_below_d3dFlags;
            }
            seenWall = (pWall->seen & HR_SEENNEGBELOW);
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_ABOVE:
         if (pWall->neg_sidedef->above_bmap)
         {
            pDib = pWall->neg_sidedef->above_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->neg_above_xyz[i];
               bgra[i] = pWall->neg_above_bgra[i];
               stBase[i] = pWall->neg_above_stBase[i];
               flags = pWall->neg_above_d3dFlags;
            }
            seenWall = (pWall->seen & HR_SEENNEGABOVE);
         }
         else
            pDib = NULL;
         break;

      default:
         break;
      }
   }

   if (NULL == pDib)
      return;
//      pDib = current_room.sectors[0].floor;

   if (!seenWall)
      D3DRenderWallUpdate(pWall, pDib, &flags, xyz, stBase, bgra, type, side);

   pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, 0, 0, 0);
   if (NULL == pPacket)
      return;
   pChunk = D3DRenderChunkNew(pPacket);
   assert(pChunk);

   pChunk->numIndices = 4;
   pChunk->numVertices = 4;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   pChunk->flags = flags;
   pChunk->pSideDef = pSideDef;
   pChunk->pSectorPos = pWall->pos_sector;
   pChunk->pSectorNeg = pWall->neg_sector;
   pChunk->side = side;
   pChunk->plane.a = pWall->separator.a;
   pChunk->plane.b = pWall->separator.b;
   pChunk->plane.c = pWall->separator.c;

   pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

   if (bDynamic)
      pChunk->pMaterialFctn = &D3DMaterialWallDynamicChunk;
   else
      pChunk->pMaterialFctn = &D3DMaterialWallStaticChunk;

   for (vertex = 0; vertex < 4; vertex++)
   {
      pChunk->xyz[vertex].x = xyz[vertex].x;
      pChunk->xyz[vertex].y = xyz[vertex].y;
      pChunk->xyz[vertex].z = xyz[vertex].z;

      pChunk->st0[vertex].s = stBase[vertex].s - pWall->scrollS;
      pChunk->st0[vertex].t = stBase[vertex].t + pWall->scrollT;

      pChunk->bgra[vertex].b = bgra[vertex].b;
      pChunk->bgra[vertex].g = bgra[vertex].g;
      pChunk->bgra[vertex].r = bgra[vertex].r;
      pChunk->bgra[vertex].a = bgra[vertex].a;
   }

   pChunk->indices[0] = 1;
   pChunk->indices[1] = 2;
   pChunk->indices[2] = 0;
   pChunk->indices[3] = 3;
}

void D3DRenderPacketWallMaskAdd(WallData *pWall, d3d_render_pool_new *pPool, unsigned int type,
                        int side, Bool bDynamic)
{
   Sidedef         *pSideDef = NULL;
   custom_xyz      xyz[4];
   custom_st      stBase[4];
   custom_bgra      bgra[MAX_NPTS];
   unsigned int   flags;
   PDIB         pDib = NULL;
   int            vertex;
   Bool         bNoVTile = FALSE;
   Bool         bNoLookThrough = FALSE;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   // pos and neg sidedefs have their x and y coords reversed
   if (side > 0)
   {
      pSideDef = pWall->pos_sidedef;

      if (NULL == pSideDef)
         return;

      switch (type)
      {
      case D3DRENDER_WALL_NORMAL:
         if (pWall->pos_sidedef->normal_bmap)
         {
            pDib = pWall->pos_sidedef->normal_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->pos_normal_xyz[i];
               bgra[i] = pWall->pos_normal_bgra[i];
               stBase[i] = pWall->pos_normal_stBase[i];
               flags = pWall->pos_normal_d3dFlags;
            }
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_BELOW:
         if (pWall->pos_sidedef->below_bmap)
         {
            pDib = pWall->pos_sidedef->below_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->pos_below_xyz[i];
               bgra[i] = pWall->pos_below_bgra[i];
               stBase[i] = pWall->pos_below_stBase[i];
               flags = pWall->pos_below_d3dFlags;
            }
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_ABOVE:
         if (pWall->pos_sidedef->above_bmap)
         {
            pDib = pWall->pos_sidedef->above_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->pos_above_xyz[i];
               bgra[i] = pWall->pos_above_bgra[i];
               stBase[i] = pWall->pos_above_stBase[i];
               flags = pWall->pos_above_d3dFlags;
            }
         }
         else
            pDib = NULL;
         break;

      default:
         break;
      }
   }
   else if (side < 0)
   {
      pSideDef = pWall->neg_sidedef;

      if (NULL == pSideDef)
         return;

      switch (type)
      {
      case D3DRENDER_WALL_NORMAL:
         if (pWall->neg_sidedef->normal_bmap)
         {
            pDib = pWall->neg_sidedef->normal_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->neg_normal_xyz[i];
               bgra[i] = pWall->neg_normal_bgra[i];
               stBase[i] = pWall->neg_normal_stBase[i];
               flags = pWall->neg_normal_d3dFlags;
            }
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_BELOW:
         if (pWall->neg_sidedef->below_bmap)
         {
            pDib = pWall->neg_sidedef->below_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->neg_below_xyz[i];
               bgra[i] = pWall->neg_below_bgra[i];
               stBase[i] = pWall->neg_below_stBase[i];
               flags = pWall->neg_below_d3dFlags;
            }
         }
         else
            pDib = NULL;
         break;

      case D3DRENDER_WALL_ABOVE:
         if (pWall->neg_sidedef->above_bmap)
         {
            pDib = pWall->neg_sidedef->above_bmap;
            for (int i = 0; i < 4; i++)
            {
               xyz[i] = pWall->neg_above_xyz[i];
               bgra[i] = pWall->neg_above_bgra[i];
               stBase[i] = pWall->neg_above_stBase[i];
               flags = pWall->neg_above_d3dFlags;
            }
         }
         else
            pDib = NULL;
         break;

      default:
         break;
      }
   }

   if (pWall->pos_sidedef)
   {
      if (pWall->pos_sidedef->flags & WF_NO_VTILE)
         bNoVTile = TRUE;
      if (pWall->pos_sidedef->flags & WF_NOLOOKTHROUGH)
         bNoLookThrough = TRUE;
   }

   if (pWall->neg_sidedef)
   {
      if (pWall->neg_sidedef->flags & WF_NO_VTILE)
         bNoVTile = TRUE;
      if (pWall->neg_sidedef->flags & WF_NOLOOKTHROUGH)
         bNoLookThrough = TRUE;
   }

   if (NULL == pDib)
      return;

   if ((pSideDef->flags & WF_NOLOOKTHROUGH) == 0)
   {
      if ((bNoLookThrough == 0) || (bNoVTile == 0))
         return;
   }

   // Only called after D3DRenderPacketWallAdd, which updates data.
   //D3DRenderWallUpdate(pWall, pDib, &flags, xyz, stBase, bgra, type, side);

   pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, 0, 0, 0);
   if (NULL == pPacket)
      return;
   pChunk = D3DRenderChunkNew(pPacket);
   assert(pChunk);

   flags = 0;
   pChunk->numIndices = 4;
   pChunk->numVertices = 4;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   pChunk->flags = flags;
   pChunk->pSideDef = pSideDef;
   pChunk->pSectorPos = pWall->pos_sector;
   pChunk->pSectorNeg = pWall->neg_sector;
   pChunk->side = side;
   pChunk->zBias = ZBIAS_MASK;

//   if (bNoVTile)
//      pChunk->flags |= D3DRENDER_NOCULL;

   pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

   pChunk->pMaterialFctn = &D3DMaterialMaskChunk;

   for (vertex = 0; vertex < 4; vertex++)
   {
      pChunk->xyz[vertex].x = xyz[vertex].x;
      pChunk->xyz[vertex].y = xyz[vertex].y;
      pChunk->xyz[vertex].z = xyz[vertex].z;

      pChunk->st0[vertex].s = stBase[vertex].s;
      pChunk->st0[vertex].t = stBase[vertex].t;

      pChunk->bgra[vertex].b = bgra[vertex].b;
      pChunk->bgra[vertex].g = bgra[vertex].g;
      pChunk->bgra[vertex].r = bgra[vertex].r;
      pChunk->bgra[vertex].a = bgra[vertex].a;
   }

   pChunk->indices[0] = 1;
   pChunk->indices[1] = 2;
   pChunk->indices[2] = 0;
   pChunk->indices[3] = 3;

   if (bNoVTile)
   {
      if (side > 0)
      {
         pSideDef = pWall->pos_sidedef;

         xyz[0].x = pWall->x0;
         xyz[3].x = pWall->x1;
         xyz[1].x = pWall->x0;
         xyz[2].x = pWall->x1;

         xyz[0].y = pWall->y0;
         xyz[3].y = pWall->y1;
         xyz[1].y = pWall->y0;
         xyz[2].y = pWall->y1;

         xyz[1].z = xyz[0].z;
         xyz[2].z = xyz[3].z;

         switch (type)
         {
            case D3DRENDER_WALL_NORMAL:
            {
               xyz[0].z = (float)pWall->z2;
               xyz[3].z = (float)pWall->zz2;
            }
            break;

            case D3DRENDER_WALL_BELOW:
            {
               xyz[0].z = (float)pWall->z1;
               xyz[3].z = (float)pWall->zz1;
            }
            break;

            case D3DRENDER_WALL_ABOVE:
            {
               xyz[0].z = (float)pWall->z3;
               xyz[3].z = (float)pWall->zz3;
            }
            break;

            default:
            break;
         }
      }
      else if (side < 0)
      {
         pSideDef = pWall->neg_sidedef;

         xyz[0].x = pWall->x1;
         xyz[3].x = pWall->x0;
         xyz[1].x = pWall->x1;
         xyz[2].x = pWall->x0;

         xyz[0].y = pWall->y1;
         xyz[3].y = pWall->y0;
         xyz[1].y = pWall->y1;
         xyz[2].y = pWall->y0;

         xyz[1].z = xyz[0].z;
         xyz[2].z = xyz[3].z;

         switch (type)
         {
            case D3DRENDER_WALL_NORMAL:
            {
               xyz[0].z = (float)pWall->zz2;
               xyz[3].z = (float)pWall->z2;
            }
            break;

            case D3DRENDER_WALL_BELOW:
            {
               xyz[0].z = (float)pWall->zz1;
               xyz[3].z = (float)pWall->z1;
            }
            break;

            case D3DRENDER_WALL_ABOVE:
            {
               xyz[0].z = (float)pWall->zz3;
               xyz[3].z = (float)pWall->z3;
            }
            break;

            default:
            break;
         }
      }
      else
         assert(0);

      pPacket = D3DRenderPacketFindMatch(pPool, gpNoLookThrough, NULL, 0, 0, 0);
      if (NULL == pPacket)
         return;
      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);

      flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->flags = flags;
      pChunk->pSideDef = pSideDef;
      pChunk->pSectorPos = pWall->pos_sector;
      pChunk->pSectorNeg = pWall->neg_sector;
      pChunk->side = side;
      pChunk->zBias = ZBIAS_WORLD + 1;

      if (bNoVTile)
         pChunk->flags |= D3DRENDER_NOCULL;

      pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

      pChunk->pMaterialFctn = &D3DMaterialMaskChunk;

      for (vertex = 0; vertex < 4; vertex++)
      {
         pChunk->xyz[vertex].x = xyz[vertex].x;
         pChunk->xyz[vertex].y = xyz[vertex].y;
         pChunk->xyz[vertex].z = xyz[vertex].z;

         pChunk->st0[vertex].s = stBase[vertex].s - pWall->scrollS;
         pChunk->st0[vertex].t = stBase[vertex].t + pWall->scrollT;

         pChunk->bgra[vertex].b = bgra[vertex].b;
         pChunk->bgra[vertex].g = bgra[vertex].g;
         pChunk->bgra[vertex].r = bgra[vertex].r;
         pChunk->bgra[vertex].a = bgra[vertex].a;
      }

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }
}

void D3DRenderFloorMaskAdd(BSPnode *pNode, d3d_render_pool_new *pPool, Bool bDynamic)
{
   Sector      *pSector = pNode->u.leaf.sector;
   int         vertex;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   // Only called after D3DRenderPacketFloorAdd, which updates data.
   // if (!pNode->seenFloorThisFrame)
   //   D3DRenderFloorUpdate(pNode, NULL, pNode->floor_xyz, NULL, pNode->floor_bgra);

   pPacket = D3DRenderPacketFindMatch(pPool, gpNoLookThrough, NULL, 0, 0, 0);
   if (NULL == pPacket)
      return;
   pChunk = D3DRenderChunkNew(pPacket);
   assert(pChunk);

//   pChunk->numIndices = (pNode->u.leaf.poly.npts - 2) * 3;
   pChunk->numVertices = pNode->u.leaf.poly.npts;
   pChunk->numIndices = pChunk->numVertices;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   pChunk->pSector = pSector;
   pChunk->zBias = ZBIAS_MASK;
   pChunk->flags |= D3DRENDER_FLOOR;

   pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

   pChunk->pMaterialFctn = &D3DMaterialMaskChunk;

   for (vertex = 0; vertex < pNode->u.leaf.poly.npts; vertex++)
   {
      pChunk->xyz[vertex].x = pNode->floor_xyz[vertex].x;
      pChunk->xyz[vertex].y = pNode->floor_xyz[vertex].y;
      pChunk->xyz[vertex].z = pNode->floor_xyz[vertex].z;

      pChunk->bgra[vertex].b = pNode->floor_bgra[vertex].b;
      pChunk->bgra[vertex].g = pNode->floor_bgra[vertex].g;
      pChunk->bgra[vertex].r = pNode->floor_bgra[vertex].r;
      pChunk->bgra[vertex].a = pNode->floor_bgra[vertex].a;
   }

   {
      u_int   index;
      int   first, last;

      first = 1;
      last = pChunk->numVertices - 1;

      pChunk->indices[0] = 0;
      pChunk->indices[1] = last--;
      pChunk->indices[2] = first++;

      for (index = 3; index < pChunk->numIndices; first++, last--, index += 2)
      {
         pChunk->indices[index] = last;
         pChunk->indices[index + 1] = first;
      }
   }
}

void D3DRenderCeilingMaskAdd(BSPnode *pNode, d3d_render_pool_new *pPool, Bool bDynamic)
{
   Sector      *pSector = pNode->u.leaf.sector;
   int         vertex;
   int         left, top;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   left = top = 0;

   // Only called after D3DRenderPacketCeilingAdd, which updates data.
   // if (!pNode->seenCeilThisFrame)
   //   D3DRenderCeilingUpdate(pNode, NULL, pNode->ceiling_xyz, NULL, pNode->ceiling_bgra);

   pPacket = D3DRenderPacketFindMatch(pPool, gpNoLookThrough, NULL, 0, 0, 0);
   if (NULL == pPacket)
      return;
   pChunk = D3DRenderChunkNew(pPacket);
   assert(pChunk);

//   pChunk->numIndices = (pNode->u.leaf.poly.npts - 2) * 3;
   pChunk->numVertices = pNode->u.leaf.poly.npts;
   pChunk->numIndices = pChunk->numVertices;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   pChunk->pSector = pSector;
   pChunk->zBias = ZBIAS_MASK;
   pChunk->flags |= D3DRENDER_CEILING;

   pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

   pChunk->pMaterialFctn = &D3DMaterialMaskChunk;

   for (vertex = 0; vertex < pNode->u.leaf.poly.npts; vertex++)
   {
      pChunk->xyz[vertex].x = pNode->ceiling_xyz[vertex].x;
      pChunk->xyz[vertex].y = pNode->ceiling_xyz[vertex].y;
      pChunk->xyz[vertex].z = pNode->ceiling_xyz[vertex].z;

      pChunk->bgra[vertex].b = pNode->ceiling_bgra[vertex].b;
      pChunk->bgra[vertex].g = pNode->ceiling_bgra[vertex].g;
      pChunk->bgra[vertex].r = pNode->ceiling_bgra[vertex].r;
      pChunk->bgra[vertex].a = pNode->ceiling_bgra[vertex].a;
   }

   {
      u_int   index;
      int   first, last;

      first = 1;
      last = pChunk->numVertices - 1;

      pChunk->indices[0] = 0;
      pChunk->indices[1] = first++;
      pChunk->indices[2] = last--;

      for (index = 3; index < pChunk->numIndices; first++, last--, index += 2)
      {
         pChunk->indices[index] = first;
         pChunk->indices[index + 1] = last;
      }
   }

   if ((pSector->sloped_ceiling == NULL) && (pSector->ceiling_height !=
      current_room.sectors[0].ceiling_height))
   {
      int   vertex, i;

      pPacket = D3DRenderPacketFindMatch(pPool, gpDLightOrange, NULL, 0, 0, 0);
      if (NULL == pPacket)
         return;
      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);

      pPacket->pMaterialFctn = &D3DMaterialWorldPacket;

      for (i = 0, vertex = 0; i < pNode->u.leaf.poly.npts; i++)
      {
         if (vertex >= MAX_NPTS)
         {
            pChunk = D3DRenderChunkNew(pPacket);
            vertex = 0;
            pChunk->numVertices = 20;
            pChunk->numIndices = pChunk->numVertices;
            pChunk->numPrimitives = pChunk->numVertices - 2;
            pChunk->pSector = pSector;
            pChunk->zBias = 0;
            pChunk->flags = D3DRENDER_NOCULL;

            pChunk->pMaterialFctn = &D3DMaterialMaskChunk;
         }

         pChunk->xyz[vertex].x = pNode->ceiling_xyz[i].x;
         pChunk->xyz[vertex].y = pNode->ceiling_xyz[i].y;
         pChunk->xyz[vertex].z = pNode->ceiling_xyz[i].z;

         pChunk->bgra[vertex].b = pNode->ceiling_bgra[i].b;
         pChunk->bgra[vertex].g = pNode->ceiling_bgra[i].g;
         pChunk->bgra[vertex].r = pNode->ceiling_bgra[i].r;
         pChunk->bgra[vertex].a = pNode->ceiling_bgra[i].a;

         pChunk->indices[vertex] = vertex;

         vertex++;

         pChunk->xyz[vertex].x = pNode->ceiling_xyz[i].x;
         pChunk->xyz[vertex].y = pNode->ceiling_xyz[i].y;
         pChunk->xyz[vertex].z = 65535;

         pChunk->bgra[vertex].b = pNode->ceiling_bgra[i].b;
         pChunk->bgra[vertex].g = pNode->ceiling_bgra[i].g;
         pChunk->bgra[vertex].r = pNode->ceiling_bgra[i].r;
         pChunk->bgra[vertex].a = pNode->ceiling_bgra[i].a;

         pChunk->indices[vertex] = vertex;

         vertex++;
      }

      pChunk->numVertices = vertex - 1;
      pChunk->numIndices = pChunk->numVertices;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->pSector = pSector;
      pChunk->zBias = 0;
      pChunk->flags = D3DRENDER_NOCULL;

      pChunk->pMaterialFctn = &D3DMaterialMaskChunk;
   }
}

Bool D3DComputePlayerOverlayArea(PDIB pdib, char hotspot, AREA *obj_area)
{
   float   screenW, screenH;

   if (stretchfactor == 2)
   {
      screenW = (float)(gD3DRect.right - gD3DRect.left) / (float)(MAXX * (stretchfactor * 1.75f));
      screenH = (float)(gD3DRect.bottom - gD3DRect.top) / (float)(MAXY * (stretchfactor * 2.25f));
   }
   else
   {
      screenW = (float)(gD3DRect.right - gD3DRect.left) / (float)(MAXX * (stretchfactor * 1.25f));
      screenH = (float)(gD3DRect.bottom - gD3DRect.top) / (float)(MAXY * (stretchfactor * 1.5f));
   }

   if (hotspot < 1 || hotspot > HOTSPOT_PLAYER_MAX)
   {
      debug(("ComputePlayerOverlayArea found hotspot out of range (%d).\n", (int) hotspot));
      return False;
   }

   // Find x position
   switch (hotspot)
   {
   case HOTSPOT_NW:
   case HOTSPOT_W:
   case HOTSPOT_SW:
      obj_area->x = 0;
      break;

   case HOTSPOT_SE:
   case HOTSPOT_E:
   case HOTSPOT_NE:
      obj_area->x = gScreenWidth - DibWidth(pdib) / (float)screenW;
      break;

   case HOTSPOT_N:
   case HOTSPOT_S:
   case HOTSPOT_CENTER:
      obj_area->x = (gScreenWidth - DibWidth(pdib) / (float)screenW) / 2;
      break;
   }

   // Find y position
   switch (hotspot)
   {
   case HOTSPOT_NW:
   case HOTSPOT_N:
   case HOTSPOT_NE:
      obj_area->y = 0;
      break;

   case HOTSPOT_SW:
   case HOTSPOT_S:
   case HOTSPOT_SE:
      obj_area->y = gScreenHeight - DibHeight(pdib) / (float)screenH;
      break;

   case HOTSPOT_W:
   case HOTSPOT_E:
   case HOTSPOT_CENTER:
      obj_area->y = (gScreenHeight - DibHeight(pdib) / (float)screenH) / 2;
      break;
   }

   obj_area->x += DibXOffset(pdib) / (float)screenW;
   obj_area->y += DibYOffset(pdib) / (float)screenH;
   obj_area->cx = DibWidth(pdib) / (float)screenW;
   obj_area->cy = DibHeight(pdib) / (float)screenH;
   return True;
}

void D3DRenderNamesDraw3D(d3d_render_cache_system *pCacheSystem, d3d_render_pool_new *pPool,
                  room_type *room, Draw3DParams *params, font_3d *pFont)
{
   D3DMATRIX         mat, xForm, trans;
   int               strLen, sector_flags, offset;
   room_contents_node   *pRNode;
   list_type         list;
   long            dx, dy, angle, top, bottom;
   PDIB            pDib;
   custom_xyz         vector;
   custom_st         st[4];
   custom_bgra         bgra;
   float            lastDistance, width, height, x, z, depth, distance;
   char            *pName, *ptr;
   TBYTE            c;
   COLORREF         fg_color;
   Color            color;
   BYTE            *palette;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   // base objects
   for (list = room->contents; list != NULL; list = list->next)
   {
      float glyph_scale = 255;

      pRNode = (room_contents_node *)list->data;

      if (pRNode->obj.id == player.id
         || !(pRNode->obj.flags & OF_DISPLAY_NAME)
         || (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
         || !config.draw_player_names && (pRNode->obj.flags & OF_PLAYER)
         || !config.draw_npc_names && (pRNode->obj.flags & OF_NPC)
         || !config.draw_sign_names && (pRNode->obj.flags & OF_SIGN))
         continue;

      vector.x = pRNode->motion.x - player.x;
      vector.y = pRNode->motion.y - player.y;

      distance = (vector.x * vector.x) + (vector.y * vector.y);
      distance = (float)sqrt((double)distance);
      if (pRNode->obj.flags & OF_SIGN)
      {
         distance = max(distance, 2 * FINENESS);
         distance = min(distance, 4 * FINENESS);
      }
      else
      {
         if (distance <= 0)
            distance = 1;
         else if (distance >= MAX_NAME_DISTANCE
            && !(pRNode->obj.id != INVALID_ID
               && pRNode->obj.id == GetUserTargetID()
               && (pRNode->obj.drawingtype != DRAWFX_INVISIBLE)))
            continue;
      }

      pDib = GetObjectPdib(pRNode->obj.icon_res, 0, pRNode->obj.animate->group);

      if (NULL == pDib)
         continue;

      dx = pRNode->motion.x - params->viewer_x;
      dy = pRNode->motion.y - params->viewer_y;

      angle = (pRNode->angle - intATan2(-dy,-dx)) & NUMDEGREES_MASK;

      pName = LookupNameRsc(pRNode->obj.name_res);
      strLen = strlen(pName);

      angle = pRNode->angle - (params->viewer_angle + 3072);

      if (angle < -4096)
         angle += 4096;

      /* Make sure that object is above the floor. */
      if (!GetRoomHeight(room->tree, &top, &bottom, &sector_flags, pRNode->motion.x, pRNode->motion.y))
         continue;

      // No depth for override flags.
      if (ROOM_OVERRIDE_MASK & GetRoomFlags())
      {
         depth = 0.0f;
      }
      else
      {
         // Set object depth based on "depth" sector flags
         depth = sector_depths[SectorDepth(sector_flags)];
      }

      //         z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;

//         z += ((float)pRNode->boundingHeightAdjust * 4.0f);// / 1.66f;

      MatrixTranslate(&mat, (float)pRNode->motion.x, (float)max(bottom,
         pRNode->motion.z) - depth +
         (((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f) +
         ((float)pRNode->boundingHeightAdjust * 4.0f), (float)pRNode->motion.y);
      MatrixMultiply(&xForm, &mPlayerHeadingTrans, &mat);

      fg_color = GetPlayerNameColor(&pRNode->obj, pName);

      // Some names never grow darker, they use PALETTEINDEX().
      if (HIBYTE(HIWORD(fg_color)) == HIBYTE(HIWORD(PALETTEINDEX(0))))
      {
         //XXX: normally, SetTextColor() works with PALETTEINDEX() types fine,
         //     but not here for unknown reason
         //     so we convert to our base_palette[] PALETTERGB() type.
         //
         color = base_palette[LOBYTE(LOWORD(fg_color))];
      }
      else
      {
         // Draw name with color that fades with distance, just like object
         if (pRNode->obj.flags & (OF_FLICKERING | OF_FLASHING))
         {
            //palette = GetLightPalette(object->distance, object->light, FINENESS,GetFlicker(r));
            palette = GetLightPalette(D3DRENDER_LIGHT_DISTANCE, 63, FINENESS,0);
         }
         else
         {
            //palette = GetLightPalette(object->distance, object->light, FINENESS,0);
            palette = GetLightPalette(D3DRENDER_LIGHT_DISTANCE, 63, FINENESS,0);
         }
         color = base_palette[palette[GetClosestPaletteIndex(fg_color)]];
         D3DObjectLightingCalc(room, pRNode, &bgra, 0);

         glyph_scale = max(bgra.b, bgra.g);
         glyph_scale = max(glyph_scale, bgra.r);
      }

      bgra.r = color.red * glyph_scale / 255;
      bgra.g = color.green * glyph_scale / 255;
      bgra.b = color.blue * glyph_scale / 255;
      bgra.a = COLOR_MAX;
      
      lastDistance = 0;

      for (offset = 0; offset <= 1; offset++)
      {
         x = 0.0f;
         z = 0;
//         z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;

//         z += ((float)pRNode->boundingHeightAdjust * 4.0f);// / 1.66f;

         ptr = pName;

         while (c = *ptr++)
         {
            x += (pFont->texST[c - 32][1].s - pFont->texST[c - 32][0].s) *
               pFont->texWidth / pFont->texScale * (distance / FINENESS);
         }

         ptr = pName;

         while (c = *ptr++)
         {
            int   i;

            // flip t values since bmps are upside down
            st[0].s = pFont->texST[c - 32][0].s;
            st[0].t = pFont->texST[c - 32][1].t;
            st[1].s = pFont->texST[c - 32][0].s;
            st[1].t = pFont->texST[c - 32][0].t;
            st[2].s = pFont->texST[c - 32][1].s;
            st[2].t = pFont->texST[c - 32][0].t;
            st[3].s = pFont->texST[c - 32][1].s;
            st[3].t = pFont->texST[c - 32][1].t;

            width = (st[2].s - st[0].s) * pFont->texWidth * 2.0f / pFont->texScale *
               (distance / FINENESS);
            height = (st[0].t - st[2].t) * pFont->texHeight * 2.0f / pFont->texScale *
               (distance / FINENESS);

            pPacket = D3DRenderPacketFindMatch(pPool, pFont->pTexture, NULL, 0, 0, 0);
            if (NULL == pPacket)
               return;
            pChunk = D3DRenderChunkNew(pPacket);
            assert(pChunk);

            pChunk->flags = pRNode->obj.flags;
            pChunk->numIndices = 4;
            pChunk->numVertices = 4;
            pChunk->numPrimitives = pChunk->numVertices - 2;

            // zBias is scaled way differently in Dx9
            // try to put names as much in the front as possible
            if (offset == 0)
               pChunk->zBias = 255;
            else
               pChunk->zBias = 254;

            if (offset)
            {
               //MatrixScale(&scale, 1.01f);
               MatrixTranslate(&trans, -30.0f * distance / MAX_NAME_DISTANCE,
                  -30.0f * distance / MAX_NAME_DISTANCE, 0);
               MatrixMultiply(&pChunk->xForm, &trans, &xForm);
            }
            else
               MatrixCopy(&pChunk->xForm, &xForm);

            pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
            pChunk->pMaterialFctn = &D3DMaterialObjectChunk;

            pChunk->xyz[0].x = x;
            pChunk->xyz[0].y = 0;
            pChunk->xyz[0].z = z;

            pChunk->st0[0].s = st[0].s;
            pChunk->st0[0].t = st[0].t;

            pChunk->xyz[1].x = x;
            pChunk->xyz[1].y = 0;
            pChunk->xyz[1].z = z + height;

            pChunk->st0[1].s = st[1].s;
            pChunk->st0[1].t = st[1].t;

            pChunk->xyz[2].x = x - width;
            pChunk->xyz[2].y = 0;
            pChunk->xyz[2].z = z + height;

            pChunk->st0[2].s = st[2].s;
            pChunk->st0[2].t = st[2].t;

            pChunk->xyz[3].x = x - width;
            pChunk->xyz[3].y = 0;
            pChunk->xyz[3].z = z;

            pChunk->st0[3].s = st[3].s;
            pChunk->st0[3].t = st[3].t;

            if (offset != 0)
            {
               bgra.r = 0;
               bgra.g = 0;
               bgra.b = 0;
            }

            for (i = 0; i < 4; i++)
            {
               pChunk->bgra[i].r = bgra.r;
               pChunk->bgra[i].g = bgra.g;
               pChunk->bgra[i].b = bgra.b;
               pChunk->bgra[i].a = bgra.a;
            }

            pChunk->indices[0] = 1;
            pChunk->indices[1] = 2;
            pChunk->indices[2] = 0;
            pChunk->indices[3] = 3;

            if (pRNode->obj.drawingtype == DRAWFX_SECONDTRANS)
            {
               pChunk->alphaRef = TEMP_ALPHA_REF;
               pChunk->xLat0 = 0;
               pChunk->xLat1 = pRNode->obj.secondtranslation;
            }
            else if (pRNode->obj.drawingtype == DRAWFX_DOUBLETRANS)
            {
               pChunk->alphaRef = TEMP_ALPHA_REF;
               pChunk->xLat0 = pRNode->obj.translation;
               pChunk->xLat1 = pRNode->obj.secondtranslation;
            }
            else
            {
               if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
                  pChunk->alphaRef = D3DRENDER_TRANS25 - 1;
               else if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
                  pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
               else if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
                  pChunk->alphaRef = D3DRENDER_TRANS75 - 1;
               else if (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
                  pChunk->alphaRef = 1;
               else if (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
                  pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
               else if (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)
                  pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
               else
                  pChunk->alphaRef = TEMP_ALPHA_REF;
               pChunk->xLat0 = pRNode->obj.translation;
               pChunk->xLat1 = 0;
            }

            x -= width;
         }
      }
   }
}

void D3DRenderQuestInfoDraw3D(d3d_render_cache_system *pCacheSystem, d3d_render_pool_new *pPool,
   room_type *room, Draw3DParams *params, font_3d *pFont)
{
   D3DMATRIX         mat, xForm, trans;
   int               strLen, sector_flags, offset;
   room_contents_node   *pRNode;
   list_type         list;
   long            dx, dy, angle, top, bottom;
   PDIB            pDib;
   custom_xyz         vector;
   custom_st         st[4];
   custom_bgra         bgra;
   float            width, height, x, z, depth, distance;
   char            *pText, *ptr;
   TBYTE            c;
   COLORREF         fg_color;
   Color            color;
   BYTE            *palette;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   pText = "!";
   strLen = strlen(pText);
   float texScale = pFont->texScale * 0.7f;

   for (list = room->contents; list != NULL; list = list->next)
   {
      float glyph_scale = 255;

      pRNode = (room_contents_node *)list->data;

      if (pRNode->obj.id == player.id
         || (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
         || !((pRNode->obj.flags & OF_NPCACTIVEQUEST)
            || (pRNode->obj.flags & OF_NPCHASQUESTS)
            || (pRNode->obj.flags & OF_MOBKILLQUEST)))
         continue;

      /////////////////////////////////////////////////////////////////////////
      // Distance, angle, get bitmap
      /////////////////////////////////////////////////////////////////////////

      vector.x = pRNode->motion.x - player.x;
      vector.y = pRNode->motion.y - player.y;

      distance = (vector.x * vector.x) + (vector.y * vector.y);
      distance = sqrtf(distance);

      if (distance <= 0)
         distance = 1;
      else if (distance >= MAX_NAME_DISTANCE
         && !(pRNode->obj.id != INVALID_ID
            && pRNode->obj.id == GetUserTargetID()
            && (pRNode->obj.drawingtype != DRAWFX_INVISIBLE)))
         continue;

      pDib = GetObjectPdib(pRNode->obj.icon_res, 0, pRNode->obj.animate->group);

      if (NULL == pDib)
         continue;

      dx = pRNode->motion.x - params->viewer_x;
      dy = pRNode->motion.y - params->viewer_y;

      angle = (pRNode->angle - intATan2(-dy, -dx)) & NUMDEGREES_MASK;

      angle = pRNode->angle - (params->viewer_angle + 3072);

      if (angle < -4096)
         angle += 4096;

      /////////////////////////////////////////////////////////////////////////
      // Room height/depth
      /////////////////////////////////////////////////////////////////////////

      /* Make sure that object is above the floor. */
      if (!GetRoomHeight(room->tree, &top, &bottom, &sector_flags, pRNode->motion.x, pRNode->motion.y))
         continue;

      // No depth for override flags.
      if (ROOM_OVERRIDE_MASK & GetRoomFlags())
      {
         depth = 0.0f;
      }
      else
      {
         // Set object depth based on "depth" sector flags
         depth = sector_depths[SectorDepth(sector_flags)];
      }

      /////////////////////////////////////////////////////////////////////////
      // Set up text positioning
      /////////////////////////////////////////////////////////////////////////

      float matdy = (float)max(bottom, pRNode->motion.z) - depth +
         (((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f) +
         ((float)pRNode->boundingHeightAdjust * 4.0f);
      matdy += ((pFont->texST[pText[0] - 32][1].t - pFont->texST[pText[0] - 32][0].t)
        * pFont->texHeight * 2.0f / texScale * (distance / FINENESS));

      MatrixTranslate(&mat, (float)pRNode->motion.x, matdy, (float)pRNode->motion.y);
      MatrixMultiply(&xForm, &mPlayerHeadingTrans, &mat);

      /////////////////////////////////////////////////////////////////////////
      // Text coloring
      /////////////////////////////////////////////////////////////////////////

      fg_color = GetQuestInfoColor(&pRNode->obj);

      // Some names never grow darker, they use PALETTEINDEX().
      if (HIBYTE(HIWORD(fg_color)) == HIBYTE(HIWORD(PALETTEINDEX(0))))
      {
         //XXX: normally, SetTextColor() works with PALETTEINDEX() types fine,
         //     but not here for unknown reason
         //     so we convert to our base_palette[] PALETTERGB() type.
         //
         color = base_palette[LOBYTE(LOWORD(fg_color))];
      }
      else
      {
         // Draw name with color that fades with distance, just like object
         if (pRNode->obj.flags & (OF_FLICKERING | OF_FLASHING))
         {
            //palette = GetLightPalette(object->distance, object->light, FINENESS,GetFlicker(r));
            palette = GetLightPalette(D3DRENDER_LIGHT_DISTANCE, 63, FINENESS, 0);
         }
         else
         {
            //palette = GetLightPalette(object->distance, object->light, FINENESS,0);
            palette = GetLightPalette(D3DRENDER_LIGHT_DISTANCE, 63, FINENESS, 0);
         }
         color = base_palette[palette[GetClosestPaletteIndex(fg_color)]];
         D3DObjectLightingCalc(room, pRNode, &bgra, 0);

         glyph_scale = max(bgra.b, bgra.g);
         glyph_scale = max(glyph_scale, bgra.r);
      }

      bgra.r = color.red * glyph_scale / 255;
      bgra.g = color.green * glyph_scale / 255;
      bgra.b = color.blue * glyph_scale / 255;
      bgra.a = COLOR_MAX;

      /////////////////////////////////////////////////////////////////////////
      // Room height/depth
      /////////////////////////////////////////////////////////////////////////
      for (offset = 0; offset <= 1; offset++)
      {
         x = 0.0f;
         z = 0;
         ptr = pText;

         while (c = *ptr++)
         {
            x += (pFont->texST[c - 32][1].s - pFont->texST[c - 32][0].s) *
               pFont->texWidth / texScale * (distance / FINENESS);
         }

         ptr = pText;

         while (c = *ptr++)
         {
            int   i;

            // flip t values since bmps are upside down
            st[0].s = pFont->texST[c - 32][0].s;
            st[0].t = pFont->texST[c - 32][1].t;
            st[1].s = pFont->texST[c - 32][0].s;
            st[1].t = pFont->texST[c - 32][0].t;
            st[2].s = pFont->texST[c - 32][1].s;
            st[2].t = pFont->texST[c - 32][0].t;
            st[3].s = pFont->texST[c - 32][1].s;
            st[3].t = pFont->texST[c - 32][1].t;

            width = (st[2].s - st[0].s) * pFont->texWidth * 2.0f / texScale *
               (distance / FINENESS);
            height = (st[0].t - st[2].t) * pFont->texHeight * 2.0f / texScale *
               (distance / FINENESS);

            pPacket = D3DRenderPacketFindMatch(pPool, pFont->pTexture, NULL, 0, 0, 0);
            if (NULL == pPacket)
               return;
            pChunk = D3DRenderChunkNew(pPacket);
            assert(pChunk);

            pChunk->flags = pRNode->obj.flags;
            pChunk->numIndices = 4;
            pChunk->numVertices = 4;
            pChunk->numPrimitives = pChunk->numVertices - 2;

            // zBias is scaled way differently in Dx9
            // try to put names as much in the front as possible
            if (offset == 0)
               pChunk->zBias = 255;
            else
               pChunk->zBias = 254;

            if (offset)
            {
               //MatrixScale(&scale, 1.01f);
               MatrixTranslate(&trans, -30.0f * distance / MAX_NAME_DISTANCE,
                  -30.0f * distance / MAX_NAME_DISTANCE, 0);
               MatrixMultiply(&pChunk->xForm, &trans, &xForm);
            }
            else
               MatrixCopy(&pChunk->xForm, &xForm);

            pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
            pChunk->pMaterialFctn = &D3DMaterialObjectChunk;

            pChunk->xyz[0].x = x;
            pChunk->xyz[0].y = 0;
            pChunk->xyz[0].z = z;

            pChunk->st0[0].s = st[0].s;
            pChunk->st0[0].t = st[0].t;

            pChunk->xyz[1].x = x;
            pChunk->xyz[1].y = 0;
            pChunk->xyz[1].z = z + height;

            pChunk->st0[1].s = st[1].s;
            pChunk->st0[1].t = st[1].t;

            pChunk->xyz[2].x = x - width;
            pChunk->xyz[2].y = 0;
            pChunk->xyz[2].z = z + height;

            pChunk->st0[2].s = st[2].s;
            pChunk->st0[2].t = st[2].t;

            pChunk->xyz[3].x = x - width;
            pChunk->xyz[3].y = 0;
            pChunk->xyz[3].z = z;

            pChunk->st0[3].s = st[3].s;
            pChunk->st0[3].t = st[3].t;

            if (offset != 0)
            {
               bgra.r = 0;
               bgra.g = 0;
               bgra.b = 0;
            }

            for (i = 0; i < 4; i++)
            {
               pChunk->bgra[i].r = bgra.r;
               pChunk->bgra[i].g = bgra.g;
               pChunk->bgra[i].b = bgra.b;
               pChunk->bgra[i].a = bgra.a;
            }

            pChunk->indices[0] = 1;
            pChunk->indices[1] = 2;
            pChunk->indices[2] = 0;
            pChunk->indices[3] = 3;

            pChunk->alphaRef = TEMP_ALPHA_REF;
            pChunk->xLat0 = pRNode->obj.translation;
            pChunk->xLat1 = 0;

            x -= width;
         }
      }
   }
}

void D3DRenderResizeDisplay(int left, int top, int right, int bottom)
{
   gD3DRect.left = left;
   gD3DRect.right = left + right;
   gD3DRect.top = top;
   gD3DRect.bottom = top + bottom;
}

void D3DRenderEnableToggle(void)
{
   gD3DEnabled = 1 - gD3DEnabled;

   if (gD3DEnabled)
   {
      memset(gBits, 0, MAXX * MAXY);
      memset(gBufferBits, 0, MAXX * 2 * MAXY * 2);
   }
}

int D3DRenderIsEnabled(void)
{
   return gD3DEnabled;
}

void D3DRenderLMapsBuild(void)
{
   D3DLOCKED_RECT   lockedRect;
   unsigned char   *pBits = NULL;
   int            width, height;

   // white glow
   IDirect3DDevice9_CreateTexture(gpD3DDevice, 32, 32, 1, 0,
      D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &gpDLightWhite, NULL);

   IDirect3DTexture9_LockRect(gpDLightWhite, 0, &lockedRect, NULL, 0);

   pBits = (unsigned char *)lockedRect.pBits;

   for (height = 0; height < 32; height++)
   {
      for (width = 0; width < 32; width++)
      {
         float scale = sqrtf((height - 16) * (height - 16) + (width - 16) * (width - 16));
         scale = 16.0f - scale;
         scale = max(scale, 0);
         scale /= 16.0f;

         if ((height == 0) || (height == 31) ||
            (width == 0) || (width == 31))
            scale = 0;

         *(pBits++) = 255 * scale;
         *(pBits++) = 255 * scale;
         *(pBits++) = 255 * scale;
         *(pBits++) = 255;
      }
   }

   IDirect3DTexture9_UnlockRect(gpDLightWhite, 0);

   // orange glow
   IDirect3DDevice9_CreateTexture(gpD3DDevice, 32, 32, 1, 0,
      D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &gpDLightOrange, NULL);

   IDirect3DTexture9_LockRect(gpDLightOrange, 0, &lockedRect, NULL, 0);

   pBits = (unsigned char *)lockedRect.pBits;

   for (height = 0; height < 32; height++)
   {
      for (width = 0; width < 32; width++)
      {
         float scale = sqrtf((height - 16) * (height - 16) + (width - 16) * (width - 16));

         scale = 16.0f - scale;
         scale = max(scale, 0);
         scale /= 16.0f;

         if ((height == 0) || (height == 31) ||
            (width == 0) || (width == 31))
            scale = 0;

         *(pBits++) = LIGHTMAP_B * scale;
         *(pBits++) = LIGHTMAP_G * scale;
         *(pBits++) = LIGHTMAP_R * scale;
         *(pBits++) = COLOR_MAX * scale;
      }
   }

   IDirect3DTexture9_UnlockRect(gpDLightOrange, 0);

   // no look through texture
   IDirect3DDevice9_CreateTexture(gpD3DDevice, 1, 1, 1, 0,
                                  D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &gpNoLookThrough, NULL);

   IDirect3DTexture9_LockRect(gpNoLookThrough, 0, &lockedRect, NULL, 0);

   pBits = (unsigned char *)lockedRect.pBits;

   *(pBits++) = 0;
   *(pBits++) = 0;
   *(pBits++) = 0;
   *(pBits++) = 0;

   IDirect3DTexture9_UnlockRect(gpNoLookThrough, 0);
}

void D3DRenderFontInit(font_3d *pFont, HFONT hFont)
{
   D3DCAPS9      d3dCaps;
   HDC            hDC;
   HBITMAP         hbmBitmap;
   DWORD         *pBitmapBits;
   BITMAPINFO      bmi;
   long         x = 0;
   long         y = 0;
   TBYTE         str[2] = _T("x");
   TBYTE         c;
   SIZE         size;
   D3DLOCKED_RECT   d3dlr;
   BYTE         *pDstRow;
   WORD         *pDst16;
   BYTE         bAlpha;
   HRESULT         hr;

   pFont->fontHeight = GetFontHeight(hFont);
//   pFont->flags = flags;
   pFont->flags = 0;
   pFont->texScale = 1.0f;
   
   if (pFont->fontHeight > 40)
      pFont->texWidth = pFont->texHeight = 1024;
   else if (pFont->fontHeight > 20)
      pFont->texWidth = pFont->texHeight = 512;
   else
      pFont->texWidth = pFont->texHeight = 256;
   
   IDirect3DDevice9_GetDeviceCaps(gpD3DDevice, &d3dCaps);
  
   if (pFont->texWidth > (long) d3dCaps.MaxTextureWidth)
   {
      pFont->texScale = (float)pFont->texWidth / (float)d3dCaps.MaxTextureWidth;
      pFont->texHeight = pFont->texWidth = d3dCaps.MaxTextureWidth;
   }
  
   if (pFont->pTexture)
      IDirect3DDevice9_Release(pFont->pTexture);
   
   hr = IDirect3DDevice9_CreateTexture(
      gpD3DDevice, pFont->texWidth,
      pFont->texHeight, 1, 0, D3DFMT_A4R4G4B4,
      D3DPOOL_MANAGED, &pFont->pTexture, NULL);

   memset(&bmi.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
   bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth = (int)pFont->texWidth;
   bmi.bmiHeader.biHeight = -(int)pFont->texHeight;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biCompression = BI_RGB;
   bmi.bmiHeader.biBitCount = 32;
   
   hDC = CreateCompatibleDC(gBitsDC);
   hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (VOID**)&pBitmapBits, NULL, 0 );
   SetMapMode(hDC, MM_TEXT);
  
/*   nHeight = -MulDiv(pFont->fontHeight, (int)(GetDeviceCaps(hDC, LOGPIXELSY)
 * pFont->texScale), 72);
 dwBold = (pFont->flags & D3DFONT_BOLD) ? FW_BOLD : FW_NORMAL;
 dwItalic = (pFont->flags & D3DFONT_ITALIC) ? TRUE : FALSE;
 hFont = CreateFont(nHeight, 0, 0, 0, dwBold, dwItalic,
 FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
 VARIABLE_PITCH, pFont->strFontName);*/
  
   assert(hFont);
   
   SelectObject(hDC, hbmBitmap);
   SelectObject(hDC, hFont);
   
   // Set text properties
   SetTextColor(hDC, RGB(255,255,255));
   SetBkColor(hDC, 0);
   SetTextAlign(hDC, TA_TOP);
   
   for(c = 32; c < 256 && c > 31; c++ )
   {
      BOOL   temp;
      int left_offset, right_offset;

      str[0] = c;
      GetTextExtentPoint32(hDC, (LPCSTR)str, 1, &size);
      
      if (!GetCharABCWidths(hDC, c, c, &pFont->abc[c-32])) 
      {
         pFont->abc[c-32].abcA = 0;
         pFont->abc[c-32].abcB = size.cx;
         pFont->abc[c-32].abcC = 0;
      }
      
      left_offset = abs(pFont->abc[c-32].abcA);
      right_offset = abs(pFont->abc[c-32].abcC);
      size.cx = abs(pFont->abc[c-32].abcA) + pFont->abc[c-32].abcB + abs(pFont->abc[c-32].abcC);
      
      // Not spacing these out enough causes artefacts in the name graphics.
      // Add 2 pixels after each letter, used to be one.
      if (x + size.cx + 2 > pFont->texWidth) // old method: size.cx + 1
      {
         x  = 0;
         y += size.cy + 1;
      }

      temp = ExtTextOut(hDC, x + left_offset, y + 0, ETO_OPAQUE, NULL, (LPCSTR)str, 1, NULL);

      pFont->texST[c-32][0].s = ((FLOAT)(x+0)) / pFont->texWidth;
      pFont->texST[c-32][0].t = ((FLOAT)(y+0)) / pFont->texHeight;
      pFont->texST[c-32][1].s = ((FLOAT)(x+0 + size.cx)) / pFont->texWidth;
      pFont->texST[c-32][1].t = ((FLOAT)(y+0 + size.cy)) / pFont->texHeight;

      x += size.cx + 2; // old method: size.cx + 1
   }
   
   hr = IDirect3DTexture9_LockRect(pFont->pTexture, 0, &d3dlr, 0, 0);

   if (SUCCEEDED(hr))
   {
      pDstRow = (BYTE*)d3dlr.pBits;
   
      for(y = 0; y < pFont->texHeight; y++)
      {
         pDst16 = (WORD *)pDstRow;
         for(x = 0; x < pFont->texWidth; x++)
         {
            bAlpha = (BYTE)((pBitmapBits[pFont->texWidth * y + x] & 0xff) >> 4);
            if (bAlpha > 0)
            {
               *pDst16++ = (bAlpha << 12) | 0x0fff;
            }
            else
            {
               *pDst16++ = 0x0000;
            }
         }
         pDstRow += d3dlr.Pitch;
      }
   
      IDirect3DTexture9_UnlockRect(pFont->pTexture, 0);
   }
   
   DeleteObject(hbmBitmap);
   DeleteDC(hDC);
}

void D3DRenderSkyboxDraw(d3d_render_pool_new *pPool)
{
   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHABLENDENABLE, FALSE);

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &mPlayerHPMul);

//   XformMatrixPerspective(&mat, -PI / 4.0f, PI / 6.0f, 100.0f, 150000.0f);
//   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &mat);

   for (int i = 0; i < 6; ++i)
   {
      pPacket = D3DRenderPacketFindMatch(pPool, gpSkyboxTextures[gCurBackground][i], NULL, 0, 0, 0);
      if (NULL == pPacket)
         return;

      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);

      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pPacket->pMaterialFctn = &D3DMaterialWorldPacket;
      pChunk->pMaterialFctn = &D3DMaterialSkyboxChunk;
      pChunk->flags |= D3DRENDER_NOAMBIENT;

      // add xyz, st, and bgra data
      for (int j = 0; j < 4; ++j)
      {
         pChunk->xyz[j].x = gSkyboxXYZ[(i * 4 * 3) + (j * 3)];
         pChunk->xyz[j].z = gSkyboxXYZ[(i * 4 * 3) + (j * 3) + 1];
         pChunk->xyz[j].y = gSkyboxXYZ[(i * 4 * 3) + (j * 3) + 2];

         pChunk->st0[j].s = gSkyboxST[(i * 4 * 2) + (j * 2)];
         pChunk->st0[j].t = gSkyboxST[(i * 4 * 2) + (j * 2) + 1];

         pChunk->bgra[j].b = gSkyboxBGRA[(i * 4 * 4) + (j * 4)];// / 2.0f;
         pChunk->bgra[j].g = gSkyboxBGRA[(i * 4 * 4) + (j * 4) + 1];// / 2.0f;
         pChunk->bgra[j].r = gSkyboxBGRA[(i * 4 * 4) + (j * 4) + 2];// / 2.0f;
         pChunk->bgra[j].a = gSkyboxBGRA[(i * 4 * 4) + (j * 4) + 3];

      }
      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }
}

// UV coords reversed for rotated texture.
void D3DRenderWallUpdate(WallData *pWall, PDIB pDib, unsigned int *flags, custom_xyz *pXYZ,
   custom_st *pST, custom_bgra *pBGRA, unsigned int type, int side)
{
   Sidedef *pSideDef = NULL;
   int paletteIndex;
   BYTE light;

   if (pDib == NULL)
      return;

   // pos and neg sidedefs have their x and y coords reversed
   if (side > 0)
   {
      if (type == D3DRENDER_WALL_NORMAL)
         pWall->seen |= HR_SEENPOSNORMAL;
      else if (type == D3DRENDER_WALL_ABOVE)
         pWall->seen |= HR_SEENPOSABOVE;
      else if (type == D3DRENDER_WALL_BELOW)
         pWall->seen |= HR_SEENPOSBELOW;

      pSideDef = pWall->pos_sidedef;

      if (NULL == pWall->pos_sector)
         light = 0;
      else
         light = pWall->pos_sector->light;
   }
   else if (side < 0)
   {
      if (type == D3DRENDER_WALL_NORMAL)
         pWall->seen |= HR_SEENNEGNORMAL;
      else if (type == D3DRENDER_WALL_ABOVE)
         pWall->seen |= HR_SEENNEGABOVE;
      else if (type == D3DRENDER_WALL_BELOW)
         pWall->seen |= HR_SEENNEGBELOW;

      pSideDef = pWall->neg_sidedef;

      if (NULL == pWall->neg_sector)
         light = 0;
      else
         light = pWall->neg_sector->light;
   }
   else
   {
      debug(("Wall with 0 side!\n"));
      return;
   }


   if (pSideDef->animate != NULL && pSideDef->animate->animation == ANIMATE_SCROLL)
   {
      float invWidth = 1.0f / (float)pDib->width;
      float invHeight = 1.0f / (float)pDib->height;

      pWall->scrollT = (pSideDef->animate->u.scroll.xoffset * pDib->shrink * invHeight);
      pWall->scrollS = (pSideDef->animate->u.scroll.yoffset * pDib->shrink * invWidth);
   }
   else
   {
      pWall->scrollT = 0.0f;
      pWall->scrollS = 0.0f;
   }


   float a, b;
   int distX, distY, distance;
   long lightScale;
   long lo_end = FINENESS - shade_amount;

   for (int i = 0; i < 4; i++)
   {
      distX = pXYZ[i].x - player.x;
      distY = pXYZ[i].y - player.y;

      distance = DistanceGet(distX, distY);

      if (shade_amount != 0)
      {
         a = pWall->separator.a;
         b = pWall->separator.b;

         if (side < 0)
         {
            a = -a;
            b = -b;
         }

         lightScale = (long)(a * sun_vect.x +
            b * sun_vect.y) >> LOG_FINENESS;

         lightScale = (lightScale + FINENESS) >> 1; // map to 0 to 1 range

         lightScale = lo_end + ((lightScale * shade_amount) >> LOG_FINENESS);

         // Dx9.0 applies too much shading, so set a lower limit of 860 here for now.
         if (lightScale > FINENESS)
            lightScale = FINENESS;
         else if (lightScale < 0)
            lightScale = 0;
      }
      else
         lightScale = FINENESS;

      pWall->lightscale = lightScale;

      if (gD3DDriverProfile.bFogEnable)
         paletteIndex = GetLightPaletteIndex(FINENESS, light, lightScale, 0);
      else
         paletteIndex = GetLightPaletteIndex(distance, light, lightScale, 0);

      if (light <= 127)
         *flags |= D3DRENDER_NOAMBIENT;

      pBGRA[i].r = pBGRA[i].g = pBGRA[i].b = paletteIndex * COLOR_AMBIENT / 64;
      pBGRA[i].a = 255;
   }
}

// UV coords reversed for rotated texture.
void D3DRenderFloorUpdate(BSPnode *pNode, PDIB pDib, custom_xyz *pXYZ, custom_st *pST,
   custom_bgra *pBGRA)
{
   Sector *pSector = pNode->u.leaf.sector;
   int paletteIndex, distX, distY, distance;
   long lightscale;
   float inv64 = 0.0009765625f;

   pNode->seenFloorThisFrame = True;
   for (int count = 0; count < pNode->u.leaf.poly.npts; count++)
   {
      // xyz
      pXYZ[count].x = pNode->u.leaf.floor.xyz[count].x;
      pXYZ[count].y = pNode->u.leaf.floor.xyz[count].y;
      if (pSector->sloped_floor)
         pXYZ[count].z = pNode->u.leaf.floor.xyz[count].z;
      else
         pXYZ[count].z = (float)pSector->floor_height;

      // st
      if (pST)
      {
         pST[count].t = pNode->u.leaf.floor.st[count].t;
         pST[count].s = pNode->u.leaf.floor.st[count].s;

         if (pSector->animate != NULL
            && pSector->animate->animation == ANIMATE_SCROLL
            && pSector->flags & SF_SCROLL_FLOOR)
         {
            pST[count].t -= pSector->animate->u.scroll.yoffset;// * pDib->shrink;
            pST[count].s += pSector->animate->u.scroll.xoffset;// * pDib->shrink;
         }

         pST[count].s *= inv64;
         pST[count].t *= inv64;
      }
      // bgra
      distX = pXYZ[count].x - player.x;
      distY = pXYZ[count].y - player.y;

      lightscale = FINENESS;

      if (shade_amount != 0)
      {
         // floor is sloped, and it's marked as being steep enough to be eligible for
         // directional lighting
         if ((pNode->u.leaf.sector->sloped_floor != NULL) &&
            (pNode->u.leaf.sector->sloped_floor->flags & SLF_DIRECTIONAL))
         {
            long lo_end = FINENESS - shade_amount;

            // light scale is based on dot product of surface normal and sun vector
            lightscale = (long)(pNode->u.leaf.sector->sloped_floor->plane.a * sun_vect.x +
               pNode->u.leaf.sector->sloped_floor->plane.b * sun_vect.y +
               pNode->u.leaf.sector->sloped_floor->plane.c * sun_vect.z) >> LOG_FINENESS;

            lightscale = (lightscale + FINENESS) >> 1; // map to 0 to 1 range

            lightscale = lo_end + ((lightscale * shade_amount) >> LOG_FINENESS);

            if (lightscale > FINENESS)
               lightscale = FINENESS;
            else if (lightscale < 0)
               lightscale = 0;

            pNode->u.leaf.sector->sloped_floor->lightscale = lightscale;
         }
      }
      else
      { // normal light intensity
         if (pNode->u.leaf.sector->sloped_floor != NULL)
            pNode->u.leaf.sector->sloped_floor->lightscale = FINENESS;
      }

      distance = DistanceGet(distX, distY);

      if (gD3DDriverProfile.bFogEnable)
         paletteIndex = GetLightPaletteIndex(FINENESS, pSector->light, lightscale, 0);
      else
         paletteIndex = GetLightPaletteIndex(distance, pSector->light, lightscale, 0);

      pBGRA[count].r = pBGRA[count].g = pBGRA[count].b = paletteIndex * COLOR_AMBIENT / 64;
      pBGRA[count].a = 255;
   }
}

// UV coords reversed for rotated texture.
void D3DRenderCeilingUpdate(BSPnode *pNode, PDIB pDib, custom_xyz *pXYZ, custom_st *pST,
   custom_bgra *pBGRA)
{
   Sector *pSector = pNode->u.leaf.sector;
   int paletteIndex, distX, distY, distance;
   long lightscale;
   float inv64 = 0.0009765625f;

   pNode->seenCeilThisFrame = True;

   for (int count = 0; count < pNode->u.leaf.poly.npts; count++)
   {
      // xyz
      pXYZ[count].x = pNode->u.leaf.ceil.xyz[count].x;
      pXYZ[count].y = pNode->u.leaf.ceil.xyz[count].y;
      if (pSector->sloped_ceiling)
         pXYZ[count].z = pNode->u.leaf.ceil.xyz[count].z;
      else
         pXYZ[count].z = (float)pSector->ceiling_height;

      // st
      if (pST)
      {
         pST[count].t = pNode->u.leaf.ceil.st[count].t;
         pST[count].s = pNode->u.leaf.ceil.st[count].s;

         if (pSector->animate != NULL
            && pSector->animate->animation == ANIMATE_SCROLL
            && pSector->flags & SF_SCROLL_CEILING)
         {
            pST[count].t -= pSector->animate->u.scroll.yoffset;// * pDib->shrink;
            pST[count].s += pSector->animate->u.scroll.xoffset;// * pDib->shrink;
         }

         pST[count].s *= inv64;
         pST[count].t *= inv64;
      }

      // bgra
      distX = pXYZ[count].x - player.x;
      distY = pXYZ[count].y - player.y;

      lightscale = FINENESS;

      if (shade_amount != 0)
      {
         if ((pNode->u.leaf.sector->sloped_ceiling != NULL) &&
            (pNode->u.leaf.sector->sloped_ceiling->flags & SLF_DIRECTIONAL))
         {
            long lo_end = FINENESS - shade_amount;

            // light scale is based on dot product of surface normal and sun vector
            lightscale = (long)(pNode->u.leaf.sector->sloped_ceiling->plane.a * sun_vect.x +
               pNode->u.leaf.sector->sloped_ceiling->plane.b * sun_vect.y +
               pNode->u.leaf.sector->sloped_ceiling->plane.a * sun_vect.z) >> LOG_FINENESS;

            lightscale = (lightscale + FINENESS) >> 1; // map to 0 to 1 range

            lightscale = lo_end + ((lightscale * shade_amount) >> LOG_FINENESS);

            if (lightscale > FINENESS)
               lightscale = FINENESS;
            else if (lightscale < 0)
               lightscale = 0;

            pNode->u.leaf.sector->sloped_ceiling->lightscale = lightscale;
         }
      }
      else
      {
         if (pNode->u.leaf.sector->sloped_ceiling != NULL)
            pNode->u.leaf.sector->sloped_ceiling->lightscale = FINENESS;
      }

      distance = DistanceGet(distX, distY);

      if (gD3DDriverProfile.bFogEnable)
         paletteIndex = GetLightPaletteIndex(FINENESS, pSector->light, lightscale, 0);
      else
         paletteIndex = GetLightPaletteIndex(distance, pSector->light, lightscale, 0);

      pBGRA[count].r = pBGRA[count].g = pBGRA[count].b = paletteIndex * COLOR_AMBIENT / 64;
      pBGRA[count].a = 255;
   }
}

void D3DRenderBackgroundsLoad(char *pFilename, int index)
{
   FILE   *pFile;
   png_structp   pPng = NULL;
   png_infop   pInfo = NULL;
   png_infop   pInfoEnd = NULL;
   png_bytepp   rows;

   D3DLOCKED_RECT      lockedRect;
   unsigned char      *pBits = NULL;
   unsigned int      w, h, b;
   int               pitchHalf, bytePP;
   fpos_t            pos;

   pFile = fopen(pFilename, "rb");
   if (pFile == NULL)
      return;

   pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (NULL == pPng)
   {
      fclose(pFile);
      return;
   }

   pInfo = png_create_info_struct(pPng);
   if (NULL == pInfo)
   {
      png_destroy_read_struct(&pPng, NULL, NULL);
      fclose(pFile);
      return;
   }

   pInfoEnd = png_create_info_struct(pPng);
   if (NULL == pInfoEnd)
   {
      png_destroy_read_struct(&pPng, &pInfo, NULL);
      fclose(pFile);
      return;
   }

   if (setjmp(png_jmpbuf(pPng)))
   {
      png_destroy_read_struct(&pPng, &pInfo, &pInfoEnd);
      fclose(pFile);
      return;
   }

   png_destroy_read_struct(&pPng, &pInfo, &pInfoEnd);

   pos = 0;

   {
      int   i;
      png_bytep   curRow;

      for (i = 0; i < 6; i++)
      {
         pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
         pInfo = png_create_info_struct(pPng);
         pInfoEnd = png_create_info_struct(pPng);
         setjmp(png_jmpbuf(pPng));

         fseek(pFile, pos, SEEK_SET);

         png_init_io(pPng, pFile);
         png_read_png(pPng, pInfo, PNG_TRANSFORM_IDENTITY, NULL);
         rows = png_get_rows(pPng, pInfo);

         bytePP = pPng->pixel_depth / 8;

         IDirect3DDevice9_CreateTexture(gpD3DDevice, pPng->width, pPng->height, 1, 0,
                                        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &gpSkyboxTextures[index][i], NULL);

         IDirect3DTexture9_LockRect(gpSkyboxTextures[index][i], 0, &lockedRect, NULL, 0);

         pitchHalf = lockedRect.Pitch / 2;

         pBits = (unsigned char *)lockedRect.pBits;

         for (h = 0; h < pPng->height; h++)
         {
            curRow = rows[h];

            for (w = 0; w < pPng->width; w++)
            {
               for (b = 0; b < 4; b++)
               {
                  if (b == 3)
                     pBits[h * lockedRect.Pitch + w * 4 + b] = 255;
                  else
              // Apparently PNGs are BGR, while DirectX wants RGB
                     pBits[h * lockedRect.Pitch + w * 4 + (2 - b)] =
                        curRow[(w * 3) + b];
               }
            }
         }

         IDirect3DTexture9_UnlockRect(gpSkyboxTextures[index][i], 0);

         fgetpos(pFile, &pos);
         png_destroy_read_struct(&pPng, &pInfo, &pInfoEnd);
      }
   }

//   png_destroy_read_struct(&pPng, &pInfo, &pInfoEnd);
   fclose(pFile);
}

void D3DRenderBackgroundSet2(ID background)
{
   char   *filename = NULL;
   filename = LookupRsc(background);

   if (filename)
   {
      if (strstr(filename, "skya.bgf"))
         gCurBackground = 0;
      else if (strstr(filename, "skyb.bgf"))
         gCurBackground = 1;
      else if (strstr(filename, "skyc.bgf"))
         gCurBackground = 2;
      else if (strstr(filename, "skyd.bgf"))
         gCurBackground = 3;
      else if (strstr(filename, "redsky.bgf"))
         gCurBackground = 4;
   }

   // force a rebuild since static lightmaps might have changed
   gD3DRedrawAll |= D3DRENDER_REDRAW_ALL;
}

// new render stuff
void D3DRenderPoolInit(d3d_render_pool_new *pPool, int size, int packetSize)
{
   d3d_render_packet_new   *pPacket = NULL;
   u_int   i;

   pPool->size = size;
   pPool->curPacket = 0;
   pPacket = (d3d_render_packet_new *)D3DRenderMalloc(sizeof(d3d_render_packet_new) * size);
   assert(pPacket);
   pPool->renderPacketList = list_create(pPacket);
   pPool->packetSize = packetSize;

   D3DRenderPoolReset(pPool, NULL);

   for (i = 0; i < pPool->size; i++)
   {
      pPacket->size = packetSize;
//      pPool->renderPackets[i].size = 128;
   }
}

void D3DRenderPoolShutdown(d3d_render_pool_new *pPool)
{
   list_destroy(pPool->renderPacketList);
   memset(pPool, 0, sizeof(d3d_render_pool_new));
}

void D3DRenderPoolReset(d3d_render_pool_new *pPool, void *pMaterialFunc)
{
   pPool->curPacket = 0;
   pPool->numLists = 0;
   pPool->curPacketList = pPool->renderPacketList;
   pPool->pMaterialFctn = (MaterialFctn) pMaterialFunc;
}

d3d_render_packet_new *D3DRenderPacketNew(d3d_render_pool_new *pPool)
{
   d3d_render_packet_new   *pPacket;

   if (pPool->curPacket >= pPool->size)
   {
      if (pPool->curPacketList->next == NULL)
      {
         pPacket = (d3d_render_packet_new *)D3DRenderMalloc(sizeof(d3d_render_packet_new) * pPool->size);
         assert(pPacket);
         list_add_item(pPool->renderPacketList, pPacket);
      }
      else
         pPacket = (d3d_render_packet_new *)pPool->curPacketList->next->data;

      pPool->curPacketList = pPool->curPacketList->next;
      pPool->curPacket = 1;
      pPool->numLists++;

      pPacket = (d3d_render_packet_new *)pPool->curPacketList->data;
   }
   else
   {
      pPacket = (d3d_render_packet_new *)pPool->curPacketList->data;
      pPacket += pPool->curPacket;

      if (pPool->curPacket == 12)
         gpPacket = pPacket;

      pPool->curPacket++;
   }

   D3DRenderPacketInit(pPacket);
   return pPacket;
}

void D3DRenderPacketInit(d3d_render_packet_new *pPacket)
{
   pPacket->curChunk = 0;
   pPacket->effect = 0;
   pPacket->flags = 0;
   pPacket->pDib = NULL;
   pPacket->pMaterialFctn = NULL;
   pPacket->pTexture = NULL;
   pPacket->xLat0 = 0;
   pPacket->xLat1 = 0;
}

d3d_render_chunk_new *D3DRenderChunkNew(d3d_render_packet_new *pPacket)
{
   if (pPacket->curChunk >= (pPacket->size - 1))
      return NULL;
   else
   {
      pPacket->curChunk++;
      D3DRenderChunkInit(&pPacket->renderChunks[pPacket->curChunk - 1]);
      return &pPacket->renderChunks[pPacket->curChunk - 1];
   }
}

void D3DRenderChunkInit(d3d_render_chunk_new *pChunk)
{
   pChunk->curIndex = 0;
   pChunk->flags = 0;
   pChunk->zBias = 0;
   pChunk->numIndices = 0;
   pChunk->xLat0 = 0;
   pChunk->xLat1 = 0;
   pChunk->pSector = NULL;
   pChunk->pSectorNeg = NULL;
   pChunk->pSectorPos = NULL;
   pChunk->pSideDef = NULL;
   pChunk->pMaterialFctn = NULL;
   pChunk->pRenderCache = NULL;
}

d3d_render_packet_new *D3DRenderPacketFindMatch(d3d_render_pool_new *pPool, LPDIRECT3DTEXTURE9 pTexture,
                                    PDIB pDib, BYTE xLat0, BYTE xLat1, BYTE effect)
{
   u_int                  count, numPackets;
   d3d_render_packet_new   *pPacket;
   list_type            list;

   for (list = pPool->renderPacketList; list != pPool->curPacketList->next; list = list->next)
   {
      pPacket = (d3d_render_packet_new *)list->data;

      if (list == pPool->curPacketList)
         numPackets = pPool->curPacket;
      else
         numPackets = pPool->size;

      // for each packet
      for (count = 0; count < numPackets; count++, pPacket++)
      {
         // if we find a match that isn't full already, return it
         if ((pPacket->pDib == pDib) && (pPacket->pTexture == pTexture) &&
            (pPacket->xLat0 == xLat0) && (pPacket->xLat1 == xLat1) &&
            (pPacket->effect == effect))
         {
            if (pPacket->curChunk < (pPacket->size - 1))
               return pPacket;
         }
      }
   }

   // otherwise, return a new one (or NULL if no more remain)
   pPacket = D3DRenderPacketNew(pPool);

   if (pPacket)
   {
      pPacket->pDib = pDib;
      pPacket->pTexture = pTexture;
      pPacket->xLat0 = xLat0;
      pPacket->xLat1 = xLat1;
      pPacket->effect = effect;
      pPacket->size = pPool->packetSize;
   }

   return pPacket;
}

void D3DGetBackgroundOverlayPosition(BackgroundOverlay *pOverlay, Draw3DParams *params, Pnt3D *bObj)
{
   if (pOverlay->x > 4096)
      pOverlay->x -= 4096;
   else if (pOverlay->x < 0)
      pOverlay->x += 4096;
   float fAngleObj = (pOverlay->x * PI * 2 / 4096);

   bObj->x = 75000.0f + (75000.0f * cos(fAngleObj));
   bObj->y = 75000.0f + (75000.0f * sin(fAngleObj));
   bObj->z = params->viewer_height + (pOverlay->y << 6);
}

void D3DRenderBackgroundObjectsDraw(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params)
{
   D3DMATRIX mat;
   Pnt3D bObj;
   PDIB pDib;
   d3d_render_packet_new *pPacket = NULL;
   d3d_render_chunk_new  *pChunk = NULL;

   for (list_type list = room->bg_overlays; list != NULL; list = list->next)
   {
      BackgroundOverlay *pBObj = (BackgroundOverlay *)list->data;

      if (!pBObj)
         continue;

      D3DGetBackgroundOverlayPosition(pBObj, params, &bObj);

      // Hardcoded to use 1st group in bgf file
      pDib = GetObjectPdib(pBObj->obj.icon_res, 0, 0);
      if (!pDib)
         continue;

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, 0, 0, 0);
      if (!pPacket)
         continue;

      pChunk = D3DRenderChunkNew(pPacket);
      if (!pChunk)
         continue;

      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;
      pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = &D3DMaterialObjectChunk;
      pChunk->zBias = 0;
      pChunk->alphaRef = TEMP_ALPHA_REF;

      // Background objects don't adjust angle, but we adjust the viewing angle
      // for the player so they view the object face on.
      MatrixTranslate(&mat, (float)bObj.x, (float)bObj.z, (float)bObj.y);
      MatrixMultiply(&pChunk->xForm, &mPlayerHeadingTrans, &mat);

      pChunk->xyz[0].x = (float)pDib->width / (float)pDib->shrink * -8.0f + (float)pDib->xoffset;
      pChunk->xyz[0].z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;

      pChunk->xyz[1].x = (float)pDib->width / (float)pDib->shrink * -8.0f + (float)pDib->xoffset;
      pChunk->xyz[1].z = -(float)pDib->yoffset * 4.0f;

      pChunk->xyz[2].x = (float)pDib->width / (float)pDib->shrink * 8.0f + (float)pDib->xoffset;
      pChunk->xyz[2].z = -(float)pDib->yoffset * 4.0f;

      pChunk->xyz[3].x = (float)pDib->width / (float)pDib->shrink * 8.0f + (float)pDib->xoffset;
      pChunk->xyz[3].z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;

      {
         float oneOverW, oneOverH;

         oneOverW = 1.0f / pDib->width;
         oneOverH = 1.0f / pDib->height;

         pChunk->st0[0].s = 1.0f - oneOverW;
         pChunk->st0[0].t = oneOverH;
         pChunk->st0[1].s = 1.0f - oneOverW;
         pChunk->st0[1].t = 1.0f - oneOverH;
         pChunk->st0[2].s = oneOverW;
         pChunk->st0[2].t = 1.0f - oneOverH;
         pChunk->st0[3].s = oneOverW;
         pChunk->st0[3].t = oneOverH;
      }

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      for (int i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = COLOR_MAX;
         pChunk->bgra[i].g = COLOR_MAX;
         pChunk->bgra[i].r = COLOR_MAX;
         pChunk->bgra[i].a = COLOR_MAX;
      }

#ifndef NODPRINTFS
      gNumObjects++;
#endif
   }
}

void D3DRenderObjectsDraw(d3d_render_pool_new *pPool, room_type *room,
                      Draw3DParams *params, BYTE flags, Bool drawTransparent)
{
   D3DMATRIX         mat;
   int               i;
   room_contents_node   *pRNode;
   long            dx, dy, angle;
   PDIB            pDib;
   custom_xyz         xyz[4];
   custom_bgra         bgra;
   float            lastDistance, depth;
   BYTE            xLat0, xLat1;
   int               sector_flags;
   long            top, bottom;

   d3d_render_packet_new   *pPacket = NULL;
   d3d_render_chunk_new   *pChunk = NULL;

   // base objects
   for (list_type list = room->contents; list != NULL; list = list->next)
   {
      pRNode = (room_contents_node *)list->data;

      if (pRNode == NULL
         || !pRNode->visible
         || pRNode->obj.id == player.id)
         continue;

      if (flags == DRAWFX_INVISIBLE)
      {
         if (pRNode->obj.drawingtype != DRAWFX_INVISIBLE)
            continue;
      }
      else
      {
         if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
            continue;

         if (drawTransparent != ((pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
            || (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
            || (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
            || (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
            || (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
            || (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)))
            continue;
      }

      dx = pRNode->motion.x - params->viewer_x;
      dy = pRNode->motion.y - params->viewer_y;

      angle = (pRNode->angle - intATan2(-dy,-dx)) & NUMDEGREES_MASK;

      pDib = GetObjectPdib(pRNode->obj.icon_res, angle, pRNode->obj.animate->group);

      if (NULL == pDib)
         continue;

      if (pRNode->obj.drawingtype == DRAWFX_SECONDTRANS)
      {
         xLat0 = 0;
         xLat1 = pRNode->obj.secondtranslation;
      }
      else if (pRNode->obj.drawingtype == DRAWFX_DOUBLETRANS)
      {
         xLat0 = pRNode->obj.translation;
         xLat1 = pRNode->obj.secondtranslation;
      }
      else
      {
         xLat0 = pRNode->obj.translation;
         xLat1 = 0;
      }

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, xLat0, xLat1,
         pRNode->obj.drawingtype);
      if (NULL == pPacket)
         return;

      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);

      pChunk->drawingtype = pRNode->obj.drawingtype;

      if (flags == DRAWFX_INVISIBLE)
      {
         pPacket->pMaterialFctn = &D3DMaterialObjectInvisiblePacket;
         pChunk->pMaterialFctn = &D3DMaterialObjectInvisibleChunk;
      }
      else
      {
         pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
         pChunk->pMaterialFctn = &D3DMaterialObjectChunk;
      }

      angle = pRNode->angle - (params->viewer_angle + 3072);

      if (angle < -4096)
         angle += 4096;

      /* Make sure that object is above the floor. */
      if (!GetRoomHeight(room->tree, &top, &bottom, &sector_flags, pRNode->motion.x, pRNode->motion.y))
      {
         continue;
      }

      // No depth for override flags.
      if (ROOM_OVERRIDE_MASK & GetRoomFlags())
      {
         depth = 0.0f;
      }
      else
      {
         // Set object depth based on "depth" sector flags
         depth = sector_depths[SectorDepth(sector_flags)];
      }

      MatrixTranslate(&mat, (float)pRNode->motion.x, max(bottom, pRNode->motion.z) - depth,
         (float)pRNode->motion.y);
      MatrixMultiply(&pChunk->xForm, &mPlayerHeadingTrans, &mat);

      xyz[0].x = (float)pDib->width / (float)pDib->shrink * -8.0f + (float)pDib->xoffset;
      xyz[0].z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;
      xyz[0].y = 0.0f;

      xyz[1].x = (float)pDib->width / (float)pDib->shrink * -8.0f + (float)pDib->xoffset;
      xyz[1].z = -(float)pDib->yoffset * 4.0f;
      xyz[1].y = 0.0f;

      xyz[2].x = (float)pDib->width / (float)pDib->shrink * 8.0f + (float)pDib->xoffset;
      xyz[2].z = -(float)pDib->yoffset * 4.0f;
      xyz[2].y = 0.0f;

      xyz[3].x = (float)pDib->width / (float)pDib->shrink * 8.0f + (float)pDib->xoffset;
      xyz[3].z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;
      xyz[3].y = 0.0f;

      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = xLat0;
      pChunk->xLat1 = xLat1;
      pChunk->zBias = ZBIAS_BASE;

      lastDistance = 0;

      if (pRNode->obj.drawingtype == DRAWFX_BLACK)
      {
         bgra.b = bgra.g = bgra.r = 0;
         bgra.a = 255;
      }
      else
      {
         if (D3DObjectLightingCalc(room, pRNode, &bgra, 0))
            pChunk->flags |= D3DRENDER_NOAMBIENT;
      }

/*      if(pRNode->obj.id != INVALID_ID && pRNode->obj.id == GetUserTargetID())
      {
         bgra.b = 0.0f;
         bgra.g = 0.0f;
         bgra.r = 255.0f;
      }*/

      if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
      {
         bgra.a = D3DRENDER_TRANS25;
         pChunk->alphaRef = D3DRENDER_TRANS25 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
      {
         bgra.a = D3DRENDER_TRANS75;
         pChunk->alphaRef = D3DRENDER_TRANS75 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      else
      {
         pChunk->alphaRef = TEMP_ALPHA_REF;
      }

      for (i = 0; i < 4; i++)
      {
         pChunk->xyz[i].x = xyz[i].x;
         pChunk->xyz[i].y = xyz[i].y;
         pChunk->xyz[i].z = xyz[i].z;

         pChunk->bgra[i].b = bgra.b;
         pChunk->bgra[i].g = bgra.g;
         pChunk->bgra[i].r = bgra.r;
         pChunk->bgra[i].a = bgra.a;
      }

      {
         float oneOverW, oneOverH;

         oneOverW = (1.0f / pDib->width);
         oneOverH = (1.0f / pDib->height);

         pChunk->st0[0].s = 1.0f - oneOverW;
         pChunk->st0[0].t = oneOverH;
         pChunk->st0[1].s = 1.0f - oneOverW;
         pChunk->st0[1].t = 1.0f - oneOverH;
         pChunk->st0[2].s = oneOverW;
         pChunk->st0[2].t = 1.0f - oneOverH;
         pChunk->st0[3].s = oneOverW;
         pChunk->st0[3].t = oneOverH;
      }

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // now add object to visible object list
      if ((pRNode->obj.id != INVALID_ID) && (pRNode->obj.id != player.id))
      {
         D3DMATRIX   localToScreen, rot, mat;
         custom_xyzw   topLeft, topRight, bottomLeft, bottomRight, center;
         ObjectRange *range = FindVisibleObjectById(pRNode->obj.id);
         int         w, h;
         int         tempLeft, tempRight, tempTop, tempBottom;
         int         distX, distY, distance;

         if (pRNode->obj.id == player.id)
            break;

         w = gD3DRect.right - gD3DRect.left;
         h = gD3DRect.bottom - gD3DRect.top;

         topLeft.x = pChunk->xyz[3].x;
         topLeft.y = pChunk->xyz[3].z;
         topLeft.z = 0;
         topLeft.w = 1.0f;

         topRight.x = pChunk->xyz[3].x;
         topRight.y = pChunk->xyz[3].z;
         topRight.z = 0;
         topRight.w = 1.0f;

         bottomLeft.x = pChunk->xyz[1].x;
         bottomLeft.y = pChunk->xyz[1].z;
         bottomLeft.z = 0;
         bottomLeft.w = 1.0f;

         bottomRight.x = pChunk->xyz[1].x;
         bottomRight.y = pChunk->xyz[1].z;
         bottomRight.z = 0;
         bottomRight.w = 1.0f;

         MatrixMultiply(&rot, &mPlayerHeading, &mPlayerPitchFudge);
         MatrixMultiply(&mat, &mPlayerViewTranslate, &rot);
         XformMatrixPerspective(&localToScreen, FOV_H, FOV_V, 1.0f, 2000000.0f);
         MatrixMultiply(&mat, &pChunk->xForm, &mat);
         MatrixMultiply(&localToScreen, &mat, &localToScreen);

         MatrixMultiplyVector(&topLeft, &localToScreen, &topLeft);
         MatrixMultiplyVector(&topRight, &localToScreen, &topRight);
         MatrixMultiplyVector(&bottomLeft, &localToScreen, &bottomLeft);
         MatrixMultiplyVector(&bottomRight, &localToScreen, &bottomRight);

         topLeft.x /= topLeft.w;
         topLeft.y /= topLeft.w;
         topLeft.z /= topLeft.w;
         bottomRight.x /= bottomRight.w;
         bottomRight.y /= bottomRight.w;
         bottomRight.z /= bottomRight.w;

         topLeft.z = topLeft.z * 2.0f - 1.0f;
         bottomRight.z = bottomRight.z * 2.0f - 1.0f;

         topRight.x = bottomRight.x;
         topRight.y = topLeft.y;
         topRight.z = topLeft.z;
         bottomLeft.x = topLeft.x;
         bottomLeft.y = bottomRight.y;
         bottomLeft.z = topLeft.z;

         center.x = (topLeft.x + topRight.x) / 2.0f;
         center.y = (topLeft.y + bottomLeft.y) / 2.0f;
         center.z = topLeft.z;

         if (flags == DRAWFX_INVISIBLE)
         {
            pChunk->st1[0].s = D3DRENDER_CLIP_TO_SCREEN_X(bottomRight.x, gScreenWidth) / gScreenWidth;
            pChunk->st1[0].t = D3DRENDER_CLIP_TO_SCREEN_Y(topLeft.y, gScreenHeight) / gScreenHeight;
            pChunk->st1[1].s = D3DRENDER_CLIP_TO_SCREEN_X(bottomRight.x, gScreenWidth) / gScreenWidth;
            pChunk->st1[1].t = D3DRENDER_CLIP_TO_SCREEN_Y(bottomRight.y, gScreenHeight) / gScreenHeight;
            pChunk->st1[2].s = D3DRENDER_CLIP_TO_SCREEN_X(topLeft.x, gScreenWidth) / gScreenWidth;
            pChunk->st1[2].t = D3DRENDER_CLIP_TO_SCREEN_Y(bottomRight.y, gScreenHeight) / gScreenHeight;
            pChunk->st1[3].s = D3DRENDER_CLIP_TO_SCREEN_X(topLeft.x, gScreenWidth) / gScreenWidth;
            pChunk->st1[3].t = D3DRENDER_CLIP_TO_SCREEN_Y(topLeft.y, gScreenHeight) / gScreenHeight;

            pChunk->st1[0].s -= (gFrame & 3) / 256.0f;
            pChunk->st1[0].t -= (gFrame & 3) / 256.0f;
            pChunk->st1[1].s -= (gFrame & 3) / 256.0f;
            pChunk->st1[1].t += (gFrame & 3) / 256.0f;
            pChunk->st1[2].s += (gFrame & 3) / 256.0f;
            pChunk->st1[2].t += (gFrame & 3) / 256.0f;
            pChunk->st1[3].s += (gFrame & 3) / 256.0f;
            pChunk->st1[3].t -= (gFrame & 3) / 256.0f;
         }

         if (
            (
            (D3DRENDER_CLIP(topLeft.x, 1.0f) &&
            D3DRENDER_CLIP(topLeft.y, 1.0f)) ||
            (D3DRENDER_CLIP(bottomLeft.x, 1.0f) &&
            D3DRENDER_CLIP(bottomLeft.y, 1.0f)) ||
            (D3DRENDER_CLIP(topRight.x, 1.0f) &&
            D3DRENDER_CLIP(topRight.y, 1.0f)) ||
            (D3DRENDER_CLIP(bottomRight.x, 1.0f) &&
            D3DRENDER_CLIP(bottomRight.y, 1.0f)) ||
            (D3DRENDER_CLIP(center.x, 1.0f))
            ) &&
            D3DRENDER_CLIP(topLeft.z, 1.0f))
         {
            tempLeft = (topLeft.x * w / 2) + (w >> 1);
            tempRight = (bottomRight.x * w / 2) + (w >> 1);
            tempTop = (topLeft.y * -h / 2) + (h >> 1);
            tempBottom = (bottomRight.y * -h / 2) + (h >> 1);

            if (config.large_area)
            {
               tempLeft >>= 1;
               tempRight >>= 1;
               tempTop >>= 1;
               tempBottom >>= 1;
            }

            distX = pRNode->motion.x - player.x;
            distY = pRNode->motion.y - player.y;

            distance = DistanceGet(distX, distY);

            if (range == NULL)
            {
               // Set up new visible object
               range = &visible_objects[num_visible_objects];
               range->id       = pRNode->obj.id;
               range->distance = distance;
               range->left_col   = tempLeft;
               range->right_col  = tempRight;
               range->top_row    = tempTop;
               range->bottom_row = tempBottom;

               num_visible_objects = ++num_visible_objects > MAXOBJECTS ? MAXOBJECTS : num_visible_objects;
            }
            
            // Record boundaries of drawing area
            range->left_col   = min(range->left_col, tempLeft);
            range->right_col  = max(range->right_col, tempRight);
            range->top_row    = min(range->top_row, tempTop);
            range->bottom_row = max(range->bottom_row, tempBottom);
         }
      }

      if (pRNode->obj.id != INVALID_ID && pRNode->obj.id == GetUserTargetID() &&
         (pRNode->obj.drawingtype != DRAWFX_INVISIBLE))
      {
         pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, xLat0, xLat1,
            pRNode->obj.drawingtype);
         if (NULL == pPacket)
            return;
         pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
         pChunk = D3DRenderChunkNew(pPacket);
         assert(pChunk);
         pChunk->drawingtype = DRAWFX_TRANSLUCENT50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
         pChunk->numIndices = 4;
         pChunk->numVertices = 4;
         pChunk->numPrimitives = pChunk->numVertices - 2;
         pChunk->xLat0 = xLat0;
         pChunk->xLat1 = xLat1;
         pChunk->zBias = ZBIAS_TARGETED;

         MatrixMultiply(&pChunk->xForm, &mPlayerHeadingTrans, &mat);

         if (flags == DRAWFX_INVISIBLE)
         {
            pChunk->pMaterialFctn = &D3DMaterialObjectInvisibleChunk;
         }
         else
         {
            pChunk->pMaterialFctn = &D3DMaterialTargetedObjectChunk;
         }

         D3DObjectLightingCalc(room, pRNode, &bgra, 0);

         for (i = 0; i < 4; i++)
         {
            pChunk->xyz[i].x = xyz[i].x;
            pChunk->xyz[i].y = 0;
            pChunk->xyz[i].z = xyz[i].z;

            switch (config.halocolor)
            {
            case TARGET_COLOR_RED:
               pChunk->bgra[i].b = 0;
               pChunk->bgra[i].g = 0;
               pChunk->bgra[i].r = min(bgra.r * 2.0f, 255);
               pChunk->bgra[i].a = 255;//D3DRENDER_TRANS50;
               break;

            case TARGET_COLOR_BLUE:
               pChunk->bgra[i].b = min(bgra.r * 2.0f, 255);
               pChunk->bgra[i].g = 0;
               pChunk->bgra[i].r = 0;
               pChunk->bgra[i].a = 255;//D3DRENDER_TRANS50;
               break;

            default: // TARGET_COLOR_GREEN
               pChunk->bgra[i].b = 0;
               pChunk->bgra[i].g = min(bgra.r * 2.0f, 255);
               pChunk->bgra[i].r = 0;
               pChunk->bgra[i].a = 255;//D3DRENDER_TRANS50;
               break;
            }
         }

         pChunk->xyz[0].x -= 96.0f / (float)pDib->shrink;
         pChunk->xyz[0].z += 96.0f / (float)pDib->shrink;
         pChunk->xyz[1].x -= 96.0f / (float)pDib->shrink;
         pChunk->xyz[1].z -= 96.0f / (float)pDib->shrink;
         pChunk->xyz[2].x += 96.0f / (float)pDib->shrink;
         pChunk->xyz[2].z -= 96.0f / (float)pDib->shrink;
         pChunk->xyz[3].x += 96.0f / (float)pDib->shrink;
         pChunk->xyz[3].z += 96.0f / (float)pDib->shrink;

         pChunk->st0[0].s = 1.0f;
         pChunk->st0[0].t = 0.0f;
         pChunk->st0[1].s = 1.0f;
         pChunk->st0[1].t = 1.0f;
         pChunk->st0[2].s = 0.0f;
         pChunk->st0[2].t = 1.0f;
         pChunk->st0[3].s = 0.0f;
         pChunk->st0[3].t = 0.0f;

         pChunk->indices[0] = 1;
         pChunk->indices[1] = 2;
         pChunk->indices[2] = 0;
         pChunk->indices[3] = 3;
      }
   }
   return;
}

void D3DRenderOverlaysDraw(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params,
                     BOOL underlays, BYTE flags, Bool drawTransparent)
{
   D3DMATRIX         mat;
   int               i;
   room_contents_node   *pRNode;
   long            dx, dy, angle, top, bottom;
   PDIB            pDib, pDibOv, pDibOv2;
   Overlay            *pOverlay;
   custom_xyz         xyz[4];
   custom_bgra         bgra;
   float            lastDistance, invShrink, invOvShrink, invOv2Shrink, depthf;
   BYTE            xLat0, xLat1, zBias;
   int               sector_flags;
   Bool            bHotspot;

   d3d_render_packet_new   *pPacket = NULL;
   d3d_render_chunk_new   *pChunk = NULL;

   for (list_type list = room->contents; list != NULL; list = list->next)
   {
      pRNode = (room_contents_node *)list->data;

      if (pRNode == NULL
         || !pRNode->visible
         || pRNode->obj.id == player.id
         || *pRNode->obj.overlays == NULL)
         continue;

      if (flags == DRAWFX_INVISIBLE)
      {
         if (pRNode->obj.drawingtype != DRAWFX_INVISIBLE)
            continue;
      }
      else
      {
         if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
            continue;

         if (drawTransparent != ((pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
            || (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
            || (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
            || (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
            || (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
            || (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)))
            continue;
      }

      int depth;
      // three passes each
      for (int pass = 0; pass < 4; pass++)
      {
         // flush cache between passes
         // unlock all buffers

         if (underlays)
         {
            if (pass == 0)
               continue;
            switch (pass)
            {
            case 1:
               depth = HOTSPOT_UNDERUNDER;
               zBias = ZBIAS_UNDERUNDER;
               break;
            case 2:
               depth = HOTSPOT_UNDER;
               zBias = ZBIAS_UNDER;
               break;
            case 3:
               depth = HOTSPOT_UNDEROVER;
               zBias = ZBIAS_UNDEROVER;
               break;
            }
         }
         else
         {
            switch (pass)
            {
            case 0:
               depth = HOTSPOT_OVERUNDEROVERUNDER;
               zBias = ZBIAS_OVERUNDEROVERUNDER;
               break;
            case 1:
               depth = HOTSPOT_OVERUNDER;
               zBias = ZBIAS_OVERUNDER;
               break;
            case 2:
               depth = HOTSPOT_OVER;
               zBias = ZBIAS_OVER;
               break;
            case 3:
               depth = HOTSPOT_OVEROVER;
               zBias = ZBIAS_OVEROVER;
               break;
            }
         }

         for (list_type list2 = *pRNode->obj.overlays; list2 != NULL; list2 = list2->next)
         {
            bHotspot = FALSE;

            pOverlay = (Overlay *)list2->data;

            if (NULL == pOverlay)
               continue;

            dx = pRNode->motion.x - params->viewer_x;
            dy = pRNode->motion.y - params->viewer_y;

            angle = (pRNode->angle - intATan2(-dy,-dx)) & NUMDEGREES_MASK;

            pDib = GetObjectPdib(pRNode->obj.icon_res, angle, pRNode->obj.animate->group);
            pDibOv = GetObjectPdib(pOverlay->icon_res, angle, pOverlay->animate.group);

            if ((NULL == pDib) || (NULL == pDibOv))
               continue;

            invShrink = 1.0f / pDib->shrink;
            invOvShrink = 1.0f / pDibOv->shrink;

//            if (pDibOv->shrink > 7)
//               continue;

            if (pOverlay->hotspot)
            {
               POINT   point;
               int      retVal, retVal2;

               retVal = FindHotspotPdib(pDib, pOverlay->hotspot, &point);

               if (retVal != HOTSPOT_NONE)
               {
                  if (retVal != depth)
                  //if (1)
                     continue;

                  // set top right corner at top right corner of base bitmap
                  xyz[3].x = (float)pDib->width * invShrink * 8.0f;
                  xyz[3].z = ((float)pDib->height * invShrink * 16.0f);

                  xyz[2].x = xyz[3].x;
                  xyz[2].z = xyz[3].z -
                     ((float)pDibOv->height * invOvShrink * 16.0f);

                  xyz[1].x = xyz[3].x -
                     (float)pDibOv->width * invOvShrink * 16.0f;;
                  xyz[1].z = xyz[3].z -
                     ((float)pDibOv->height * invOvShrink * 16.0f);

                  xyz[0].x = xyz[3].x -
                     (float)pDibOv->width * invOvShrink * 16.0f;
                  xyz[0].z = xyz[3].z;

                  // add base object's offsets
                  for (i = 0; i < 4; i++)
                  {
                     xyz[i].x += pDib->xoffset;
                     xyz[i].z -= pDib->yoffset * 4.0f;
                  }

                  for (i = 0; i < 4; i++)
                  {
                     // add hotspot
                     xyz[i].x -= point.x * 16.000f * invShrink;
                     xyz[i].z -= (float)point.y * 16.000f * invShrink;
                     // add overlay offsets
                     xyz[i].x -= pDibOv->xoffset * 16.000f * invShrink;
                     xyz[i].z -= pDibOv->yoffset * 16.000f * invShrink;
                  }

                  bHotspot = TRUE;
               }
               else
               {
                  // the hotspot wasn't in the base object pdib
                  // must be an overlay on an overlay, so find base overlay
                  list_type   tempList;

                  for (tempList = *pRNode->obj.overlays; tempList != NULL; tempList = tempList->next)
                  {
                     POINT   point2;
                     Overlay   *pTempOverlay = (Overlay *)tempList->data;

                     if (pTempOverlay == NULL)
                        continue;

                     pDibOv2 = GetObjectPdib(pTempOverlay->icon_res, angle, pTempOverlay->animate.group);

                     if (NULL == pDibOv2)
                        continue;

                     invOv2Shrink = 1.0f / pDibOv2->shrink;

                     if ((retVal = FindHotspotPdib(pDibOv2, pOverlay->hotspot, &point2)) != HOTSPOT_NONE)
                     {
                        int   tempRetVal;

                        // pDib = the base object
                        // pDibOv2 = the base overlay
                        // pDibOv = the current overlay we are drawing
                        retVal2 = FindHotspotPdib(pDib, pTempOverlay->hotspot, &point);

                        if (retVal2 != HOTSPOT_NONE)
                        {
                           if (retVal == HOTSPOT_OVER)
                           {
                              if (retVal2 == HOTSPOT_OVER)
                                 tempRetVal = HOTSPOT_OVEROVER;
                              else
                                 tempRetVal = HOTSPOT_UNDEROVER;
                           }
                           else
                           {
                              if (retVal2 == HOTSPOT_OVER)
                                 tempRetVal = HOTSPOT_OVERUNDER;
                              else
                                 tempRetVal = HOTSPOT_UNDERUNDER;
                           }

                           if (tempRetVal != depth)
//                           if (1)
                              goto TEMP_END2;

                           // set top right corner at top right corner of base bitmap
                           xyz[3].x = (float)pDib->width * invShrink * 8.0f;
                           xyz[3].z = ((float)pDib->height * invShrink * 16.0f);

                           xyz[2].x = xyz[3].x;
                           xyz[2].z = xyz[3].z -
                              ((float)pDibOv->height * invOvShrink * 16.0f);

                           xyz[1].x = xyz[3].x -
                              (float)pDibOv->width * invOvShrink * 16.0f;;
                           xyz[1].z = xyz[3].z -
                              ((float)pDibOv->height * invOvShrink * 16.0f);

                           xyz[0].x = xyz[3].x -
                              (float)pDibOv->width * invOvShrink * 16.0f;
                           xyz[0].z = xyz[3].z;

                           // add base object's offsets
                           for (i = 0; i < 4; i++)
                           {
                              xyz[i].x += pDib->xoffset;
                              xyz[i].z -= pDib->yoffset * 4.0f;
                           }

                           for (i = 0; i < 4; i++)
                           {
                              // add hotspot
                              xyz[i].x -= point.x * 16.000f * invShrink;
                              xyz[i].z -= (float)point.y * 16.000f * invShrink;
                              // add overlay offsets
                              xyz[i].x -= pDibOv2->xoffset * 16.000f * invShrink;
                              xyz[i].z -= pDibOv2->yoffset * 16.000f * invShrink;
                              // add hotspot
                              xyz[i].x -= point2.x * 16.000f * invOv2Shrink;
                              xyz[i].z -= (float)point2.y * 16.000f * invOv2Shrink;
                              // add overlay offsets
                              xyz[i].x -= pDibOv->xoffset * 16.000f * invOv2Shrink;
                              xyz[i].z -= pDibOv->yoffset * 16.000f * invOv2Shrink;
                           }

                           bHotspot = TRUE;
                        }
                        else
                           continue;
                     }
                     else
                        continue;
                  }
               }
            }
            else
               continue;

            if (bHotspot)
            {
               if (pRNode->obj.drawingtype == DRAWFX_SECONDTRANS)
               {
                  xLat0 = 0;
                  xLat1 = pRNode->obj.secondtranslation;
               }
               else if (pRNode->obj.drawingtype == DRAWFX_DOUBLETRANS)
               {
                  xLat0 = pOverlay->translation;
                  xLat1 = pRNode->obj.secondtranslation;
               }
               else
               {
                  xLat0 = pOverlay->translation;
                  xLat1 = 0;
               }

               pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDibOv, xLat0, xLat1,
                  pRNode->obj.drawingtype);

               if (NULL == pPacket)
                  continue;

               pChunk = D3DRenderChunkNew(pPacket);
               assert(pChunk);

               pChunk->drawingtype = DRAWFX_TRANSLUCENT50;
               pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
               pChunk->numIndices = 4;
               pChunk->numVertices = 4;
               pChunk->numPrimitives = pChunk->numVertices - 2;
               pChunk->xLat0 = xLat0;
               pChunk->xLat1 = xLat1;
               pChunk->zBias = zBias;

               zBias++;

               if (flags == DRAWFX_INVISIBLE)
               {
                  pPacket->pMaterialFctn = &D3DMaterialObjectInvisiblePacket;
                  pChunk->pMaterialFctn = &D3DMaterialObjectInvisibleChunk;
               }
               else
               {
                  pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
                  pChunk->pMaterialFctn = &D3DMaterialObjectChunk;
               }

               for (i = 0; i < 4; i++)
               {
                  pChunk->xyz[i].x = xyz[i].x;
                  pChunk->xyz[i].y = 0;//zHack;
                  pChunk->xyz[i].z = xyz[i].z;
               }

               angle = pRNode->angle - (params->viewer_angle + 3072);

               if (angle < -4096)
                  angle += 4096;

               /* Make sure that object is above the floor. */
               if (!GetRoomHeight(room->tree, &top, &bottom, &sector_flags, pRNode->motion.x, pRNode->motion.y))
               {
                  continue;
               }

               // No depth for override flags.
               if (ROOM_OVERRIDE_MASK & GetRoomFlags())
               {
                  depthf = 0.0f;
               }
               else
               {
                  // Set object depth based on "depth" sector flags
                  depthf = sector_depths[SectorDepth(sector_flags)];
               }

               MatrixTranslate(&mat, (float)pRNode->motion.x, (float)max(bottom,
                  pRNode->motion.z) - depthf, (float)pRNode->motion.y);
               MatrixMultiply(&pChunk->xForm, &mPlayerHeadingTrans, &mat);

               lastDistance = 0;

               if (pRNode->obj.drawingtype == DRAWFX_BLACK)
               {
                  bgra.b = bgra.g = bgra.r = 0;
                  bgra.a = 255;
               }
               else
               {
                  if (D3DObjectLightingCalc(room, pRNode, &bgra, 0))
                     pChunk->flags |= D3DRENDER_NOAMBIENT;
               }

               if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
                  bgra.a = D3DRENDER_TRANS25;
               if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
                  bgra.a = D3DRENDER_TRANS50;
               if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
                  bgra.a = D3DRENDER_TRANS75;
               if (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
                  bgra.a = D3DRENDER_TRANS50;
               if (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
                  bgra.a = D3DRENDER_TRANS50;
               if (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)
                  bgra.a = D3DRENDER_TRANS50;

               for (i = 0; i < 4; i++)
               {
                  pChunk->bgra[i].b = bgra.b;
                  pChunk->bgra[i].g = bgra.g;
                  pChunk->bgra[i].r = bgra.r;
                  pChunk->bgra[i].a = bgra.a;
               }

               {
                  float oneOverW, oneOverH;

                  oneOverW = (1.0f / pDibOv->width);
                  oneOverH = (1.0f / pDibOv->height);

                  pChunk->st0[0].s = 1.0f - oneOverW;
                  pChunk->st0[0].t = oneOverH;
                  pChunk->st0[1].s = 1.0f - oneOverW;
                  pChunk->st0[1].t = 1.0f - oneOverH;
                  pChunk->st0[2].s = oneOverW;
                  pChunk->st0[2].t = 1.0f - oneOverH;
                  pChunk->st0[3].s = oneOverW;
                  pChunk->st0[3].t = oneOverH;
               }

               pChunk->indices[0] = 1;
               pChunk->indices[1] = 2;
               pChunk->indices[2] = 0;
               pChunk->indices[3] = 3;

               // now add object to visible object list
               if ((pRNode->obj.id != INVALID_ID) && (pRNode->obj.id != player.id))
               {
                  D3DMATRIX   localToScreen, mat;
                  custom_xyzw   topLeft, topRight, bottomLeft, bottomRight, center;
                  ObjectRange *range = FindVisibleObjectById(pRNode->obj.id);
                  int         w, h;
                  int         tempLeft, tempRight, tempTop, tempBottom;
                  int         distX, distY, distance;

                  w = gD3DRect.right - gD3DRect.left;
                  h = gD3DRect.bottom - gD3DRect.top;

                  topLeft.x = pChunk->xyz[3].x;
                  topLeft.y = pChunk->xyz[3].z;
                  topLeft.z = 0;
                  topLeft.w = 1.0f;

                  topRight.x = pChunk->xyz[3].x;
                  topRight.y = pChunk->xyz[3].z;
                  topRight.z = 0;
                  topRight.w = 1.0f;

                  bottomLeft.x = pChunk->xyz[1].x;
                  bottomLeft.y = pChunk->xyz[1].z;
                  bottomLeft.z = 0;
                  bottomLeft.w = 1.0f;

                  bottomRight.x = pChunk->xyz[1].x;
                  bottomRight.y = pChunk->xyz[1].z;
                  bottomRight.z = 0;
                  bottomRight.w = 1.0f;

                  MatrixMultiply(&mat, &mPlayerViewTranslate, &mPlayerHPFudgeMul);
                  XformMatrixPerspective(&localToScreen, FOV_H, FOV_V, 1.0f, Z_RANGE);
                  MatrixMultiply(&mat, &pChunk->xForm, &mat);
                  MatrixMultiply(&localToScreen, &mat, &localToScreen);

                  MatrixMultiplyVector(&topLeft, &localToScreen, &topLeft);
                  MatrixMultiplyVector(&topRight, &localToScreen, &topRight);
                  MatrixMultiplyVector(&bottomLeft, &localToScreen, &bottomLeft);
                  MatrixMultiplyVector(&bottomRight, &localToScreen, &bottomRight);

                  topLeft.x /= topLeft.w;
                  topLeft.y /= topLeft.w;
                  topLeft.z /= topLeft.w;
                  bottomRight.x /= bottomRight.w;
                  bottomRight.y /= bottomRight.w;
                  bottomRight.z /= bottomRight.w;

                  topLeft.z = topLeft.z * 2.0f - 1.0f;
                  bottomRight.z = bottomRight.z * 2.0f - 1.0f;

                  topRight.x = bottomRight.x;
                  topRight.y = topLeft.y;
                  topRight.z = topLeft.z;
                  bottomLeft.x = topLeft.x;
                  bottomLeft.y = bottomRight.y;
                  bottomLeft.z = topLeft.z;

                  center.x = (topLeft.x + topRight.x) / 2.0f;
                  center.y = (topLeft.y + bottomLeft.y) / 2.0f;
                  center.z = topLeft.z;

                  if (flags == DRAWFX_INVISIBLE)
                  {
                     pChunk->st1[0].s = D3DRENDER_CLIP_TO_SCREEN_X(bottomRight.x, gScreenWidth) / gScreenWidth;
                     pChunk->st1[0].t = D3DRENDER_CLIP_TO_SCREEN_Y(topLeft.y, gScreenHeight) / gScreenHeight;
                     pChunk->st1[1].s = D3DRENDER_CLIP_TO_SCREEN_X(bottomRight.x, gScreenWidth) / gScreenWidth;
                     pChunk->st1[1].t = D3DRENDER_CLIP_TO_SCREEN_Y(bottomRight.y, gScreenHeight) / gScreenHeight;
                     pChunk->st1[2].s = D3DRENDER_CLIP_TO_SCREEN_X(topLeft.x, gScreenWidth) / gScreenWidth;
                     pChunk->st1[2].t = D3DRENDER_CLIP_TO_SCREEN_Y(bottomRight.y, gScreenHeight) / gScreenHeight;
                     pChunk->st1[3].s = D3DRENDER_CLIP_TO_SCREEN_X(topLeft.x, gScreenWidth) / gScreenWidth;
                     pChunk->st1[3].t = D3DRENDER_CLIP_TO_SCREEN_Y(topLeft.y, gScreenHeight) / gScreenHeight;

                        pChunk->st1[0].s -= (gFrame & 3) / 256.0f;
                        pChunk->st1[0].t -= (gFrame & 3) / 256.0f;
                        pChunk->st1[1].s -= (gFrame & 3) / 256.0f;
                        pChunk->st1[1].t += (gFrame & 3) / 256.0f;
                        pChunk->st1[2].s += (gFrame & 3) / 256.0f;
                        pChunk->st1[2].t += (gFrame & 3) / 256.0f;
                        pChunk->st1[3].s += (gFrame & 3) / 256.0f;
                        pChunk->st1[3].t -= (gFrame & 3) / 256.0f;
                  }

                  if (
                     (
                     (D3DRENDER_CLIP(topLeft.x, 1.0f) &&
                     D3DRENDER_CLIP(topLeft.y, 1.0f)) ||
                     (D3DRENDER_CLIP(bottomLeft.x, 1.0f) &&
                     D3DRENDER_CLIP(bottomLeft.y, 1.0f)) ||
                     (D3DRENDER_CLIP(topRight.x, 1.0f) &&
                     D3DRENDER_CLIP(topRight.y, 1.0f)) ||
                     (D3DRENDER_CLIP(bottomRight.x, 1.0f) &&
                     D3DRENDER_CLIP(bottomRight.y, 1.0f)) ||
                     (D3DRENDER_CLIP(center.x, 1.0f) &&
                     D3DRENDER_CLIP(center.y, 1.0f))
                     ) &&
                     D3DRENDER_CLIP(topLeft.z, 1.0f))
                  {
                     tempLeft   = (topLeft.x * w / 2) + (w >> 1);
                     tempRight  = (bottomRight.x * w / 2) + (w >> 1);
                     tempTop    = (topLeft.y * -h / 2) + (h >> 1);
                     tempBottom = (bottomRight.y * -h / 2) + (h >> 1);

                     if (config.large_area)
                     {
                        tempLeft >>= 1;
                        tempRight >>= 1;
                        tempTop >>= 1;
                        tempBottom >>= 1;
                     }

                     distX = pRNode->motion.x - player.x;
                     distY = pRNode->motion.y - player.y;

                     distance = DistanceGet(distX, distY);

                     if (range == NULL)
                     {
                        // Set up new visible object
                        range = &visible_objects[num_visible_objects];
                        range->id       = pRNode->obj.id;
                        range->distance = distance;
                        range->left_col   = tempLeft;
                        range->right_col  = tempRight;
                        range->top_row    = tempTop;
                        range->bottom_row = tempBottom;

                        num_visible_objects = ++num_visible_objects > MAXOBJECTS ? MAXOBJECTS : num_visible_objects;
                     }
                     
                     /* Record boundaries of drawing area */
                     range->left_col   = min(range->left_col, tempLeft);
                     range->right_col  = max(range->right_col, tempRight);

                     if (tempTop <= range->top_row)
                     {
                        int   i;
                        pRNode->boundingHeightAdjust =
                           ((pDib->height / pDib->shrink) - pDib->yoffset) / 4.0f;
                        i = 0;
                     }

                     range->top_row    = min(range->top_row, tempTop);
                     range->bottom_row = max(range->bottom_row, tempBottom);
                  }
               }

               if (pRNode->obj.id != INVALID_ID && pRNode->obj.id == GetUserTargetID() &&
                  (pRNode->obj.drawingtype != DRAWFX_INVISIBLE))
               {
                  pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDibOv, xLat0, xLat1,
                                             pRNode->obj.drawingtype);
                  if (NULL == pPacket)
                     continue;
                  pPacket->pMaterialFctn = &D3DMaterialObjectPacket;

                  pChunk = D3DRenderChunkNew(pPacket);
                  assert(pChunk);

                  pChunk->drawingtype = DRAWFX_TRANSLUCENT50;
                  pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
                  pChunk->numIndices = 4;
                  pChunk->numVertices = 4;
                  pChunk->numPrimitives = pChunk->numVertices - 2;
                  pChunk->xLat0 = xLat0;
                  pChunk->xLat1 = xLat1;
                  pChunk->zBias = ZBIAS_TARGETED;

                  MatrixMultiply(&pChunk->xForm, &mPlayerHeadingTrans, &mat);

                  if (flags == DRAWFX_INVISIBLE)
                  {
                     pChunk->pMaterialFctn = &D3DMaterialObjectInvisibleChunk;
                  }
                  else
                  {
                     pChunk->pMaterialFctn = &D3DMaterialTargetedObjectChunk;
                  }

                  D3DObjectLightingCalc(room, pRNode, &bgra, 0);

                  for (i = 0; i < 4; i++)
                  {
                     pChunk->xyz[i].x = xyz[i].x;
                     pChunk->xyz[i].y = 0;
                     pChunk->xyz[i].z = xyz[i].z;

                     switch (config.halocolor)
                     {
                        case 0:
                           pChunk->bgra[i].b = 0;
                           pChunk->bgra[i].g = 0;
                           pChunk->bgra[i].r = min(bgra.r * 2.0f, 255);
                           pChunk->bgra[i].a = 255;//D3DRENDER_TRANS50;
                        break;

                        case 1:
                           pChunk->bgra[i].b = min(bgra.r * 2.0f, 255);
                           pChunk->bgra[i].g = 0;
                           pChunk->bgra[i].r = 0;
                           pChunk->bgra[i].a = 255;//D3DRENDER_TRANS50;
                        break;

                        default:
                           pChunk->bgra[i].b = 0;
                           pChunk->bgra[i].g = min(bgra.r * 2.0f, 255);
                           pChunk->bgra[i].r = 0;
                           pChunk->bgra[i].a = 255;//D3DRENDER_TRANS50;
                        break;
                     }
                  }

                  pChunk->xyz[0].x -= 96.0f  * invOvShrink;
                  pChunk->xyz[0].z += 96.0f  * invOvShrink;
                  pChunk->xyz[1].x -= 96.0f  * invOvShrink;
                  pChunk->xyz[1].z -= 96.0f  * invOvShrink;
                  pChunk->xyz[2].x += 96.0f  * invOvShrink;
                  pChunk->xyz[2].z -= 96.0f  * invOvShrink;
                  pChunk->xyz[3].x += 96.0f  * invOvShrink;
                  pChunk->xyz[3].z += 96.0f  * invOvShrink;

                  pChunk->st0[0].s = 1.0f;
                  pChunk->st0[0].t = 0.0f;
                  pChunk->st0[1].s = 1.0f;
                  pChunk->st0[1].t = 1.0f;
                  pChunk->st0[2].s = 0.0f;
                  pChunk->st0[2].t = 1.0f;
                  pChunk->st0[3].s = 0.0f;
                  pChunk->st0[3].t = 0.0f;

                  pChunk->indices[0] = 1;
                  pChunk->indices[1] = 2;
                  pChunk->indices[2] = 0;
                  pChunk->indices[3] = 3;
               }

#ifndef NODPRINTFS
               gNumObjects++;
#endif
TEMP_END2:
               {
                  int i = 0;
               }
            }
         }
      }
   }
}

void D3DRenderProjectilesDrawNew(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params)
{
   D3DMATRIX         mat;
   Projectile         *pProjectile;
   list_type         list;
   long            dx, dy, angle;
   PDIB            pDib;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   // base objects
   for (list = room->projectiles; list != NULL; list = list->next)
   {
      BYTE   xLat0, xLat1;

      pProjectile = (Projectile *)list->data;

      dx = pProjectile->motion.x - params->viewer_x;
      dy = pProjectile->motion.y - params->viewer_y;

      angle = (pProjectile->angle - intATan2(-dy,-dx)) & NUMDEGREES_MASK;

      pDib = GetObjectPdib(pProjectile->icon_res, angle, pProjectile->animate.group);

      if (NULL == pDib)
         continue;

      xLat0 = pProjectile->translation;
      xLat1 = 0;

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, xLat0, xLat1, 0);
      if (NULL == pPacket)
         return;

      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);

      pChunk->flags = pProjectile->flags;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = pProjectile->translation;
      pChunk->xLat1 = 0;
      pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = &D3DMaterialObjectChunk;

      angle = pProjectile->angle - (params->viewer_angle + 3072);

      if (angle < -4096)
         angle += 4096;

      MatrixTranslate(&mat, (float)pProjectile->motion.x, (float)pProjectile->motion.z,
         (float)pProjectile->motion.y);
      MatrixMultiply(&pChunk->xForm, &mPlayerHeadingTrans, &mat);

      pChunk->xyz[0].x = (float)pDib->width / (float)pDib->shrink * -8.0f + (float)pDib->xoffset;
      pChunk->xyz[0].z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;

      pChunk->xyz[1].x = (float)pDib->width / (float)pDib->shrink * -8.0f + (float)pDib->xoffset;
      pChunk->xyz[1].z = -(float)pDib->yoffset * 4.0f;

      pChunk->xyz[2].x = (float)pDib->width / (float)pDib->shrink * 8.0f + (float)pDib->xoffset;
      pChunk->xyz[2].z = -(float)pDib->yoffset * 4.0f;

      pChunk->xyz[3].x = (float)pDib->width / (float)pDib->shrink * 8.0f + (float)pDib->xoffset;
      pChunk->xyz[3].z = ((float)pDib->height / (float)pDib->shrink * 16.0f) - (float)pDib->yoffset * 4.0f;

      {
         float   oneOverW, oneOverH;

         oneOverW = 1.0f / pDib->width;
         oneOverH = 1.0f / pDib->height;

         pChunk->st0[0].s = 1.0f - oneOverW;
         pChunk->st0[0].t = oneOverH;
         pChunk->st0[1].s = 1.0f - oneOverW;
         pChunk->st0[1].t = 1.0f - oneOverH;
         pChunk->st0[2].s = oneOverW;
         pChunk->st0[2].t = 1.0f - oneOverH;
         pChunk->st0[3].s = oneOverW;
         pChunk->st0[3].t = oneOverH;
      }

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      unsigned char alpha;
      if ((pProjectile->flags & PROJ_FLAG_TRANSPARENT50) == PROJ_FLAG_TRANSPARENT50)
      {
         alpha = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      else
      {
         alpha = COLOR_MAX;
         pChunk->alphaRef = TEMP_ALPHA_REF;
      }

      for (int i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = COLOR_MAX;
         pChunk->bgra[i].g = COLOR_MAX;
         pChunk->bgra[i].r = COLOR_MAX;
         pChunk->bgra[i].a = alpha;
      }

#ifndef NODPRINTFS
      gNumObjects++;
#endif
   }
}

void D3DRenderPlayerOverlaysDraw(d3d_render_pool_new *pPool, room_type *room, Draw3DParams *params)
{
   room_contents_node   *pRNode;
   PDIB            pDib;
   custom_bgra         bgra;
   AREA            objArea;
   float            screenW, screenH;

   int i, count;
   object_node *obj;
   list_type overlays;
   BYTE drawingtype;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   screenW = (float)(gD3DRect.right - gD3DRect.left) / (float)(MAXX * stretchfactor);
   screenH = (float)(gD3DRect.bottom - gD3DRect.top) / (float)(MAXY * stretchfactor);

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAREF, TEMP_ALPHA_REF);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHABLENDENABLE, FALSE);

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &mIdentity);
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &mIdentity);

   // Get player's object drawing flags for special drawing effects
   pRNode = GetRoomObjectById(player.id);
   if (pRNode == NULL)
      drawingtype = 0;
   else
      drawingtype = pRNode->obj.drawingtype;

   for (i=0; i < NUM_PLAYER_OVERLAYS; i++)
   {
      BYTE   xLat0, xLat1;

      PlayerOverlay *pOverlay = &player.poverlays[i];

      if (pOverlay->obj == NULL || pOverlay->hotspot == 0)
         continue;

      obj = pOverlay->obj;

      pDib = GetObjectPdib(obj->icon_res, 0, obj->animate->group);
      if (pDib == NULL)
         continue;

      D3DComputePlayerOverlayArea(pDib, pOverlay->hotspot, &objArea);
//      objArea.x *= screenW;
//      objArea.y *= screenH;
//      objArea.cx /= screenW;
//      objArea.cy /= screenH;

      overlays = *(obj->overlays);

      if (overlays != NULL)
         D3DRenderPlayerOverlayOverlaysDraw(pPool, overlays, pDib, room, params,
            &objArea, TRUE);

      if (obj->drawingtype == DRAWFX_SECONDTRANS)
      {
         xLat0 = 0;
         xLat1 = obj->secondtranslation;
      }
      else if (obj->drawingtype == DRAWFX_DOUBLETRANS)
      {
         xLat0 = obj->translation;
         xLat1 = obj->secondtranslation;
      }
      else
      {
         xLat0 = obj->translation;
         xLat1 = 0;
      }

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDib, xLat0, xLat1,
         pRNode->obj.drawingtype);
      if (NULL == pPacket)
         return;

      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);

      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = xLat0;
      pChunk->xLat1 = xLat1;

      if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
      {
         pPacket->pMaterialFctn = &D3DMaterialObjectInvisiblePacket;
         pChunk->pMaterialFctn = &D3DMaterialObjectInvisibleChunk;
      }
      else
      {
         pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
         pChunk->pMaterialFctn = &D3DMaterialObjectChunk;
      }

      pChunk->drawingtype = pRNode->obj.drawingtype;

      MatrixIdentity(&pChunk->xForm);

      if (pRNode->obj.drawingtype == DRAWFX_BLACK)
      {
         bgra.b = bgra.g = bgra.r = 0;
         bgra.a = 255;
      }
      else
      {
         D3DObjectLightingCalc(room, pRNode, &bgra, 0);
/*            lastDistance = D3DRenderObjectLightGetNearest(pRNode);

         light = D3DRenderObjectGetLight(room->tree, pRNode);

         if ((pRNode->obj.flags & OF_FLICKERING) || (pRNode->obj.flags & OF_FLASHING))
            light = GetLightPaletteIndex(D3DRENDER_LIGHT_DISTANCE, light, FINENESS,
                     -pRNode->obj.lightAdjust);
         else
            light = GetLightPaletteIndex(D3DRENDER_LIGHT_DISTANCE, light, FINENESS,
                     0);

         light = light * COLOR_AMBIENT / 64;

         bgra.r = bgra.g = bgra.b = min(255, light + lastDistance);
         bgra.a = 255;*/
      }

      if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
      {
         bgra.a = D3DRENDER_TRANS25;
         pChunk->alphaRef = D3DRENDER_TRANS25 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
      {
         bgra.a = D3DRENDER_TRANS75;
         pChunk->alphaRef = D3DRENDER_TRANS75 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      if (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)
      {
         bgra.a = D3DRENDER_TRANS50;
         pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
      }
      else
      {
         pChunk->alphaRef = TEMP_ALPHA_REF;
      }

      pChunk->xyz[0].x = pChunk->xyz[1].x = objArea.x;
      pChunk->xyz[0].z = pChunk->xyz[3].z = objArea.y;
      pChunk->xyz[2].x = pChunk->xyz[3].x = pChunk->xyz[0].x + objArea.cx;
      pChunk->xyz[2].z = pChunk->xyz[1].z = pChunk->xyz[0].z + objArea.cy;

//      D3DObjectLightingCalc(room, pRNode, &bgra);

      for (count = 0; count < 4; count++)
      {
         if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
         {
            pChunk->st1[count].s = pChunk->xyz[count].x / gScreenWidth;
            pChunk->st1[count].t = pChunk->xyz[count].z / gScreenHeight;
         }

         pChunk->xyz[count].x = D3DRENDER_SCREEN_TO_CLIP_X(pChunk->xyz[count].x,
            gScreenWidth);
         pChunk->xyz[count].z = D3DRENDER_SCREEN_TO_CLIP_Y(pChunk->xyz[count].z,
            gScreenHeight);
         pChunk->xyz[count].y = PLAYER_OVERLAY_Z;

         pChunk->bgra[count].b = bgra.b;
         pChunk->bgra[count].g = bgra.g;
         pChunk->bgra[count].r = bgra.r;
         pChunk->bgra[count].a = bgra.a;
      }

      {
         float   oneOverW, oneOverH;

         oneOverW = (1.0f / pDib->width);
         oneOverH = (1.0f / pDib->height);

         pChunk->st0[0].s = oneOverW;
         pChunk->st0[0].t = oneOverH;
         pChunk->st0[1].s = oneOverW;
         pChunk->st0[1].t = 1.0f - oneOverH;
         pChunk->st0[2].s = 1.0f - oneOverW;
         pChunk->st0[2].t = 1.0f - oneOverH;
         pChunk->st0[3].s = 1.0f - oneOverW;
         pChunk->st0[3].t = oneOverH;
      }

      pChunk->st1[0].t -= (gFrame & 3) / 256.0f;
      pChunk->st1[1].s -= (gFrame & 3) / 256.0f;
      pChunk->st1[1].t += (gFrame & 3) / 256.0f;
      pChunk->st1[2].s += (gFrame & 3) / 256.0f;
      pChunk->st1[2].t += (gFrame & 3) / 256.0f;
      pChunk->st1[3].s += (gFrame & 3) / 256.0f;
      pChunk->st1[3].t -= (gFrame & 3) / 256.0f;

/*      pChunk->st1[0].s = 0.0f;
      pChunk->st1[0].t = 0.0f;
      pChunk->st1[1].s = 0.0f;
      pChunk->st1[1].t = 1.0f;
      pChunk->st1[2].s = 1.0f;
      pChunk->st1[2].t = 1.0f;
      pChunk->st1[3].s = 1.0f;
      pChunk->st1[3].t = 0.0f;*/

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      if (overlays != NULL)
         D3DRenderPlayerOverlayOverlaysDraw(pPool, overlays, pDib, room, params,
            &objArea, FALSE);
   }
}

void D3DRenderPlayerOverlayOverlaysDraw(d3d_render_pool_new *pPool, list_type overlays,
      PDIB pDib, room_type *room, Draw3DParams *params, AREA *objArea, BOOL underlays)
{
   int               pass, depth;
   room_contents_node   *pRNode;
   custom_xyz         xyz[4];
   custom_bgra         bgra;
   PDIB            pDibOv;
   Overlay            *pOverlay;
   int               i, zBias;
   float            lastDistance, screenW, screenH;
   BYTE            xLat0, xLat1, drawingtype;

   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   screenW = (float)(gD3DRect.right - gD3DRect.left) / (float)(MAXX * (stretchfactor * 1.5f));
   screenH = (float)(gD3DRect.bottom - gD3DRect.top) / (float)(MAXY * (stretchfactor * 1.5f));

   // Get player's object drawing flags for special drawing effects
   pRNode = GetRoomObjectById(player.id);

   if (pRNode == NULL)
      drawingtype = 0;
   else
      drawingtype = pRNode->obj.drawingtype;

   pass = 0;

   if (overlays != NULL)
   {
      for (; pass < 3; pass++)
      {
         if (underlays)
            switch (pass)
            {
               case 0: depth = HOTSPOT_UNDERUNDER;
                  zBias = ZBIAS_UNDERUNDER;
               break;

               case 1: depth = HOTSPOT_UNDER;
                  zBias = ZBIAS_UNDER;
               break;

               case 2: depth = HOTSPOT_UNDEROVER;
                  zBias = ZBIAS_UNDEROVER;
               break;
            }
         else
            switch (pass)
            {
               case 0: depth = HOTSPOT_OVERUNDER;
                  zBias = ZBIAS_OVERUNDER;
               break;

               case 1: depth = HOTSPOT_OVER;
                  zBias = ZBIAS_OVER;
               break;

               case 2: depth = HOTSPOT_OVEROVER;
                  zBias = ZBIAS_OVEROVER;
               break;
            }

         pOverlay = (Overlay *)overlays->data;

         pDibOv = GetObjectPdib(pOverlay->icon_res, 0, pOverlay->animate.group);

         if ((NULL == pDib) || (NULL == pDibOv))
            continue;

         if (pOverlay->hotspot)
         {
            POINT   point;
            int      retVal;

            retVal = FindHotspotPdib(pDib, pOverlay->hotspot, &point);

            if (retVal != HOTSPOT_NONE)
            {
               if (retVal != depth)
                  continue;

               for (i = 0; i < 4; i++)
               {
                  // add hotspot
                  xyz[i].x = (float)point.x / pDib->shrink / (float)screenW;
                  xyz[i].z = (float)point.y / pDib->shrink / (float)screenH;
                  // add overlay offsets
                  xyz[i].x += (float)pDibOv->xoffset / pDibOv->shrink / (float)screenW;
                  xyz[i].z += (float)pDibOv->yoffset / pDibOv->shrink / (float)screenH;
               }

               xyz[1].z += (float)pDibOv->height / pDibOv->shrink / (float)screenH;
               xyz[2].x += (float)pDibOv->width / pDibOv->shrink / (float)screenW;
               xyz[2].z += (float)pDibOv->height / pDibOv->shrink / (float)screenH;
               xyz[3].x += (float)pDibOv->width / pDibOv->shrink / (float)screenW;
            }
            else
            {
               assert(0);
               // the hotspot wasn't in the base object pdib
               // must be an overlay on an overlay, so find base overlay
            }
         }

         for (i = 0; i < 4; i++)
         {
            xyz[i].x += objArea->x;
            xyz[i].z += objArea->y;
         }

         if (pRNode->obj.drawingtype == DRAWFX_SECONDTRANS)
         {
            xLat0 = 0;
            xLat1 = pRNode->obj.secondtranslation;
         }
         else if (pRNode->obj.drawingtype == DRAWFX_DOUBLETRANS)
         {
            xLat0 = pOverlay->translation;
            xLat1 = pRNode->obj.secondtranslation;
         }
         else
         {
            xLat0 = pOverlay->translation;
            xLat1 = 0;
         }

         pPacket = D3DRenderPacketFindMatch(pPool, NULL, pDibOv, xLat0, xLat1,
            pRNode->obj.drawingtype);
         if (NULL == pPacket)
            return;

         pChunk = D3DRenderChunkNew(pPacket);
         assert(pChunk);

         pChunk->drawingtype = pRNode->obj.drawingtype;
         pChunk->numIndices = 4;
         pChunk->numVertices = 4;
         pChunk->numPrimitives = pChunk->numVertices - 2;
         pChunk->xLat0 = xLat0;
         pChunk->xLat1 = xLat1;
         
         if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
         {
            pPacket->pMaterialFctn = &D3DMaterialObjectInvisiblePacket;
            pChunk->pMaterialFctn = &D3DMaterialObjectInvisibleChunk;
         }
         else
         {
            pPacket->pMaterialFctn = &D3DMaterialObjectPacket;
            pChunk->pMaterialFctn = &D3DMaterialObjectChunk;
         }

         MatrixIdentity(&pChunk->xForm);

         lastDistance = 0;

         if (pRNode->obj.drawingtype == DRAWFX_BLACK)
         {
            bgra.b = bgra.g = bgra.r = 0;
            bgra.a = 255;
         }
         else
         {
            D3DObjectLightingCalc(room, pRNode, &bgra, 0);
         }

         if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT25)
         {
            bgra.a = D3DRENDER_TRANS25;
            pChunk->alphaRef = D3DRENDER_TRANS25 - 1;
         }
         if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT50)
         {
            bgra.a = D3DRENDER_TRANS50;
            pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
         }
         if (pRNode->obj.drawingtype == DRAWFX_TRANSLUCENT75)
         {
            bgra.a = D3DRENDER_TRANS75;
            pChunk->alphaRef = D3DRENDER_TRANS75 - 1;
         }
         if (pRNode->obj.drawingtype == DRAWFX_DITHERTRANS)
         {
            bgra.a = D3DRENDER_TRANS50;
            pChunk->alphaRef = 1;
         }
         if (pRNode->obj.drawingtype == DRAWFX_DITHERINVIS)
         {
            bgra.a = D3DRENDER_TRANS50;
            pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
         }
         if (pRNode->obj.drawingtype == DRAWFX_DITHERGREY)
         {
            bgra.a = D3DRENDER_TRANS50;
            pChunk->alphaRef = D3DRENDER_TRANS50 - 1;
         }
         else
         {
            pChunk->alphaRef = TEMP_ALPHA_REF;
         }

         for (i = 0; i < 4; i++)
         {
            if (pRNode->obj.drawingtype == DRAWFX_INVISIBLE)
            {
               pChunk->st1[i].s = xyz[i].x / gScreenWidth;
               pChunk->st1[i].t = xyz[i].z / gScreenHeight;
            }

            pChunk->xyz[i].x = D3DRENDER_SCREEN_TO_CLIP_X(xyz[i].x, gScreenWidth);
            pChunk->xyz[i].z = D3DRENDER_SCREEN_TO_CLIP_Y(xyz[i].z, gScreenHeight);
            pChunk->xyz[i].y = PLAYER_OVERLAY_Z;

            pChunk->bgra[i].b = bgra.b;
            pChunk->bgra[i].g = bgra.g;
            pChunk->bgra[i].r = bgra.r;
            pChunk->bgra[i].a = bgra.a;
         }

         {
            float   oneOverW, oneOverH;

            oneOverW = (1.0f / pDib->width);
            oneOverH = (1.0f / pDib->height);

            pChunk->st0[0].s = oneOverW;
            pChunk->st0[0].t = oneOverH;
            pChunk->st0[1].s = oneOverW;
            pChunk->st0[1].t = 1.0f - oneOverH;
            pChunk->st0[2].s = 1.0f - oneOverW;
            pChunk->st0[2].t = 1.0f - oneOverH;
            pChunk->st0[3].s = 1.0f - oneOverW;
            pChunk->st0[3].t = oneOverH;
         }

         pChunk->st1[0].t -= (gFrame & 3) / 256.0f;
         pChunk->st1[1].s -= (gFrame & 3) / 256.0f;
         pChunk->st1[1].t += (gFrame & 3) / 256.0f;
         pChunk->st1[2].s += (gFrame & 3) / 256.0f;
         pChunk->st1[2].t += (gFrame & 3) / 256.0f;
         pChunk->st1[3].s += (gFrame & 3) / 256.0f;
         pChunk->st1[3].t -= (gFrame & 3) / 256.0f;

         pChunk->indices[0] = 1;
         pChunk->indices[1] = 2;
         pChunk->indices[2] = 0;
         pChunk->indices[3] = 3;
      }
   }
}

void D3DRenderViewElementsDraw(d3d_render_pool_new *pPool)
{
   int                  i;
   float               screenW, screenH, foffset;
   int                  offset = 0;
   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   screenW = (float)(gD3DRect.right - gD3DRect.left) / (float)(MAXX * stretchfactor);
   screenH = (float)(gD3DRect.bottom - gD3DRect.top) / (float)(MAXY * stretchfactor);

   if (GetFocus() == hMain)
      offset = 4;

   foffset = 1.0f / 64.0f;

   if (stretchfactor == 1)
   {
      // top left
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[0 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(72 / (float)screenH, gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(58 / (float)screenW, gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(72 / (float)screenH, gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(58 / (float)screenW, gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // top right
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[1 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 58 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 58 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(72 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(72 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // bottom left
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[2 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 73 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(58 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight,
         gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(58 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 73 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // bottom right
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[3 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 58 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 73 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 58 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight,
         gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight,
         gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 73 / (float)screenH, gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }
   else
   {
      // top left
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[0 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(56 / (float)screenH, gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(46 / (float)screenW, gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(56 / (float)screenH, gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(46 / (float)screenW, gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // top right
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[1 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 47 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 47 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(56 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(56 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // bottom left
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[2 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 56 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 0.5f, gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(46 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 0.5f,
         gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(46 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 56 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;

      // bottom right
      pPacket = D3DRenderPacketNew(pPool);
      pPacket->pDib = NULL;
      pPacket->pTexture = gpViewElements[3 + offset];
      pPacket->xLat0 = 0;
      pPacket->xLat1 = 0;
      pPacket->effect = 0;
      pPacket->size = pPool->packetSize;

      pChunk = D3DRenderChunkNew(pPacket);
      pChunk->flags = 0;
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pChunk->xLat0 = 0;
      pChunk->xLat1 = 0;

      pPacket->pMaterialFctn = D3DMaterialObjectPacket;
      pChunk->pMaterialFctn = D3DMaterialNone;

      pChunk->xyz[0].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 47 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[0].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 56 / (float)screenH,
         gScreenHeight);
      pChunk->xyz[0].y = VIEW_ELEMENT_Z;
      pChunk->xyz[1].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth - 47 / (float)screenW,
         gScreenWidth);
      pChunk->xyz[1].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 0.5f,
         gScreenHeight);
      pChunk->xyz[1].y = VIEW_ELEMENT_Z;
      pChunk->xyz[2].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[2].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 0.5f,
         gScreenHeight);
      pChunk->xyz[2].y = VIEW_ELEMENT_Z;
      pChunk->xyz[3].x = D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth,
         gScreenWidth);
      pChunk->xyz[3].z = D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight - 56 / (float)screenH, gScreenHeight);
      pChunk->xyz[3].y = VIEW_ELEMENT_Z;

      for (i = 0; i < 4; i++)
      {
         pChunk->bgra[i].b = 255;
         pChunk->bgra[i].g = 255;
         pChunk->bgra[i].r = 255;
         pChunk->bgra[i].a = 255;
      }

      pChunk->st0[0].s = foffset;
      pChunk->st0[0].t = foffset;
      pChunk->st0[1].s = foffset;
      pChunk->st0[1].t = 1.0f - foffset;
      pChunk->st0[2].s = 1.0f - foffset;
      pChunk->st0[2].t = 1.0f - foffset;
      pChunk->st0[3].s = 1.0f - foffset;
      pChunk->st0[3].t = foffset;

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }
}

void D3DPostOverlayEffects(d3d_render_pool_new *pPool)
{
   static DWORD         timeLastFrame = 0;
   DWORD               timeCurrent, timeDelta;
   int                  i;
   d3d_render_chunk_new   *pChunk;
   d3d_render_packet_new   *pPacket;

   timeCurrent = timeGetTime();
   timeDelta = timeCurrent - timeLastFrame;
   timeLastFrame = timeCurrent;

   // Flash of XLAT.  Could be color, blindness, whatever.
   if (effects.flashxlat != XLAT_IDENTITY)
   {
      custom_bgra   bgra;

      effects.duration -= (int)timeDelta;
      switch (effects.flashxlat)
      {
         case XLAT_BLEND10RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 25;
         break;

         case XLAT_BLEND20RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 50;
         break;

         case XLAT_BLEND25RED:
         case XLAT_BLEND30RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 75;
         break;

         case XLAT_BLEND40RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 100;
         break;

         case XLAT_BLEND50RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 125;
         break;

         case XLAT_BLEND60RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 150;
         break;

         case XLAT_BLEND70RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 175;
         break;

         case XLAT_BLEND75RED:
         case XLAT_BLEND80RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 200;
         break;

         case XLAT_BLEND90RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 225;
         break;

         case XLAT_BLEND100RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 255;
         break;

         case XLAT_BLEND10WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 25;
         break;

         case XLAT_BLEND20WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 50;
         break;

         case XLAT_BLEND30WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 75;
         break;

         case XLAT_BLEND40WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 100;
         break;

         case XLAT_BLEND50WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 125;
         break;

         case XLAT_BLEND60WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 150;
         break;

         case XLAT_BLEND70WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 175;
         break;

         case XLAT_BLEND80WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 200;
         break;

         case XLAT_BLEND90WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 225;
         break;

         case XLAT_BLEND100WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 255;
         break;

         case XLAT_BLEND25YELLOW:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 255;
         break;

         case XLAT_BLEND25GREEN:
            bgra.b = 0; bgra.g = COLOR_MAX; bgra.r = 0; bgra.a = 64;
         break;
         case XLAT_BLEND50GREEN:
            bgra.b = 0; bgra.g = COLOR_MAX; bgra.r = 0; bgra.a = 128;
         break;
         case XLAT_BLEND75GREEN:
            bgra.b = 0; bgra.g = COLOR_MAX; bgra.r = 0; bgra.a = 192;
         break;
         case XLAT_BLEND25BLUE:
            bgra.b = COLOR_MAX; bgra.g = 0; bgra.r = 0; bgra.a = 64;
         break;
         case XLAT_BLEND50BLUE:
            bgra.b = COLOR_MAX; bgra.g = 0; bgra.r = 0; bgra.a = 128;
         break;
         case XLAT_BLEND75BLUE:
            bgra.b = COLOR_MAX; bgra.g = 0; bgra.r = 0; bgra.a = 192;
         break;

         default:
            bgra.b = 0; bgra.g = 0; bgra.r = 0; bgra.a = 255;
         break;
      }

      if (effects.duration <= 0)
      {
         effects.flashxlat = XLAT_IDENTITY;
         effects.duration = 0;
      }

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, NULL, 0, 0, 0);
      if (NULL == pPacket)
         return;
      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pPacket->pMaterialFctn = &D3DMaterialEffectPacket;
      pChunk->pMaterialFctn = &D3DMaterialEffectChunk;
      MatrixIdentity(&pChunk->xForm);

      for (i = 0; i < 4; i++)
      {
         CHUNK_BGRA_SET(pChunk, i, bgra.b, bgra.g, bgra.r, bgra.a);
      }

      CHUNK_XYZ_SET(pChunk, 0, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 1, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 2, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 3, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }

   if (effects.xlatOverride > 0)
   {
      custom_bgra   bgra;

      switch (effects.xlatOverride)
      {
         case XLAT_BLEND10RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 25;
         break;

         case XLAT_BLEND20RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 50;
         break;

         case XLAT_BLEND25RED:
         case XLAT_BLEND30RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 75;
         break;

         case XLAT_BLEND40RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 100;
         break;

         case XLAT_BLEND50RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 125;
         break;

         case XLAT_BLEND60RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 150;
         break;

         case XLAT_BLEND70RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 175;
         break;

         case XLAT_BLEND75RED:
         case XLAT_BLEND80RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 200;
         break;

         case XLAT_BLEND90RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 225;
         break;

         case XLAT_BLEND100RED:
            bgra.b = 0; bgra.g = 0; bgra.r = COLOR_MAX; bgra.a = 255;
         break;

         case XLAT_BLEND10WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 25;
         break;

         case XLAT_BLEND20WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 50;
         break;

         case XLAT_BLEND30WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 75;
         break;

         case XLAT_BLEND40WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 100;
         break;

         case XLAT_BLEND50WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 125;
         break;

         case XLAT_BLEND60WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 150;
         break;

         case XLAT_BLEND70WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 175;
         break;

         case XLAT_BLEND80WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 200;
         break;

         case XLAT_BLEND90WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 225;
         break;

         case XLAT_BLEND100WHITE:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 255;
         break;

         case XLAT_BLEND25YELLOW:
            bgra.b = COLOR_MAX; bgra.g = COLOR_MAX; bgra.r = COLOR_MAX; bgra.a = 255;
         break;

         case XLAT_BLEND25GREEN:
            bgra.b = 0; bgra.g = COLOR_MAX; bgra.r = 0; bgra.a = 64;
         break;
         case XLAT_BLEND50GREEN:
            bgra.b = 0; bgra.g = COLOR_MAX; bgra.r = 0; bgra.a = 128;
         break;
         case XLAT_BLEND75GREEN:
            bgra.b = 0; bgra.g = COLOR_MAX; bgra.r = 0; bgra.a = 192;
         break;
         case XLAT_BLEND25BLUE:
            bgra.b = COLOR_MAX; bgra.g = 0; bgra.r = 0; bgra.a = 64;
         break;
         case XLAT_BLEND50BLUE:
            bgra.b = COLOR_MAX; bgra.g = 0; bgra.r = 0; bgra.a = 128;
         break;
         case XLAT_BLEND75BLUE:
            bgra.b = COLOR_MAX; bgra.g = 0; bgra.r = 0; bgra.a = 192;
         break;

         default:
            bgra.b = 0; bgra.g = 0; bgra.r = 0; bgra.a = 255;
         break;
      }

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, NULL, 0, 0, 0);
      if (NULL == pPacket)
         return;
      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pPacket->pMaterialFctn = &D3DMaterialEffectPacket;
      pChunk->pMaterialFctn = &D3DMaterialEffectChunk;
      MatrixIdentity(&pChunk->xForm);

      for (i = 0; i < 4; i++)
      {
         CHUNK_BGRA_SET(pChunk, i, bgra.b, bgra.g, bgra.r, bgra.a);
      }

      CHUNK_XYZ_SET(pChunk, 0, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 1, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 2, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 3, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }

   // May be drawn over room or map.

   // whiteout
   if (effects.whiteout)
   {
      int   whiteout = min(effects.whiteout, 500);

      whiteout = whiteout * COLOR_MAX / 500;
      whiteout = max(whiteout, 200);

      pPacket = D3DRenderPacketFindMatch(pPool, NULL, NULL, 0, 0, 0);
      if (NULL == pPacket)
         return;
      pChunk = D3DRenderChunkNew(pPacket);
      assert(pChunk);
      pChunk->numIndices = 4;
      pChunk->numVertices = 4;
      pChunk->numPrimitives = pChunk->numVertices - 2;
      pPacket->pMaterialFctn = &D3DMaterialEffectPacket;
      pChunk->pMaterialFctn = &D3DMaterialEffectChunk;
      MatrixIdentity(&pChunk->xForm);

      for (i = 0; i < 4; i++)
      {
         CHUNK_BGRA_SET(pChunk, i, COLOR_MAX, COLOR_MAX, COLOR_MAX, whiteout);
      }

      CHUNK_XYZ_SET(pChunk, 0, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 1, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 2, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
      CHUNK_XYZ_SET(pChunk, 3, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
         D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));

      pChunk->indices[0] = 1;
      pChunk->indices[1] = 2;
      pChunk->indices[2] = 0;
      pChunk->indices[3] = 3;
   }

   // Pain (always drawn last).
   if (config.pain)
   {
      if (effects.pain)
      {
         int   pain = min(effects.pain, 2000);

         pain = pain * 204 / 2000;

         pPacket = D3DRenderPacketFindMatch(pPool, NULL, NULL, 0, 0, 0);
         if (NULL == pPacket)
            return;
         pChunk = D3DRenderChunkNew(pPacket);
         assert(pChunk);
         pChunk->numIndices = 4;
         pChunk->numVertices = 4;
         pChunk->numPrimitives = pChunk->numVertices - 2;
         pPacket->pMaterialFctn = &D3DMaterialEffectPacket;
         pChunk->pMaterialFctn = &D3DMaterialEffectChunk;
         MatrixIdentity(&pChunk->xForm);

         for (i = 0; i < 4; i++)
         {
            CHUNK_BGRA_SET(pChunk, i, 0, 0, COLOR_MAX, pain);
         }

         CHUNK_XYZ_SET(pChunk, 0, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
            D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));
         CHUNK_XYZ_SET(pChunk, 1, D3DRENDER_SCREEN_TO_CLIP_X(0, gScreenWidth), 0,
            D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
         CHUNK_XYZ_SET(pChunk, 2, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
            D3DRENDER_SCREEN_TO_CLIP_Y(gScreenHeight, gScreenHeight));
         CHUNK_XYZ_SET(pChunk, 3, D3DRENDER_SCREEN_TO_CLIP_X(gScreenWidth, gScreenWidth), 0,
            D3DRENDER_SCREEN_TO_CLIP_Y(0, gScreenHeight));

         pChunk->indices[0] = 1;
         pChunk->indices[1] = 2;
         pChunk->indices[2] = 0;
         pChunk->indices[3] = 3;
      }
   }
}

LPDIRECT3DTEXTURE9 D3DRenderFramebufferTextureCreate(LPDIRECT3DTEXTURE9 pTex0,
                                                     LPDIRECT3DTEXTURE9 pTex1,
                                                     float width, float height)
{
   LPDIRECT3DSURFACE9   pSrc, pDest[2], pZBuf;
   RECT            rect;
   HRESULT            hr;
   d3d_render_packet_new   *pPacket;
   d3d_render_chunk_new   *pChunk;

   D3DCacheSystemReset(&gEffectCacheSystem);
   D3DRenderPoolReset(&gEffectPool, &D3DMaterialEffectPool);

   // get pointer to backbuffer surface and z/stencil surface
   hr = IDirect3DDevice9_GetRenderTarget(gpD3DDevice, 0, &pSrc);
   hr = IDirect3DDevice9_GetDepthStencilSurface(gpD3DDevice, &pZBuf);

   // get pointer to texture surface for rendering
   hr = IDirect3DTexture9_GetSurfaceLevel(pTex0, 0, &pDest[0]);
   hr = IDirect3DTexture9_GetSurfaceLevel(pTex1, 0, &pDest[1]);

   rect.left = rect.top = 0;
   rect.right = gScreenWidth;
   rect.bottom = gScreenHeight;

   // copy framebuffer to texture
   hr = IDirect3DDevice9_StretchRect(gpD3DDevice, pSrc, &rect, pDest[0], &rect, D3DTEXF_NONE);
   
   // clear local->screen transforms
   hr = IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &mIdentity);
   hr = IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_VIEW, &mIdentity);
   hr = IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_PROJECTION, &mIdentity);

   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, FALSE);
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, FALSE);

   hr = IDirect3DDevice9_SetRenderTarget(gpD3DDevice, 0, pDest[1]);

   pPacket = D3DRenderPacketNew(&gEffectPool);
   if (NULL == pPacket)
      return NULL;
   pChunk = D3DRenderChunkNew(pPacket);
   pPacket->pMaterialFctn = D3DMaterialEffectPacket;
   pChunk->pMaterialFctn = D3DMaterialEffectChunk;
   pPacket->pTexture = pTex0;
   pChunk->numIndices = pChunk->numVertices = 4;
   pChunk->numPrimitives = pChunk->numVertices - 2;
   MatrixIdentity(&pChunk->xForm);

   CHUNK_XYZ_SET(pChunk, 0, D3DRENDER_SCREEN_TO_CLIP_X(0, 256),
      0, D3DRENDER_SCREEN_TO_CLIP_Y(0, 256));
   CHUNK_XYZ_SET(pChunk, 1, D3DRENDER_SCREEN_TO_CLIP_X(0, 256),
      0, D3DRENDER_SCREEN_TO_CLIP_Y(256, 256));
   CHUNK_XYZ_SET(pChunk, 2, D3DRENDER_SCREEN_TO_CLIP_X(256, 256),
      0, D3DRENDER_SCREEN_TO_CLIP_Y(256, 256));
   CHUNK_XYZ_SET(pChunk, 3, D3DRENDER_SCREEN_TO_CLIP_X(256, 256),
      0, D3DRENDER_SCREEN_TO_CLIP_Y(0, 256));

   CHUNK_ST0_SET(pChunk, 0, 0.0f, 0.0f);
   CHUNK_ST0_SET(pChunk, 1, 0.0f, gScreenHeight / 1024.0f);
   CHUNK_ST0_SET(pChunk, 2, gScreenWidth / 1024.0f, gScreenHeight / 1024.0f);
   CHUNK_ST0_SET(pChunk, 3, gScreenWidth / 1024.0f, 0.0f);

   CHUNK_BGRA_SET(pChunk, 0, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX);
   CHUNK_BGRA_SET(pChunk, 1, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX);
   CHUNK_BGRA_SET(pChunk, 2, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX);
   CHUNK_BGRA_SET(pChunk, 3, COLOR_MAX, COLOR_MAX, COLOR_MAX, COLOR_MAX);

   CHUNK_INDEX_SET(pChunk, 0, 1);
   CHUNK_INDEX_SET(pChunk, 1, 2);
   CHUNK_INDEX_SET(pChunk, 2, 0);
   CHUNK_INDEX_SET(pChunk, 3, 3);

   D3DCacheFill(&gEffectCacheSystem, &gEffectPool, 1);
   D3DCacheFlush(&gEffectCacheSystem, &gEffectPool, 1, D3DPT_TRIANGLESTRIP);

   // restore render target to backbuffer
   hr = IDirect3DDevice9_SetRenderTarget(gpD3DDevice, 0, pSrc);
   hr = IDirect3DDevice9_SetDepthStencilSurface(gpD3DDevice, pZBuf);
   
   hr = IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ZWRITEENABLE, TRUE);
   
   hr = IDirect3DSurface9_Release(pSrc);
   hr = IDirect3DSurface9_Release(pZBuf);
   hr = IDirect3DSurface9_Release(pDest[0]);
   hr = IDirect3DSurface9_Release(pDest[1]);

   return pTex1;
}

/*
* IsHidden: Checks the coordinates (bounding box) against the player's frustum. If
*           the box doesn't fall inside the player's view, returns TRUE. Doesn't
*           account for height or occlusion. Modified from Bbox_shadowed in drawbsp.c.
*/
Bool IsHidden(Draw3DParams *params, long x0, long y0, long x1, long y1)
{
   long center_a, center_b, left_a, left_b, right_a, right_b;

   center_a = COS(params->viewer_angle) >> 6;
   center_b = SIN(params->viewer_angle) >> 6;

   left_a = -center_b + ((center_a * MAXX) >> LOG_VIEWER_DISTANCE);
   left_b = center_a + ((center_b * MAXX) >> LOG_VIEWER_DISTANCE);

   right_a = center_b + ((center_a * MAXX) >> LOG_VIEWER_DISTANCE);
   right_b = -center_a + ((center_b * MAXX) >> LOG_VIEWER_DISTANCE);

   x0 -= params->viewer_x;
   y0 -= params->viewer_y;
   x1 -= params->viewer_x;
   y1 -= params->viewer_y;

   if (center_a * x0 + center_b * y0 < 0
      && center_a * x0 + center_b * y1 < 0
      && center_a * x1 + center_b * y0 < 0
      && center_a * x1 + center_b * y1 < 0)
   {
      return True;                /* all behind viewer */
   }

   long l0, l1, l2, l3, r0, r1, r2, r3;

   l0 = left_a * x0 + left_b * y0;
   l1 = left_a * x0 + left_b * y1;
   l2 = left_a * x1 + left_b * y1;
   l3 = left_a * x1 + left_b * y0;

   if (l0 < 0 && l1 < 0 && l2 < 0 && l3 < 0)
   {
      return True;                /* all in quadrants 1&3 */
   }

   r0 = right_a * x0 + right_b * y0;
   r1 = right_a * x0 + right_b * y1;
   r2 = right_a * x1 + right_b * y1;
   r3 = right_a * x1 + right_b * y0;

   if (r0 < 0 && r1 < 0 && r2 < 0 && r3 < 0)
   {
      return True;                /* all in quadrants 2&3 */
   }

   return False;
}
