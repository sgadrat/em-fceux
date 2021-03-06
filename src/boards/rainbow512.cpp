/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2014 CaitSith2
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

/*
 * Roms still using NES 1.0 format should be loaded as 32K CHR RAM.
 * Roms defined under NES 2.0 should use the VRAM size field, defining 7, 8 or 9, based on how much VRAM should be present.
 * UNIF doesn't have this problem, because unique board names can define this information.
 * The UNIF names are UNROM-512-8K, UNROM-512-16K and UNROM-512-32K
 *
 * The battery flag in the NES header enables flash,  Mirrror mode 2 Enables MI_0 and MI_1 mode.
 * Known games to use this board are: 
 *    Battle Kid 2: Mountain of Torment (512K PRG, 8K CHR RAM, Horizontal Mirroring, Flash disabled)
 *    Study Hall (128K PRG (in 512K flash chip), 8K CHR RAM, Horizontal Mirroring, Flash enabled)
 * Although Xmas 2013 uses a different board, where LEDs can be controlled (with writes to the $8000-BFFF space),
 * it otherwise functions identically.
*/

#include "mapinc.h"
#include "../ines.h"
#include "rainbow_esp.h"

#undef RAINBOW_DEBUG
//define RAINBOW_DEBUG

#ifdef RAINBOW_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

#define MAPPER_VERSION		0b01000000

static uint8 latche, latcheinit, bus_conflict, chrram_mask, software_id=false;
static uint16 latcha;
static uint8 *flashdata;
static uint32 *flash_write_count;
static uint8 *FlashPage[32];

static uint8 flash_save=0, flash_state=0, flash_mode=0, flash_bank;
static void (*WLSync)(void);
static void (*WHSync)(void);

/* ESP interface */

static EspFirmware *esp = NULL;
static bool esp_enable = true;
static bool irq_enable = true;

static DECLFW(RAINBOW512Write) {
	UDBG("RAINBOW write %04x %02x\n", A, V);
	esp->rx(V);
}

static DECLFR(RAINBOW512Read) {
	UDBG("RAINBOW read %04x\n", A);
	return esp->tx();
}

static DECLFW(RAINBOW512WriteFlags) {
	UDBG("RAINBOW write %04x %02x\n", A, V);
	esp_enable = V & 0x01;
	irq_enable = V & 0x40;
}

static DECLFR(RAINBOW512ReadFlags) {
	uint8 esp_rts_flag = esp->getGpio4() ? 0x80 : 0x00;
	uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
	uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
	UDBG("RAINBOW read flags %04x => %02x\n", A, esp_rts_flag | esp_enable_flag | irq_enable_flag);
	return esp_rts_flag | esp_enable_flag | irq_enable_flag;
}

static DECLFR(RAINBOW512ReadVersion) {
	return MAPPER_VERSION;
}

static void RAINBOW512MapIrq(int32) {
	if (irq_enable) {
		if (esp->getGpio4()) {
			X6502_IRQBegin(FCEU_IQEXT);
		} else {
			X6502_IRQEnd(FCEU_IQEXT);
		}
	}
}

/* UNROM512 + Rainbow equivalent mapper*/

static INLINE void setfpageptr(int s, uint32 A, uint8 *p) {
	uint32 AB = A >> 11;
	int x;

	if (p)
		for (x = (s >> 1) - 1; x >= 0; x--) {
			FlashPage[AB + x] = p - A;
		}
	else
		for (x = (s >> 1) - 1; x >= 0; x--) {
			FlashPage[AB + x] = 0;
		}
}

static void setfprg16(uint32 A, uint32 V) {
	if (PRGsize[0] >= 16384) {
		V &= PRGmask16[0];
		setfpageptr(16, A, flashdata ? (&flashdata[V << 14]) : 0);
	} else {
		uint32 VA = V << 3;
		int x;

		for (x = 0; x < 8; x++)
			setfpageptr(2, A + (x << 11), flashdata ? (&flashdata[((VA + x) & PRGmask2[0]) << 11]) : 0);
	}
}

static void inc_flash_write_count(uint8 bank, uint32 A)
{
	flash_write_count[(bank*4) + ((A&0x3000)>>12)]++;
	if(!flash_write_count[(bank*4) + ((A&0x3000)>>12)])
		flash_write_count[(bank*4) + ((A&0x3000)>>12)]++;
}

static uint32 GetFlashWriteCount(uint8 bank, uint32 A)
{
	return flash_write_count[(bank*4) + ((A&0x3000)>>12)];
}

static void StateRestore(int version) {
	WHSync();
}

static DECLFW(UNROM512LLatchWrite)
{
	latche = V;
	latcha = A;
	WLSync();
}

static DECLFW(UNROM512HLatchWrite)
{
	if (bus_conflict)
		latche = (V == CartBR(A)) ? V : 0;
	else
		latche = V;
	latcha = A;
	WHSync();
}

static DECLFR(UNROM512LatchRead)
{
	uint8 flash_id[3]={0xB5,0xB6,0xB7};
	if(software_id)
	{
		if(A&1)
			return flash_id[ROM_size>>4];
		else
			return 0xBF;
	}
	if(flash_save)
	{
		if(A < 0xC000)
		{
			if(GetFlashWriteCount(flash_bank,A))
				return FlashPage[A >> 11][A];
		}
		else
		{
			if(GetFlashWriteCount(ROM_size-1,A))
				return FlashPage[A >> 11][A];
		}
	}
	return Page[A >> 11][A];
}

static void UNROM512LatchPower(void) {
	UDBG("Rainbow512 power\n");

	latche = latcheinit;
	WHSync();
	SetReadHandler(0x8000, 0xFFFF, UNROM512LatchRead);
	if(!flash_save)
		SetWriteHandler(0x8000, 0xFFFF, UNROM512HLatchWrite);
	else
	{
		SetWriteHandler(0x8000,0xBFFF,UNROM512LLatchWrite);
		SetWriteHandler(0xC000,0xFFFF,UNROM512HLatchWrite);
	}

	SetWriteHandler(0x5000, 0x5000, RAINBOW512Write);
	SetReadHandler(0x5000, 0x5000, RAINBOW512Read);
	SetWriteHandler(0x5001, 0x5001, RAINBOW512WriteFlags);
	SetReadHandler(0x5001, 0x5001, RAINBOW512ReadFlags);
	SetReadHandler(0x5C03, 0x5C03, RAINBOW512ReadVersion);

	esp = new BrokeStudioFirmware;
	esp_enable = true;
	irq_enable = true;
}

static void UNROM512LatchClose(void) {
	UDBG("Rainbow512 close\n");
	if(flash_write_count)
		FCEU_gfree(flash_write_count);
	if(flashdata)
		FCEU_gfree(flashdata);
	if (esp)
		delete esp;
	flash_write_count = NULL;
	flashdata = NULL;
	esp = NULL;
}


static void UNROM512LSync() {
	int erase_a[5]={0x9555,0xAAAA,0x9555,0x9555,0xAAAA};
	int erase_d[5]={0xAA,0x55,0x80,0xAA,0x55};
	int erase_b[5]={1,0,1,1,0};

	if(flash_mode==0)
	{
		if((latcha == erase_a[flash_state]) && (latche == erase_d[flash_state]) && (flash_bank == erase_b[flash_state]))
		{
			flash_state++;
			if(flash_state == 5)
			{
				flash_mode=1;
			}
		}
		else if ((flash_state==2)&&(latcha==0x9555)&&(latche==0xA0)&&(flash_bank==1))
		{
			flash_state++;
			flash_mode=2;
		}
		else if ((flash_state==2)&&(latcha==0x9555)&&(latche==0x90)&&(flash_bank==1))
		{
			flash_state=0;
			software_id=true;
		}
		else
		{
			if(latche==0xF0)
				software_id=false;
			flash_state=0;
		}
	}
	else if(flash_mode==1)	//Chip Erase or Sector Erase
	{
		if(latche==0x30)
		{
			inc_flash_write_count(flash_bank,latcha);
			memset(&FlashPage[(latcha & 0xF000) >> 11][latcha & 0xF000],0xFF,0x1000);
		}
		else if (latche==0x10)
		{
			for(uint32 i=0;i<(ROM_size*4);i++)
				inc_flash_write_count(i>>2,i<<12);
			memset(flashdata,0xFF,ROM_size*0x4000);	//Erasing the rom chip as instructed. Crash rate calulated to be 99.9% :)
		}
		flash_state=0;
		flash_mode=0;
	}
	else if(flash_mode==2)	//Byte Program
	{
		if(!GetFlashWriteCount(flash_bank,latcha))
		{
			inc_flash_write_count(flash_bank,latcha);
			memcpy(&FlashPage[(latcha & 0xF000) >> 11][latcha & 0xF000],&Page[(latcha & 0xF000)>>11][latcha & 0xF000],0x1000);
		}
		FlashPage[latcha>>11][latcha]&=latche;
		flash_state=0;
		flash_mode=0;
	}
}

static void UNROM512HSync()
{
	flash_bank=latche&(ROM_size-1);
	
	setprg16(0x8000, flash_bank);
	setprg16(0xc000, ~0);
	setfprg16(0x8000, flash_bank);
	setfprg16(0xC000, ~0);
	setchr8r(0, (latche & chrram_mask) >> 5);
	setmirror(MI_0+(latche>>7));
}

void RAINBOW512_Init(CartInfo *info) {
	UDBG("Rainbow512 init\n");

	flash_state=0;
	flash_bank=0;
	flash_save=info->battery;

#if 0
	if(info->vram_size == 8192)
		chrram_mask = 0;
	else if (info->vram_size == 16384)
		chrram_mask = 0x20;
	else
		chrram_mask = 0x60;
#else
	// info->vram_size does not exists in fceux 2.2.2
	//   Assume the common value of 0 for iNES Flags 8 (meaning 8k RAM)
	chrram_mask = 0;
#endif

	int mirror = (head.ROM_type & 1) | ((head.ROM_type & 8) >> 2);
	switch (mirror)
	{
	case 0: // hard horizontal, internal
		SetupCartMirroring(MI_H, 1, NULL);
		break;
	case 1: // hard vertical, internal
		SetupCartMirroring(MI_V, 1, NULL);
		break;
	case 2: // switchable 1-screen, internal (flags: 4-screen + horizontal)
		SetupCartMirroring(MI_0, 0, NULL);
		break;
	case 3: // hard four screen, last 8k of 32k RAM (flags: 4-screen + vertical)
#if 0
		SetupCartMirroring(   4, 1, VROM + (info->vram_size - 8192));
#else
		// Not sure if we can support it easily, just fallback to something random
		SetupCartMirroring(MI_0, 0, NULL);
#endif
		break;
	}

	bus_conflict = !info->battery;
	latcheinit = 0;
	WLSync = UNROM512LSync;
	WHSync = UNROM512HSync;
	info->Power = UNROM512LatchPower;
	info->Close = UNROM512LatchClose;
	GameStateRestore = StateRestore;
	if(flash_save)
	{
		flashdata = (uint8*)FCEU_gmalloc(ROM_size*0x4000);
		flash_write_count = (uint32*)FCEU_gmalloc(ROM_size*4*sizeof(uint32));
		info->SaveGame[0] = (uint8*)flash_write_count;
		info->SaveGame[1] = flashdata;
		info->SaveGameLen[0] = ROM_size*4*sizeof(uint32);
		info->SaveGameLen[1] = ROM_size*0x4000;
		AddExState(flash_write_count,ROM_size*4*sizeof(uint32),0,"FLASH_WRITE_COUNT");
		AddExState(flashdata,ROM_size*0x4000,0,"FLASH_DATA");
		AddExState(&flash_state,1,0,"FLASH_STATE");
		AddExState(&flash_mode,1,0,"FLASH_MODE");
		AddExState(&flash_bank,1,0,"FLASH_BANK");
		AddExState(&latcha,2,0,"LATA");
	}
	AddExState(&latche, 1, 0, "LATC");
	AddExState(&bus_conflict, 1, 0, "BUSC");

	// Set a hook on hblank to be able periodically check if we have to send an interupt
	MapIRQHook = RAINBOW512MapIrq;
}
