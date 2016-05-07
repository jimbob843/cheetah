/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			klib.c
 * Description:		Kernel library functions
 * Author:			James Smith
 * Created:			05-Sep-2006
 * Last Modified:	13-Sep-2006
 *
 */

#include "kernel.h"

 
int strcmp( char *s1, char *s2 ) {
 
	while (*s1 == *s2++) {
		if (*s1++ == 0) {
			return 0;
		}
	}
	
	return (*s1 - *(s2-1));
}

int strlen( char *s ) {
	int count = 0;
	while (*s != 0) {
		count++;
		s++;
	}
	return count;
}


void memcpy( void *src, void *dest, int bytes ) {
	MEMCPY( src, dest, bytes );
}

void memclr( void *dest, int bytes ) {
	MEMCLR( dest, bytes );
}
