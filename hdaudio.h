/********************************************************************************
**    Copyright (c) 2025 HD Audio Driver Project. All Rights Reserved.
**
**    Ported from BleskOS HD Audio Driver to Windows Driver Model
**
********************************************************************************/

#ifndef _HDAUDIO_H_
#define _HDAUDIO_H_

#include <portcls.h>
#include <stdunk.h>
#include "debug.h"
#include "shared.h"

/*****************************************************************************
 * Defines for HD Audio
 *****************************************************************************/

// Maximum number of HDA sound cards supported
#define MAX_NUMBER_OF_HDA_SOUND_CARDS 4

// Communication types
#define HDA_UNINITALIZED 0
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

/*****************************************************************************
 * HD Audio register definitions
 *****************************************************************************/

// HD Audio controller registers will be defined here
// These would be ported from the BleskOS driver implementation

/*****************************************************************************
 * Structures
 *****************************************************************************/

// PCI device info structure (WDM style)
typedef struct _PCI_DEVICE_INFO {
    ULONG VendorId;
    ULONG DeviceId;
    ULONG BusNumber;
    ULONG DeviceFunction;
    PVOID MappedResources;
} PCI_DEVICE_INFO, *PPCI_DEVICE_INFO;

// HD Audio info structure (WDM style)
typedef struct _HDA_DEVICE_EXTENSION {
    // PCI device information
    PCI_DEVICE_INFO PciInfo;

    // Base addresses and stream information
    ULONG Base;
    ULONG InputStreamBase;
    ULONG OutputStreamBase;
    ULONG CommunicationType;
    ULONG CodecNumber;
    ULONG IsInitalizedUsefulOutput;
    ULONG SelectedOutputNode;

    // CORB/RIRB buffers
    PULONG CorbMem;
    ULONG CorbPointer;
    ULONG CorbNumberOfEntries;
    PULONG RirbMem;
    ULONG RirbPointer;
    ULONG RirbNumberOfEntries;

    // Output buffer information
    PULONG OutputBufferList;
    ULONG SoundLength;
    ULONG BytesOnOutputForStoppingSound;
    ULONG LengthOfNodePath;

    // AFG (Audio Function Group) node capabilities
    ULONG AfgNodeSampleCapabilities;
    ULONG AfgNodeStreamFormatCapabilities;
    ULONG AfgNodeInputAmpCapabilities;
    ULONG AfgNodeOutputAmpCapabilities;

    // Audio output nodes
    ULONG AudioOutputNodeNumber;
    ULONG AudioOutputNodeSampleCapabilities;
    ULONG AudioOutputNodeStreamFormatCapabilities;

    ULONG OutputAmpNodeNumber;
    ULONG OutputAmpNodeCapabilities;

    // Secondary audio output nodes
    ULONG SecondAudioOutputNodeNumber;
    ULONG SecondAudioOutputNodeSampleCapabilities;
    ULONG SecondAudioOutputNodeStreamFormatCapabilities;
    ULONG SecondOutputAmpNodeNumber;
    ULONG SecondOutputAmpNodeCapabilities;

    // Pin nodes
    ULONG PinOutputNodeNumber;
    ULONG PinHeadphoneNodeNumber;

    // Resources
    PDEVICE_OBJECT PhysicalDeviceObject;
    PDEVICE_OBJECT FunctionalDeviceObject;
    PADAPTERCOMMON AdapterCommon;
    
    // Synchronization
    KSPIN_LOCK HardwareLock;
    
    // Current state information
    BOOLEAN PowerState;
    BOOLEAN HeadphoneConnected;
    
    // Selected card
    ULONG SelectedHdaCard;
} HDA_DEVICE_EXTENSION, *PHDA_DEVICE_EXTENSION;

/*****************************************************************************
 * Interface Definitions
 *****************************************************************************/

/*****************************************************************************
 * IHdaCodec
 *****************************************************************************
 * Interface for HD Audio codec operations.
 */
DECLARE_INTERFACE_(IHdaCodec, IUnknown)
{
    STDMETHOD_(NTSTATUS, Init)
    (   THIS_
        IN      PRESOURCELIST   ResourceList,
        IN      PDEVICE_OBJECT  DeviceObject
    )   PURE;
    
    STDMETHOD_(NTSTATUS, SendVerb)
    (   THIS_
        IN      ULONG   Codec,
        IN      ULONG   Node,
        IN      ULONG   Verb,
        IN      ULONG   Command,
        OUT     PULONG  Response
    )   PURE;
    
    STDMETHOD_(UCHAR, GetNodeType)
    (   THIS_
        IN      ULONG   Codec,
        IN      ULONG   Node
    )   PURE;
    
    STDMETHOD_(USHORT, GetNodeConnectionEntry)
    (   THIS_
        IN      ULONG   Codec,
        IN      ULONG   Node,
        IN      ULONG   ConnectionEntryNumber
    )   PURE;
    
    STDMETHOD_(NTSTATUS, SetNodeGain)
    (   THIS_
        IN      ULONG   Codec,
        IN      ULONG   Node,
        IN      ULONG   NodeType,
        IN      ULONG   Capabilities,
        IN      ULONG   Gain
    )   PURE;
    
    STDMETHOD_(NTSTATUS, EnablePinOutput)
    (   THIS_
        IN      ULONG   Codec,
        IN      ULONG   PinNode
    )   PURE;
    
    STDMETHOD_(NTSTATUS, DisablePinOutput)
    (   THIS_
        IN      ULONG   Codec,
        IN      ULONG   PinNode
    )   PURE;
    
    STDMETHOD_(BOOLEAN, IsHeadphoneConnected)
    (   THIS_
        void
    )   PURE;
    
    STDMETHOD_(NTSTATUS, InitializeCodec)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   CodecNumber
    )   PURE;
    
    STDMETHOD_(NTSTATUS, InitializeAudioFunctionGroup)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   AfgNodeNumber
    )   PURE;
    
    STDMETHOD_(NTSTATUS, InitializeOutputPin)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   PinNodeNumber
    )   PURE;
    
    STDMETHOD_(NTSTATUS, InitializeAudioOutput)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   AudioOutputNodeNumber
    )   PURE;
    
    STDMETHOD_(NTSTATUS, SetVolume)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   Volume
    )   PURE;
    
    STDMETHOD_(NTSTATUS, CheckHeadphoneConnectionChange)
    (   THIS_
        void
    )   PURE;
    
    STDMETHOD_(BOOLEAN, IsSupportedChannelSize)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      UCHAR   Size
    )   PURE;
    
    STDMETHOD_(BOOLEAN, IsSupportedSampleRate)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   SampleRate
    )   PURE;
    
    STDMETHOD_(USHORT, GetSoundDataFormat)
    (   THIS_
        IN      ULONG   SampleRate,
        IN      ULONG   Channels,
        IN      ULONG   BitsPerSample
    )   PURE;
    
    STDMETHOD_(NTSTATUS, PlayPcmDataInLoop)
    (   THIS_
        IN      ULONG   SoundCardNumber,
        IN      ULONG   SampleRate
    )   PURE;
    
    STDMETHOD_(NTSTATUS, StopSound)
    (   THIS_
        IN      ULONG   SoundCardNumber
    )   PURE;
    
    STDMETHOD_(ULONG, GetActualStreamPosition)
    (   THIS_
        IN      ULONG   SoundCardNumber
    )   PURE;
};

typedef IHdaCodec *PHDACODEC;

/*****************************************************************************
 * Function Prototypes
 *****************************************************************************/

/*****************************************************************************
 * NewHdaCodec()
 *****************************************************************************
 * Create a new HD Audio codec object.
 */
NTSTATUS NewHdaCodec
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);

/*****************************************************************************
 * NewAdapterHda()
 *****************************************************************************
 * Create a new HD Audio adapter object.
 */
NTSTATUS NewAdapterHda
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);

/*****************************************************************************
 * CreateMiniportWaveHda()
 *****************************************************************************
 * Create a new HD Audio wave miniport.
 */
NTSTATUS CreateMiniportWaveHda
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);

/*****************************************************************************
 * CreateMiniportTopologyHda()
 *****************************************************************************
 * Create a new HD Audio topology miniport.
 */
NTSTATUS CreateMiniportTopologyHda
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);

/*****************************************************************************
 * Interface GUIDs
 *****************************************************************************/

// {D7581D51-4220-4D65-8270-A83A96C7985E}
DEFINE_GUID(IID_IHdaCodec, 
0xd7581d51, 0x4220, 0x4d65, 0x82, 0x70, 0xa8, 0x3a, 0x96, 0xc7, 0x98, 0x5e);

#endif // _HDAUDIO_H_

