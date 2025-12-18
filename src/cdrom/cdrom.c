/****************************************************************************
  Copyright (c) 2005 Vasco Alexandre da Silva Costa

  Please read the license terms contained in the LICENSE and
  publiclicensecontract.doc files which should be contained with this
  distribution.
 ****************************************************************************/

#include "cdrom/cdrom.h"

#include <string.h>

#include <SDL3/SDL.h>

#include "error/error.h"

int CDRomInstalled = 0;

#ifndef THECLOU_DISABLE_SDL_CDROM
#if defined(SDL_MAJOR_VERSION) && SDL_MAJOR_VERSION >= 2
#define THECLOU_DISABLE_SDL_CDROM 1
#endif
#endif

#if !defined(THECLOU_DISABLE_SDL_CDROM)

static SDL_CD *CDROM_Device = NULL;
static int CDROM_DriveNr = -1;
static char CDDrive[256] = "X:\\";

static int CDROM_SetGameCDDrive(void);
static int CDROM_GetDeviceStatus(void);
static int CDROM_CheckMediaChanged(void);

int CDROM_Install(void)
{
    CDRomInstalled = 0;
    CDROM_DriveNr = -1;

    if (SDL_InitSubSystem(SDL_INIT_CDROM) != 0) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "SDL_INIT_CDROM failed: %s", SDL_GetError());
        return -1;
    }

    if (SDL_CDNumDrives() <= 0) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "No CD/DVD drives detected");
        SDL_QuitSubSystem(SDL_INIT_CDROM);
        return -1;
    }

    if (CDROM_SetGameCDDrive() == -1) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "\"The Clou!\" CD not found");
        SDL_QuitSubSystem(SDL_INIT_CDROM);
        return 0;
    }

    CDROM_Device = SDL_CDOpen(CDROM_DriveNr);
    if (!CDROM_Device) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "SDL_CDOpen failed: %s", SDL_GetError());
        CDROM_DriveNr = -1;
        SDL_QuitSubSystem(SDL_INIT_CDROM);
        return -1;
    }

    if (SDL_CDStatus(CDROM_Device) == CD_ERROR) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "SDL_CDStatus error: %s", SDL_GetError());
        SDL_CDClose(CDROM_Device);
        CDROM_Device = NULL;
        CDROM_DriveNr = -1;
        SDL_QuitSubSystem(SDL_INIT_CDROM);
        return -1;
    }

    CDRomInstalled = 1;
    return 1;
}

void CDROM_UnInstall(void)
{
    if (CDROM_Device) {
        SDL_CDClose(CDROM_Device);
        CDROM_Device = NULL;
    }

    if (SDL_WasInit(SDL_INIT_CDROM)) {
        SDL_QuitSubSystem(SDL_INIT_CDROM);
    }

    CDRomInstalled = 0;
    CDROM_DriveNr = -1;
}

int CDROM_WaitForMedia(void)
{
    int count = 0;

    if (!CDRomInstalled || !CDROM_Device) {
        return -1;
    }

    while (CDROM_CheckMediaChanged() != 0) {
        SDL_Delay(10);
        count++;

        if (count == 10000) {
            DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                     "CD/DVD not found in drive %s", CDDrive);
            return -1;
        }
    }

    return 0;
}

void CDROM_PlayAudioTrack(U8 TrackNum)
{
    if (!CDRomInstalled || !CDROM_Device) {
        return;
    }

    if (CDROM_CheckMediaChanged() != 0) {
        return;
    }

    if (!TrackNum || CDROM_Device->numtracks < TrackNum) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "CD track %u not available", (unsigned int) TrackNum);
        return;
    }

    SDL_CDPlayTracks(CDROM_Device, TrackNum - 1, 0, 1, 0);
    CDROM_GetDeviceStatus();
}

void CDROM_PlayAudioSequence(U8 TrackNum, U32 StartOffset, U32 EndOffset)
{
    if (!CDRomInstalled || !CDROM_Device) {
        return;
    }

    if (CDROM_CheckMediaChanged() != 0) {
        return;
    }

    if (!TrackNum || CDROM_Device->numtracks < TrackNum) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "CD track %u not available", (unsigned int) TrackNum);
        return;
    }

    if (EndOffset < StartOffset) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
                 "Invalid CD offsets: start %lu end %lu",
                 (unsigned long) StartOffset, (unsigned long) EndOffset);
        return;
    }

    DebugMsg(ERR_DEBUG, ERROR_MODULE_BASE,
             "Playing CD track %u from %lu to %lu",
             (unsigned int) TrackNum,
             (unsigned long) StartOffset,
             (unsigned long) EndOffset);

    SDL_CDPlayTracks(CDROM_Device, TrackNum - 1, (int) StartOffset, 0,
                     (int) (EndOffset - StartOffset));
    CDROM_GetDeviceStatus();
}

void CDROM_StopAudioTrack(void)
{
    if (CDROM_Device && CDROM_Device->status != CD_STOPPED) {
        SDL_CDStop(CDROM_Device);
        CDROM_GetDeviceStatus();
    }
}

static int CDROM_GetDeviceStatus(void)
{
    if (!CDROM_Device) {
        return -1;
    }

    return SDL_CDStatus(CDROM_Device);
}

static int CDROM_CheckMediaChanged(void)
{
    int status = CDROM_GetDeviceStatus();

    if (status < 0) {
        return 1;
    }

    return status == CD_TRAYEMPTY;
}

static int CDROM_SetGameCDDrive(void)
{
    int numDrives = SDL_CDNumDrives();
    int drive;

    for (drive = 0; drive < numDrives; drive++) {
        SDL_CD *probe = SDL_CDOpen(drive);

        if (!probe) {
            continue;
        }

        if (CD_INDRIVE(SDL_CDStatus(probe))) {
            /* Match "The Clou!" disc by expected data track and track lengths. */
            if ((probe->numtracks == 24) &&
                (probe->track[0].type == SDL_DATA_TRACK) &&
                (probe->track[1].length > 146000) &&
                (probe->track[1].length < 147000)) {
                const char *name = SDL_CDName(drive);

                if (name) {
                    strncpy(CDDrive, name, sizeof(CDDrive) - 1);
                    CDDrive[sizeof(CDDrive) - 1] = '\0';
                }

                CDROM_DriveNr = drive;
                SDL_CDClose(probe);
                return drive;
            }
        }

        SDL_CDClose(probe);
    }

    return -1;
}

#else  /* THECLOU_DISABLE_SDL_CDROM */

int CDROM_Install(void)
{
    DebugMsg(ERR_WARNING, ERROR_MODULE_BASE,
             "SDL CD-ROM subsystem not available in this build");
    CDRomInstalled = 0;
    return -1;
}

void CDROM_UnInstall(void)
{
    CDRomInstalled = 0;
}

int CDROM_WaitForMedia(void)
{
    return -1;
}

void CDROM_PlayAudioTrack(U8 TrackNum)
{
    (void) TrackNum;
}

void CDROM_PlayAudioSequence(U8 TrackNum, U32 StartOffset, U32 EndOffset)
{
    (void) TrackNum;
    (void) StartOffset;
    (void) EndOffset;
}

void CDROM_StopAudioTrack(void)
{
}

#endif /* THECLOU_DISABLE_SDL_CDROM */
