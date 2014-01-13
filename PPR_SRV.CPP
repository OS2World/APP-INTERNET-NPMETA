#define INCL_BASE
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_DOSNMPIPES
#include <os2.h>
#include <signal.h>
#include <setjmp.h>

#define HF_STDOUT 1

static jmp_buf jbuf;

extern "C" {
  void my_sig( int );
}

// Ctrl-Break indicator
static volatile int bBreak=0;
void my_sig( int ) { bBreak=1; longjmp( jbuf, 1 ); }

int main( int argc, char* argv[] ) {
  const cbBuf=10240;
  PSZ szPipeName="\\PIPE\\PRINTF";
  if ( argc>1 ) szPipeName=argv[1];
  HPIPE hPipe=NULLHANDLE;             /* Pipe handle */
  APIRET rc=NO_ERROR;               /* Return code */

  rc=DosCreateNPipe( szPipeName,
                     &hPipe,
                     NP_ACCESS_DUPLEX,
                     NP_RMESG | NP_WMESG |
                     NP_WAIT |
                     0x01,          /* Unique instance of pipe */
                     0,             /* Output buffer size */
                     cbBuf,         /* Input buffer size */
                     0L );          /* Use default time-out */
  if ( rc==NO_ERROR ) {
    _SigFunc old_int=signal(SIGINT, my_sig);
    _SigFunc old_brk=signal(SIGBREAK, my_sig);
    PSZ szBuf=new char[cbBuf];
    for(;;) {
      setjmp( jbuf );
      if ( bBreak ) {
        DosBeep( 400, 150 );
        break;
      }
      rc=DosConnectNPipe( hPipe );
      if ( rc==NO_ERROR ) {
        ULONG ulBytes=0;                      /* Bytes read or written */
        rc=DosRead( hPipe,            /* Handle of pipe */
                    szBuf,               /* Buffer for message read */
                    cbBuf,       /* Buffer size */
                    &ulBytes);             /* Number of bytes actually read */
        if ( rc==NO_ERROR ) {
          ULONG ulDummy=1, ulDummy1=0;
          // for DosCallNPipe
          DosWrite( hPipe, &ulDummy, sizeof( ulDummy ), &ulDummy1 );
          DosWrite( HF_STDOUT, szBuf, ulBytes, &ulBytes );
        } else break;
        DosResetBuffer( hPipe );
        DosDisConnectNPipe( hPipe );
      } else {
        DosBeep( 400, 150 );
      }
    }
    delete[] szBuf;
    DosClose( hPipe );
    signal(SIGBREAK, old_brk);
    signal(SIGINT, old_int);
  } else {
    DosBeep( 400, 150 );
  }
  return rc;
}



