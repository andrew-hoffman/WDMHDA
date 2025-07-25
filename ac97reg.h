/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

#ifndef _AC97REG_H_
#define _AC97REG_H_

// We use enum types cause the compiler can check variable passing if it is
// an enum (otherwise you could pass any value). That doesn't save us from
// doing reasonable run time checks in that range.
enum AC97Register
{
	AC97REG_RESET = 0,
	AC97REG_MASTER_VOLUME,
	AC97REG_HPHONE_VOLUME,
	AC97REG_MMONO_VOLUME,
	AC97REG_MASTER_TONE,
	AC97REG_BEEP_VOLUME,
	AC97REG_PHONE_VOLUME,
	AC97REG_MIC_VOLUME,
	AC97REG_LINE_IN_VOLUME,
	AC97REG_CD_VOLUME,
	AC97REG_VIDEO_VOLUME,
	AC97REG_AUX_VOLUME,
	AC97REG_PCM_OUT_VOLUME,
	AC97REG_RECORD_SELECT,
	AC97REG_RECORD_GAIN,
	AC97REG_RECORD_GAIN_MIC,
	AC97REG_GENERAL,
	AC97REG_3D_CONTROL,
	AC97REG_RESERVED,
	AC97REG_POWERDOWN,

    // AC97-2.0 registers
	AC97REG_EXT_AUDIO_ID,
	AC97REG_EXT_AUDIO_CTRL,
	AC97REG_PCM_FRONT_RATE,
	AC97REG_PCM_SURR_RATE,
	AC97REG_PCM_LFE_RATE,
	AC97REG_PCM_LR_RATE,
	AC97REG_MIC_RATE,
	AC97REG_6CH_VOL_CLFE,
	AC97REG_6CH_VOL_LRSURR,
	AC97REG_RESERVED2,

    // Modem registers from 0x3C to 0x58 (next 15 enums)
    // Vendor Reserved = 0x5A-0x7A (next 16 enums)

	// Vendor IDs
	AC97REG_VENDOR_ID1 = 0x3E,      // thats register address 0x7C
	AC97REG_VENDOR_ID2,

    // Defines an invalid register. Likewise, this is the highest
    // possible value that can be used.
	AC97REG_INVALID
};

#if (DBG)
// Note: This array only has the first 29 registers defined.
//       There are many more.
const PCHAR RegStrings[] =
{
    "REG_RESET",
    "REG_MASTER_VOLUME",
    "REG_HPHONE_VOLUME",
    "REG_MMONO_VOLUME",
    "REG_MASTER_TONE",
    "REG_BEEP_VOLUME",
    "REG_PHONE_VOLUME",
    "REG_MIC_VOLUME",
    "REG_LINEIN_VOLUME",
    "REG_CD_VOLUME",
    "REG_VIDEO_VOLUME",
    "REG_AUX_VOLUME",
    "REG_PCMOUT_VOLUME",
    "REG_RECORD_SELECT",
    "REG_RECORD_GAIN",
    "REG_RECORD_GAIN_MIC",
    "REG_GENERAL",
    "REG_3D_CONTROL",
    "REG_RESERVED",
    "REG_POWERDOWN",
    "REG_EXT_AUDIO_ID",
    "REG_EXT_AUDIO_CTRL",
    "REG_PCM_FRONT_RATE",
    "REG_PCM_SURR_RATE",
    "REG_PCM_LFE_RATE",
    "REG_PCM_LR_RATE",
    "REG_MIC_RATE",
    "REG_6CH_VOL_CLFE",
    "REG_6CH_VOL_LRSURR",
    "REG_RESERVED2"
};
#endif

// This array maps the node controls to the AC97 registers.
// E.g. if you mute the master volume control you should modify AC97
// register AC97REG_MASTER_VOLUME
typedef struct {
    AC97Register	reg;	// we would only need one byte, but enums are int
    WORD            mask;   // registers are 16 bit.
} tMapNodeToReg;

const tMapNodeToReg stMapNodeToReg[] =
{
    // TODO: loopback
    {AC97REG_PCM_OUT_VOLUME, 0x1F1F},   // NODE_WAVEOUT_VOLUME
    {AC97REG_PCM_OUT_VOLUME, 0x8000},   // NODE_WAVEOUT_MUTE
    {AC97REG_GENERAL, 0x8000},          // NODE_VIRT_WAVEOUT_3D_BYPASS
    {AC97REG_BEEP_VOLUME, 0x001E},      // NODE_PCBEEP_VOLUME
    {AC97REG_BEEP_VOLUME, 0x8000},      // NODE_PCBEEP_MUTE
    {AC97REG_PHONE_VOLUME, 0x001F},     // NODE_PHONE_VOLUME
    {AC97REG_PHONE_VOLUME, 0x8000},     // NODE_PHONE_MUTE
    {AC97REG_GENERAL, 0x0100},          // NODE_MIC_SELECT
    {AC97REG_MIC_VOLUME, 0x0040},       // NODE_MIC_BOOST
    {AC97REG_MIC_VOLUME, 0x001F},       // NODE_MIC_VOLUME
    {AC97REG_MIC_VOLUME, 0x8000},       // NODE_MIC_MUTE
    {AC97REG_LINE_IN_VOLUME, 0x1F1F},   // NODE_LINEIN_VOLUME
    {AC97REG_LINE_IN_VOLUME, 0x8000},   // NODE_LINEIN_MUTE
    {AC97REG_CD_VOLUME, 0x1F1F},        // NODE_CD_VOLUME
    {AC97REG_CD_VOLUME, 0x8000},        // NODE_CD_MUTE
    {AC97REG_VIDEO_VOLUME, 0x1F1F},     // NODE_VIDEO_VOLUME
    {AC97REG_VIDEO_VOLUME, 0x8000},     // NODE_VIDEO_MUTE
    {AC97REG_AUX_VOLUME, 0x1F1F},       // NODE_AUX_VOLUME
    {AC97REG_AUX_VOLUME, 0x8000},       // NODE_AUX_MUTE
    {AC97REG_INVALID, 0x0000},          // NODE_MAIN_MIX doesn't has controls
    {AC97REG_3D_CONTROL, 0x0F00},       // NODE_VIRT_3D_CENTER
    {AC97REG_3D_CONTROL, 0x000F},       // NODE_VIRT_3D_DEPTH
    {AC97REG_GENERAL, 0x2000},          // NODE_VIRT_3D_ENABLE
    {AC97REG_INVALID, 0x0000},          // NODE_BEEP_MIX doesn't has controls
    {AC97REG_MASTER_TONE, 0x0F00},      // NODE_BASS
    {AC97REG_MASTER_TONE, 0x000F},      // NODE_TREBLE
    {AC97REG_GENERAL, 0x1000},          // NODE_LOUDNESS
    {AC97REG_GENERAL, 0x4000},          // NODE_SIMUL_STEREO
    {AC97REG_MASTER_VOLUME, 0x3F3F},    // NODE_MASTEROUT_VOLUME
    {AC97REG_MASTER_VOLUME, 0x8000},    // NODE_MASTEROUT_MUTE
    {AC97REG_HPHONE_VOLUME, 0x3F3F},    // NODE_HPHONE_VOLUME
    {AC97REG_HPHONE_VOLUME, 0x8000},    // NODE_HPHONE_MUTE
    {AC97REG_GENERAL, 0x0200},          // NODE_MONOOUT_SELECT
    {AC97REG_MMONO_VOLUME, 0x803F},     // NODE_VIRT_MONOOUT_VOLUME1
    {AC97REG_MMONO_VOLUME, 0x803F},     // NODE_VIRT_MONOOUT_VOLUME2
    {AC97REG_RECORD_SELECT, 0x0707},    // NODE_WAVEIN_SELECT
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME1
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME2
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME3
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME4
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME5
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME6
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME7
    {AC97REG_RECORD_GAIN, 0x0F0F},      // NODE_VIRT_MASTER_INPUT_VOLUME8
    {AC97REG_RECORD_GAIN_MIC, 0x000F},  // NODE_MICIN_VOLUME
    {AC97REG_RECORD_GAIN_MIC, 0x8000},  // NODE_MICIN_MUTE
};

#endif  //_AC97REG_H_
