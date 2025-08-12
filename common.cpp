/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

// Every debug output has "Modulname text"
static char STR_MODULENAME[] = "ICH Common: ";

#include "common.h"


/*****************************************************************************
 * Static Members
 *****************************************************************************
 */

//
// This is the register cache including registry names and default values. The
// first WORD contains the register value and the second WORD contains a flag.
// Currently, we only set SHREG_INVALID if we have to read the register at
// startup (that's true when there is no constant default value for the
// register).  Note that we cache the registers only to prevent read access to
// the AC97 CoDec during runtime, because this is slow (40us).
// We only set SHREG_INIT if we want to set the register to default at driver
// startup.  If needed, the third field contains the registry name and the
// forth field contains a default value that is used when there is no registry
// entry.
// The flag SHREG_NOCACHE is used when we don't want to cache the register
// at all.  This is neccessary for status registers and sample rate registers.
//
tAC97Registers CAdapterCommon::m_stAC97Registers[] =
{
{0x0000, SHREG_INVALID, NULL,               0},         // AC97REG_RESET
{0x8000, SHREG_INIT,    L"MasterVolume",    0x0000},	// AC97REG_MASTER_VOLUME
{0x8000, SHREG_INIT,    L"HeadphoneVolume", 0x0000},    // AC97REG_HPHONE_VOLUME
{0x8000, SHREG_INIT,    L"MonooutVolume",   0x0000},    // AC97REG_MMONO_VOLUME
{0x0F0F, SHREG_INIT,    L"ToneControls",    0x0F0F},    // AC97REG_MASTER_TONE
{0x0000, SHREG_INVALID |
         SHREG_INIT,    L"BeepVolume",      0x0000},    // AC97REG_BEEP_VOLUME
{0x8008, SHREG_INIT,    L"PhoneVolume",     0x8008},    // AC97REG_PHONE_VOLUME
{0x8008, SHREG_INIT,    L"MicVolume",       0x8008},    // AC97REG_MIC_VOLUME
{0x8808, SHREG_INIT,    L"LineInVolume",    0x0808},    // AC97REG_LINE_IN_VOLUME
{0x8808, SHREG_INIT,    L"CDVolume",        0x0808},    // AC97REG_CD_VOLUME
{0x8808, SHREG_INIT,    L"VideoVolume",     0x0808},    // AC97REG_VIDEO_VOLUME
{0x8808, SHREG_INIT,    L"AUXVolume",       0x0808},    // AC97REG_AUX_VOLUME
{0x8808, SHREG_INIT,    L"WaveOutVolume",   0x0808},    // AC97REG_PCM_OUT_VOLUME
{0x0000, SHREG_INIT,    L"RecordSelect",    0x0404},    // AC97REG_RECORD_SELECT
{0x8000, SHREG_INIT,    L"RecordGain",      0x0000},    // AC97REG_RECORD_GAIN
{0x8000, SHREG_INIT,    L"RecordGainMic",   0x0000},    // AC97REG_RECORD_GAIN_MIC
{0x0000, SHREG_INIT,    L"GeneralPurpose",  0x0000},    // AC97REG_GENERAL
{0x0000, SHREG_INIT,    L"3DControl",       0x0000},    // AC97REG_3D_CONTROL
{0x0000, SHREG_NOCACHE, NULL,               0},         // AC97REG_RESERVED
{0x0000, SHREG_NOCACHE |
         SHREG_INIT,    L"PowerDown",       0},         // AC97REG_POWERDOWN
// AC97-2.0 registers
{0x0000, SHREG_INVALID, NULL,               0},         // AC97REG_EXT_AUDIO_ID
{0x0000, SHREG_NOCACHE, NULL,               0},         // AC97REG_EXT_AUDIO_CTRL
{0xBB80, SHREG_NOCACHE, NULL,               0},         // AC97REG_PCM_FRONT_RATE
{0xBB80, SHREG_NOCACHE, NULL,               0},         // AC97REG_PCM_SURR_RATE
{0xBB80, SHREG_NOCACHE, NULL,               0},         // AC97REG_PCM_LFE_RATE
{0xBB80, SHREG_NOCACHE, NULL,               0},         // AC97REG_PCM_LR_RATE
{0xBB80, SHREG_NOCACHE, NULL,               0},         // AC97REG_MIC_RATE
{0x8080, 0,             NULL,               0},         // AC97REG_6CH_VOL_CLFE
{0x8080, 0,             NULL,               0},         // AC97REG_6CH_VOL_LRSURR
{0x0000, SHREG_NOCACHE, NULL,               0}          // AC97REG_RESERVED2

// We leave the other values blank.  There would be a huge gap with 31
// elements that are currently unused, and then there would be 2 other
// (used) values, the vendor IDs.  We just force a read from the vendor 
// IDs in the end of ProbeHWConfig to fill the cache.
};


//
// This is the hardware configuration information.  The first struct is for 
// nodes, which we default to FALSE.  The second struct is for Pins, which 
// contains the configuration (FALSE) and the registry string which is the
// reason for making a static struct so we can just fill in the name.
//
tHardwareConfig CAdapterCommon::m_stHardwareConfig =
{
    // Nodes
    {{FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE},
     {FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE}, {FALSE}},
    // Pins
    {{FALSE, L"DisablePCBeep"},     // PINC_PCBEEP_PRESENT
     {FALSE, L"DisablePhone"},      // PINC_PHONE_PRESENT
     {FALSE, L"DisableMic2"},       // PINC_MIC2_PRESENT
     {FALSE, L"DisableVideo"},      // PINC_VIDEO_PRESENT
     {FALSE, L"DisableAUX"},        // PINC_AUX_PRESENT
     {FALSE, L"DisableHeadphone"},  // PINC_HPOUT_PRESENT
     {FALSE, L"DisableMonoOut"},    // PINC_MONOOUT_PRESENT
     {FALSE, L"DisableMicIn"},      // PINC_MICIN_PRESENT
     {FALSE, L"DisableMic"},        // PINC_MIC_PRESENT
     {FALSE, L"DisableLineIn"},     // PINC_LINEIN_PRESENT
     {FALSE, L"DisableCD"}}         // PINC_CD_PRESENT
};
ULONG audBufSize = 9600 * 2 * 2; //100ms of 2ch 16 bit audio

#pragma code_seg("PAGE")
/*****************************************************************************
 * NewAdapterCommon
 *****************************************************************************
 * Create a new adapter common object.
 */
NTSTATUS NewAdapterCommon
(
    OUT PUNKNOWN   *Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN    UnknownOuter    OPTIONAL,
    IN  POOL_TYPE   PoolType
)
{
    PAGED_CODE ();

    ASSERT (Unknown);

    DOUT (DBG_PRINT, ("[NewAdapterCommon]"));

    STD_CREATE_BODY_
    (
        CAdapterCommon,
        Unknown,
        UnknownOuter,
        PoolType,
        PADAPTERCOMMON
    );
}   


/*****************************************************************************
 * CAdapterCommon::Init
 *****************************************************************************
 * Initialize the adapter common object -> initialize and probe HW.
 * Pass only checked resources.
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::Init
(
    IN  PRESOURCELIST   ResourceList,
    IN  PDEVICE_OBJECT  DeviceObject
)
{
    PAGED_CODE ();

    ASSERT (ResourceList);
    ASSERT (DeviceObject);

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

	//is there anything in config space we need to set?

	//there may be multiple instances of this driver loaded at once
	//on systems with HDMI display audio support for instance
	//but it should be one driver per HDA controller.
	//which may access multiple codecs but for now only sending 1 audio stream to all
	//hardware mixing may come, MUCH later.

    //
    //Get the memory base address for the HDA controller registers. 
    //

    // Get memory resource - note we only want the first BAR
	// on Skylake and newer mobile chipsets,
	// there is a DSP interface at BAR2 for Intel Smart Sound Technology

	DOUT(DBG_SYSINFO, ("%d Resources in List", ResourceList->NumberOfEntries() ));

    for (ULONG i = 0; i < ResourceList->NumberOfEntries(); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = ResourceList->FindTranslatedEntry(CmResourceTypeMemory, i);
        if (desc) {
			if (!memLength) {
				physAddr = desc->u.Memory.Start;
				memLength = desc->u.Memory.Length; 
			}
            DOUT(DBG_SYSINFO, ("Resource %d: Phys Addr = 0x%X, Mem Length = 0x%X", 
				i, physAddr, memLength));
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
	} else {
		DOUT(DBG_SYSINFO, ("Phys Addr = 0x%X, Mem Length = 0x%X", physAddr, memLength));
	}

	//Map the device memory into kernel space
    m_pHDARegisters = (PUCHAR) MmMapIoSpace(physAddr, memLength, MmNonCached);
    if (!m_pHDARegisters) {
        DOUT (DBG_ERROR, ("Failed to map HD Audio registers to kernel mem"));
        return STATUS_BUFFER_TOO_SMALL;
    }
	else {
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
	InputStreamBase = (Base + 0x80);
	OutputStreamBase = (Base + 0x80 + (0x20 * ((caps>>8) & 0xF))); //skip input streams ports

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
	pDeviceDescription -> MaximumLength		= audBufSize + 8192;

	//we have to pretend we're on an Alpha and have "map registers" to allocate
	ULONG nMapRegisters = 0;

	DMA_Adapter = IoGetDmaAdapter (
		m_pDeviceObject,
		pDeviceDescription,
		&nMapRegisters );

	DOUT(DBG_SYSINFO, ("Map Registers = %d", nMapRegisters));

	//do we have enough of this fictional resource?
	//map registers used = number of pages allocated + 1 

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
		1024,
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
	// - or should we be doing this through WaveCyclic?
	//not sure if that can give me the desired 128 byte phys alignment but we might
	//get that automatically just by mapping more than a 4k page worth.

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

	//
    // Reset the controller and init registers
    //
    ntStatus = InitHDA ();
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;
    

    //
    // Probe codec configuration
    //
    ntStatus = ProbeHWConfig ();
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Probing of hardware configuration failed!"));
        return ntStatus;
    }

#if (DBG)
    DumpConfig ();
#endif

	//return an error at the end for now so it doesnt run the ac97 specific parts of the driver further
    ntStatus = STATUS_INVALID_PARAMETER;
	
	//SetAC97Default ();

    //
    // Initialize the device state.
    //
    m_PowerState = PowerDeviceD0;

    return ntStatus;
}


/*****************************************************************************
 * CAdapterCommon::~CAdapterCommon
 *****************************************************************************
 * Destructor.
 */
CAdapterCommon::~CAdapterCommon ()
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CAdapterCommon::~CAdapterCommon]"));

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

	if((BufVirtualAddress != NULL) && (DMA_Adapter!= NULL)){
		DOUT (DBG_PRINT, ("freeing audio buffer"));
		DMA_Adapter->DmaOperations->FreeCommonBuffer(
					DMA_Adapter,
					audBufSize,
                      BufLogicalAddress,
                      BufVirtualAddress,
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

}

#if (DBG)
/*****************************************************************************
 * CAdapterCommon::DumpConfig
 *****************************************************************************
 * Dumps the HW configuration for the AC97 codec.
 */
void CAdapterCommon::DumpConfig (void)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CAdapterCommon::DumpConfig]"));

    //
    // Print debug output for MICIN.
    //
    if (GetPinConfig (PINC_MICIN_PRESENT))
    {
        DOUT (DBG_PROBE, ("MICIN found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No MICIN found"));
    }

    //
    // Print debug output for tone controls.
    //
    if (GetNodeConfig (NODEC_TONE_PRESENT))
    {
        DOUT (DBG_PROBE, ("Tone controls found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No tone controls found"));
    }

    //
    // Print debug output for mono out.
    //
    if (!GetPinConfig (PINC_MONOOUT_PRESENT))
    {
        DOUT (DBG_PROBE, ("No mono out found"));
    }

    //
    // Print debug output for headphones.
    //
    if (!GetPinConfig (PINC_HPOUT_PRESENT))
    {
        DOUT (DBG_PROBE, ("No headphone out found"));
    }

    //
    // Print debug output for loudness.
    //
    if (GetNodeConfig (NODEC_LOUDNESS_PRESENT))
    {
        DOUT (DBG_PROBE, ("Loudness found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No Loudness found"));
    }

    //
    // Print debug output for 3D.
    //
    if (GetNodeConfig (NODEC_3D_PRESENT))
    {
        DOUT (DBG_PROBE, ("3D controls found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No 3D controls found"));
    }

    //
    // Print debug output for pc beep.
    //
    if (GetPinConfig (PINC_PCBEEP_PRESENT))
    {
        DOUT (DBG_PROBE, ("PC beep found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No PC beep found"));
    }

    //
    // Print debug output for phone line (or mono line input).
    //
    if (GetPinConfig (PINC_PHONE_PRESENT))
    {
        DOUT (DBG_PROBE, ("Phone found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No Phone found"));
    }

    //
    // Print debug output for video.
    //
    if (GetPinConfig (PINC_VIDEO_PRESENT))
    {
        DOUT (DBG_PROBE, ("Video in found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No Video in found"));
    }

    //
    // Print debug output for AUX.
    //
    if (GetPinConfig (PINC_AUX_PRESENT))
    {
        DOUT (DBG_PROBE, ("AUX in found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No AUX in found"));
    }

    //
    // Print debug output for second miorophone.
    //
    if (GetPinConfig (PINC_MIC2_PRESENT))
    {
        DOUT (DBG_PROBE, ("MIC2 found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("No MIC2 found"));
    }
    
    //
    // Print debug output for 3D stuff.
    //
    if (GetNodeConfig (NODEC_3D_PRESENT))
    {
        if (GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE))
        {
            DOUT (DBG_PROBE, ("Adjustable 3D center control found"));
        }
        if (GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE))
        {
            DOUT (DBG_PROBE, ("Nonadjustable 3D depth control found"));
        }
    }

    //
    // Print debug output for quality of master volume.
    //
    if (GetNodeConfig (NODEC_6BIT_MASTER_VOLUME))
    {
        DOUT (DBG_PROBE, ("6bit master out found"));
    }
    else
    {
        DOUT (DBG_PROBE, ("5bit master out found"));
    }

    //
    // Print debug output for quality of headphones volume.
    //
    if (GetPinConfig (PINC_HPOUT_PRESENT))
    {
        if (GetNodeConfig (NODEC_6BIT_HPOUT_VOLUME))
        {
            DOUT (DBG_PROBE, ("6bit headphone out found"));
        }
        else
        {
            DOUT (DBG_PROBE, ("5bit headphone out found"));
        }
    }

    //
    // Print debug output for quality of mono out volume.
    //
    if (GetPinConfig (PINC_MONOOUT_PRESENT))
    {
        if (GetNodeConfig (NODEC_6BIT_MONOOUT_VOLUME))
        {
            DOUT (DBG_PROBE, ("6bit mono out found"));
        }
        else
        {
            DOUT (DBG_PROBE, ("5bit mono out found"));
        }
    }

    //
    // Print sample rate information.
    //
    if (GetNodeConfig (NODEC_PCM_VARIABLERATE_SUPPORTED))
    {
        DOUT (DBG_PROBE, ("PCM variable sample rate supported"));
    }
    else
    {
        DOUT (DBG_PROBE, ("only 48KHz PCM supported"));
    }

    //
    // Print double rate information.
    //
    if (GetNodeConfig (NODEC_PCM_DOUBLERATE_SUPPORTED))
    {
        DOUT (DBG_PROBE, ("PCM double sample rate supported"));
    }

    //
    // Print mic rate information.
    //
    if (GetNodeConfig (NODEC_MIC_VARIABLERATE_SUPPORTED))
    {
        DOUT (DBG_PROBE, ("MIC variable sample rate supported"));
    }
    else
    {
        DOUT (DBG_PROBE, ("only 48KHz MIC supported"));
    }

    // print DAC information
    if (GetNodeConfig (NODEC_CENTER_DAC_PRESENT))
    {
        DOUT (DBG_PROBE, ("center DAC found"));
    }
    if (GetNodeConfig (NODEC_SURROUND_DAC_PRESENT))
    {
        DOUT (DBG_PROBE, ("surround DAC found"));
    }
    if (GetNodeConfig (NODEC_LFE_DAC_PRESENT))
    {
        DOUT (DBG_PROBE, ("LFE DAC found"));
    }
}
#endif

/*****************************************************************************
 * CAdapterCommon::NonDelegatingQueryInterface
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 * We basically just check any GUID we know and return this object in case we
 * know it.
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::NonDelegatingQueryInterface
(
    IN  REFIID  Interface,
    OUT PVOID * Object
)
{
    PAGED_CODE ();

    ASSERT (Object);

    DOUT (DBG_PRINT, ("[CAdapterCommon::NonDelegatingQueryInterface]"));

    // Is it IID_IUnknown?
    if (IsEqualGUIDAligned (Interface, IID_IUnknown))
    {
        *Object = (PVOID)(PUNKNOWN)(PADAPTERCOMMON)this;
    }
    else
    // or IID_IAdapterCommon ...
    if (IsEqualGUIDAligned (Interface, IID_IAdapterCommon))
    {
        *Object = (PVOID)(PADAPTERCOMMON)this;
    }
    else
    // or IID_IAdapterPowerManagment ...
    if (IsEqualGUIDAligned (Interface, IID_IAdapterPowerManagment))
    {
        *Object = (PVOID)(PADAPTERPOWERMANAGMENT)this;
    }
    else
    {
        // nothing found, must be an unknown interface.
        *Object = NULL;
        return STATUS_INVALID_PARAMETER;
    }

    //
    // We reference the interface for the caller.
    //
    ((PUNKNOWN)*Object)->AddRef ();
    return STATUS_SUCCESS;
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
    if (NT_SUCCESS (ntStatus))
    {        
//        ntStatus = PowerUpCodec ();
    }
    else
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
		hda_initalize_audio_function_group(codec_number, node); //initalize Audio Function Group
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

void hda_initalize_audio_function_group(UCHAR codec_number, UCHAR afg_node_number) {
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
	DOUT (DBG_SYSINFO, ("\n\nLIST OF ALL NODES IN AFG:"));
	ULONG subordinate_node_count_reponse = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x04);
	ULONG pin_alternative_output_node_number = 0, pin_speaker_default_node_number = 0, pin_speaker_node_number = 0, pin_headphone_node_number = 0;
	pin_output_node_number = 0;
	pin_headphone_node_number = 0;

	for (ULONG node = ((subordinate_node_count_reponse>>16) & 0xFF), last_node = (node+(subordinate_node_count_reponse & 0xFF)), 
		type_of_node = 0; node < last_node; node++) {

		//log number of node
		DOUT (DBG_SYSINFO, ("\n%d ", node);
  
		//get type of node
		type_of_node = hda_get_node_type(codec_number, node);

		//process node
		//TODO: can i express this more compactly as a switch statement?

		if(type_of_node == HDA_WIDGET_AUDIO_OUTPUT) {
			DOUT (DBG_SYSINFO, ("Audio Output"));

			//disable every audio output by connecting it to stream 0
			hda_send_verb(codec_number, node, 0x706, 0x00);
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_INPUT) {
			DOUT (DBG_SYSINFO, ("Audio Input"));
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_MIXER) {
			DOUT (DBG_SYSINFO, ("Audio Mixer"));
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_SELECTOR) {
			DOUT (DBG_SYSINFO, ("Audio Selector"));
		}
		else if(type_of_node == HDA_WIDGET_PIN_COMPLEX) {
			DOUT (DBG_SYSINFO, ("Pin Complex "));

			//read type of PIN
			type_of_node = ((hda_send_verb(codec_number, node, 0xF1C, 0x00) >> 20) & 0xF);

			if(type_of_node == HDA_PIN_LINE_OUT) {
				DOUT (DBG_SYSINFO, ("Line Out"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_SPEAKER) {
				DOUT (DBG_SYSINFO, ("Speaker "));

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
							DOUT (DBG_SYSINFO, ("connected output device"));
							//we will use first pin with connected device, so save node number only for first PIN
							if(pin_speaker_node_number==0) {
								pin_speaker_node_number = node;
							}
						} else {
							DOUT (DBG_SYSINFO, ("not output capable"));
						}
					} else {
						DOUT (DBG_SYSINFO, ("not jack or fixed device"));
					}
				} else {
					DOUT (DBG_SYSINFO, ("no output device"));
				}
			} else if(type_of_node == HDA_PIN_HEADPHONE_OUT) {
				DOUT (DBG_SYSINFO, ("Headphone Out"));

				//save node number
				//TODO: handle if there are multiple HP nodes
				pin_headphone_node_number = node;
			} else if(type_of_node == HDA_PIN_CD) {
				DOUT (DBG_SYSINFO, ("CD"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_SPDIF_OUT) {
				DOUT (DBG_SYSINFO, ("SPDIF Out"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_DIGITAL_OTHER_OUT) {
				DOUT (DBG_SYSINFO, ("Digital Other Out"));
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_MODEM_LINE_SIDE) {
				DOUT (DBG_SYSINFO, ("Modem Line Side"));

				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_MODEM_HANDSET_SIDE) {
				DOUT (DBG_SYSINFO, ("Modem Handset Side"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_LINE_IN) {
				DOUT (DBG_SYSINFO, ("Line In"));
			} else if(type_of_node == HDA_PIN_AUX) {
				DOUT (DBG_SYSINFO, ("AUX"));
			} else if(type_of_node == HDA_PIN_MIC_IN) {
				DOUT (DBG_SYSINFO, ("Mic In"));
			} else if(type_of_node == HDA_PIN_TELEPHONY) {
				DOUT (DBG_SYSINFO, ("Telephony"));
			} else if(type_of_node == HDA_PIN_SPDIF_IN) {
				DOUT (DBG_SYSINFO, ("SPDIF In"));
			} else if(type_of_node == HDA_PIN_DIGITAL_OTHER_IN) {
				DOUT (DBG_SYSINFO, ("Digital Other In"));
			} else if(type_of_node == HDA_PIN_RESERVED) {
				DOUT (DBG_SYSINFO, ("Reserved"));
			} else if(type_of_node == HDA_PIN_OTHER) {
				DOUT (DBG_SYSINFO, ("Other"));
			}
		}
		//if it's not a PIN then what other type of node is it?
		else if(type_of_node == HDA_WIDGET_POWER_WIDGET) {
			DOUT (DBG_SYSINFO, ("Power Widget"));
		} else if(type_of_node == HDA_WIDGET_VOLUME_KNOB) {
			DOUT (DBG_SYSINFO, ("Volume Knob"));
		} else if(type_of_node == HDA_WIDGET_BEEP_GENERATOR) {
			DOUT (DBG_SYSINFO, ("Beep Generator"));
		} else if(type_of_node == HDA_WIDGET_VENDOR_DEFINED) {
			DOUT (DBG_SYSINFO, ("Vendor defined"));
		} else {
			DOUT (DBG_SYSINFO, ("Reserved type"));
		}

		//log all connected nodes
		DOUT (DBG_SYSINFO, (" "));
		UCHAR connection_entry_number = 0;
		USHORT connection_entry_node = hda_get_node_connection_entry(codec_number, node, 0);
		while (connection_entry_node != 0x0000) {
			DOUT (DBG_SYSINFO, ("%d ", connection_entry_node));
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

	//initalize output PINs
	DOUT (DBG_SYSINFO, ("\n"));
	if (pin_speaker_default_node_number != 0) {
		//initalize speaker
		is_initalized_useful_output = STATUS_TRUE;
		if (pin_speaker_node_number != 0) {
			DOUT (DBG_SYSINFO, ("\nSpeaker output");
			hda_initalize_output_pin(sound_card_number, pin_speaker_node_number); //initalize speaker with connected output device
			pin_output_node_number = pin_speaker_node_number; //save speaker node number
		}	
		else {
			DOUT (DBG_SYSINFO, ("\nDefault speaker output");
			hda_initalize_output_pin(sound_card_number, pin_speaker_default_node_number); //initalize default speaker
			pin_output_node_number = pin_speaker_default_node_number; //save speaker node number
		}

		//save speaker path
		second_audio_output_node_number = audio_output_node_number;
		second_audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
		second_audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
		second_output_amp_node_number = output_amp_node_number;
		second_output_amp_node_capabilities = output_amp_node_capabilities;

		//if codec has also headphone output, initalize it
		if (pin_headphone_node_number != 0) {
			DOUT (DBG_SYSINFO, ("\n\nHeadphone output");
			hda_initalize_output_pin(sound_card_number, pin_headphone_node_number); //initalize headphone output
			pin_headphone_node_number = pin_headphone_node_number; //save headphone node number

			//if first path and second path share nodes, left only info for first path
			if(audio_output_node_number==second_audio_output_node_number) {
				second_audio_output_node_number = 0;
			}
			if(output_amp_node_number==second_output_amp_node_number) {
				second_output_amp_node_number = 0;
			}

			//find headphone connection status
			if(hda_is_headphone_connected()==STATUS_TRUE) {
				hda_disable_pin_output(codec_number, pin_output_node_number);
				selected_output_node = pin_headphone_node_number;
			} else {
				selected_output_node = pin_output_node_number;
			}

			//add task for checking headphone connection
			//TODO: this will have to be a DPC and hda_send_verb has to be protected by a semaphore 1st

			//create_task(hda_check_headphone_connection_change, TASK_TYPE_USER_INPUT, 50);
		}
	}
	else if(pin_headphone_node_number!=0) { //codec do not have speaker, but only headphone output
		DOUT (DBG_SYSINFO, ("\nHeadphone output"));
		is_initalized_useful_output = STATUS_TRUE;
		hda_initalize_output_pin(sound_card_number, pin_headphone_node_number); //initalize headphone output
		pin_output_node_number = pin_headphone_node_number; //save headphone node number
	}
	else if(pin_alternative_output_node_number!=0) { //codec have only alternative output
		DOUT (DBG_SYSINFO, ("\nAlternative output"));
		is_initalized_useful_output = STATUS_FALSE;
		hda_initalize_output_pin(sound_card_number, pin_alternative_output_node_number); //initalize alternative output
		pin_output_node_number = pin_alternative_output_node_number; //save alternative output node number
	}
	else {
		DOUT (DBG_SYSINFO, ("\nCodec do not have any output PINs"));
	}
}


//TODO: next functions
CAdapterCommon:hda_get_node_type( void ){

}

CAdapterCommon:hda_get_node_connection_entry( void){

}

CAdapterCommon:hda_initalize_output_pin( void){

}


/*****************************************************************************
 * CAdapterCommon::ProbeHWConfig
 *****************************************************************************
 * Probes the hardware configuration.
 * If this function returns with an error, then the configuration is not
 * complete! Probing the registers is done by reading them (and comparing with
 * the HW default value) or when the default is unknown, writing to them and
 * reading back + restoring.
 * Additionally, we read the registry so that a HW vendor can overwrite (means
 * disable) found registers in case the adapter (e.g. video) is not visible to
 * the user (he can't plug in a video audio there).
 *
 * This is a very long function with all of the error checking!
 */
NTSTATUS CAdapterCommon::ProbeHWConfig (void)
{
    PAGED_CODE ();

    NTSTATUS ntStatus = STATUS_SUCCESS;
    WORD     wCodecID;
    WORD     wCodecReg;

    DOUT (DBG_PRINT, ("[CAdapterCommon::ProbeHWConfig]"));

    //
    // Wait for the whatever 97 to complete reset and establish a link.
    //
    ntStatus = PrimaryCodecReady ();
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    //
    // Master volume is one of the supported registers on an AC97
    //
    ntStatus = ReadCodecRegister (AC97REG_MASTER_VOLUME, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // Default is x8000.
    if (wCodecReg != 0x8000)
        return STATUS_NO_SUCH_DEVICE;

    //
    // This gives us information about the AC97 CoDec
    //
    ntStatus = ReadCodecRegister (AC97REG_RESET, &wCodecID);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    //
    // Fill out the configuration stuff.
    //

    SetPinConfig (PINC_MICIN_PRESENT, wCodecID & 0x0001);
    
    // Check if OEM wants to disable MIC record line.
    if (DisableAC97Pin (PINC_MICIN_PRESENT))
        SetPinConfig (PINC_MICIN_PRESENT, FALSE);

    // If we still have MIC record line, enable the DAC in ext. audio register.
    if (GetPinConfig (PINC_MICIN_PRESENT))
        // Enable ADC MIC.
        WriteCodecRegister (AC97REG_EXT_AUDIO_CTRL, 0, 0x4000);
    else
        // Disable ADC MIC.
        WriteCodecRegister (AC97REG_EXT_AUDIO_CTRL, 0x4000, 0x4000);

    //
    // Continue setting configuration information.
    //

    SetNodeConfig (NODEC_TONE_PRESENT, wCodecID & 0x0004);
    SetNodeConfig (NODEC_SIMUL_STEREO_PRESENT, wCodecID & 0x0008);
    SetPinConfig (PINC_HPOUT_PRESENT, wCodecID & 0x0010);
    
    // Check if OEM wants to disable headphone output.
    if (DisableAC97Pin (PINC_HPOUT_PRESENT))
        SetPinConfig (PINC_HPOUT_PRESENT, FALSE);

    SetNodeConfig (NODEC_LOUDNESS_PRESENT, wCodecID & 0x0020);
    SetNodeConfig (NODEC_3D_PRESENT, wCodecID & 0x7C00);

    //
    // Test for the input pins that are always there but could be disabled
    // by the HW vender
    //

    // Check if OEM wants to disable mic input.
    if (DisableAC97Pin (PINC_MIC_PRESENT))
        SetPinConfig (PINC_MIC_PRESENT, FALSE);
    else
        SetPinConfig (PINC_MIC_PRESENT, TRUE);
    
    // Check if OEM wants to disable line input.
    if (DisableAC97Pin (PINC_LINEIN_PRESENT))
        SetPinConfig (PINC_LINEIN_PRESENT, FALSE);
    else
        SetPinConfig (PINC_LINEIN_PRESENT, TRUE);

    // Check if OEM wants to disable CD input.
    if (DisableAC97Pin (PINC_CD_PRESENT))
        SetPinConfig (PINC_CD_PRESENT, FALSE);
    else
        SetPinConfig (PINC_CD_PRESENT, TRUE);


    //
    // For the rest, we have to probe the registers.
    //

    //
    // Test for Mono out.
    //
    ntStatus = ReadCodecRegister (AC97REG_MMONO_VOLUME, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // Default is x8000.
    if (wCodecReg == 0x8000)
        SetPinConfig (PINC_MONOOUT_PRESENT, TRUE);
    else
        SetPinConfig (PINC_MONOOUT_PRESENT, FALSE);

    // Check if OEM wants to disable mono output.
    if (DisableAC97Pin (PINC_MONOOUT_PRESENT))
        SetPinConfig (PINC_MONOOUT_PRESENT, FALSE);

    //
    // Test for PC beeper support.
    //
    ntStatus = ReadCodecRegister (AC97REG_BEEP_VOLUME, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // default is x0 or x8000. If it's 0x8000 then we know for sure that the
    // CoDec has a PcBeep, otherwise we have to check the register
    if (wCodecReg == 0x8000)
        SetPinConfig (PINC_PCBEEP_PRESENT, TRUE);
    else if (!wCodecReg)
    {
        // mute the pc beeper.
        ntStatus = WriteCodecRegister (AC97REG_BEEP_VOLUME, 0x8000, -1);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;

        // read back
        ntStatus = ReadCodecRegister (AC97REG_BEEP_VOLUME, &wCodecReg);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;

        if (wCodecReg == 0x8000)
        {
            // yep, we have support.
            SetPinConfig (PINC_PCBEEP_PRESENT, TRUE);
            // reset to default value.
            WriteCodecRegister (AC97REG_BEEP_VOLUME, 0x0, -1);
        }
        else
            // nope, not present
            SetPinConfig (PINC_PCBEEP_PRESENT, FALSE);
    }
    else
        // any other value then 0x0 and 0x8000.
        SetPinConfig (PINC_PCBEEP_PRESENT, FALSE);

    // Check if OEM wants to disable beeper support.
    if (DisableAC97Pin (PINC_PCBEEP_PRESENT))
        SetPinConfig (PINC_PCBEEP_PRESENT, FALSE);

    //
    // Test for phone support.
    //
    ntStatus = ReadCodecRegister (AC97REG_PHONE_VOLUME, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // Default is x8008.
    if (wCodecReg == 0x8008)
        SetPinConfig (PINC_PHONE_PRESENT, TRUE);
    else
        SetPinConfig (PINC_PHONE_PRESENT, FALSE);

    // Check if OEM wants to disable phone input.
    if (DisableAC97Pin (PINC_PHONE_PRESENT))
        SetPinConfig (PINC_PHONE_PRESENT, FALSE);

    //
    // Test for video support.
    //
    ntStatus = ReadCodecRegister (AC97REG_VIDEO_VOLUME, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // Default is x8808.
    if (wCodecReg == 0x8808)
        SetPinConfig (PINC_VIDEO_PRESENT, TRUE);
    else
        SetPinConfig (PINC_VIDEO_PRESENT, FALSE);

    // Check if OEM wants to disable video input.
    if (DisableAC97Pin (PINC_VIDEO_PRESENT))
        SetPinConfig (PINC_VIDEO_PRESENT, FALSE);

    //
    // Test for Aux support.
    //
    ntStatus = ReadCodecRegister (AC97REG_AUX_VOLUME, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // Default is 0x8808.
    if (wCodecReg == 0x8808)
        SetPinConfig (PINC_AUX_PRESENT, TRUE);
    else
        SetPinConfig (PINC_AUX_PRESENT, FALSE);

    // Check if OEM wants to disable aux input.
    if (DisableAC97Pin (PINC_AUX_PRESENT))
        SetPinConfig (PINC_AUX_PRESENT, FALSE);

    //
    // Test for Mic2 source.
    //
    ntStatus = ReadCodecRegister (AC97REG_GENERAL, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    // Test for Mic2 select bit.
    if (wCodecReg & 0x0100)
        SetPinConfig (PINC_MIC2_PRESENT, TRUE);
    else
    {
        // Select Mic2 as source.
        ntStatus = WriteCodecRegister (AC97REG_GENERAL, 0x0100, 0x0100);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;

        // Read back.
        ntStatus = ReadCodecRegister (AC97REG_GENERAL, &wCodecReg);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;

        if (wCodecReg & 0x0100)
        {
            // Yep, we have support so set it to the default value.
            SetPinConfig (PINC_MIC2_PRESENT, TRUE);
            // reset to default value.
            WriteCodecRegister (AC97REG_GENERAL, 0, 0x0100);
        }
        else
            SetPinConfig (PINC_MIC2_PRESENT, FALSE);
    }

    // Check if OEM wants to disable mic2 input.
    if (DisableAC97Pin (PINC_MIC2_PRESENT))
        SetPinConfig (PINC_MIC2_PRESENT, FALSE);

    //
    // Test the 3D controls.
    //
    if (GetNodeConfig (NODEC_3D_PRESENT))
	{
		//
        // First test for fixed 3D controls. Write default value ...
        //
		ntStatus = WriteCodecRegister (AC97REG_3D_CONTROL, 0, -1);
		if (!NT_SUCCESS (ntStatus))
			return ntStatus;

        // Read 3D register. Default is 0 when adjustable, otherwise it is
        // a fixed value.
		ntStatus = ReadCodecRegister (AC97REG_3D_CONTROL, &wCodecReg);
		if (!NT_SUCCESS (ntStatus))
			return ntStatus;

        //
        // Check center and depth separately.
        //
		
        // For center
        if (wCodecReg & 0x0F00)
			SetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE, FALSE);
        else
			SetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE, TRUE);
	
        // For depth
        if (wCodecReg & 0x000F)
			SetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE, FALSE);
        else
			SetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE, TRUE);

        //
        // Test for adjustable controls.
        //
		WriteCodecRegister (AC97REG_3D_CONTROL, 0x0A0A, -1);
		
        // Read 3D register. Now it should be 0x0A0A for adjustable controls,
        // otherwise it is a fixed control or simply not there.
		ReadCodecRegister (AC97REG_3D_CONTROL, &wCodecReg);

        // Restore the default value
		WriteCodecRegister (AC97REG_3D_CONTROL, 0, -1);

        // Check the center control for beeing adjustable
        if (GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE) &&
            (wCodecReg & 0x0F00) != 0x0A00)
        {
            SetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE, FALSE);
        }
        
        // Check the depth control for beeing adjustable
        if (GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE) &&
            (wCodecReg & 0x000F) != 0x000A)
        {
            SetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE, FALSE);
        }
    }

    //
	// Check for 6th bit support in volume controls.  To check the 6th bit, 
    // we first have to write a value (with 6th bit set) and then read it 
    // back.  After that, we should restore the register to its default value.
    //

    //
    // Start with the master volume.
    //
    ntStatus = WriteCodecRegister (AC97REG_MASTER_VOLUME, 0xA020, -1);
	if (!NT_SUCCESS (ntStatus))
		return ntStatus;

    // Read back.
	ntStatus = ReadCodecRegister (AC97REG_MASTER_VOLUME, &wCodecReg);
	if (!NT_SUCCESS (ntStatus))
		return ntStatus;

    // Check return.
    if (wCodecReg & 0x2020)
        SetNodeConfig (NODEC_6BIT_MASTER_VOLUME, TRUE);
    else
        SetNodeConfig (NODEC_6BIT_MASTER_VOLUME, FALSE);

    // Restore default value.
    WriteCodecRegister (AC97REG_MASTER_VOLUME, 0x8000, -1);

    //
    // Check for a headphone volume control.
    //
    if (GetPinConfig (PINC_HPOUT_PRESENT))
    {
        // Check headphone volume.
        ntStatus = WriteCodecRegister (AC97REG_HPHONE_VOLUME, 0xA020, -1);
	    if (!NT_SUCCESS (ntStatus))
		    return ntStatus;

        // Read back
	    ntStatus = ReadCodecRegister (AC97REG_HPHONE_VOLUME, &wCodecReg);
	    if (!NT_SUCCESS (ntStatus))
		    return ntStatus;

        // Check return
        if (wCodecReg & 0x2020)
            SetNodeConfig (NODEC_6BIT_HPOUT_VOLUME, TRUE);
        else
            SetNodeConfig (NODEC_6BIT_HPOUT_VOLUME, FALSE);

        // Restore default value.
        WriteCodecRegister (AC97REG_HPHONE_VOLUME, 0x8000, -1);
    }

    //
    // Mono out there?
    //
    if (GetPinConfig (PINC_MONOOUT_PRESENT))
    {
        // Check headphone volume.
        ntStatus = WriteCodecRegister (AC97REG_MMONO_VOLUME, 0x8020, -1);
	    if (!NT_SUCCESS (ntStatus))
		    return ntStatus;

        // Read back
	    ntStatus = ReadCodecRegister (AC97REG_MMONO_VOLUME, &wCodecReg);
	    if (!NT_SUCCESS (ntStatus))
		    return ntStatus;

        // Check return
        if (wCodecReg & 0x0020)
            SetNodeConfig (NODEC_6BIT_MONOOUT_VOLUME, TRUE);
        else
            SetNodeConfig (NODEC_6BIT_MONOOUT_VOLUME, FALSE);

        // Restore default value.
        WriteCodecRegister (AC97REG_MMONO_VOLUME, 0x8000, -1);
    }

    //
    // Get extended AC97 V2.0 information
    //

    // First check for V2 codec.
    ntStatus = ReadCodecRegister (AC97REG_PCM_FRONT_RATE, &wCodecReg);
	if (!NT_SUCCESS (ntStatus))
		return ntStatus;

    // The V2 codec have the default value of 0xBB80. A V1 codec would
    // have 0x0000.
    if (wCodecReg == 0xBB80)
    {
        // We are lucky. Just read the register.
        ntStatus = ReadCodecRegister (AC97REG_EXT_AUDIO_ID, &wCodecReg);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;
    }
    else
    {
        // We want to disable all features, so lets assume a read with
        // value 0, that will disable all features (see above).
        wCodecReg = 0;
    }


    //
    // Store the information
    //
    SetNodeConfig (NODEC_PCM_VARIABLERATE_SUPPORTED, wCodecReg & 0x0001);
    SetNodeConfig (NODEC_PCM_DOUBLERATE_SUPPORTED, wCodecReg & 0x0002);
    SetNodeConfig (NODEC_MIC_VARIABLERATE_SUPPORTED, wCodecReg & 0x0008);
    SetNodeConfig (NODEC_CENTER_DAC_PRESENT, wCodecReg & 0x0040);
    SetNodeConfig (NODEC_SURROUND_DAC_PRESENT, wCodecReg & 0x0080);
    SetNodeConfig (NODEC_LFE_DAC_PRESENT, wCodecReg & 0x0100);


    //
    // Enable variable sample rate in the control register and disable
    // double rate.
    //
    WriteCodecRegister (AC97REG_EXT_AUDIO_CTRL, wCodecReg & 0x0009, 0x000B);

    //
    // We read these registers because they are dependent on the codec.
    //
    ReadCodecRegister (AC97REG_VENDOR_ID1, &wCodecReg);
    ReadCodecRegister (AC97REG_VENDOR_ID2, &wCodecReg);

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::AcquireCodecSemiphore
 *****************************************************************************
 * Acquires the AC97 semaphore.  This can not be called at dispatch level
 * because it can timeout if a lower IRQL thread has the semaphore.
 */
NTSTATUS CAdapterCommon::AcquireCodecSemiphore ()
{
    PAGED_CODE ();

	//TODO how to rewrite this for HDA?

    DOUT (DBG_PRINT, ("[CAdapterCommon::AcquireCodecSemiphore]"));

    ULONG ulCount = 0;
    while (READ_PORT_UCHAR (m_pBusMasterBase + CAS) & CAS_CAS)
    {
        //
        // Do we want to give up??
        //
        if (ulCount++ > 100)
        {
            DOUT (DBG_ERROR, ("Cannot acquire semaphore."));
            return STATUS_IO_TIMEOUT;
        }

        //
        // Let's wait a little, 40us and then try again.
        //
        KeStallExecutionProcessor (40L);
    }

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::ReadCodecRegister
 *****************************************************************************
 * Reads a HDA CODEC register. only(?) call at PASSIVE_LEVEL.
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::ReadCodecRegister
(
    IN  AC97Register reg,
    OUT PWORD wData
)
{
    PAGED_CODE ();
    
    ASSERT (wData);
    ASSERT (reg < AC97REG_INVALID);    // audio can only be in the primary codec

    NTSTATUS ntStatus;
    ULONG    Status;

    DOUT (DBG_PRINT, ("[CAdapterCommon::ReadCodecRegister]"));
    
    //
    // Check if we have to access the HW directly.
    //
    if (m_bDirectRead || (m_stAC97Registers[reg].wFlags & SHREG_INVALID) ||
        (m_stAC97Registers[reg].wFlags & SHREG_NOCACHE))
    {
        //
        // Grab the codec access semiphore.
        //
        ntStatus = AcquireCodecSemiphore ();
        if (!NT_SUCCESS (ntStatus))
        {
            DOUT (DBG_ERROR, ("ReadCodecRegister couldn't acquire the semiphore"
                " for reg. %s", reg <= AC97REG_RESERVED2 ? RegStrings[reg] :
                                reg == AC97REG_VENDOR_ID1 ? "REG_VENDOR_ID1" :
                                reg == AC97REG_VENDOR_ID2 ? "REG_VENDOR_ID2" :
                                "REG_INVALID"));
            return ntStatus;
        }

        //
        // Read the data.
        //
        *wData = READ_PORT_USHORT (m_pCodecBase + reg);

        //
        // Check to see if the read was successful.
        //
        Status = READ_PORT_ULONG ((PULONG)(m_pBusMasterBase + GLOB_STA));
        if (Status & GLOB_STA_RCS)
        {
            //
            // clear the timeout bit
            //
            WRITE_PORT_ULONG ((PULONG)(m_pBusMasterBase + GLOB_STA), Status);
            *wData = 0;
            DOUT (DBG_ERROR, ("ReadCodecRegister timed out for register %s",
                    reg <= AC97REG_RESERVED2 ? RegStrings[reg] :
                    reg == AC97REG_VENDOR_ID1 ? "REG_VENDOR_ID1" :
                    reg == AC97REG_VENDOR_ID2 ? "REG_VENDOR_ID2" :
                    "REG_INVALID"));
            return STATUS_IO_TIMEOUT;
        } 
        
        //
        // Clear invalid flag
        //
        m_stAC97Registers[reg].wCache = *wData;
        m_stAC97Registers[reg].wFlags &= ~SHREG_INVALID;
        
        DOUT (DBG_REGS, ("AC97READ: %s = 0x%04x (HW)",
                reg <= AC97REG_RESERVED2 ? RegStrings[reg] :
                reg == AC97REG_VENDOR_ID1 ? "REG_VENDOR_ID1" :
                reg == AC97REG_VENDOR_ID2 ? "REG_VENDOR_ID2" : 
                "REG_INVALID", *wData));
    }
    else
    {
        //
        // Otherwise, use the value in the cache.
        //
        *wData = m_stAC97Registers[reg].wCache;
        DOUT (DBG_REGS, ("AC97READ: %s = 0x%04x (C)",
                reg <= AC97REG_RESERVED2 ? RegStrings[reg] :
                reg == AC97REG_VENDOR_ID1 ? "REG_VENDOR_ID1" :
                reg == AC97REG_VENDOR_ID2 ? "REG_VENDOR_ID2" : 
                "REG_INVALID", *wData));
    }

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::WriteCodecRegister
 *****************************************************************************
 * Writes to a HDA CODEC register.  This can only be done at passive level because
 * the AcquireCodecSemiphore call could fail!
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::WriteCodecRegister
(
    IN  AC97Register reg,
    IN  WORD wData,
    IN  WORD wMask
)
{
    PAGED_CODE ();
    
    ASSERT (reg < AC97REG_INVALID);    // audio can only be in the primary codec

    WORD TempData = 0;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CAdapterCommon::WriteCodecRegister]"));

    //
    // No mask?  Could happen when you try to prg. left channel of a
    // mono volume.
    //
    if (!wMask)
        return STATUS_SUCCESS;

    //
    // Check to see if we are only writing specific bits.  If so, we want
    // to leave some bits in the register alone.
    //
    if (wMask != 0xffff)
    {
        //
        // Read the current register contents.
        //
        ntStatus = ReadCodecRegister (reg, &TempData);
        if (!NT_SUCCESS (ntStatus))
        {
            DOUT (DBG_ERROR, ("WriteCodecRegiser read for mask failed"));
            return ntStatus;
        }

        //
        // Do the masking.
        //
        TempData &= ~wMask;
        TempData |= (wMask & wData);
    }
    else
    {
        TempData = wData;
    }
    

    //
    // Grab the codec access semiphore.
    //
    ntStatus = AcquireCodecSemiphore ();
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("WriteCodecRegister failed for register %s",
            reg <= AC97REG_RESERVED2 ? RegStrings[reg] :
            reg == AC97REG_VENDOR_ID1 ? "REG_VENDOR_ID1" :
            reg == AC97REG_VENDOR_ID2 ? "REG_VENDOR_ID2" : "REG_INVALID"));
        return ntStatus;
    }
            
    //
    // Write the data.
    //
    WRITE_PORT_USHORT (m_pCodecBase + reg, TempData);

    //
    // Update cache.
    //
    m_stAC97Registers[reg].wCache = TempData;
    
    DOUT (DBG_REGS, ("AC97WRITE: %s -> 0x%04x", 
                   reg <= AC97REG_RESERVED2 ? RegStrings[reg] :
                   reg == AC97REG_VENDOR_ID1 ? "REG_VENDOR_ID1" :
                   reg == AC97REG_VENDOR_ID2 ? "REG_VENDOR_ID2" : 
                   "REG_INVALID", TempData));
    
    
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::PrimaryCodecReady
 *****************************************************************************
 * Checks whether the primary codec is present and ready.  This may take
 * awhile if we are bringing it up from a cold reset so give it a second
 * before giving up.
 */
NTSTATUS CAdapterCommon::PrimaryCodecReady (void)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CAdapterCommon::PrimaryCodecReady]"));

    
    //
    // Enable the AC link and raise the reset line.
    //
    DWORD dwRegValue = ReadBMControlRegister32 (GLOB_CNT);
    
    // If someone enabled GPI Interrupt Enable, then he hopefully handles that
    // too.
    dwRegValue = (dwRegValue | GLOB_CNT_COLD) & ~(GLOB_CNT_ACLOFF | GLOB_CNT_PRIE);
    WriteBMControlRegister (GLOB_CNT, dwRegValue);

    //
    // Wait for the Codec to be ready.
    //
    ULONGLONG ullStartTime = PcGetTimeInterval (0);
    do
    {
        if (READ_PORT_ULONG ((PULONG)(m_pBusMasterBase + GLOB_STA)) & 
            GLOB_STA_PCR)
        {
            return STATUS_SUCCESS;
        }
        
        //
        // Let's wait a little, 50us and then try again.
        //
        KeStallExecutionProcessor (50L);
    } while (PcGetTimeInterval (ullStartTime) < GTI_MILLISECONDS (1000));

    DOUT (DBG_ERROR, ("PrimaryCodecReady timed out!"));
    return STATUS_IO_TIMEOUT;
}


/*****************************************************************************
 * CAdapterCommon::PowerUpCodec
 *****************************************************************************
 * Sets the Codec to the highest power state and waits until the Codec reports
 * that the power state is reached.
 */
NTSTATUS CAdapterCommon::PowerUpCodec (void)
{
    PAGED_CODE ();

    WORD        wCodecReg;
    NTSTATUS    ntStatus;

    DOUT (DBG_PRINT, ("[CAdapterCommon::PowerUpCodec]"));

    //
    // Power up the Codec.
    //
    WriteCodecRegister (AC97REG_POWERDOWN, 0x00, -1);

    //
    // Wait for the Codec to be powered up.
    //
    ULONGLONG ullStartTime = PcGetTimeInterval (0);
    do
    {
        //
        // Read the power management register.
        //
        ntStatus = ReadCodecRegister (AC97REG_POWERDOWN, &wCodecReg);
        if (!NT_SUCCESS (ntStatus))
        {
            wCodecReg = 0;      // Will cause an error.
            break;
        }

        //
        // Check the power state. Should be ready.
        //
        if ((wCodecReg & 0x0f) == 0x0f)
            break;

        //
        // Let's wait a little, 50us and then try again.
        //
        KeStallExecutionProcessor (50L);
    } while (PcGetTimeInterval (ullStartTime) < GTI_MILLISECONDS (1000));

    // Check if we timed out.
    if ((wCodecReg & 0x0f) != 0x0f)
    {
        DOUT (DBG_ERROR, ("PowerUpCodec timed out. CoDec not powered up."));
        ntStatus = STATUS_DEVICE_NOT_READY;
    }

    return ntStatus;
}


/*****************************************************************************
 * CAdapterCommon::ProgramSampleRate
 *****************************************************************************
 * Programs the sample rate. If the rate cannot be programmed, the routine
 * restores the register and returns STATUS_UNSUCCESSFUL.
 * We don't handle double rate sample rates here, because the Intel ICH con-
 * troller cannot serve CoDecs with double rate or surround sound. If you want
 * to modify this driver for another AC97 controller, then you might want to
 * change this function too.
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::ProgramSampleRate
(
    IN  AC97Register    Register,
    IN  DWORD           dwSampleRate
)
{
    PAGED_CODE ();

    WORD     wOldRateReg, wCodecReg;
    NTSTATUS ntStatus;

    DOUT (DBG_PRINT, ("[CAdapterCommon::ProgramSampleRate]"));

    //
    // Check if we support variable sample rate.
    //
    switch(Register)
    {
        case AC97REG_MIC_RATE:
            //
            // Variable sample rate supported?
            //
            if (GetNodeConfig (NODEC_MIC_VARIABLERATE_SUPPORTED))
            {
                // Range supported?
                if (dwSampleRate > 48000ul)
                {
                    // Not possible.
                    DOUT (DBG_VSR, ("Samplerate %d not supported", dwSampleRate));
                    return STATUS_NOT_SUPPORTED;
                }
            }
            else
            {
                // Only 48000KHz possible.
                if (dwSampleRate != 48000ul)
                {
                    DOUT (DBG_VSR, ("Samplerate %d not supported", dwSampleRate));
                    return STATUS_NOT_SUPPORTED;
                }

                return STATUS_SUCCESS;
            }
            break;

        case AC97REG_PCM_FRONT_RATE:
        case AC97REG_PCM_SURR_RATE:
        case AC97REG_PCM_LFE_RATE:
        case AC97REG_PCM_LR_RATE:
            //
            // Variable sample rate supported?
            //
            if (GetNodeConfig (NODEC_PCM_VARIABLERATE_SUPPORTED))
            {
                //
                // Check range supported
                //
                if (dwSampleRate > 48000ul)
                {
                    DOUT (DBG_VSR, ("Samplerate %d not supported", dwSampleRate));
                    return STATUS_NOT_SUPPORTED;
                }
            }
            else
            {
                // Only 48KHz possible.
                if (dwSampleRate != 48000)
                {
                    DOUT (DBG_VSR, ("Samplerate %d not supported", dwSampleRate));
                    return STATUS_NOT_SUPPORTED;
                }

                return STATUS_SUCCESS;
            }
            break;

        default:
            DOUT (DBG_ERROR, ("Invalid sample rate register!"));
            return STATUS_UNSUCCESSFUL;
    }

    
    //
    // Save the old sample rate register.
    //
    ntStatus = ReadCodecRegister (Register, &wOldRateReg);
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;
    
    //
    // program the rate.
    //
    ntStatus = WriteCodecRegister (Register, (WORD)dwSampleRate, -1);
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Cannot program sample rate."));
        return ntStatus;
    }

    //
    // Read it back.
    //
    ntStatus = ReadCodecRegister (Register, &wCodecReg);
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Cannot read sample rate."));
        return ntStatus;
    }

    //
    // Validate.
    //
    if (wCodecReg != dwSampleRate)
    {
        //
        // restore sample rate and ctrl register.
        //
        WriteCodecRegister (Register, wOldRateReg, -1);
        
        DOUT (DBG_VSR, ("Samplerate %d not supported", dwSampleRate));
        return STATUS_NOT_SUPPORTED;
    }
    
    DOUT (DBG_VSR, ("Samplerate changed to %d.", dwSampleRate));
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::PowerChangeState
 *****************************************************************************
 * Change power state for the device.  We handle the codec, PowerChangeNotify 
 * in the wave miniport handles the DMA registers.
 */
STDMETHODIMP_(void) CAdapterCommon::PowerChangeState
(
    IN  POWER_STATE NewState
)
{
    PAGED_CODE ();

	NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CAdapterCommon::PowerChangeNotify]"));

    //
    // Check to see if this is the current power state.
    //
    if (NewState.DeviceState == m_PowerState)
    {
        DOUT (DBG_POWER, ("New device state equals old state."));
        return;
    }

    //
    // Check the new device state.
    //
    if ((NewState.DeviceState < PowerDeviceD0) ||
        (NewState.DeviceState > PowerDeviceD3))
    {
        DOUT (DBG_ERROR, ("Unknown device state: D%d.", 
             (ULONG)NewState.DeviceState - (ULONG)PowerDeviceD0));
        return;
    }

    DOUT (DBG_POWER, ("Changing state to D%d.", (ULONG)NewState.DeviceState -
                    (ULONG)PowerDeviceD0));

    //
    // Switch on new state.
    //
    switch (NewState.DeviceState)
    {
        case PowerDeviceD0:
            //
            // If we are coming from D3 we have to restore the registers cause
            // there might have been a power loss.
            //
            if (m_PowerState == PowerDeviceD3)
            {
                //
                // Reset AD3 to indicate that we are now awake.
                // Because the system has only one power irp at a time, we are sure
                // that the modem driver doesn't get called while we are restoring
                // power.
                //
                WriteBMControlRegister (GLOB_STA,
                    ReadBMControlRegister32 (GLOB_STA) & ~GLOB_STA_AD3);

                //
                // Restore codec registers.
                //
                ntStatus = RestoreCodecRegisters ();
            }
            else        // We are coming from power state D1
            {
                ntStatus = PowerUpCodec ();
            }

            // Print error code.
            if (!NT_SUCCESS (ntStatus))
            {
                DOUT (DBG_ERROR, ("PowerChangeState failed to restore the codec."));
            }
            break;
    
        case PowerDeviceD1:
            //
            // This sleep state is the lowest latency sleep state with respect
            // to the latency time required to return to D0. If the 
            // driver is not being used an inactivity timer in portcls will 
            // place the driver in this state after a timeout period 
            // controllable via the registry.
            //
    
            // Let's power down the DAC/ADC's and analog mixer.
            WriteCodecRegister (AC97REG_POWERDOWN, 0x0700, -1);
            break;

        case PowerDeviceD2:
            //
            // This is a medium latency sleep state. In addition to powering down
            // DAC/ADC's and analog mixer, we also power down the Vref, HP amp
            // and external Amp but not AC-Link and Clk.
            //
            WriteCodecRegister (AC97REG_POWERDOWN, 0xCF00, -1);
            break;

        case PowerDeviceD3:
            //
            // This is a full hibernation state and is the longest latency sleep
            // state. In this modes the power could be removed or reduced that
            // much that the AC97 controller looses information, so we save
            // whatever we have to save.
            //

            //
            // Powerdown ADC, DAC, Mixer, Vref, HP amp, and Exernal Amp but not
            // AC-link and Clk
            //
            WriteCodecRegister (AC97REG_POWERDOWN, 0xCF00, -1);

            //
            // Set the AD3 bit.
            //
            ULONG ulReg = ReadBMControlRegister32 (GLOB_STA);
            WriteBMControlRegister (GLOB_STA, ulReg | GLOB_STA_AD3);
            
            //
            // We check if the modem is sleeping. If it is, we can shut off the
            // AC link also. We shut off the AC link also if the modem is not
            // there.
            //
            if ((ulReg & GLOB_STA_MD3) || !(ulReg & GLOB_STA_SCR))
            {
                // Set Codec to super sleep
                WriteCodecRegister (AC97REG_POWERDOWN, 0xFF00, -1);

                // Disable the AC-link signals
                ulReg = ReadBMControlRegister32 (GLOB_CNT);
                WriteBMControlRegister (GLOB_CNT, ulReg | GLOB_CNT_ACLOFF);
            }
            break;
    }
    
    //
    // Save the new state.  This local value is used to determine when to 
    // cache property accesses and when to permit the driver from accessing 
    // the hardware.
    //
    m_PowerState = NewState.DeviceState;
    DOUT (DBG_POWER, ("Entering D%d", (ULONG)m_PowerState -
                      (ULONG)PowerDeviceD0));
}


/*****************************************************************************
 * CAdapterCommon::QueryPowerChangeState
 *****************************************************************************
 * Query to see if the device can change to this power state
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::QueryPowerChangeState
(
    IN  POWER_STATE NewState
)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CAdapterCommon::QueryPowerChangeState]"));

    // Check here to see of a legitimate state is being requested
    // based on the device state and fail the call if the device/driver
    // cannot support the change requested.  Otherwise, return STATUS_SUCCESS.
    // Note: A QueryPowerChangeState() call is not guaranteed to always preceed
    // a PowerChangeState() call.

    // check the new state being requested
    switch (NewState.DeviceState)
	{
	    case PowerDeviceD0:
        case PowerDeviceD1:
	    case PowerDeviceD2:
        case PowerDeviceD3:
		    return STATUS_SUCCESS;
        
        default:
            DOUT (DBG_ERROR, ("Unknown device state: D%d.", 
                 (ULONG)NewState.DeviceState - (ULONG)PowerDeviceD0));
            return STATUS_NOT_IMPLEMENTED;
	}
}


/*****************************************************************************
 * CAdapterCommon::QueryDeviceCapabilities
 *****************************************************************************
 * Called at startup to get the caps for the device.  This structure provides
 * the system with the mappings between system power state and device power
 * state.  This typically will not need modification by the driver.
 * If the driver modifies these mappings then the driver is not allowed to
 * change the mapping to a weaker power state (e.g. from S1->D3 to S1->D1).
 * 
 */
STDMETHODIMP_(NTSTATUS) CAdapterCommon::QueryDeviceCapabilities
(
    IN  PDEVICE_CAPABILITIES PowerDeviceCaps
)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CAdapterCommon::QueryDeviceCapabilities]"));

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::RestoreAC97Registers
 *****************************************************************************
 * Preset the AC97 registers with default values. The routine first checks if
 * There are registry entries for the default values. If not, we have hard
 * coded values too ;)
 */
NTSTATUS CAdapterCommon::SetAC97Default (void)
{
    PAGED_CODE ();
    
    PREGISTRYKEY    DriverKey;
    PREGISTRYKEY    SettingsKey;
    UNICODE_STRING  sKeyName;
    ULONG           ulDisposition;
    ULONG           ulResultLength;
    PVOID           KeyInfo = NULL;

    DOUT (DBG_PRINT, ("[CAdapterCommon::SetAC97Default]"));
    
    // open the driver registry key
    NTSTATUS ntStatus = PcNewRegistryKey (&DriverKey,        // IRegistryKey
                                          NULL,              // OuterUnknown
                                          DriverRegistryKey, // Registry key type
                                          KEY_ALL_ACCESS,    // Access flags
                                          m_pDeviceObject,   // Device object
                                          NULL,              // Subdevice
                                          NULL,              // ObjectAttributes
                                          0,                 // Create options
                                          NULL);             // Disposition
    if (NT_SUCCESS (ntStatus))
    {
        // make a unicode string for the subkey name
        RtlInitUnicodeString (&sKeyName, L"Settings");

        // open the settings subkey
        ntStatus = DriverKey->NewSubKey (&SettingsKey,            // Subkey
                                         NULL,                    // OuterUnknown
                                         KEY_ALL_ACCESS,          // Access flags
                                         &sKeyName,               // Subkey name
                                         REG_OPTION_NON_VOLATILE, // Create options
                                         &ulDisposition);

        if (NT_SUCCESS (ntStatus))
        {
            // allocate data to hold key info
            KeyInfo = ExAllocatePool (PagedPool,
                                      sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                                      sizeof(WORD));
            if (NULL != KeyInfo)
            {
                // loop through all mixer settings
                for (AC97Register i = AC97REG_RESET; i <= AC97REG_RESERVED2;
                    i = (AC97Register)(i + 1))
                {
                    if (m_stAC97Registers[i].wFlags & SHREG_INIT)
                    {
                        // init key name
                        RtlInitUnicodeString (&sKeyName,
                                              m_stAC97Registers[i].sRegistryName);
    
                        // query the value key
                        ntStatus = SettingsKey->QueryValueKey (&sKeyName,
                                        KeyValuePartialInformation,
                                        KeyInfo,
                                        sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                                            sizeof(WORD),
                                        &ulResultLength);
                        if (NT_SUCCESS (ntStatus))
                        {
                            PKEY_VALUE_PARTIAL_INFORMATION PartialInfo =
                                        (PKEY_VALUE_PARTIAL_INFORMATION)KeyInfo;

                            if (PartialInfo->DataLength == sizeof(WORD))
                            {
                                // set mixer register to registry value
                                WriteCodecRegister
                                    (i, *(PWORD)PartialInfo->Data, -1);
                            }
                            else    // write the hard coded default
                            {
                                // if key access failed, set to default
                                WriteCodecRegister
                                    (i, m_stAC97Registers[i].wWantedDefault, -1);
                            }
                        }
                        else  // write the hard coded default
                        {
                            // if key access failed, set to default
                            WriteCodecRegister
                                (i, m_stAC97Registers[i].wWantedDefault, -1);
                        }
                    }
                }

                // we want to return status success even if the last QueryValueKey
                // failed.
                ntStatus = STATUS_SUCCESS;

                // free the key info
                ExFreePool (KeyInfo);
            }

            // release the settings key
            SettingsKey->Release ();
        }

        // release the driver key
        DriverKey->Release ();
    }


    // in case we did not query the registry (cause of lack of resources)
    // restore default values and return insufficient resources.
    if (!NT_SUCCESS (ntStatus) || !KeyInfo)
    {
        // copy hard coded default settings
        for (AC97Register i = AC97REG_RESET; i < AC97REG_RESERVED2;
             i = (AC97Register)(i + 1))
        {
            if (m_stAC97Registers[i].wFlags & SHREG_INIT)
            {
                WriteCodecRegister (i, m_stAC97Registers[i].wWantedDefault, -1);
            }
        }

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}


/*****************************************************************************
 * CAdapterCommon::DisableAC97Pin
 *****************************************************************************
 * Returns TRUE when the HW vendor wants to disable the pin. A disabled pin is
 * not shown to the user (means it is not included in the topology). The
 * reason for doing this could be that some of the input lines like Aux or
 * Video are not available to the user (to plug in something) but the codec
 * can handle those lines.
 */
BOOL CAdapterCommon::DisableAC97Pin
(
    IN  TopoPinConfig pin
)
{
    PAGED_CODE ();

    PREGISTRYKEY    DriverKey;
    PREGISTRYKEY    SettingsKey;
    UNICODE_STRING  sKeyName;
    ULONG           ulDisposition;
    ULONG           ulResultLength;
    PVOID           KeyInfo = NULL;
    BOOL            bDisable = FALSE;

    DOUT (DBG_PRINT, ("[CAdapterCommon::DisableAC97Pin]"));
    
    // open the driver registry key
    NTSTATUS ntStatus = PcNewRegistryKey (&DriverKey,        // IRegistryKey
                                          NULL,              // OuterUnknown
                                          DriverRegistryKey, // Registry key type
                                          KEY_ALL_ACCESS,    // Access flags
                                          m_pDeviceObject,   // Device object
                                          NULL,              // Subdevice
                                          NULL,              // ObjectAttributes
                                          0,                 // Create options
                                          NULL);             // Disposition
    if (NT_SUCCESS (ntStatus))
    {
        // make a unicode string for the subkey name
        RtlInitUnicodeString (&sKeyName, L"Settings");

        // open the settings subkey
        ntStatus = DriverKey->NewSubKey (&SettingsKey,            // Subkey
                                         NULL,                    // OuterUnknown
                                         KEY_ALL_ACCESS,          // Access flags
                                         &sKeyName,               // Subkey name
                                         REG_OPTION_NON_VOLATILE, // Create options
                                         &ulDisposition);

        if (NT_SUCCESS (ntStatus))
        {
            // allocate data to hold key info
            KeyInfo = ExAllocatePool (PagedPool,
                                      sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                                      sizeof(BYTE));
            if (NULL != KeyInfo)
            {
                // init key name
                RtlInitUnicodeString (&sKeyName, m_stHardwareConfig.
                                            Pins[pin].sRegistryName);
    
                // query the value key
                ntStatus = SettingsKey->QueryValueKey (&sKeyName,
                                   KeyValuePartialInformation,
                                   KeyInfo,
                                   sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                                        sizeof(BYTE),
                                   &ulResultLength );
                if (NT_SUCCESS (ntStatus))
                {
                    PKEY_VALUE_PARTIAL_INFORMATION PartialInfo =
                                (PKEY_VALUE_PARTIAL_INFORMATION)KeyInfo;

                    if (PartialInfo->DataLength == sizeof(BYTE))
                    {
                        // store the value
                        if (*(PBYTE)PartialInfo->Data)
                            bDisable = TRUE;
                        else
                            bDisable = FALSE;
                    }
                }

                // free the key info
                ExFreePool (KeyInfo);
            }

            // release the settings key
            SettingsKey->Release ();
        }

        // release the driver key
        DriverKey->Release ();
    }

    // if one of the stuff above fails we return the default, which is FALSE.
    return bDisable;
}


/*****************************************************************************
 * CAdapterCommon::RestoreCodecRegisters
 *****************************************************************************
 * write back cached mixer values to codec registers
 */
NTSTATUS CAdapterCommon::RestoreCodecRegisters (void)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CAdapterCommon::RestoreCodecRegisters]"));

    //
    // Initialize the HDA codec from scratch
    //
    NTSTATUS ntStatus = InitHDA ();
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    //
    // Restore all codec registers.  Failure is not critical but we will
    // print a message just the same.
	//

	//   for (AC97Register i = AC97REG_MASTER_VOLUME; i < AC97REG_RESERVED2; 
	//      i = (AC97Register)(i + 1))
	//	{
	//     WriteCodecRegister (i, m_stAC97Registers[i].wCache, -1);
	//	}

	return STATUS_SUCCESS;
}

/*****************************************************************************
 * Non paged code begins here
 *****************************************************************************
 */ 

#pragma code_seg()

STDMETHODIMP_(ULONG) CAdapterCommon::hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command) {
	DOUT (DBG_PRINT, ("[CAdapterCommon::hda_send_verb]"));
	//using CORB/RIRB interface only (immediate cmd interface is not sdupported)
	//TODO: check sizes of components passed in
	//TODO: locking
	//TODO: check for unsolicited responses and maybe schedule a DPC to deal with them

	ULONG value = ((codec<<28) | (node<<20) | (verb<<8) | (command));
	DOUT (DBG_SYSINFO, ("Write codec verb 0x%X position %d", value, CorbPointer));
 
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
			//	components->hda[selected_hda_card].communication_type = HDA_UNINITALIZED;
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

	//return response
	return value;
	//TODO: implement immediate command interface if it is ever necessary
}



/*****************************************************************************
 * CAdapterCommon::WriteBMControlRegister
 *****************************************************************************
 * Writes a byte (UCHAR) to BusMaster Control register.
 */
STDMETHODIMP_(void) CAdapterCommon::WriteBMControlRegister
(
    IN  ULONG ulOffset,
    IN  UCHAR ucValue
)
{
    DOUT (DBG_PRINT, ("[CAdapterCommon::WriteBMControlRegister] (UCHAR)"));
    
    WRITE_PORT_UCHAR ((PUCHAR)(m_pBusMasterBase + ulOffset), ucValue);

    DOUT (DBG_REGS, ("WriteBMControlRegister wrote 0x%2x to 0x%4x.", 
                   ucValue, m_pBusMasterBase + ulOffset));
}

/*****************************************************************************
 * CAdapterCommon::WriteBMControlRegister
 *****************************************************************************
 * Writes a word (USHORT) to BusMaster Control register.
 */
STDMETHODIMP_(void) CAdapterCommon::WriteBMControlRegister
(
    IN  ULONG ulOffset,
    IN  USHORT usValue
)
{
    DOUT (DBG_PRINT, ("[CAdapterCommon::WriteBMControlRegister (USHORT)]"));
	
    WRITE_PORT_USHORT ((PUSHORT)(m_pBusMasterBase + ulOffset), usValue);

    DOUT (DBG_REGS, ("WriteBMControlRegister wrote 0x%4x to 0x%4x", 
                   usValue, m_pBusMasterBase + ulOffset));
}

/*****************************************************************************
 * CAdapterCommon::WriteBMControlRegister
 *****************************************************************************
 * Writes a DWORD (ULONG) to BusMaster Control register.
 */
STDMETHODIMP_(void) CAdapterCommon::WriteBMControlRegister
(
    IN  ULONG ulOffset,
    IN  ULONG ulValue
)
{
    DOUT (DBG_PRINT, ("[CAdapterCommon::WriteBMControlRegister (ULONG)]"));
	
    WRITE_PORT_ULONG ((PULONG)(m_pBusMasterBase + ulOffset), ulValue);

    DOUT (DBG_REGS, ("WriteBMControlRegister wrote 0x%8x to 0x%4x.", 
                   ulValue, m_pBusMasterBase + ulOffset));
}

/*****************************************************************************
 * CAdapterCommon::ReadBMControlRegister8
 *****************************************************************************
 * Read a byte (UCHAR) from BusMaster Control register.
 */
STDMETHODIMP_(UCHAR) CAdapterCommon::ReadBMControlRegister8
(
    IN  ULONG ulOffset
)
{
    UCHAR ucValue = UCHAR(-1);

    DOUT (DBG_PRINT, ("[CAdapterCommon::ReadBMControlRegister8]"));
	
    ucValue = READ_PORT_UCHAR ((PUCHAR)(m_pBusMasterBase + ulOffset));
				
    DOUT (DBG_REGS, ("ReadBMControlRegister read 0x%2x from 0x%4x.", ucValue,
                   m_pBusMasterBase + ulOffset));

    return ucValue;
}

/*****************************************************************************
 * CAdapterCommon::ReadBMControlRegister16
 *****************************************************************************
 * Read a word (USHORT) from BusMaster Control register.
 */
STDMETHODIMP_(USHORT) CAdapterCommon::ReadBMControlRegister16
(
    IN  ULONG ulOffset
)
{
    USHORT usValue = USHORT(-1);

    DOUT (DBG_PRINT, ("[CAdapterCommon::ReadBMControlRegister16]"));
	
	usValue = READ_PORT_USHORT ((PUSHORT)(m_pBusMasterBase + ulOffset));
				
    DOUT (DBG_REGS, ("ReadBMControlRegister read 0x%4x = 0x%4x", usValue,
                   m_pBusMasterBase + ulOffset));

    return usValue;
}

/*****************************************************************************
 * CAdapterCommon::ReadBMControlRegister32
 *****************************************************************************
 * Read a dword (ULONG) from BusMaster Control register.
 */
STDMETHODIMP_(ULONG) CAdapterCommon::ReadBMControlRegister32
(
    IN  ULONG ulOffset
)
{
    ULONG ulValue = ULONG(-1);

    DOUT (DBG_PRINT, ("[CAdapterCommon::ReadBMControlRegister32]"));
	
	ulValue = READ_PORT_ULONG ((PULONG)(m_pBusMasterBase + ulOffset));
				
    DOUT (DBG_REGS, ("ReadBMControlRegister read 0x%8x = 0x%4x", ulValue,
                   m_pBusMasterBase + ulOffset));

    return ulValue;
}

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

