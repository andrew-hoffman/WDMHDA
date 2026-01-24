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
#define DEBUG_LEVEL DEBUGLVL_VERBOSE
#include "ksdebug.h"
#include "debug.h"





/*****************************************************************************
 * Constants
 */

//
// DSP/DMA constants
// 
#define MAXLEN_DMA_BUFFER       0x20000 //128k


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
#define HDA_STREAMBASE(n) ((0x80+(0x20*n)))
#define SDCTL_RUN 0x2
#define SDCTL_IE 0x10

// Node types
#define HDA_OUTPUT_NODE 0x1
#define HDA_INPUT_NODE 0x2

/* PCI space */
#define HDA_PCIREG_TCSEL 0x44

//Defines for Intel SCH HDA snoop control
#define INTEL_SCH_HDA_DEVC 0x78
#define INTEL_SCH_HDA_DEVC_NOSNOOP (1<<11)

/* Defines for ATI HD Audio support in SB450 south bridge */
#define ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR 0x42
#define ATI_SB450_HDAUDIO_ENABLE_SNOOP 0x02

/* Defines for Nvidia HDA support */
#define NVIDIA_HDA_TRANSREG_ADDR 0x4e
#define NVIDIA_HDA_ENABLE_COHBITS 0x0f

typedef struct _REG_BOOL_SETTING
{
    PCWSTR   ValueName;     // Registry value name under \Settings
    BOOLEAN* Variable;      // Storage location to write into
    BOOLEAN  DefaultValue;  // Value used if registry entry is missing/invalid
} REG_BOOL_SETTING;

typedef struct
{
    PWCHAR   KeyName;
    BYTE     RegisterIndex;
    BYTE     RegisterSetting;
} MIXERSETTING,*PMIXERSETTING;

#define MAX_OUTPUT_PATHS 8

typedef struct
{
	ULONG audio_output_node_number;
    ULONG audio_output_node_sample_capabilities;
    ULONG audio_output_node_stream_format_capabilities;
    ULONG output_amp_node_number;
    ULONG output_amp_node_capabilities;
} HDA_NODE_PATH, *PHDA_NODE_PATH;

typedef struct _HDA_OUTPUT_LIST {
    ULONG count;
    HDA_NODE_PATH paths[MAX_OUTPUT_PATHS];
} HDA_OUTPUT_LIST;

typedef enum _HDA_INTERRUPT_TYPE
{
    HDAINT_NONE = 0,               // IRQ not for us (no status bits set)

    /* Controller-level interrupts (GCTL/INTSTS) */
    HDAINT_CONTROLLER,             // Non-stream controller event

    /* Stream-related interrupts */
    HDAINT_STREAM,                 // One or more streams signaled
    HDAINT_STREAM_ERROR,

    /* Fatal / reset conditions */
    HDAINT_FATAL                  // Controller halted or unrecoverable error
} HDA_INTERRUPT_TYPE;

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))


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

	STDMETHOD_(NTSTATUS,ProgramSampleRate)
    (   THIS_
        IN  DWORD           dwSampleRate
    )   PURE;

	STDMETHOD_(UCHAR,hda_is_supported_sample_rate)
    (   THIS_
		IN  ULONG           sample_rate
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
