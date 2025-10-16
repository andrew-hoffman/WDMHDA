/*****************************************************************************
 * common.h - Common code used by all the HDA miniports.
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation.  All Rights Reserved.
 *
 * A combination of random functions that are used by all the miniports.
 * This class also handles all the interrupts for the card.
 *
 */

/*
 * THIS IS A BIT BROKEN FOR NOW.  IT USES A SINGLETON OBJECT FOR WHICH THERE
 * IS ONLY ONE INSTANCE PER DRIVER.  THIS MEANS THERE CAN ONLY ONE CARD
 * SUPPORTED IN ANY GIVEN MACHINE.  THIS WILL BE FIXED.
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#define PC_NEW_NAMES    1

#include "portcls.h"
#include "DMusicKS.h"

//get all debug prints
#define DEBUG_LEVEL DEBUGLVL_BLAB
#include "ksdebug.h"
#include "debug.h"





/*****************************************************************************
 * Constants
 */

//
// DSP/DMA constants
// 
#define MAXLEN_DMA_BUFFER       0x4000

#define DSP_REG_CMSD0           0x00
#define DSP_REG_CMSR0           0x01
#define DSP_REG_CMSD1           0x02
#define DSP_REG_CMSR1           0x03
#define DSP_REG_MIXREG          0x04
#define DSP_REG_MIXDATA         0x05
#define DSP_REG_RESET           0x06
#define DSP_REG_FMD0            0x08
#define DSP_REG_FMR0            0x09
#define DSP_REG_READ            0x0A
#define DSP_REG_WRITE           0x0C
#define DSP_REG_DATAAVAIL       0x0E

#define DSP_REG_ACK8BIT         0x0E
#define DSP_REG_ACK16BIT        0x0F

//
// controller commands
//
#define DSP_CMD_WAVEWRPIO       0x10  // wave output (programmed I/O)
#define DSP_CMD_WAVEWR          0x14  // interrupt-driven 8 bit linear wave output
#define DSP_CMD_WAVEWRA         0x1C  // auto mode 8 bit out
#define DSP_CMD_WAVERD          0x24  // interrupt-driven 8 bit linear wave input
#define DSP_CMD_WAVERDA         0x2C  // auto mode 8 bit in
#define DSP_CMD_WAVEWRHS        0x90  // high speed mode write
#define DSP_CMD_WAVERDHS        0x98  // high speed mode read
#define DSP_CMD_SETSAMPRATE     0x40  // set sample rate
#define DSP_CMD_SETBLCKSIZE     0x48  // set block size
#define DSP_CMD_SPKRON          0xD1  // speaker on
#define DSP_CMD_SPKROFF         0xD3  // speaker off
#define DSP_CMD_SPKRSTATUS      0xD8  // speaker status (0=off, FF=on)
#define DSP_CMD_PAUSEDMA        0xD0  // pause DMA
#define DSP_CMD_CONTDMA         0xD4  // continue DMA
#define DSP_CMD_HALTAUTODMA     0xDA  // stop DMA autoinit mode
#define DSP_CMD_INVERTER        0xE0  // byte inverter
#define DSP_CMD_GETDSPVER       0xE1  // get dsp version
#define DSP_CMD_GENERATEINT     0xF2  // cause sndblst to generate an interrupt.

//
// SB-16 support
//
#define DSP_CMD_SETDACRATE      0x41  // set SBPro-16 DAC rate
#define DSP_CMD_SETADCRATE      0x42  // set SBPro-16 ADC rate
#define DSP_CMD_STARTDAC16      0xB6  // start 16-bit DAC
#define DSP_CMD_STARTADC16      0xBE  // start 16-bit ADC
#define DSP_CMD_STARTDAC8       0xC6  // start 8-bit DAC
#define DSP_CMD_STARTADC8       0xCE  // start 8-bit ADC
#define DSP_CMD_PAUSEDMA16      0xD5  // pause 16-bit DMA
#define DSP_CMD_CONTDMA16       0xD6  // continue 16-bit DMA
#define DSP_CMD_HALTAUTODMA16   0xD9  // halt 16-bit DMA

//
// Indexed mixer registers
//
#define DSP_MIX_DATARESETIDX    0x00

#define DSP_MIX_MASTERVOLIDX_L  0x00
#define DSP_MIX_MASTERVOLIDX_R  0x01
#define DSP_MIX_VOICEVOLIDX_L   0x02
#define DSP_MIX_VOICEVOLIDX_R   0x03
#define DSP_MIX_FMVOLIDX_L      0x04
#define DSP_MIX_FMVOLIDX_R      0x05
#define DSP_MIX_CDVOLIDX_L      0x06
#define DSP_MIX_CDVOLIDX_R      0x07
#define DSP_MIX_LINEVOLIDX_L    0x08
#define DSP_MIX_LINEVOLIDX_R    0x09
#define DSP_MIX_MICVOLIDX       0x0A
#define DSP_MIX_SPKRVOLIDX      0x0B
#define DSP_MIX_OUTMIXIDX       0x0C
#define DSP_MIX_ADCMIXIDX_L     0x0D
#define DSP_MIX_ADCMIXIDX_R     0x0E
#define DSP_MIX_INGAINIDX_L     0x0F
#define DSP_MIX_INGAINIDX_R     0x10
#define DSP_MIX_OUTGAINIDX_L    0x11
#define DSP_MIX_OUTGAINIDX_R    0x12
#define DSP_MIX_AGCIDX          0x13
#define DSP_MIX_TREBLEIDX_L     0x14
#define DSP_MIX_TREBLEIDX_R     0x15
#define DSP_MIX_BASSIDX_L       0x16
#define DSP_MIX_BASSIDX_R       0x17

#define DSP_MIX_BASEIDX         0x30
#define DSP_MIX_MAXREGS         (DSP_MIX_BASSIDX_R + 1)

#define DSP_MIX_IRQCONFIG       0x80
#define DSP_MIX_DMACONFIG       0x81

//
// Bit layout for DSP_MIX_OUTMIXIDX.
//
#define MIXBIT_MIC_LINEOUT      0
#define MIXBIT_CD_LINEOUT_R     1
#define MIXBIT_CD_LINEOUT_L     2
#define MIXBIT_LINEIN_LINEOUT_R 3
#define MIXBIT_LINEIN_LINEOUT_L 4

//
// Bit layout for DSP_MIX_ADCMIXIDX_L and DSP_MIX_ADCMIXIDX_R.
//
#define MIXBIT_MIC_WAVEIN       0
#define MIXBIT_CD_WAVEIN_R      1
#define MIXBIT_CD_WAVEIN_L      2
#define MIXBIT_LINEIN_WAVEIN_R  3
#define MIXBIT_LINEIN_WAVEIN_L  4
#define MIXBIT_SYNTH_WAVEIN_R   5
#define MIXBIT_SYNTH_WAVEIN_L   6

//
// Bit layout for MIXREG_MIC_AGC
//
#define MIXBIT_MIC_AGC          0

//
// MPU401 ports
//
#define MPU401_REG_STATUS   0x01    // Status register
#define MPU401_DRR          0x40    // Output ready (for command or data)
#define MPU401_DSR          0x80    // Input ready (for data)

#define MPU401_REG_DATA     0x00    // Data in
#define MPU401_REG_COMMAND  0x01    // Commands
#define MPU401_CMD_RESET    0xFF    // Reset command
#define MPU401_CMD_UART     0x3F    // Switch to UART mod

/*****************************************************************************
 * Defines for HD Audio
 *****************************************************************************/

// Communication types
#define HDA_UNINITIALIZED 0
#define HDA_CORB_RIRB 1
#define HDA_PIO 2

// Widget types
#define HDA_WIDGET_AUDIO_OUTPUT 0x0
#define HDA_WIDGET_AUDIO_INPUT 0x1
#define HDA_WIDGET_AUDIO_MIXER 0x2
#define HDA_WIDGET_AUDIO_SELECTOR 0x3
#define HDA_WIDGET_PIN_COMPLEX 0x4
#define HDA_WIDGET_POWER_WIDGET 0x5
#define HDA_WIDGET_VOLUME_KNOB 0x6
#define HDA_WIDGET_BEEP_GENERATOR 0x7
#define HDA_WIDGET_VENDOR_DEFINED 0xF

// Pin definitions
#define HDA_PIN_LINE_OUT 0x0
#define HDA_PIN_SPEAKER 0x1
#define HDA_PIN_HEADPHONE_OUT 0x2
#define HDA_PIN_CD 0x3
#define HDA_PIN_SPDIF_OUT 0x4
#define HDA_PIN_DIGITAL_OTHER_OUT 0x5
#define HDA_PIN_MODEM_LINE_SIDE 0x6
#define HDA_PIN_MODEM_HANDSET_SIDE 0x7
#define HDA_PIN_LINE_IN 0x8
#define HDA_PIN_AUX 0x9
#define HDA_PIN_MIC_IN 0xA
#define HDA_PIN_TELEPHONY 0xB
#define HDA_PIN_SPDIF_IN 0xC
#define HDA_PIN_DIGITAL_OTHER_IN 0xD
#define HDA_PIN_RESERVED 0xE
#define HDA_PIN_OTHER 0xF

// Node types
#define HDA_OUTPUT_NODE 0x1
#define HDA_INPUT_NODE 0x2

typedef struct
{
    PWCHAR   KeyName;
    BYTE     RegisterIndex;
    BYTE     RegisterSetting;
} MIXERSETTING,*PMIXERSETTING;

DEFINE_GUID(IID_IAdapterCommon,
0x7eda2950, 0xbf9f, 0x11d0, 0x87, 0x1f, 0x0, 0xa0, 0xc9, 0x11, 0xb5, 0x44);

/*****************************************************************************
 * IAdapterCommon
 *****************************************************************************
 * Interface for adapter common object.
 */
DECLARE_INTERFACE_(IAdapterCommon,IUnknown)
{
    STDMETHOD_(NTSTATUS,Init)
    (   THIS_
        IN      PRESOURCELIST   ResourceList,
        IN      PDEVICE_OBJECT  DeviceObject
    )   PURE;
    
    STDMETHOD_(PINTERRUPTSYNC,GetInterruptSync)
    (   THIS
    )   PURE;

    STDMETHOD_(PUNKNOWN *,WavePortDriverDest)
    (   THIS
    )   PURE;

    STDMETHOD_(PUNKNOWN *,MidiPortDriverDest)
    (   THIS
    )   PURE;

    STDMETHOD_(void,SetWaveServiceGroup)
    (   THIS_
        IN      PSERVICEGROUP   ServiceGroup
    )   PURE;

    STDMETHOD_(BYTE,ReadController)
    (   THIS
    )   PURE;

    STDMETHOD_(BOOLEAN,WriteController)
    (   THIS_
        IN      BYTE    Value
    )   PURE;

    STDMETHOD_(NTSTATUS,ResetController)
    (   THIS
    )   PURE;

	STDMETHOD_(void,hda_start_sound)
    (   THIS
    )   PURE;

	STDMETHOD_(void,hda_stop_sound)
    (   THIS
    )   PURE;

	STDMETHOD_(NTSTATUS,hda_stop_stream)
    (   THIS
    )   PURE;

	STDMETHOD_(ULONG,hda_get_actual_stream_position)
    (   THIS
    )   PURE;

	STDMETHOD_(NTSTATUS,hda_showtime)
    (   THIS_
        IN      PDMACHANNEL DmaChannel
    )   PURE;

    STDMETHOD_(void,MixerRegWrite)
    (   THIS_
        IN      BYTE    Index,
        IN      BYTE    Value
    )   PURE;
    
    STDMETHOD_(BYTE,MixerRegRead)
    (   THIS_
        IN      BYTE    Index
    )   PURE;

    STDMETHOD_(void,MixerReset)
    (   THIS
    )   PURE;

    STDMETHOD(RestoreMixerSettingsFromRegistry)
    (   THIS
    )   PURE;

    STDMETHOD(SaveMixerSettingsToRegistry)
    (   THIS
    )   PURE;
};

typedef IAdapterCommon *PADAPTERCOMMON;

/*****************************************************************************
 * NewAdapterCommon()
 *****************************************************************************
 * Create a new adapter common object.
 */
NTSTATUS
NewAdapterCommon
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);

#endif  //_COMMON_H_
