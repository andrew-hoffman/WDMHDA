/*****************************************************************************
 * common.cpp - Common code used by all the HDA miniports.
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation.  All rights reserved.
 *
 * Implmentation of the common code object.  This class deals with interrupts
 * for the device, and is a collection of common code used by all the
 * miniports.
 */

#include "stdunk.h"
#include "common.h"


#define STR_MODULENAME "HDAAdapter: "




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
	//HDA_DEVICE_EXTENSION m_DevExt;		// Device extension
	PUCHAR m_pHDARegisters;     // MMIO registers
	PUCHAR Base;
    USHORT InputStreamBase;
    USHORT OutputStreamBase;
	PMDL mdl;
	KSPIN_LOCK QLock;

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

    PINTERRUPTSYNC          m_pInterruptSync;
    PUCHAR                  m_pWaveBase;
    PUCHAR                  m_pUartBase;
    PPORTWAVECYCLIC         m_pPortWave;
    PPORTDMUS               m_pPortUart;
    PSERVICEGROUP           m_pServiceGroupWave;
    ULONGLONG               m_startTime;
    BOOL                    m_bCaptureActive;
    BYTE                    MixerSettings[DSP_MIX_MAXREGS];

    void AcknowledgeIRQ
    (   void
    );
    BOOLEAN AdapterISR
    (   void
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


	STDMETHODIMP_(NTSTATUS) InitHDA (void);
	STDMETHODIMP_(NTSTATUS) InitializeCodec (UCHAR num);

	STDMETHODIMP_(ULONG)	hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command);
	STDMETHODIMP_(PULONG)	get_bdl_mem(void);
	STDMETHODIMP_(void)		hda_initialize_audio_function_group(ULONG codec_number, ULONG afg_node_number); 
	STDMETHODIMP_(ULONG)	hda_get_actual_stream_position(void);
	STDMETHODIMP_(UCHAR)	hda_get_node_type(ULONG codec, ULONG node);
	STDMETHODIMP_(ULONG)	hda_get_node_connection_entry(ULONG codec, ULONG node, ULONG connection_entry_number);
	STDMETHODIMP_(void)		hda_initialize_output_pin(ULONG pin_node_number);
	STDMETHODIMP_(void)		hda_initalize_audio_output(ULONG audio_output_node_number);
	STDMETHODIMP_(void)		hda_initalize_audio_mixer(ULONG audio_mixer_node_number);
	STDMETHODIMP_(void)		hda_initalize_audio_selector(ULONG audio_selector_node_number);
	STDMETHODIMP_(BOOL)		hda_is_headphone_connected (void);
	STDMETHODIMP_(void)		hda_set_node_gain(ULONG codec, ULONG node, ULONG node_type, ULONG capabilities, ULONG gain);
	STDMETHODIMP_(void)		hda_set_volume(ULONG volume);
	STDMETHODIMP_(void)		hda_start_sound (void);
	STDMETHODIMP_(void)		hda_stop_sound (void);

	STDMETHODIMP_(void)		hda_check_headphone_connection_change(void);
	STDMETHODIMP_(UCHAR)	hda_is_supported_channel_size(UCHAR size);
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
	ULONG audBufSize = 65536; 
	ULONG BdlSize = 256 * 16 * 2; //256 entries, 16 bytes, x2 for shadow bdl;

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

	//init spin lock for protecting the CORB/RIRB
	KeInitializeSpinLock(&QLock);

	//is there anything in config space we need to set?

	//there may be multiple instances of this driver loaded at once
	//on systems with HDMI display audio support for instance
	//but it should be one driver object per HDA controller.
	//which may access multiple codecs but for now only sending 1 audio stream to all
	//hardware mixing may come, MUCH later.

    //
    //Get the memory base address for the HDA controller registers. 
    //

    // Get memory resource - note we only want the first BAR
	// on Skylake and newer mobile chipsets,
	// there is a DSP interface at BAR2 for Intel Smart Sound Technology
	// TODO: fix adapter.cpp to check for this better

	DOUT(DBG_SYSINFO, ("%d Resources in List", ResourceList->NumberOfEntries() ));

    for (ULONG i = 0; i < ResourceList->NumberOfEntries(); i++) {
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

	//Map the device memory into kernel space
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
	for (i = 0; i < 16; i++) {
		DOUT(DBG_SYSINFO, ("Reg %d 0x%X", i, ((PUCHAR)m_pHDARegisters)[i]  ));
	}
#endif
	//read capabilities
	USHORT caps = readUSHORT(0x00);

	//check 64OK flag
	is64OK = caps & 1;

	//TODO check that we have at least 1 output or bidirectional stream engine

	DOUT( DBG_SYSINFO, ("Version: %d.%d", readUCHAR(0x03), readUCHAR(0x02) ));

	//offsets for stream engines
	InputStreamBase = (0x80);
	OutputStreamBase = (0x80 + (0x20 * ((caps >> 8) & 0xF))); //skip input streams ports

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
	pDeviceDescription -> ScatterGather		= FALSE; // for THIS purpose it can't
	pDeviceDescription -> DemandMode		= FALSE;
	pDeviceDescription -> AutoInitialize	= FALSE;
	pDeviceDescription -> Dma32BitAddresses = TRUE;
	pDeviceDescription -> IgnoreCount		= FALSE;
	pDeviceDescription -> Reserved1			= FALSE;
	pDeviceDescription -> Dma64BitAddresses = is64OK; //it might. doesnt matter to win98
	pDeviceDescription -> DmaChannel		= 0;
	pDeviceDescription -> InterfaceType		= PCIBus;
	pDeviceDescription -> MaximumLength		= audBufSize + BdlSize + 8192;

	//number of "map registers" doesn't matter at all here, but i need somewhere to put it
	ULONG nMapRegisters = 0;

	DMA_Adapter = IoGetDmaAdapter (
		m_pDeviceObject,
		pDeviceDescription,
		&nMapRegisters );

	DOUT(DBG_SYSINFO, ("Map Registers = %d", nMapRegisters));

	//now we call the AllocateCommonBuffer function pointer in that struct

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

	//same for CORB

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

	//same for BDL

	PVOID BdlVirtualAddress = NULL;
	PPHYSICAL_ADDRESS pBdlLogicalAddress = NULL;
	
	
	BdlVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		BdlSize,
		pBdlLogicalAddress, //out param
		FALSE ); //CacheEnabled is probably ignored

	BdlMemPhys = *pBdlLogicalAddress;
	BdlMemVirt = (PULONG) BdlVirtualAddress;

	//same for DMA Position buffer

	PVOID DmaVirtualAddress = NULL;
	PPHYSICAL_ADDRESS pDmaLogicalAddress = NULL;
	
	
	DmaVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		1024,
		pDmaLogicalAddress, //out param
		FALSE ); //CacheEnabled is probably ignored

	DmaPosPhys = *pDmaLogicalAddress;
	DmaPosVirt = (PULONG) DmaVirtualAddress;


	//map the audio buffer too
	//we should be doing this through creating a DmaChannel but we dont have that yet
	//not sure if that can give me the desired 128 byte phys alignment but we might
	//get that automatically just by mapping more than a 4k page worth.
	/*

	PPHYSICAL_ADDRESS pBufLogicalAddress = NULL;
	BufVirtualAddress = NULL;
	
	BufVirtualAddress = DMA_Adapter->DmaOperations->AllocateCommonBuffer (
		DMA_Adapter,
		audBufSize,
		pBufLogicalAddress, //out param
		FALSE );
	BufLogicalAddress = *pBufLogicalAddress;

	DOUT(DBG_SYSINFO, ("Buffer Virt Addr = 0x%X,", BufVirtualAddress));
	DOUT(DBG_SYSINFO, ("Buffer Phys Addr = 0x%X,", BufLogicalAddress));

	if (!BufVirtualAddress) {
		DOUT(DBG_ERROR, ("Couldn't map virt Buffer Space"));
		return STATUS_BUFFER_TOO_SMALL;
	}
	if (BufLogicalAddress.QuadPart == 0) {
		DOUT(DBG_ERROR, ("Couldn't map phys Buffer Space"));
		return STATUS_NO_MEMORY;
	}
	*/

	//
    // Reset the controller and init registers
    //
	ntStatus = InitHDA ();

    if (!NT_SUCCESS (ntStatus))
        return ntStatus;
	    if(NT_SUCCESS(ntStatus))
    {
        _DbgPrintF(DEBUGLVL_VERBOSE,("ResetController Succeeded"));
        AcknowledgeIRQ();
    
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
        if (NT_SUCCESS(ntStatus) && m_pInterruptSync)
        {                                                                       //  run this ISR first
            ntStatus = m_pInterruptSync->RegisterServiceRoutine(InterruptServiceRoutine,PVOID(this),FALSE);
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = m_pInterruptSync->Connect();
            }

            // if we could not connect or register the ISR, release the object.
            if (!NT_SUCCESS (ntStatus))
            {
                m_pInterruptSync->Release();
                m_pInterruptSync = NULL;
            }
        }
    } else
    {
        _DbgPrintF(DEBUGLVL_TERSE,("ResetController Failure"));
    }
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
	//Free audio buffer
	if((BufVirtualAddress != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing audio buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					audBufSize,
                      BufLogicalAddress,
                      BufVirtualAddress,
                      FALSE );
	}

	//free DMA buffers
	if((RirbMemVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing rirb buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      RirbMemPhys,
                      RirbMemVirt,
                      FALSE );
	}
	if((CorbMemVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing Corb buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      CorbMemPhys,
                      CorbMemVirt,
                      FALSE );
	}
	if((BdlMemVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing Bdl buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      BdlMemPhys,
                      BdlMemVirt,
                      FALSE );
	}
	if((DmaPosVirt != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing Dma buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					2048,
                      DmaPosPhys,
                      DmaPosVirt,
                      FALSE );
	}

	//free DMA adapter object
	if (DMA_Adapter) {
		DMA_Adapter->DmaOperations->PutDmaAdapter(DMA_Adapter);
		DMA_Adapter = NULL;
	}
	//free device description
	if (pDeviceDescription) {
		ExFreePool(pDeviceDescription);
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
 * CAdapterCommon::InitHDA
 *****************************************************************************
 * Initialize the HDA controller. Only call this from IRQL = Passive
 */

NTSTATUS CAdapterCommon::InitHDA (void)
{
    PAGED_CODE ();
    
	NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;

    DOUT (DBG_PRINT, ("[CAdapterCommon::InitHDA]"));

	//TODO: try to stop all possible running streams before resetting

	//we're not supposed to write to any registers before reset

	//Reset the whole controller. Spec says this can take up to 521 us

	writeUCHAR(0x08,0x0);

	for (int i = 0; i < 10; i++) {
		KeStallExecutionProcessor(100);
		if ((readUCHAR(0x08) & 0x1) == 0x0) {
			DOUT(DBG_SYSINFO, ("Controller Reset Started %d", i));
			break;
		} else if (i == 9) {
			DOUT (DBG_ERROR, ("Controller Not Responding Reset Start"));
			return STATUS_TIMEOUT;
		}
	}

	KeStallExecutionProcessor(100);
	writeUCHAR(0x08,0x1);

	for (i = 0; i < 10; i++) {
		KeStallExecutionProcessor(100);
		if ((readUCHAR(0x08) & 0x1) == 0x1) {
			DOUT(DBG_SYSINFO, ("Controller Reset Complete %d", i));
			break;
		}
		else if (i == 9) {
			DOUT (DBG_ERROR, ("Controller Not Responding Reset Complete"));
			return STATUS_TIMEOUT;
		}
	}

	//disable interrupts
	writeULONG (0x20, 0);
 
	//turn off dma position transfer
	writeULONG (0x70, 0);
	writeULONG (0x74, 0);
 
	//disable synchronization
	writeULONG (0x34, 0);
	writeULONG (0x38, 0);

	//stop CORB and RIRB
	writeUCHAR (0x4C, 0x0);
	writeUCHAR (0x5C, 0x0);

	//TODO: cache flush needed here?

	//write the needed logical addresses for CORB/RIRB to the HDA controller registers

	//corb addr
	writeULONG (0x40, CorbMemPhys.LowPart);
	if (is64OK){
		writeULONG (0x44, CorbMemPhys.HighPart);
	}
	else {
		writeULONG (0x44, 0);
	}
	//corb number of entries (each entry is 4 bytes)
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
		//i'm not gonna bother for now since CORB/RIRB support is required for
		//UAA compliant hardware, eg. anything with existing Windows drivers
		DOUT (DBG_ERROR, ("CORB only supports PIO"));
		return STATUS_NOT_IMPLEMENTED;
	}
	//Reset corb read pointer
	writeUSHORT (0x4A, 0x8000); //write a 1 to bit 15
	for (i = 0; i < 5; i++) {
		KeStallExecutionProcessor(10);
		if ((readUSHORT(0x4A) & 0x8000) == 0x8000) break;
		//read back the 1 to verify reset
	}
	if ((readUSHORT(0x4A) & 0x8000) == 0x0000){
		DOUT (DBG_ERROR, ("Controller Not Responding to CORB Pointer Reset"));
		return STATUS_TIMEOUT;
	}

	//then write a 0 and read back the 0 to verify a clear
	writeUSHORT(0x4A, 0x0000);
	for (i = 0; i < 5; i++) {
		KeStallExecutionProcessor(10);
		if ((readUSHORT(0x4A) & 0x8000) == 0x0000) break;
	}
	if ((readUSHORT(0x4A) & 0x8000) == 0x8000){
		DOUT (DBG_ERROR, ("Controller Not Responding to CORB Pointer Reset"));
		return STATUS_TIMEOUT;
	}

	writeUSHORT (0x48,0);
	CorbPointer = 1; //always points to next free entry

	//rirb addr

	writeULONG (0x50, RirbMemPhys.LowPart);
	if (is64OK){
		writeULONG (0x54, RirbMemPhys.HighPart);
	}
	else {
		writeULONG (0x54, 0);
	}
	//rirb number of entries (each entry is 8 bytes)
	
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
		return STATUS_NOT_IMPLEMENTED;
	}

	//reset RIRB write pointer to 0th entry
	writeUSHORT (0x58, 0x8000);

	//dma position buffer pointer
	writeULONG (0x70, DmaPosPhys.LowPart);
	if (is64OK){
		writeULONG (0x74, DmaPosPhys.HighPart);
	}
	else {
		writeULONG (0x74, 0);
	}


	KeStallExecutionProcessor(10);
	writeULONG(0x5A, 0); //disable interrupts
	RirbPointer = 1; //always points to next free entry
	
	//start CORB and RIRB. i think this also makes the HDA controller zero them
	writeUCHAR ( 0x4C, 0x2);
	writeUCHAR ( 0x5C, 0x2);

	//TODO: i will need 2 implementations of send_codec_verb
	//one immediate that stalls, and one with a callback
	//maybe take more than one verb at once w/o blocking

	//find codecs on the link.
	//this can also be done by checking bits in STATESTS register

	ULONG codec_id = 0;

	for(UCHAR codec_number = 0;  codec_number < 15; codec_number++) {
		//communication_type = HDA_CORB_RIRB;
		codec_id = hda_send_verb(codec_number, 0, 0xF00, 0);
		DOUT (DBG_SYSINFO, ("codec %d response 0x%X", codec_number, codec_id));

		if(codec_id != 0) {
			DOUT (DBG_SYSINFO, ("HDA: Codec %d CORB/RIRB communication interface", codec_id));

			//initialize the first codec we find that gives a nonzero response
			//TODO: init all codecs on link
			ntStatus = InitializeCodec(codec_number);
		
		break; //initalization is complete
		}
	}

    //ok now if we got here we have a working link
	//and we're ready to go init whatever codec we found

//	NTSTATUS ntStatus = PrimaryCodecReady ();
    if (!NT_SUCCESS (ntStatus))
    {        
        DOUT (DBG_ERROR, ("Initialization of HDA CoDec failed."));
    }



    return ntStatus;
}

/*****************************************************************************
 * CAdapterCommon::InitializeCodec
 *****************************************************************************
 */

NTSTATUS CAdapterCommon::InitializeCodec (UCHAR codec_number)
{
    PAGED_CODE ();

	 //test if this codec exist
	ULONG codec_id = hda_send_verb(codec_number, 0, 0xF00, 0);
	if(codec_id == 0x00000000) {
		return STATUS_NOT_FOUND;
	}
	codecNumber = codec_number;

	//log basic codec info
	DOUT (DBG_SYSINFO, ("Codec %d ", codec_number ) );
	DOUT (DBG_SYSINFO, ("VID: %04x", (codec_id>>16) ));
	DOUT (DBG_SYSINFO, ("PID: %04x", (codec_id & 0xFFFF) ));

	//find Audio Function Groups
	ULONG response = hda_send_verb(codec_number, 0, 0xF00, 0x04);

	DOUT (DBG_SYSINFO, ( "First Group node: %d Number of Groups: %d", 
	 (response >> 16) & 0xFF, response & 0xFF 
	 ));
	ULONG node = (response >> 16) & 0xFF;
	for(ULONG last_node = (node + (response & 0xFF)); node < last_node; node++) {
		if((hda_send_verb(codec_number, node, 0xF00, 0x05) & 0x7F)==0x01) { //this is Audio Function Group
		hda_initialize_audio_function_group(codec_number, node); //initialize Audio Function Group
		return STATUS_SUCCESS;
		}
	}
	DOUT (DBG_ERROR, ("HDA ERROR: No AFG found"));
	return STATUS_NOT_FOUND;	
}

/*****************************************************************************
 * CAdapterCommon::initialize_audio_function_group
 *****************************************************************************
 */

void CAdapterCommon::hda_initialize_audio_function_group(ULONG codec_number, ULONG afg_node_number) {
	//reset AFG
	hda_send_verb(codec_number, afg_node_number, 0x7FF, 0x00);

	//enable power for AFG
	hda_send_verb(codec_number, afg_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codec_number, afg_node_number, 0x708, 0x00);

	//read available info
	afg_node_sample_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x0A);
	afg_node_stream_format_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x0B);
	afg_node_input_amp_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x0D);
	afg_node_output_amp_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x12);

	//log AFG info
	DOUT (DBG_SYSINFO, ("\nAudio Function Group node %d", afg_node_number));
	DOUT (DBG_SYSINFO, ("\nAFG sample capabilities: 0x%x", afg_node_sample_capabilities));
	DOUT (DBG_SYSINFO, ("\nAFG stream format capabilities: 0x%x", afg_node_stream_format_capabilities));
	DOUT (DBG_SYSINFO, ("\nAFG input amp capabilities: 0x%x", afg_node_input_amp_capabilities));
	DOUT (DBG_SYSINFO, ("\nAFG output amp capabilities: 0x%x", afg_node_output_amp_capabilities));

	//log all AFG nodes and find useful PINs
	DbgPrint( ("\n\nLIST OF ALL NODES IN AFG:"));
	ULONG subordinate_node_count_reponse = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x04);
	ULONG pin_alternative_output_node_number = 0, pin_speaker_default_node_number = 0, pin_speaker_node_number = 0, pin_headphone_node_number = 0;
	pin_output_node_number = 0;
	pin_headphone_node_number = 0;

	for (ULONG node = ((subordinate_node_count_reponse>>16) & 0xFF), last_node = (node+(subordinate_node_count_reponse & 0xFF)), 
		type_of_node = 0; node < last_node; node++) {

		//log number of node
		DbgPrint("\n%d ", node);
  
		//get type of node
		type_of_node = hda_get_node_type(codec_number, node);

		//process node
		//TODO: can i express this more compactly as a switch statement?

		if(type_of_node == HDA_WIDGET_AUDIO_OUTPUT) {
			DbgPrint( ("Audio Output"));

			//disable every audio output by connecting it to stream 0
			hda_send_verb(codec_number, node, 0x706, 0x00);
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_INPUT) {
			DbgPrint( ("Audio Input"));
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_MIXER) {
			DbgPrint( ("Audio Mixer"));
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_SELECTOR) {
			DbgPrint( ("Audio Selector"));
		}
		else if(type_of_node == HDA_WIDGET_PIN_COMPLEX) {
			DbgPrint( ("Pin Complex "));

			//read type of PIN
			type_of_node = ((hda_send_verb(codec_number, node, 0xF1C, 0x00) >> 20) & 0xF);

			if(type_of_node == HDA_PIN_LINE_OUT) {
				DbgPrint( ("Line Out"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_SPEAKER) {
				DbgPrint( ("Speaker "));

				//first speaker node is default speaker
				if(pin_speaker_default_node_number==0) {
					pin_speaker_default_node_number = node;
				}

				//find if there is device connected to this PIN
				if((hda_send_verb(codec_number, node, 0xF00, 0x09) & 0x4)==0x4) {
					//find if it is jack or fixed device
					if((hda_send_verb(codec_number, node, 0xF1C, 0x00)>>30)!=0x1) {
						//find if is device output capable
						if((hda_send_verb(codec_number, node, 0xF00, 0x0C) & 0x10)==0x10) {
							//there is connected device
							DbgPrint( ("connected output device"));
							//we will use first pin with connected device, so save node number only for first PIN
							if(pin_speaker_node_number==0) {
								pin_speaker_node_number = node;
							}
						} else {
							DbgPrint( ("not output capable"));
						}
					} else {
						DbgPrint( ("not jack or fixed device"));
					}
				} else {
					DbgPrint( ("no output device"));
				}
			} else if(type_of_node == HDA_PIN_HEADPHONE_OUT) {
				DbgPrint( ("Headphone Out"));

				//save node number
				//TODO: handle if there are multiple HP nodes
				pin_headphone_node_number = node;
			} else if(type_of_node == HDA_PIN_CD) {
				DbgPrint( ("CD"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_SPDIF_OUT) {
				DbgPrint( ("SPDIF Out"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_DIGITAL_OTHER_OUT) {
				DbgPrint( ("Digital Other Out"));
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_MODEM_LINE_SIDE) {
				DbgPrint( ("Modem Line Side"));

				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_MODEM_HANDSET_SIDE) {
				DbgPrint( ("Modem Handset Side"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_LINE_IN) {
				DbgPrint( ("Line In"));
			} else if(type_of_node == HDA_PIN_AUX) {
				DbgPrint( ("AUX"));
			} else if(type_of_node == HDA_PIN_MIC_IN) {
				DbgPrint( ("Mic In"));
			} else if(type_of_node == HDA_PIN_TELEPHONY) {
				DbgPrint( ("Telephony"));
			} else if(type_of_node == HDA_PIN_SPDIF_IN) {
				DbgPrint( ("SPDIF In"));
			} else if(type_of_node == HDA_PIN_DIGITAL_OTHER_IN) {
				DbgPrint( ("Digital Other In"));
			} else if(type_of_node == HDA_PIN_RESERVED) {
				DbgPrint( ("Reserved"));
			} else if(type_of_node == HDA_PIN_OTHER) {
				DbgPrint( ("Other"));
			}
		}
		//if it's not a PIN then what other type of node is it?
		else if(type_of_node == HDA_WIDGET_POWER_WIDGET) {
			DbgPrint( ("Power Widget"));
		} else if(type_of_node == HDA_WIDGET_VOLUME_KNOB) {
			DbgPrint( ("Volume Knob"));
		} else if(type_of_node == HDA_WIDGET_BEEP_GENERATOR) {
			DbgPrint( ("Beep Generator"));
		} else if(type_of_node == HDA_WIDGET_VENDOR_DEFINED) {
			DbgPrint( ("Vendor defined"));
		} else {
			DbgPrint( ("Reserved type"));
		}

		//log all connected nodes
		DbgPrint( (" "));
		UCHAR connection_entry_number = 0;
		ULONG connection_entry_node = hda_get_node_connection_entry(codec_number, node, 0);
		while (connection_entry_node != 0x0000) {
			DbgPrint( "%d ", connection_entry_node);
			connection_entry_number++;
			connection_entry_node = hda_get_node_connection_entry(codec_number, node, connection_entry_number);
		}
	}
	
	//reset variables of second path
	second_audio_output_node_number = 0;
	second_audio_output_node_sample_capabilities = 0;
	second_audio_output_node_stream_format_capabilities = 0;
	second_output_amp_node_number = 0;
	second_output_amp_node_capabilities = 0;

	//initialize output PINs
	DbgPrint( ("\n"));
	if (pin_speaker_default_node_number != 0) {
		//initialize speaker
		is_initialized_useful_output = TRUE;
		if (pin_speaker_node_number != 0) {
			DbgPrint("\nSpeaker output");
			hda_initialize_output_pin(pin_speaker_node_number); //initialize speaker with connected output device
			pin_output_node_number = pin_speaker_node_number; //save speaker node number
		}	
		else {
			DbgPrint("\nDefault speaker output");
			hda_initialize_output_pin(pin_speaker_default_node_number); //initialize default speaker
			pin_output_node_number = pin_speaker_default_node_number; //save speaker node number
		}

		//save speaker path
		second_audio_output_node_number = audio_output_node_number;
		second_audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
		second_audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
		second_output_amp_node_number = output_amp_node_number;
		second_output_amp_node_capabilities = output_amp_node_capabilities;

		//if codec has also headphone output, initialize it
		if (pin_headphone_node_number != 0) {
			DbgPrint("\n\nHeadphone output");
			hda_initialize_output_pin(pin_headphone_node_number); //initialize headphone output
			pin_headphone_node_number = pin_headphone_node_number; //save headphone node number

			//if first path and second path share nodes, left only info for first path
			if(audio_output_node_number==second_audio_output_node_number) {
				second_audio_output_node_number = 0;
			}
			if(output_amp_node_number==second_output_amp_node_number) {
				second_output_amp_node_number = 0;
			}

			//find headphone connection status
			if(hda_is_headphone_connected() == TRUE) {
				hda_disable_pin_output(codec_number, pin_output_node_number);
				selected_output_node = pin_headphone_node_number;
			} else {
				selected_output_node = pin_output_node_number;
			}

			//add task for checking headphone connection
			//TODO: this will have to be a DPC and hda_send_verb has to be protected by a mutex 1st

			//create_task(hda_check_headphone_connection_change, TASK_TYPE_USER_INPUT, 50);
		}
	}
	else if(pin_headphone_node_number!=0) { //codec do not have speaker, but only headphone output
		DbgPrint("\nHeadphone output");
		is_initialized_useful_output = TRUE;
		hda_initialize_output_pin(pin_headphone_node_number); //initialize headphone output
		pin_output_node_number = pin_headphone_node_number; //save headphone node number
	}
	else if(pin_alternative_output_node_number!=0) { //codec have only alternative output
		DbgPrint("\nAlternative output");
		is_initialized_useful_output = FALSE;
		hda_initialize_output_pin(pin_alternative_output_node_number); //initialize alternative output
		pin_output_node_number = pin_alternative_output_node_number; //save alternative output node number
	}
	else {
		DbgPrint("\nCodec do not have any output PINs");
	}
}

UCHAR CAdapterCommon::hda_get_node_type (ULONG codec, ULONG node){
	return (UCHAR) ((hda_send_verb(codec, node, 0xF00, 0x09) >> 20) & 0xF);
}

ULONG CAdapterCommon::hda_get_node_connection_entry (ULONG codec, ULONG node, ULONG connection_entry_number) {
	//read connection capabilities
	ULONG connection_list_capabilities = hda_send_verb(codec, node, 0xF00, 0x0E);
	
	//test if this connection even exist
	if(connection_entry_number >= (connection_list_capabilities & 0x7F)) {
		return 0x0000;
	}

	//return number of connected node
	if((connection_list_capabilities & 0x80) == 0x00) { //short form
		return ((hda_send_verb(codec, node, 0xF02, ((connection_entry_number/4)*4)) >> ((connection_entry_number%4)*8)) & 0xFF);
	}
	else { //long form
		return ((hda_send_verb(codec, node, 0xF02, ((connection_entry_number/2)*2)) >> ((connection_entry_number%2)*16)) & 0xFFFF);
	}
}

void CAdapterCommon::hda_set_node_gain(ULONG codec, ULONG node, ULONG node_type, ULONG capabilities, ULONG gain) {
	//this will apply to left and right
	ULONG payload = 0x3000;

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
		payload |= (((capabilities>>8) & 0x7F)*gain/100); //recalculate range 0-100 to range of node steps
	}

	//change gain
	hda_send_verb(codec, node, 0x300, payload);
}

void CAdapterCommon::hda_enable_pin_output(ULONG codec, ULONG pin_node) {
	hda_send_verb(codec, pin_node, 0x707, (hda_send_verb(codec, pin_node, 0xF07, 0x00) | 0x40));
}

void CAdapterCommon::hda_disable_pin_output(ULONG codec, ULONG pin_node) {
	hda_send_verb(codec, pin_node, 0x707, (hda_send_verb(codec, pin_node, 0xF07, 0x00) & ~0x40));
}

BOOL CAdapterCommon::hda_is_headphone_connected ( void ) {
	if (pin_headphone_node_number != 0
    && (hda_send_verb(codecNumber, pin_headphone_node_number, 0xF09, 0x00) & 0x80000000) == 0x80000000) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

void CAdapterCommon::hda_initialize_output_pin ( ULONG pin_node_number) {
    PAGED_CODE ();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CAdapterCommon::hda_initialize_output_pin]"));
	//reset variables of first path
	audio_output_node_number = 0;
	audio_output_node_sample_capabilities = 0;
	audio_output_node_stream_format_capabilities = 0;
	output_amp_node_number = 0;
	output_amp_node_capabilities = 0;

	//turn on power for PIN
	hda_send_verb(codecNumber, pin_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codecNumber, pin_node_number, 0x708, 0x00);

	//disable any processing
	hda_send_verb(codecNumber, pin_node_number, 0x703, 0x00);

	//enable PIN
	hda_send_verb(codecNumber, pin_node_number, 0x707, (hda_send_verb(codecNumber, pin_node_number, 0xF07, 0x00) | 0x80 | 0x40));

	//enable EAPD + L-R swap
	hda_send_verb(codecNumber, pin_node_number, 0x70C, 0x6);

	//set maximal volume for PIN
	ULONG pin_output_amp_capabilities = hda_send_verb(codecNumber, pin_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, pin_node_number, HDA_OUTPUT_NODE, pin_output_amp_capabilities, 100);
	if(pin_output_amp_capabilities != 0) {
		//we will control volume by PIN node
		output_amp_node_number = pin_node_number;
		output_amp_node_capabilities = pin_output_amp_capabilities;
	}

	//start enabling path of nodes
	length_of_node_path = 0;
	hda_send_verb(codecNumber, pin_node_number, 0x701, 0x00); //select first node
	ULONG first_connected_node_number = hda_get_node_connection_entry(codecNumber, pin_node_number, 0); //get first node number
	ULONG type_of_first_connected_node = hda_get_node_type(codecNumber, first_connected_node_number); //get type of first node
	if(type_of_first_connected_node==HDA_WIDGET_AUDIO_OUTPUT) {
		hda_initalize_audio_output(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_MIXER) {
		hda_initalize_audio_mixer(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_SELECTOR) {
		hda_initalize_audio_selector(first_connected_node_number);
	}
	else {
		DOUT (DBG_PRINT, ("\nHDA ERROR: PIN have connection %d", first_connected_node_number));
	}
}

void CAdapterCommon::hda_initalize_audio_output(ULONG audio_output_node_number) {
	DOUT (DBG_PRINT, ("Initalizing Audio Output %d", audio_output_node_number));
	audio_output_node_number = audio_output_node_number;

	//turn on power for Audio Output
	hda_send_verb(codecNumber, audio_output_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codecNumber, audio_output_node_number, 0x708, 0x00);

	//disable any processing
	hda_send_verb(codecNumber, audio_output_node_number, 0x703, 0x00);

	//connect Audio Output to stream 1 channel 0
	hda_send_verb(codecNumber, audio_output_node_number, 0x706, 0x10);

	//set maximal volume for Audio Output
	ULONG audio_output_amp_capabilities = hda_send_verb(codecNumber, audio_output_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, audio_output_node_number, HDA_OUTPUT_NODE, audio_output_amp_capabilities, 100);
	if(audio_output_amp_capabilities!=0) {
		//we will control volume by Audio Output node
		output_amp_node_number = audio_output_node_number;
		output_amp_node_capabilities = audio_output_amp_capabilities;
	}

	//read info, if something is not present, take it from AFG node
	ULONG audio_output_sample_capabilities = hda_send_verb(codecNumber, audio_output_node_number, 0xF00, 0x0A);
	if(audio_output_sample_capabilities==0) {
		audio_output_node_sample_capabilities = afg_node_sample_capabilities;
	}
	else {
		audio_output_node_sample_capabilities = audio_output_sample_capabilities;
	}
	ULONG audio_output_stream_format_capabilities = hda_send_verb(codecNumber, audio_output_node_number, 0xF00, 0x0B);
	if(audio_output_stream_format_capabilities==0) {
		audio_output_node_stream_format_capabilities = afg_node_stream_format_capabilities;
	}
	else {
		audio_output_node_stream_format_capabilities = audio_output_stream_format_capabilities;
	}
	if(output_amp_node_number==0) { //if nodes in path do not have output amp capabilities, volume will be controlled by Audio Output node with capabilities taken from AFG node
		output_amp_node_number = audio_output_node_number;
		output_amp_node_capabilities = afg_node_output_amp_capabilities;
	}

	//because we are at end of node path, log all gathered info
	DOUT (DBG_PRINT, ("Sample Capabilites: 0x%x", audio_output_node_sample_capabilities));
	DOUT (DBG_PRINT, ("Stream Format Capabilites: 0x%x", audio_output_node_stream_format_capabilities));
	DOUT (DBG_PRINT, ("Volume node: %d", output_amp_node_number));
	DOUT (DBG_PRINT, ("Volume capabilities: 0x%x", output_amp_node_capabilities));
}

void CAdapterCommon::hda_initalize_audio_mixer(ULONG audio_mixer_node_number) {
	if(length_of_node_path>=10) {
		DOUT (DBG_PRINT,("HDA ERROR: too long path"));
		return;
	}
	DOUT (DBG_PRINT,("Initalizing Audio Mixer %d", audio_mixer_node_number));

	//turn on power for Audio Mixer
	hda_send_verb(codecNumber, audio_mixer_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codecNumber, audio_mixer_node_number, 0x708, 0x00);

	//set maximal volume for Audio Mixer
	ULONG audio_mixer_amp_capabilities = hda_send_verb(codecNumber, audio_mixer_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, audio_mixer_node_number, HDA_OUTPUT_NODE, audio_mixer_amp_capabilities, 100);
	if(audio_mixer_amp_capabilities != 0) {
		//we will control volume by Audio Mixer node
		output_amp_node_number = audio_mixer_node_number;
		output_amp_node_capabilities = audio_mixer_amp_capabilities;
	}

	//continue in path
	length_of_node_path++;
	ULONG first_connected_node_number = hda_get_node_connection_entry(codecNumber, audio_mixer_node_number, 0); //get first node number
	ULONG type_of_first_connected_node = hda_get_node_type(codecNumber, first_connected_node_number); //get type of first node
	if(type_of_first_connected_node == HDA_WIDGET_AUDIO_OUTPUT) {
		hda_initalize_audio_output(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_MIXER) {
		hda_initalize_audio_mixer(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_SELECTOR) {
		hda_initalize_audio_selector(first_connected_node_number);
	}
	else {
		DOUT (DBG_PRINT,("HDA ERROR: Mixer have connection %d", first_connected_node_number));
	}
}

void CAdapterCommon::hda_initalize_audio_selector(ULONG audio_selector_node_number) {
	if(length_of_node_path>=10) {
		DOUT (DBG_PRINT,("HDA ERROR: too long path"));
	return;
	}
	DOUT (DBG_PRINT,("Initalizing Audio Selector %d", audio_selector_node_number));

	//turn on power for Audio Selector
	hda_send_verb(codecNumber, audio_selector_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codecNumber, audio_selector_node_number, 0x708, 0x00);

	//disable any processing
	hda_send_verb(codecNumber, audio_selector_node_number, 0x703, 0x00);

	//set maximal volume for Audio Selector
	ULONG audio_selector_amp_capabilities = hda_send_verb(codecNumber, audio_selector_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, audio_selector_node_number, HDA_OUTPUT_NODE, audio_selector_amp_capabilities, 100);
	if(audio_selector_amp_capabilities != 0) {
		//we will control volume by Audio Selector node
		output_amp_node_number = audio_selector_node_number;
		output_amp_node_capabilities = audio_selector_amp_capabilities;
	}
	
	//continue in path
	length_of_node_path++;
	hda_send_verb(codecNumber, audio_selector_node_number, 0x701, 0x00); //select first node
	ULONG first_connected_node_number = hda_get_node_connection_entry(codecNumber, audio_selector_node_number, 0); //get first node number
	ULONG type_of_first_connected_node = hda_get_node_type(codecNumber, first_connected_node_number); //get type of first node
	if(type_of_first_connected_node == HDA_WIDGET_AUDIO_OUTPUT) {
		hda_initalize_audio_output(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_MIXER) {
		hda_initalize_audio_mixer(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_SELECTOR) {
		hda_initalize_audio_selector(first_connected_node_number);
	}
	else {
		DOUT (DBG_PRINT,("HDA ERROR: Selector have connection %d", first_connected_node_number));
	}
}
/*
void CAdapterCommon::hda_check_headphone_connection_change(void) {
	//TODO: schedule as a periodic task
	if(selected_output_node == pin_output_node_number && hda_is_headphone_connected()==TRUE) { //headphone was connected
		hda_disable_pin_output(codecNumber, pin_output_node_number);
		selected_output_node = pin_headphone_node_number;
	}
	else if(selected_output_node == pin_headphone_node_number && hda_is_headphone_connected()==FALSE) { //headphone was disconnected
		hda_enable_pin_output(codecNumber, pin_output_node_number);
		selected_output_node = pin_output_node_number;
	}
}
*/

UCHAR CAdapterCommon::hda_is_supported_channel_size(UCHAR size) {
	UCHAR channel_sizes[5] = {8, 16, 20, 24, 32};
	ULONG mask=0x00010000;
 
	//get bit of requested size in capabilities
	for(int i=0; i<5; i++) {
		if(channel_sizes[i]==size) {
			break;
		}
	mask <<= 1;
	}
 
	if((audio_output_node_sample_capabilities & mask)==mask) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

UCHAR CAdapterCommon::hda_is_supported_sample_rate(ULONG sample_rate) {
	ULONG sample_rates[11] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
	USHORT mask=0x0000001;
 
	//get bit of requested sample rate in capabilities
	for(int i=0; i<11; i++) {
		if(sample_rates[i]==sample_rate) {
			break;
		}
		mask <<= 1;
	}
 
	if((audio_output_node_sample_capabilities & mask)==mask) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

#pragma code_seg() //End of pageable code.

STDMETHODIMP_(ULONG) CAdapterCommon::hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command) {
	//DOUT (DBG_PRINT, ("[CAdapterCommon::hda_send_verb]"));
	//using CORB/RIRB interface only (immediate cmd interface is not supported)

	//TODO: check sizes of components passed in 
	//TODO: check for unsolicited responses and maybe schedule a DPC to deal with them



	ULONG value = ((codec<<28) | (node<<20) | (verb<<8) | (command));
	//DOUT (DBG_SYSINFO, ("Write codec verb 0x%X position %d", value, CorbPointer));

	//acquire spin lock: this isnt great to do as well as blocking but what can i do
	KIRQL oldirql;
	KeAcquireSpinLock(&QLock, &oldirql);

 
	//write verb
	WRITE_REGISTER_ULONG(CorbMemVirt + (CorbPointer), value);
	//KeFlushIoBuffers(mdl, FALSE, TRUE); //does this do anything? it's defined to nothing in wdm.h
  
	//move write pointer
	writeUSHORT(0x48, CorbPointer);
  
	//wait for RIRB pointer to update to match

	for(ULONG ticks = 0; ticks < 5; ++ticks) {
		KeStallExecutionProcessor(10);
		if(readUSHORT (0x58) == CorbPointer) {
			//KeFlushIoBuffers(mdl, TRUE, TRUE);  
			break;
		}
		if( (readUSHORT (0x58) != CorbPointer) && (ticks == 4)) {
			DOUT (DBG_ERROR, ("No Response to Codec Verb"));
			//	communication_type = HDA_UNINITIALIZED;
			return STATUS_TIMEOUT;
		}
	}

	//read response. each response is 8 bytes long but we only care about the lower 32 bits
	value = READ_REGISTER_ULONG (RirbMemVirt + (RirbPointer * 2));

	//move pointers
	CorbPointer++;
	if(CorbPointer == CorbNumberOfEntries) {
		CorbPointer = 0;
	}
	RirbPointer++;
	if(RirbPointer == RirbNumberOfEntries) {
		RirbPointer = 0;
	}

	//unlock
	KeReleaseSpinLock(&QLock, oldirql);

	//return response
	return value;
	//TODO: implement immediate command interface if it is ever necessary
}


/*****************************************************************************
 * CAdapterCommon::ReadController()
 *****************************************************************************
 * Read a byte from the controller.
 */
STDMETHODIMP_(BYTE)
CAdapterCommon::
ReadController
(   void
)
{
	DOUT (DBG_PRINT, ("[CAdapterCommon::ReadController]"));
    BYTE returnValue = BYTE(-1);
	/*

    ASSERT(m_pWaveBase);

    ULONGLONG m_startTime = PcGetTimeInterval(0);

    do
    {
        if
        (   READ_PORT_UCHAR
            (
                m_pWaveBase + DSP_REG_DATAAVAIL
            )
        &   0x80
        )
        {
            returnValue =
                READ_PORT_UCHAR
                (
                    m_pWaveBase + DSP_REG_READ
                );
        }
    } while ((PcGetTimeInterval(m_startTime) < GTI_MILLISECONDS(20)) &&
             (BYTE(-1) == returnValue));


    ASSERT((BYTE(-1) != returnValue) || !"ReadController timeout!");
	*/

    return returnValue;
}

/*****************************************************************************
 * CAdapterCommon::WriteController()
 *****************************************************************************
 * Write a byte to the controller.
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
	/*
    ASSERT(m_pWaveBase);

    BOOLEAN     returnValue = FALSE;
    ULONGLONG   m_startTime   = PcGetTimeInterval(0);

    do
    {

        BYTE status =
            READ_PORT_UCHAR
            (
                m_pWaveBase + DSP_REG_WRITE
            );

        if ((status & 0x80) == 0)
        {
            WRITE_PORT_UCHAR
            (
                m_pWaveBase + DSP_REG_WRITE,
                Value
            );

            returnValue = TRUE;
        }
    } while ((PcGetTimeInterval(m_startTime) < GTI_MILLISECONDS(20)) &&
              ! returnValue);


    ASSERT(returnValue || !"WriteController timeout");
	*/

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
    IN      BYTE    Index,
    IN      BYTE    Value
)
{

	DOUT (DBG_PRINT, ("[CAdapterCommon::MixerRegWrite]"));\
	DOUT (DBG_PRINT, ("trying to write %d to %d", Value, Index));

	if (Index == 1){
		DOUT (DBG_PRINT, ("set volume %d", Value / 2));
		hda_set_volume(Value / 2); //supposed to be 0-100 this is as close as i can get
	}

    /*
	ASSERT( m_pWaveBase );
    BYTE actualIndex;

    // only hit the hardware if we're in an acceptable power state
    if( m_PowerState <= PowerDeviceD1 )
    {
        actualIndex = (BYTE) ((Index < 0x80) ? (Index + DSP_MIX_BASEIDX) : Index);
    
        WRITE_PORT_UCHAR
        (
            m_pWaveBase + DSP_REG_MIXREG,
            actualIndex
        );
    
        WRITE_PORT_UCHAR
        (
            m_pWaveBase + DSP_REG_MIXDATA,
            Value
        );
    }

    if(Index < DSP_MIX_MAXREGS)
    {
        MixerSettings[Index] = Value;
    }
	*/
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
	DOUT (DBG_PRINT, ("trying to read from %d", Index));

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
    /*ASSERT(m_pWaveBase);

    WRITE_PORT_UCHAR
    (
        m_pWaveBase + DSP_REG_MIXREG,
        DSP_MIX_DATARESETIDX
    );

    WRITE_PORT_UCHAR
    (
        m_pWaveBase + DSP_REG_MIXDATA,
        0
    );

    RestoreMixerSettingsFromRegistry();
	*/
}

/*****************************************************************************
 * CAdapterCommon::AcknowledgeIRQ()
 *****************************************************************************
 * Acknowledge interrupt request.
  TODO FIXME
 */
void
CAdapterCommon::
AcknowledgeIRQ
(   void
)
{	
	//read INTSTS register
	ULONG u = readULONG(0x24);
	DOUT (DBG_PRINT, ("[CAdapterCommon::AcknowledgeIRQ], %X", u));
	//uh how do i actually clear the interrupt from streams?
	//writeULONG(0x20,0x0); //just disables interrupts it doesnt ack them
	//write 1 to clear irq status on output stream 1
	setULONGBit(OutputStreamBase + 3,0x0004);

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

	//ntStatus = InitHDA();

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
        
        // make a unicode strong for the subkey name
        RtlInitUnicodeString( &KeyName, L"Settings" );

        // open the settings subkey
        ntStatus = DriverKey->NewSubKey( &SettingsKey,              // Subkey
                                         NULL,                      // OuterUnknown
                                         KEY_ALL_ACCESS,            // Access flags
                                         &KeyName,                  // Subkey name
                                         REG_OPTION_NON_VOLATILE,   // Create options
                                         NULL );
        if(NT_SUCCESS(ntStatus))
        {
            // loop through all mixer settings
            for(UINT i = 0; i < SIZEOF_ARRAY(MixerSettings); i++)
            {
                // init key name
                RtlInitUnicodeString( &KeyName, DefaultMixerSettings[i].KeyName );

                // set the key
                DWORD KeyValue = DWORD(MixerSettings[DefaultMixerSettings[i].RegisterIndex]);
                ntStatus = SettingsKey->SetValueKey( &KeyName,                 // Key name
                                                     REG_DWORD,                // Key type
                                                     PVOID(&KeyValue),
                                                     sizeof(DWORD) );
                if(!NT_SUCCESS(ntStatus))
                {
                    break;
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
 * CAdapterCommon::ProgramSampleRate
 *****************************************************************************
 * Programs the sample rate. If the rate cannot be programmed, the routine
 * restores the register and returns STATUS_UNSUCCESSFUL.
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::ProgramSampleRate
(
    IN  DWORD           dwSampleRate
)
{
    PAGED_CODE ();

    WORD     wOldRateReg, wCodecReg;
    NTSTATUS ntStatus;

	//TODO: use reg param to select between
	//primary or secondary audio output or capture input

    DOUT (DBG_PRINT, ("[CAdapterCommon::ProgramSampleRate]"));

    //
    // Check what sample rates we support based on
	// afg_node_sample_capabilities & audio_output_node_sample_capabilities
    //
	// if either is zero, give up as nothing has been inited yet
	if((!afg_node_sample_capabilities) || (!audio_output_node_sample_capabilities)){
		    DOUT (DBG_ERROR, ("Invalid sample rate register!"));
            return STATUS_UNSUCCESSFUL;
	}

	if (!hda_is_supported_sample_rate(dwSampleRate)) {
		// Not a supported sample rate
		DOUT (DBG_VSR, ("Samplerate %d not supported", dwSampleRate));
		return STATUS_NOT_SUPPORTED;
	}

	USHORT format = hda_return_sound_data_format(dwSampleRate, 2, 16);

	//set stream data format
	writeUSHORT(OutputStreamBase + 0x12, format);

	//set Audio Output nodes data format
	hda_send_verb(codecNumber, audio_output_node_number, 0x200, format);
	if(second_audio_output_node_number != 0) {
		hda_send_verb(codecNumber, second_audio_output_node_number, 0x200, format);
	}
    
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

                // Save the new state.  This local value is used to determine when to cache
                // property accesses and when to permit the driver from accessing the hardware.
                m_PowerState = NewState.DeviceState;

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
NTSTATUS
InterruptServiceRoutine
(
    IN      PINTERRUPTSYNC  InterruptSync,
    IN      PVOID           DynamicContext
)
{
    ASSERT(InterruptSync);
    ASSERT(DynamicContext);

    CAdapterCommon *that = (CAdapterCommon *) DynamicContext;

    UCHAR portStatus = 0xff;

    //
    // We are here because the MPU tried and failed, so
    // must be a wave interrupt.
    //
    //ASSERT(that->m_pWaveBase);
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

    //
    // ACK the ISR. note we don't have any direct access to CAdapterCommon gotta use the pointer
    //
	that->AcknowledgeIRQ();
	

    return STATUS_SUCCESS;
}

STDMETHODIMP_(NTSTATUS) CAdapterCommon::hda_stop_stream (void) {
	DOUT (DBG_PRINT, ("[CAdapterCommon::hda_stop_stream]"));
    //stop output DMA engine
	
	writeUCHAR(OutputStreamBase + 0x00, 0x00);
	ULONG ticks = 0;
	while(ticks++<2) {
		KeStallExecutionProcessor(1);
		if((readUCHAR(OutputStreamBase + 0x00) & 0x2)==0x0) {
			break;
		}
	}
	if((readUCHAR(OutputStreamBase + 0x00) & 0x2)==0x2) {
		DOUT (DBG_ERROR, ("HDA: can not stop stream"));
		return STATUS_TIMEOUT;
	}
 
	//reset stream registers
	writeUCHAR(OutputStreamBase + 0x00, 0x01);
	ticks = 0;
	while(ticks++<10) {
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
	return STATUS_TIMEOUT;
	}
	KeStallExecutionProcessor(5);

	//clear error bits
	writeUCHAR(OutputStreamBase + 0x03, 0x1C);
    return STATUS_SUCCESS;
}

STDMETHODIMP_(void) CAdapterCommon::hda_start_sound(void) {
	//TODO: this may freeze Virtualbox. Why?
	writeUCHAR(OutputStreamBase + 0x02, 0x14);
	writeUCHAR(OutputStreamBase + 0x00, 0x02);
}

STDMETHODIMP_(void) CAdapterCommon::hda_stop_sound(void) {
	writeUCHAR(OutputStreamBase + 0x00, 0x00);
}


STDMETHODIMP_(ULONG) CAdapterCommon::hda_get_actual_stream_position(void) {
	return readULONG(OutputStreamBase + 0x04);
}

void CAdapterCommon::hda_set_volume(ULONG volume) {
	hda_set_node_gain(codecNumber, output_amp_node_number, HDA_OUTPUT_NODE, output_amp_node_capabilities, volume);
	if(second_output_amp_node_number != 0) {
		hda_set_node_gain(codecNumber, second_output_amp_node_number, HDA_OUTPUT_NODE, second_output_amp_node_capabilities, volume);
	}
}



//HDA codec register read and write functions

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

	NTSTATUS ntStatus = STATUS_SUCCESS;

	DOUT (DBG_PRINT, ("[CAdapterCommon::hda_showtime]"));

	//get the physical and virtual address pointers from the dma channel object
	BufLogicalAddress = DmaChannel->PhysicalAddress();
	BufVirtualAddress = DmaChannel->SystemAddress();
	audBufSize = DmaChannel->BufferSize();

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

	DOUT(DBG_SYSINFO, ("write to audio ram"));

	// can i write to my audio buffer at all?
	for(int i = 0; i < 10; ++i)
		((PUSHORT)BufVirtualAddress)[i] = 0xeeee;
	for(i = 10; i < 20; ++i)
		((PUSHORT)BufVirtualAddress)[i] = 0xaaaa;
	for(i = 20; i < 30; ++i)
		((PUSHORT)BufVirtualAddress)[i] = 0xcccc;

	DOUT(DBG_SYSINFO, ("garbage written in"));
	ProgramSampleRate(44100);

	//fill buffer entries
	BdlMemVirt[0] = BufLogicalAddress.LowPart;
	BdlMemVirt[1] = BufLogicalAddress.HighPart;
	BdlMemVirt[2] = audBufSize;
	BdlMemVirt[3] = 0;

	//fill buffer entries - there have to be at least two entries in BDL, so i write same one twice
	BdlMemVirt[4] = BufLogicalAddress.LowPart;
	BdlMemVirt[5] = BufLogicalAddress.HighPart;
	BdlMemVirt[6] = audBufSize;
	BdlMemVirt[7] = 0;

	DOUT(DBG_SYSINFO, ("BDL all set up"));

	KeFlushIoBuffers(mdl, FALSE, TRUE); 
	//flush processor cache to RAM to be sure sound card will read correct data? if this does anything?

	//set buffer registers
	writeULONG(OutputStreamBase + 0x18, BdlMemPhys.LowPart);
	writeULONG(OutputStreamBase + 0x1C, BdlMemPhys.HighPart);
	writeULONG(OutputStreamBase + 0x08, audBufSize * 2);
	writeUSHORT(OutputStreamBase + 0x0C, 1); //there are two entries in buffer

	DOUT(DBG_SYSINFO, ("buffer address programmed"));


	//set stream data format
	writeUSHORT(OutputStreamBase + 0x12, hda_return_sound_data_format(44100, 2, 16));

	//set Audio Output node data format
	hda_send_verb(codecNumber, audio_output_node_number, 0x200, hda_return_sound_data_format(44100, 2, 16));
	if(second_audio_output_node_number != 0) {
		hda_send_verb(codecNumber, second_audio_output_node_number, 0x200, hda_return_sound_data_format(44100, 2, 16));
	}
	KeStallExecutionProcessor(10);

	DOUT(DBG_SYSINFO, ("ready to start the stream"));


	//start streaming to stream 1
	writeUCHAR(OutputStreamBase + 0x02, 0x14);
	writeUCHAR(OutputStreamBase + 0x00, 0x02);

	DOUT(DBG_SYSINFO, ("showtime"));
	
    return ntStatus;
}

STDMETHODIMP_(USHORT) CAdapterCommon::hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample) {
 USHORT data_format = 0;

 //channels
 data_format = (USHORT)(channels-1);

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
 if(sample_rate==48000) {
  data_format |= ((0x0)<<8);
 }
 else if(sample_rate==44100) {
  data_format |= ((0x40)<<8);
 }
 else if(sample_rate==32000) {
  data_format |= ((0xA)<<8);
 }
 else if(sample_rate==22050) {
  data_format |= ((0x41)<<8);
 }
 else if(sample_rate==16000) {
  data_format |= ((0x2)<<8);
 }
 else if(sample_rate==11025) {
  data_format |= ((0x43)<<8);
 }
 else if(sample_rate==8000) {
  data_format |= ((0x5)<<8);
 }
 else if(sample_rate==88200) {
  data_format |= ((0x48)<<8);
 }
 else if(sample_rate==96000) {
  data_format |= ((0x8)<<8);
 }
 else if(sample_rate==176400) {
  data_format |= ((0x58)<<8);
 }
 else if(sample_rate==192000) {
  data_format |= ((0x18)<<8);
 }

 return data_format;
}

STDMETHODIMP_(PULONG) CAdapterCommon::get_bdl_mem(void){
	return BdlMemVirt;
}
