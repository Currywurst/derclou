/****************************************************************************
  Copyright (c) 2005 Vasco Alexandre da Silva Costa

  Please read the license terms contained in the LICENSE and
  publiclicensecontract.doc files which should be contained with this
  distribution.
 ****************************************************************************/
#include <SDL3/SDL.h>

#include "disk/disk.h"

#include "base/base.h"

#include "sound/fx.h"
#include "sound/newsound.h"

#include "sound/hsc.h"

char currSoundName[DSK_PATH_MAX];


void sndInit(void)
{
    currSoundName[0] = '\0';

    hscInit();
}

void sndDone(void)
{
    sndAudioLock();
    hscDone();
    sndAudioUnlock();
}

void sndPlaySound(char *name, U32 mode)
{
    char path[DSK_PATH_MAX];

    if (strcmp(currSoundName, name) != 0) {
	strcpy(currSoundName, name);

	if (FXBase.us_AudioOk) {
	    dskBuildPathName(DISK_CHECK_FILE, SOUND_DIRECTORY, name, path);

            sndAudioLock();

	    hscLoad(path);

            sndAudioUnlock();
	}
    }
}

char *sndGetCurrSoundName(void)
{
    return currSoundName;
}


void sndFading(short int targetVol)
{
    static int restoreVolume = -1;
    const int fadeStep = 4;
    const int fadeDelayMs = 10;
    int currentVolume, destVolume, step;

    if (!FXBase.us_AudioOk) {
        return;
    }

    sndAudioLock();
    currentVolume = clamp(setup.MusicVolume, 0, SND_MAX_VOLUME);
    sndAudioUnlock();

    if (restoreVolume < 0) {
        restoreVolume = currentVolume;
    }

    if (targetVol <= 0) {
        destVolume = restoreVolume;
    } else {
        restoreVolume = currentVolume; /* remember level to restore later */
        destVolume = clamp(targetVol, 0, SND_MAX_VOLUME);
    }

    if (destVolume == currentVolume) {
        return;
    }

    step = (destVolume > currentVolume) ? fadeStep : -fadeStep;

    while (currentVolume != destVolume) {
        currentVolume += step;

        if ((step > 0 && currentVolume > destVolume) ||
            (step < 0 && currentVolume < destVolume)) {
            currentVolume = destVolume;
        }

        sndAudioLock();
        setup.MusicVolume = currentVolume;
        sndAudioUnlock();

        SDL_Delay(fadeDelayMs);
    }
}

void sndStopSound(U8 dummy)
{
    if (FXBase.us_AudioOk) {
        sndAudioLock();

        hscReset();

        sndAudioUnlock();
    }
}
