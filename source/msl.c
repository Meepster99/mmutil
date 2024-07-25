/****************************************************************************
 *                                                          __              *
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 *         Copyright (c) 2008, Mukunda Johnson (mukunda@maxmod.org)         *
 *                                                                          *
 * Permission to use, copy, modify, and/or distribute this software for any *
 * purpose with or without fee is hereby granted, provided that the above   *
 * copyright notice and this permission notice appear in all copies.        *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES *
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF         *
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR  *
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN    *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF  *
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.           *
 ****************************************************************************/

// MAXMOD SOUNDBANK

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "errors.h"
#include "defs.h"
#include "files.h"
#include "mas.h"
#include "mod.h"
#include "s3m.h"
#include "xm.h"
#include "it.h"
#include "wav.h"
#include "simple.h"
#include "version.h"
#include "systems.h"
#include "samplefix.h"

FILE*	F_SCRIPT=NULL;

FILE*	F_SAMP=NULL;
FILE*	F_SONG=NULL;

FILE*	F_HEADER=NULL;

u16		MSL_NSAMPS;
u16		MSL_NSONGS;

char	str_msl[256];

#define TMP_SAMP "sampJ328G54AU3.tmp"
#define TMP_SONG "songDJ34957FAI.tmp"

void MSL_PrintDefinition( char* filename, u16 id, char* prefix );

#define SAMPLE_HEADER_SIZE (12 + (( target_system == SYSTEM_NDS ) ? 4:0))

void MSL_Erase( void )
{
	MSL_NSAMPS = 0;
	MSL_NSONGS = 0;
	file_delete( TMP_SAMP );
	file_delete( TMP_SONG );
}

u16 MSL_AddSample( Sample* samp )
{
	u32 sample_length;
	u32 x;
	file_open_write_end( TMP_SAMP );

	sample_length = samp->sample_length;

	write32( ((samp->format & SAMPF_16BIT) ? sample_length*2 : sample_length ) + SAMPLE_HEADER_SIZE  +4); // +4 for sample padding
	write8 ( (target_system == SYSTEM_GBA) ? MAS_TYPE_SAMPLE_GBA : MAS_TYPE_SAMPLE_NDS );
	write8( MAS_VERSION );
	write8( samp->filename[0] == '#' ? 1 : 0);
	write8( BYTESMASHER );

	Write_SampleData(samp);

	file_close_write();
	MSL_NSAMPS++;
	return MSL_NSAMPS-1;
}

typedef struct tSampleInfo {
	u32 samp_len;
	u32 samp_llen;
	u8 sformat;
	unsigned hash;
} SampleInfo;

unsigned sampleInfoLen = 0;
unsigned sampleInfoMax = 0;
SampleInfo* sampleInfoList = NULL;

u16 MSL_AddSampleC( Sample* samp )
{

	if(sampleInfoList == NULL) {
		sampleInfoList = malloc(64 * sizeof(SampleInfo));
		sampleInfoMax = 64;
	}

	if(sampleInfoLen == sampleInfoMax) {
		sampleInfoMax *= 2;
		sampleInfoList = realloc(sampleInfoList, sampleInfoMax * sizeof(SampleInfo));
	}

	u32 st;
	u32 samp_len;
	u32 samp_llen;
	u8 sformat;
	u8 target_sformat;

	u32 h_filesize;
	int samp_id;
	bool samp_match;
		
	int fsize=file_size( TMP_SAMP );
	if( fsize == 0 )
	{
		return MSL_AddSample( samp );
	}
	
	// new plan
	// hash all previous samples, store in a set(more realistically array, linear search)

	SampleInfo tempSampleInfo;
	tempSampleInfo.samp_len = samp->sample_length;
	tempSampleInfo.samp_llen = samp->loop_type ? samp->loop_end-samp->loop_start : 0xFFFFFFFF;
	tempSampleInfo.sformat = target_system == SYSTEM_NDS ? sample_dsformat(samp) : SAMP_FORMAT_U8;
	
	unsigned tempHash = 0;
	if( samp->format & SAMPF_16BIT ) {
		for(st=0; st<samp->sample_length; st++) {
			tempHash = (tempHash * 31) + ((u16*)samp->data)[st];
		}
	} else {
		for(st=0; st<samp->sample_length; st++) {
			tempHash = (tempHash * 31) + ((u8*)samp->data)[st];
		}
	}
	tempSampleInfo.hash = tempHash;

	for(unsigned i=0; i<sampleInfoLen; i++) {
		if(tempSampleInfo.samp_len == sampleInfoList[i].samp_len &&
		tempSampleInfo.samp_llen == sampleInfoList[i].samp_llen &&
		tempSampleInfo.sformat == sampleInfoList[i].sformat &&
		tempSampleInfo.hash == sampleInfoList[i].hash) {
			return i;
		}
	}
	
	sampleInfoList[sampleInfoLen++] = tempSampleInfo;
	
	return MSL_AddSample( samp );
}

u16 MSL_AddModule( MAS_Module* mod )
{
	int x;
	int samp_id;
	// ADD SAMPLES
	for( x = 0; x < mod->samp_count; x++ )
	{
		samp_id = MSL_AddSampleC( &mod->samples[x] );
		if( mod->samples[x].filename[0] == '#' )
			MSL_PrintDefinition( mod->samples[x].filename+1, (u16)samp_id, "SFX_" );
		mod->samples[x].msl_index = samp_id;
	}
	
	file_open_write_end( TMP_SONG );
	Write_MAS( mod, false, true );
	file_close_write();
	MSL_NSONGS++;
	return MSL_NSONGS-1;
}

void MSL_Export( char* filename )
{
	u32 x;
	u32 y;
	u32 file_size;

	u32* parap_samp;
	u32* parap_song;

	file_open_write( filename );
	write16( MSL_NSAMPS );
	write16( MSL_NSONGS );
	write8( '*' );
	write8( 'm' );
	write8( 'a' );
	write8( 'x' );
	write8( 'm' );
	write8( 'o' );
	write8( 'd' );
	write8( '*' );
	
	parap_samp = (u32*)malloc( MSL_NSAMPS * sizeof( u32 ) );
	parap_song = (u32*)malloc( MSL_NSONGS * sizeof( u32 ) );
	
	// reserve space for parapointers
	for( x = 0; x < MSL_NSAMPS; x++ )
		write32( 0xAAAAAAAA );
	for( x = 0; x < MSL_NSONGS; x++ )
		write32( 0xAAAAAAAA );
	// copy samples
	file_open_read( TMP_SAMP );
	for( x = 0; x < MSL_NSAMPS; x++ )
	{
		align32();
		parap_samp[x] = file_tell_write();
		file_size = read32();
		write32( file_size );
		for( y = 0; y < file_size+4; y++ )
			write8( read8() );
	}
	file_close_read();
	
	file_open_read( TMP_SONG );
	for( x = 0; x < MSL_NSONGS; x++ )
	{
		align32();
		parap_song[x] = file_tell_write();
		file_size = read32();
		write32( file_size );
		for( y = 0; y < file_size+4; y++ )
			write8( read8() );
	}
	file_close_read();
	
	file_seek_write( 0x0C, SEEK_SET );
	for( x = 0; x < MSL_NSAMPS; x++ )
		write32( parap_samp[x] );
	for( x=  0; x < MSL_NSONGS; x++ )
		write32( parap_song[x] );

	file_close_write();

	if( parap_samp )
		free( parap_samp );
	if( parap_song )
		free( parap_song );
}

void MSL_PrintDefinition( char* filename, u16 id, char* prefix )
{
	char newtitle[64];
	int x,s=0;
	if( filename[0] == 0 )	// empty string
		return;
	for( x = 0; x < (int)strlen( filename ); x++ )
	{
		if( filename[x] == '\\' || filename[x] == '/' ) s = x+1; 
	}
	for( x = s; x < (int)strlen( filename ); x++ )
	{
		if( filename[x] != '.' )
		{
			newtitle[x-s] = toupper(filename[x]);
			if( newtitle[x-s] >= ' ' && newtitle[x-s] <= '/' )
				newtitle[x-s] = '_';
			if( newtitle[x-s] >= ':' && newtitle[x-s] <= '@' )
				newtitle[x-s] = '_';
			if( newtitle[x-s] >= '[' && newtitle[x-s] <= '`' )
				newtitle[x-s] = '_';
			if( newtitle[x-s] >= '{' )
				newtitle[x-s] = '_';
		}
		else
		{
			break;
		}
	}
	newtitle[x-s] = 0;
	if( F_HEADER )
	{
		fprintf( F_HEADER, "#define %s%s	%i\r\n", prefix, newtitle, id );
	}
}

void MSL_LoadFile( char* filename, bool verbose )
{
	Sample wav;
	MAS_Module mod;
	int f_ext;
	if( file_open_read( filename ) )
	{
		printf( "Cannot open %s for reading! Skipping.\n", filename );
		return;
	}
	f_ext = get_ext( filename );
	switch( f_ext )
	{
	case INPUT_TYPE_MOD:
		Load_MOD( &mod, verbose );
		MSL_PrintDefinition( filename, MSL_AddModule( &mod ), "MOD_" );
		Delete_Module( &mod );
		break;
	case INPUT_TYPE_S3M:
		Load_S3M( &mod, verbose );
		MSL_PrintDefinition( filename, MSL_AddModule( &mod ), "MOD_" );
		Delete_Module( &mod );
		break;
	case INPUT_TYPE_XM:
		Load_XM( &mod, verbose );
		MSL_PrintDefinition( filename, MSL_AddModule( &mod ), "MOD_" );
		Delete_Module( &mod );
		break;
	case INPUT_TYPE_IT:
		Load_IT( &mod, verbose );
		MSL_PrintDefinition( filename, MSL_AddModule( &mod ), "MOD_" );
		Delete_Module( &mod );
		break;
	case INPUT_TYPE_WAV:
		Load_WAV( &wav, verbose, true );
		wav.filename[0] = '#';	// set SFX flag (for demo)
		MSL_PrintDefinition( filename, MSL_AddSample( &wav ), "SFX_" );
		free( wav.data );
		break;
	default:
		// print error/warning
		printf( "Unknown file %s...\n", filename );
	}
	file_close_read();
	
}

int MSL_Create( char* argv[], int argc, char* output, char* header, bool verbose )
{
//	int str_w=0;
//	u8 pmode=0;
//	bool comment=false;

	int x;

	MSL_Erase();
	str_msl[0] = 0;
	F_HEADER=NULL;
	if( header )
	{
		F_HEADER = fopen( header, "wb" );
	}

//	if( !F_HEADER )
//		return -1;	// needs header file!
	
	file_open_write( TMP_SAMP );
	file_close_write();
	file_open_write( TMP_SONG );
	file_close_write();
	
	for( x = 1; x < argc; x++ )
	{
		if( argv[x][0] == '-' )
		{
			
		}
		else
		{
			MSL_LoadFile( argv[x], verbose );
		}
	}

	MSL_Export( output );

	if( F_HEADER )
	{
		fprintf( F_HEADER, "#define MSL_NSONGS	%i\r\n", MSL_NSONGS );
		fprintf( F_HEADER, "#define MSL_NSAMPS	%i\r\n", MSL_NSAMPS );
		fprintf( F_HEADER, "#define MSL_BANKSIZE	%i\r\n", (MSL_NSAMPS+MSL_NSONGS) );
		fclose( F_HEADER );
		F_HEADER=NULL;
	}

	file_delete( TMP_SAMP );
	file_delete( TMP_SONG );
	return ERR_NONE;
}