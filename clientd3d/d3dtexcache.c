// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
//
// d3dtexcache.c: Responsible for directX texture cache, loading textures
//                from files (not temp textures) and setting current palette.
//                Textures aren't cycled out of cache, as even loading the
//                majority of the bgfs (i.e. more than any user is likely to
//                see while playing) only takes ~300mb. Switching to png textures
//                would likely require cycling out based on last-time-accessed
//                and/or ref count.

#include "client.h"

#define TEXTURE_TABLE_SIZE 6151

// Data structure for texture lookup table.
typedef struct tex_cache_entry
{
   LPDIRECT3DTEXTURE9 pTexture;
   unsigned int       pDibID;
   int                size;
   int                effects;
   BYTE               xLat0;
   BYTE               xLat1;
   char               frame;
   struct tex_cache_entry *next;
} tex_cache_entry;

extern d3d_driver_profile gD3DDriverProfile;
extern Color              base_palette[];
PALETTEENTRY              gPalette[256];
int                       numMipMaps = 5;

static tex_cache_entry **tex_cache;
static unsigned int tex_cache_size = TEXTURE_TABLE_SIZE;
static unsigned int num_table_entries = 0;

/////////////////////////////////////////////////////////////////
// Texture cache/table section
/////////////////////////////////////////////////////////////////

void D3DTexCacheInit()
{
   // Table of pointers of tex_cache_entry
   tex_cache = (tex_cache_entry **)calloc(tex_cache_size, sizeof(tex_cache_entry *));
   num_table_entries = 0;

   // Number of mipmaps/texture levels. Config is a boolean, pick 5 (on) or 1 (off).
   if (config.mipMaps)
      numMipMaps = 5;
   else
      numMipMaps = 1;

   D3DRenderPaletteSet(0, 0, 0);
}

void D3DTexCacheShutdown()
{
   tex_cache_entry *t, *temp;

   for (unsigned int i = 0; i < tex_cache_size; ++i)
   {
      t = tex_cache[i];
      while (t != NULL)
      {
         temp = t->next;

         if (t->pTexture)
         {
            IDirect3DDevice9_Release(t->pTexture);
            t->pTexture = NULL;
         }
         free(t);

         t = temp;
      }
      tex_cache[i] = NULL;
   }

   free(tex_cache);
}

// replaces D3DCacheTextureLookup
LPDIRECT3DTEXTURE9 D3DTexCacheGetTexture(d3d_render_packet_new *pPacket, int effect)
{
   LPDIRECT3DTEXTURE9 pTexture = NULL;
   bool new_tex = true;

   // Get by texture ID
   tex_cache_entry *tex = tex_cache[pPacket->pDib->uniqueID % tex_cache_size];
   while (tex != NULL)
   {
      new_tex = false;
      if ((pPacket->pDib->uniqueID == tex->pDibID) &&
         (pPacket->pDib->frame == tex->frame))
      {
         if ((pPacket->xLat0 == tex->xLat0) &&
            (pPacket->xLat1 == tex->xLat1) &&
            (effect == tex->effects))
         {
            return tex->pTexture;
         }
      }
      tex = tex->next;
   }

   // Not in table, create.
   pTexture = D3DRenderTextureCreateFromBGF(pPacket->pDib, pPacket->xLat0,
      pPacket->xLat1, effect);

   if (NULL == pTexture)
      return NULL;

   tex = (tex_cache_entry *)D3DRenderMalloc(sizeof(tex_cache_entry));

   D3DSURFACE_DESC surfDesc;
   IDirect3DTexture9_GetLevelDesc(pTexture, 0, &surfDesc);

   tex->effects = effect;
   tex->pDibID = pPacket->pDib->uniqueID;
   tex->frame = pPacket->pDib->frame;
   tex->pTexture = pTexture;
   tex->xLat0 = pPacket->xLat0;
   tex->xLat1 = pPacket->xLat1;
   tex->size = surfDesc.Width * surfDesc.Height;

   switch (surfDesc.Format)
   {
   case D3DFMT_A8R8G8B8:
      tex->size *= 4;
   case D3DFMT_A4R4G4B4:
      tex->size *= 2;
   }

   // Add to texture table.
   unsigned int hash_num = tex->pDibID % tex_cache_size;
   tex->next = (new_tex) ? NULL : tex_cache[hash_num];
   tex_cache[hash_num] = tex;
   ++num_table_entries;

   return tex->pTexture;
}

/////////////////////////////////////////////////////////////////
// Palette & texture section
/////////////////////////////////////////////////////////////////

void D3DRenderPaletteSet(UINT xlatID0, UINT xlatID1, BYTE flags)
{
   xlat   *pXLat0, *pXLat1, *pXLatGrey;
   Color   *pPalette;
   int      i;

   return;

   pXLat0 = FindStandardXlat(xlatID0);
   pXLat1 = FindStandardXlat(xlatID1);

   pPalette = base_palette;

   switch (flags)
   {
   case DRAWFX_DRAW_PLAIN:
   case DRAWFX_DOUBLETRANS:
   case DRAWFX_BLACK:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_TRANSLUCENT25:
   case DRAWFX_TRANSLUCENT50:
   case DRAWFX_TRANSLUCENT75:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLat0)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat0)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat0)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_INVISIBLE:
      break;

   case DRAWFX_TRANSLATE:
      break;

   case DRAWFX_DITHERINVIS:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLat0)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat0)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat0)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_DITHERGREY:
      pXLatGrey = FindStandardXlat(XLAT_FILTERWHITE90);
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLatGrey)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLatGrey)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLatGrey)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_DITHERTRANS:
      if ((0 == xlatID1) || (xlatID0 == xlatID1))
      {
         for (i = 0; i < 256; i++)
         {
            gPalette[i].peRed = pPalette[fastXLAT(i, pXLat0)].red;
            gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat0)].green;
            gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat0)].blue;

            if (i == 254)
               gPalette[i].peFlags = 0;
            else
               gPalette[i].peFlags = 255;
         }
      }
      else
      {
         for (i = 0; i < 256; i++)
         {
            gPalette[i].peRed = pPalette[fastXLAT(i, pXLat1)].red;
            gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat1)].green;
            gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat1)].blue;

            if (i == 254)
               gPalette[i].peFlags = 0;
            else
               gPalette[i].peFlags = 255;
         }
      }
      break;

   case DRAWFX_SECONDTRANS:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLat1)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat1)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat1)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   default:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;
   }

   IDirect3DDevice9_SetPaletteEntries(gpD3DDevice, 0, gPalette);

   IDirect3DDevice9_SetCurrentTexturePalette(gpD3DDevice, 0);
}

void D3DRenderPaletteSetNew(UINT xlatID0, UINT xlatID1, BYTE flags)
{
   xlat   *pXLat0, *pXLat1, *pXLatGrey;
   Color   *pPalette;
   int      i;
   BYTE   effect;

   pXLat0 = FindStandardXlat(xlatID0);
   pXLat1 = FindStandardXlat(xlatID1);

   pPalette = base_palette;
   effect = flags;

   switch (effect)
   {
   case DRAWFX_DRAW_PLAIN:
   case DRAWFX_DOUBLETRANS:
   case DRAWFX_BLACK:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_TRANSLUCENT25:
   case DRAWFX_TRANSLUCENT50:
   case DRAWFX_TRANSLUCENT75:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLat0)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat0)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat0)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_INVISIBLE:
      break;

   case DRAWFX_TRANSLATE:
      break;

   case DRAWFX_DITHERINVIS:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLat0)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat0)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat0)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_DITHERGREY:
      pXLatGrey = FindStandardXlat(XLAT_FILTERWHITE90);
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLatGrey)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLatGrey)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLatGrey)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   case DRAWFX_DITHERTRANS:
      if ((0 == xlatID1) || (xlatID0 == xlatID1))
      {
         for (i = 0; i < 256; i++)
         {
            gPalette[i].peRed = pPalette[fastXLAT(i, pXLat0)].red;
            gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat0)].green;
            gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat0)].blue;

            if (i == 254)
               gPalette[i].peFlags = 0;
            else
               gPalette[i].peFlags = 255;
         }
      }
      else
      {
         for (i = 0; i < 256; i++)
         {
            gPalette[i].peRed = pPalette[fastXLAT(i, pXLat1)].red;
            gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat1)].green;
            gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat1)].blue;

            if (i == 254)
               gPalette[i].peFlags = 0;
            else
               gPalette[i].peFlags = 255;
         }
      }
      break;

   case DRAWFX_SECONDTRANS:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(i, pXLat1)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(i, pXLat1)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(i, pXLat1)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;

   default:
      for (i = 0; i < 256; i++)
      {
         gPalette[i].peRed = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].red;
         gPalette[i].peGreen = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].green;
         gPalette[i].peBlue = pPalette[fastXLAT(fastXLAT(i, pXLat0), pXLat1)].blue;

         if (i == 254)
            gPalette[i].peFlags = 0;
         else
            gPalette[i].peFlags = 255;
      }
      break;
   }
}

// Texture loader for BGF files. No longer need to load rotated textures separately,
// uv coords are flipped for floor/wall/ceilings.
LPDIRECT3DTEXTURE9 D3DRenderTextureCreateFromBGF(PDIB pDib, BYTE xLat0, BYTE xLat1, BYTE effect)
{
   D3DLOCKED_RECT      lockedRect;
   LPDIRECT3DTEXTURE9   pTexture = NULL;
   LPDIRECT3DTEXTURE9   pTextureFinal = NULL;
   unsigned char      *pBits = NULL;
   unsigned int      w, h;
   unsigned short      *pPixels16;
   int               si, sj, di, dj;
   int               k, l, newWidth, newHeight, diffWidth, diffHeight;
   int               skipValW, skipValH, pitchHalf;
   Color            lastColor;

   lastColor.red = 128;
   lastColor.green = 128;
   lastColor.blue = 128;

   D3DRenderPaletteSetNew(xLat0, xLat1, effect);
   skipValW = skipValH = 1;

   // convert to power of 2 texture, rounding down
   w = h = 0x80000000;

   while (!(w & pDib->width))
      w = w >> 1;

   while (!(h & pDib->height))
      h = h >> 1;

   // if either dimension is less than 256 pixels, round it back up
   if (pDib->width < D3DRENDER_TEXTURE_THRESHOLD)
   {
      if (w != pDib->width)
         w <<= 1;

      newWidth = w;
      diffWidth = newWidth - pDib->width;
      skipValW = -1;
   }
   else
   {
      newWidth = w;
      diffWidth = pDib->width - newWidth;
      skipValW = 1;
   }

   if (pDib->height < D3DRENDER_TEXTURE_THRESHOLD)
   {
      if (h != pDib->height)
         h <<= 1;

      newHeight = h;
      diffHeight = newHeight - pDib->height;
      skipValH = -1;
   }
   else
   {
      newHeight = h;
      diffHeight = pDib->height - newHeight;
      skipValH = 1;
   }

   pBits = DibPtr(pDib);

   int reqMipMaps = numMipMaps;
   if (reqMipMaps > 1 && (newWidth < 16 || newHeight < 16))
      reqMipMaps = 1;

   if (gD3DDriverProfile.bManagedTextures)
      IDirect3DDevice9_CreateTexture(gpD3DDevice, newWidth, newHeight, reqMipMaps, 0,
         D3DFMT_A1R5G5B5, D3DPOOL_MANAGED, &pTexture, NULL);
   else
      IDirect3DDevice9_CreateTexture(gpD3DDevice, newWidth, newHeight, reqMipMaps, 0,
         D3DFMT_A1R5G5B5, D3DPOOL_SYSTEMMEM, &pTexture, NULL);

   if (pTexture == NULL)
      return NULL;

   // If we are using mipmaps, need to modify each texture level.
   int pNewHeight = newHeight;
   int pNewWidth = newWidth;
   int levelAdd = 1;
   for (DWORD iLevel = 0; iLevel < pTexture->GetLevelCount(); ++iLevel, levelAdd *= 2)
   {
      k = -pNewWidth;
      l = -pNewHeight;

      IDirect3DTexture9_LockRect(pTexture, iLevel, &lockedRect, NULL, 0);

      pitchHalf = lockedRect.Pitch / 2;
      pPixels16 = (unsigned short *)lockedRect.pBits;

      for (si = 0, di = 0; di < newHeight; si += levelAdd, di++)
      {
         if (diffHeight)
            if ((l += diffHeight) >= 0)
            {
               si += skipValH;
               l -= pNewHeight;
            }

         for (dj = 0, sj = 0; dj < newWidth; dj++, sj += levelAdd)
         {
            if (diffWidth)
               if ((k += diffWidth) >= 0)
               {
                  sj += skipValW;
                  k -= pNewWidth;
               }

            // 16bit 1555 textures
            if (gPalette[pBits[si * pDib->width + sj]].peFlags != 0)
            {
               pPixels16[di * pitchHalf + dj] =
                  (gPalette[pBits[si * pDib->width + sj]].peBlue >> 3) |
                  ((gPalette[pBits[si * pDib->width + sj]].peGreen >> 3) << 5) |
                  ((gPalette[pBits[si * pDib->width + sj]].peRed >> 3) << 10);
               pPixels16[di * pitchHalf + dj] |=
                  gPalette[pBits[si * pDib->width + sj]].peFlags ? (1 << 15) : 0;

               lastColor.red = gPalette[pBits[si * pDib->width + sj]].peRed;
               lastColor.green = gPalette[pBits[si * pDib->width + sj]].peGreen;
               lastColor.blue = gPalette[pBits[si * pDib->width + sj]].peBlue;
            }
            else
            {
               pPixels16[di * pitchHalf + dj] =
                  (lastColor.blue >> 3) |
                  ((lastColor.green >> 3) << 5) |
                  ((lastColor.red >> 3) << 10);
               pPixels16[di * pitchHalf + dj] |=
                  gPalette[pBits[si * pDib->width + sj]].peFlags ? (1 << 15) : 0;
            }
         }
      }

      IDirect3DTexture9_UnlockRect(pTexture, iLevel);
      newWidth /= 2;
      newHeight /= 2;
      skipValW *= 2;
      skipValH *= 2;
   }

   if (gD3DDriverProfile.bManagedTextures == FALSE)
   {
      IDirect3DDevice9_CreateTexture(gpD3DDevice, pNewWidth, pNewHeight, reqMipMaps, 0,
         D3DFMT_A1R5G5B5, D3DPOOL_DEFAULT, &pTextureFinal, NULL);

      if (pTextureFinal)
      {
         IDirect3DTexture9_AddDirtyRect(pTextureFinal, NULL);
         IDirect3DDevice9_UpdateTexture(
            gpD3DDevice, (IDirect3DBaseTexture9 *)pTexture,
            (IDirect3DBaseTexture9 *)pTextureFinal);
      }
      IDirect3DDevice9_Release(pTexture);

      return pTextureFinal;
   }
   else
      return pTexture;
}

// Used for view elements.
LPDIRECT3DTEXTURE9 D3DRenderTextureCreateFromResource(BYTE *ptr, int width, int height)
{
   D3DLOCKED_RECT      lockedRect;
   LPDIRECT3DTEXTURE9   pTexture = NULL;
   LPDIRECT3DTEXTURE9   pTextureFinal = NULL;
   unsigned char      *pBits = NULL;
   unsigned int      w, h;
   unsigned short      *pPixels16;
   int               si, sj, di, dj;
   int               k, l, newWidth, newHeight, diffWidth, diffHeight;
   int               skipValW, skipValH, pitchHalf;

   D3DRenderPaletteSetNew(0, 0, 0);

   skipValW = skipValH = 1;

   // convert to power of 2 texture, rounding down
   w = h = 0x80000000;

   while (!(w & width))
      w = w >> 1;

   while (!(h & height))
      h = h >> 1;

   // if either dimension is less than 256 pixels, round it back up
   if (width < D3DRENDER_TEXTURE_THRESHOLD)
   {
      if (w != width)
         w <<= 1;

      newWidth = w;
      diffWidth = newWidth - width;
      skipValW = -1;
   }
   else
   {
      newWidth = w;
      diffWidth = width - newWidth;
      skipValW = 1;
   }

   if (height < D3DRENDER_TEXTURE_THRESHOLD)
   {
      if (h != height)
         h <<= 1;

      newHeight = h;
      diffHeight = newHeight - height;
      skipValH = -1;
   }
   else
   {
      newHeight = h;
      diffHeight = height - newHeight;
      skipValH = 1;
   }

   k = -newWidth;
   l = -newHeight;

   pBits = ptr;

   if (gD3DDriverProfile.bManagedTextures)
      IDirect3DDevice9_CreateTexture(gpD3DDevice, newHeight, newWidth, 1, 0,
         D3DFMT_A1R5G5B5, D3DPOOL_MANAGED, &pTexture, NULL);
   else
      IDirect3DDevice9_CreateTexture(gpD3DDevice, newHeight, newWidth, 1, 0,
         D3DFMT_A1R5G5B5, D3DPOOL_SYSTEMMEM, &pTexture, NULL);

   if (NULL == pTexture)
      return NULL;

   IDirect3DTexture9_LockRect(pTexture, 0, &lockedRect, NULL, 0);

   pitchHalf = lockedRect.Pitch / 2;

   pPixels16 = (unsigned short *)lockedRect.pBits;

   //   for (dj = 0, sj = 0; dj < newHeight; dj++, sj++)
   for (dj = newHeight - 1, sj = 0; dj >= 0; dj--, sj++)
   {
      if (diffHeight)
         if ((l += diffHeight) >= 0)
         {
            sj += skipValH;
            l -= newHeight;
         }

      for (si = 0, di = 0; di < newWidth; si++, di++)
      {
         if (diffWidth)
            if ((k += diffWidth) >= 0)
            {
               si += skipValW;
               k -= newWidth;
            }

         // 16bit 1555 textures
         pPixels16[dj * pitchHalf + di] =
            (gPalette[pBits[(sj * width) + si]].peBlue >> 3) |
            ((gPalette[pBits[(sj * width) + si]].peGreen >> 3) << 5) |
            ((gPalette[pBits[(sj * width) + si]].peRed >> 3) << 10);
         pPixels16[dj * pitchHalf + di] |=
            gPalette[pBits[(sj * width) + si]].peFlags ? (1 << 15) : 0;
      }
   }

   IDirect3DTexture9_UnlockRect(pTexture, 0);

   if (gD3DDriverProfile.bManagedTextures == FALSE)
   {
      IDirect3DDevice9_CreateTexture(gpD3DDevice, newHeight, newWidth, 1, 0,
         D3DFMT_A1R5G5B5, D3DPOOL_DEFAULT, &pTextureFinal, NULL);

      if (pTextureFinal)
      {
         IDirect3DTexture9_AddDirtyRect(pTextureFinal, NULL);
         IDirect3DDevice9_UpdateTexture(
            gpD3DDevice, (IDirect3DBaseTexture9 *)pTexture,
            (IDirect3DBaseTexture9 *)pTextureFinal);
      }
      IDirect3DDevice9_Release(pTexture);

      return pTextureFinal;
   }
   else
      return pTexture;
}

// Currently unused.
LPDIRECT3DTEXTURE9 D3DRenderTextureCreateFromPNG(char *pFilename)
{
   FILE   *pFile;
   png_structp   pPng = NULL;
   png_infop   pInfo = NULL;
   png_infop   pInfoEnd = NULL;
   png_bytepp   rows;

   D3DLOCKED_RECT      lockedRect;
   LPDIRECT3DTEXTURE9   pTexture = NULL;
   unsigned char      *pBits = NULL;
   unsigned int      w, h, b;
   int               pitchHalf, bytePP, stride;

   pFile = fopen(pFilename, "rb");
   if (pFile == NULL)
      return NULL;

   pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (NULL == pPng)
   {
      fclose(pFile);
      return NULL;
   }

   pInfo = png_create_info_struct(pPng);
   if (NULL == pInfo)
   {
      png_destroy_read_struct(&pPng, NULL, NULL);
      fclose(pFile);
      return NULL;
   }

   pInfoEnd = png_create_info_struct(pPng);
   if (NULL == pInfoEnd)
   {
      png_destroy_read_struct(&pPng, &pInfo, NULL);
      fclose(pFile);
      return NULL;
   }

   if (setjmp(png_jmpbuf(pPng)))
   {
      png_destroy_read_struct(&pPng, &pInfo, &pInfoEnd);
      fclose(pFile);
      return NULL;
   }

   png_init_io(pPng, pFile);
   png_read_png(pPng, pInfo, PNG_TRANSFORM_IDENTITY, NULL);
   rows = png_get_rows(pPng, pInfo);

   bytePP = pPng->pixel_depth / 8;
   stride = pPng->width * bytePP - bytePP;

   {
      int   i;
      png_bytep   curRow;

      for (i = 0; i < 6; i++)
      {
         IDirect3DDevice9_CreateTexture(gpD3DDevice, pPng->width, pPng->height, 1, 0,
            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture, NULL);

         IDirect3DTexture9_LockRect(pTexture, 0, &lockedRect, NULL, 0);

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
                     pBits[h * lockedRect.Pitch + w * 4 + (3 - b)] =
                     curRow[(w * bytePP) + b];
                  //                     pBits[h * lockedRect.Pitch + w * 4 + b] =
                  //                        curRow[(stride) - (w * bytePP) + b];
               }
            }
         }

         IDirect3DTexture9_UnlockRect(pTexture, 0);
      }
   }

   png_destroy_read_struct(&pPng, &pInfo, &pInfoEnd);
   fclose(pFile);

   return pTexture;
}
