/*
 * Cheetah v1.0   Copyright Marlet Limited 2006
 *
 * File:			klib.h
 * Description:		Kernel library functions
 * Author:			James Smith
 * Created:			05-Sep-2006
 * Last Modified:	13-Sep-2006
 *
 */
 
 
int strcmp( char *s1, char *s2 );
int strlen( char *s );

void memcpy( void *src, void *dest, int bytes );
void memclr( void *dest, int bytes );
