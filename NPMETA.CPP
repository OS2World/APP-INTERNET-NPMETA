/***************************************************************************
 *
 * File name   :  npdraw.cpp
 *
 *  Copyright (C) 1996 IBM Corporation
 *
 *      DISCLAIMER OF WARRANTIES.  The following [enclosed] code is
 *      sample code created by IBM Corporation. This sample code is not
 *      part of any standard or IBM product and is provided to you solely
 *      for  the purpose of assisting you in the development of your
 *      applications.  The code is provided "AS IS", without
 *      warranty of any kind.  IBM shall not be liable for any damages
 *      arising out of your use of the sample code, even if they have been
 *      advised of the possibility of such damages.
 *
 ***************************************************************************/

#define INCL_DOS
#define INCL_WIN
#define INCL_DOSPROCESS
#define INCL_GPI
#include <os2.h>

#include <string.h>
#include <stdlib.h>

#ifndef _NPAPI_H_
#include "npapi.h"
#endif

#include <pprintf.h>
const char* szPPR="\\pipe\\printf";
const char* szPPR1="\\pipe\\printf1";

#define DEBUG_PRINT
#ifndef DEBUG_PRINT
extern "C"
int pprintf( const char* , const char* , ... ) {
  return 0;
}
#endif

int pprMatrix( PCSZ sz, MATRIXLF* matlf ) {
  return pprintf( sz, "\t%12d,%12d,%12d\n\r\t%12d,%12d,%12d\n\r\t%12d,%12d,%12d\n\r",
     (int)matlf->fxM11, (int)matlf->fxM12, (int)matlf->lM13,
     (int)matlf->fxM21, (int)matlf->fxM22, (int)matlf->lM23,
     (int)matlf->lM31, (int)matlf->lM32, (int)matlf->lM33 );
}

//
// Instance state information about the plugin.
//
// *Developers*: Use this struct to hold per-instance
//               information that you'll need in the
//               various functions in this file.
//

typedef struct _PluginInstance PluginInstance;
typedef struct _PluginInstance
{
    NPWindow*       fWindow;
    HWND            hWnd;
    uint16          fMode;
    HPS             hps;
    HMF             hmf;
    PCHAR           bufMeta;
    ULONG           cbMeta;
    ULONG           offMeta;
    RECTL           rclMeta;
    RECTL           rclWnd;
    PFNWP           lpfnOldWndProc;
    NPSavedData*    pSavedInstanceData;
    PluginInstance* pNext;
} PluginInstance;

MRESULT APIENTRY
SubClassFunc(HWND hWnd,ULONG Message,MPARAM wParam, MPARAM lParam);

void Draw(PluginInstance *This, HPS hps, BOOL fPrinting, BOOL fReset);
void SetTransform( PluginInstance *This, HPS hps, PRECTL rcl );
void SetMatrix( PluginInstance *This, HPS hps, PRECTL rclWin, MATRIXLF* mtlf );
void SelectMetaFile( PluginInstance *This, HMF hmf );

HMODULE DLLInstance=NULL;

#if defined ( __cplusplus )
extern "C" {
#endif

/* _CRT_init is the C run-time environment initialization function.         */
/* It will return 0 to indicate success and -1 to indicate failure.         */
   int _CRT_init(void);

/* __ctordtorInit calls the C++ run-time constructors for static objects.   */
   void __ctordtorInit(void);

/* __ctordtorTerm calls the C++ run-time destructors for static objects.    */
   void __ctordtorTerm(void);

#ifdef   STATIC_LINK

/* _CRT_term is the C run-time environment termination function.            */
/* It only needs to be called when the C run-time functions are statically  */
/* linked.                                                                  */
   void _CRT_term(void);

#else

/* A clean up routine registered with DosExitList must be used if runtime   */
/* calls are required at exit AND the runtime is dynamically linked.  This  */
/* will guarantee that this clean up routine is run before the library DLL  */
/* is terminated.  Do any application housekeeping in cleanup()             */
    static void _System cleanup(ULONG ulReason);

#endif


#if defined ( __cplusplus )
}
#endif


extern "C"
unsigned long _System _DLL_InitTerm(unsigned long hModule, unsigned long
                                    ulFlag)
{
    DLLInstance = (HMODULE) hModule;
    switch (ulFlag)
    {
        case 0:
            if ( _CRT_init() == -1 )
            {
                return(0UL);
            }
#if defined ( __cplusplus )
            __ctordtorInit();
#endif

#ifndef  STATIC_LINK

         /*******************************************************************/
         /* A DosExitList routine must be used to clean up if runtime calls */
         /* are required at exit and the runtime is dynamically linked.     */
         /*******************************************************************/

            DosExitList(0x0000FF00|EXLST_ADD, cleanup);
#endif
            break;
        case 1:

#if defined ( __cplusplus )
            __ctordtorTerm();
#endif

#ifdef  STATIC_LINK
            _CRT_term();
#endif
            break;
    }

    return 1;
}

#ifndef  STATIC_LINK
static void cleanup(ULONG ulReason)
{
   /* do any DLL cleanup here if needed AND if dynamically linked to the */
   /* C Runtime libraries                                                */
   DosExitList(EXLST_EXIT, cleanup);   /* remember to unlink ourselves   */
   return ;
}
#endif

// A plugin instance typically will subclass the plugin's client window, so
// it can get Windows messages, (such as paint, palettechanged, keybd, etc).
// To do work associated with a specific plugin instance the WndProc which
// receives the Windows messages, (named "SubClassFunc" herein), needs access
// to the "This" (PluginInstance*) ptr.
// If the plugin wants all the keyboard messages, it will have to
// create a frame window with it's own accelerator table (even it it is empty)
// and insure that that frame tracks the plugin window that Navigator creates,
// otherwise the Navigator Accelerators will interfere with the WM_CHARs
// that the plugin windows receive...

// When Navigator registers the plugin client's window class, (the class for
// the window passed in NPP_SetWindow()), Navigator reserves 4
// "extra" window bytes for the plugins use... at QWL_USER
// Associate the hWnd with pInstance by setting the hWnd member of the
// PluginInstance struct.
static void AssociateInstance(HWND hWnd, PluginInstance* pInstance)
{
    pInstance->hWnd = hWnd;     // redundant, but usefull to get hwnd from
                                // pinstance later.
    BOOL rc = WinSetWindowULong(hWnd, QWL_USER, (ULONG)pInstance);
}

// Find the PluginInstance associated with this hWnd and return it
static PluginInstance* GetInstance(HWND hWnd)
{
    return (PluginInstance*)WinQueryWindowULong(hWnd, QWL_USER);
}

//----------------------------------------------------------------------------
// NPP_Initialize:
//----------------------------------------------------------------------------
NPError NPP_Initialize(void)
{
    // do your one time initialization here, such as dynamically loading
    // dependant DLLs
    return NPERR_NO_ERROR;
}


//----------------------------------------------------------------------------
// NPP_Shutdown:
//----------------------------------------------------------------------------
void NPP_Shutdown(void)
{
    // do your one time uninitialization here, such as unloading dynamically
    // loaded DLLs
}


//----------------------------------------------------------------------------
// NPP_New:
//----------------------------------------------------------------------------
NPError NP_LOADDS
NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved)
{
  if ( instance == NULL ) return NPERR_INVALID_INSTANCE_ERROR;

  instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
  PluginInstance* This = (PluginInstance*) instance->pdata;

  if ( This == NULL ) return NPERR_OUT_OF_MEMORY_ERROR;
  //
  // *Developers*: Initialize fields of your plugin
  // instance data here.  If the NPSavedData is non-
  // NULL, you can use that data (returned by you from
  // NPP_Destroy to set up the new plugin instance.
  //

  This->fWindow = 0;
  // mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h)
  This->fMode = mode;
  This->hWnd = 0;
  This->hps = NULL;
  This->hmf = NULL;
  This->bufMeta=NULL;
  This->cbMeta=0;
  This->offMeta=0;
  This->pSavedInstanceData = saved;
  This->pNext = 0;

  return NPERR_NO_ERROR;
}


//-----------------------------------------------------------------------------
// NPP_Destroy:
//-----------------------------------------------------------------------------
NPError NP_LOADDS
NPP_Destroy(NPP instance, NPSavedData** save) {
  if ( instance == 0 ) return NPERR_INVALID_INSTANCE_ERROR;

  PluginInstance* This = (PluginInstance*) instance->pdata;

  //
  // *Developers*: If desired, call NP_MemAlloc to create a
  // NPSavedDate structure containing any state information
  // that you want restored if this plugin instance is later
  // recreated.
  //

  if ( This ) {
    // destroy hmf and PS
    if ( This->hmf ) GpiDeleteMetaFile( This->hmf );
    if ( This->hps ) GpiDestroyPS( This->hps );
    // delete buffer
    if ( This->bufMeta ) {
      free( This->bufMeta );
      This->bufMeta=NULL;
      This->cbMeta=0;
      This->offMeta=0;
    }
    // Remove the subclass for the client window
    if ( This->hWnd ) WinSubclassWindow( This->hWnd, This->lpfnOldWndProc );
    // make some saved instance data if necessary
    if ( This->pSavedInstanceData == 0 ) {
      // make a struct header for the data
      This->pSavedInstanceData =
          (NPSavedData*)NPN_MemAlloc(sizeof (struct _NPSavedData));
      // fill in the struct
      if ( This->pSavedInstanceData != 0 ) {
        This->pSavedInstanceData->len = 0;
        This->pSavedInstanceData->buf = 0;

        // replace the def below and references to it with your data
        #define SIDATA "aSavedInstanceDataBlock"

        // the data
        This->pSavedInstanceData->buf = NPN_MemAlloc(sizeof SIDATA);

        if( This->pSavedInstanceData->buf ) {
          strcpy((char*)This->pSavedInstanceData->buf, SIDATA);
          This->pSavedInstanceData->len = sizeof SIDATA;
        }
      }
    }

    // save some instance data
    *save = This->pSavedInstanceData;

    NPN_MemFree(instance->pdata);
    instance->pdata = 0;
  }
  return NPERR_NO_ERROR;
}


//----------------------------------------------------------------------------
// NPP_SetWindow:
//----------------------------------------------------------------------------
NPError NP_LOADDS
NPP_SetWindow(NPP instance, NPWindow* window) {
  pprintf( szPPR, "NPP_SetWindow\n\r" );
  if ( instance == 0 ) return NPERR_INVALID_INSTANCE_ERROR;

  PluginInstance* This = (PluginInstance*) instance->pdata;

  //
  // *Developers*: Before setting fWindow to point to the
  // new window, you may wish to compare the new window
  // info to the previous window (if any) to note window
  // size changes, etc.
  //

  // if window handle changed
  if ( This->hWnd != (HWND)window->window ) {
    pprintf( szPPR, "HWND (hmf %x) changed, %x --> %x\n\r", This->hmf, (int)This->hWnd, (int)window->window );
    // remember the new window
    This->fWindow = window;
    if ( This->hWnd ) {
      // Remove the subclass for the old client window
      WinSubclassWindow(This->hWnd, This->lpfnOldWndProc);
    }
    // remember the new window handle
    This->hWnd = (HWND)This->fWindow->window;
    // subclass the window
    This->lpfnOldWndProc = WinSubclassWindow(This->hWnd, SubClassFunc);
    AssociateInstance( This->hWnd, This );
    /* paint a background for the drawing */
    WinQueryWindowRect( This->hWnd, &This->rclWnd );
//    WinFillRect( This->hps, &This->rclWnd, CLR_PALEGRAY );
    // destroy old PS and create new PS
    if ( This->hps ) GpiDestroyPS( This->hps );
    HDC hdc = WinQueryWindowDC( This->hWnd );
    if ( !hdc ) hdc = WinOpenWindowDC( This->hWnd );
    SIZEL siz={ 0, 0 };
    HAB hab=WinQueryAnchorBlock( This->hWnd );
    This->hps = GpiCreatePS( hab, hdc, &siz,
                                  PU_PELS | GPIT_NORMAL | GPIA_ASSOC );
    pprintf( szPPR, "GpiCreatePS, hdc=%x, hps=%x\n\r", (int)hdc, (int)This->hps );
    if ( !This->hps ) {
      pprintf( szPPR, "GpiCreatePS failed, Err=%x\n\r",
        WinGetLastError( hab ) );
    }
    SetTransform( This, This->hps, &This->rclWnd );
  } else { // check if window coordinates changed.
           //It may happens for full-screan
    RECTL rcl;
    WinQueryWindowRect( This->hWnd, &rcl );
    if ( memcmp( &This->rclWnd, &rcl, sizeof( rcl ) ) ) {
      pprintf( szPPR, "Rect (hmf %x) changed, ( %d, %d ) - ( %d, %d ) --> ( %d, %d ) - ( %d, %d )\n\r",
        This->hmf,
        This->rclWnd.xLeft, This->rclWnd.yBottom, This->rclWnd.xRight, This->rclWnd.yTop,
        rcl.xLeft, rcl.yBottom, rcl.xRight, rcl.yTop );
      memcpy( &This->rclWnd, &rcl, sizeof( rcl ) );
      SetTransform( This, This->hps, &This->rclWnd );
    }
  }
  return NPERR_NO_ERROR;
}


//----------------------------------------------------------------------------
// NPP_NewStream:
//----------------------------------------------------------------------------
NPError NP_LOADDS
NPP_NewStream(NPP instance,
              NPMIMEType type,
              NPStream *stream,
              NPBool seekable,
              uint16 *stype)
{
  if ( instance == 0 ) return NPERR_INVALID_INSTANCE_ERROR;
  PluginInstance* This = (PluginInstance*) instance->pdata;
//  pprintf( szPPR, "NPP_NewStream( url=%s, end=%d )\n\r", stream->url, stream->end );
  // delete buffer
  if ( This->bufMeta ) {
    free( This->bufMeta );
    This->bufMeta=NULL;
    This->cbMeta=0;
    This->offMeta=0;
  }
  if ( stream->end ) This->cbMeta=stream->end;
  else This->cbMeta=0x10000; // 64K initial size for unknown size stream
  //create buffer
  This->bufMeta=(char*)malloc( This->cbMeta );

  // if your plugin must operate file based, you may wish to do this:
  //    *stype = NP_ASFILE;
  // remember, though, that use of NP_ASFILE is strongly discouraged;
  // your plugin should attempt to work with data as it comes in on
  // the stream if at all possible

//  *stype = NP_ASFILE;

  return NPERR_NO_ERROR;
}


//
// *Developers*:
// These next 2 functions are directly relevant in a plug-in which handles the
// data in a streaming manner.  If you want zero bytes because no buffer space
// is YET available, return 0.  As long as the stream has not been written
// to the plugin, Navigator will continue trying to send bytes.  If the plugin
// doesn't want them, just return some large number from NPP_WriteReady(), and
// ignore them in NPP_Write().  For a NP_ASFILE stream, they are still called
// but can safely be ignored using this strategy.
//

//----------------------------------------------------------------------------
// NPP_WriteReady:
//----------------------------------------------------------------------------
int32 NP_LOADDS
NPP_WriteReady(NPP instance, NPStream *stream) {
  if ( instance == 0 ) return NPERR_INVALID_INSTANCE_ERROR;
  PluginInstance* This = (PluginInstance*) instance->pdata;
  int rest=This->cbMeta-This->offMeta;
  if ( rest<=0 ) {
    This->bufMeta=(char*)realloc( This->bufMeta, This->cbMeta*=2 );
    rest=This->cbMeta-This->offMeta;
  }
  return rest;   // Number of bytes ready to accept in NPP_Write()
}


//----------------------------------------------------------------------------
// NPP_Write:
//----------------------------------------------------------------------------
int32 NP_LOADDS
NPP_Write(NPP instance, NPStream *stream,
          int32 offset, int32 len, void *buffer) {
  if ( instance == 0 ) return NPERR_INVALID_INSTANCE_ERROR;
  PluginInstance* This = (PluginInstance*) instance->pdata;
//  pprintf( szPPR, "NPP_Write off=%d len=%d\n\r", offset, len );
  if ( This->bufMeta ) {
    int rest=This->cbMeta-offset;
    if ( len>rest ) len=rest;
    memcpy( This->bufMeta+offset, buffer, len );
    This->offMeta=offset+len;
  }
  return len;
}


//----------------------------------------------------------------------------
// NPP_DestroyStream:
//----------------------------------------------------------------------------
NPError NP_LOADDS
NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason) {
  if ( instance == 0 ) return NPERR_INVALID_INSTANCE_ERROR;
  PluginInstance* This = (PluginInstance*) instance->pdata;

  if ( reason == NPRES_DONE && This->bufMeta ) {
    // create metafile from buffer

    // 1-st, create empty metafile
    HAB hab=WinQueryAnchorBlock( This->hWnd );
    DEVOPENSTRUC dop;
    dop.pszLogAddress = (PSZ) NULL;
    dop.pszDriverName = "DISPLAY";
    HDC hdcMeta = DevOpenDC( hab,
        OD_METAFILE,                 /* Metafile device context             */
        "*",                         /* Ignores OS2.INI                     */
        2L,                          /* Uses first two fields               */
        (PDEVOPENDATA) &dop,         /* Device information                  */
        (HDC) NULLHANDLE );          /* Compatible device context           */
    SIZEL sizlPage={ 0, 0 };
    HPS hpsMeta = GpiCreatePS( hab, hdcMeta, &sizlPage, PU_PELS | GPIA_ASSOC );
    GpiAssociate( hpsMeta, (HDC)NULLHANDLE );
    HMF hmf = DevCloseDC( hdcMeta );
    GpiDestroyPS( hpsMeta );

    // 2-nd, add real data
    GpiSetMetaFileBits( hmf, 0, This->offMeta, This->bufMeta );
    SelectMetaFile( This, hmf );
  }
  // delete buffer
  if ( This->bufMeta ) {
    free( This->bufMeta );
    This->bufMeta=NULL;
    This->cbMeta=0;
    This->offMeta=0;
  }
  return NPERR_NO_ERROR;
}


//----------------------------------------------------------------------------
// NPP_StreamAsFile:
//----------------------------------------------------------------------------
void NP_LOADDS
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname) {
  if ( instance == 0 ) return;
  PluginInstance* This = (PluginInstance*) instance->pdata;
  if ( This ) {
    HAB hab=WinQueryAnchorBlock( This->hWnd );
    HMF hmf=GpiLoadMetaFile( hab, fname );
    if ( hmf ) {
      SelectMetaFile( This, hmf );
    } else pprintf( szPPR, "GpiLoadMetaFile( %s ) failed\n\r", fname );
  }
}


//----------------------------------------------------------------------------
// NPP_Print:
//----------------------------------------------------------------------------
void NP_LOADDS
NPP_Print(NPP instance, NPPrint* printInfo) {
  pprintf( szPPR, "NPP_Print\n\r" );
  if ( printInfo == 0 ) return;  // trap invalid parm
  if ( instance ) {
    PluginInstance* This = (PluginInstance*) instance->pdata;

    if ( printInfo->mode == NP_FULL ) {
      //
      // *Developers*: If your plugin would like to take over
      // printing completely when it is in full-screen mode,
      // set printInfo->pluginPrinted to TRUE and print your
      // plugin as you see fit.  If your plugin wants Netscape
      // to handle printing in this case, set printInfo->pluginPrinted
      // to FALSE (the default) and do nothing.  If you do want
      // to handle printing yourself, printOne is true if the
      // print button (as opposed to the print menu) was clicked.
      // On the Macintosh, platformPrint is a THPrint; on Windows,
      // platformPrint is a structure (defined in npapi.h) containing
      // the printer name, port, etc.
      //
      void* platformPrint = printInfo->print.fullPrint.platformPrint;
      NPBool printOne = printInfo->print.fullPrint.printOne;

      printInfo->print.fullPrint.pluginPrinted = FALSE; // Do the default

    } else {   // If not fullscreen, we must be embedded
      //
      // *Developers*: If your plugin is embedded, or is full-screen
      // but you returned false in pluginPrinted above, NPP_Print
      // will be called with mode == NP_EMBED.  The NPWindow
      // in the printInfo gives the location and dimensions of
      // the embedded plugin on the printed page.  On the Macintosh,
      // platformPrint is the printer port; on Windows, platformPrint
      // is the handle to the printing device context. On OS/2,
      // platformPrint is the printing presentation space (HPS).
      //
      NPWindow* printWindow = &(printInfo->print.embedPrint.window);

      /* get Presentation Space and save it */
      void* platformPrint = printInfo->print.embedPrint.platformPrint;
      HPS hps = (HPS)platformPrint;

      /* create GPI various data structures about the drawing area */
      POINTL offWindow = { (int)printWindow->x, (int)printWindow->y };
      RECTL rect = { (int)printWindow->x,
                     (int)printWindow->y,
                     (int)printWindow->x + (int)printWindow->width,
                     (int)printWindow->y + (int)printWindow->height };

      /* get model transform so origin is 0,0 */
//      MATRIXLF matModel;
//      GpiQueryModelTransformMatrix( hps, 9L, &matModel );
//      GpiTranslate( hps, &matModel, TRANSFORM_ADD, &offWindow );
//      GpiSetModelTransformMatrix( hps, 9L, &matModel, TRANSFORM_REPLACE );

      if ( This->hmf ) {
//        pprintf( szPPR, "PrintWindow ( %d, %d ) - ( %d, %d )\n\r",
//        rect.xLeft, rect.yBottom, rect.xRight, rect.yTop );
        MATRIXLF mtlf;
/*
        GpiQueryDefaultViewMatrix( hps, 9L, &mtlf );
        pprintf( szPPR, "GpiQueryDefaultViewMatrix\n\r" );
        pprMatrix( szPPR, &mtlf );
        GpiQueryModelTransformMatrix( hps, 9L, &mtlf );
        pprintf( szPPR, "GpiQueryModelTransformMatrix\n\r" );
        pprMatrix( szPPR, &mtlf );
        GpiQueryViewingTransformMatrix( hps, 9L, &mtlf );
        pprintf( szPPR, "GpiQueryViewingTransformMatrix\n\r" );
        pprMatrix( szPPR, &mtlf );

        SIZEL siz;
        LONG lOpt=GpiQueryPS( hps, &siz );
        pprintf( szPPR, "GpiQueryPS : (%d, %d), %x\n\r", siz.cx, siz.cy, lOpt );
*/
        HAB hab=WinQueryAnchorBlock( This->hWnd );

        SetMatrix( This, hps, &rect, &mtlf );
        // for correct GpiPlayMetaFile output
        if ( GpiSetViewingTransformMatrix( hps, 9L, &mtlf, TRANSFORM_REPLACE ) ) {
//          pprintf( szPPR, "GpiSetViewingTransformMatrix done\n\r" );
//          pprMatrix( szPPR, &mtlf );
        } else {
          pprintf( szPPR, "GpiSetViewingTransformMatrix failed, Err=%x\n\r",
            WinGetLastError( hab ) );
        }
/*
        if ( GpiSetDefaultViewMatrix( hps, 9L, &mtlf, TRANSFORM_REPLACE ) ) {
          pprintf( szPPR, "GpiSetDefaultViewMatrix done\n\r" );
          pprMatrix( szPPR, &mtlf );
        } else {
          pprintf( szPPR, "GpiSetDefaultViewMatrix failed, Err=%x\n\r",
            WinGetLastError( hab ) );
        }
*/
        // for correct box display
        if ( GpiSetModelTransformMatrix( hps, 9L, &mtlf, TRANSFORM_REPLACE ) ) {
//          pprintf( szPPR, "GpiSetModelTransformMatrix done\n\r" );
//          pprMatrix( szPPR, &mtlf );
        } else {
          pprintf( szPPR, "GpiSetModelTransformMatrix failed, Err=%x\n\r",
            WinGetLastError( hab ) );
        }

      }
      /* draw using common drawing routine */
      Draw( This, hps, TRUE, FALSE );
    }
  }
}


//----------------------------------------------------------------------------
// NPP_HandleEvent:
// Mac-only.
//----------------------------------------------------------------------------
int16 NP_LOADDS NPP_HandleEvent(NPP instance, void* event)
{
    NPBool eventHandled = FALSE;
    if (instance == 0   )
        return eventHandled;

    PluginInstance* This = (PluginInstance*) instance->pdata;

    //
    // *Developers*: The "event" passed in is a Macintosh
    // EventRecord*.  The event.what field can be any of the
    // normal Mac event types, or one of the following additional
    // types defined in npapi.h: getFocusEvent, loseFocusEvent,
    // adjustCursorEvent.  The focus events inform your plugin
    // that it will become, or is no longer, the recepient of
    // key events.  If your plugin doesn't want to receive key
    // events, return false when passed at getFocusEvent.  The
    // adjustCursorEvent is passed repeatedly when the mouse is
    // over your plugin; if your plugin doesn't want to set the
    // cursor, return false.  Handle the standard Mac events as
    // normal.  The return value for all standard events is currently
    // ignored except for the key event: for key events, only return
    // true if your plugin has handled that particular key event.
    //

    return eventHandled;
}

//
// Here is a sample subclass function.
//
MRESULT APIENTRY
SubClassFunc(  HWND hWnd,
               ULONG Message,
               MPARAM wParam,
               MPARAM lParam)
{
  PluginInstance *This = GetInstance(hWnd);

  switch(Message) {
  case WM_REALIZEPALETTE:
    WinInvalidateRect(hWnd, 0, TRUE);
    WinUpdateWindow(hWnd);
    return 0;
    break;

  case WM_PAINT:
    {

      RECTL rcl;
      WinQueryWindowRect( This->hWnd, &rcl );
      pprintf( szPPR, "Rect (hmf %x) : ( %d, %d ) - ( %d, %d )\n\r",
        This->hmf,
        rcl.xLeft, rcl.yBottom, rcl.xRight, rcl.yTop );
      WinInvalidateRect(hWnd, 0, TRUE);

      RECTL invalidRect;
      WinBeginPaint( hWnd, This->hps, &invalidRect );

      /* draw using common drawing routine */
      Draw( This, This->hps, FALSE, TRUE );
      WinEndPaint( This->hps );

      return (MRESULT)0;
    }
    break;

  default:
    break;
  }

  return ((PFNWP)This->lpfnOldWndProc)( hWnd, Message, wParam, lParam );
}

void Draw( PluginInstance *This, HPS hps, BOOL fPrinting, BOOL fReset ) {
/*
  if (fPrinting) {
    // You do not normally print a streaming plugin. We will display a
    // bounding rectangle and a message string (if it fits)
    POINTL ptl = { 0, 0 };
    GpiMove(hps, &ptl);
    GpiSetColor(hps, CLR_BLACK);
    GpiBox(hps, DRO_OUTLINE, endPoint, 0L, 0L);
    ptl.x += 40;
    ptl.y = endPoint->y/4*3;
    char *msg = "NPDRAW Plugin";
    GpiCharStringAt(hps, &ptl, strlen(msg), msg);
    ptl.y = endPoint->y/4*2;
    msg = "does not print";
    GpiCharStringAt(hps, &ptl, strlen(msg), msg);
    ptl.y = endPoint->y/4;
    msg = "streaming data.";
    GpiCharStringAt(hps, &ptl, strlen(msg), msg);
  } else
*/
  {
    if ( This->hmf ) {
      LONG lSegCount=0;
      char szDesc[20];
      LONG opt[9]={0,LT_DEFAULT,0,LC_LOADDISC,RES_NORESET,
                   SUP_DEFAULT,CTAB_REPLACE,CREA_DEFAULT,
                   DDEF_DEFAULT};
      HAB hab=WinQueryAnchorBlock( This->hWnd );
      LONG lHits=GpiPlayMetaFile( hps, This->hmf, 9L, opt, &lSegCount,
        sizeof( szDesc ), szDesc );
      if ( lHits!=GPI_OK ) pprintf( szPPR, "GpiPlayMetaFile error %d (%s)\n\r", lHits, szDesc );
      else pprintf( szPPR, "GpiPlayMetaFile OK (%s)\n\r", szDesc );
      if ( fReset ) {
        pprintf( szPPR, "Reset hps\n\r" );
        MATRIXLF   matlfSaveTransform;
        if ( GpiQueryDefaultViewMatrix( hps, 9L, &matlfSaveTransform ) ) {
          if ( GpiResetPS( hps, GRES_ALL ) ) {
            if ( GpiSetDefaultViewMatrix( hps, 9L, &matlfSaveTransform, TRANSFORM_REPLACE ) ) {
//              pprintf( szPPR, "DefaultViewMatrix preserved:\n\r" );
//              pprMatrix( szPPR, &matlfSaveTransform );
            } else {
              pprintf( szPPR, "GpiSetDefaultViewMatrix failed, Err=%x\n\r",
                WinGetLastError( hab ) );
            }
          } else {
            pprintf( szPPR, "GpiResetPS failed, Err=%x\n\r",
              WinGetLastError( hab ) );
          }
        } else {
          pprintf( szPPR, "GpiQueryDefaultViewMatrix failed, Err=%x\n\r",
            WinGetLastError( hab ) );
        }
      }
    }
  }
}

void SetTransform( PluginInstance *This, HPS hps, PRECTL rclWin ) {
  if ( !This->hmf ) return;
  HAB hab=WinQueryAnchorBlock( This->hWnd );
  MATRIXLF mtlf;
  SetMatrix( This, hps, rclWin, &mtlf );
  if ( GpiSetDefaultViewMatrix( hps, 9L, &mtlf, TRANSFORM_REPLACE ) ) {
//    pprintf( szPPR, "GpiSetDefaultViewMatrix done\n\r" );
//    pprMatrix( szPPR, &mtlf );
  } else {
    pprintf( szPPR, "GpiSetDefaultViewMatrix failed, Err=%x\n\r",
      WinGetLastError( hab ) );
  }
}

void SetMatrix( PluginInstance *This, HPS hps, PRECTL rclWin, MATRIXLF* mtlf ) {
  if ( !This->hmf ) return;
  HAB hab=WinQueryAnchorBlock( This->hWnd );
  pprintf( szPPR, "Metafile rect (%d, %d) - (%d, %d)\n\r",
    This->rclMeta.xLeft, This->rclMeta.yBottom, This->rclMeta.xRight, This->rclMeta.yTop );
  pprintf( szPPR, "Window rect (%d, %d) - (%d, %d)\n\r",
    rclWin->xLeft, rclWin->yBottom, rclWin->xRight, rclWin->yTop );
  int dxMeta=This->rclMeta.xRight-This->rclMeta.xLeft;
  int dyMeta=This->rclMeta.yTop-This->rclMeta.yBottom;
  int dxWin=rclWin->xRight-rclWin->xLeft;
  int dyWin=rclWin->yTop-rclWin->yBottom;
  FIXED afxScale[2] = { (dxWin*0x10000)/dxMeta,
                        (dyWin*0x10000)/dyMeta };
  POINTL ptlScaleOffset = { rclWin->xLeft-This->rclMeta.xLeft,
                            rclWin->yBottom-This->rclMeta.yBottom };
  if ( GpiTranslate( hps, mtlf, TRANSFORM_REPLACE, &ptlScaleOffset ) ) {
//    pprintf( szPPR, "offset= (%12d,%12d)\n\r", ptlScaleOffset.x, ptlScaleOffset.y );
//    pprMatrix( szPPR, mtlf );
  } else {
    pprintf( szPPR, "GpiTranslate failed, Err=%x\n\r",
      WinGetLastError( hab ) );
  }
  ptlScaleOffset.x=rclWin->xLeft;
  ptlScaleOffset.y=rclWin->yBottom;
  if ( GpiScale( hps, mtlf, TRANSFORM_ADD, afxScale, &ptlScaleOffset ) ) {
//    pprintf( szPPR, "scale= (%12d,%12d)\n\r", afxScale[0], afxScale[1] );
//    pprMatrix( szPPR, mtlf );
  } else {
    pprintf( szPPR, "GpiScale failed, Err=%x\n\r",
      WinGetLastError( hab ) );
  }
}

void SelectMetaFile( PluginInstance *This, HMF hmf ) {
  if ( This->hmf ) GpiDeleteMetaFile( This->hmf );
  This->hmf=hmf;
  HAB hab=WinQueryAnchorBlock( This->hWnd );
  HPS hps=This->hps;
  if ( !hps ) {
    pprintf( szPPR, "No hps ???\n\r" );
    return;
  }
  //calculate bounds
  HDC hdc = WinQueryWindowDC( This->hWnd );
  if ( !hdc ) hdc = WinOpenWindowDC( This->hWnd );
  GpiResetBoundaryData( hps );
  if ( GpiAssociate( hps, NULL ) ) {
    GpiSetDrawControl( hps, DCTL_BOUNDARY, DCTL_ON );
    Draw( This, hps, FALSE, FALSE );
    if ( GpiQueryBoundaryData( hps, &This->rclMeta ) && (This->rclMeta.xLeft<This->rclMeta.xRight) ) {
      if ( !GpiAssociate( hps, hdc ) ) pprintf( szPPR, "GpfAssociate( hps, hdc ) failed\n\r" );
      GpiSetDrawControl( hps, DCTL_BOUNDARY, DCTL_OFF );
      GpiResetPS( hps, GRES_ALL);
      SetTransform( This, hps, &This->rclWnd );
    } else {
      pprintf( szPPR, "GpiQueryBoundaryData failed, Err=%x\n\r",
        WinGetLastError( hab ) );
    }
  } else pprintf( szPPR, "GpfAssociate( hps, NULL ) failed\n\r" );
  // invalidate window to ensure a redraw
  WinInvalidateRect( This->hWnd, 0, TRUE );
}
