/****************************************************************************
  Copyright (c) 2005 Vasco Alexandre da Silva Costa
  Portions copyright (c) 2005 Jens Granseuer

  Please read the license terms contained in the LICENSE and
  publiclicensecontract.doc files which should be contained with this
  distribution.
 ****************************************************************************/
#include <limits.h>
#include <math.h>

#include "SDL.h"

#include "base/base.h"

#include "sound/buffer.h"
#include "sound/newsound.h"
#include "sound/fx.h"
#include "sound/hsc.h"

struct FXBase FXBase;

static bool SfxChannelOn = false;
static bool MusicChannelOn = true;

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


static void sndCallBack(void *udata, Uint8 *stream, int len)
{
    S16 *MixStream = (S16 *) stream;
    const float sfxFactor = ((float)setup.SfxVolume) / SND_MAX_VOLUME,
            musicFactor = ((float)setup.MusicVolume) / SND_MAX_VOLUME;
    int sizeStream, i;

    if (MusicChannelOn) {
	hscMusicPlayer(len);
    }

    SDL_memset(stream, 0, len);

    sizeStream = len / sizeof(S16);
    for (i = 0; i < sizeStream; i++) {
	S16 Sfx, Music;

	Sfx = Music = 0;
	if (SfxChannelOn) {
	    sndRemoveBuffer(FXBase.pSfxBuffer, &Sfx, sizeof(Sfx));
	}
	if (MusicChannelOn) {
	    sndRemoveBuffer(FXBase.pMusicBuffer, &Music, sizeof(Music));
	}

    float sample = sfxFactor * (float) Sfx + musicFactor * (float) Music;
    sample += (float) SpeechNextSample();

    if (sample > INT16_MAX) {
        sample = INT16_MAX;
    } else if (sample < INT16_MIN) {
        sample = INT16_MIN;
    }

    MixStream[i] = (S16) sample;
    }
}

void InitAudio(void)
{
    SDL_AudioSpec spec;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
	FXBase.us_AudioOk = 1;
    } else {
	DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_InitSubSystem: %s", SDL_GetError());
	FXBase.us_AudioOk = 0;
	return;
    }

    spec.freq       = SND_FREQUENCY;
    spec.format     = AUDIO_S16;
    spec.channels   = 1;
    spec.samples    = SND_SAMPLES;
    spec.callback   = sndCallBack;
    spec.userdata   = NULL;

    if (SDL_OpenAudio(&spec, NULL)) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_OpenAudio: %s", SDL_GetError());
        FXBase.us_AudioOk = 0;
	return;
    }

    FXBase.pSfxBuffer = sndCreateBuffer(SND_BUFFER_SIZE);
    FXBase.pMusicBuffer = sndCreateBuffer(SND_BUFFER_SIZE);

    SDL_PauseAudio(0);
}

void RemoveAudio(void)
{
    sndStopSpeechSample();

    SDL_CloseAudio();

    if (SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    FXBase.us_AudioOk = 0;
}

static void sndSetFxChannel(bool enabled)
{
    SDL_LockAudio();

    SfxChannelOn = enabled;

    SDL_UnlockAudio();
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
    SDL_LockAudio();

    SfxChannelOn = true;

    SDL_UnlockAudio();
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
    SDL_AudioCVT cvt;
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

    if (SDL_BuildAudioCVT(&cvt,
                          wavSpec.format, wavSpec.channels, wavSpec.freq,
                          AUDIO_S16, 1, SND_FREQUENCY) < 0) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_BuildAudioCVT failed for %s: %s",
                 fileName, SDL_GetError());
        SDL_FreeWAV(wavBuffer);
        return false;
    }

    cvt.len = wavLength;
    cvt.buf = SDL_malloc((size_t) cvt.len * (size_t) cvt.len_mult);
    if (!cvt.buf) {
        SDL_FreeWAV(wavBuffer);
        ErrorMsg(No_Mem, ERROR_MODULE_SOUND, 0);
        return false;
    }
    memcpy(cvt.buf, wavBuffer, wavLength);
    SDL_FreeWAV(wavBuffer);

    if (SDL_ConvertAudio(&cvt) < 0) {
        DebugMsg(ERR_WARNING, ERROR_MODULE_SOUND,
                 "SDL_ConvertAudio failed for %s: %s",
                 fileName, SDL_GetError());
        SDL_free(cvt.buf);
        return false;
    }

    frames = cvt.len_cvt / sizeof(S16);
    if (!frames) {
        SDL_free(cvt.buf);
        return false;
    }

    SDL_LockAudio();
    SpeechFreeBufferUnlocked();
    Speech.samples = (S16 *) cvt.buf;
    Speech.frames = frames;
    Speech.cursor = 0;
    Speech.active = true;
    SDL_UnlockAudio();

    return true;
}

void sndStopSpeechSample(void)
{
    SDL_LockAudio();
    SpeechFreeBufferUnlocked();
    SDL_UnlockAudio();
}

bool sndSpeechSamplePlaying(void)
{
    bool playing;

    SDL_LockAudio();
    playing = Speech.active;
    SDL_UnlockAudio();

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

