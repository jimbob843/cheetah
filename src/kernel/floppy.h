/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			floppy.h
 * Description:		FAT12 floppy driver (header)
 * Author:			James Smith
 * Created:			05-Sep-2006
 * Last Modified:	13-Sep-2006
 *
 */
 
#include "kernel.h"

int flp_InitDevice( void );
void flp_IRQHandler( void );
void flp_ReadSector( BYTE drive, BYTE head, BYTE cylinder, BYTE sector,
					BYTE *buffer );
void flp_SeekTrack( BYTE drive, BYTE head, BYTE cylinder );


struct _FAT12DirectoryEntry {
	char filename[8];
	char extension[3];
	BYTE attribute;
	BYTE reserved[10];
	WORD lastupdatetime;
	WORD lastupdatedate;
	WORD firstcluster;
	DWORD filesize;
};
typedef struct _FAT12DirectoryEntry FAT12DirectoryEntry;


struct _FILE {
	FAT12DirectoryEntry *direntry;
	DWORD fileposition;
	BYTE *buffer;
	int bufferposition;
	WORD currentcluster;
};
typedef struct _FILE FILE;



int fopen( char *filename, char *mode );
int fread( FILE *fp, BYTE *buffer, int bytes );
int fgetc( FILE *fp );
void fclose( FILE *fp );



