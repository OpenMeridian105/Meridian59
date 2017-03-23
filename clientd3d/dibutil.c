// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * dibutil.c:  Load a bgf file.
 */

#include "client.h"

static BYTE magic[] = {0x42, 0x47, 0x46, 0x11};

#define BGF_VERSION 10

static Bool DibOpenFileReal(char *szFile, Bitmaps *b);
static Bool DibReadBits(file_node *f, PDIB pdib, int version);
/************************************************************************/
/*
 * DibOpenFile: Load the bitmaps in the file given by filename into the
 *   given bitmap structure.  Return TRUE on success.
 */
Bool DibOpenFile(char *szFile, Bitmaps *b)
{
   return DibOpenFileReal(szFile, b);
}
/************************************************************************/
/*
 * DibHash: Create a hash of BGF filename using djb2 algorithm.
 */
unsigned int DibHash(char *str)
{
   unsigned int hash = 5381;
   int c;

   while (c = *str++)
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

   return hash;
}
/************************************************************************/
Bool DibOpenFileReal(char *szFile, Bitmaps *b)
{
   file_node f;
   DWORD   dwLen, dwBits;
   DWORD   width, height, xoffset, yoffset;
   BYTE    byte, num_hotspots, shrink;
   int     i, j, size, offset, num_indices, temp, version;
   char		*start;

   if (!CliMappedFileOpenRead(szFile, &f))
      return False;

   // Check magic number
   for (i = 0; i < 4; i++)
      if (CliMappedFileRead(&f, &byte, 1) != 1 || byte != magic[i])
         return MappedFileClose(&f);

   // Check version
   if (CliMappedFileRead(&f, &version, 4) != 4 || version < BGF_VERSION)
   {
      debug(("Bad BGF version %d\n", version));
      return MappedFileClose(&f);
   }

   // Skip bitmap name
   f.ptr += MAX_BITMAPNAME;

   // Read # of bitmaps
   if (CliMappedFileRead(&f, &b->num_bitmaps, 4) != 4)
      return MappedFileClose(&f);

   // Read # of bitmap groups
   if (CliMappedFileRead(&f, &b->num_groups, 4) != 4)
      return MappedFileClose(&f);

   // Read max # of indices in a group
   if (CliMappedFileRead(&f, &b->max_indices, 4) != 4)
      return MappedFileClose(&f);
   b->max_indices++;  // Leave room to store the # of indices actually present in a group

   // Read "shrink factor" of bitmaps
   if (CliMappedFileRead(&f, &temp, 4) != 4)
      return MappedFileClose(&f);
   shrink = (BYTE)temp;
   if (shrink == 0)
      shrink = 1;    // Avoid divide by zero errors elsewhere in client

   // Allocate memory for bitmap pointers and indices
   size = b->num_bitmaps * sizeof(PDIB) + b->num_groups * b->max_indices * sizeof(int);
   b->pdibs = (PDIB *)SafeMalloc(size);
   b->indices = (int *)((BYTE *)b->pdibs + b->num_bitmaps * sizeof(PDIB));
   memset(b->pdibs, 0, size);

   // Read in bitmaps
   b->size = 0;
   for (i = 0; i < b->num_bitmaps; i++)
   {
      if (CliMappedFileRead(&f, &width, 4) != 4)
         return MappedFileClose(&f);

      if (CliMappedFileRead(&f, &height, 4) != 4)
         return MappedFileClose(&f);

      if (CliMappedFileRead(&f, &xoffset, 4) != 4)
         return MappedFileClose(&f);

      if (CliMappedFileRead(&f, &yoffset, 4) != 4)
         return MappedFileClose(&f);

      if (CliMappedFileRead(&f, &num_hotspots, 1) != 1)
         return MappedFileClose(&f);

      /* How much memory do we need to hold the PDIB? */
      dwBits = width * height;
      dwLen = dwBits + sizeof(DIBHEADER) + num_hotspots * (sizeof(POINT) + sizeof(BYTE));

      b->size += dwLen;

      // Arrangement of PDIB in memory:
      // Header
      // bytes of bitmap
      // array of hotspot numbers
      // array of hotspot positions

      b->pdibs[i] = (PDIB)SafeMalloc(dwLen);

      b->pdibs[i]->width = width;
      b->pdibs[i]->height = height;
      b->pdibs[i]->xoffset = xoffset;
      b->pdibs[i]->yoffset = yoffset;
      b->pdibs[i]->num_hotspots = num_hotspots;
      b->pdibs[i]->numbers = (char *)(DibPtr(b->pdibs[i]) + dwBits);
      b->pdibs[i]->hotspots = (POINT *)(b->pdibs[i]->numbers + num_hotspots * sizeof(BYTE));
      b->pdibs[i]->shrink = shrink;

      // Create unique ID for this BGF using filename (including extension).
      start = strrchr(szFile, '\\');
      if (start)
         start++;
      else
         start = szFile;

      char string[FILENAME_MAX];
      strcpy(string, start);
      strupr(string);
      b->pdibs[i]->uniqueID = DibHash(string);

      b->pdibs[i]->frame = i;

      // read in the hotspots
      for (j = 0; j < num_hotspots; j++)
      {
         if ((CliMappedFileRead(&f, &b->pdibs[i]->numbers[j], 1) != 1) ||
            (CliMappedFileRead(&f, &b->pdibs[i]->hotspots[j].x, 4) != 4) ||
            (CliMappedFileRead(&f, &b->pdibs[i]->hotspots[j].y, 4) != 4))
         {
            BitmapsFree(b);
            return MappedFileClose(&f);
         }
      }

      /* read in the bits */
      if (!DibReadBits(&f, b->pdibs[i], version))
      {
         BitmapsFree(b);
         return MappedFileClose(&f);
      }
   }

   // Read in indices
   for (i = 0; i < b->num_groups; i++)
   {
      // offset stores where we are in indices array
      offset = b->max_indices * i;
      if (CliMappedFileRead(&f, &num_indices, 4) != 4)
      {
         BitmapsFree(b);
         return MappedFileClose(&f);
      }
      b->indices[offset] = num_indices;
      for (j = 0; j < num_indices; j++)
      {
         offset++;
         if (CliMappedFileRead(&f, &b->indices[offset], 4) != 4)
         {
            BitmapsFree(b);
            return MappedFileClose(&f);
         }
      }
   }

   MappedFileClose(&f);
   return True;
}
/************************************************************************/
/*
 * DibReadBits:  Read bits of an image into the given PDIB.  The file pointer
 *   for f must be positioned at the start of the image information.
 *   version is version of BGF file.
 *   Return True on success.
 */
Bool DibReadBits(file_node *f, PDIB pdib, int version)
{
   BYTE *bits, type;
   int length, temp, compressed_length, retval;
   uLongf len;

   bits = DibPtr(pdib);
   length = DibWidth(pdib) * DibHeight(pdib);

   // See if image is compressed
   if (CliMappedFileRead(f, &type, 1) != 1) return False;

   switch (type)
   {
   case 0:
      if (CliMappedFileRead(f, &temp, 4) != 4) return False;  // Skip unused bytes
      if (CliMappedFileRead(f, bits, length) != length) return False;
      break;
   case 1:
      if (CliMappedFileRead(f, &compressed_length, 4) != 4) return False;

      len = length;
      retval = uncompress((Bytef *) bits, &len, (const Bytef *) f->ptr, compressed_length);
      if (retval != Z_OK)
      {
         debug(("DibReadBits error during decompression: %d\n", retval));
         return False;
      }

      f->ptr += compressed_length;
      break;
   default:
      debug(("DibReadBits got bad type byte %d\n", (int) type));
      return False;
   }
   return True;
}
/************************************************************************/
/*
 * DibOpenBitmapFile:  Opens any .bmp file, puts the color palette into
 *   phPalette and bitmap into phBitmap.
 *   Return True on success.
 */
Bool DibOpenBitmapFile(char *szFile, HBITMAP *phBitmap, HPALETTE *phPalette)
{
   BITMAP bm;

   *phBitmap = NULL;
   *phPalette = NULL;

   // Use LoadImage() to get the image loaded into a DIBSection
   *phBitmap = (HBITMAP)LoadImage(NULL, szFile, IMAGE_BITMAP, 0, 0,
      LR_CREATEDIBSECTION | LR_LOADFROMFILE);
   if (*phBitmap == NULL)
   {
      return FALSE;
   }

   // Get the color depth of the DIBSection
   GetObject(*phBitmap, sizeof(BITMAP), &bm);
   // If the DIBSection is 256 color or less, it has a color table
   if ((bm.bmBitsPixel * bm.bmPlanes) <= 8)
   {
      HDC           hMemDC;
      HBITMAP       hOldBitmap;
      RGBQUAD       rgb[256];
      LPLOGPALETTE  pLogPal;
      WORD          i;

      // Create a memory DC and select the DIBSection into it
      hMemDC = CreateCompatibleDC(NULL);
      hOldBitmap = (HBITMAP)SelectObject(hMemDC, *phBitmap);
      // Get the DIBSection's color table
      GetDIBColorTable(hMemDC, 0, 256, rgb);
      // Create a palette from the color tabl
      pLogPal = (LOGPALETTE *)malloc(sizeof(LOGPALETTE) + (256 * sizeof(PALETTEENTRY)));
      if (!pLogPal)
         return FALSE;
      pLogPal->palVersion = 0x300;
      pLogPal->palNumEntries = 256;
      for (i = 0;i<256;i++)
      {
         pLogPal->palPalEntry[i].peRed = rgb[i].rgbRed;
         pLogPal->palPalEntry[i].peGreen = rgb[i].rgbGreen;
         pLogPal->palPalEntry[i].peBlue = rgb[i].rgbBlue;
         pLogPal->palPalEntry[i].peFlags = 0;
      }
      *phPalette = CreatePalette(pLogPal);
      // Clean up
      free(pLogPal);
      SelectObject(hMemDC, hOldBitmap);
      DeleteDC(hMemDC);
   }
   else   // It has no color table, so use a halftone palette
   {
      HDC hRefDC;

      hRefDC = GetDC(NULL);
      *phPalette = CreateHalftonePalette(hRefDC);
      ReleaseDC(NULL, hRefDC);
   }
   return TRUE;
}
/************************************************************************/
/*
 * DibOpenBitmapResource:  Opens any .bmp resource, puts the color palette
 *   into phPalette and bitmap into phBitmap.
 *   Return True on success.
 */
Bool DibOpenBitmapResource(HINSTANCE hInst, UINT rsc, HBITMAP *phBitmap, HPALETTE *phPalette)
{
   BITMAP bm;

   *phBitmap = NULL;
   *phPalette = NULL;

   // Use LoadImage() to get the image loaded into a DIBSection
   *phBitmap = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(rsc), IMAGE_BITMAP, 0, 0,
      LR_CREATEDIBSECTION);
   if (*phBitmap == NULL)
   {
      return FALSE;
   }

   // Get the color depth of the DIBSection
   GetObject(*phBitmap, sizeof(BITMAP), &bm);
   // If the DIBSection is 256 color or less, it has a color table
   if ((bm.bmBitsPixel * bm.bmPlanes) <= 8)
   {
      HDC           hMemDC;
      HBITMAP       hOldBitmap;
      RGBQUAD       rgb[256];
      LPLOGPALETTE  pLogPal;
      WORD          i;

      // Create a memory DC and select the DIBSection into it
      hMemDC = CreateCompatibleDC(NULL);
      hOldBitmap = (HBITMAP)SelectObject(hMemDC, *phBitmap);
      // Get the DIBSection's color table
      GetDIBColorTable(hMemDC, 0, 256, rgb);
      // Create a palette from the color tabl
      pLogPal = (LOGPALETTE *)malloc(sizeof(LOGPALETTE) + (256 * sizeof(PALETTEENTRY)));
      if (!pLogPal)
         return FALSE;
      pLogPal->palVersion = 0x300;
      pLogPal->palNumEntries = 256;
      for (i = 0;i<256;i++)
      {
         pLogPal->palPalEntry[i].peRed = rgb[i].rgbRed;
         pLogPal->palPalEntry[i].peGreen = rgb[i].rgbGreen;
         pLogPal->palPalEntry[i].peBlue = rgb[i].rgbBlue;
         pLogPal->palPalEntry[i].peFlags = 0;
      }
      *phPalette = CreatePalette(pLogPal);
      // Clean up
      free(pLogPal);
      SelectObject(hMemDC, hOldBitmap);
      DeleteDC(hMemDC);
   }
   else   // It has no color table, so use a halftone palette
   {
      HDC hRefDC;

      hRefDC = GetDC(NULL);
      *phPalette = CreateHalftonePalette(hRefDC);
      ReleaseDC(NULL, hRefDC);
   }
   return TRUE;
}
/************************************************************************/
/*
 * Free memory associated with PDIB.
 */
void DibFree(PDIB pdib)
{
   SafeFree(pdib);
}
/************************************************************************/
/*
 * Free memory associated with bitmap structure.
 */
void BitmapsFree(Bitmaps *b)
{
   int i;
   
   // First free individual bitmaps, if they have been loaded
   for (i=0; i < b->num_bitmaps; i++)
      if (b->pdibs[i] != NULL)
	 DibFree(b->pdibs[i]);
   SafeFree(b->pdibs);
}
