/*
**	$Filename: gfx/gfx.h
**	$Release:  0
**	$Revision: 0.1
**	$Date:     06-02-94
**
**	gfx functions for "Der Clou!"
**
**   (c) 1994 ...and avoid panic by, H. Gaberschek
**	    All Rights Reserved.
**
*/
/****************************************************************************
  Portions copyright (c) 2005 Vasco Alexandre da Silva Costa
  Portions copyright (c) 2005 Jens Granseuer

  Please read the license terms contained in the LICENSE and
  publiclicensecontract.doc files which should be contained with this
  distribution.
 ****************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "base/base.h"

#include "gfx/gfx.h"
#include "gfx/gfx.ph"

struct _GC {
    Rect clip;

    GfxDrawModeE mode;

    U8 foreground;              /* entspricht dem Farbregister */
    U8 background;              /* ebenfalls absolut und nicht relativ zum */
    U8 outline;                 /* Registerstart */

    U8 colorStart;
    U8 End;

    U16 cursorX;
    U16 cursorY;

    Font *font;
};

void gfxILBMToRAW(const U8 *src, U8 *dst, size_t size);

void gfxRealRefreshArea(U16 x, U16 y, U16 w, U16 h);

SDL_Surface *Screen;
SDL_Surface *windowSurface;

SDL_Window *sdlWindow;
SDL_Renderer *sdlRenderer;
SDL_Texture *sdlTexture;

ubyte DecrBuffer[GFX_DECR_BUFFER_SIZE] = {0};

#define GFX_WINDOW_PREF_FILE "window.cfg"

typedef struct WindowPrefs {
    unsigned scale;
    int width;
    int height;
    bool loaded;
} WindowPrefs;

static WindowPrefs windowPrefs = {0, 0, 0, false};
static char *windowPrefsPath = NULL;
static bool windowResizeLocked = false;
static int currentWindowWidth = SCREEN_WIDTH;
static int currentWindowHeight = SCREEN_HEIGHT;

typedef struct FrameClock {
    bool initialized;
    double targetFrameMs;
    double simulationTickMs;
    double deltaMs;
    Uint64 perfFreq;
    Uint64 lastCounter;
    Uint64 targetFrameCounts;
} FrameClock;

static FrameClock FrameTimer = {
    false,
    1000.0 / 60.0,
    40.0,
    40.0,
    0,
    0,
    0
};

static SDL_Palette *gfxGetScreenPalette(void)
{
    SDL_Palette *palette;

    if (!Screen)
        return NULL;

    palette = SDL_GetSurfacePalette(Screen);
    if (!palette)
        palette = SDL_CreateSurfacePalette(Screen);

    return palette;
}

static void gfxEnsurePerfFreq(void)
{
    if (FrameTimer.perfFreq)
        return;

    FrameTimer.perfFreq = SDL_GetPerformanceFrequency();
    if (FrameTimer.perfFreq == 0)
        FrameTimer.perfFreq = 1;
}

static void gfxInitFrameClock(void)
{
    if (FrameTimer.initialized)
        return;

    gfxEnsurePerfFreq();

    FrameTimer.deltaMs = FrameTimer.simulationTickMs;
    FrameTimer.lastCounter = SDL_GetPerformanceCounter();

    if (FrameTimer.targetFrameCounts == 0)
        gfxSetFrameRate(60);

    FrameTimer.initialized = true;
}

void gfxSetFrameRate(U32 fps)
{
    double frameMs;

    if (fps == 0)
        fps = 60;

    gfxEnsurePerfFreq();

    frameMs = 1000.0 / (double) fps;
    FrameTimer.targetFrameMs = frameMs;
    FrameTimer.targetFrameCounts =
        (Uint64) ((FrameTimer.targetFrameMs / 1000.0) *
                  (double) FrameTimer.perfFreq);

    if (FrameTimer.targetFrameCounts == 0)
        FrameTimer.targetFrameCounts = 1;
}

double gfxGetFrameDeltaMs(void)
{
    gfxInitFrameClock();
    return FrameTimer.deltaMs;
}

double gfxGetFrameDeltaTicks(void)
{
    gfxInitFrameClock();
    return FrameTimer.deltaMs / FrameTimer.simulationTickMs;
}

double gfxGetSimulationTickMs(void)
{
    return FrameTimer.simulationTickMs;
}

static unsigned gfxClampScale(unsigned scale)
{
    if (scale == 0)
        scale = 1;
    return scale;
}

static void gfxInitWindowPrefsPath(void)
{
    char *prefDir;
    size_t len;

    if (windowPrefsPath)
        return;

    prefDir = SDL_GetPrefPath("DerClou", "DerClou");
    if (!prefDir)
        return;

    len = strlen(prefDir) + strlen(GFX_WINDOW_PREF_FILE) + 1;
    windowPrefsPath = (char *)malloc(len);
    if (windowPrefsPath) {
        strcpy(windowPrefsPath, prefDir);
        strcat(windowPrefsPath, GFX_WINDOW_PREF_FILE);
    }

    SDL_free(prefDir);
}

static void gfxLoadWindowPrefs(void)
{
    FILE *fp;
    char line[128];
    unsigned value;

    if (!windowPrefsPath)
        return;

    fp = fopen(windowPrefsPath, "r");
    if (!fp)
        return;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "scale=%u", &value) == 1) {
            if (value >= 1)
                windowPrefs.scale = value;
        } else if (sscanf(line, "width=%u", &value) == 1) {
            windowPrefs.width = (int)value;
        } else if (sscanf(line, "height=%u", &value) == 1) {
            windowPrefs.height = (int)value;
        }
    }

    fclose(fp);

    if (windowPrefs.scale >= 1)
        windowPrefs.loaded = true;
}

static void gfxSaveWindowPrefs(void)
{
    FILE *fp;

    if (!windowPrefsPath || !windowPrefs.loaded)
        return;

    fp = fopen(windowPrefsPath, "w");
    if (!fp)
        return;

    fprintf(fp, "scale=%u\n", setup.Scale);
    fprintf(fp, "width=%d\n", windowPrefs.width);
    fprintf(fp, "height=%d\n", windowPrefs.height);

    fclose(fp);
}

static void gfxUpdateWindowMetrics(unsigned scale)
{
    unsigned clampedScale = gfxClampScale(scale);
    windowPrefs.scale = clampedScale;
    windowPrefs.width = (int)(SCREEN_WIDTH * clampedScale);
    windowPrefs.height = (int)(SCREEN_HEIGHT * clampedScale);
    currentWindowWidth = windowPrefs.width;
    currentWindowHeight = windowPrefs.height;
    windowPrefs.loaded = true;
}

static void gfxApplyWindowScale(unsigned scale, bool updateWindow, bool persist)
{
    scale = gfxClampScale(scale);

    if (setup.FullScreen)
        persist = false;

    setup.Scale = scale;
    gfxUpdateWindowMetrics(scale);

    if (updateWindow && sdlWindow && !setup.FullScreen) {
        windowResizeLocked = true;
        SDL_SetWindowSize(sdlWindow, windowPrefs.width, windowPrefs.height);
        windowResizeLocked = false;
    }

    if (persist)
        gfxSaveWindowPrefs();
}

static unsigned gfxChooseScaleFromResize(int width, int height, bool growing)
{
    unsigned metric;

    if (growing) {
        unsigned widthScaleCeil = (unsigned)((width + SCREEN_WIDTH - 1) / SCREEN_WIDTH);
        unsigned heightScaleCeil = (unsigned)((height + SCREEN_HEIGHT - 1) / SCREEN_HEIGHT);
        metric = (widthScaleCeil > heightScaleCeil) ? widthScaleCeil : heightScaleCeil;
    } else {
        unsigned widthScaleFloor = (unsigned)(width / SCREEN_WIDTH);
        unsigned heightScaleFloor = (unsigned)(height / SCREEN_HEIGHT);
        metric = (widthScaleFloor < heightScaleFloor) ? widthScaleFloor : heightScaleFloor;
    }

    if (metric < 1)
        metric = 1;

    return metric;
}

static void gfxHandleWindowResizeEvent(int width, int height)
{
    unsigned targetScale;
    bool growing;
    long long prevArea;
    long long newArea;

    if (width <= 0 || height <= 0)
        return;

    prevArea = (long long)currentWindowWidth * (long long)currentWindowHeight;
    newArea = (long long)width * (long long)height;
    growing = (newArea >= prevArea);
    targetScale = gfxChooseScaleFromResize(width, height, growing);

    if (targetScale == setup.Scale &&
        width == currentWindowWidth &&
        height == currentWindowHeight) {
        return;
    }

    gfxApplyWindowScale(targetScale, true, true);
}

void gfxHandleWindowEvent(const SDL_Event *ev)
{
    SDL_WindowID windowId;

    if (!ev || !sdlWindow)
        return;

    windowId = SDL_GetWindowID(sdlWindow);

    switch (ev->type) {
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        if (ev->window.windowID != windowId)
            break;
        if (setup.FullScreen || windowResizeLocked)
            break;
        gfxHandleWindowResizeEvent(ev->window.data1, ev->window.data2);
        break;

    default:
        break;
    }
}

/********************************************************************
 * inits & dons
 */

void gfxInit(void)
{
    Uint32 flags;
    int sw, sh;

    gfxInitWindowPrefsPath();

    if (!setup.ScaleOverride) {
        gfxLoadWindowPrefs();
        if (windowPrefs.loaded && windowPrefs.scale >= 1)
            setup.Scale = windowPrefs.scale;
    }

    gfxApplyWindowScale(setup.Scale, false, false);

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX,
                 "SDL video initialization failed: %s", SDL_GetError());
        return;
    }
    gfxInitFrameClock();

    flags = SDL_WINDOW_OPENGL;

    if (setup.FullScreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
        sw = SCREEN_WIDTH;
        sh = SCREEN_HEIGHT;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
        sw = SCREEN_WIDTH * setup.Scale;
        sh = SCREEN_HEIGHT * setup.Scale;
    }

    sdlWindow = SDL_CreateWindow("Der Clou!", sw, sh, flags);
    if (!sdlWindow) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX,
                 "SDL_CreateWindow failed: %s", SDL_GetError());
        return;
    }

    sdlRenderer = SDL_CreateRenderer(sdlWindow, NULL);
    if (!sdlRenderer) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX,
                 "SDL_CreateRenderer failed: %s", SDL_GetError());
        return;
    }

    SDL_ShowWindow(sdlWindow);

    if (!SDL_SetRenderLogicalPresentation(sdlRenderer,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_GFX,
                 "SDL_SetRenderLogicalPresentation failed: %s",
                 SDL_GetError());
    }

    if (setup.FullScreen) {
        if (!SDL_SetWindowFullscreenMode(sdlWindow, NULL)) {
            DebugMsg(ERR_WARNING, ERROR_MODULE_GFX,
                     "SDL_SetWindowFullscreenMode failed: %s",
                     SDL_GetError());
        }
    } else {
        SDL_SetWindowMinimumSize(sdlWindow, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    SDL_GetWindowSize(sdlWindow, &currentWindowWidth, &currentWindowHeight);

    if (sdlWindow) {
        int pixelW = 0;
        int pixelH = 0;

        SDL_GetWindowSizeInPixels(sdlWindow, &pixelW, &pixelH);
        if (pixelW > 0 && pixelH > 0) {
            SDL_WarpMouseInWindow(sdlWindow, pixelW / 2, pixelH / 2);
        }
    }

    if (!setup.FullScreen) {
        windowPrefs.width = currentWindowWidth;
        windowPrefs.height = currentWindowHeight;
        windowPrefs.loaded = true;
    }

    SDL_RaiseWindow(sdlWindow);
    SDL_SetWindowAlwaysOnTop(sdlWindow, true);
    SDL_SetWindowAlwaysOnTop(sdlWindow, false);
    SDL_SetWindowKeyboardGrab(sdlWindow, true);

    windowSurface = SDL_CreateSurface(SCREEN_WIDTH, SCREEN_HEIGHT,
                                      SDL_PIXELFORMAT_XRGB8888);
    if (!windowSurface) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX,
                 "SDL_CreateSurface window failed: %s", SDL_GetError());
        return;
    }

    sdlTexture = SDL_CreateTexture(sdlRenderer,
                                   SDL_PIXELFORMAT_XRGB8888,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!sdlTexture) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX,
                 "SDL_CreateTexture failed: %s", SDL_GetError());
        return;
    }
    {
#if SDL_VERSION_ATLEAST(3, 4, 0)
        const SDL_ScaleMode scaleMode = SDL_SCALEMODE_PIXELART;
#else
        const SDL_ScaleMode scaleMode = SDL_SCALEMODE_NEAREST;
#endif
        if (!SDL_SetTextureScaleMode(sdlTexture, scaleMode)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_GFX,
                     "SDL_SetTextureScaleMode failed: %s", SDL_GetError());
        }
    }

    Screen = SDL_CreateSurface(SCREEN_WIDTH, SCREEN_HEIGHT,
                               SDL_PIXELFORMAT_INDEX8);
    if (!Screen) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX,
                 "SDL_CreateSurface screen failed: %s", SDL_GetError());
        return;
    }
    if (!gfxGetScreenPalette()) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_GFX,
                 "Unable to allocate screen palette: %s", SDL_GetError());
    }

    gfxSetGC(NULL);

    /* diese RP müssen nur ein Bild maximaler Größe aufnehmen können */
    /* in anderen Modulen wird vorausgesetzt, daß alle RastPorts gleich */
    /* groß sind und auch gleich groß wie die StdBuffer sind */
    /* StdBuffer = 61 * 1024 = 62464, Mem: 62400 */

    /* Ausnahme (nachträglich) : der RefreshRP ist nur 320 * 140 Pixel groß!! */

    gfxInitMemRastPort(&StdRP0InMem, SCREEN_WIDTH, SCREEN_HEIGHT); /* CMAP muß auch Platz haben ! */
    gfxInitMemRastPort(&StdRP1InMem, SCREEN_WIDTH, SCREEN_HEIGHT);

    gfxInitMemRastPort(&AnimRPInMem, SCREEN_WIDTH, SCREEN_HEIGHT);
    gfxInitMemRastPort(&AddRPInMem, SCREEN_WIDTH, SCREEN_HEIGHT);
    gfxInitMemRastPort(&LSObjectRPInMem, SCREEN_WIDTH, SCREEN_HEIGHT);

    gfxInitMemRastPort(&LSFloorRPInMem, SCREEN_WIDTH, 32);

    /* der RefreshRP muß den ganzen Bildschirm aufnehmen können */
    gfxInitMemRastPort(&RefreshRPInMem, SCREEN_WIDTH, SCREEN_HEIGHT);

    gfxInitMemRastPort(&ScratchRP, SCREEN_WIDTH, SCREEN_HEIGHT);

    gfxInitMemRastPort(&BobRPInMem, SCREEN_WIDTH, SCREEN_HEIGHT);

    gfxInitMemRastPort(&LSRPInMem, LS_MAX_AREA_WIDTH, LS_MAX_AREA_HEIGHT);

    bubbleFont =
        gfxOpenFont(GFX_BUBBLE_FONT_NAME, 4, 8, 32, 255, SCREEN_WIDTH, 24);
    menuFont =
        gfxOpenFont(GFX_MENU_FONT_NAME, 5, 9, 32, 255, SCREEN_WIDTH, 36);

    gfxInitGC(&LowerGC,
        0, 0, 320, 140,
        0, 191,
        bubbleFont);
    gfxInitGC(&MenuGC,
        0, 140, 320, 60,
        191, 255,
        menuFont);

    gfxInitGC(&LSUpperGC,
        0, 0, 320, 128,
        0, 191,
        bubbleFont);
    gfxInitGC(&LSMenuGC,
        0, 128, 320, 72,
        191, 255,
        menuFont);

    gfxInitCollList();
    gfxInitPictList();

    gfxSetVideoMode(GFX_VIDEO_MCGA);	/* after rastports !! */
}

void gfxDone(void)
{
    gfxSaveWindowPrefs();

    if (PictureList) {
	RemoveList(PictureList);
	PictureList = NULL;
    }

    if (CollectionList) {
	RemoveList(CollectionList);
	CollectionList = NULL;
    }

    gfxCloseFont(bubbleFont);
    gfxCloseFont(menuFont);

    gfxDoneMemRastPort(&StdRP0InMem);
    gfxDoneMemRastPort(&StdRP1InMem);

    gfxDoneMemRastPort(&AnimRPInMem);
    gfxDoneMemRastPort(&AddRPInMem);
    gfxDoneMemRastPort(&LSObjectRPInMem);

    gfxDoneMemRastPort(&LSFloorRPInMem);

    gfxDoneMemRastPort(&RefreshRPInMem);

    gfxDoneMemRastPort(&ScratchRP);

    gfxDoneMemRastPort(&BobRPInMem);

    gfxDoneMemRastPort(&LSRPInMem);

    if (Screen) {
        SDL_DestroySurface(Screen);
        Screen = NULL;
    }
    if (windowSurface) {
        SDL_DestroySurface(windowSurface);
        windowSurface = NULL;
    }
    if (sdlTexture) {
        SDL_DestroyTexture(sdlTexture);
        sdlTexture = NULL;
    }
    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = NULL;
    }
    if (sdlWindow) {
        SDL_DestroyWindow(sdlWindow);
        sdlWindow = NULL;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    if (windowPrefsPath) {
        free(windowPrefsPath);
        windowPrefsPath = NULL;
    }
}

void gfxSetGC(GC *gc)
{
    GfxBase.gc = gc;
}

void gfxSetVideoMode(ubyte uch_NewMode)
{
    GfxBase.uch_VideoMode = uch_NewMode;

    switch (uch_NewMode) {
    case GFX_VIDEO_MCGA:
	l_gc = &LowerGC;
	u_gc = &LowerGC;
	m_gc = &MenuGC;

	gfxSetRGB(NULL, 0, 0, 16, 12);
	gfxSetRGB(NULL, 255, 63, 63, 63);	/* mouse */
	break;

    case GFX_VIDEO_NCH4:
	l_gc = &LowerGC;
	u_gc = &LSUpperGC;
	m_gc = &LSMenuGC;

	gfxLSInit();

	gfxSetRGB(NULL, 0, 0, 16, 12);
	break;

    default:
    case GFX_VIDEO_TEXT:
	return;
    }

    gfxClearArea(NULL);
}

void gfxCorrectUpperRPBitmap(void)
	/* should be called after each scrolling (and before displaying a bubble) */
{
/*
	NCH4UpperRP.p_BitMap = (void *) ((U32) GfxBoardBase + gfxNCH4GetCurrScrollOffset());
*/
}

/********************************************************************
 * lists
 */

static void gfxInitCollList(void)
{
    char pathname[DSK_PATH_MAX];
    LIST *tempList = CreateList();
    NODE *n;

    CollectionList = CreateList();

    dskBuildPathName(DISK_CHECK_FILE, TEXT_DIRECTORY, COLL_LIST_TXT, pathname);
    ReadList(tempList, 0, pathname);

    for (n = (NODE *) LIST_HEAD(tempList); NODE_SUCC(n);
	 n = (NODE *) NODE_SUCC(n)) {
	struct Collection *coll;

	coll =
	    CreateNode(CollectionList, sizeof(struct Collection),
		       txtGetKey(2, NODE_NAME(n)));

	coll->us_CollId = (uword) txtGetKeyAsULONG(1, NODE_NAME(n));

	coll->puch_Filename = NODE_NAME(coll);
	coll->prepared = NULL;

	coll->us_TotalWidth = (uword) txtGetKeyAsULONG(3, NODE_NAME(n));
	coll->us_TotalHeight = (uword) txtGetKeyAsULONG(4, NODE_NAME(n));

	coll->uch_ColorRangeStart = (uword) txtGetKeyAsULONG(5, NODE_NAME(n));
	coll->uch_ColorRangeEnd = (uword) txtGetKeyAsULONG(6, NODE_NAME(n));
    }

    RemoveList(tempList);
}

static void gfxInitPictList(void)
{
    char pathname[DSK_PATH_MAX];
    LIST *tempList = CreateList();
    NODE *n;

    PictureList = CreateList();

    dskBuildPathName(DISK_CHECK_FILE, TEXT_DIRECTORY, PICT_LIST_TXT, pathname);
    ReadList(tempList, 0, pathname);

    for (n = LIST_HEAD(tempList); NODE_SUCC(n); n = NODE_SUCC(n)) {
	struct Picture *pict;

	pict = CreateNode(PictureList, sizeof(*pict), NULL);

	pict->us_PictId = (uword) txtGetKeyAsULONG(1, NODE_NAME(n));
	pict->us_CollId = (uword) txtGetKeyAsULONG(2, NODE_NAME(n));

	pict->us_XOffset = (uword) txtGetKeyAsULONG(3, NODE_NAME(n));
	pict->us_YOffset = (uword) txtGetKeyAsULONG(4, NODE_NAME(n));

	pict->us_Width = (uword) txtGetKeyAsULONG(5, NODE_NAME(n));
	pict->us_Height = (uword) txtGetKeyAsULONG(6, NODE_NAME(n));

	pict->us_DestX = (uword) txtGetKeyAsULONG(7, NODE_NAME(n));
	pict->us_DestY = (uword) txtGetKeyAsULONG(8, NODE_NAME(n));
    }

    RemoveList(tempList);
}

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rectangle;

/*
 * Clips the rectangle B against rectangle A.
 */
static Rectangle Clip(Rectangle A, Rectangle B)
{
    int dist;
    Rectangle C = { 0, 0, -1, -1 };

    if (B.x >= A.x+A.w) {
        return C;
    }
    if (B.x+B.w <= A.x) {
        return C;
    }
    if (B.y >= A.y+A.h) {
        return C;
    }
    if (B.y+B.h <= A.y) {
        return C;
    }

    dist = A.x - B.x;
    if (dist > 0) {
        B.x += dist;
        B.w -= dist;
    }
    dist = (B.x+B.w) - (A.x+A.w);
    if (dist > 0) {
        B.w -= dist;
    }
    dist = A.y - B.y;
    if (dist > 0) {
        B.y += dist;
        B.h -= dist;
    }
    dist = (B.y+B.h) - (A.y+A.h);
    if (dist > 0) {
        B.h -= dist;
    }

    return B;
}

/********************************************************************
 * Rastports...
 */

static void gfxInitGC(GC *gc, U16 x, U16 y, U16 w, U16 h,
                      U8 colorStart, U8 End, Font *font)
{
    Rectangle dstR, dstR2;

    dstR.x = 0;
    dstR.y = 0;
    dstR.w = SCREEN_WIDTH;
    dstR.h = SCREEN_HEIGHT;

    dstR2.x = x;
    dstR2.y = y;
    dstR2.w = w;
    dstR2.h = h;
    dstR = Clip(dstR, dstR2);

    if (dstR.w <= 0 || dstR.h <= 0) {
        DebugMsg(ERR_ERROR, ERROR_MODULE_GFX, "gfxInitGC");
    }

    gc->clip.x      = dstR.x;
    gc->clip.y      = dstR.y;
    gc->clip.w      = dstR.w;
    gc->clip.h      = dstR.h;

    gc->mode        = GFX_JAM_1;

    gc->foreground  = colorStart+1;
    gc->background  = colorStart;
    gc->outline     = colorStart+1;

    gc->colorStart  = colorStart;
    gc->End         = End;

    gfxMoveCursor(gc, 0, 0);

    gc->font        = font;
}



/*******************************************************************
 * FONTS
 * gfxOpenFont
 * gfxCloseFont
 */

static Font *gfxOpenFont(char *fileName, U16 w, U16 h,
                         unsigned char first, unsigned char last,
                         U16 sw, U16 sh)
{
    Font *font;

    char path[DSK_PATH_MAX];
    U8 *lbm;

    SDL_Surface *bmp;
    U32 size;


    /* create font structure. */
    font = TCAllocMem(sizeof(*font), true);

    font->w     = w;
    font->h     = h;

    font->first = first;
    font->last  = last;


    dskBuildPathName(DISK_CHECK_FILE, PICTURE_DIRECTORY, fileName, path);
    lbm = dskLoad(path);


    bmp = SDL_CreateSurface(sw, sh, SDL_PIXELFORMAT_INDEX8);
    size = sw * sh;

    if (SDL_MUSTLOCK(bmp))
	SDL_LockSurface(bmp);

    gfxILBMToRAW(lbm, bmp->pixels, size);

    if (SDL_MUSTLOCK(bmp))
	SDL_UnlockSurface(bmp);

    free(lbm);

    SDL_SetSurfaceColorKey(bmp, true, 0);
    font->bmp = bmp;

    return font;
}

void gfxCloseFont(Font *font)
{
    if (font) {
        SDL_DestroySurface(font->bmp);

	TCFreeMem(font, sizeof(*font));
    }
}

/*******************************************************************
 * gfxMoveCursor (amiga function: Move)
 * gfxSetPens    (amiga defines:   SetAPen, SetBPen, SetOPen)
 * gfxRectFill   (amiga function: RectFill)
 * gfxTextLength (amiga function: TextLength)
 * gfxSetDrMd    (amiga define:   SetDrMd)
 * gfxDraw	     (amiga function: Draw)
 * gfxReadPixel  (amiga function: ReadPixel)
 */

void gfxDraw(GC *gc, U16 x, U16 y)
{
    /* this function doesn't perform clipping properly... but that's ok! */
    x += gc->clip.x;
    y += gc->clip.y;

    if (x < gc->clip.w && y < gc->clip.h) {
	U16 rx, ry, rx1, ry1, dx, dy, sx, sy, dw, i;
	SDL_Surface *dst;
	U8 color, *dp;

	rx = x;
	ry = y;

	rx1 = gc->cursorX;
	ry1 = gc->cursorY;

        if (rx < rx1) {
            sx = rx;
            dx = rx1 - rx + 1;
        } else {
            sx = rx1;
            dx = rx - rx1 + 1;
        }

        if (ry < ry1) {
            sy = ry;
            dy = ry1 - ry + 1;
        } else {
            sy = ry1;
            dy = ry - ry1 + 1;
        }

	color = gc->foreground;
        dst = Screen;

	if (SDL_MUSTLOCK(dst))
	    SDL_LockSurface(dst);

        dw = dst->w;

	dp = dst->pixels;
        dp += sy * dw + sx;

	if (rx == rx1) {
	    for (i=0; i<dy; i++) {
		*dp = color;
		dp += dw;
	    }
	} else if (ry == ry1) {
            memset(dp, color, dx);
	} else {
            /* the line is slanted. oops. */
        }

	if (SDL_MUSTLOCK(dst))
	    SDL_UnlockSurface(dst);

	gc->cursorX = x;
	gc->cursorY = y;

        if (rx == rx1) {
            gfxRefreshArea(sx, 1, sy, dy);
        } else if (ry == ry1) {
            gfxRefreshArea(sx, dx, sy, 1);
        } else {
            /* the line is slanted. oops. */
            DebugMsg(ERR_DEBUG, ERROR_MODULE_GFX, "gfxDraw");
        }
    }
}

void gfxMoveCursor(GC *gc, U16 x, U16 y)
{
    gc->cursorX = gc->clip.x + min(x, gc->clip.w-1);
    gc->cursorY = gc->clip.y + min(y, gc->clip.h-1);
}

void gfxSetPens(GC *gc, U8 foreground, U8 background, U8 outline)
{
    if (foreground != GFX_SAME_PEN)
	gc->foreground = foreground;

    if (background != GFX_SAME_PEN)
	gc->background = background;

    if (outline != GFX_SAME_PEN)
	gc->outline = outline;
}

void gfxRectFill(GC *gc, U16 sx, U16 sy, U16 ex, U16 ey)
	/* minimum size: 3 * 3 pixels ! */
{
    U16 tmp;
    SDL_Rect dst, dst2;

    if (sx > ex) {
	tmp = sx;
	sx = ex;
	ex = tmp;
    }

    if (sy > ey) {
	tmp = sy;
	sy = ey;
	ey = tmp;
    }

    dst.x = gc->clip.x + sx;
    dst.y = gc->clip.y + sy;
    dst.w = ex - sx + 1;
    dst.h = ey - sy + 1;

    SDL_FillSurfaceRect(Screen, &dst, gc->outline);

    dst2 = dst;
    dst2.x++;
    dst2.y++;
    dst2.w -= 2;
    dst2.h -= 2;

    SDL_FillSurfaceRect(Screen, &dst2, gc->foreground);

    gfxRefreshArea(dst.x, dst.y, dst.w, dst.h);
}

void gfxSetDrMd(GC *gc, GfxDrawModeE mode)
{
    gc->mode = mode;
}

void gfxSetFont(GC *gc, Font *font)
{
    gc->font = font;
}

/* berechnet die L�nge eines Textes in Pixel */
U16 gfxTextWidth(GC *gc, const char *txt, size_t len)
{
    size_t w;

    w = len * gc->font->w;

    if (w > UINT16_MAX) {
        return 0;
    } else {
        return w;
    }
}

/*******************************************************************
 * access & calc functions
 */

struct Collection *gfxGetCollection(uword us_CollId)
{
    return GetNthNode(CollectionList, (U32) (us_CollId - 1));
}

struct Picture *gfxGetPicture(uword us_PictId)
{
    return GetNthNode(PictureList, (U32) (us_PictId - 1));
}

/* the collection must have been prepared (PrepareColl) beforehand */
void gfxGetPalette(U16 collId, U8 *palette)
{
    memcpy(palette, ScratchRP.palette, GFX_PALETTE_SIZE);
}

static S32 gfxGetRealDestY(GC *gc, S32 destY)
{
    destY -= gc->clip.y;

    return destY;
}

/*******************************************************************
 * collections...
 */

/* nach dieser Funktion befindet sich im ScratchRP die entpackte Collection */
void gfxPrepareColl(U16 collId)
{
    struct Collection *coll;

    if (!(coll = gfxGetCollection(collId))) {
        DebugMsg(ERR_DEBUG, ERROR_MODULE_GFX, "gfxPrepareColl");
	return;
    }

    if (coll->prepared) {
        gfxScratchFromMem(coll->prepared);
    } else {
	char pathname[DSK_PATH_MAX];

	/* Dateiname erstellen */
	dskBuildPathName(DISK_CHECK_FILE, PICTURE_DIRECTORY, coll->puch_Filename, pathname);

	gfxLoadILBM(pathname);

	/*
         * coll->prepared wird nicht mit dem ScratchRP initialisert, da
	 * es sonst zu Inkonsistenzen kommen könnte. Collections im Scratch
	 * werden als nicht vorbereitet betrachtet, da der ScratchRP ständig
	 * durch andere Bilder überschrieben wird
         */
    }
}

void gfxLoadILBM(char *fileName)
{
    U8 *lbm;

    /* Collection laden */
    lbm = dskLoad(fileName);

    gfxSetCMAP(lbm);
    gfxILBMToRAW(lbm, ScratchRP.pixels, SCREEN_SIZE);
    free(lbm);
}

void gfxUnPrepareColl(U16 collId)
{
    /* dummy function */
}

void gfxCollFromMem(U16 collId)
{
    struct Collection *coll;
    MemRastPort *rp;

    if ((coll = gfxGetCollection(collId)) && (rp = coll->prepared)) {
	gfxScratchFromMem(rp);
    }
}

void gfxCollToMem(U16 collId, MemRastPort *rp)
{
    struct Collection *coll;

    /*
     * wenn sich in diesem MemRastPort ein anderes Bild befindet so wird dieses
     * nun aus dem MemRastPort "entfernt"
     */
    if (rp->collId != GFX_NO_COLL_IN_MEM && collId != rp->collId) {
	struct Collection *oldColl;

        if ((oldColl = gfxGetCollection(rp->collId))) {
	    oldColl->prepared = NULL;
        }
    }

    gfxPrepareColl(collId);
    gfxScratchToMem(rp);

    /* enter the MemRastPort in the new collection */
    if ((coll = gfxGetCollection(collId))) {
        coll->prepared = rp;
    }
    rp->collId = collId;
}

/*******************************************************************
 * print functions
 */

void gfxSetRect(uword us_X, uword us_Width)
{
    GlobalPrintRect.us_X = us_X;
    GlobalPrintRect.us_Width = us_Width;
}

static void
ScreenBlitChar(GC *gc, SDL_Surface *src, Rect *src_rect,
               SDL_Surface *dst, Rect *dst_rect, U8 color)
{
    Rectangle srcR, dstR, dstR2, areaR;

    U8 *dp, *sp;
    U16 h;

    /* clip. */
    srcR.x = src_rect->x;
    srcR.y = src_rect->y;
    srcR.w = src_rect->w;
    srcR.h = src_rect->h;

    dstR.x = 0;
    dstR.y = 0;
    dstR.w = dst->w;
    dstR.h = dst->h;

    dstR2.x = dst_rect->x + gc->clip.x;
    dstR2.y = dst_rect->y + gc->clip.y;
    dstR2.w = gc->clip.w;
    dstR2.h = gc->clip.h;
    dstR = Clip(dstR, dstR2);

    if (dstR.w <= 0 || dstR.h <= 0) {
        return;
    }

    /* blit. */
    areaR.x = dstR.x;
    areaR.y = dstR.y;
    areaR.w = min(dstR.w, srcR.w);
    areaR.h = min(dstR.h, srcR.h);

    dp = dst->pixels;
    sp = src->pixels;

    dp += dstR.y * dst->w + dstR.x;
    sp += srcR.y * src->w + srcR.x;

    h = areaR.h;

    while (h--) {
        U16 x;
        for (x=0; x<areaR.w; x++) {
            if (sp[x] != 0) {
                dp[x] = color;
            }
        }
        dp += dst->pitch;
        sp += src->pitch;
    }
}

void gfxPrintExact(GC *gc, const char *txt, U16 x, U16 y)
{
    const Font *font = gc->font;
    const U16 w = font->w,
              h = font->h,
              base = font->first;
    const U16 chars_per_line = SCREEN_WIDTH/w;
    const U8 fg = gc->foreground;
    size_t len = strlen(txt), t;

    SDL_Rect area;
    Rect srcR, dstR;
    SDL_Surface *src, *dst;

    area.x = gc->clip.x;
    area.y = gc->clip.y;
    area.w = min(gc->clip.w, len * w);
    area.h = min(gc->clip.h, h);

    area.x += x;
    area.y += y;

    if (gc->mode == GFX_JAM_2)
	SDL_FillSurfaceRect(Screen, &area, gc->background);

    srcR.w = w;
    srcR.h = h;

    dstR.x = x;
    dstR.y = y;

    src = font->bmp;
    dst = Screen;

    if (SDL_MUSTLOCK(dst))
        SDL_LockSurface(dst);
    if (SDL_MUSTLOCK(src))
        SDL_LockSurface(src);

    for (t=0; t<len; t++) {
        U16 ch;

	ch  = (U8)txt[t];
        ch -= base;

	srcR.y = (ch / chars_per_line) * h;
	srcR.x = (ch % chars_per_line) * w;

        ScreenBlitChar(gc, src, &srcR, dst, &dstR, fg);

        dstR.x += w;
    }

    if (SDL_MUSTLOCK(src))
        SDL_UnlockSurface(src);
    if (SDL_MUSTLOCK(dst))
        SDL_UnlockSurface(dst);

    gfxRefreshArea(area.x, area.y, area.w, area.h);
}

void gfxPrint(GC *gc, const char *txt, U16 y, U32 mode)
{
    U16 x = GlobalPrintRect.us_X;
    U16 w = gfxTextWidth(gc, txt, strlen(txt));

    if (mode & GFX_PRINT_RIGHT)
	x += GlobalPrintRect.us_Width - w;

    if (mode & GFX_PRINT_CENTER) {
	if (w <= GlobalPrintRect.us_Width)
	    x += (GlobalPrintRect.us_Width - w) / 2;
    }

    if (mode & GFX_PRINT_SHADOW) {
	U8 tmp;

        tmp = gc->foreground;
	gc->foreground = gc->background;

	gfxPrintExact(gc, txt, x+1, y+1);

	gc->foreground = tmp;
    }

    gfxPrintExact(gc, txt, x, y);
}

/*******************************************************************
 * refresh...
 */

/* kopiert aktuelles Bild in den RefreshRP */
void gfxPrepareRefresh(void)
{
    if (SDL_MUSTLOCK(Screen))
	SDL_LockSurface(Screen);

    memcpy(RefreshRPInMem.pixels, Screen->pixels, SCREEN_SIZE);

    if (SDL_MUSTLOCK(Screen))
	SDL_UnlockSurface(Screen);
}

/* kopiert aktuellen Inhalt des RefreshRP in den Bildschirmspeicher */
void gfxRefresh(void)
{
    if (SDL_MUSTLOCK(Screen))
	SDL_LockSurface(Screen);

    memcpy(Screen->pixels, RefreshRPInMem.pixels, SCREEN_SIZE);

    if (SDL_MUSTLOCK(Screen))
	SDL_UnlockSurface(Screen);

    gfxRefreshArea(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

/*******************************************************************
 * presentation
 */

void gfxRAWBlit(U8 * sp, U8 * dp, const int x1, const int y1, const int x2,
		const int y2, const int w, const int h, const int sw,
		const int dw)
{
    const int ds = sw - w, dd = dw - w;
    int x, y;

    sp += (y1 * sw) + x1;
    dp += (y2 * dw) + x2;

    for (y = 0; y < h; y++) {
	for (x = 0; x < w; x++) {
	    *dp++ = *sp++;
	}
	sp += ds;
	dp += dd;
    }
}

static GC *gfxGetGC(S32 l_DestY)
{
    GC *gc = NULL;

    switch (GfxBase.uch_VideoMode) {
    case GFX_VIDEO_NCH4:
        gc = u_gc;

        if (l_DestY >= 60) {
            gc = l_gc;

            if (l_DestY >= gc->clip.y + gc->clip.h)
                gc = m_gc;
        }
	break;

    default:
    case GFX_VIDEO_MCGA:
	gc = u_gc;

	if (l_DestY >= gc->clip.y + gc->clip.h) {
	    gc = l_gc;

	    if (l_DestY >= gc->clip.y + gc->clip.h)
		gc = m_gc;
	}
	break;
    }

    return gc;
}

void gfxWaitTOF(void)
{
    Uint64 now;
    Uint64 elapsedCounts;
    double elapsedMs;

    gfxInitFrameClock();

    now = SDL_GetPerformanceCounter();
    elapsedCounts = now - FrameTimer.lastCounter;

    if (elapsedCounts < FrameTimer.targetFrameCounts) {
        double remainingMs =
            (double) (FrameTimer.targetFrameCounts - elapsedCounts) * 1000.0 /
            (double) FrameTimer.perfFreq;
        Uint32 delayMs = 0;

        if (remainingMs > 1.0)
            delayMs = (Uint32) remainingMs;
        else if (remainingMs > 0.0)
            delayMs = 1;

        if (delayMs)
            SDL_Delay(delayMs);

        now = SDL_GetPerformanceCounter();
        elapsedCounts = now - FrameTimer.lastCounter;
    }

    elapsedMs = (double) elapsedCounts * 1000.0 /
        (double) FrameTimer.perfFreq;

    if (elapsedMs > FrameTimer.simulationTickMs * 8.0)
        elapsedMs = FrameTimer.simulationTickMs * 8.0;

    FrameTimer.deltaMs = elapsedMs;
    FrameTimer.lastCounter = now;
}

void gfxWaitTOR(void)
{
    SDL_Delay(20);
}

void gfxWaitTOS(void)
{
    SDL_Delay(250);
}

void gfxClearArea(GC *gc)
{
    SDL_Rect area;

    if (gc) {
        area.x = gc->clip.x;
        area.y = gc->clip.y;
        area.w = gc->clip.w;
        area.h = gc->clip.h;
    } else {
        area.x = 0;
        area.y = 0;
        area.w = SCREEN_WIDTH;
        area.h = SCREEN_HEIGHT;
    }
    SDL_FillSurfaceRect(Screen, &area, 0);

    gfxRefreshArea(area.x, area.y, area.w, area.h);
}

void gfxSetRGB(GC *gc, U8 color, U8 r, U8 g, U8 b)
{
    SDL_Color colors[1];

    colors[0].r = r;
    colors[0].g = g;
    colors[0].b = b;

    SDL_Palette *pal = gfxGetScreenPalette();

    if (!pal)
        return;

    SDL_SetPaletteColors(pal, colors, color, 1);

    gfxRealRefreshArea(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void gfxSetColorRange(ubyte uch_ColorStart, ubyte uch_End)
{
    GlobalColorRange.uch_Start = uch_ColorStart;
    GlobalColorRange.uch_End = uch_End;
}

void gfxGetPaletteFromReg(U8 *palette)
{
    SDL_Palette *pal = gfxGetScreenPalette();

    if (pal) {
	SDL_Color *colors;
	int ncolors, i;

	ncolors = pal->ncolors;
	colors = pal->colors;

        for (i=0; i<ncolors; i++) {
            *palette++ = colors->r;
            *palette++ = colors->g;
            *palette++ = colors->b;
            colors++;
        }
    }
}

void gfxChangeColors(GC *gc, U32 delay, U32 mode, U8 *palette)
	/* l_Delay is min 1 */
{
    U16 t, st, en;
    SDL_Color colors[256];
    U8 cols[GFX_PALETTE_SIZE];
    Rect area;
    S32 time, fakt, s;
    SDL_Palette *pal = gfxGetScreenPalette();

    if (!pal)
        return;

    if (gc) {
	st = gc->colorStart;
	en = gc->End;

        area = gc->clip;
    } else {
	st = GlobalColorRange.uch_Start;
	en = GlobalColorRange.uch_End;

        area.x = 0;
        area.y = 0;
        area.w = SCREEN_WIDTH;
        area.h = SCREEN_HEIGHT;
    }

    delay = max(delay, 1);	/* delay must not be zero! */

    time = delay;

    switch (mode) {
    case GFX_FADE_OUT:
	
        gfxGetPaletteFromReg(cols);

        fakt=128/time;

        for (s=time; s>=0; s--) {
            gfxRealRefreshArea(area.x, area.y, area.w, area.h);
            gfxWaitTOR();

            for (t=st; t<=en; t++) {
                colors[t].r = ((S32)cols[t*3] * (fakt * s)) / 128;
                colors[t].g = 16 + (((S32)cols[t*3+1] - 16) * (fakt * s)) / 128;
                colors[t].b = 12 + (((S32)cols[t*3+2] - 12) * (fakt * s)) / 128;
            }
            SDL_SetPaletteColors(pal, &colors[st], st, en - st + 1);
        }

	for (t=st; t<=en; t++) {
	    colors[t].r = 0;
	    colors[t].g = 0;
	    colors[t].b = 0;
        }
    SDL_SetPaletteColors(pal, &colors[st], st, en - st + 1);
	break;

    case GFX_BLEND_UP:
        for (t=0; t<GFX_PALETTE_SIZE; t++) {
            cols[t] = palette[t];
        }

        fakt=128/time;

	for (t=st; t<=en; t++) {
	    colors[t].r = 0;
	    colors[t].g = 0;
	    colors[t].b = 0;
        }
	SDL_SetPaletteColors(pal, &colors[st], st, en - st + 1);

        for (s=0; s<=time; s++) {
            gfxRealRefreshArea(area.x, area.y, area.w, area.h);
            gfxWaitTOR();

            for (t=st; t<=en; t++) {
                colors[t].r = ((S32)cols[t*3] * (fakt * s)) / 128;
                colors[t].g = 16 + (((S32)cols[t*3+1] - 16) * (fakt * s)) / 128;
                colors[t].b = 12 + (((S32)cols[t*3+2] - 12) * (fakt * s)) / 128;
            }
            SDL_SetPaletteColors(pal, &colors[st], st, en - st + 1);
        }

        gfxRealRefreshArea(area.x, area.y, area.w, area.h);
        gfxWaitTOR();

	for (t=st; t<=en; t++) {
	    colors[t].r = palette[t*3+0];
	    colors[t].g = palette[t*3+1];
	    colors[t].b = palette[t*3+2];
        }
    SDL_SetPaletteColors(pal, &colors[st], st, en - st + 1);
	break;
    }

    gfxRealRefreshArea(area.x, area.y, area.w, area.h);
}


void gfxShow(uword us_PictId, U32 ul_Mode, S32 l_Delay, S32 l_XPos, S32 l_YPos)
{
    struct Picture *pict;
    struct Collection *coll;
    GC *gc;
    U16 destX;
    U16 destY;

    if (!(pict = gfxGetPicture(us_PictId)))
	return;
    if (!(coll = gfxGetCollection(pict->us_CollId)))
	return;

    destX = pict->us_DestX;
    destY = pict->us_DestY;

    if (l_XPos != -1)
	destX = l_XPos;

    if (l_YPos != -1)
	destY = l_YPos;

    if (GfxBase.gc)
	gc = GfxBase.gc;
    else
	gc = gfxGetGC(destY);

    destY = gfxGetRealDestY(gc, destY);

    gfxPrepareColl(pict->us_CollId);

    if (!(ul_Mode & GFX_NO_REFRESH))
	gfxPrepareRefresh();

    if (ul_Mode & GFX_CLEAR_FIRST)
	gfxClearArea(gc);

    if (ul_Mode & GFX_FADE_OUT) {
	gfxSetColorRange(coll->uch_ColorRangeStart, coll->uch_ColorRangeEnd);
	gfxChangeColors(NULL, l_Delay, GFX_FADE_OUT, NULL);
    }

    if (!l_Delay && (ul_Mode & GFX_BLEND_UP)) {
	gfxSetColorRange(coll->uch_ColorRangeStart,
	                 coll->uch_ColorRangeEnd);
	gfxChangeColors(NULL, l_Delay, GFX_BLEND_UP, ScratchRP.palette);
	
    }

    gfxScreenFreeze();
    if (ul_Mode & GFX_OVERLAY)
        gfxBlit(gc, &ScratchRP, pict->us_XOffset, pict->us_YOffset,
                destX, destY, pict->us_Width, pict->us_Height, true);
    if (ul_Mode & GFX_ONE_STEP)
        gfxBlit(gc, &ScratchRP, pict->us_XOffset, pict->us_YOffset,
                destX, destY, pict->us_Width, pict->us_Height, false);

    if (l_Delay && (ul_Mode & GFX_BLEND_UP)) {
	gfxSetColorRange(coll->uch_ColorRangeStart,
	                 coll->uch_ColorRangeEnd);
	gfxChangeColors(NULL, l_Delay, GFX_BLEND_UP, ScratchRP.palette);
    }
    gfxScreenThaw(gc, destX, destY, pict->us_Width, pict->us_Height);

    gfxSetGC(NULL);
}

/*******************************************************************
 * misc
 */


S32 gfxGetILBMSize(struct Collection *coll)
{
    uword w = coll->us_TotalWidth;
    uword h = coll->us_TotalHeight;
    S32 size;

    w = ((w + 15) & 0xfff0);	/* round up to a word */

    size = w * h;

    return size;
}

static void gfxSetCMAP(const Uint8 *src)
{
    /* look for CMAP chunk */
    while (memcmp(src, "CMAP", 4) != 0)
	src++;

    src += 4;			/* skip CMAP chunk */
    src += 4;			/* skip size of CMAP chunk */

    memcpy(ScratchRP.palette, src, GFX_PALETTE_SIZE);
}

static void MakeMCGA(U16 b, U8 *pic, uword PlSt, S16 c)
{
    U16 bit;
    S16 i;

    bit = 0x80;
    for (i=0; i<8; i++) {
        if ((b & bit) && (c > i)) {
          pic[i] |= PlSt;
        }
        bit >>= 1;
    }
}

static U16 MemRead_U16BE(const U8 *p)
{
    return ((U16) p[1] | ((U16) p[0] << 8));
}

void gfxILBMToRAW(const U8 *src, U8 *dst, size_t size)
{
    U16 bpp = 0, w = 0, h = 0;
    const U8 *sp;
    U8 *pic, *pic1;
    int s, t, b, x;
    uword a, y;
    uword flag;

    sp = src;
    pic = dst;

    sp += 8;

    if (memcmp(sp, "ILBM", 4) != 0) {
	return;
    }

    sp += 12;

    w = MemRead_U16BE(sp);
    sp += 2;
    h = MemRead_U16BE(sp);
    sp += 6;
    bpp = *sp;
    sp += 2;
    flag = *sp;

    h = min(h, 192);

    while (memcmp(sp, "BODY", 4) != 0) {
	sp++;
    }

    sp += 8;

    memset(pic, 0, size);

    if (flag) {
	pic1 = pic;
	for (t=0; t<h; t++) {
	    for (s=0; s<bpp; s++) {
		pic = pic1;	/* Anfang der aktuellen Zeile */
		b = ((w + 15) & 0xfff0);
		do {
		    a = *sp;	/* Kommando (wiederholen oder übernehmen */
		    sp++;	/* nächstes Zeichen */
		    if (a > 128) {	/* Zeichen wiederholen */

			a = 257 - a;

			y = *sp;
			sp++;
			for (x = 1; x <= a; x++) {
			    MakeMCGA(y, pic, (1 << s), b);
			    pic += 8;
			    b -= 8;
			}
		    } else {	/* Zeichen übernehmen */

			for (x = 0; x <= a; x++) {
			    y = *sp;
			    sp++;
			    MakeMCGA(y, pic, (1 << s), b);
			    pic += 8;
			    b -= 8;
			}
		    }
		}
		while (b > 0);
	    }
	    pic1 += SCREEN_WIDTH;
	}
    } else {
	pic1 = pic;
	for (t=0; t<h; t++) {
	    for (s=0; s<bpp; s++) {
		pic = pic1;	/* Anfang der aktuellen Zeile */
		b = ((w + 15) & 0xfff0);
		do {
		    y = *sp;
		    sp++;
		    MakeMCGA(y, pic, (1 << s), b);
		    pic += 8;
		    b -= 8;
		}
		while (b > 0);
	    }
	    pic1 += SCREEN_WIDTH;
	}
    }
}

size_t XMSOffset = 0;
U8 *XMSHandle;


static const char *names[5] = {
    "an1_1.anm",
    "an2_4.anm",
    "an3_11.anm",
    "an4_11.anm",
    "an5_11.anm"
};

int frames[5] = { 9, 67, 196, 500, 180 };
int rate[5] = { 60, 17, 7, 7, 7 };


#define MaxAnm 6

int sync[MaxAnm * 2] = {
    0, 1,
    0, 5,
    1, 2,
    1, 27,
    1, 39,
    1, 53
};

static const char *fname[MaxAnm] = {
    "church.voc",
    "church.voc",
    "church.voc",
    "church.voc",
    "church.voc",
    "church.voc"
};

#define MAX_TRACKS 24
int CDFrames[MAX_TRACKS * 6] = {
    /* Anim#, Frame#, (0..full frame; 1..partial frame), Frame Start, Frame End */

    2, 100, 0, 3, 0, 0,
    2, 143, 0, 4, 0, 0,
    3, 5, 0, 23, 0, 0,
    3, 74, 0, 8, 0, 0,
    3, 90, 0, 24, 0, 0,
    3, 118, 0, 9, 0, 0,
    3, 137, 0, 14, 0, 0,
    3, 177, 0, 19, 0, 0,
    3, 205, 0, 10, 0, 0,
    3, 222, 0, 15, 0, 0,
    3, 242, 0, 20, 0, 0,
    3, 260, 0, 16, 0, 0,
    3, 293, 0, 21, 0, 0,
    3, 327, 0, 11, 0, 0,
    3, 344, 0, 17, 0, 0,
    3, 375, 0, 5, 0, 0,
    3, 427, 1, 12, 0, 2 * 75,
    3, 445, 1, 12, 3 * 75, 5 * 75,
    3, 467, 1, 12, 8 * 75, 11 * 75,
    4, 5, 0, 18, 0, 0,
    4, 44, 0, 22, 0, 0,
    4, 66, 0, 6, 0, 0,
    4, 115, 0, 13, 0, 0,
    4, 153, 0, 7, 0, 0
};

static U32 Amg2Pc(U32 s)
{
    return ((s & 255) << 24) +
	(((s >> 8) & 255) << 16) +
	(((s >> 16) & 255) << 8) + (((s >> 24) & 255));
}


static void orbyte(U8 *ptr, U8 data, U8 dmask)
{
  U8 dmaskoff = ~dmask; 

  if (0x80 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x40 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x20 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x10 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x08 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x04 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x02 & data) *ptr++ |= dmask; else *ptr++ &= dmaskoff; 
  if (0x01 & data) *ptr   |= dmask; else *ptr   &= dmaskoff; 
}


static void ProcessAnimation(U8 *dp, U8 *sp)
{
    U32 size, rsize, *lp;
    U8 *me, *me1, *me2, *st;
    U32 offs[8];
    U8 plane, planeMask;
    int i, j, col;

    /* skip header */
    XMSOffset += 4;
    /* get size */
    memcpy(&size, XMSHandle+XMSOffset, 4);
    XMSOffset += 4;

    rsize = Amg2Pc(size);

    me = dp;

    sp = XMSHandle+XMSOffset;
    XMSOffset += rsize;

    while (memcmp(sp, "DLTA", 4) != 0) {
	sp++;
    }

    sp += 8;
    lp = (U32 *)sp;
    st = sp;			/* start of DLTA block */

    for (i=0; i<8; i++)
	offs[i] = Amg2Pc(lp[i]);

    me2 = me;

    for (plane = 0, planeMask = 1; plane < 8; plane++, planeMask <<= 1) {
	if (offs[plane] != 0) {
	    sp = st + offs[plane];
	    me1 = me2;

	    for (col=0; col<40; col++) {
                U8 op_cnt;

		me = me1;
		op_cnt = *sp++;

		for (i=0; i<op_cnt; i++) {
                    U8 op;

		    op = *sp++;

		    if (op == 0) {
                        /* same op */
                        U8 cnt;
                        U8 val;

			cnt = *sp++;
			val = *sp++;

			for (j=0; j<cnt; j++) {
                            orbyte(me, val, planeMask);
			    me += SCREEN_WIDTH;
			}
		    } else {
			if ((op & 0x80)) {
                            /* uniq op */
                            U8 cnt;

			    cnt = op & 0x7f;
			    for (j=0; j<cnt; j++) {
                                U8 val;
                            
                                val = *sp++;
                                orbyte(me, val, planeMask);
				me += SCREEN_WIDTH;
			    }
                        } else {
                            /* skip op */
			    me += (SCREEN_WIDTH * op);
			}
		    }

		}
		me1 += 8;	/* next column */
	    }
	}
    }
}

void ShowIntro(void)
{
    U8 *cp;
    char head[4];
    int t, s, anims;
    size_t size, rsize;
    U8 colorTABLE[GFX_PALETTE_SIZE];
    bool endi = false;
    GC ScreenGC;
    MemRastPort A, B;

    XMSHandle = malloc(818*1024);

    /******************************** Init Gfx ********************************/
    gfxInitMemRastPort(&A, SCREEN_WIDTH, SCREEN_HEIGHT);
    gfxInitMemRastPort(&B, SCREEN_WIDTH, SCREEN_HEIGHT);

    gfxInitGC(&ScreenGC,
        0, 0, 320, 200,
        0, 255,
        NULL);
    gfxSetColorRange(0, 255);

    memset(colorTABLE, 0, sizeof(colorTABLE));

    for (anims=0; anims<5; anims++) {
        FILE *fp;
        char pathName[DSK_PATH_MAX];
        bool showA;

        if (setup.CDAudioFromCD) {
            CDROM_StopAudioTrack();
        }
        if (setup.CDAudioFromWav) {
            sndStopSpeechSample();
        }

        if (!dskBuildPathName(DISK_CHECK_FILE, "intropix", names[anims], pathName)) {
            continue;
        }

        fp = dskOpen(pathName, "rb");

	if (fp) {
            XMSOffset = 0;

	    fread(&head[0], 1, 4, fp);
	    fread(&size, 1, 4, fp);
	    rsize = Amg2Pc(size);

            dskRead(fp, XMSHandle, rsize);
	    dskClose(fp);

            /* skip header */
	    XMSOffset += 4;
            
            cp = XMSHandle+XMSOffset;
            
            /* skip header */
	    XMSOffset += 4;
            /* get size */
            memcpy(&size, XMSHandle+XMSOffset, 4);
	    XMSOffset += 4;

	    rsize = Amg2Pc(size);

	    XMSOffset += rsize;

            gfxChangeColors(NULL, 20, GFX_BLEND_UP, colorTABLE);
            gfxClearArea(NULL);

            /* copy from file to A & B */
            gfxSetCMAP(cp);
            gfxILBMToRAW(cp, ScratchRP.pixels, SCREEN_SIZE);
            gfxScratchToMem(&A);
            gfxScratchToMem(&B);

	    for (t=0, showA=true; t<frames[anims]; t++, showA=!showA) {
                SDL_Event ev;

                while (SDL_PollEvent(&ev)) {
	            gfxHandleWindowEvent(&ev);
                    switch (ev.type) {
                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
                    case SDL_EVENT_QUIT:
                        endi = true;
                        goto endit;

                    default:
                        break;
                    }
                }

                gfxScreenFreeze();

                if (showA) {
                    gfxBlit(&ScreenGC, &A, 0, 0, 0, 0,
                        SCREEN_WIDTH, SCREEN_HEIGHT, false);
                } else {
                    gfxBlit(&ScreenGC, &B, 0, 0, 0, 0,
                        SCREEN_WIDTH, SCREEN_HEIGHT, false);
                }

                if (t == 0) {
                    gfxChangeColors(NULL, 20, GFX_BLEND_UP, ScratchRP.palette);
                }

                gfxScreenThaw(&ScreenGC, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);


            {
            /* Bias animation pacing closer to modern 60 Hz timing without dropping the legacy flavour */
            const double legacyTickMs = FrameTimer.simulationTickMs;
            const double modernFrameMs = FrameTimer.targetFrameMs;
            const double legacyWeight = 0.25;
            const double modernWeight = 1.0 - legacyWeight;
            const double blendedFrameMs =
                (legacyTickMs * legacyWeight) + (modernFrameMs * modernWeight);
            double targetMs = (double) rate[anims] * blendedFrameMs;
            double elapsedMs = 0.0;

            while (elapsedMs < targetMs) {
                gfxWaitTOF();
                elapsedMs += gfxGetFrameDeltaMs();
            }
            }


                if (showA) {
                    ProcessAnimation(B.pixels, cp);
                } else {
                    ProcessAnimation(A.pixels, cp);
                }

                for (s=0; s<MaxAnm; s++) {
                    if (sync[s*2]==anims && sync[s*2+1] == t) {
                        sndPrepareFX(fname[s]);
                        sndPlayFX();
                    }
                }

                if (anims == 1 && t == 62) {
                    sndPlaySound("title.bk", 0);
	        }

            if (setup.CDAudio) {

	            for (s = 0; s < MAX_TRACKS; s++) {
                    if ((CDFrames[s * 6] == anims)
                        && (CDFrames[s * 6 + 1] == t)) {
                        sndFading(16);

                        if (setup.CDAudioFromCD) {
                            CDROM_StopAudioTrack();
                            if (CDFrames[s * 6 + 2] == 0)
                                CDROM_PlayAudioTrack((unsigned char) CDFrames[s * 6 + 3]);
                            else
                                CDROM_PlayAudioSequence((unsigned char) CDFrames[s * 6 + 3],
                                                        (unsigned long) CDFrames[s * 6 + 4],
                                                        (unsigned long) CDFrames[s * 6 + 5]);
                        } else if (setup.CDAudioFromWav) {
                            const unsigned trackFlag = (unsigned) CDFrames[s * 6 + 2];
                            const unsigned trackNumber = (unsigned) CDFrames[s * 6 + 3];
                            const unsigned long startOffset = (unsigned long) CDFrames[s * 6 + 4];
                            char wavClip[32];

                            bool canPlay = false;
                            if (trackFlag == 0) {
                                canPlay = true;
                            } else if (trackFlag != 0 && startOffset == 0) {
                                /* Partial sequences unsupported, but allow zero-offset clips. */
                                canPlay = true;
                            }

                            if (canPlay && trackNumber > 0) {
                                snprintf(wavClip, sizeof(wavClip), "intro_%02u", trackNumber - 1);
                                sndStopSpeechSample();
                                if (!sndPlaySpeechSample(wavClip)) {
                                    DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                                             "Missing intro speech sample: %s.wav", wavClip);
                                }
                            }
                        }

                        break;
	                }
	            }
	        }

	        endi = false;
	    }

endit:
            gfxChangeColors(NULL, 1, GFX_FADE_OUT, colorTABLE);

	    if (endi) {
		goto endit2;
            }
	}
    }

endit2:
    gfxClearArea(NULL);

    if (setup.CDAudioFromCD) {
        CDROM_StopAudioTrack();
    }
    if (setup.CDAudioFromWav) {
        sndStopSpeechSample();
    }
    if (setup.CDAudio) {
        sndFading(0);
    }

    gfxDoneMemRastPort(&A);
    gfxDoneMemRastPort(&B);

    free(XMSHandle);
}

void gfxInitMemRastPort(MemRastPort *rp, U16 width, U16 height)
{
    rp->w = width;
    rp->h = height;

    memset(rp->palette, 0, GFX_PALETTE_SIZE);

    rp->pixels = TCAllocMem(width * height, true);
    rp->collId = GFX_NO_COLL_IN_MEM;
}

void gfxDoneMemRastPort(MemRastPort *rp)
{
    TCFreeMem(rp->pixels, rp->w * rp->h);
    rp->pixels = NULL;
}

void gfxScratchFromMem(MemRastPort *src)
{
    if (src) {
        memcpy(ScratchRP.palette, src->palette, GFX_PALETTE_SIZE);
        memcpy(ScratchRP.pixels, src->pixels, src->w * src->h);
    }
}

void gfxScratchToMem(MemRastPort *dst)
{
    if (dst) {
        memcpy(dst->palette, ScratchRP.palette, GFX_PALETTE_SIZE);
        memcpy(dst->pixels, ScratchRP.pixels, dst->w * dst->h);
    }
}


void gfxBlit(GC *gc, MemRastPort *src, U16 sx, U16 sy, U16 dx, U16 dy,
             U16 w, U16 h, bool has_mask)
{
    Rectangle srcR, srcR2, dstR, dstR2, areaR;
    SDL_Surface *dst;
    U8 *dp, *sp;
    U16 x, y;

    /* clip. */
    srcR.x = 0;
    srcR.y = 0;
    srcR.w = src->w;
    srcR.h = src->h;

    srcR2.x = sx;
    srcR2.y = sy;
    srcR2.w = w;
    srcR2.h = h;
    srcR = Clip(srcR, srcR2);

    if (srcR.w <= 0 || srcR.h <= 0)
        return;

    dstR.x = gc->clip.x;
    dstR.y = gc->clip.y;
    dstR.w = gc->clip.w;
    dstR.h = gc->clip.h;

    dstR2.x = gc->clip.x + dx;
    dstR2.y = gc->clip.y + dy;
    dstR2.w = w;
    dstR2.h = h;
    dstR = Clip(dstR, dstR2);

    if (dstR.w <= 0 || dstR.h <= 0)
        return;

    /* blit. */
    areaR.x = dstR.x;
    areaR.y = dstR.y;
    areaR.w = min(dstR.w, srcR.w);
    areaR.h = min(dstR.h, srcR.h);

    dst = Screen;

    dp = dst->pixels;
    sp = src->pixels;

    dp += dstR.y * SCREEN_WIDTH + dstR.x;
    sp += srcR.y * src->w + srcR.x;

    w = areaR.w;
    h = areaR.h;

    if (SDL_MUSTLOCK(dst))
	SDL_LockSurface(dst);

    if (has_mask) {
	for (y=0; y<h; y++) {
	    for (x=0; x<w; x++) {
		if (sp[x] != 0) {
		    dp[x] = sp[x];
		}
	    }
	    dp += SCREEN_WIDTH;
	    sp += src->w;
	}
    } else {
	for (y=0; y<h; y++) {
	    memcpy(dp, sp, w);
	    dp += SCREEN_WIDTH;
	    sp += src->w;
	}
    }

    if (SDL_MUSTLOCK(dst))
	SDL_UnlockSurface(dst);

    gfxRefreshArea(areaR.x, areaR.y, areaR.w, areaR.h);
}

static int screen_freeze_count = 0;

/* ZZZ */
void gfxRealRefreshArea(U16 x, U16 y, U16 w, U16 h)
{
    Rectangle areaR, areaR2;
    void *pixels;
    int pitch;

    areaR.x = 0;
    areaR.y = 0;
    areaR.w = SCREEN_WIDTH;
    areaR.h = SCREEN_HEIGHT;

    areaR2.x = x-2;
    areaR2.y = y-2;
    areaR2.w = w+4;
    areaR2.h = h+4;
    areaR = Clip(areaR, areaR2);

    x = areaR.x;
    y = areaR.y;
    w = areaR.w;
    h = areaR.h;

    SDL_BlitSurface(Screen, NULL, windowSurface, NULL);

    if (SDL_LockTexture(sdlTexture, NULL, &pixels, &pitch)) {
        SDL_ConvertPixels(windowSurface->w, windowSurface->h,
                windowSurface->format,
                windowSurface->pixels, windowSurface->pitch,
                SDL_PIXELFORMAT_XRGB8888,
                pixels, pitch);
        SDL_UnlockTexture(sdlTexture);
    } else {
        DebugMsg(ERR_WARNING, ERROR_MODULE_GFX,
                 "SDL_LockTexture failed: %s", SDL_GetError());
    }

    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(sdlRenderer);
    SDL_RenderTexture(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);
}


void gfxRefreshArea(U16 x, U16 y, U16 w, U16 h)
{
    if (screen_freeze_count == 0) {
        gfxRealRefreshArea(x, y, w, h);
    }
}

void gfxScreenFreeze(void)
{
    screen_freeze_count++;
}

void gfxScreenThaw(GC *gc, U16 x, U16 y, U16 w, U16 h)
{
  if (screen_freeze_count > 0) {
    Rectangle dstR, dstR2;

    screen_freeze_count--;

    dstR.x = gc->clip.x;
    dstR.y = gc->clip.y;
    dstR.w = gc->clip.w;
    dstR.h = gc->clip.h;

    dstR2.x = gc->clip.x + x;
    dstR2.y = gc->clip.y + y;
    dstR2.w = w;
    dstR2.h = h;
    dstR = Clip(dstR, dstR2);

    if (dstR.w > 0 && dstR.h > 0) {
        gfxRefreshArea(dstR.x, dstR.y, dstR.w, dstR.h);
    }
  }
}

void MemBlit(MemRastPort *src, Rect *src_rect,
             MemRastPort *dst, Rect *dst_rect, ROpE op)
{
    Rectangle srcR, srcR2, dstR, dstR2, areaR;
    U16 sw, dw, x, y;
    register U8 *dp, *sp;

    /* clip. */
    srcR.x = 0;
    srcR.y = 0;
    srcR.w = src->w;
    srcR.h = src->h;

    srcR2.x = src_rect->x;
    srcR2.y = src_rect->y;
    srcR2.w = src_rect->w;
    srcR2.h = src_rect->h;
    srcR = Clip(srcR, srcR2);

    if (srcR.w <= 0 || srcR.h <= 0)
        return;

    dstR.x = 0;
    dstR.y = 0;
    dstR.w = dst->w;
    dstR.h = dst->h;

    dstR2.x = dst_rect->x;
    dstR2.y = dst_rect->y;
    dstR2.w = dst_rect->w;
    dstR2.h = dst_rect->h;
    dstR = Clip(dstR, dstR2);

    if (dstR.w <= 0 || dstR.h <= 0)
        return;

    areaR.x = dstR.x;
    areaR.y = dstR.y;
    areaR.w = min(dstR.w, srcR.w);
    areaR.h = min(dstR.h, srcR.h);

    /* blit. */
    sw = src->w;
    dw = dst->w;

    dp = dst->pixels;
    sp = src->pixels;

    dp += dstR.y * dw + dstR.x;
    sp += srcR.y * sw + srcR.x;

    switch (op) {
    case GFX_ROP_BLIT:
        for (y=0; y<areaR.h; y++) {
            memcpy(dp, sp, areaR.w);
            dp += dw;
            sp += sw;
        }
        break;

    case GFX_ROP_MASK_BLIT:
        for (y=0; y<areaR.h; y++) {
            for (x=0; x<areaR.w; x++) {
                if (sp[x] != 0) {
                    dp[x] = sp[x] | (dp[x] & 0x40);
                }
            }
            dp += dw;
            sp += sw;
        }
        break;

    case GFX_ROP_CLR:
        for (y=0; y<areaR.h; y++) {
            for (x=0; x<areaR.w; x++) {
                dp[x] &= ~sp[x];
            }
            dp += dw;
            sp += sw;
        }
        break;

    case GFX_ROP_SET:
        for (y=0; y<areaR.h; y++) {
            for (x=0; x<areaR.w; x++) {
                dp[x] |= sp[x];
            }
            dp += dw;
            sp += sw;
        }
        break;

    case GFX_ROP_OVERLAY:
        for (y=0; y<areaR.h; y++) {
            for (x=0; x<areaR.w; x++) {
                if (sp[x] != 0)
                    dp[x] = sp[x];
            }
            dp += dw;
            sp += sw;
        }
        break;

    case GFX_ROP_OVERLAY_MSK:
        for (y=0; y<areaR.h; y++) {
            for (x=0; x<areaR.w; x++) {
                if (sp[x] != 0)
                    dp[x] = sp[x] | (dp[x] & 0x40);
            }
            dp += dw;
            sp += sw;
        }
        break;
    }
}

void gfxWindowToLogical(int windowX, int windowY, int *logicalX, int *logicalY)
{
    float logicalXf = (float) windowX;
    float logicalYf = (float) windowY;
    bool converted = false;

    if (sdlRenderer) {
        converted = SDL_RenderCoordinatesFromWindow(sdlRenderer,
                                                    (float) windowX,
                                                    (float) windowY,
                                                    &logicalXf,
                                                    &logicalYf);
    }

    if (!converted) {
        unsigned scale = setup.Scale ? setup.Scale : 1;
        logicalXf = (float) windowX / (float) scale;
        logicalYf = (float) windowY / (float) scale;
    }

    if (logicalXf < 0.0f)
        logicalXf = 0.0f;
    if (logicalYf < 0.0f)
        logicalYf = 0.0f;
    if (logicalXf > (float)(SCREEN_WIDTH - 1))
        logicalXf = (float)(SCREEN_WIDTH - 1);
    if (logicalYf > (float)(SCREEN_HEIGHT - 1))
        logicalYf = (float)(SCREEN_HEIGHT - 1);

    if (logicalX)
        *logicalX = (int) logicalXf;
    if (logicalY)
        *logicalY = (int) logicalYf;
}

void gfxGetMouseXY(GC *gc, U16 *pMouseX, U16 *pMouseY)
{
    float MouseXf = 0.0f, MouseYf = 0.0f;
    int MouseX = 0, MouseY = 0;

    SDL_PumpEvents();

    SDL_GetMouseState(&MouseXf, &MouseYf);
    MouseX = (int) SDL_roundf(MouseXf);
    MouseY = (int) SDL_roundf(MouseYf);

    if (pMouseX || pMouseY)
        gfxWindowToLogical(MouseX, MouseY, &MouseX, &MouseY);

    if (pMouseX) {
	if (MouseX < gc->clip.x)
	    MouseX = 0;
	else
	    MouseX -= gc->clip.x;
	*pMouseX = MouseX;
    }

    if (pMouseY) {
	if (MouseY < gc->clip.y)
	    MouseY = 0;
	else
	    MouseY -= gc->clip.y;
	*pMouseY = MouseY;
    }
}

