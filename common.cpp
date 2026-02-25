/*****************************************************************************
 * common.cpp - Common code used by all the HDA miniports.
 *****************************************************************************
 * Copyright (c) 2025 Drew Hoffman
 * Released under MIT License
 * Code from BleskOS and Microsoft's driver samples used under MIT license. 
 *
 * Implmentation of the common code object.  This class deals with interrupts
 * for the device, and is a collection of common code used by all the
 * miniports.
 */

#include "stdunk.h"
#include "common.h"
#include "codec.h"


#define STR_MODULENAME "HDACommon: "

#define BDLE_FLAG_IOC  0x01

typedef struct _BDLE {
    ULONG64 Address;
    ULONG   Length;
    ULONG   Flags;
} BDLE;

#define CHUNK_SIZE 1792 //100ms of 44khz 16bit stereo rounded up to nearest 128b

/*****************************************************************************
 * CAdapterCommon
 
 *****************************************************************************
 * Adapter common object.
 */
class CAdapterCommon
:   public IAdapterCommon,
    public IAdapterPowerManagement,
    public CUnknown
    
{
private:
	PMDL mdl;
	KSPIN_LOCK QLock;
	PDMA_ADAPTER DMA_Adapter;
	PDEVICE_DESCRIPTION pDeviceDescription;
	
	//PCI IDs
	USHORT pci_ven;
	USHORT pci_dev;
	PUCHAR pConfigMem;
	USHORT codec_ven;
	USHORT codec_dev;

	// MMIO registers
	volatile PUCHAR m_pHDARegisters;     
	PUCHAR Base;
    USHORT InputStreamBase;
    USHORT OutputStreamBase;

	// CORB/RIRB buffers
	// need to be in different 4k pages
	volatile PULONG RirbMemVirt;
	PHYSICAL_ADDRESS RirbMemPhys;
    USHORT RirbPointer;
    USHORT RirbNumberOfEntries;

    volatile PULONG CorbMemVirt;
	PHYSICAL_ADDRESS CorbMemPhys;
    USHORT CorbPointer;
    USHORT CorbNumberOfEntries;

	volatile PULONG BdlMemVirt;
	PHYSICAL_ADDRESS BdlMemPhys;
	USHORT BdlPointer;
    USHORT BdlNumberOfEntries;

	volatile PULONG DmaPosVirt;
	PHYSICAL_ADDRESS DmaPosPhys;
	ULONG bad_dpos_count;

    // Output buffer information
    PULONG OutputBufferList;
	PVOID BufVirtualAddress;
	PHYSICAL_ADDRESS BufLogicalAddress;

	UCHAR interrupt;
	ULONG memLength;
	UCHAR codecNumber;
	UCHAR nSDO;
	UCHAR FirstOutputStream;
	USHORT statests;

	BOOLEAN is64OK;

	ULONG communication_type;

	ULONG debug_kludge;

	// Codec array - packed array of initialized codecs (not sparse)
	HDA_Codec* pCodecs[16];
	UCHAR codecCount;  // Number of actually initialized codecs

    BOOLEAN m_bDMAInitialized;   // DMA initialized flag
    BOOLEAN m_bCORBInitialized;  // CORB initialized flag
    BOOLEAN m_bRIRBInitialized;  // RIRB initialized flag
    PDEVICE_OBJECT m_pDeviceObject;     // Device object used for registry access.
    DEVICE_POWER_STATE m_PowerState;    // Current power state of the device.

    PINTERRUPTSYNC          m_pInterruptSync;
    PUCHAR                  m_pWaveBase;
    PUCHAR                  m_pUartBase;
    PPORTWAVECYCLIC         m_pPortWave;
    PPORTDMUS               m_pPortUart;
    PSERVICEGROUP           m_pServiceGroupWave;
    ULONGLONG               m_startTime;
    BOOL                    m_bCaptureActive;
    BYTE                    MixerSettings[DSP_MIX_MAXREGS];

    HDA_INTERRUPT_TYPE AcknowledgeIRQ
    (   void
    );
    BOOLEAN AdapterISR
    (   void
    );
	STDMETHODIMP_(NTSTATUS) ReadWriteConfigSpace(
    IN PDEVICE_OBJECT  DeviceObject,
    IN ULONG  ReadOrWrite,  // 0 for read, 1 for write
    IN PVOID  Buffer,
    IN ULONG  Offset,
    IN ULONG  Length
    );
	STDMETHODIMP_(NTSTATUS) WriteConfigSpaceByte(UCHAR offset, UCHAR andByte, UCHAR orByte);
	STDMETHODIMP_(NTSTATUS) WriteConfigSpaceWord(UCHAR offset, USHORT andWord, USHORT orWord);
	STDMETHODIMP_(BOOLEAN) ReadRegistryBoolean(
    IN  PCWSTR   ValueName,
    IN  BOOLEAN  DefaultValue
	);

public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CAdapterCommon);
    ~CAdapterCommon();

    /*****************************************************************************
     * IAdapterCommon methods
     */
    STDMETHODIMP_(NTSTATUS) Init
    (
        IN      PRESOURCELIST   ResourceList,
        IN      PDEVICE_OBJECT  DeviceObject
    );
    STDMETHODIMP_(PINTERRUPTSYNC) GetInterruptSync
    (   void
    );
	STDMETHODIMP_(PDEVICE_DESCRIPTION) GetDeviceDescription
    (   void
    );
    STDMETHODIMP_(PUNKNOWN *) WavePortDriverDest
    (   void
    );
    STDMETHODIMP_(PUNKNOWN *) MidiPortDriverDest
    (   void
    );
    STDMETHODIMP_(void) SetWaveServiceGroup
    (
        IN      PSERVICEGROUP   ServiceGroup
    );
    STDMETHODIMP_(BYTE) ReadController
    (   void
    );
    STDMETHODIMP_(BOOLEAN) WriteController
    (
        IN      BYTE    Value
    );
    STDMETHODIMP_(NTSTATUS) ResetController
    (   void
    );
	STDMETHODIMP_(NTSTATUS) hda_stop_stream
    (   void
    );
    STDMETHODIMP_(void) MixerRegWrite
    (
        IN      BYTE    Index,
        IN      BYTE    Value
    );
    STDMETHODIMP_(BYTE) MixerRegRead
    (
        IN      BYTE    Index
    );
    STDMETHODIMP_(void) MixerReset
    (   void
    );
    STDMETHODIMP RestoreMixerSettingsFromRegistry
    (   void
    );
    STDMETHODIMP SaveMixerSettingsToRegistry
    (   void
    );

	STDMETHODIMP_(NTSTATUS) ProgramSampleRate
    (
        IN  DWORD dwSampleRate
    );


	STDMETHODIMP_(NTSTATUS) InitHDAController (void);

	STDMETHODIMP_(ULONG)	hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command);
	static ULONG SendVerb(CAdapterCommon* pAdapter, ULONG codec, ULONG node, ULONG verb, ULONG command);
	STDMETHODIMP_(PULONG)	get_bdl_mem(void);
	STDMETHODIMP_(ULONG)	hda_get_actual_stream_position(void);
	STDMETHODIMP_(UCHAR)	hda_get_node_type(ULONG codec, ULONG node);
	STDMETHODIMP_(ULONG)	hda_get_node_connection_entries(ULONG codec, ULONG node, ULONG connection_entries_number);
	STDMETHODIMP_(BOOLEAN)	hda_is_headphone_connected (void);
	STDMETHODIMP_(void)		hda_set_node_gain(ULONG codec, ULONG node, ULONG node_type, ULONG capabilities, ULONG gain, UCHAR ch);
	STDMETHODIMP_(void)		hda_set_volume(ULONG volume, UCHAR ch);
	STDMETHODIMP_(void)		hda_start_sound (void);
	STDMETHODIMP_(void)		hda_stop_sound (void);

	STDMETHODIMP_(void)		hda_check_headphone_connection_change(void);
	STDMETHODIMP_(UCHAR)	hda_is_supported_sample_rate(ULONG sample_rate);
	STDMETHODIMP_(void)		hda_enable_pin_output(ULONG codec, ULONG pin_node);
	STDMETHODIMP_(void)		hda_disable_pin_output(ULONG codec, ULONG pin_node);
	STDMETHODIMP_(NTSTATUS)	hda_showtime(PDMACHANNEL DmaChannel);
	STDMETHODIMP_(USHORT)	hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample);
	
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

    
    /*************************************************************************
     * IAdapterPowerManagement implementation
     *
     * This macro is from PORTCLS.H.  It lists all the interface's functions.
     */
    IMP_IAdapterPowerManagement;

    friend
    NTSTATUS
    NewAdapterCommon
    (
        OUT     PADAPTERCOMMON *    OutAdapterCommon,
        IN      PRESOURCELIST       ResourceList
    );
    friend
    NTSTATUS
    InterruptServiceRoutine
    (
        IN      PINTERRUPTSYNC  InterruptSync,
        IN      PVOID           DynamicContext
    );
};

static
MIXERSETTING DefaultMixerSettings[] =
{
    { L"LeftMasterVol",   DSP_MIX_MASTERVOLIDX_L,     0xD8 },
    { L"RightMasterVol",  DSP_MIX_MASTERVOLIDX_R,     0xD8 },
    { L"LeftWaveVol",     DSP_MIX_VOICEVOLIDX_L,      0xD8 },
    { L"RightWaveVol",    DSP_MIX_VOICEVOLIDX_R,      0xD8 },
    { L"LeftMidiVol",     DSP_MIX_FMVOLIDX_L,         0xD8 },
    { L"RightMidiVol",    DSP_MIX_FMVOLIDX_R,         0xD8 },
    { L"LeftCDVol",       DSP_MIX_CDVOLIDX_L,         0xD8 },
    { L"RightCDVol",      DSP_MIX_CDVOLIDX_R,         0xD8 },
    { L"LeftLineInVol",   DSP_MIX_LINEVOLIDX_L,       0xD8 },
    { L"RightLineInVol",  DSP_MIX_LINEVOLIDX_R,       0xD8 },
    { L"MicVol",          DSP_MIX_MICVOLIDX,          0xD8 },
    { L"PcSpkrVol",       DSP_MIX_SPKRVOLIDX,         0x00 },
    { L"OutputMixer",     DSP_MIX_OUTMIXIDX,          0x1E },
    { L"LeftInputMixer",  DSP_MIX_ADCMIXIDX_L,        0x55 },
    { L"RightInputMixer", DSP_MIX_ADCMIXIDX_R,        0x2B },
    { L"LeftInputGain",   DSP_MIX_INGAINIDX_L,        0x00 },
    { L"RightInputGain",  DSP_MIX_INGAINIDX_R,        0x00 },
    { L"LeftOutputGain",  DSP_MIX_OUTGAINIDX_L,       0x80 },
    { L"RightOutputGain", DSP_MIX_OUTGAINIDX_R,       0x80 },
    { L"MicAGC",          DSP_MIX_AGCIDX,             0x01 },
    { L"LeftTreble",      DSP_MIX_TREBLEIDX_L,        0x80 },
    { L"RightTreble",     DSP_MIX_TREBLEIDX_R,        0x80 },
    { L"LeftBass",        DSP_MIX_BASSIDX_L,          0x80 },
    { L"RightBass",       DSP_MIX_BASSIDX_R,          0x80 },
};

//driver settings via registry keys
static BOOLEAN skipControllerReset;
static BOOLEAN skipCodecReset;
static BOOLEAN useAltOut;
static BOOLEAN useSPDIF;
static BOOLEAN useDisabledPins;
static BOOLEAN useDmaPos;

static REG_BOOL_SETTING g_BooleanSettings[] =
{
    { L"SkipControllerReset", &skipControllerReset, FALSE },
    { L"SkipCodecReset",      &skipCodecReset,      FALSE },
    { L"UseAltOut",           &useAltOut,           FALSE },
    { L"UseSPDIF",            &useSPDIF,            TRUE  },
    { L"UseDisabledPins",     &useDisabledPins,     FALSE },
    { L"UseDmaPos",           &useDmaPos,           FALSE },
};


#pragma code_seg("PAGE")

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
)
{
    PAGED_CODE();

    ASSERT(Unknown);

    STD_CREATE_BODY_
    (
        CAdapterCommon,
        Unknown,
        UnknownOuter,
        PoolType,
        PADAPTERCOMMON
    );
}   
	//100ms of 2ch 16 bit audio 4410 * 2 * 2
	ULONG audBufSize = MAXLEN_DMA_BUFFER; 
	ULONG BdlSize = 256 * 16; //256 entries, 16 bytes each

/*****************************************************************************
 * CAdapterCommon::Init()
 *****************************************************************************
 * Initialize an adapter common object.
 */
NTSTATUS
CAdapterCommon::
Init
(
    IN      PRESOURCELIST   ResourceList,
    IN      PDEVICE_OBJECT  DeviceObject
)
{
    PAGED_CODE();

    ASSERT(ResourceList);
    ASSERT(DeviceObject);

	NTSTATUS ntStatus = STATUS_SUCCESS;
	PHYSICAL_ADDRESS physAddr = {0};

    DOUT (DBG_PRINT, ("[CAdapterCommon::Init]"));
	ULONG i;
        
    //
    // Save the device object
    //
    m_pDeviceObject = DeviceObject;

	//Make sure cache line size set in device object is >= 128 byte for alignment reasons
    DOUT(DBG_SYSINFO, ("Initial PDO align was %d", 
				m_pDeviceObject -> AlignmentRequirement));

	if (m_pDeviceObject -> AlignmentRequirement < FILE_128_BYTE_ALIGNMENT) {
		m_pDeviceObject -> AlignmentRequirement = FILE_128_BYTE_ALIGNMENT;
		    DOUT(DBG_SYSINFO, ("Adjusted it to %d", 
				m_pDeviceObject -> AlignmentRequirement));
	}

	//init spin lock for protecting the CORB/RIRB or PIO
	KeInitializeSpinLock(&QLock);
	
	//Read settings from registry
	for (i = 0; i < ARRAY_COUNT(g_BooleanSettings); ++i){
		*g_BooleanSettings[i].Variable =
            ReadRegistryBoolean(g_BooleanSettings[i].ValueName, 
			g_BooleanSettings[i].DefaultValue);
    }

	// Initialize codec array
	codecCount = 0;
	for (i = 0; i < 16; i++) {
		pCodecs[i] = NULL;
	}

	//send an IRP asking the bus driver to read all of configspace
	//asking the PnP Config manager does NOT work since we're still in the middle of StartDevice()
	ULONG pci_ven = 0;
	ULONG pci_dev = 0;
	pConfigMem = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 256,'gfcP');
	ntStatus = ReadWriteConfigSpace(
		m_pDeviceObject,
		0,			//read
		pConfigMem,	// Buffer to store the configuration data		
		0,			// Offset into the configuration space
		256         // Number of bytes to read
	);

	if (!NT_SUCCESS (ntStatus)){
		DbgPrint( "\nPCI Configspace Read Failed! 0x%X\n", ntStatus);
        return ntStatus;
	} else {
		PUSHORT pConfigMemS = (PUSHORT)pConfigMem;
		//VID and PID are first 2 words of configspace
		pci_ven = (USHORT)pConfigMemS[0];
		pci_dev = (USHORT)pConfigMemS[1];
		DbgPrint( "\nHDA Controller: VID:0x%04X PID:0x%04X : ", pci_ven, pci_dev);
	}

	USHORT tmp;
	
	//apply device-specific config space patches depending on the VID/PID
	switch (pci_ven){
		case 0x1002: //ATI
		case 0x1022: //AMD
			if ( (pci_dev == 0x437b) || (pci_dev == 0x4383) //ATI SB
				|| (pci_dev == 0x780d) //Hudson
				|| (pci_dev == 0x1457) || (pci_dev == 0x1487) //AMD Ryzen
				|| (pci_dev == 0x157a) || (pci_dev == 0x15e3) //AMD APU
			){
				DbgPrint( "ATI/AMD SB450/600/APU\n");
				//Enable Snoop
				ntStatus = WriteConfigSpaceByte(0x42,
					~0x07, ATI_SB450_HDAUDIO_ENABLE_SNOOP);
				//should read back & confirm it was set.
			} else if ( (pci_dev == 0x0002) || (pci_dev == 0x1308)
				|| (pci_dev == 0x157a) || (pci_dev == 0x15b3)
			){				
				DbgPrint( "ATI/AMD HDMI Nosnoop\n");
				//Disable Snoop
				ntStatus = WriteConfigSpaceByte(0x42,
					~ATI_SB450_HDAUDIO_ENABLE_SNOOP, 0);
			} else {
				DbgPrint( "ATI/AMD Other\n");
			}
			break;
		case 0x10de: //nvidia
			if ( (pci_dev == 0x026c) || (pci_dev == 0x0371) //MCP5x
			  || (pci_dev == 0x03e4) || (pci_dev == 0x03f0) //MCP61
			  || (pci_dev == 0x044a) || (pci_dev == 0x044b) //MCP65
			  || (pci_dev == 0x055c) || (pci_dev == 0x055d) //MCP67
			  || (pci_dev == 0x0774) || (pci_dev == 0x0775) //MCP77
			  || (pci_dev == 0x0776) || (pci_dev == 0x0777) //MCP77
		  	  || (pci_dev == 0x07fc) || (pci_dev == 0x07fd) //MCP73
			  || (pci_dev == 0x0ac0) || (pci_dev == 0x0ac1) //MCP79
			  || (pci_dev == 0x0ac2) || (pci_dev == 0x0ac3) //MCP79
			  || (pci_dev == 0x0d94) || (pci_dev == 0x0d95) //MCP89
			  || (pci_dev == 0x0d96) || (pci_dev == 0x0d97) //MCP
			){
				DbgPrint( "Nvidia MCP\n");			 
				ntStatus = WriteConfigSpaceByte(0x4e,
					~NVIDIA_HDA_ENABLE_COHBITS, NVIDIA_HDA_ENABLE_COHBITS);
			} else{
				DbgPrint("Nvidia HDMI\n");
			}
			break;
		case 0x8086: //Intel
			switch (pci_dev){
				case 0x2668://ICH6
				case 0x27D8://ICH7
					DbgPrint( "Intel ICH6/7\n");
					//set device 27 function 0 configspace offset 40h bit 0 to 1
					//to enable HDA link mode (if it isnt already)
					ntStatus = WriteConfigSpaceByte(0x40, 0xfe, 0x01);
					break;
				//ICH
				case 0x269a:
				case 0x284b:
				case 0x2911:
				case 0x293e:
				case 0x293f:
				case 0x3a3e: //ICH8
				case 0x3a6e: //ICH9
					DbgPrint( "Intel ICH\n"); //these don't need nosnoop flag changed (?)
					break;
				//PCH
				case 0x1c20: 
				case 0x1d20:
				case 0x1e20:
				case 0x8c20:
				case 0x8ca0:
				case 0x8d20:
				case 0x8d21:
				case 0xa1f0:
				case 0xa270:
				case 0x9c20:
				case 0x9c21:
				case 0x9ca0:

				//SKL
				case 0xa170: 
				case 0x9d70:
				case 0xa171:
				case 0x9d71:
				case 0xa2f0:
				case 0xa348:
				case 0x9dc8:
				case 0x02c8:
				case 0x06c8:
				case 0xf1c8:
				case 0xa3f0:
				case 0xf0c8:
				case 0x34c8:
				case 0x3dc8:
				case 0x4dc8:
				case 0xa0c8:
				case 0x43c8:
				case 0x490d:
				case 0x7da0:
				case 0x51c8:
				case 0x51cc:
				case 0x4b55:
				case 0x4b58:
				case 0x5a98:
				case 0x1a98:
				case 0x3198:

				//HDMI
				case 0x0a0c: 
				case 0x0c0c:
				case 0x0d0c:
				case 0x160c:
				
				//SCH
				case 0x3b56: 
				case 0x811b:
				case 0x080a:
				case 0x0f04:
				case 0x2284:

					DbgPrint( "Intel PCH/SCH/SKL/HDMI\n");
					//disable no-snoop transaction feature (clear bit 11) if it is set
					tmp = *((PUSHORT)(pConfigMem + INTEL_SCH_HDA_DEVC));
					if((tmp & INTEL_SCH_HDA_DEVC_NOSNOOP) != 0){
						DbgPrint( "0x%X - disabling nosnoop transactions\n", tmp);
						ntStatus = WriteConfigSpaceWord(INTEL_SCH_HDA_DEVC,
							~((USHORT)INTEL_SCH_HDA_DEVC_NOSNOOP), 0);
					} else {
						DbgPrint( "0x%X - snoop already ok\n", tmp);
					}
					break;
				default:
					DbgPrint( "Unknown intel chipset PID %X\n", pci_dev);

			}
			break;
		case 0x10b9://ULI M5461
			DbgPrint( "ULI\n");
			//disable and zero out BAR1 on this hardware; it advertises 64 bit addressing support but can't deliver
			//not that we can use it anyway in 9x.
			//this is according to ALSA.
			//TODO: test this if i can find this rare chipset
			ntStatus = WriteConfigSpaceWord(0x40,0xffef,0x0010);
			ntStatus = WriteConfigSpaceWord(0x14,0x0000,0x0000);
			ntStatus = WriteConfigSpaceWord(0x16,0x0000,0x0000);
			break;
		default:
			DbgPrint( "unknown or no special patches\n");
			break;
	}
	//Set TCSEL (offset 44h in config space, lowest 3 bits) to 0 on some hardware to avoid crackling/static.
	//the Watlers and MPXPlay drivers set this byte on all but ATI controllers
	//I'm not sure if class 0 is the highest or lowest priority. some hardware defaults to traffic class 7
	if(pci_ven != 0x1002){
		ntStatus = WriteConfigSpaceByte(0x44, 0xf8, 0x0);
	}

	if (!NT_SUCCESS (ntStatus)){
		DbgPrint( "\nPCI Config Space Write Failed! 0x%X\n", ntStatus);
        //return ntStatus; //is failure fatal? i dont think so
	}

	//there may be multiple instances of this driver loaded at once
	//on systems with HDMI display audio support for instance
	//but it should be one driver object per HDA controller.
	//which may access multiple codecs
	//for now only sending 1 output audio stream to all

    //
    //Get the memory base address for the HDA controller registers. 
    // note we only want the first BAR
	// on Skylake and newer mobile chipsets,
	// there is a DSP interface at BAR2 for Intel Smart Sound Technology
	// TODO: fix adapter.cpp to check for this better

	DOUT(DBG_SYSINFO, ("%d Resources in List", ResourceList->NumberOfEntries() ));

    for (i = 0; i < ResourceList->NumberOfEntries(); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = ResourceList->FindTranslatedEntry(CmResourceTypeMemory, i);
        if (desc) {
			if (!memLength) {
				physAddr = desc->u.Memory.Start;
				memLength = desc->u.Memory.Length; 
			}
            DOUT(DBG_SYSINFO, ("Resource %d: Phys Addr = 0x%08lX%08lX, Mem Length = %lX", 
				i, physAddr.HighPart, physAddr.LowPart, memLength));
        } else {
				// get interrupt resource too if there is one
			 desc = ResourceList->FindTranslatedEntry(CmResourceTypeInterrupt, i);
			 if (desc) {
				// interrupt = desc->u.Interrupt.Vector;
				DOUT(DBG_SYSINFO, ("Resource %d: Affinity %d, Level %d, Vector = 0x%X", 
				i, desc->u.Interrupt.Affinity, desc->u.Interrupt.Level, desc->u.Interrupt.Vector));
			 }
		}
    }

	if (!memLength) {
        DOUT (DBG_ERROR, ("No memory for HD Audio controller registers"));
        return STATUS_INSUFFICIENT_RESOURCES;
	} 

	//Map the HDA controller PCI registers into kernel memory 
    m_pHDARegisters = (PUCHAR) MmMapIoSpace(physAddr, memLength, MmNonCached);

    if (!m_pHDARegisters) {
        DOUT (DBG_ERROR, ("Failed to map HD Audio registers to kernel mem"));
        return STATUS_NO_MEMORY;
    } else if (memLength < 16383) {
		DOUT (DBG_ERROR, ("BAR0 is too small to be HDA"));
		return STATUS_BUFFER_TOO_SMALL;
	} else {
        DOUT(DBG_SYSINFO, ("Virt Addr = 0x%X, Mem Length = 0x%X", m_pHDARegisters, memLength));
    }
	Base = (PUCHAR)m_pHDARegisters;

#if (DBG)
	//try reading something from the mapped memory and see if it makes sense as hda registers
	//for (i = 0; i < 16; i++) {
	//	DOUT(DBG_SYSINFO, ("Reg %d 0x%X", i, ((PUCHAR)m_pHDARegisters)[i]  ));
	//}
#endif
	//read capabilities
	USHORT caps = readUSHORT(0x00);

	//check 64OK flag
	is64OK = caps & 1;

	//TODO check that we have at least 1 output or bidirectional stream engine

	DOUT( DBG_SYSINFO, ("Version: %d.%d", readUCHAR(0x03), readUCHAR(0x02) ));

	//offsets for stream engines
	InputStreamBase = (0x80);
	UCHAR numCaptureStreams = ((caps >> 8) & 0xF);
	UCHAR numPlaybackStreams = ((caps >> 12) & 0xf);
	if(!numCaptureStreams && !numPlaybackStreams){
		DOUT(DBG_ERROR, ("Capabilities reports no streams. Guessing 4"));
		//TODO check on ULI and ATI systems that report weird caps!
		FirstOutputStream = 4;
	} else {
		FirstOutputStream = numCaptureStreams;
	}

	OutputStreamBase = HDA_STREAMBASE(FirstOutputStream); //skip input streams ports
	switch((caps >> 1) & 0x3){
	case 0:
		nSDO = 1;
		break;
	case 1:
		nSDO = 2;
		break;
	case 2:
		nSDO = 4;
		break;
	}

	DOUT( DBG_SYSINFO, ("caps 0x%X: input streams:%d output streams:%d bd streams:%d SDOs:%d 64ok:%d",
		caps,
		((caps >> 8) & 0xF),
		((caps >> 12) & 0xF),
		((caps >> 3) & 0x1f),
		nSDO,
		is64OK
		));

	//allocate common buffers
	//for CORB, RIRB, BDL buffer, DMA position buffer

	//the spec doesn't tell you this, but these can NOT share a 4k page
	//so we need a separate mapping for each one.

	//create device description object for our DMA
	pDeviceDescription = (PDEVICE_DESCRIPTION)ExAllocatePool (PagedPool,
                                      sizeof (DEVICE_DESCRIPTION));
	if (!pDeviceDescription) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//zero that struct
	RtlZeroMemory(pDeviceDescription, sizeof (DEVICE_DESCRIPTION));

	pDeviceDescription -> Version			= DEVICE_DESCRIPTION_VERSION;
	pDeviceDescription -> Master			= TRUE;
	pDeviceDescription -> ScatterGather		= TRUE; 
	pDeviceDescription -> DemandMode		= FALSE;
	pDeviceDescription -> AutoInitialize	= FALSE;
	pDeviceDescription -> Dma32BitAddresses = TRUE;
	pDeviceDescription -> IgnoreCount		= FALSE;
	pDeviceDescription -> Reserved1			= FALSE;
	pDeviceDescription -> Dma64BitAddresses = is64OK; //it might. doesnt matter to win98
	pDeviceDescription -> DmaChannel		= 0;
	pDeviceDescription -> InterfaceType		= PCIBus;
	pDeviceDescription -> MaximumLength		= BdlSize + 4096 + 8192;

	//number of "map registers" doesn't matter at all here, but i need somewhere to put it
	ULONG nMapRegisters = 0;

	DMA_Adapter = IoGetDmaAdapter (
		m_pDeviceObject,
		pDeviceDescription,
		&nMapRegisters );

	DOUT(DBG_SYSINFO, ("Map Registers = %d", nMapRegisters));

	//now we call the AllocateCommonBuffer function pointer in that struct
	
	//Allocate RIRB
	PVOID RirbVirtualAddress = NULL;
	PPHYSICAL_ADDRESS pRirbLogicalAddress = NULL;	
	
	RirbVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		2048,
		pRirbLogicalAddress, //out param
		FALSE ); //CacheEnabled is probably ignored
	RirbMemPhys = *pRirbLogicalAddress;
	RirbMemVirt = (PULONG) RirbVirtualAddress;
	//mdl = IoAllocateMdl(VirtualAddress, 8192, FALSE, FALSE, NULL);

	DOUT(DBG_SYSINFO, ("RIRB Virt Addr = 0x%X,", RirbMemVirt));
	DOUT(DBG_SYSINFO, ("RIRB Phys Addr = 0x%X,", RirbMemPhys));

	if (!RirbMemVirt) {
		DOUT(DBG_ERROR, ("Couldn't map virt RIRB Space"));
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (RirbMemPhys.QuadPart == 0) {
		DOUT(DBG_ERROR, ("Couldn't map phys RIRB Space"));
		return STATUS_NO_MEMORY;
	}

	if (is64OK == FALSE) {
		ASSERT(RirbMemPhys.HighPart == 0);
	}

	//check 128-byte alignment of what we received
	ASSERT( (RirbMemPhys.LowPart & 0x7F) == 0);

	//allocate CORB
	PVOID CorbVirtualAddress = NULL;
	PPHYSICAL_ADDRESS pCorbLogicalAddress = NULL;	
	
	CorbVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		1024,
		pCorbLogicalAddress, //out param
		FALSE ); //CacheEnabled is probably ignored

	CorbMemPhys = *pCorbLogicalAddress;
	CorbMemVirt = (PULONG) CorbVirtualAddress;

	DOUT(DBG_SYSINFO, ("Corb Virt Addr = 0x%X,", CorbMemVirt));
	DOUT(DBG_SYSINFO, ("Corb Phys Addr = 0x%X,", CorbMemPhys));

	if (!CorbMemVirt) {
		DOUT(DBG_ERROR, ("Couldn't map virt Corb Space"));
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (CorbMemPhys.QuadPart == 0) {
		DOUT(DBG_ERROR, ("Couldn't map phys Corb Space"));
		return STATUS_NO_MEMORY;
	}

	if (is64OK == FALSE) {
		ASSERT(CorbMemPhys.HighPart == 0);
	}

	//check 128-byte alignment of what we received
	ASSERT( (CorbMemPhys.LowPart & 0x7F) == 0);

	//allocate BDL
	PVOID BdlVirtualAddress = NULL;
	PPHYSICAL_ADDRESS pBdlLogicalAddress = NULL;
	
	
	BdlVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		BdlSize,
		pBdlLogicalAddress, //out param
		FALSE ); //CacheEnabled is probably ignored

	BdlMemPhys = *pBdlLogicalAddress;
	BdlMemVirt = (PULONG) BdlVirtualAddress;

	if (!BdlMemVirt) {
		DOUT(DBG_ERROR, ("Couldn't map virt BDL Space"));
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (BdlMemPhys.QuadPart == 0) {
		DOUT(DBG_ERROR, ("Couldn't map phys BDL Space"));
		return STATUS_NO_MEMORY;
	}

	if (is64OK == FALSE) {
		ASSERT(BdlMemPhys.HighPart == 0);
	}

	//allocate DMA Position Buffer
	PVOID DmaVirtualAddress = NULL;
	PPHYSICAL_ADDRESS pDmaLogicalAddress = NULL;
	
	
	DmaVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		1024,
		pDmaLogicalAddress, //out param
		FALSE ); //CacheEnabled is probably ignored

	DmaPosPhys = *pDmaLogicalAddress;
	DmaPosVirt = (PULONG) DmaVirtualAddress;

	if (!DmaPosVirt) {
		DOUT(DBG_ERROR, ("Couldn't map virt DMA Position Buffer"));
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (DmaPosPhys.QuadPart == 0) {
		DOUT(DBG_ERROR, ("Couldn't map phys DMA Position Buffer"));
		return STATUS_NO_MEMORY;
	}

	if (is64OK == FALSE) {
		ASSERT(DmaPosPhys.HighPart == 0);
	}
	
	if (!NT_SUCCESS (ntStatus)){
		DbgPrint( "\nBuffer Mapping Failed! 0x%X\n", ntStatus);
        return ntStatus;
	}


	// Not mapping an audio buffer yet, that happens when the
	// Wave miniport calls showtime()

	//
    // Reset the controller and init registers
    //
	DbgPrint( ("\nInit HDA Controller!\n"));

	ntStatus = InitHDAController ();

    if (!NT_SUCCESS (ntStatus)){
		DbgPrint( "\nResetController Failed! 0x%X\n", ntStatus);
        return ntStatus;
	} else {
		DbgPrint( "\nResetController Succeeded! 0x%X\n", ntStatus);
	}
    
    //
    // Hook up the interrupt.
    //
    ntStatus = PcNewInterruptSync(                              // See portcls.h
                                        &m_pInterruptSync,          // Save object ptr
                                        NULL,                       // OuterUnknown(optional).
                                        ResourceList,               // He gets IRQ from ResourceList.
                                        0,                          // Resource Index
                                        InterruptSyncModeNormal     // Run ISRs once until we get SUCCESS
                                     );
	if (!NT_SUCCESS(ntStatus) || m_pInterruptSync == NULL) {
			DbgPrint("PcNewInterruptSync failed: 0x%X\n", ntStatus);
			return ntStatus;
	}

    //  run this ISR first

    ntStatus = m_pInterruptSync->RegisterServiceRoutine(InterruptServiceRoutine,PVOID(this),FALSE);

    if (!NT_SUCCESS(ntStatus)) {
		DbgPrint("RegisterServiceRoutine failed: 0x%X\n", ntStatus);
		m_pInterruptSync->Release();
		m_pInterruptSync = NULL;
		return ntStatus;
	}
       
	ntStatus = m_pInterruptSync->Connect();

	if (!NT_SUCCESS(ntStatus)) {
		DbgPrint("InterruptSync->Connect failed: 0x%X\n", ntStatus);
		m_pInterruptSync->Release();
		m_pInterruptSync = NULL;
		return ntStatus;
	}

	DbgPrint("Init Finished Successfully!\n");
    return ntStatus;
}

/*****************************************************************************
 * CAdapterCommon::~CAdapterCommon()
 *****************************************************************************
 * Destructor.
 */
CAdapterCommon::
~CAdapterCommon
(   void
)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CAdapterCommon::~CAdapterCommon]"));

	//At least try to stop the stream before destruction
	hda_stop_stream ();

	//Delete all codec objects
	for (UCHAR i = 0; i < 16; i++) {
		if (pCodecs[i] != NULL) {
			delete pCodecs[i];
			pCodecs[i] = NULL;
		}
	}

	//do NOT free the audio buffer now minwave is managing it

	//free DMA buffers
	if((RirbMemVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing rirb buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      RirbMemPhys,
                      RirbMemVirt,
                      FALSE );
		RirbMemVirt = NULL;
	}
	if((CorbMemVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing Corb buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      CorbMemPhys,
                      CorbMemVirt,
                      FALSE );
		CorbMemVirt = NULL;
	}
	if((BdlMemVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing Bdl buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      BdlMemPhys,
                      BdlMemVirt,
                      FALSE );
		BdlMemVirt = NULL;
	}
	if((DmaPosVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing Dma buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      DmaPosPhys,
                      DmaPosVirt,
                      FALSE );
		DmaPosVirt = NULL;
	}

	//free DMA adapter object
	if (DMA_Adapter) {
		DMA_Adapter->DmaOperations->PutDmaAdapter(DMA_Adapter);
		DMA_Adapter = NULL;
	}
	//free device description
	if (pDeviceDescription) {
		ExFreePool(pDeviceDescription);
		pDeviceDescription = NULL;
	}
	//free device description
	if (pConfigMem) {
		ExFreePool(pConfigMem);
		pConfigMem = NULL;
	}
	//free PCI BAR space	
    if (m_pHDARegisters) {
		MmUnmapIoSpace(m_pHDARegisters, memLength);
		m_pHDARegisters = NULL; // Ensure the pointer is set to NULL after unmapping
	}

    if (m_pInterruptSync)
    {
        m_pInterruptSync->Disconnect();
        m_pInterruptSync->Release();
    }
    if (m_pPortWave)
    {
        m_pPortWave->Release();
    }
    if (m_pPortUart)
    {
        m_pPortUart->Release();
    }
    if (m_pServiceGroupWave)
    {
        m_pServiceGroupWave->Release();
    }
}

/*****************************************************************************
 * CAdapterCommon::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.
 */
STDMETHODIMP
CAdapterCommon::
NonDelegatingQueryInterface
(
    REFIID  Interface,
    PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PADAPTERCOMMON(this)));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IAdapterCommon))
    {
        *Object = PVOID(PADAPTERCOMMON(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IAdapterPowerManagment))
    {
        *Object = PVOID(PADAPTERPOWERMANAGMENT(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

/*****************************************************************************
 * CAdapterCommon::WavePortDriverDest()
 *****************************************************************************
 * Get a pointer to the wave port pointer.
 */
STDMETHODIMP_(PUNKNOWN *)
CAdapterCommon::
WavePortDriverDest
(   void
)
{
    PAGED_CODE();

    return (PUNKNOWN *) &m_pPortWave;
}

/*****************************************************************************
 * CAdapterCommon::GetInterruptSync()
 *****************************************************************************
 * Get a pointer to the interrupt synchronization object.
 */
STDMETHODIMP_(PINTERRUPTSYNC)
CAdapterCommon::
GetInterruptSync
(   void
)
{
    PAGED_CODE();

    return m_pInterruptSync;
}

/*****************************************************************************
 * CAdapterCommon::GetDeviceDescription()
 *****************************************************************************
 * Get a pointer to the DeviceDescription object.
 */
STDMETHODIMP_(PDEVICE_DESCRIPTION)
CAdapterCommon::
GetDeviceDescription
(   void
)
{
    PAGED_CODE();

    return pDeviceDescription;
}

/*****************************************************************************
 * CAdapterCommon::MidiPortDriverDest()
 *****************************************************************************
 * Get a pointer to the UART port pointer.
 */
STDMETHODIMP_(PUNKNOWN *)
CAdapterCommon::
MidiPortDriverDest
(   void
)
{
    PAGED_CODE();

    return (PUNKNOWN *) &m_pPortUart;
}

/*****************************************************************************
 * CAdapterCommon::SetWaveServiceGroup()
 *****************************************************************************
 * Provides a pointer to the wave service group.
 */
STDMETHODIMP_(void)
CAdapterCommon::
SetWaveServiceGroup
(
    IN      PSERVICEGROUP   ServiceGroup
)
{
	PAGED_CODE ();
    if (m_pServiceGroupWave)
    {
        m_pServiceGroupWave->Release();
    }

    m_pServiceGroupWave = ServiceGroup;
    if( m_pServiceGroupWave )
    {
        m_pServiceGroupWave->AddRef();
    }
}
/*****************************************************************************
 * CAdapterCommon::InitHDAController
 *****************************************************************************
 * Initialize the HDA controller. Only call this from IRQL = Passive
 */

STDMETHODIMP_(NTSTATUS) CAdapterCommon::InitHDAController (void)
{
    PAGED_CODE ();
	UCHAR codec_number;
	ULONG codec_id;
	ULONG i;
    
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

    DOUT (DBG_PRINT, ("[CAdapterCommon::InitHDAController]"));

	//TODO: try to stop all possible running streams before resetting?

	//we're not supposed to write to any registers before reset

	statests = readUSHORT(0x0E);
	DOUT(DBG_SYSINFO, ("STATESTS = %X", statests ));

	//to preserve BIOS pin information,
	//ONLY reset the whole controller if no codecs have enum. in statests
	if ((statests != 0) && skipControllerReset) {
		DOUT(DBG_SYSINFO, ("Skipping reset"));
	} else {
		
		//Reset the whole controller. Spec says this can take up to 521 us

		DOUT(DBG_SYSINFO, ("Resetting HDA Controller"));

		writeUCHAR(0x08,0x0);
		
		//wait for reset start acknowledgement
		for (i = 0; i < 100; i++) {
			KeStallExecutionProcessor(150);
			if ((readUCHAR(0x08) & 0x1) == 0x0) {
				DOUT(DBG_SYSINFO, ("Controller Reset Started %d", i));
				break;
			} else if (i == 99) {
				DOUT (DBG_ERROR, ("Controller Not Responding Reset Start"));
				return STATUS_UNSUCCESSFUL;
			}
		}

		KeStallExecutionProcessor(100);
		writeUCHAR(0x08,0x1);
		//try to disable interrupts immediately
		writeULONG (0x20, 0x0);
	
		//wait for reset complete
		for (i = 0; i < 100; i++) {
			KeStallExecutionProcessor(150);
			if ((readUCHAR(0x08) & 0x1) == 0x1) {
				DOUT(DBG_SYSINFO, ("Controller Reset Complete %d", i));
				break;
			}
			else if (i == 99) {
				DOUT (DBG_ERROR, ("Controller Not Responding Reset Complete"));
				return STATUS_UNSUCCESSFUL;
			}
		}

		// now we're supposed to wait at least 1ms for codec reset events before continuing
		DOUT(DBG_SYSINFO, ("waiting for codecs to enumerate on link"));
		KeStallExecutionProcessor(1000);
	}

	//disable interrupts again just to be sure
	writeULONG (0x20, 0x0);

	//turn ON WAKEEN interrupts so we can see if any codecs ever respond to reset
	//writeUSHORT(0x0C, 0x7FFF);

	// clear dma position buffer address
	writeULONG (0x70, 0);
	writeULONG (0x74, 0);
 
	//disable synchronization at both registers it could be
	writeULONG (0x34, 0);
	writeULONG (0x38, 0);

	//stop CORB and RIRB
	writeUCHAR (0x4C, 0x0);
	writeUCHAR (0x5C, 0x0);


	
	statests = readUSHORT(0x0E);
	DOUT(DBG_SYSINFO, ("STATESTS = %X", statests ));
	//this may read zero, not sure if writing a 1 to a bit just acknowledges or clears it too
	//but if it clears it we'll get an interrupt with it first and print a msg there

	//write the needed logical addresses for CORB/RIRB to the HDA controller registers

	//corb addr
	writeULONG (0x40, CorbMemPhys.LowPart);
	if (is64OK){
		writeULONG (0x44, CorbMemPhys.HighPart);
	}
	else {
		writeULONG (0x44, 0);
	}
	//corb number of entries (each entries is 4 bytes)
	//check what is supported and set it to the max supported

	if ((readUCHAR (0x4E)  & 0x40) == 0x40){
		//corb size is 256 entries
		DOUT (DBG_SYSINFO, ("Corb size 256 entries"));
		CorbNumberOfEntries = 256;
		writeUCHAR (0x4E, 0x2);
	} else if ((readUCHAR (0x4E)  & 0x40) == 0x20){
		//corb size is 16 entries
		DOUT (DBG_SYSINFO, ("Corb size 16 entries"));
		CorbNumberOfEntries = 16;
		writeUCHAR (0x4E, 0x1);
	} else if ((readUCHAR (0x4E)  & 0x40) == 0x10){
		//corb size is 2 entries
		DOUT (DBG_SYSINFO, ("Corb size 2 entries"));
		CorbNumberOfEntries = 2;
		writeUCHAR (0x4E, 0x0);
	} else {
		//CORB not supported, need to use PIO
		DOUT (DBG_ERROR, ("CORB only supports PIO"));
		goto hda_use_pio_interface;
		//return STATUS_UNSUCCESSFUL;
	}
	//Reset corb read pointer
	writeUSHORT (0x4A, 0x8000); //write a 1 to bit 15
	for (i = 0; i < 50; i++) {
		if ((readUSHORT(0x4A) & 0x8000) == 0x8000) break;
		KeStallExecutionProcessor(10);
		//read back the 1 to verify reset
	}
	if ((readUSHORT(0x4A) & 0x8000) == 0x0000){
		//VMWare never returns a 1 in this bit so we fall back to PIO. that's fine for now
		DOUT (DBG_ERROR, ("Controller Not Responding to CORB Pointer Reset On"));
		goto hda_use_pio_interface;
		//return STATUS_UNSUCCESSFUL;
	}

	//then write a 0 and read back the 0 to verify a clear
	writeUSHORT(0x4A, 0x0000);
	for (i = 0; i < 50; i++) {
		if ((readUSHORT(0x4A) & 0x8000) == 0x0000) break;
		KeStallExecutionProcessor(10);
	}
	if ((readUSHORT(0x4A) & 0x8000) == 0x8000){
		DOUT (DBG_ERROR, ("Controller Not Responding to CORB Pointer Reset Off"));
		goto hda_use_pio_interface;
		//return STATUS_UNSUCCESSFUL;
	}

	writeUSHORT (0x48,0);
	CorbPointer = 1; //always points to next free entries

	//rirb addr

	writeULONG (0x50, RirbMemPhys.LowPart);
	if (is64OK){
		writeULONG (0x54, RirbMemPhys.HighPart);
	}
	else {
		writeULONG (0x54, 0);
	}
	//rirb number of entries (each entries is 8 bytes)
	
	if ((readUCHAR (0x5E)  & 0x40) == 0x40){
		//rirb size is 256 entries
		DOUT (DBG_SYSINFO, ("rirb size 256 entries"));
		RirbNumberOfEntries = 256;
		writeUCHAR (0x5E, 0x2);
	} else if ((readUCHAR (0x5E)  & 0x40) == 0x20){
		//rirb size is 16 entries
		DOUT (DBG_SYSINFO, ("rirb size 16 entries"));
		RirbNumberOfEntries = 16;
		writeUCHAR (0x5E, 0x1);
	} else if ((readUCHAR (0x5E)  & 0x40) == 0x10){
		//rirb size is 2 entries
		DOUT (DBG_SYSINFO, ("rirb size 2 entries"));
		RirbNumberOfEntries = 2;
		writeUCHAR (0x5E, 0x0);
	} else {
		//rirb not supported, need to use PIO
		DOUT (DBG_ERROR, ("RIRB only supports PIO"))
		goto hda_use_pio_interface;
		//return STATUS_NOT_IMPLEMENTED;
	}

	//reset RIRB write pointer to 0th entries
	writeUSHORT (0x58, 0x8000);

	//dma position buffer pointer
	writeULONG (0x70, DmaPosPhys.LowPart);
	if (is64OK){
		writeULONG (0x74, DmaPosPhys.HighPart);
	}
	else {
		writeULONG (0x74, 0);
	}

	if(useDmaPos){
		// turn on dma position transfer
		writeULONG (0x70, DmaPosPhys.LowPart | 0x1);
	}


	KeStallExecutionProcessor(10);
	writeULONG(0x5A, 0); //disable interrupts
	RirbPointer = 1; //always points to next free entries
	
	//start CORB and RIRB. i think this also makes the HDA controller zero them
	writeUCHAR ( 0x4C, 0x2);
	writeUCHAR ( 0x5C, 0x2);

	communication_type = HDA_CORB_RIRB;

	//TODO: i will need 2 implementations of send_codec_verb
	//one immediate that stalls, and one with a callback
	//maybe take more than one verb at once w/o blocking

	//find codecs on the link.

	//check codecs enumerated in STATESTS first if available

	//TODO: rewrite without the gotos!
	if(statests == 0)
		goto blind_probe;
	
	DOUT (DBG_SYSINFO, ("Probing codecs found in STATESTS"));

	for(codec_number = 0; codec_number < 16; codec_number++) {
		communication_type = HDA_CORB_RIRB;
		if (((statests >> codec_number) & 1) == 0)
			continue;

		communication_type = HDA_CORB_RIRB;

		codec_id = hda_send_verb(codec_number, 0, 0xF00, 0);

		DOUT (DBG_SYSINFO, ("Codec %d response 0x%X", codec_number, codec_id));
		
		if((codec_id != 0x0) && (codec_id != 0xFFFFFFFF) && (codec_id != STATUS_UNSUCCESSFUL)) {
			DOUT (DBG_SYSINFO, ("HDA: Codec %d CORB/RIRB communication interface", codec_id));
			//create and initialize codec object and pack into array
			HDA_Codec* pCodec = new(NonPagedPool) HDA_Codec(useSPDIF, useAltOut, codec_number, this);
			if (pCodec) {
				pCodecs[codecCount++] = pCodec;
				ntStatus = pCodec->InitializeCodec();
				if (!NT_SUCCESS(ntStatus)) {
					// If initialization failed, keep trying other codecs
					codecCount--;
					delete pCodec;
				}
			}
		} else if((statests >> (codec_number+1)) == 0){
			//if no further codecs left in statests
			goto blind_probe;
		}

	}

	if (!NT_SUCCESS (ntStatus))
    {        
        DOUT (DBG_ERROR, ("Initialization of HDA Codec failed."));
    }
	return ntStatus;

blind_probe:

	DOUT (DBG_SYSINFO, ("Probing codecs blind"));

	//If we haven't found anything yet, resort to blindly trying to reset each codec
	for (codec_number = 0; codec_number < 16; codec_number++) {

		communication_type = HDA_CORB_RIRB;
		
		//send codec reset command (Realtek needs it before it will respond to IDs?)
		//there won't be a response
		hda_send_verb(codec_number, 1, 0x7ff, 0);

		//stall for at least 477 clocks for codec reset turnaround
		KeStallExecutionProcessor(50);		

		communication_type = HDA_CORB_RIRB;

		codec_id = hda_send_verb(codec_number, 0, 0xF00, 0);

		DOUT (DBG_SYSINFO, ("Codec %d response 0x%X", codec_number, codec_id));
		
		if(codec_number == 15 && codec_id == STATUS_UNSUCCESSFUL){
			//if we got to last codec and no responses from any of them
			DOUT (DBG_ERROR, ("CORB/RIRB Communication is Broken."));
			if (codecCount == 0) {
				return STATUS_UNSUCCESSFUL;
			}
		}

		if((codec_id != 0) && (codec_id != 0xFFFFFFFF) && (codec_id != STATUS_UNSUCCESSFUL)) {

			DOUT (DBG_SYSINFO, ("HDA: Codec %d CORB/RIRB communication interface", codec_id));

			//create and initialize codec object and pack into array
			HDA_Codec* pCodec = new(NonPagedPool) HDA_Codec(useSPDIF, useAltOut, codec_number, this);
			if (pCodec) {
				pCodecs[codecCount++] = pCodec;
				ntStatus = pCodec->InitializeCodec();
				if (!NT_SUCCESS(ntStatus)) {
					// If initialization failed, keep trying other codecs
					codecCount--;
					delete pCodec;
					ntStatus = STATUS_UNSUCCESSFUL;
				}
			}
		}
	}

    if (!NT_SUCCESS (ntStatus))
    {        
        DOUT (DBG_ERROR, ("Initialization of HDA CoDec failed."));
    }
	return ntStatus;
	
hda_use_pio_interface:

	DOUT (DBG_SYSINFO, ("Using Immediate Command Interface."));

	//stop CORB and RIRB
	writeUCHAR ( 0x4C, 0x0);
	writeUCHAR ( 0x5C, 0x0);

	communication_type = HDA_PIO;

	for(codec_number = 0, codec_id = 0; codec_number < 16; codec_number++) {

		communication_type = HDA_PIO;
		
		hda_send_verb(codec_number, 1, 0x7ff, 0);

		communication_type = HDA_PIO;

		codec_id = hda_send_verb(codec_number, 0, 0xF00, 0);

		if(codec_number == 15 && codec_id == STATUS_UNSUCCESSFUL){
			//if we got to last codec and no responses from any of them
			DOUT (DBG_ERROR, ("PIO Communication is Broken."));
			if (codecCount == 0) {
				return STATUS_UNSUCCESSFUL;
			}
		}

		if((codec_id != 0) && (codec_id != STATUS_UNSUCCESSFUL)) {
			DOUT (DBG_SYSINFO, ("HDA:  Codec %d PIO communication interface", codec_id));
			//create and initialize codec object and pack into array
			HDA_Codec* pCodec = new(NonPagedPool) HDA_Codec(useSPDIF, useAltOut, codec_number, this);
			if (pCodec) {
				pCodecs[codecCount++] = pCodec;
				ntStatus = pCodec->InitializeCodec();
				if (!NT_SUCCESS(ntStatus)) {
					// If initialization failed, keep trying other codecs
					codecCount--;
					delete pCodec;
					ntStatus = STATUS_UNSUCCESSFUL;
				}
			}
		}
	}
	if (codecCount == 0)
    {        
        DOUT (DBG_ERROR, ("Initialization of HDA Codec failed."));
        return STATUS_UNSUCCESSFUL;
    }
    return ntStatus;
}

/*****************************************************************************
 * Note: hda_initialize_audio_function_group, hda_initialize_output_pin,
 * hda_initialize_audio_output, hda_initialize_audio_mixer, and 
 * hda_initialize_audio_selector have been moved to HDA_Codec class
 *****************************************************************************/

// Reads any Boolean value from the Registry Settings key.
// If nonexistent, returns DefaultValue
STDMETHODIMP_(BOOLEAN)
CAdapterCommon::ReadRegistryBoolean(
    IN  PCWSTR   ValueName,
    IN  BOOLEAN  DefaultValue
	)
{
    PAGED_CODE();

    PREGISTRYKEY   DriverKey   = NULL;
    PREGISTRYKEY   SettingsKey = NULL;
    UNICODE_STRING KeyName;
    ULONG          Disposition;
    NTSTATUS       Status;
    BOOLEAN        Result = DefaultValue;

	const ULONG AllocSize =
    sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD);

    PKEY_VALUE_PARTIAL_INFORMATION Info =
        (PKEY_VALUE_PARTIAL_INFORMATION)
            ExAllocatePool(PagedPool, AllocSize);

	Status = PcNewRegistryKey( &DriverKey,               // IRegistryKey
                               NULL,                     // OuterUnknown
                               DriverRegistryKey,        // Registry key type
                               KEY_READ,				 // Access flags
                               m_pDeviceObject,          // Device object
                               NULL,                     // Subdevice
                               NULL,                     // ObjectAttributes
                               0,                        // Create options
                               NULL );                   // Disposition

    if (!NT_SUCCESS(Status))
        goto Exit;

    RtlInitUnicodeString(&KeyName, L"Settings");

    Status = DriverKey->NewSubKey(
        &SettingsKey,
        NULL,
        KEY_READ,
        &KeyName,
        REG_OPTION_NON_VOLATILE,
        &Disposition
    );
    if (!NT_SUCCESS(Status))
        goto Exit;

    if (!Info)
        goto Exit;

    RtlInitUnicodeString(&KeyName, ValueName);

    Status = SettingsKey->QueryValueKey(
        &KeyName,
        KeyValuePartialInformation,
        Info,
        AllocSize,
        &Disposition
    );

    if (NT_SUCCESS(Status) &&
        Info->DataLength >= sizeof(BYTE))
    {
        Result = (*(PBYTE)Info->Data) ? TRUE : FALSE;
    }

    ExFreePool(Info);

Exit:
    if (SettingsKey) SettingsKey->Release();
    if (DriverKey)   DriverKey->Release();

    return Result;
}


//***End of pageable code!***
//everything above this must have PAGED_CODE ();
#pragma code_seg() 

// Note: hda_check_headphone_connection_change has been moved to HDA_Codec class
// For backward compatibility, delegate to all codecs
STDMETHODIMP_(void) CAdapterCommon::hda_check_headphone_connection_change(void) {
	//TODO: schedule as a periodic task DPC
	//and make sure to clean up correctly on driver unload!
	for (int i = 0; i < codecCount; i++) {
		if (pCodecs[i] != NULL) {
			pCodecs[i]->hda_check_headphone_connection_change();
		}
	}
}


// Note: This function has been moved to HDA_Codec class
// For backward compatibility, check all codecs - only return TRUE if ALL support it
STDMETHODIMP_(UCHAR) CAdapterCommon::hda_is_supported_sample_rate(ULONG sample_rate) {
	if (codecCount == 0) {
		return FALSE;
	}
	for (int i = 0; i < codecCount; i++) {
		if (pCodecs[i] == NULL || !pCodecs[i]->hda_is_supported_sample_rate(sample_rate)) {
			return FALSE;
		}
	}
	return TRUE;
}


STDMETHODIMP_(ULONG) CAdapterCommon::hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command) {
	//DOUT (DBG_PRINT, ("[CAdapterCommon::hda_send_verb]"));

	//TODO: check sizes of components passed in
	//note that verbs and parameters are variable length so verbs are left aligned in the fields here
	//verbs which take a 16 bit payload will be shifted left like verb 2h will be verb 0x200 in this code. 

	//TODO: check for unsolicited responses and maybe schedule a DPC to deal with them
	//TODO: check for responses with high bit set (those are the errors)
	
	KIRQL oldirql;
	ULONG value = ((codec<<28) | (node<<20) | (verb<<8) | (command));
	//DOUT (DBG_SYSINFO, ("Write codec verb 0x%X position %d", value, CorbPointer));
	if (communication_type == HDA_CORB_RIRB) {

		ULONG response = STATUS_UNSUCCESSFUL;
		
		//CORB/RIRB interface

		//acquire spin lock: this isnt great to do as well as blocking but what can i do
		KeAcquireSpinLock(&QLock, &oldirql);

		//get current rirb pointer temp
		USHORT RirbTmp = readUSHORT (0x58);
		BOOLEAN valid = FALSE;
 
		//write verb
		WRITE_REGISTER_ULONG(CorbMemVirt + (CorbPointer), value);
  
		//move write pointer
		writeUSHORT(0x48, CorbPointer);
  
		//wait for RIRB pointer to increment/wrap

		for(ULONG ticks = 0; ticks < 600; ++ticks) {
			KeStallExecutionProcessor(10);
			if(readUSHORT (0x58) != RirbTmp) {
				valid = TRUE;
				break;
			} else if (ticks == 599) {
				//6 ms and no movement
				DOUT (DBG_ERROR, ("No Response to Codec Verb"));
				//communication_type = HDA_UNINITIALIZED;
			}
		}
		if (valid){

			//read response. each response is 8 bytes long but we only care about the lower 32 bits
			response = READ_REGISTER_ULONG (RirbMemVirt + (RirbPointer * 2));

			//move RIRB pointer **only if we got a response**

			//TODO: if we have usolicited responses on there may be more than 1 difference
			//between last read and last written
			RirbPointer++;
			if(RirbPointer == RirbNumberOfEntries) {
				RirbPointer = 0;
			}
		} 
		
		//move corb pointer
		CorbPointer++;
		if(CorbPointer == CorbNumberOfEntries) {
			CorbPointer = 0;
		}
		//unlock! must get here!
		KeReleaseSpinLock(&QLock, oldirql);

		//return response whatever it is. single point of exit
		return response;

	} else if (communication_type == HDA_PIO){
		
		//Immediate command interface
		//acquire spin lock: this isnt great to do as well as blocking but what can ya do
		KeAcquireSpinLock(&QLock, &oldirql);

		//DOUT (DBG_SYSINFO, ("Write codec verb Immediate:0x%X", value));
		//clear Immediate Result Valid bit
		writeUSHORT(0x68, 0x2);

		//write verb
		writeULONG(0x60, value);

		//start verb transfer
		writeUSHORT(0x68, 0x1);

		//poll for response
		ULONG ticks = 0;
		while (++ticks < 600) {
			KeStallExecutionProcessor(10);

			//wait for Immediate Result Valid bit = set and Immediate Command Busy bit = clear
			if ((readUSHORT(0x68) & 0x3) == 0x2) {
				//clear Immediate Result Valid bit
				writeUSHORT(0x68, 0x2);
				
				ULONG response = readULONG(0x64);
				//unlock!
				KeReleaseSpinLock(&QLock, oldirql);
				//return response
				return response;
			}
		}

		//there was no response after 6 ms
		//unlock
		KeReleaseSpinLock(&QLock, oldirql);
		DOUT (DBG_ERROR, ("\nHDA PIO ERROR: no response"));

		communication_type = HDA_UNINITIALIZED;
		return STATUS_UNSUCCESSFUL;
	} else {
		return STATUS_UNSUCCESSFUL;
	}
}


/*****************************************************************************
 * CAdapterCommon::ReadController()
 *****************************************************************************
 * Read a byte from the controller.
 * TODO: delete these entirely, nothing else uses them
 */
STDMETHODIMP_(BYTE)
CAdapterCommon::
ReadController
(   void
)
{
	DOUT (DBG_PRINT, ("[CAdapterCommon::ReadController]"));
    BYTE returnValue = BYTE(-1);

    return returnValue;
}


/*****************************************************************************
 * CAdapterCommon::WriteController()
 *****************************************************************************
 * Write a byte to the controller.
 * TODO: remove from interface
 */
STDMETHODIMP_(BOOLEAN)
CAdapterCommon::
WriteController
(
    IN      BYTE    Value
)
{
	DOUT (DBG_PRINT, ("[CAdapterCommon::WriteController]"));\
	DOUT (DBG_PRINT, ("trying to write %d", Value));

	BYTE returnValue = BYTE(-1);

    return returnValue;
}

/*****************************************************************************
 * CAdapterCommon::MixerRegWrite()
 *****************************************************************************
 * Writes a mixer register.
 */
STDMETHODIMP_(void)
CAdapterCommon::
MixerRegWrite
(
    IN      BYTE    index,
    IN      BYTE    Value
)
{

	DOUT (DBG_PRINT, ("[CAdapterCommon::MixerRegWrite]"));
	DOUT (DBG_PRINT, ("trying to write %d to %d", Value, index));

    // only hit the hardware if we're in an acceptable power state
    if( m_PowerState <= PowerDeviceD1 ) {
		if (index == 0 || index == 1) { //left or right channel respectively
			DOUT (DBG_PRINT, ("set volume of %d to %d", index, Value));
			hda_set_volume(Value, index + 1); //supposed to be 0-255 range
		}
#if (DBG)
		if ((index == 20) && (debug_kludge == 1)){
			//Terrible Kludge to reset the codec
			//and dump the codec config to the console
			//when the Master Volume, then Treble sliders are moved in Audio Properties
			//(in that order)
			InitHDAController();

		}
#endif
    }

	debug_kludge = index;


    if(index < DSP_MIX_MAXREGS)
    {
        MixerSettings[index] = Value;
    }
}

/*****************************************************************************
 * CAdapterCommon::MixerRegRead()
 *****************************************************************************
 * Reads a mixer register.
 */
STDMETHODIMP_(BYTE)
CAdapterCommon::
MixerRegRead
(
    IN      BYTE    Index
)
{
	DOUT (DBG_PRINT, ("[CAdapterCommon::MixerRegRead]"));
	DOUT (DBG_PRINT, ("read from mixer reg %d: %d", Index, MixerSettings[Index]));

    if(Index < DSP_MIX_MAXREGS)
    {
        return MixerSettings[Index];
    }

    return 0;
}

/*****************************************************************************
 * CAdapterCommon::MixerReset()
 *****************************************************************************
 * Resets the mixer
 */
STDMETHODIMP_(void)
CAdapterCommon::
MixerReset
(   void
)
{

	DOUT (DBG_PRINT, ("[CAdapterCommon::MixerReset]"));
    RestoreMixerSettingsFromRegistry();
}

/*****************************************************************************
 * CAdapterCommon::AcknowledgeIRQ()
 *****************************************************************************
 * Acknowledge any interrupt request
 */
HDA_INTERRUPT_TYPE CAdapterCommon::AcknowledgeIRQ()
{
    ULONG intsts = readULONG(0x24);

	if (intsts == 0xFFFFFFFF) {
		//Device glitch, MMIO Bus Error or controller shut down.
		DOUT(DBG_PRINT, ("**Glitch IRQ"));
        return HDAINT_NONE;
    }
	if (! (intsts & (1UL << 31))){
		//IRQ is NOT ours unless GIS=1
		return HDAINT_NONE;
	}

	BOOLEAN streamSeen = FALSE;
    BOOLEAN ctrlSeen   = FALSE;

    /* ---- Stream interrupts ---- */
    ULONG streamMask = intsts & 0x3FFFFFFF; // bits 0-29

    for (UCHAR stream = 0; stream < 30; ++stream) {
		    if (streamMask & (1UL << stream)) {

				UCHAR sdsts = readUCHAR(HDA_STREAMBASE(stream) + 3);

				if (sdsts) {
					// Acknowledge only asserted bits
					writeUCHAR(HDA_STREAMBASE(stream) + 3, sdsts);

					// If this is not a stream we manage, quiesce it
					if (stream != FirstOutputStream) {
						UCHAR ctl = readUCHAR(HDA_STREAMBASE(stream) + 0);
						ctl &= ~(SDCTL_RUN | SDCTL_IE);
						writeUCHAR(HDA_STREAMBASE(stream) + 0, ctl);
					}

				streamSeen = TRUE;
			}
        }
    }

    /* ---- Controller interrupts ---- */

    /* RIRB */
    UCHAR rirbsts = readUCHAR(0x5D);
    if (rirbsts & 0x05)   // response interrupt or overrun
    {
        writeUCHAR(0x5D, rirbsts);
        ctrlSeen = TRUE;
    }

    /* CORB */
    UCHAR corbsts = readUCHAR(0x4D);
    if (corbsts & 0x01)   // memory error
    {
        writeUCHAR(0x4D, corbsts);
        ctrlSeen = TRUE;
    }

    /* STATESTS */
    USHORT statests = readUSHORT(0x0E);
    if (statests)
    {
        writeUSHORT(0x0E, statests);
        ctrlSeen = TRUE;
    }

    if (streamSeen)
        return HDAINT_STREAM;

    if (ctrlSeen)
        return HDAINT_CONTROLLER;

    // GIS was set but nothing decoded. claim and move on
    DOUT(DBG_PRINT, ("Unexpected IRQ: INTSTS=%08lX", intsts));
    return HDAINT_CONTROLLER;
}

/*****************************************************************************
 * CAdapterCommon::ResetController()
 *****************************************************************************
 * Resets the controller.
 */
STDMETHODIMP_(NTSTATUS)
CAdapterCommon::
ResetController(void)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	DOUT (DBG_PRINT, ("[CAdapterCommon::ResetController]"));

	ntStatus = hda_stop_stream();

	ntStatus = InitHDAController();

    return ntStatus;
}

/*****************************************************************************
 * CAdapterCommon::RestoreMixerSettingsFromRegistry()
 *****************************************************************************
 * Restores the mixer settings based on settings stored in the registry.
 */
STDMETHODIMP
CAdapterCommon::
RestoreMixerSettingsFromRegistry
(   void
)
{
    PREGISTRYKEY    DriverKey;
    PREGISTRYKEY    SettingsKey;

    _DbgPrintF(DEBUGLVL_VERBOSE,("[RestoreMixerSettingsFromRegistry]"));
    
    // open the driver registry key
    NTSTATUS ntStatus = PcNewRegistryKey( &DriverKey,               // IRegistryKey
                                          NULL,                     // OuterUnknown
                                          DriverRegistryKey,        // Registry key type
                                          KEY_ALL_ACCESS,           // Access flags
                                          m_pDeviceObject,          // Device object
                                          NULL,                     // Subdevice
                                          NULL,                     // ObjectAttributes
                                          0,                        // Create options
                                          NULL );                   // Disposition
    if(NT_SUCCESS(ntStatus))
    {
        UNICODE_STRING  KeyName;
        ULONG           Disposition;
        
        // make a unicode strong for the subkey name
        RtlInitUnicodeString( &KeyName, L"Settings" );



        // open the settings subkey
        ntStatus = DriverKey->NewSubKey( &SettingsKey,              // Subkey
                                         NULL,                      // OuterUnknown
                                         KEY_ALL_ACCESS,            // Access flags
                                         &KeyName,                  // Subkey name
                                         REG_OPTION_NON_VOLATILE,   // Create options
                                         &Disposition );
        if(NT_SUCCESS(ntStatus))
        {
            ULONG   ResultLength;

            if(Disposition == REG_CREATED_NEW_KEY)
            {
                // copy default settings
                for(ULONG i = 0; i < SIZEOF_ARRAY(DefaultMixerSettings); i++)
                {
                    MixerRegWrite( DefaultMixerSettings[i].RegisterIndex,
                                   DefaultMixerSettings[i].RegisterSetting );
                }
            } else
            {
                // allocate data to hold key info
                PVOID KeyInfo = ExAllocatePool(PagedPool, sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD));
                if(NULL != KeyInfo)
                {
                    // loop through all mixer settings
                    for(UINT i = 0; i < SIZEOF_ARRAY(DefaultMixerSettings); i++)
                    {
                        // init key name
                        RtlInitUnicodeString( &KeyName, DefaultMixerSettings[i].KeyName );
        
                        // query the value key
                        ntStatus = SettingsKey->QueryValueKey( &KeyName,
                                                               KeyValuePartialInformation,
                                                               KeyInfo,
                                                               sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(DWORD),
                                                               &ResultLength );
                        if(NT_SUCCESS(ntStatus))
                        {
                            PKEY_VALUE_PARTIAL_INFORMATION PartialInfo = PKEY_VALUE_PARTIAL_INFORMATION(KeyInfo);
    
                            if(PartialInfo->DataLength == sizeof(DWORD))
                            {
                                // set mixer register to registry value
                                MixerRegWrite( DefaultMixerSettings[i].RegisterIndex,
                                               BYTE(*(PDWORD(PartialInfo->Data))) );
                            }
                        } else
                        {
                            // if key access failed, set to default
                            MixerRegWrite( DefaultMixerSettings[i].RegisterIndex,
                                           DefaultMixerSettings[i].RegisterSetting );
                        }
                    }
    
                    // free the key info
                    ExFreePool(KeyInfo);
                } else
                {
                    // copy default settings
                    for(ULONG i = 0; i < SIZEOF_ARRAY(DefaultMixerSettings); i++)
                    {
                        MixerRegWrite( DefaultMixerSettings[i].RegisterIndex,
                                       DefaultMixerSettings[i].RegisterSetting );
                    }

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                }
            }

            // release the settings key
            SettingsKey->Release();
        }

        // release the driver key
        DriverKey->Release();

    }

    return ntStatus;
}

/*****************************************************************************
 * CAdapterCommon::SaveMixerSettingsToRegistry()
 *****************************************************************************
 * Saves the mixer settings to the registry.
 */
STDMETHODIMP
CAdapterCommon::
SaveMixerSettingsToRegistry
(   void
)
{
    PREGISTRYKEY    DriverKey;
    PREGISTRYKEY    SettingsKey;

    _DbgPrintF(DEBUGLVL_VERBOSE,("[SaveMixerSettingsToRegistry]"));

	if(MixerSettings == NULL || DefaultMixerSettings == NULL) {
		return STATUS_UNSUCCESSFUL;
	}
    
    // open the driver registry key
    NTSTATUS ntStatus = PcNewRegistryKey( &DriverKey,               // IRegistryKey
                                          NULL,                     // OuterUnknown
                                          DriverRegistryKey,        // Registry key type
                                          KEY_ALL_ACCESS,           // Access flags
                                          m_pDeviceObject,          // Device object
                                          NULL,                     // Subdevice
                                          NULL,                     // ObjectAttributes
                                          0,                        // Create options
                                          NULL );                   // Disposition
    if(! NT_SUCCESS(ntStatus) || DriverKey == NULL) {
		return STATUS_UNSUCCESSFUL;
    }
    UNICODE_STRING  KeyName;
        
    // make a unicode strong for the subkey name
    RtlInitUnicodeString( &KeyName, L"Settings" );

    // open the settings subkey
    ntStatus = DriverKey->NewSubKey( &SettingsKey,              // Subkey
                                     NULL,                      // OuterUnknown
                                     KEY_ALL_ACCESS,            // Access flags
                                     &KeyName,                  // Subkey name
                                     REG_OPTION_NON_VOLATILE,   // Create options
                                     NULL );
    if(! NT_SUCCESS(ntStatus) || SettingsKey == NULL) {
		return STATUS_UNSUCCESSFUL;
		DriverKey->Release();
	}
    // loop through all mixer settings
    for(UINT i = 0; i < SIZEOF_ARRAY(DefaultMixerSettings); i++) {
        // init key name
        RtlInitUnicodeString( &KeyName, DefaultMixerSettings[i].KeyName );

		//ignore out of bounds mixer settings
		ULONG reg = DefaultMixerSettings[i].RegisterIndex;
		if (reg > SIZEOF_ARRAY(MixerSettings)){
			DOUT (DBG_ERROR, ("Out of bounds mixer setting! %d",reg));
			continue;
		}

        // set the key
        DWORD KeyValue = DWORD(MixerSettings[reg]);
        ntStatus = SettingsKey->SetValueKey( &KeyName,                 // Key name
                                             REG_DWORD,                // Key type
                                             PVOID(&KeyValue),
                                             sizeof(DWORD) );
        if(!NT_SUCCESS(ntStatus)) {
			break;
        }
	}

	// release the settings key
	SettingsKey->Release();

    // release the driver key
    DriverKey->Release();

    return ntStatus;
}

/*****************************************************************************
 * CAdapterCommon::ProgramSampleRate
 *****************************************************************************
 * Programs the sample rate for all outputs. 
 * If the rate cannot be programmed, the routine returns STATUS_UNSUCCESSFUL.
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::ProgramSampleRate
(
    IN  DWORD           dwSampleRate
	//Currently always stereo 16-bit, the KMixer can upconvert mono/8bit
)
{
    PAGED_CODE ();

	ULONG status = 0;

	//set sample rate on all codecs

    DOUT (DBG_PRINT, ("[CAdapterCommon::ProgramSampleRate]"));
	for (int i = 0; i < codecCount; i++) {
		if (pCodecs[i] != NULL) {
			status = pCodecs[i]->ProgramSampleRate(dwSampleRate);
			if(!NT_SUCCESS(status))
				return status;
		}
	}
	// if that's ok, set stream data format
	writeUSHORT(OutputStreamBase + 0x12, 
		hda_return_sound_data_format(dwSampleRate, 2, 16));

	// todo: adjust size of BDL chunks based on samplerate.
	// output gets crunchy if rate is set too low for the irq frequency
    
    DOUT (DBG_VSR, ("Samplerate changed to %d.", dwSampleRate));

    return STATUS_SUCCESS;
}

/*****************************************************************************
 * CAdapterCommon::PowerChangeState()
 *****************************************************************************
 * Change power state for the device.
 */
STDMETHODIMP_(void)
CAdapterCommon::
PowerChangeState
(
    IN      POWER_STATE     NewState
)
{
    UINT i;

    _DbgPrintF( DEBUGLVL_VERBOSE, ("[CAdapterCommon::PowerChangeState]"));

    // is this actually a state change??
    if( NewState.DeviceState != m_PowerState )
    {
        // switch on new state
        switch( NewState.DeviceState )
        {
            case PowerDeviceD0:
                // Insert your code here for entering the full power state (D0).
                // This code may be a function of the current power state.  Note that
                // property accesses such as volume and mute changes may occur when
                // the device is in a sleep state (D1-D3) and should be cached in
                // the driver to be restored upon entering D0.  However, it should
                // also be noted that new miniport and new streams will only be
                // attempted at D0 (portcls will place the device in D0 prior to the
                // NewStream call).
				_DbgPrintF(DEBUGLVL_VERBOSE,("  Entering D0 from",ULONG(m_PowerState)-ULONG(PowerDeviceD0)));

                // Save the new state.  This local value is used to determine when to cache
                // property accesses and when to permit the driver from accessing the hardware.

				//Re-init the codec if coming from D2 or D3
				if((ULONG(m_PowerState)-ULONG(PowerDeviceD0)) >= 2){
					InitHDAController();
				}

				// restore mixer settings
				for(i = 0; i < DSP_MIX_MAXREGS - 1; i++)
                {
                    if( i != DSP_MIX_MICVOLIDX )
                    {
                        MixerRegWrite( BYTE(i), MixerSettings[i] );
                    }
                }

            case PowerDeviceD1:
                // This sleep state is the lowest latency sleep state with respect to the
                // latency time required to return to D0.  The driver can still access
                // the hardware in this state if desired.  If the driver is not being used
                // an inactivity timer in portcls will place the driver in this state after
                // a timeout period controllable via the registry.
                
            case PowerDeviceD2:
                // This is a medium latency sleep state.  In this state the device driver
                // cannot assume that it can touch the hardware so any accesses need to be
                // cached and the hardware restored upon entering D0 (or D1 conceivably).
                
            case PowerDeviceD3:
                // This is a full hibernation state and is the longest latency sleep state.
                // The driver cannot access the hardware in this state and must cache any
                // hardware accesses and restore the hardware upon returning to D0 (or D1).
                
                // Save the new state.
                m_PowerState = NewState.DeviceState;

                _DbgPrintF(DEBUGLVL_VERBOSE,("  Entering D%d",ULONG(m_PowerState)-ULONG(PowerDeviceD0)));
                break;
    
            default:
                _DbgPrintF(DEBUGLVL_VERBOSE,("  Unknown Device Power State"));
                break;
        }
    }
}

/*****************************************************************************
 * CAdapterCommon::QueryPowerChangeState()
 *****************************************************************************
 * Query to see if the device can
 * change to this power state
 */
STDMETHODIMP_(NTSTATUS)
CAdapterCommon::
QueryPowerChangeState
(
    IN      POWER_STATE     NewStateQuery
)
{
    _DbgPrintF( DEBUGLVL_TERSE, ("[CAdapterCommon::QueryPowerChangeState]"));

    // Check here to see of a legitimate state is being requested
    // based on the device state and fail the call if the device/driver
    // cannot support the change requested.  Otherwise, return STATUS_SUCCESS.
    // Note: A QueryPowerChangeState() call is not guaranteed to always preceed
    // a PowerChangeState() call.

    return STATUS_SUCCESS;
}

/*****************************************************************************
 * CAdapterCommon::QueryDeviceCapabilities()
 *****************************************************************************
 * Called at startup to get the caps for the device.  This structure provides
 * the system with the mappings between system power state and device power
 * state.  This typically will not need modification by the driver.
 * 
 */
STDMETHODIMP_(NTSTATUS)
CAdapterCommon::
QueryDeviceCapabilities
(
    IN      PDEVICE_CAPABILITIES    PowerDeviceCaps
)
{
    _DbgPrintF( DEBUGLVL_TERSE, ("[CAdapterCommon::QueryDeviceCapabilities]"));

    return STATUS_SUCCESS;
}

/*****************************************************************************
 * InterruptServiceRoutine()
 *****************************************************************************
 * ISR.
 */

NTSTATUS InterruptServiceRoutine
(
    IN      PINTERRUPTSYNC  InterruptSync,
    IN      PVOID           DynamicContext
)
{

    ASSERT(DynamicContext);

    CAdapterCommon *that = (CAdapterCommon *) DynamicContext;

    //_DbgPrintF( DEBUGLVL_TERSE, ("***[CAdapterCommon::InterruptServiceRoutine]"));

	//
    // ACK the ISR. note we don't have any direct access to CAdapterCommon, gotta use the pointer
	// todo: how do we handle being called if the AdapterCommon object isn't inited yet?
	// todo: call from a DPC fallback if the IRQ does not appear to be firing properly

    // get out of here immediately if it's not our IRQ
	HDA_INTERRUPT_TYPE irqType = that->AcknowledgeIRQ();
	if(irqType == HDAINT_NONE) {
		return STATUS_UNSUCCESSFUL;
	} else if (irqType != HDAINT_STREAM) {
		return STATUS_SUCCESS; //without queuing the DPC
	}

    ASSERT(InterruptSync);
    ASSERT(that->m_pServiceGroupWave);

    //
    // Make sure there is a wave port driver.
    //
    if (that->m_pPortWave)
    {
        //
        // Tell it it needs to do some work.
        //
        that->m_pPortWave->Notify(that->m_pServiceGroupWave);
    }
    return STATUS_SUCCESS;
}

//stop stream and clear all stream registers
STDMETHODIMP_(NTSTATUS) CAdapterCommon::hda_stop_stream (void) {
	NTSTATUS ntStatus = STATUS_SUCCESS;

	DOUT (DBG_PRINT, ("[CAdapterCommon::hda_stop_stream]"));
    
	//turn off IRQs for stream 1
	
	//stop output DMA engine
	writeUCHAR(OutputStreamBase + 0x00, 0x00);
	ULONG ticks = 0;
	while(ticks++ < 40) {
		//wait till the run bit reads 0 to confirm it has stopped
		//should be within 40 us
		KeStallExecutionProcessor(1);
		if((readUCHAR(OutputStreamBase + 0x00) & SDCTL_RUN )== 0x0 ) {
			break;
		}
	}
	if((readUCHAR(OutputStreamBase + 0x00) & SDCTL_RUN ) == SDCTL_RUN) {
		DOUT (DBG_ERROR, ("HDA: can not stop stream"));
		ntStatus = STATUS_TIMEOUT;
	}
 
	//reset stream registers
	writeUCHAR(OutputStreamBase + 0x00, 0x01);
	ticks = 0;
	while(ticks++ < 10) {
		KeStallExecutionProcessor(1);
		if((readUCHAR(OutputStreamBase + 0x00) & 0x1)==0x1) {
			break;
		}
	}
	if((readUCHAR(OutputStreamBase + 0x00) & 0x1)==0x0) {
		DOUT (DBG_ERROR, ("HDA: can not start resetting stream"));
	}
	KeStallExecutionProcessor(5);
	writeUCHAR(OutputStreamBase + 0x00, 0x00);
	ticks = 0;
	while(ticks++<10) {
		KeStallExecutionProcessor(1);
			if((readUCHAR(OutputStreamBase + 0x00) & 0x1)==0x0) {
			break;
		}
	}
	if((readUCHAR(OutputStreamBase + 0x00) & 0x1)==0x1) {
		DOUT (DBG_ERROR, ("HDA: can not stop resetting stream"));
		ntStatus = STATUS_TIMEOUT;
	}
	KeStallExecutionProcessor(5);

	//clear error bits
	writeUCHAR(OutputStreamBase + 0x03, 0x1C);
    return ntStatus;
}

STDMETHODIMP_(void) CAdapterCommon::hda_start_sound(void) {
	//start playing output stream 1. With interrupts
	writeUCHAR(OutputStreamBase + 0x02, 0x14);
	writeUCHAR(OutputStreamBase + 0x00, 0x06);

	//
    // Make sure there is a wave port driver.
    //
    if (m_pPortWave)
    {
        //
        // Tell it it needs to do some work.
        //
        m_pPortWave->Notify(m_pServiceGroupWave);
    }
}

STDMETHODIMP_(void) CAdapterCommon::hda_stop_sound(void) {
	writeUCHAR(OutputStreamBase + 0x00, 0x00);
	ULONG ticks = 0;
	while(ticks++ < 40) {
		//wait till the run bit reads 0 to confirm it has stopped
		//should be within 40 us
		KeStallExecutionProcessor(1);
		if((readUCHAR(OutputStreamBase + 0x00) & 0x2)==0x0) {
			break;
		}
	}
	if((readUCHAR(OutputStreamBase + 0x00) & 0x2)==0x2) {
		DOUT (DBG_ERROR, ("HDA: can not stop stream"));
	}
}


STDMETHODIMP_(ULONG) CAdapterCommon::hda_get_actual_stream_position(void) {
	//todo: support multiple streams
	USHORT stream_id = FirstOutputStream; // stream 4 for most chipsets		
	if (useDmaPos){
		ULONG dpos = *(ULONG *)(((UCHAR *)DmaPosVirt) + (stream_id * 8));
		ULONG lpos = readULONG(OutputStreamBase + 0x04);

		//check if DMA position buffer is moving or if it's stuck at 0
		//TODO: dpos ever updating from 0 is not sufficient to confirm it works all the time
		// but IS sufficient to detect emulators with no support for dpos
		if (dpos != 0) {
			bad_dpos_count = 0;
			return dpos;
		} else if ((dpos == 0) && (lpos != 0)){
			//LPIB is moving but DMA isn't
			if (++bad_dpos_count > 9){
				DOUT (DBG_ERROR, ("DMA position buffer not working, falling back to LPIB"));
				useDmaPos = FALSE;
				return lpos;
			}
			return dpos;
		} else {
			return 0;
		}
	} else {
		//using LPIB
		return readULONG(OutputStreamBase + 0x04);
	}
}

// Note: hda_set_volume has been moved to HDA_Codec class
// For backward compatibility, delegate to all codecs
STDMETHODIMP_(void) CAdapterCommon::hda_set_volume(ULONG volume, UCHAR ch) {
	for (int i = 0; i < codecCount; i++) {
		if (pCodecs[i] != NULL) {
			pCodecs[i]->hda_set_volume(volume, ch);
		}
	}
}

STDMETHODIMP_(void) CAdapterCommon::hda_set_node_gain(ULONG codec, ULONG node, ULONG node_type, ULONG capabilities, ULONG gain, UCHAR ch) {
	//ch bit 0: left channel bit 1: right channel
	ch &= 3;
	ULONG payload = ch << 12;

	//set type of node
	if((node_type & HDA_OUTPUT_NODE) == HDA_OUTPUT_NODE) {
		payload |= 0x8000;
	}
	if((node_type & HDA_INPUT_NODE) == HDA_INPUT_NODE) {
		payload |= 0x4000;
	}

	//set number of gain
	if(gain == 0 && (capabilities & 0x80000000) == 0x80000000) {
		payload |= 0x80; //mute
	}
	else {
		payload |= (((capabilities>>8) & 0x7F)*gain/256); //recalculate range
	}

	//change gain
	hda_send_verb(codec, node, 0x300, payload);
}


//HDA controller register read and write functions

STDMETHODIMP_(UCHAR) CAdapterCommon::readUCHAR(USHORT reg)
{
	return READ_REGISTER_UCHAR((PUCHAR)(m_pHDARegisters + reg));
}

STDMETHODIMP_(void) CAdapterCommon::writeUCHAR(USHORT cmd, UCHAR value)
{
	WRITE_REGISTER_UCHAR((PUCHAR)(m_pHDARegisters + cmd), value);
}

STDMETHODIMP_(void) CAdapterCommon::setUCHARBit(USHORT reg, UCHAR flag)
{
	writeUCHAR(reg, readUCHAR(reg) | flag);
}

STDMETHODIMP_(void) CAdapterCommon::clearUCHARBit(USHORT reg, UCHAR flag)
{
	writeUCHAR(reg, readUCHAR(reg) & ~flag);
}

STDMETHODIMP_(USHORT) CAdapterCommon::readUSHORT(USHORT reg)
{
	return READ_REGISTER_USHORT((PUSHORT)(m_pHDARegisters + reg));
}

STDMETHODIMP_(void) CAdapterCommon::writeUSHORT(USHORT cmd, USHORT value)
{
	WRITE_REGISTER_USHORT((PUSHORT)(m_pHDARegisters + cmd), value);
}


STDMETHODIMP_(ULONG) CAdapterCommon::readULONG(USHORT reg)
{
	return READ_REGISTER_ULONG((PULONG)(m_pHDARegisters + reg));
}

STDMETHODIMP_(void) CAdapterCommon::writeULONG(USHORT cmd, ULONG value)
{
	WRITE_REGISTER_ULONG((PULONG)(m_pHDARegisters + cmd), value);
}

STDMETHODIMP_(void) CAdapterCommon::setULONGBit(USHORT reg, ULONG flag)
{
	writeULONG(reg, readULONG(reg) | flag);
}

STDMETHODIMP_(void) CAdapterCommon::clearULONGBit(USHORT reg, ULONG flag)
{
	writeULONG(reg, readULONG(reg) & ~flag);
}

STDMETHODIMP_(NTSTATUS) CAdapterCommon::hda_showtime(PDMACHANNEL DmaChannel) {
	
	int i = 0;
	NTSTATUS ntStatus = STATUS_SUCCESS;

	DOUT (DBG_PRINT, ("[CAdapterCommon::hda_showtime]"));

	//get the physical and virtual address pointers from the dma channel object
	BufLogicalAddress = DmaChannel->PhysicalAddress();
	BufVirtualAddress = DmaChannel->SystemAddress();
	audBufSize = DmaChannel->BufferSize();

	DOUT(DBG_SYSINFO, ("Audio Buffer Virt Addr = 0x%X,", BufVirtualAddress));
	DOUT(DBG_SYSINFO, ("Audio Buffer Phys Addr = 0x%X,", BufLogicalAddress));
	DOUT(DBG_SYSINFO, ("Audio Buffer Size = %d,", audBufSize));

    //
    // Initialize the device state.
    //
    m_PowerState = PowerDeviceD0;

	DOUT(DBG_SYSINFO, ("reset streams"));

	ntStatus = hda_stop_stream ();
	if (!NT_SUCCESS (ntStatus)){
        DOUT (DBG_ERROR, ("Can't reset stream"));
        return ntStatus;
    }
	//ProgramSampleRate(44100);
	
	//divide the buffer into <entries> chunks (must be a power of 2)
	
	USHORT entries = 64;
	for(i = 0; i < (entries * 4); i += 4){
		BdlMemVirt[i+0] = BufLogicalAddress.LowPart + (i/4)*(audBufSize/entries);
		BdlMemVirt[i+1] = BufLogicalAddress.HighPart;
		BdlMemVirt[i+2] = audBufSize / entries;
		BdlMemVirt[i+3] = BDLE_FLAG_IOC; //interrupt on completion ON
	}
	
	//fill BDL entries out with 10 ms buffer chunks (1792 bytes at 44100)
	//this does not work on Virtualbox - do buffers really need to be power of 2 secretly?
	/*
	BDLE* Bdl = reinterpret_cast<BDLE*>(BdlMemVirt);
	PHYSICAL_ADDRESS BasePhys = BufLogicalAddress;
    ULONG offset = 0;
    USHORT entries = 0;

    while ((offset + CHUNK_SIZE) <= audBufSize && entries < 256)
    {
        Bdl[entries].Address = BasePhys.QuadPart + offset;
        Bdl[entries].Length  = CHUNK_SIZE;
        Bdl[entries].Flags   = BDLE_FLAG_IOC;     // interrupt every ~10 ms
        offset += CHUNK_SIZE;
        entries++;
    }

    //handle any leftover < CHUNK_SIZE tail
    ULONG remainder = audBufSize - offset;
    if (remainder >= 128) {
        remainder &= ~127;  // trim to 128-byte boundary
        Bdl[entries].Address = BasePhys.QuadPart + offset;
        Bdl[entries].Length  = remainder;
        Bdl[entries].Flags   = BDLE_FLAG_IOC;
        entries++;
    }
	*/
	
	//let's print enough of the BDL and make sure we've got it right
	/*
	for(i = 0; i < ((int)BdlSize); i += 4){
		DOUT(DBG_SYSINFO, 
		("BDL %d: Phys Addr 0x%08lX %08lX Length %d Flags %X", 
				(i/4), BdlMemVirt[i+1], BdlMemVirt[i], BdlMemVirt[i+2], BdlMemVirt[i+3]));
	}
	*/
	

	DOUT(DBG_SYSINFO, ("BDL all set up"));

	//KeFlushIoBuffers is defined to nothing in the NT DDK but exists in the 98 DDK
	KeFlushIoBuffers(mdl, FALSE, TRUE); 
	//flush processor cache to RAM to be sure sound card will read correct data
	//absolute performance killer so only doing this ONCE.
	__asm {
		wbinvd;
	}

	//set buffer registers
	writeULONG(OutputStreamBase + 0x18, BdlMemPhys.LowPart);
	writeULONG(OutputStreamBase + 0x1C, BdlMemPhys.HighPart);
	writeULONG(OutputStreamBase + 0x08, audBufSize);
	writeUSHORT(OutputStreamBase + 0x0C, entries - 1); //there are entries-1 entries in buffer

	DOUT(DBG_SYSINFO, ("buffer address programmed"));

	KeStallExecutionProcessor(10);

	DOUT(DBG_SYSINFO, ("ready to start the stream"));

	//clear pending codec interrupts once, we do not care
	UCHAR rirbsts = readUCHAR(0x5D);
	DOUT(DBG_SYSINFO, ("rirb sts %X",rirbsts));
	writeUCHAR(0x5D, 0x5);

	//enable interrupts from output stream 1
	//TODO account for other streams in use
	writeULONG(0x20, ((1 << 31) | (1 << FirstOutputStream)) ); 

	DOUT(DBG_SYSINFO, ("showtime"));

	// wait for Play state to actually start the stream though.
	
    return ntStatus;
}

STDMETHODIMP_(USHORT) CAdapterCommon::hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample) {
 USHORT data_format = 0;

 //channels
 data_format = (USHORT)((channels-1) & 0xf);

 //bits per sample
 if(bits_per_sample==16) {
  data_format |= ((0x1)<<4);
 }
 else if(bits_per_sample==20) {
  data_format |= ((0x2)<<4);
 }
 else if(bits_per_sample==24) {
  data_format |= ((0x3)<<4);
 }
 else if(bits_per_sample==32) {
  data_format |= ((0x4)<<4);
 }

 //sample rate
 if (sample_rate == 48000) {
  data_format |= ((0x0)<<8);
 }
 //24000 is supported as a stream rate but NOT a codec format
 else if(sample_rate==16000) {
  data_format |= ((0x2)<<8);
 }
 else if(sample_rate==8000) {
  data_format |= ((0x5)<<8);
 }
 else if(sample_rate==44100) {
  data_format |= ((0x40)<<8);
 }
 else if(sample_rate==22050) {
  data_format |= ((0x41)<<8);
 }
 else if(sample_rate==11025) {
  data_format |= ((0x43)<<8);
 }
 else if(sample_rate==96000) {
  data_format |= ((0x8)<<8);
 }
 else if(sample_rate==32000) {
  data_format |= ((0xA)<<8);
 }
 else if(sample_rate==88200) {
  data_format |= ((0x48)<<8);
 }
 else if(sample_rate==176400) {
  data_format |= ((0x58)<<8);
 }
 else if(sample_rate==192000) {
  data_format |= ((0x18)<<8);
 }
 return data_format;
}

/* Function to read/write PCI configuration space using IRP
from this KB article:
https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/obtaining-device-configuration-information-at-irql---passive-level
it's the only working example i found.
*/

STDMETHODIMP_(NTSTATUS)
CAdapterCommon::ReadWriteConfigSpace(
    IN PDEVICE_OBJECT  DeviceObject,
    IN ULONG  ReadOrWrite,  // 0 for read, 1 for write
    IN PVOID  Buffer, //must be in NONpaged heap
    IN ULONG  Offset,
    IN ULONG  Length
    )
{
    KEVENT event;
    NTSTATUS status;
    PIRP irp;
    IO_STATUS_BLOCK ioStatusBlock;
    PIO_STACK_LOCATION irpStack;
    PDEVICE_OBJECT targetObject;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    targetObject = IoGetAttachedDeviceReference(DeviceObject);
    irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
                                       targetObject,
                                       NULL,
                                       0,
                                       NULL,
                                       &event,
                                       &ioStatusBlock);
    if (irp == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }
    irpStack = IoGetNextIrpStackLocation(irp);
    if (ReadOrWrite == 0) {
        irpStack->MinorFunction = IRP_MN_READ_CONFIG;
    } else {
        irpStack->MinorFunction = IRP_MN_WRITE_CONFIG;
    }
    irpStack->Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
    irpStack->Parameters.ReadWriteConfig.Buffer = Buffer;
    irpStack->Parameters.ReadWriteConfig.Offset = Offset;
    irpStack->Parameters.ReadWriteConfig.Length = Length;

    // Initialize the status to error in case the bus driver does not 
    // set it correctly.
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    status = IoCallDriver(targetObject, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatusBlock.Status;
    }
End:
    // Done with reference
    ObDereferenceObject(targetObject);
    return status;
}

//set flag in our mirror of configspace and then write it back
STDMETHODIMP_(NTSTATUS) CAdapterCommon::WriteConfigSpaceByte(UCHAR offset, UCHAR andByte, UCHAR orByte){
	ASSERT(m_pDeviceObject);
	ASSERT(pConfigMem);
	pConfigMem[offset] = (pConfigMem[offset] & andByte) | orByte;
	return ReadWriteConfigSpace(
		m_pDeviceObject,
		1,			//write
		pConfigMem + offset,	// Buffer to store the configuration data		
		offset,			// Offset into the configuration space
		1         // Number of bytes to write
		);
}
  

//set flag in our mirror of configspace and then write it back
STDMETHODIMP_(NTSTATUS) CAdapterCommon::WriteConfigSpaceWord(UCHAR offset, USHORT andWord, USHORT orWord){
	ASSERT(m_pDeviceObject);
	ASSERT(pConfigMem);
	//cast to get a PUSHORT at arbitrary offset
	PUSHORT ptemp = (PUSHORT)(&pConfigMem[offset]);
	*ptemp = (*ptemp & andWord) | orWord;
	return ReadWriteConfigSpace(
		m_pDeviceObject,
		1,			//write
		pConfigMem + offset,	// Buffer to store the configuration data		
		offset,			// Offset into the configuration space
		2         // Number of bytes to write
		);
}

