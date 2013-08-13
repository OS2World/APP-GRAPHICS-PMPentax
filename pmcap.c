#define INCL_WIN
#define INCL_GPI
#define INCL_BITMAPFILEFORMAT
#include <os2.h>

#include "pmcap.h"
#include "pmcapdll.h"

MRESULT EXPENTRY _loadds ClientWndProc(HWND, USHORT, MPARAM, MPARAM);

CHAR szClientClass[] = "Capture";
HAB  hab;

int main(void)
{
    static ULONG flFrameFlags = FCF_TITLEBAR      | FCF_SYSMENU    |
                                FCF_SIZEBORDER    | FCF_MINMAX     |
                                FCF_SHELLPOSITION | FCF_TASKLIST   |
                                FCF_MENU          | FCF_ACCELTABLE |
                                FCF_ICON;
    HMQ          hmq;
    HWND         hwndFrame, hwndClient;
    QMSG         qmsg;

    hab = WinInitialize(0);
    hmq = WinCreateMsgQueue(hab, 0);

    WinRegisterClass(hab, szClientClass, ClientWndProc, CS_SIZEREDRAW, 0);

    hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE,
                                   &flFrameFlags, szClientClass, "PM Capture",
                                   0L, 0, ID_RESOURCE, &hwndClient);

    if (!hwndFrame)
        return 1;

    WinSetWindowText(hwndFrame, "PM Pentax");

    while (WinGetMsg(hab, &qmsg, NULL, 0, 0))
        WinDispatchMsg(hab, &qmsg);

    WinDestroyWindow(hwndFrame);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    return 0;
}

HBITMAP CopyScreenToBitmap(VOID)
{
    BITMAPINFOHEADER bmp;
    HBITMAP          hbm;
    HDC              hdcMemory;
    HPS              hps, hpsMemory;
    LONG             alBmpFormats[2];
    POINTL           aptl[3];
    SIZEL            sizl;
                                  // Create memory DC and PS

    hdcMemory = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULL);

    sizl.cx = sizl.cy = 0;
    hpsMemory = GpiCreatePS(hab, hdcMemory, &sizl,
                            PU_PELS    | GPIF_DEFAULT |
                            GPIT_MICRO | GPIA_ASSOC);

                                  // Create bitmap for destination

    GpiQueryDeviceBitmapFormats(hpsMemory, 2L, alBmpFormats);

    bmp.cbFix     = sizeof(bmp);
    bmp.cx        = (USHORT) WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    bmp.cy        = (USHORT) WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
    bmp.cPlanes   = (USHORT) alBmpFormats[0];
    bmp.cBitCount = (USHORT) alBmpFormats[1];

    hbm = GpiCreateBitmap(hpsMemory, &bmp, 0L, NULL, NULL);

                                  // Copy from screen to bitmap
    if (hbm != NULL) {
         GpiSetBitmap(hpsMemory, hbm);
         hps = WinGetScreenPS(HWND_DESKTOP);

         aptl[0].x = 0;
         aptl[0].y = 0;
         aptl[1].x = bmp.cx;
         aptl[1].y = bmp.cy;
         aptl[2].x = 0;
         aptl[2].y = 0;

         WinLockVisRegions(HWND_DESKTOP, TRUE);

         GpiBitBlt(hpsMemory, hps, 3L, aptl, ROP_SRCCOPY, BBO_IGNORE);

         WinLockVisRegions(HWND_DESKTOP, FALSE);

         WinReleasePS(hps);
         }

    /* Save the bitmap to disk */
    if (hbm != NULL) {
        HFILE              hFile;
        USHORT             usAction, usWritten;
        static CHAR        szFileName[14] = "scr_00.bmp";
        PCHAR              pchBits;
        SEL                selBits;
        BITMAPFILEHEADER   bfh;
        struct {
            BITMAPINFO     bmi;
            RGB            argbColors[255];
            } bmap;
        LONG               lStartScan, lScans, lScansRead;
        USHORT             usLineSize;
        static UCHAR       uchFileNo = 0;

        /* Fix up the file name */
        szFileName[5] = (CHAR)('0' + uchFileNo % 10);
        szFileName[4] = (CHAR)('0' + uchFileNo / 10);
        uchFileNo++;
        if (uchFileNo == 100)
            uchFileNo = 0;

        /* Create the bitmap file */
        if (!DosOpen(szFileName, &hFile, &usAction, 0L, FILE_NORMAL,
            FILE_CREATE | FILE_TRUNCATE,
            OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYWRITE, 0L)) {

            /* Fill out bitmap file header */
            bfh.usType   = BFT_BMAP;
            bfh.cbSize   = sizeof(bfh);
            bfh.xHotspot = 0;
            bfh.yHotspot = 0;
            bfh.offBits  = sizeof(bfh) + 256 * sizeof(RGB);
            bfh.bmp      = bmp;

            /* Set up for writing out the bitmap */
            bmap.bmi.cbFix      = sizeof(BITMAPINFOHEADER);
            GpiQueryBitmapParameters(hbm, (BITMAPINFOHEADER*)(&bmap.bmi));
            DosAllocSeg(0, &selBits, 0);
            pchBits    = MAKEP(selBits, 0);
            lStartScan = 0;
            usLineSize = ((bmap.bmi.cBitCount * bmap.bmi.cx + 31) / 32) * bmap.bmi.cPlanes * 4;
            lScans     = 65535L / usLineSize;

            /* Read just one scanline to get the bitmap header */
            GpiQueryBitmapBits(hpsMemory, 0L, 1L, pchBits, &bmap.bmi);

            /* Write out the bitmap file header and color table */
            DosWrite(hFile, &bfh, sizeof(bfh) - sizeof(bfh.bmp), &usWritten);
            /* Note: Watch out for the size written. The RGB struct seems to be
             * throwing the compiler off so we calculate the size manually.
             */
            DosWrite(hFile, &bmap, sizeof(bmp) + sizeof(RGB) * 256, &usWritten);

            /* Read chunks of the bitmap and spit it out to the file */
            for (;;) {
                lScansRead = GpiQueryBitmapBits(hpsMemory, lStartScan, lScans, pchBits, &bmap.bmi);
                DosWrite(hFile, pchBits, (USHORT)(usLineSize * lScansRead), &usWritten);
                if (lScansRead != lScans)
                    break;
                lStartScan += lScansRead;
                }
            /* Clean up */
            DosFreeSeg(selBits);
            DosClose(hFile);
            }
        }
                                   // Clean up
    GpiDestroyPS(hpsMemory);
    DevCloseDC(hdcMemory);

    return hbm;
}

HBITMAP CopyBitmap(HBITMAP hbmSrc)
{
     BITMAPINFOHEADER bmp;
     HBITMAP          hbmDst;
     HDC              hdcSrc, hdcDst;
     HPS              hpsSrc, hpsDst;
     POINTL           aptl[3];
     SIZEL            sizl;

                                   // Create memory DC's and PS's
     hdcSrc = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULL);
     hdcDst = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULL);

     sizl.cx = sizl.cy = 0;
     hpsSrc = GpiCreatePS(hab, hdcSrc, &sizl, PU_PELS    | GPIF_DEFAULT |
                                              GPIT_MICRO | GPIA_ASSOC);

     hpsDst = GpiCreatePS(hab, hdcDst, &sizl, PU_PELS    | GPIF_DEFAULT |
                                              GPIT_MICRO | GPIA_ASSOC);

                                   // Create bitmap

     GpiQueryBitmapParameters(hbmSrc, &bmp);
     hbmDst = GpiCreateBitmap(hpsDst, &bmp, 0L, NULL, NULL);

                                   // Copy from source to destination

     if (hbmDst != NULL) {
          GpiSetBitmap(hpsSrc, hbmSrc);
          GpiSetBitmap(hpsDst, hbmDst);

          aptl[0].x = aptl[0].y = 0;
          aptl[1].x = bmp.cx;
          aptl[1].y = bmp.cy;
          aptl[2]   = aptl[0];

          GpiBitBlt(hpsDst, hpsSrc, 3L, aptl, ROP_SRCCOPY, BBO_IGNORE);
          }
                                   // Clean up
     GpiDestroyPS(hpsSrc);
     GpiDestroyPS(hpsDst);
     DevCloseDC(hdcSrc);
     DevCloseDC(hdcDst);

     return hbmDst;
}

VOID BitmapCreationError(HWND hwnd)
{
     WinMessageBox(HWND_DESKTOP, hwnd, "Cannot create bitmap.",
                   szClientClass, 0, MB_OK | MB_ICONEXCLAMATION);
}

/* Process WM_CHAR messages from system queue and see if they match a hotkey */
/* In this case Ctrl-Alt-X */
VOID CheckHotkey(MPARAM mp1, MPARAM mp2, HWND hwnd) {
    USHORT fsFlags = SHORT1FROMMP(mp1); /* Flags */
    BYTE   scan    = CHAR4FROMMP(mp1);  /* Scancode */

    mp2;

    if (!(fsFlags & KC_KEYUP))
        return;
    if ((scan == 0x2D) && (fsFlags & KC_CTRL) && (fsFlags & KC_ALT)) {
        DosBeep(500, 100);
        WinPostMsg(hwnd, WM_COMMAND, MPFROMSHORT(IDM_SNAP), 0);
        }
}

MRESULT EXPENTRY _loadds ClientWndProc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    static CHAR      *szMenuText[3] = {NULL,
                                       "C~apture",
                                       "A~bout..." };

    static MENUITEM  mi[3] = {MIT_END, MIS_SEPARATOR, 0, 0,         NULL, 0,
                              MIT_END, MIS_TEXT,      0, IDM_SNAP,  NULL, 0,
                              MIT_END, MIS_TEXT,      0, IDM_ABOUT, NULL, 0};

    static HBITMAP   hbm;
    static HWND      hwndMenu;
    static SHORT     sDisplay = IDM_ACTUAL;
    HBITMAP          hbmClip;
    HPS              hps;
    RECTL            rclClient;
    USHORT           usfInfo;
    HWND             hwndSysMenu, hwndSysSubMenu;
    MENUITEM         miSysMenu;
    SHORT            sItem, idSysMenu;


    switch (msg) {
        case WM_CREATE:
            hwndMenu = WinWindowFromID(
                WinQueryWindow(hwnd, QW_PARENT, FALSE),
                FID_MENU);

            /* Alter the system menu */
            hwndSysMenu = WinWindowFromID(
                WinQueryWindow(hwnd, QW_PARENT, FALSE),
                FID_SYSMENU);

            idSysMenu = SHORT1FROMMR(WinSendMsg (hwndSysMenu,
                                     MM_ITEMIDFROMPOSITION,
                                     NULL, NULL));

            WinSendMsg(hwndSysMenu, MM_QUERYITEM,
                MPFROM2SHORT(idSysMenu, FALSE),
                MPFROMP(&miSysMenu));

            hwndSysSubMenu = miSysMenu.hwndSubMenu;

            for (sItem = 0; sItem < 3; sItem++)
                WinSendMsg (hwndSysSubMenu, MM_INSERTITEM,
                    MPFROMP(mi + sItem),
                    MPFROMP(szMenuText [sItem]));

            /* Install our input hook */
            InitDLL(hab, hwnd);
            return FALSE;

        case WM_INITMENU:
            switch (SHORT1FROMMP(mp1)) {
                case IDM_EDIT:
                    WinSendMsg(hwndMenu, MM_SETITEMATTR,
                               MPFROM2SHORT(IDM_COPY, TRUE),
                               MPFROM2SHORT(MIA_DISABLED,
                                    hbm != NULL ? 0 : MIA_DISABLED));

                    WinSendMsg(hwndMenu, MM_SETITEMATTR,
                               MPFROM2SHORT(IDM_PASTE, TRUE),
                               MPFROM2SHORT(MIA_DISABLED,
                                   WinQueryClipbrdFmtInfo(hab, CF_BITMAP, &usfInfo)
                                       ? 0 : MIA_DISABLED));
                    return FALSE;
                }
            break;

        case WM_COMMAND:
            switch (COMMANDMSG(&msg)->cmd) {

                case IDM_ABOUT:
                    WinDlgBox(HWND_DESKTOP, hwnd,
                        NULL, 0, DLG_ABOUT, NULL);
                    return FALSE;

                case IDM_COPY:
                                        // Make copy of stored bitmap

                    hbmClip = CopyBitmap(hbm);

                                        // Set clipboard data to copy of bitmap

                    if (hbmClip != NULL) {
                        WinOpenClipbrd(hab);
                        WinEmptyClipbrd(hab);
                        WinSetClipbrdData(hab, (ULONG) hbmClip,
                                          CF_BITMAP, CFI_HANDLE);
                        WinCloseClipbrd(hab);
                        }
                    else
                        BitmapCreationError(hwnd);
                    return FALSE;

                case IDM_PASTE:
                                         // Get bitmap from clipboard

                    WinOpenClipbrd(hab);
                    hbmClip = (HBITMAP) WinQueryClipbrdData(hab, CF_BITMAP);

                    if (hbmClip != NULL) {
                        if (hbm != NULL)
                            GpiDeleteBitmap(hbm);

                                       // Make copy of it

                        hbm = CopyBitmap(hbmClip);

                        if (hbm == NULL)
                            BitmapCreationError(hwnd);
                        }
                    WinCloseClipbrd(hab);
                    WinInvalidateRect(hwnd, NULL, FALSE);
                    return FALSE;

                case IDM_CAPTURE:
                case IDM_SNAP:
                    if (hbm != NULL)
                        GpiDeleteBitmap(hbm);

                    hbm = CopyScreenToBitmap();

                    if (hbm == NULL)
                        BitmapCreationError(hwnd);

                    WinInvalidateRect(hwnd, NULL, FALSE);
                    return FALSE;

                case IDM_ACTUAL:
                case IDM_STRETCH:
                    WinSendMsg(hwndMenu, MM_SETITEMATTR,
                               MPFROM2SHORT(sDisplay, TRUE),
                               MPFROM2SHORT(MIA_CHECKED, 0));

                    sDisplay = COMMANDMSG(&msg)->cmd;

                    WinSendMsg(hwndMenu, MM_SETITEMATTR,
                               MPFROM2SHORT(sDisplay, TRUE),
                               MPFROM2SHORT(MIA_CHECKED, MIA_CHECKED));

                    WinInvalidateRect(hwnd, NULL, FALSE);
                    return FALSE;
                }

        case WM_PAINT:
            hps = WinBeginPaint(hwnd, NULL, NULL);
            GpiErase(hps);

            if (hbm != NULL) {
                WinQueryWindowRect(hwnd, &rclClient);

                WinDrawBitmap(hps, hbm, NULL, (PPOINTL) &rclClient,
                              CLR_NEUTRAL, CLR_BACKGROUND,
                              sDisplay == IDM_STRETCH ?
                                  DBM_STRETCH : DBM_NORMAL);
                }
            WinEndPaint(hps);
            return FALSE;

        case MYWM_CHAR:
            CheckHotkey(mp1, mp2, hwnd);
            return FALSE;

        case WM_DESTROY:
            /* Deinstall the input hook */
            CloseDLL();

            if (hbm != NULL)
                GpiDeleteBitmap(hbm);
            return FALSE;
        }
    return WinDefWindowProc(hwnd, msg, mp1, mp2);
}
