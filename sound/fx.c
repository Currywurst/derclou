/****************************************************************************
  Copyright (c) 2005 Vasco Alexandre da Silva Costa
  Portions copyright (c) 2005 Jens Granseuer

  Please read the license terms contained in the LICENSE and
  publiclicensecontract.doc files which should be contained with this
  distribution.
 ****************************************************************************/
#include <limits.h>
#include <math.h>

#include <SDL3/SDL.h>

#include "base/base.h"

#include "sound/buffer.h"
#include "sound/newsound.h"
#include "sound/fx.h"
#include "sound/hsc.h"

#define SND_STREAM_CHUNK_BYTES (SND_SAMPLES * (int)sizeof(S16))

struct FXBase FXBase;

static bool SfxChannelOn = false;
static bool MusicChannelOn = true;
static Uint8 MixChunk[SND_STREAM_CHUNK_BYTES];

typedef struct SpeechChannel {
    S16 *samples;
    size_t frames;
    size_t cursor;
    bool active;
} SpeechChannel;

static SpeechChannel Speech = { NULL, 0, 0, false };
static bool SpeechDirChecked = false;
static bool SpeechDirAvailable = false;

static void LoadVOC(const char *fileName);
static void SpeechFreeBufferUnlocked(void);
static S16 SpeechNextSample(void);
static void MixAudioChunk(Uint8 *stream, int len);
static int SDLCALL sndAudioThread(void *userdata);

void sndAudioLock(void)
{
    if (FXBase.audioMutex) {
        SDL_LockMutex(FXBase.audioMutex);
    }
}

void sndAudioUnlock(void)
{
    if (FXBase.audioMutex) {
        SDL_UnlockMutex(FXBase.audioMutex);
    }
}

static void MixAudioChunk(Uint8 *stream, int len)
{
    const float sfxFactor = ((float) setup.SfxVolume) / SND_MAX_VOLUME;
    const float musicFactor = ((float) setup.MusicVolume) / SND_MAX_VOLUME;
    const int alignedLen = len & ~(int) (sizeof(S16) - 1);
    int frames, i;

    if (alignedLen <= 0) {
        return;
    }

    if (MusicChannelOn) {
        hscMusicPlayer(alignedLen);
    }

    SDL_memset(stream, 0, alignedLen);

    frames = alignedLen / (int) sizeof(S16);
    for (i = 0; i < frames; i++) {
        S16 sfxSample = 0;
        S16 musicSample = 0;

        if (SfxChannelOn) {
            sndRemoveBuffer(FXBase.pSfxBuffer, &sfxSample, sizeof(sfxSample));
        }
        if (MusicChannelOn) {
            sndRemoveBuffer(FXBase.pMusicBuffer, &musicSample,
                            sizeof(musicSample));
        }

        float sample = sfxFactor * (float) sfxSample
                + musicFactor * (float) musicSample;
        sample += (float) SpeechNextSample();

        if (sample > INT16_MAX) {
            sample = INT16_MAX;
        } else if (sample < INT16_MIN) {
            sample = INT16_MIN;
        }

        ((S16 *) stream)[i] = (S16) sample;
    }
}

static int SDLCALL sndAudioThread(void *userdata)
{
    (void) userdata;

    while (FXBase.audioThreadRunning) {
        const int target = SND_STREAM_CHUNK_BYTES * 4;
        int available;
        int deficit;

        if (!FXBase.audioStream) {
            SDL_Delay(10);
            continue;
        }

        available = SDL_GetAudioStreamAvailable(FXBase.audioStream);
        deficit = target - available;

        while (FXBase.audioThreadRunning && deficit > 0) {
            int chunk = SDL_min(deficit, SND_STREAM_CHUNK_BYTES);

            chunk &= ~(int) (sizeof(S16) - 1);
            if (chunk <= 0) {
                break;
            }

            sndAudioLock();
            MixAudioChunk(MixChunk, chunk);
            sndAudioUnlock();

            if (!SDL_PutAudioStreamData(FXBase.audioStream, MixChunk, chunk)) {
                DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                         "SDL_PutAudioStreamData: %s", SDL_GetError());
                SDL_Delay(10);
                break;
            }

            deficit -= chunk;
        }

        SDL_Delay(5);
    }

    return 0;
}

void InitAudio(void)
{
    SDL_AudioSpec spec;

    FXBase.us_AudioOk = 0;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_InitSubSystem: %s", SDL_GetError());
        return;
    }

    SDL_zero(spec);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq = SND_FREQUENCY;

    FXBase.audioStream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!FXBase.audioStream) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_OpenAudioDeviceStream: %s", SDL_GetError());
        return;
    }

    FXBase.pSfxBuffer = sndCreateBuffer(SND_BUFFER_SIZE);
    FXBase.pMusicBuffer = sndCreateBuffer(SND_BUFFER_SIZE);

    FXBase.audioMutex = SDL_CreateMutex();
    if (!FXBase.audioMutex) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_CreateMutex failed: %s", SDL_GetError());
        goto fail;
    }

    FXBase.audioThreadRunning = true;
    FXBase.audioThread = SDL_CreateThread(sndAudioThread, "audio-mix", NULL);
    if (!FXBase.audioThread) {
        FXBase.audioThreadRunning = false;
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_CreateThread failed: %s", SDL_GetError());
        goto fail;
    }

    if (!SDL_ResumeAudioStreamDevice(FXBase.audioStream)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
        goto fail;
    }

    FXBase.us_AudioOk = 1;
    return;

fail:
    if (FXBase.audioThread) {
        FXBase.audioThreadRunning = false;
        SDL_WaitThread(FXBase.audioThread, NULL);
        FXBase.audioThread = NULL;
    }
    if (FXBase.audioMutex) {
        SDL_DestroyMutex(FXBase.audioMutex);
        FXBase.audioMutex = NULL;
    }
    if (FXBase.audioStream) {
        SDL_DestroyAudioStream(FXBase.audioStream);
        FXBase.audioStream = NULL;
    }
    if (FXBase.pSfxBuffer) {
        sndFreeBuffer(FXBase.pSfxBuffer);
        FXBase.pSfxBuffer = NULL;
    }
    if (FXBase.pMusicBuffer) {
        sndFreeBuffer(FXBase.pMusicBuffer);
        FXBase.pMusicBuffer = NULL;
    }
}

void RemoveAudio(void)
{
    sndStopSpeechSample();

    FXBase.audioThreadRunning = false;
    if (FXBase.audioThread) {
        SDL_WaitThread(FXBase.audioThread, NULL);
        FXBase.audioThread = NULL;
    }

    if (FXBase.audioMutex) {
        SDL_DestroyMutex(FXBase.audioMutex);
        FXBase.audioMutex = NULL;
    }

    if (FXBase.audioStream) {
        if (!SDL_PauseAudioStreamDevice(FXBase.audioStream)) {
            DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                     "SDL_PauseAudioStreamDevice failed: %s",
                     SDL_GetError());
        }
        SDL_FlushAudioStream(FXBase.audioStream);
        SDL_DestroyAudioStream(FXBase.audioStream);
        FXBase.audioStream = NULL;
    }

    if (FXBase.pSfxBuffer) {
        sndFreeBuffer(FXBase.pSfxBuffer);
        FXBase.pSfxBuffer = NULL;
    }
    if (FXBase.pMusicBuffer) {
        sndFreeBuffer(FXBase.pMusicBuffer);
        FXBase.pMusicBuffer = NULL;
    }

    FXBase.us_AudioOk = 0;
}

static void sndSetFxChannel(bool enabled)
{
    sndAudioLock();

    SfxChannelOn = enabled;

    sndAudioUnlock();
}

void sndInitFX(void)
{
    sndSetFxChannel(false);
}

void sndDoneFX(void)
{
    sndSetFxChannel(false);
}

void sndPrepareFX(const char *name)
{
    sndDoneFX();

    if (FXBase.us_AudioOk) {
        char fileName[DSK_PATH_MAX];

        dskBuildPathName(DISK_CHECK_FILE, SAMPLES_DIRECTORY, name, fileName);
        LoadVOC(fileName);
    }
}

void sndPlayFX(void)
{
    sndAudioLock();

    SfxChannelOn = true;

    sndAudioUnlock();
}

bool sndSpeechLibraryAvailable(void)
{
    if (!SpeechDirChecked) {
        char dummy[DSK_PATH_MAX];

        SpeechDirAvailable = dskBuildPathName(DISK_CHECK_DIR,
                                             AUDIO_DIRECTORY,
                                             "",
                                             dummy);
        SpeechDirChecked = true;
    }

    return SpeechDirAvailable;
}

bool sndPlaySpeechSample(const char *clipName)
{
    SDL_AudioSpec wavSpec;
    Uint8 *wavBuffer = NULL;
    Uint32 wavLength = 0;
    Uint8 *converted = NULL;
    int convertedLen = 0;
    size_t frames;
    char fileName[DSK_PATH_MAX];
    char path[DSK_PATH_MAX];

    if (!clipName || !clipName[0]) {
        return false;
    }

    if (!sndSpeechLibraryAvailable()) {
        return false;
    }

    snprintf(fileName, sizeof(fileName), "%s.wav", clipName);

    if (!dskBuildPathName(DISK_CHECK_FILE, AUDIO_DIRECTORY, fileName, path)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "Speech clip missing: %s", fileName);
        return false;
    }

    if (!SDL_LoadWAV(path, &wavSpec, &wavBuffer, &wavLength)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_LoadWAV failed for %s: %s",
                 fileName, SDL_GetError());
        return false;
    }

    SDL_AudioSpec dstSpec;
    dstSpec.format = SDL_AUDIO_S16;
    dstSpec.channels = 1;
    dstSpec.freq = SND_FREQUENCY;

    if (!SDL_ConvertAudioSamples(&wavSpec, wavBuffer, (int) wavLength,
                                 &dstSpec, &converted, &convertedLen)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_ConvertAudioSamples failed for %s: %s",
                 fileName, SDL_GetError());
        SDL_free(wavBuffer);
        return false;
    }

    SDL_free(wavBuffer);

    frames = (size_t) convertedLen / sizeof(S16);
    if (!frames) {
        SDL_free(converted);
        return false;
    }

    sndAudioLock();
    SpeechFreeBufferUnlocked();
    Speech.samples = (S16 *) converted;
    Speech.frames = frames;
    Speech.cursor = 0;
    Speech.active = true;
    sndAudioUnlock();

    return true;
}

void sndStopSpeechSample(void)
{
    sndAudioLock();
    SpeechFreeBufferUnlocked();
    sndAudioUnlock();
}

bool sndSpeechSamplePlaying(void)
{
    bool playing;

    sndAudioLock();
    playing = Speech.active;
    sndAudioUnlock();

    return playing;
}

static const char magicString[] =
    "Creative Voice File\x1a";

static void SpeechFreeBufferUnlocked(void)
{
    if (Speech.samples) {
        SDL_free(Speech.samples);
        Speech.samples = NULL;
    }

    Speech.frames = 0;
    Speech.cursor = 0;
    Speech.active = false;
}

static S16 SpeechNextSample(void)
{
    S16 sample = 0;

    if (!Speech.active || !Speech.samples) {
        return 0;
    }

    if (Speech.cursor >= Speech.frames) {
        SpeechFreeBufferUnlocked();
        return 0;
    }

    sample = Speech.samples[Speech.cursor++];

    if (Speech.cursor >= Speech.frames) {
        SpeechFreeBufferUnlocked();
    }

    return sample;
}

static void LoadVOC(const char *fileName)
{
    U8 *pSoundFile, *pRawSound;
    S16 *pResampledSound;

    unsigned blockOffset, blockType, numSamples;
    U8 SR;
    unsigned sampleRate, compressionType;

    if (!(pSoundFile = dskLoad(fileName))) {
	return;
    }

    /* parse VOC header */
    if (memcmp(pSoundFile, magicString, strlen(magicString)) != 0) {
	DebugMsg(ERR_ERROR, ERROR_MODULE_SOUND, "Not a VOC file: %s",
		 fileName);
    }

    blockOffset = (pSoundFile[0x14])
		| (pSoundFile[0x15] << 8);

    /* parse VOC data block */
    if ((blockType = pSoundFile[blockOffset]) != 1) {
	DebugMsg(ERR_ERROR, ERROR_MODULE_SOUND,
		 "Unknown VOC block-type: %d",
		 blockType);
    }

    numSamples = (pSoundFile[blockOffset + 0x3] << 16)
	       | (pSoundFile[blockOffset + 0x2] << 8)
	       | (pSoundFile[blockOffset + 0x1]);
    numSamples -= 2;

    DebugMsg(ERR_DEBUG, ERROR_MODULE_SOUND,
	     "block-offset: %d, num-samples: %d",
	     blockOffset, numSamples);

    SR = pSoundFile[blockOffset + 0x4];
    sampleRate = 1000000L / (256L - SR);

    compressionType = pSoundFile[blockOffset + 0x5];

    DebugMsg(ERR_DEBUG, ERROR_MODULE_SOUND,
	     "sample-rate: %d Hz, compression-type: %d",
	     sampleRate, compressionType);

    if (compressionType != 0) {
	DebugMsg(ERR_ERROR, ERROR_MODULE_SOUND,
		 "Unknown VOC compression-type: %d",
		 compressionType);
    }

    pRawSound = &pSoundFile[blockOffset + 0x6];

    /* do linearly interpolated sound resampling */
    {
	float resampleRate, resampleRatio, numResamples;
	unsigned i, nResamples, sizeResamples;

	/* continuous resample ratio */
	resampleRate = SND_FREQUENCY;
	resampleRatio = resampleRate / sampleRate;

	/* discrete number of resampled samples */
	numResamples = ceil(resampleRatio * numSamples);
	nResamples = numResamples;

	/* discrete resample ratio */
	resampleRate = numResamples * sampleRate / numSamples;
	resampleRatio = (float) sampleRate / resampleRate;

            sizeResamples = nResamples * sizeof(*pResampledSound);
            pResampledSound = malloc(sizeResamples);
            if (!pResampledSound) {
                free(pSoundFile);
                ErrorMsg(No_Mem, ERROR_MODULE_SOUND, 0);
                return;
            }

	for (i = 0; i < nResamples; i++) {
	    float middleT, leftT;		/* time */
	    float middleS, leftS, rightS;	/* sample */
	    unsigned left, right; 

	    /* transform from resampled to original coordinates */
	    middleT = resampleRatio * i;

	    /* get neighbouring points: left and right */
	    leftT = floor(middleT);

	    left = leftT;
	    left = clamp(left, 0, numSamples - 1);

	    right = left + 1;
	    right = clamp(right, 0, numSamples - 1);

	    /* get neighbouring points sample values */
	    leftS = pRawSound[left];
	    rightS = pRawSound[right];

	    /* linear interpolation */
	    middleS = (rightS - leftS) * (middleT - leftT) + leftS;

	    /* transform samples from u8 to s16 */
	    middleS = middleS * 255.0f - 32768.0f;

	    pResampledSound[i] = (middleS + 0.5f);	/* round */
	}
	sndResetBuffer(FXBase.pSfxBuffer);
	sndInsertBuffer(FXBase.pSfxBuffer, pResampledSound, sizeResamples);

	free(pResampledSound);
    }

    free(pSoundFile);
}

