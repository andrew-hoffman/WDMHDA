/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

#ifndef _COMMON_H_
#define _COMMON_H_

#include "shared.h"

/*****************************************************************************
 * Defines for HD Audio
 *****************************************************************************/

// Communication types
#define HDA_UNinitializeD 0
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
 * Structs
 *****************************************************************************
 */
 
//                                              
// Contains pin and node configuration 
typedef struct
{
    // For nodes.
    struct
    {
        BOOL	bNodeConfig;
    } Nodes[NODEC_TOP_ELEMENT];

    // For pins.
    struct
    {
        BOOL	bPinConfig;
        PWCHAR  sRegistryName;
    } Pins[PINC_TOP_ELEMENT];
} tHardwareConfig;

//
// We cache the AC97 registers.  Additionally, we want some default values
// when the driver comes up first that are different from the HW default
// values. The string in the structure is the name of the registry entry
// that can be used instead of the hard coded default value.
//
typedef struct
{
    WORD    wCache;
    WORD    wFlags;
    PWCHAR  sRegistryName;
    WORD    wWantedDefault;
} tAC97Registers;

/*****************************************************************************
 * Structures
 *****************************************************************************/


/*****************************************************************************
 * Constants
 *****************************************************************************
 */

//
// This means shadow register are to be read at least once to initialize.
//
const WORD SHREG_INVALID = 0x0001;

//
// This means shadow register should be overwritten with default value at
// driver init.
//
const WORD SHREG_INIT = 0x0002;

//
// This constant is used to prevent register caching.
//
const WORD SHREG_NOCACHE = 0x0004;

/*****************************************************************************
 * Classes
 *****************************************************************************
 */

/*****************************************************************************
 * CAdapterCommon
 *****************************************************************************
 * This is the common adapter object shared by all miniports to access the
 * hardware.
 */
class CAdapterCommon : public IAdapterCommon, 
                       public IAdapterPowerManagement,
                       public CUnknown
{
private:
	static tAC97Registers m_stAC97Registers[64];    // The shadow registers.
	//static struct HDA_DEVICE_EXTENSION m_HDAInfo;
    static tHardwareConfig m_stHardwareConfig;      // The hardware configuration.
	//HDA_DEVICE_EXTENSION m_DevExt;		// Device extension
	PUCHAR m_pHDARegisters;     // MMIO registers
	PUCHAR Base;
    PUCHAR InputStreamBase;
    PUCHAR OutputStreamBase;
	PMDL mdl;

	// CORB/RIRB buffers
	// RIRB is at the beginning of the block, then CORB, then BDL

	PULONG RirbMemVirt;
	PHYSICAL_ADDRESS RirbMemPhys;
    USHORT RirbPointer;
    USHORT RirbNumberOfEntries;

    PULONG CorbMemVirt;
	PHYSICAL_ADDRESS CorbMemPhys;
    USHORT CorbPointer;
    USHORT CorbNumberOfEntries;

	PULONG BdlMemVirt;
	PHYSICAL_ADDRESS BdlMemPhys;
	USHORT BdlPointer;
    USHORT BdlNumberOfEntries;

	PULONG DmaPosVirt;
	PHYSICAL_ADDRESS DmaPosPhys;

    // Output buffer information
    PULONG OutputBufferList;
	PVOID BufVirtualAddress;
	PHYSICAL_ADDRESS BufLogicalAddress;

	PDMA_ADAPTER DMA_Adapter;
	PDEVICE_DESCRIPTION pDeviceDescription;
	UCHAR interrupt;
	ULONG memLength;//check
	UCHAR codecNumber;

	BOOLEAN is64OK;

	ULONG communication_type;
    //ULONG codec_number;
    ULONG is_initialized_useful_output;
    ULONG selected_output_node;
    ULONG length_of_node_path;

    ULONG afg_node_sample_capabilities;
    ULONG afg_node_stream_format_capabilities;
    ULONG afg_node_input_amp_capabilities;
    ULONG afg_node_output_amp_capabilities;

    ULONG audio_output_node_number;
    ULONG audio_output_node_sample_capabilities;
    ULONG audio_output_node_stream_format_capabilities;

    ULONG output_amp_node_number;
    ULONG output_amp_node_capabilities;

    ULONG second_audio_output_node_number;
    ULONG second_audio_output_node_sample_capabilities;
    ULONG second_audio_output_node_stream_format_capabilities;
    ULONG second_output_amp_node_number;
    ULONG second_output_amp_node_capabilities;

    ULONG pin_output_node_number;
    ULONG pin_headphone_node_number;


    BOOLEAN m_bDMAInitialized;   // DMA initialized flag
    BOOLEAN m_bCORBInitialized;  // CORB initialized flag
    BOOLEAN m_bRIRBInitialized;  // RIRB initialized flag
    PDEVICE_OBJECT m_pDeviceObject;     // Device object used for registry access.
    PWORD m_pCodecBase;                 // The HDA I/O port address.
    PUCHAR m_pBusMasterBase;            // The Bus Master base address.
    BOOL m_bDirectRead;                 // Used during init time.
    DEVICE_POWER_STATE m_PowerState;    // Current power state of the device.


    /*************************************************************************
     * CAdapterCommon methods
     *************************************************************************
     */
    
    //
    // Resets HDA audio registers.
    //
    NTSTATUS InitHDA (void);
	NTSTATUS InitializeCodec (UCHAR num);

    
    //
    // Checks for existance of registers.
    //
    NTSTATUS ProbeHWConfig (void);

    //
    // Returns true if you should disable the input or output pin.
    //
    BOOL DisableAC97Pin (IN  TopoPinConfig);

#if (DBG)
    //
    // Dumps the probed configuration.
    //
    void DumpConfig (void);
#endif

    //
    // Sets HDA registers to default.
    //
    NTSTATUS SetAC97Default (void);

    //
    // Aquires the semaphore for HDA register access.
    //
    NTSTATUS AcquireCodecSemiphore (void);

    //
    // Checks if there is a HDA link between ICH and codec.
    //
    NTSTATUS PrimaryCodecReady (void);

    //
    // Powers up the Codec.
    //
    NTSTATUS PowerUpCodec (void);
    
    //
    // Saves native audio bus master control registers values to be used 
    // upon suspend.
    //
    NTSTATUS ReadNABMCtrlRegs (void);

    //
    // Writes back native audio bus master control resgister to be used upon 
    // resume.
    //
    NTSTATUS RestoreNABMCtrlRegs (void);

public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CAdapterCommon);
    ~CAdapterCommon();

    /*************************************************************************
     * IAdapterPowerManagement methods
     *************************************************************************
     */
    IMP_IAdapterPowerManagement;

    /*************************************************************************
     * IAdapterCommon methods
     *************************************************************************
     */
    
    //
    // Initialize the adapter common object -> initialize and probe HW.
    //
    STDMETHODIMP_(NTSTATUS) Init
    (
        IN  PRESOURCELIST ResourceList,
        IN  PDEVICE_OBJECT DeviceObject
    );
    
    //
    // Returns if pin exists.
    //
    STDMETHODIMP_(BOOL) GetPinConfig
    (
        IN  TopoPinConfig pin
    )
    {
        return m_stHardwareConfig.Pins[pin].bPinConfig;
    };

    //
    // Sets the pin configuration (exist/not exist).
    //
    STDMETHODIMP_(void) SetPinConfig
    (
        IN  TopoPinConfig pin,
        IN  BOOL config
    )
    {
        m_stHardwareConfig.Pins[pin].bPinConfig = config;
    };

    //
    // Return if node exists.
    //
    STDMETHODIMP_(BOOL) GetNodeConfig
    (
        IN  TopoNodeConfig node
    )
    {
        return m_stHardwareConfig.Nodes[node].bNodeConfig;
    };

    //
    // Sets the node configuration (exist/not exist).
    //
    STDMETHODIMP_(void) SetNodeConfig
    (
        IN  TopoNodeConfig node,
        IN  BOOL config
    )
    {
        m_stHardwareConfig.Nodes[node].bNodeConfig = config;
    };

    //
    // Returns the HDA register that is assosiated with the node.
    //
    STDMETHODIMP_(AC97Register) GetNodeReg
    (   IN  TopoNodes node
    )
    {
        return stMapNodeToReg[node].reg;
    };

    //
    // Returns the HDA register mask that is assosiated with the node.
    //
    STDMETHODIMP_(WORD) GetNodeMask
    (
        IN  TopoNodes node
    )
    {
        return stMapNodeToReg[node].mask;
    };

    //
    // Reads a HDA register.
    //
    STDMETHODIMP_(NTSTATUS) ReadCodecRegister
    (
        IN  AC97Register Register,
        OUT PWORD wData
    );

    //
    // Writes a HDA register.
    //
    STDMETHODIMP_(NTSTATUS) WriteCodecRegister
    (
        IN  AC97Register Register,
        IN  WORD wData,
        IN  WORD wMask
    );

    //
    // Reads a 8 bit ICH bus master register.
    //
    STDMETHODIMP_(UCHAR) ReadBMControlRegister8
    (
        IN  ULONG ulOffset
    );

    //
    // Reads a 16 bit ICH bus master register.
    //
    STDMETHODIMP_(USHORT) ReadBMControlRegister16
    (
        IN  ULONG ulOffset
    );

    //
    // Reads a 32 bit ICH bus master register.
    //
    STDMETHODIMP_(ULONG) ReadBMControlRegister32
    (
        IN  ULONG ulOffset
    );

    //
    // Writes a 8 bit ICH bus master register.
    //                                        
    STDMETHODIMP_(void) WriteBMControlRegister
    (
        IN  ULONG ulOffset,
        IN  UCHAR Value
    );

    //
    // writes a 16 bit ICH bus master register.
    //
    STDMETHODIMP_(void) WriteBMControlRegister
    (
        IN  ULONG ulOffset,
        IN  USHORT Value
    );

    // writes a 32 bit ICH bus master register.
    STDMETHODIMP_(void) WriteBMControlRegister
    (
        IN  ULONG ulOffset,
        IN  ULONG Value
    );

	STDMETHODIMP_(ULONG)	hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command);
	STDMETHODIMP_(void)		hda_initialize_audio_function_group(ULONG codec_number, ULONG afg_node_number); 
	STDMETHODIMP_(UCHAR)	hda_get_node_type(ULONG codec, ULONG node);
	STDMETHODIMP_(ULONG)	hda_get_node_connection_entry(ULONG codec, ULONG node, ULONG connection_entry_number);
	STDMETHODIMP_(void)		hda_initialize_output_pin(ULONG pin_node_number);
	STDMETHODIMP_(void)		hda_initalize_audio_output(ULONG audio_output_node_number);
	STDMETHODIMP_(void)		hda_initalize_audio_mixer(ULONG audio_mixer_node_number);
	STDMETHODIMP_(void)		hda_initalize_audio_selector(ULONG audio_selector_node_number);
	STDMETHODIMP_(BOOL)		hda_is_headphone_connected ( void );
	STDMETHODIMP_(void)		hda_set_node_gain(ULONG codec, ULONG node, ULONG node_type, ULONG capabilities, ULONG gain);
	STDMETHODIMP_(void)		hda_set_volume(ULONG volume);
	//STDMETHODIMP_(void)	hda_check_headphone_connection_change(void);
	STDMETHODIMP_(UCHAR)	hda_is_supported_channel_size(UCHAR size);
	STDMETHODIMP_(UCHAR)	hda_is_supported_sample_rate(ULONG sample_rate);
	STDMETHODIMP_(void)		hda_enable_pin_output(ULONG codec, ULONG pin_node);
	STDMETHODIMP_(void)		hda_disable_pin_output(ULONG codec, ULONG pin_node);
	//STDMETHODIMP_(USHORT)	hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample);
	STDMETHODIMP_(UCHAR)	readUCHAR(USHORT reg);
    STDMETHODIMP_(void)		writeUCHAR(USHORT reg, UCHAR value);

	STDMETHODIMP_(void)		setUCHARBit(USHORT reg, UCHAR flag);
	STDMETHODIMP_(void)		clearUCHARBit(USHORT reg, UCHAR flag);

    STDMETHODIMP_(USHORT)	readUSHORT(USHORT reg);
    STDMETHODIMP_(void)		writeUSHORT(USHORT reg, USHORT value);

    STDMETHODIMP_(ULONG)	readULONG(USHORT reg);
    STDMETHODIMP_(void)		writeULONG(USHORT reg, ULONG value);

	STDMETHODIMP_(void)		setULONGBit(USHORT reg, ULONG flag);
	STDMETHODIMP_(void)		clearULONGBit(USHORT reg, ULONG flag);

    //
    // Write back cached mixer values to codec registers.
    //
    STDMETHODIMP_(NTSTATUS) RestoreCodecRegisters();

    //
    // Programs a sample rate.
    //
    STDMETHODIMP_(NTSTATUS) ProgramSampleRate
    (
        IN  AC97Register Register,
        IN  DWORD dwSampleRate
    );


    /*************************************************************************
     * Friends
     *************************************************************************
     */
    
    friend NTSTATUS NewAdapterCommon
    (
        OUT PADAPTERCOMMON *OutAdapterCommon,
        IN  PRESOURCELIST ResourceList
    );
};

#endif  //_COMMON_H_
