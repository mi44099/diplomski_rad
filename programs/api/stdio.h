/*! Printing on stdout, reading from stdin */

#pragma once

extern inline int get_char ();
extern inline int clear_screen ();
extern inline int goto_xy ( int x, int y );
int print ( char *format, ... );

int change_stdin ( char *new_stdin );
int change_stdout ( char *new_stdout );
int change_default_stdin ( char *new_stdin );
int change_default_stdout ( char *new_stdin );
