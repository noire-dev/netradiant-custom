/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mip.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef unsigned char byte;

#include "ifilesystem.h"

#include "imagelib.h"
#include "bytestreamutils.h"

/*
   ============================================================================

   MIP IMAGE

   Quake WAD files contain miptex files that look like this:

    Mip section
        First mip
            Mip header
            First mip (width * height)
            Second mip (width * height / 4)
            Third mip (width * height / 16)
            Fourth mip (width * height / 64)

   ============================================================================
 */

#define GET_MIP_DATA_SIZE( WIDTH, HEIGHT ) ( sizeof( WAD3_MIP ) + ( WIDTH * HEIGHT ) + ( WIDTH * HEIGHT / 4 ) + ( WIDTH * HEIGHT / 16 ) + ( WIDTH * HEIGHT / 64 ) )

const int MIP_NAME_LENGTH = 16;
const int MIP_MIPMAP_COUNT = 4;
typedef struct
{
	char name[MIP_NAME_LENGTH];
	unsigned int width, height;
	unsigned int offsets[MIP_MIPMAP_COUNT];             // four mip maps stored
} WAD3_MIP, *LPWAD3_MIP;

static const byte quakepalette[768] =
{
	0x00,0x00,0x00, 0x0f,0x0f,0x0f, 0x1f,0x1f,0x1f, 0x2f,0x2f,0x2f,
	0x3f,0x3f,0x3f, 0x4b,0x4b,0x4b, 0x5b,0x5b,0x5b, 0x6b,0x6b,0x6b,
	0x7b,0x7b,0x7b, 0x8b,0x8b,0x8b, 0x9b,0x9b,0x9b, 0xab,0xab,0xab,
	0xbb,0xbb,0xbb, 0xcb,0xcb,0xcb, 0xdb,0xdb,0xdb, 0xeb,0xeb,0xeb,
	0x0f,0x0b,0x07, 0x17,0x0f,0x0b, 0x1f,0x17,0x0b, 0x27,0x1b,0x0f,
	0x2f,0x23,0x13, 0x37,0x2b,0x17, 0x3f,0x2f,0x17, 0x4b,0x37,0x1b,
	0x53,0x3b,0x1b, 0x5b,0x43,0x1f, 0x63,0x4b,0x1f, 0x6b,0x53,0x1f,
	0x73,0x57,0x1f, 0x7b,0x5f,0x23, 0x83,0x67,0x23, 0x8f,0x6f,0x23,
	0x0b,0x0b,0x0f, 0x13,0x13,0x1b, 0x1b,0x1b,0x27, 0x27,0x27,0x33,
	0x2f,0x2f,0x3f, 0x37,0x37,0x4b, 0x3f,0x3f,0x57, 0x47,0x47,0x67,
	0x4f,0x4f,0x73, 0x5b,0x5b,0x7f, 0x63,0x63,0x8b, 0x6b,0x6b,0x97,
	0x73,0x73,0xa3, 0x7b,0x7b,0xaf, 0x83,0x83,0xbb, 0x8b,0x8b,0xcb,
	0x00,0x00,0x00, 0x07,0x07,0x00, 0x0b,0x0b,0x00, 0x13,0x13,0x00,
	0x1b,0x1b,0x00, 0x23,0x23,0x00, 0x2b,0x2b,0x07, 0x2f,0x2f,0x07,
	0x37,0x37,0x07, 0x3f,0x3f,0x07, 0x47,0x47,0x07, 0x4b,0x4b,0x0b,
	0x53,0x53,0x0b, 0x5b,0x5b,0x0b, 0x63,0x63,0x0b, 0x6b,0x6b,0x0f,
	0x07,0x00,0x00, 0x0f,0x00,0x00, 0x17,0x00,0x00, 0x1f,0x00,0x00,
	0x27,0x00,0x00, 0x2f,0x00,0x00, 0x37,0x00,0x00, 0x3f,0x00,0x00,
	0x47,0x00,0x00, 0x4f,0x00,0x00, 0x57,0x00,0x00, 0x5f,0x00,0x00,
	0x67,0x00,0x00, 0x6f,0x00,0x00, 0x77,0x00,0x00, 0x7f,0x00,0x00,
	0x13,0x13,0x00, 0x1b,0x1b,0x00, 0x23,0x23,0x00, 0x2f,0x2b,0x00,
	0x37,0x2f,0x00, 0x43,0x37,0x00, 0x4b,0x3b,0x07, 0x57,0x43,0x07,
	0x5f,0x47,0x07, 0x6b,0x4b,0x0b, 0x77,0x53,0x0f, 0x83,0x57,0x13,
	0x8b,0x5b,0x13, 0x97,0x5f,0x1b, 0xa3,0x63,0x1f, 0xaf,0x67,0x23,
	0x23,0x13,0x07, 0x2f,0x17,0x0b, 0x3b,0x1f,0x0f, 0x4b,0x23,0x13,
	0x57,0x2b,0x17, 0x63,0x2f,0x1f, 0x73,0x37,0x23, 0x7f,0x3b,0x2b,
	0x8f,0x43,0x33, 0x9f,0x4f,0x33, 0xaf,0x63,0x2f, 0xbf,0x77,0x2f,
	0xcf,0x8f,0x2b, 0xdf,0xab,0x27, 0xef,0xcb,0x1f, 0xff,0xf3,0x1b,
	0x0b,0x07,0x00, 0x1b,0x13,0x00, 0x2b,0x23,0x0f, 0x37,0x2b,0x13,
	0x47,0x33,0x1b, 0x53,0x37,0x23, 0x63,0x3f,0x2b, 0x6f,0x47,0x33,
	0x7f,0x53,0x3f, 0x8b,0x5f,0x47, 0x9b,0x6b,0x53, 0xa7,0x7b,0x5f,
	0xb7,0x87,0x6b, 0xc3,0x93,0x7b, 0xd3,0xa3,0x8b, 0xe3,0xb3,0x97,
	0xab,0x8b,0xa3, 0x9f,0x7f,0x97, 0x93,0x73,0x87, 0x8b,0x67,0x7b,
	0x7f,0x5b,0x6f, 0x77,0x53,0x63, 0x6b,0x4b,0x57, 0x5f,0x3f,0x4b,
	0x57,0x37,0x43, 0x4b,0x2f,0x37, 0x43,0x27,0x2f, 0x37,0x1f,0x23,
	0x2b,0x17,0x1b, 0x23,0x13,0x13, 0x17,0x0b,0x0b, 0x0f,0x07,0x07,
	0xbb,0x73,0x9f, 0xaf,0x6b,0x8f, 0xa3,0x5f,0x83, 0x97,0x57,0x77,
	0x8b,0x4f,0x6b, 0x7f,0x4b,0x5f, 0x73,0x43,0x53, 0x6b,0x3b,0x4b,
	0x5f,0x33,0x3f, 0x53,0x2b,0x37, 0x47,0x23,0x2b, 0x3b,0x1f,0x23,
	0x2f,0x17,0x1b, 0x23,0x13,0x13, 0x17,0x0b,0x0b, 0x0f,0x07,0x07,
	0xdb,0xc3,0xbb, 0xcb,0xb3,0xa7, 0xbf,0xa3,0x9b, 0xaf,0x97,0x8b,
	0xa3,0x87,0x7b, 0x97,0x7b,0x6f, 0x87,0x6f,0x5f, 0x7b,0x63,0x53,
	0x6b,0x57,0x47, 0x5f,0x4b,0x3b, 0x53,0x3f,0x33, 0x43,0x33,0x27,
	0x37,0x2b,0x1f, 0x27,0x1f,0x17, 0x1b,0x13,0x0f, 0x0f,0x0b,0x07,
	0x6f,0x83,0x7b, 0x67,0x7b,0x6f, 0x5f,0x73,0x67, 0x57,0x6b,0x5f,
	0x4f,0x63,0x57, 0x47,0x5b,0x4f, 0x3f,0x53,0x47, 0x37,0x4b,0x3f,
	0x2f,0x43,0x37, 0x2b,0x3b,0x2f, 0x23,0x33,0x27, 0x1f,0x2b,0x1f,
	0x17,0x23,0x17, 0x0f,0x1b,0x13, 0x0b,0x13,0x0b, 0x07,0x0b,0x07,
	0xff,0xf3,0x1b, 0xef,0xdf,0x17, 0xdb,0xcb,0x13, 0xcb,0xb7,0x0f,
	0xbb,0xa7,0x0f, 0xab,0x97,0x0b, 0x9b,0x83,0x07, 0x8b,0x73,0x07,
	0x7b,0x63,0x07, 0x6b,0x53,0x00, 0x5b,0x47,0x00, 0x4b,0x37,0x00,
	0x3b,0x2b,0x00, 0x2b,0x1f,0x00, 0x1b,0x0f,0x00, 0x0b,0x07,0x00,
	0x00,0x00,0xff, 0x0b,0x0b,0xef, 0x13,0x13,0xdf, 0x1b,0x1b,0xcf,
	0x23,0x23,0xbf, 0x2b,0x2b,0xaf, 0x2f,0x2f,0x9f, 0x2f,0x2f,0x8f,
	0x2f,0x2f,0x7f, 0x2f,0x2f,0x6f, 0x2f,0x2f,0x5f, 0x2b,0x2b,0x4f,
	0x23,0x23,0x3f, 0x1b,0x1b,0x2f, 0x13,0x13,0x1f, 0x0b,0x0b,0x0f,
	0x2b,0x00,0x00, 0x3b,0x00,0x00, 0x4b,0x07,0x00, 0x5f,0x07,0x00,
	0x6f,0x0f,0x00, 0x7f,0x17,0x07, 0x93,0x1f,0x07, 0xa3,0x27,0x0b,
	0xb7,0x33,0x0f, 0xc3,0x4b,0x1b, 0xcf,0x63,0x2b, 0xdb,0x7f,0x3b,
	0xe3,0x97,0x4f, 0xe7,0xab,0x5f, 0xef,0xbf,0x77, 0xf7,0xd3,0x8b,
	0xa7,0x7b,0x3b, 0xb7,0x9b,0x37, 0xc7,0xc3,0x37, 0xe7,0xe3,0x57,
	0x7f,0xbf,0xff, 0xab,0xe7,0xff, 0xd7,0xff,0xff, 0x67,0x00,0x00,
	0x8b,0x00,0x00, 0xb3,0x00,0x00, 0xd7,0x00,0x00, 0xff,0x00,0x00,
	0xff,0xf3,0x93, 0xff,0xf7,0xc7, 0xff,0xff,0xff, 0x9f,0x5b,0x53
};

/*
   =============
   LoadMIP
   =============
 */

Image* LoadMIPBuff( byte* buffer ){
	byte *buf_p;
	int palettelength;
	int columns, rows, numPixels;
	byte *pixbuf;
	int i;
	byte *loadedpalette;
	const byte *palette;

	loadedpalette = 0;

	PointerInputStream inputStream( buffer );

	inputStream.seek( MIP_NAME_LENGTH );
	columns = istream_read_int32_le( inputStream );
	rows = istream_read_int32_le( inputStream );
	int offset = istream_read_int32_le( inputStream );

	if ( std::size_t( columns ) > 65536 && std::size_t( rows ) > 65536 ) {
		return 0;
	}

	//unsigned long mipdatasize = GET_MIP_DATA_SIZE( columns, rows );

	palettelength = vfsLoadFile( "gfx/palette.lmp", (void **) &loadedpalette );
	if ( palettelength == 768 ) {
		palette = loadedpalette;
	}
	else
	{
		loadedpalette = 0;
		palette = quakepalette;
	}

	buf_p = buffer + offset;

	numPixels = columns * rows;

	RGBAImage* image = new RGBAImage( columns, rows );

	//Sys_Printf("lpMip->width = %i, lpMip->height = %i, lpMip->offsets[0] = %i, lpMip->offsets[1] = %i, lpMip->offsets[2] = %i, lpMip->offsets[3] = %i, numPixels = %i\n", lpMip->width, lpMip->height, lpMip->offsets[0], lpMip->offsets[1], lpMip->offsets[2], lpMip->offsets[3], numPixels);
	//for (i = 0; i < sizeof(*lpMip); i++)
	//  Sys_Printf("%02x", (int) ((unsigned char *)lpMip)[i]);

	pixbuf = image->getRGBAPixels();

	for ( i = 0; i < numPixels; i++ )
	{
		int palIndex = *buf_p++;
		*pixbuf++ = palette[palIndex * 3];
		*pixbuf++ = palette[palIndex * 3 + 1];
		*pixbuf++ = palette[palIndex * 3 + 2];
		*pixbuf++ = 0xff;
	}

	if ( loadedpalette != 0 ) {
		vfsFreeFile( loadedpalette );
	}

	return image;
}

Image* LoadMIP( ArchiveFile& file ){
	ScopedArchiveBuffer buffer( file );
	return LoadMIPBuff( buffer.buffer );
}
