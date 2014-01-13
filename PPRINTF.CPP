#include <stdarg.h>
#include <stdio.h>
#include <pprintf.h>
#define INCL_DOSNMPIPES
#include <os2.h>

static char s[1024]={0};

int pprintf( const char* szPipe, const char* szFmt, ... ) {
  va_list lst;
  va_start( lst, szFmt );
  int n=vsprintf( s, szFmt, lst );
  va_end( lst );
  if ( n>0 ) { // write string to pipe
    const to=1000; // 1 sec
    ULONG dummy=0;
    DosCallNPipe( szPipe, s, n, &dummy, sizeof( dummy ), &dummy, to );
  }
  return n;
}
