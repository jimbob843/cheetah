/*
 * Cheetah v1.0   Copyright Marlet Limited 2007
 *
 * File:			stdio.c
 * Description:		Standard Input/Output Library
 * Author:			James Smith
 * Created:			30-May-2007
 * Last Modified:	30-May-2007
 *
 */
 
#include "kernel.h"


struct filedes {
	int fd;
	char *filename;
	char *mode;
	int bufferedLBA;
	BYTE buffer[512];
	int filepointer;
};

static int filepointer = 0;
static int startclusternum = 0;

FILE *fopen( char *filename, char *mode ) {

	// Create a new FILE object
	FILE *fp = (FILE*)kmalloc(sizeof(FILE));

	// Identify the device and the filesystem

	// Try and find the filename in the directory
	fp->direntry = flp_FindDirectoryEntry( filename );
	if (fp->direntry == NULL) {
		// No such file
		return NULL;
	}

	fp->fileposition = 0;

	return fp;
}


int fread( FILE *fp, BYTE *buffer, int bytes ) {
	// [input] fp - pointer to a FILE struct
	// [input] buffer - pointer to the data buffer
	// [input] bytes - number of bytes to read

	// See if the data is in the currently buffered cluster
	int bytes_read = 0;

	while ((bytes_read < bytes) && (fp->fileposition < fp->filesize)) {

		if ((fp->buffer == NULL) || (fp->bufferposition >= 512)) {
			// The data is currently not cached. Load a cluster.
			BYTE *newbuffer = (BYTE*)kmalloc( 512 );
			if (flp_LoadNextCluster( fp, newbuffer ) == -1) {
				// Problem reading cluster
				return -1;
			}
		}

		int bytes_to_read = bytes - bytes_read;
		if (bytes_to_read > 512) {
			// Attempt to read more that one block
			bytes_to_read = 512;
		}
		if ((bytes_to_read + fp->fileposition) > fp->filesize) {
			// Attempt to read past EOF
			bytes_to_read = fp->filesize - fp->fileposition;
		}
		// Copy the next segment into the output buffer
		MEMCPY( fp->buffer, buffer[bytes_read], bytes_to_read );

		bytes_read += bytes_to_read;
		fp->bufferposition += bytes_to_read;
	}

	return bytes_read;

}


int fgetc( FILE *fp ) {
	// Read a character from the file
}


void fclose( FILE *fp ) {
	// Close the file
	kfree( fp->buffer );
	kfree( fp );
}
