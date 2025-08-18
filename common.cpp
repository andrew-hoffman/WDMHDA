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

ULONG audBufSize = 4410 * 2 * 2; //100ms of 2ch 16 bit audio

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
	//but it should be one driver object per HDA controller.
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
    if (!NT_SUCCESS (ntStatus)){
        DOUT (DBG_ERROR, ("Probing of hardware configuration failed!"));
        return ntStatus;
    }

	
	//SetAC97Default ();

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
	for(i = 0; i < 10; ++i)
		((PUSHORT)BufVirtualAddress)[i] = 0xeeee;
	for(i = 10; i < 20; ++i)
		((PUSHORT)BufVirtualAddress)[i] = 0xaaaa;
	for(i = 20; i < 30; ++i)
		((PUSHORT)BufVirtualAddress)[i] = 0x0000;

	DOUT(DBG_SYSINFO, ("garbage written in"));
	ProgramSampleRate(AC97REG_PCM_LR_RATE, 44100);

	ntStatus = hda_play_pcm_data_in_loop(BufLogicalAddress, audBufSize, 44100);
	if (!NT_SUCCESS (ntStatus)){
        DOUT (DBG_ERROR, ("Can't play anything"));
        return ntStatus;
    }
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

}



#if (DBG)
/*****************************************************************************
 * CAdapterCommon::DumpConfig
 *****************************************************************************
 * Dumps the HW configuration for the AC97 codec.
 */
void CAdapterCommon::DumpConfig (void)
{
//TODO: replace this if needed
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




/*****************************************************************************
 * CAdapterCommon::ProbeHWConfig
 *****************************************************************************
 * Probes the hardware configuration.
 * got rid of all the checking. for now we support NOTHING
 */
NTSTATUS CAdapterCommon::ProbeHWConfig (void)
{

	//TODO: delete all of this. for now i just disable everything optional
    PAGED_CODE ();

    NTSTATUS ntStatus = STATUS_SUCCESS;
    WORD     wCodecID;
    WORD     wCodecReg;

    DOUT (DBG_PRINT, ("[CAdapterCommon::ProbeHWConfig]"));

    SetPinConfig (PINC_MICIN_PRESENT, FALSE);
    SetPinConfig (PINC_HPOUT_PRESENT, FALSE);

    SetNodeConfig (NODEC_3D_PRESENT, FALSE);
    SetPinConfig (PINC_MIC_PRESENT, FALSE);
    SetPinConfig (PINC_LINEIN_PRESENT, FALSE);
    SetPinConfig (PINC_CD_PRESENT, FALSE);
    SetPinConfig (PINC_MONOOUT_PRESENT, FALSE);
    SetPinConfig (PINC_PCBEEP_PRESENT, FALSE);
    SetPinConfig (PINC_PHONE_PRESENT, FALSE);
    SetPinConfig (PINC_VIDEO_PRESENT, FALSE);
    SetPinConfig (PINC_AUX_PRESENT, FALSE);
    SetPinConfig (PINC_MIC2_PRESENT, FALSE);
	SetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE, FALSE);
	SetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE, TRUE);
    SetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE, FALSE);
    SetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE, FALSE);

    SetNodeConfig (NODEC_6BIT_MASTER_VOLUME, FALSE);

	SetNodeConfig (NODEC_6BIT_HPOUT_VOLUME, FALSE);
    SetNodeConfig (NODEC_6BIT_MONOOUT_VOLUME, FALSE);
    // We want to disable all features, so lets assume a read with
    // value 0, that will disable all features (see above).
    wCodecReg = 0;



    //
    // Store the information
    //
    SetNodeConfig (NODEC_PCM_VARIABLERATE_SUPPORTED, wCodecReg & 0x0001);
    SetNodeConfig (NODEC_PCM_DOUBLERATE_SUPPORTED, wCodecReg & 0x0002);
    SetNodeConfig (NODEC_MIC_VARIABLERATE_SUPPORTED, wCodecReg & 0x0008);
    SetNodeConfig (NODEC_CENTER_DAC_PRESENT, wCodecReg & 0x0040);
    SetNodeConfig (NODEC_SURROUND_DAC_PRESENT, wCodecReg & 0x0080);
    SetNodeConfig (NODEC_LFE_DAC_PRESENT, wCodecReg & 0x0100);

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
	/*
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
	*/
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

    DOUT (DBG_PRINT, ("Trying to read AC97 register 0x%X", reg));
    /*
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
	*/

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

   DOUT (DBG_PRINT, ("Trying to write AC97 register 0x%X with 0x%X", reg, wData));
   /*
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
    
    */
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

	/*
    
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
	*/
	return STATUS_SUCCESS;
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
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CAdapterCommon::PowerUpCodec]"));

    //
    // Power up the Codec.
    //
    //WriteCodecRegister (AC97REG_POWERDOWN, 0x00, -1);

    //
    // Wait for the Codec to be powered up.
    //
    //ULONGLONG ullStartTime = PcGetTimeInterval (0);
    //do
    //{
        //
        // Read the power management register.
        //
     //   ntStatus = ReadCodecRegister (AC97REG_POWERDOWN, &wCodecReg);
      //  if (!NT_SUCCESS (ntStatus))
      //  {
      //      wCodecReg = 0;      // Will cause an error.
      //      break;
      //  }

        //
        // Check the power state. Should be ready.
        //
       // if ((wCodecReg & 0x0f) == 0x0f)
      //      break;

        //
        // Let's wait a little, 50us and then try again.
        //
       // KeStallExecutionProcessor (50L);
    //} while (PcGetTimeInterval (ullStartTime) < GTI_MILLISECONDS (1000));

    // Check if we timed out.
    //if ((wCodecReg & 0x0f) != 0x0f)
    //{
    //    DOUT (DBG_ERROR, ("PowerUpCodec timed out. CoDec not powered up."));
    //    ntStatus = STATUS_DEVICE_NOT_READY;
    //}

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
    IN  AC97Register    Register,
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
    
	/*
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
	*/

    return STATUS_SUCCESS;
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
    // Initialize the HDA codec from scratch. nevermind trying to cache anything
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
	//DOUT (DBG_PRINT, ("[CAdapterCommon::hda_send_verb]"));
	//using CORB/RIRB interface only (immediate cmd interface is not supported)

	//TODO: check sizes of components passed in 
	//TODO: check for unsolicited responses and maybe schedule a DPC to deal with them

	ULONG value = ((codec<<28) | (node<<20) | (verb<<8) | (command));
	//DOUT (DBG_SYSINFO, ("Write codec verb 0x%X position %d", value, CorbPointer));
 
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
			//	communication_type = HDA_UNinitializeD;
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

	//TODO: unlock mutex

	//return response
	return value;
	//TODO: implement immediate command interface if it is ever necessary
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
    
    //WRITE_PORT_UCHAR ((PUCHAR)(m_pBusMasterBase + ulOffset), ucValue);

    DOUT (DBG_REGS, ("WriteBMControlRegister tried to write 0x%2x to 0x%4x.", 
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
	
    //WRITE_PORT_USHORT ((PUSHORT)(m_pBusMasterBase + ulOffset), usValue);

    DOUT (DBG_REGS, ("WriteBMControlRegister tried to write 0x%4x to 0x%4x", 
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
	
    //WRITE_PORT_ULONG ((PULONG)(m_pBusMasterBase + ulOffset), ulValue);

    DOUT (DBG_REGS, ("WriteBMControlRegister tried to write 0x%8x to 0x%4x.", 
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
	
    //ucValue = READ_PORT_UCHAR ((PUCHAR)(m_pBusMasterBase + ulOffset));
				
    DOUT (DBG_REGS, ("ReadBMControlRegister tried to read read 0x%2x from 0x%4x.", ucValue,
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
	
	//usValue = READ_PORT_USHORT ((PUSHORT)(m_pBusMasterBase + ulOffset));
				
    DOUT (DBG_REGS, ("ReadBMControlRegister tried to read 0x%4x = 0x%4x", usValue,
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
	
	//ulValue = READ_PORT_ULONG ((PULONG)(m_pBusMasterBase + ulOffset));
				
    DOUT (DBG_REGS, ("ReadBMControlRegister tried to read 0x%8x = 0x%4x", ulValue,
                   m_pBusMasterBase + ulOffset));

    return ulValue;
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

STDMETHODIMP_(NTSTATUS) CAdapterCommon::hda_play_pcm_data_in_loop(PHYSICAL_ADDRESS physAddress, ULONG bufSize, ULONG sample_rate) {

	NTSTATUS ntStatus = STATUS_SUCCESS;

	//fill buffer entries
	BdlMemVirt[0] = physAddress.LowPart;
	BdlMemVirt[1] = physAddress.HighPart;
	BdlMemVirt[2] = bufSize;
	BdlMemVirt[3] = 0;

	//fill buffer entries - there have to be at least two entries in BDL, so i write same one twice
	//writing all-zeros does not seem to work on Virtualbox for some reason.
	BdlMemVirt[4] = physAddress.LowPart;
	BdlMemVirt[5] = physAddress.HighPart;
	BdlMemVirt[6] = bufSize;
	BdlMemVirt[7] = 0;

	DOUT(DBG_SYSINFO, ("Playing buffer at at 0x%X%X size %d", physAddress.HighPart, physAddress.LowPart, bufSize));

	KeFlushIoBuffers(mdl, FALSE, TRUE); 
	//flush processor cache to RAM to be sure sound card will read correct data? if this does anything?

	//set buffer registers
	writeULONG(OutputStreamBase + 0x18, BdlMemPhys.LowPart);
	writeULONG(OutputStreamBase + 0x1C, BdlMemPhys.HighPart);
	writeULONG(OutputStreamBase + 0x08, bufSize * 2);
	writeUSHORT(OutputStreamBase + 0x0C, 1); //there are two entries in buffer

	DOUT(DBG_SYSINFO, ("buffer address programmed"));


	//set stream data format
	writeUSHORT(OutputStreamBase + 0x12, hda_return_sound_data_format(sample_rate, 2, 16));

	//set Audio Output node data format
	hda_send_verb(codecNumber, audio_output_node_number, 0x200, hda_return_sound_data_format(sample_rate, 2, 16));
	if(second_audio_output_node_number != 0) {
		hda_send_verb(codecNumber, second_audio_output_node_number, 0x200, hda_return_sound_data_format(sample_rate, 2, 16));
	}
	KeStallExecutionProcessor(10);

	DOUT(DBG_SYSINFO, ("ready to start the stream"));


	//start streaming to stream 1
	writeUCHAR(OutputStreamBase + 0x02, 0x14);
	writeUCHAR(OutputStreamBase + 0x00, 0x02);

	DOUT(DBG_SYSINFO, ("playing"));

	
    return ntStatus;
}

USHORT CAdapterCommon::hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample) {
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

