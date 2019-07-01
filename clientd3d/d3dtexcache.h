// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
#ifndef __D3DTEXCACHE_H__
#define __D3DTEXCACHE_H__

void D3DTexCacheInit();
void D3DTexCacheShutdown();

LPDIRECT3DTEXTURE9 D3DTexCacheGetTexture(d3d_render_packet_new *pPacket, int effect);

void D3DRenderPaletteSet(UINT xlatID0, UINT xlatID1, BYTE flags);
void D3DRenderPaletteSetNew(UINT xlatID0, UINT xlatID1, BYTE flags);
LPDIRECT3DTEXTURE9 D3DRenderTextureCreateFromBGF(PDIB pDib, BYTE xLat0, BYTE xLat1, BYTE effect);
LPDIRECT3DTEXTURE9 D3DRenderTextureCreateFromResource(BYTE *ptr, int width, int height);
LPDIRECT3DTEXTURE9 D3DRenderTextureCreateFromPNG(char *pFilename);

#endif
