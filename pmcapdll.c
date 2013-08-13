/***********************************************************************/
/*  A PM hook DLL - to keep the DLL simple, it only forwards WM_CHAR   */
/*  messages to the app using this DLL. No hotkey detection is done    */
/*  here.                                                              */
/***********************************************************************/

#define  INCL_WIN
#define  INCL_DOS
#define  INCL_DOSERRORS
#include <os2.h>

#include "pmcapdll.h"

int _acrtused = 0;

/***********************************************************************/
/*  Global variables.                                                  */
/***********************************************************************/
HAB     habDLL;
PFN     pfnInput;
HMODULE hMod;
HWND    hwndApp;   /* Controlling app's window handle */
BOOL    bRunning = FALSE;

BOOL EXPENTRY InputProc(HAB hab, PQMSG pqMsg, USHORT fs);

/***********************************************************************/
/*  InitDLL: This function sets up the DLL and all variables and       */
/*           starts the hook.                                          */
/***********************************************************************/
BOOL EXPENTRY InitDLL(HAB hab, HWND hwndCaller)
{
   USHORT  rc;

   if (bRunning)
      return TRUE;

   hwndApp = hwndCaller;  /* save for later */
   habDLL  = hab;

   /* this seems to be vital but WHY THE HELL??? */
   rc = DosGetModHandle(DLLNAME, &hMod);
   if (rc)
       return FALSE;

   rc = WinSetHook(habDLL, 0, HK_INPUT, (PFN)InputProc, hMod);
   if (rc == FALSE) {
       return FALSE;
   }
   return bRunning = TRUE;
}

/***********************************************************************/
/*  CloseDLL: This function stops the hook and cleans up resources     */
/*            used by DLL                                              */
/***********************************************************************/
VOID EXPENTRY CloseDLL(VOID)
{
   if (!bRunning)
      return;

   WinReleaseHook(habDLL, 0, HK_INPUT, (PFN)InputProc, hMod);
   bRunning = FALSE;
}

/***********************************************************************/
/*  InputProc: This is the input filter routine.                       */
/*  While the hook is active, all messages come here                   */
/*  before being dispatched. We simply pass all WM_CHAR messages to    */
/*  the controlling app.                                               */
/***********************************************************************/
BOOL EXPENTRY InputProc(HAB hab, PQMSG pqMsg, USHORT fs)
{
    hab; fs;

    switch (pqMsg->msg) {
        case WM_CHAR:
            WinPostMsg(hwndApp, MYWM_CHAR, pqMsg->mp1, pqMsg->mp2);
            return FALSE; /* returning TRUE ignores message but we don't want that! */
    }

    /***********************************************************************/
    /*  Pass the message on to the next hook in line.                      */
    /***********************************************************************/
    return FALSE;
}
