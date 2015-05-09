/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
 *  Copyright (C) 2015 Valtteri "tsone" Heikkila
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef _EM_H_
#define _EM_H_
#include <SDL.h>
#include "../../driver.h"
#include "../common/args.h"
#include "../common/config.h"
#include "../common/configSys.h"


// Disabled functions
// TODO: tsone: remove?
#define RefreshThrottleFPS()
#define SpeedThrottle() 0
#define IncreaseEmulationSpeed()
#define DecreaseEmulationSpeed()
#define InitJoysticks() 1
#define KillJoysticks() 0

// eoptions variable flags
#define EO_NO8LIM      1
#define EO_SUBASE      2
#define EO_CLIPSIDES   8
#define EO_SNAPNAME    16
#define EO_FOURSCORE	32
#define EO_NOTHROTTLE	64
#define EO_GAMEGENIE	128
#define EO_PAL		256
#define EO_LOWPASS	512
#define EO_AUTOHIDE	1024

// Input device options
#define NUM_INPUT_DEVICES 3
// GamePad defaults
#define GAMEPAD_NUM_DEVICES 4
#define GAMEPAD_NUM_BUTTONS 10
// PowerPad defaults
#define POWERPAD_NUM_DEVICES 2
#define POWERPAD_NUM_BUTTONS 12
// QuizKing defaults
#define QUIZKING_NUM_BUTTONS 6
// HyperShot defaults
#define HYPERSHOT_NUM_BUTTONS 4
// Mahjong defaults
#define MAHJONG_NUM_BUTTONS 21
// TopRider defaults
#define TOPRIDER_NUM_BUTTONS 8
// FTrainer defaults
#define FTRAINER_NUM_BUTTONS 12
// FamilyKeyBoard defaults
#define FAMILYKEYBOARD_NUM_BUTTONS 0x48

#define BUTTC_KEYBOARD          0x00
#define BUTTC_JOYSTICK          0x01
#define BUTTC_MOUSE             0x02

#define FCFGD_GAMEPAD   1
#define FCFGD_POWERPAD  2
#define FCFGD_HYPERSHOT 3
#define FCFGD_QUIZKING  4

#define DTestButtonJoy(a_) 0

// Audio options
// NOTE: tsone: both SOUND_BUF_MAX and SOUND_HW_BUF_MAX must be power of two!
#if 1
#if SDL_VERSION_ATLEAST(2, 0, 0)
// NOTE: tsone: for 32-bit floating-point audio (SDL2 port)
#define SOUND_RATE		48000
#define SOUND_BUF_MAX		8192
#define SOUND_HW_BUF_MAX	2048
#else
// NOTE: tsone: for SDL1.x 16-bit integer audio (SDL1)
#define SOUND_RATE		22050
#define SOUND_BUF_MAX		4096
#define SOUND_HW_BUF_MAX	512
#endif
#define SOUND_QUALITY		0
#else
#define SOUND_RATE		44100
#define SOUND_BUF_MAX		8192
#define SOUND_HW_BUF_MAX	1024
#define SOUND_QUALITY		1
#endif
#define SOUND_BUF_MASK		(SOUND_BUF_MAX-1)


#define MAXBUTTCONFIG   4
typedef struct {
	uint8  ButtType[MAXBUTTCONFIG];
	uint8  DeviceNum[MAXBUTTCONFIG];
#if SDL_VERSION_ATLEAST(2, 0, 0)
	uint32 ButtonNum[MAXBUTTCONFIG];
#else
	uint16 ButtonNum[MAXBUTTCONFIG];
#endif
	uint32 NumC;
	//uint64 DeviceID[MAXBUTTCONFIG];	/* TODO */
} ButtConfig;


const int INVALID_STATE = 99;

extern CFGSTRUCT DriverConfig[];
extern ARGPSTRUCT DriverArgs[];


extern int isloaded;
// The rate of output and emulated (internal) audio (frequency, in Hz).
extern int em_sound_rate;
// Number of audio samples per frame. Actually NTSC divisor is 60.0988, but since this is used as divisor
// to find out number of frames to skip, higher value will avoid audio buffer overflow.
extern int em_sound_frame_samples;

extern int eoptions;

extern int NoWaiting;
extern CFGSTRUCT InputConfig[];
extern ARGPSTRUCT InputArgs[];

extern bool replaceP2StartWithMicrophone;
extern ButtConfig GamePadConfig[4][10];
//extern ButtConfig powerpadsc[2][12];
//extern ButtConfig QuizKingButtons[6];
//extern ButtConfig FTrainerButtons[12];

extern const char *GamePadNames[GAMEPAD_NUM_BUTTONS];
extern const char *DefaultGamePadDevice[GAMEPAD_NUM_DEVICES];
extern const int DefaultGamePad[GAMEPAD_NUM_DEVICES][GAMEPAD_NUM_BUTTONS];

#if PERI
extern const char *PowerPadNames[POWERPAD_NUM_BUTTONS];
extern const char *DefaultPowerPadDevice[POWERPAD_NUM_DEVICES];
extern const int DefaultPowerPad[POWERPAD_NUM_DEVICES][POWERPAD_NUM_BUTTONS];

extern const char *QuizKingNames[QUIZKING_NUM_BUTTONS];
extern const char *DefaultQuizKingDevice;
extern const int DefaultQuizKing[QUIZKING_NUM_BUTTONS];

extern const char *HyperShotNames[HYPERSHOT_NUM_BUTTONS];
extern const char *DefaultHyperShotDevice;
extern const int DefaultHyperShot[HYPERSHOT_NUM_BUTTONS];

extern const char *MahjongNames[MAHJONG_NUM_BUTTONS];
extern const char *DefaultMahjongDevice;
extern const int DefaultMahjong[MAHJONG_NUM_BUTTONS];

extern const char *TopRiderNames[TOPRIDER_NUM_BUTTONS];
extern const char *DefaultTopRiderDevice;
extern const int DefaultTopRider[TOPRIDER_NUM_BUTTONS];

extern const char *FTrainerNames[FTRAINER_NUM_BUTTONS];
extern const char *DefaultFTrainerDevice;
extern const int DefaultFTrainer[FTRAINER_NUM_BUTTONS];

extern const char *FamilyKeyBoardNames[FAMILYKEYBOARD_NUM_BUTTONS];
extern const char *DefaultFamilyKeyBoardDevice;
extern const int DefaultFamilyKeyBoard[FAMILYKEYBOARD_NUM_BUTTONS];
#endif //PERI

void DoDriverArgs(void);

int InitSound();
void WriteSound(int32 *Buffer, int Count);
int KillSound(void);
int GetSoundBufferCount(void);

// TODO: tsone: huh? remove?
void SilenceSound(int s);

uint32 *GetJSOr(void);

int InitVideo(FCEUGI *gi);
int KillVideo(void);
void BlitScreen(uint8 *XBuf);

void SetOpenGLPalette(uint8 *data);
void BlitOpenGL(uint8 *buf);
void KillOpenGL(void);
int InitOpenGL(int l, int r, int t, int b, double xscale,double yscale, int stretchx, int stretchy);

int FCEUD_NetworkConnect(void);

int LoadGame(const char *path);
int CloseGame(void);
void FCEUD_Update(uint8 *XBuf, int32 *Buffer, int Count);
uint64 FCEUD_GetTime();

void PtoV(int *x, int *y);
bool FCEUD_ShouldDrawInputAids();
bool FCEUI_AviDisableMovieMessages();
bool FCEUI_AviEnableHUDrecording();
void FCEUI_SetAviEnableHUDrecording(bool enable);
bool FCEUI_AviDisableMovieMessages();
void FCEUI_SetAviDisableMovieMessages(bool disable);

Config *InitConfig(void);
void UpdateEMUCore(Config *);

void InitInputInterface(void);
void InputUserActiveFix(void);
void ParseGIInput(FCEUGI *GI);
int ButtonConfigBegin();
void ButtonConfigEnd();
void ConfigButton(char *text, ButtConfig *bc);
int DWaitButton(const uint8 *text, ButtConfig *bc, int wb);

void FCEUD_UpdateInput(void);

void UpdateInput(Config *config);
void InputCfg(const std::string &);


#endif // _EM_H_
