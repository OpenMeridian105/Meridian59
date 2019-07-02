// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
#include "client.h"

extern font_3d gFont;
extern Bool gWireframe;
extern room_type current_room;
extern d3d_driver_profile gD3DDriverProfile;
extern Draw3DParams *p;
extern LPDIRECT3DTEXTURE9 gpDLightWhite;
extern LPDIRECT3DTEXTURE9 gpBackBufferTex[16];

float D3DRenderFloorCeilFogEndCalc(d3d_render_chunk_new *pChunk)
{
   // these numbers appear to be pulled out of thin air, but they aren't.  see
   // GetLightPalette(), LightChanged3D(), and LIGHT_INDEX() for more info
   // note: sectors with the no ambient flag attenuate twice as fast in the old client.
   // bug or not, it needs to be emulated here...
   if (pChunk->flags & D3DRENDER_NOAMBIENT)
      return (16384 + (pChunk->pSector->light * FINENESS) + (p->viewer_light << 6));
   else
      return (32768 + (max(0, pChunk->pSector->light - LIGHT_NEUTRAL) * FINENESS)
         + (p->viewer_light << 6) + (current_room.ambient_light << LOG_FINENESS));
}

float D3DRenderWallFogEndCalc(d3d_render_chunk_new *pChunk)
{
   float end, light;

   end = LIGHT_NEUTRAL;

   if (pChunk->side > 0)
   {
      if (pChunk->pSectorPos)
         light = pChunk->pSectorPos->light;
      else
         light = LIGHT_NEUTRAL;
   }
   else
   {
      if (pChunk->pSectorNeg)
         light = pChunk->pSectorNeg->light;
      else
         light = LIGHT_NEUTRAL;
   }

   // these numbers appear to be pulled out of thin air, but they aren't.  see
   // GetLightPalette(), LightChanged3D(), and LIGHT_INDEX() for more info
   // note: sectors with the no ambient flag attenuate twice as fast in the old client.
   // bug or not, it needs to be emulated here...
   if (pChunk->flags & D3DRENDER_NOAMBIENT)
      end = (16384 + (light * FINENESS) + (p->viewer_light << 6));
   else
      end = (32768 + (max(0, light - LIGHT_NEUTRAL) * FINENESS) + (p->viewer_light << 6) +
      (current_room.ambient_light << LOG_FINENESS));

   return end;
}

float D3DRenderFogEndCalc(d3d_render_chunk_new *pChunk)
{
   float end, light;

   end = LIGHT_NEUTRAL;

   if (pChunk->pSector == NULL)
   {
      if (pChunk->side > 0)
      {
         if (pChunk->pSectorPos)
            light = pChunk->pSectorPos->light;
         else
            light = LIGHT_NEUTRAL;
      }
      else
      {
         if (pChunk->pSectorNeg)
            light = pChunk->pSectorNeg->light;
         else
            light = LIGHT_NEUTRAL;
      }
   }
   else
      light = pChunk->pSector->light;

   // these numbers appear to be pulled out of thin air, but they aren't.  see
   // GetLightPalette(), LightChanged3D(), and LIGHT_INDEX() for more info
   // note: sectors with the no ambient flag attenuate twice as fast in the old client.
   // bug or not, it needs to be emulated here...
   if (pChunk->flags & D3DRENDER_NOAMBIENT)
      end = (16384 + (light * FINENESS) + (p->viewer_light << 6));
   else
      end = (32768 + (max(0, light - LIGHT_NEUTRAL) * FINENESS) + (p->viewer_light << 6) +
      (current_room.ambient_light << LOG_FINENESS));

   return end;
}

Bool D3DMaterialWorldPool(d3d_render_pool_new *pPool)
{
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

   return TRUE;
}

Bool D3DMaterialWorldPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   LPDIRECT3DTEXTURE9   pTexture = NULL;

  // The else clause is handy for testing; it draws the font texture everywhere
  if (1)
  {
    if (pPacket->pTexture)
      pTexture = pPacket->pTexture;
    else if (pPacket->pDib)
      pTexture = D3DTexCacheGetTexture(pPacket, 0);
  }
  else 
    pTexture = gFont.pTexture;

   if (pTexture)
      IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *) pTexture);

   return TRUE;
}

Bool D3DMaterialSkyboxChunk(d3d_render_chunk_new *pChunk)
{
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFogEndCalc(pChunk);

      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialFloorCeilDynamicChunk(d3d_render_chunk_new *pChunk)
{
   if (gWireframe)
   {
      if (pChunk->pSector == &current_room.sectors[0])
         return FALSE;
   }

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFloorCeilFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialFloorCeilStaticChunk(d3d_render_chunk_new *pChunk)
{
   if (gWireframe)
   {
      if (pChunk->pSector == &current_room.sectors[0])
      {
         if ((pChunk->pSector->ceiling == current_room.sectors[0].ceiling)
            && (pChunk->pSector->ceiling != NULL))
            return FALSE;
      }
   }

   if (pChunk->pSector->flags & SF_HAS_ANIMATED)
      return FALSE;

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFloorCeilFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialWallDynamicChunk(d3d_render_chunk_new *pChunk)
{
   if (pChunk->flags & D3DRENDER_NO_VTILE)
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   else
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialWallStaticChunk(d3d_render_chunk_new *pChunk)
{
   if ((pChunk->pSectorPos && (pChunk->pSectorPos->flags & SF_HAS_ANIMATED))
      || (pChunk->pSectorNeg && (pChunk->pSectorNeg->flags & SF_HAS_ANIMATED)))
      return FALSE;

   if (pChunk->pSideDef->flags & WF_HAS_ANIMATED)
      return FALSE;

   // Clamp texture vertically to remove stray pixels at the top.
   if (pChunk->flags & D3DRENDER_NO_VTILE)
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   else
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderWallFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialWallMaskPool(d3d_render_pool_new *pPool)
{
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

   return TRUE;
}

Bool D3DMaterialMaskChunk(d3d_render_chunk_new *pChunk)
{
   if (pChunk->flags & D3DRENDER_NOCULL)
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_NONE);
   else
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_CULLMODE, D3DCULL_CW);

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)pChunk->zBias * -0.00001f));

   return TRUE;
}

Bool D3DMaterialNone(d3d_render_chunk_new *pPool)
{
   return TRUE;
}

Bool D3DMaterialLMapDynamicPool(d3d_render_pool_new *pPool)
{
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
   
   IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *) gpDLightWhite);

   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_TEXTURE, 0);
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_MODULATE, D3DTA_CURRENT, D3DTA_TEXTURE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_SELECTARG2, D3DTA_CURRENT, D3DTA_TEXTURE);

   return TRUE;
}

Bool D3DMaterialLMapDynamicPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   LPDIRECT3DTEXTURE9   pTexture = NULL;

   if (pPacket->pTexture)
      pTexture = pPacket->pTexture;
   else if (pPacket->pDib)
      pTexture = D3DTexCacheGetTexture(pPacket, 0);

   if (pTexture)
      IDirect3DDevice9_SetTexture(gpD3DDevice, 1, (IDirect3DBaseTexture9 *) pTexture);

   return TRUE;
}

Bool D3DMaterialLMapFloorCeilDynamicChunk(d3d_render_chunk_new *pChunk)
{
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   return TRUE;
}

Bool D3DMaterialLMapFloorCeilStaticChunk(d3d_render_chunk_new *pChunk)
{
   if (pChunk->pSector->flags & SF_HAS_ANIMATED)
      return FALSE;

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   return TRUE;
}

Bool D3DMaterialLMapWallDynamicChunk(d3d_render_chunk_new *pChunk)
{
   if (pChunk->flags & D3DRENDER_NO_VTILE)
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   else
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   return TRUE;
}

Bool D3DMaterialLMapWallStaticChunk(d3d_render_chunk_new *pChunk)
{
   if (pChunk->flags & D3DRENDER_NO_VTILE)
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
   else
      IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);

   if (pChunk->pSectorPos)
      if (pChunk->pSectorPos->flags & SF_HAS_ANIMATED)
         return FALSE;

   if (pChunk->pSectorNeg)
      if (pChunk->pSectorNeg->flags & SF_HAS_ANIMATED)
         return FALSE;

   if (pChunk->pSideDef)
      if (pChunk->pSideDef->flags & WF_HAS_ANIMATED)
         return FALSE;

   return TRUE;
}

Bool D3DMaterialObjectPool(d3d_render_pool_new *pPool)
{
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

   return TRUE;
}

Bool D3DMaterialObjectPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   LPDIRECT3DTEXTURE9   pTexture = NULL;

   if (pPacket->pTexture)
      pTexture = pPacket->pTexture;
   else if (pPacket->pDib)
      pTexture = D3DTexCacheGetTexture(pPacket, pPacket->effect);

   if (pTexture)
      IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *) pTexture);

   return TRUE;
}

Bool D3DMaterialObjectChunk(d3d_render_chunk_new *pChunk)
{
   static BYTE   lastXLat0, lastXLat1;

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &pChunk->xForm);

   if ((pChunk->xLat0) || (pChunk->xLat1))
   {
      D3DRenderPaletteSet(pChunk->xLat0, pChunk->xLat1, pChunk->drawingtype);
      lastXLat0 = pChunk->xLat0;
      lastXLat1 = pChunk->xLat1;
   }
   else if ((lastXLat0) || (lastXLat1))
   {
      D3DRenderPaletteSet(0, 0, 0);
      lastXLat0 = lastXLat1 = 0;
   }

   // apply Z-BIAS here to make sure coplanar object-layers are rendered correctly
   // layer-ordering is saved in pChunk->zBias
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)pChunk->zBias * -0.00001f));

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAREF, pChunk->alphaRef);

   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialTargetedObjectChunk(d3d_render_chunk_new *pChunk)
{
   static BYTE   lastXLat0, lastXLat1;

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &pChunk->xForm);

   if ((pChunk->xLat0) || (pChunk->xLat1))
   {
      D3DRenderPaletteSet(pChunk->xLat0, pChunk->xLat1, pChunk->drawingtype);
      lastXLat0 = pChunk->xLat0;
      lastXLat1 = pChunk->xLat1;
   }
   else if ((lastXLat0) || (lastXLat1))
   {
      D3DRenderPaletteSet(0, 0, 0);
      lastXLat0 = lastXLat1 = 0;
   }

   // apply Z-BIAS here to make sure coplanar object-layers are rendered correctly
   // layer-ordering is saved in pChunk->zBias
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)pChunk->zBias * -0.00001f));

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAREF, pChunk->alphaRef);

   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_DIFFUSE, 0);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);

   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialObjectInvisiblePool(d3d_render_pool_new *pPool)
{
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR);

   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_SELECTARG2, 0, D3DTA_TEXTURE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_SELECTARG1, D3DTA_CURRENT, 0);

   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
   IDirect3DDevice9_SetSamplerState(gpD3DDevice, 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

   IDirect3DDevice9_SetTexture(gpD3DDevice, 1, (IDirect3DBaseTexture9 *) gpBackBufferTex[0]);

   return TRUE;
}

Bool D3DMaterialObjectInvisiblePacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   LPDIRECT3DTEXTURE9   pTexture = NULL;

   if (pPacket->pTexture)
      pTexture = pPacket->pTexture;
   else if (pPacket->pDib)
      pTexture = D3DTexCacheGetTexture(pPacket, pPacket->effect);

   if (pTexture)
      IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *) pTexture);

   return TRUE;
}

Bool D3DMaterialObjectInvisibleChunk(d3d_render_chunk_new *pChunk)
{
   static BYTE   lastXLat0, lastXLat1;

   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &pChunk->xForm);

   if ((pChunk->xLat0) || (pChunk->xLat1))
   {
      D3DRenderPaletteSet(pChunk->xLat0, pChunk->xLat1, pChunk->drawingtype);
      lastXLat0 = pChunk->xLat0;
      lastXLat1 = pChunk->xLat1;
   }
   else if ((lastXLat0) || (lastXLat1))
   {
      D3DRenderPaletteSet(0, 0, 0);
      lastXLat0 = lastXLat1 = 0;
   }

   // apply Z-BIAS here to make sure coplanar object-layers are rendered correctly
   // layer-ordering is saved in pChunk->zBias
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)pChunk->zBias * -0.00001f));

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHAREF, pChunk->alphaRef);

   return TRUE;
}

Bool D3DMaterialEffectPool(d3d_render_pool_new *pPool)
{
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHABLENDENABLE, FALSE);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_DIFFUSE, 0);
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, D3DTA_DIFFUSE, 0);

   return TRUE;
}

Bool D3DMaterialEffectPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   LPDIRECT3DTEXTURE9   pTexture = NULL;

   if (pPacket->pTexture)
      pTexture = pPacket->pTexture;

   if (pTexture)
      IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *) pTexture);

   return TRUE;
}

Bool D3DMaterialBlurPool(d3d_render_pool_new *pPool)
{
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG1, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

   return TRUE;
}

Bool D3DMaterialBlurPacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   LPDIRECT3DTEXTURE9   pTexture = NULL;

   if (pPacket->pTexture)
      pTexture = pPacket->pTexture;

   if (pTexture)
      IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *) pTexture);

   return TRUE;
}

Bool D3DMaterialBlurChunk(d3d_render_chunk_new *pChunk)
{
   return TRUE;
}

Bool D3DMaterialParticlePool(d3d_render_pool_new *pPool)
{
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHATESTENABLE, TRUE);
   D3DRENDER_SET_ALPHATEST_STATE(gpD3DDevice, TRUE, TEMP_ALPHA_REF, D3DCMP_GREATEREQUAL);
   D3DRENDER_SET_ALPHABLEND_STATE(gpD3DDevice, TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_ALPHABLENDENABLE, TRUE);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

   D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);
   D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 1, D3DTOP_DISABLE, 0, 0);

   return TRUE;
}

Bool D3DMaterialParticlePacket(d3d_render_packet_new *pPacket, d3d_render_cache_system *pCacheSystem)
{
   if (pPacket->pTexture)
   {
      IDirect3DDevice9_SetTexture(gpD3DDevice, 0, (IDirect3DBaseTexture9 *)pPacket->pTexture);
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
   }
   else
   {
      D3DRENDER_SET_COLOR_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG2, 0, D3DTA_DIFFUSE);
      D3DRENDER_SET_ALPHA_STAGE(gpD3DDevice, 0, D3DTOP_SELECTARG2, 0, D3DTA_DIFFUSE);
   }

   return TRUE;
}

Bool D3DMaterialParticleChunk(d3d_render_chunk_new *pChunk)
{
   IDirect3DDevice9_SetTransform(gpD3DDevice, D3DTS_WORLD, &pChunk->xForm);

   IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_DEPTHBIAS, F2DW((float)0 * -0.00001f));
   if (gD3DDriverProfile.bFogEnable)
   {
      float end = D3DRenderFogEndCalc(pChunk);
      IDirect3DDevice9_SetRenderState(gpD3DDevice, D3DRS_FOGEND, *(DWORD *)(&end));
   }

   return TRUE;
}

Bool D3DMaterialEffectChunk(d3d_render_chunk_new *pChunk)
{
   return TRUE;
}
