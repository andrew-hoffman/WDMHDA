/*****************************************************************************
 * adapter.cpp - HDA adapter driver implementation.
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation.  All rights reserved.
 *
 * This files does setup and resource allocation/verification for the HDA
 * card. It controls which miniports are started and which resources are
 * given to each miniport. It also deals with interrupt sharing between
 * miniports by hooking the interrupt and calling the correct DPC.
 */

//
// All the GUIDS for all the miniports end up in this object.
//
#define PUT_GUIDS_HERE

#define STR_MODULENAME "HDAAdapter: "

#define DEFINE_DEBUG_VARS

#include "common.h"


/*****************************************************************************
 * Defines
 */
#define MAX_MINIPORTS 5

#if (DBG)
#define SUCCEEDS(s) ASSERT(NT_SUCCESS(s))
#else
#define SUCCEEDS(s) (s)
#endif




/*****************************************************************************
 * Externals
 */
NTSTATUS
CreateMiniportWaveCyclicHDA
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);
NTSTATUS
CreateMiniportTopologyHDA
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
);





/*****************************************************************************
 * Referenced forward
 */
extern "C"
NTSTATUS
AddDevice
(
    IN PDRIVER_OBJECT   DriverObject,
    IN PDEVICE_OBJECT   PhysicalDeviceObject
);

NTSTATUS
StartDevice
(
    IN  PDEVICE_OBJECT  DeviceObject,   // Device object.
    IN  PIRP            Irp,            // IO request packet.
    IN  PRESOURCELIST   ResourceList    // List of hardware resources.
);

BOOLEAN
AdapterISR
(
    IN PKINTERRUPT  Interrupt,
    IN PVOID        Context
);

NTSTATUS
AssignResources
(
    IN  PRESOURCELIST   ResourceList,           // All resources.
    OUT PRESOURCELIST * ResourceListTopology,   // Topology resources.
    OUT PRESOURCELIST * ResourceListWave,       // Wave resources.
    OUT PRESOURCELIST * ResourceListWaveTable,  // Wave table resources.
    OUT PRESOURCELIST * ResourceListFmSynth,    // FM synth resources.
    OUT PRESOURCELIST * ResourceListUart,       // UART resources.
    OUT PRESOURCELIST * ResourceListAdapter     // a copy needed by the adapter
);

#ifdef DO_RESOURCE_FILTERING
extern "C"
NTSTATUS
AdapterDispatchPnp
(
    IN      PDEVICE_OBJECT  pDeviceObject,
    IN      PIRP            pIrp
);
#endif


//configspace globals
//may be overwritten if more than one device added
//but hopefully we did the needful by then

ULONG pci_id;
USHORT pci_ven;
USHORT pci_dev;
	
ULONG pci_sid;
USHORT pci_ssid;
USHORT pci_svid;

PUCHAR pConfigMem;
PDEVICE_OBJECT  m_pDeviceObject;



#pragma code_seg("INIT")

/* Function to read/write PCI configuration space using IRP
from this KB article:
https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/obtaining-device-configuration-information-at-irql---passive-level
it's the only working example i found.
*/

STDMETHODIMP_(NTSTATUS) ReadWriteConfigSpace(
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
STDMETHODIMP_(NTSTATUS) WriteConfigSpaceByte(UCHAR offset, UCHAR andByte, UCHAR orByte){
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
STDMETHODIMP_(NTSTATUS) WriteConfigSpaceWord(UCHAR offset, USHORT andWord, USHORT orWord){
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

/*****************************************************************************
 * DriverEntry()
 *****************************************************************************
 * This function is called by the operating system when the driver is loaded.
 * All adapter drivers can use this code without change.
 */
extern "C"
NTSTATUS
DriverEntry
(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPathName
)
{
    PAGED_CODE();

    //
    // Tell the class driver to initialize the driver.
    //
    NTSTATUS ntStatus = PcInitializeAdapterDriver( DriverObject,
                                                   RegistryPathName,
                                                   AddDevice );

#ifdef DO_RESOURCE_FILTERING
    //
    // We want to do resource filtering, so we'll install our own PnP IRP handler.
    //
    if(NT_SUCCESS(ntStatus))
    {
        DriverObject->MajorFunction[IRP_MJ_PNP] = AdapterDispatchPnp;
    }
#endif

    return ntStatus;
}

#pragma code_seg("PAGE")
/*****************************************************************************
 * AddDevice()
 *****************************************************************************
 * This function is called by the operating system when the device is added.
 * All adapter drivers can use this code without change.
 */
extern "C"
NTSTATUS
AddDevice
(
    IN PDRIVER_OBJECT   DriverObject,
    IN PDEVICE_OBJECT   PhysicalDeviceObject
)
{
    PAGED_CODE();

    DOUT(DBG_SYSINFO, ("PDO 0x%X", 
				PhysicalDeviceObject));
	DOUT(DBG_SYSINFO, ("FDO 0x%X", 
				DriverObject));
	
	//While we have the PDO here:
	//send an IRP asking the bus driver to read all of configspace
	//asking the PnP Config manager does NOT work since we're still
	//in the middle of AddDevice() which it's doing
	
	NTSTATUS ntStatus;
	m_pDeviceObject = PhysicalDeviceObject;
	
	pConfigMem = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 256,'gfcP');
	if (!pConfigMem) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	ntStatus = ReadWriteConfigSpace(
		PhysicalDeviceObject,
		0,			//read
		pConfigMem,	// Buffer to store the configuration data		
		0,			// Offset into the configuration space
		256         // Number of bytes to read
	);

	if (!NT_SUCCESS (ntStatus)){
		DbgPrint( "\nPCI Configspace Read Failed! 0x%X\n", ntStatus);
		ExFreePool(pConfigMem);
		pConfigMem = NULL;
        return ntStatus;
	}

	PUSHORT pConfigMemS = (PUSHORT)pConfigMem;
	PULONG  pConfigMemD = (PULONG) pConfigMem;

	//VID and PID are first 2 words of configspace
	pci_id = (ULONG) pConfigMemD[0];
	pci_ven = (USHORT) pConfigMemS[0];
	pci_dev = (USHORT) pConfigMemS[1];
	
	//Subsystem and Sybsystem Vendor are at 0xB(Dword) or 0x2c(Byte)
	pci_sid = (ULONG) pConfigMemD[0xB];
	pci_ssid = (USHORT)(pci_sid >> 16);
	pci_svid = (USHORT)(pci_sid & 0xFFFF);

	DbgPrint( "\nHDA Controller:  VID:0x%04X  PID:0x%04X ", pci_ven, pci_dev);

	USHORT tmp;
	
	//apply device-specific config space patches depending on the VID/PID
	switch (pci_ven){
		case 0x15AD: //VMWare
			DbgPrint( "VMWare\n");
			//force PIO mode because VMWare works better like this
			//Registry::forcePioMode = true;
			break;
		case 0x1002: //ATI
		case 0x1022: //AMD
			if ( (pci_dev == 0x437b) || (pci_dev == 0x4383) //ATI SB
				|| (pci_dev == 0x780d) //Hudson
				|| (pci_dev == 0x1457) || (pci_dev == 0x1487) //AMD Ryzen
				|| (pci_dev == 0x157a) || (pci_dev == 0x15e3) //AMD APU
			){
				DbgPrint( "ATI/AMD SB450/600/APU Snoop\n");
				//Enable Snoop
				ntStatus = WriteConfigSpaceByte(0x42,
					~0x03, ATI_SB450_HDAUDIO_ENABLE_SNOOP);
				//should read back & confirm it was set.
			} else if ( (pci_dev == 0x0002) || (pci_dev == 0x1308)
				|| (pci_dev == 0x157a) || (pci_dev == 0x15b3)
			){				
				DbgPrint( "ATI/AMD HDMI No Snoop\n");
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
				case 0x7a50: //Raptor Lake

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

				//just in case
				default:

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

			}
			break;
		case 0x10b9://ULI M5461
			DbgPrint( "ULI\n");
			//disable and zero out BAR1 on this hardware; 
			//it advertises 64 bit addressing support but can't deliver
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
	// Set TCSEL (offset 44h in config space, lowest 3 bits) to 0 
	// to avoid crackling/static on some hardware.
	// the Watlers and MPXPlay drivers set this byte on all but ATI controllers

	// Traffic class 0 is the default "best effort" priority
	// but is the only one guaranteed to be set up on non PCIe aware OSes.
	// some HDA hardware defaults to traffic class 7

	if(pci_ven != 0x1002){
		ntStatus = WriteConfigSpaceByte(0x44, 0xf8, 0x0);
	}

	if (!NT_SUCCESS (ntStatus)){
		DbgPrint( "\nPCI Config Space Write Failed! 0x%X\n", ntStatus);
        //return ntStatus; //is failure fatal? i dont think so
	}

	//free device description
	if (pConfigMem) {
		ExFreePool(pConfigMem);
		pConfigMem = NULL;
	}

	DbgPrint( "     Subsystem: SSID:0x%04X SVID:0x%04X\n\n", pci_ssid, pci_svid);
    
	//
    // Tell the class driver to add the device.
    //

    return PcAddAdapterDevice( DriverObject,
                               PhysicalDeviceObject,
                               PCPFNSTARTDEVICE( StartDevice ),
                               MAX_MINIPORTS,
                               0 );
}

/*****************************************************************************
 * InstallSubdevice()
 *****************************************************************************
 * This function creates and registers a subdevice consisting of a port
 * driver, a minport driver and a set of resources bound together.  It will
 * also optionally place a pointer to an interface on the port driver in a
 * specified location before initializing the port driver.  This is done so
 * that a common ISR can have access to the port driver during initialization,
 * when the ISR might fire.
 */
NTSTATUS
InstallSubdevice
(
    IN      PDEVICE_OBJECT      DeviceObject,
    IN      PIRP                Irp,
    IN      PWCHAR              Name,
    IN      REFGUID             PortClassId,
    IN      REFGUID             MiniportClassId,
    IN      PFNCREATEINSTANCE   MiniportCreate      OPTIONAL,
    IN      PUNKNOWN            UnknownAdapter      OPTIONAL,
    IN      PRESOURCELIST       ResourceList,
    IN      REFGUID             PortInterfaceId,
    OUT     PUNKNOWN *          OutPortInterface,   OPTIONAL
    OUT     PUNKNOWN *          OutPortUnknown      OPTIONAL
)
{
    PAGED_CODE();
    _DbgPrintF(DEBUGLVL_VERBOSE, ("InstallSubdevice"));

    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(Name);
    ASSERT(ResourceList);

    //
    // Create the port driver object
    //
    PPORT       port;
    NTSTATUS    ntStatus = PcNewPort(&port,PortClassId);

    if (NT_SUCCESS(ntStatus))
    {
        //
        // Deposit the port somewhere if it's needed.
        //
        if (OutPortInterface)
        {
            //
            //  Failure here doesn't cause the entire routine to fail.
            //
            (void) port->QueryInterface
            (
                PortInterfaceId,
                (PVOID *) OutPortInterface
            );
        }

        PUNKNOWN miniport;
        if (NT_SUCCESS(ntStatus))
        {
            //
            // Create the miniport object
            //
            if (MiniportCreate)
            {
                ntStatus = MiniportCreate
                (
                    &miniport,
                    MiniportClassId,
                    NULL,
                    NonPagedPool
                );
            }
            else
            {
                ntStatus = PcNewMiniport((PMINIPORT*) &miniport,MiniportClassId);
            }
        }

        if (NT_SUCCESS(ntStatus))
        {
            //
            // Init the port driver and miniport in one go.
            //
            ntStatus = port->Init( DeviceObject,
                                   Irp,
                                   miniport,
                                   UnknownAdapter,
                                   ResourceList );

            if (NT_SUCCESS(ntStatus))
            {
                //
                // Register the subdevice (port/miniport combination).
                //
                ntStatus = PcRegisterSubdevice( DeviceObject,
                                                Name,
                                                port );
#if DBG
                if (!(NT_SUCCESS(ntStatus)))
                {
                    _DbgPrintF(DEBUGLVL_TERSE, ("StartDevice: PcRegisterSubdevice failed"));
                }
#endif
            }
            else
            {
                _DbgPrintF(DEBUGLVL_TERSE, ("InstallSubdevice: port->Init failed"));
            }

            //
            // We don't need the miniport any more.  Either the port has it,
            // or we've failed, and it should be deleted.
            //
            miniport->Release();
        }
        else
        {
            _DbgPrintF(DEBUGLVL_TERSE, ("InstallSubdevice: PcNewMiniport failed"));
        }

        if (NT_SUCCESS(ntStatus))
        {
            //
            // Deposit the port as an unknown if it's needed.
            //
            if (OutPortUnknown)
            {
                //
                //  Failure here doesn't cause the entire routine to fail.
                //
                (void) port->QueryInterface
                (
                    IID_IUnknown,
                    (PVOID *) OutPortUnknown
                );
            }
        }
        else
        {
            //
            // Retract previously delivered port interface.
            //
            if (OutPortInterface && (*OutPortInterface))
            {
                (*OutPortInterface)->Release();
                *OutPortInterface = NULL;
            }
        }

        //
        // Release the reference which existed when PcNewPort() gave us the
        // pointer in the first place.  This is the right thing to do
        // regardless of the outcome.
        //
        port->Release();
    }
    else
    {
        _DbgPrintF(DEBUGLVL_TERSE, ("InstallSubdevice: PcNewPort failed"));
    }

    return ntStatus;
}

/*****************************************************************************
 * StartDevice()
 *****************************************************************************
 * This function is called by the operating system when the device is started.
 * It is responsible for starting the miniports.  This code is specific to
 * the adapter because it calls out miniports for functions that are specific
 * to the adapter.
 */
NTSTATUS
StartDevice
(
    IN  PDEVICE_OBJECT  DeviceObject,   // Device object.
    IN  PIRP            Irp,            // IO request packet.
    IN  PRESOURCELIST   ResourceList    // List of hardware resources.
)
{
    PAGED_CODE();


    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(ResourceList);

    //
    // These are the sub-lists of resources that will be handed to the
    // miniports.
    //
    PRESOURCELIST   resourceListTopology    = NULL;
    PRESOURCELIST   resourceListWave        = NULL;
    PRESOURCELIST   resourceListWaveTable   = NULL;
    PRESOURCELIST   resourceListFmSynth     = NULL;
    PRESOURCELIST   resourceListUart        = NULL;
    PRESOURCELIST   resourceListAdapter     = NULL;

    //
    // These are the port driver pointers we are keeping around for registering
    // physical connections.
    //
    PUNKNOWN    unknownTopology   = NULL;
    PUNKNOWN    unknownWave       = NULL;
    PUNKNOWN    unknownWaveTable  = NULL;
    PUNKNOWN    unknownFmSynth    = NULL;

    //
    // Assign resources to individual miniports.  Each sub-list is a copy
    // of the resources from the master list. Each sublist must be released.
    //
    NTSTATUS ntStatus = AssignResources( ResourceList,
                                         &resourceListTopology,
                                         &resourceListWave,
                                         &resourceListWaveTable,
                                         &resourceListFmSynth,
                                         &resourceListUart,
                                         &resourceListAdapter );

    //
    // if AssignResources succeeded...
    //
    if(NT_SUCCESS(ntStatus))
    {
        //
        // If the adapter has resources...
        //
        PADAPTERCOMMON pAdapterCommon = NULL;
        if (resourceListAdapter)
        {
            PUNKNOWN pUnknownCommon;

            // create a new adapter common object
            ntStatus = NewAdapterCommon( &pUnknownCommon,
                                         IID_IAdapterCommon,
                                         NULL,
                                         NonPagedPool );
            if (NT_SUCCESS(ntStatus))
            {
                ASSERT( pUnknownCommon );

                // query for the IAdapterCommon interface
                ntStatus = pUnknownCommon->QueryInterface( IID_IAdapterCommon,
                                                           (PVOID *)&pAdapterCommon );
                if (NT_SUCCESS(ntStatus))
                {
                    // Initialize the object
                    ntStatus = pAdapterCommon->Init( resourceListAdapter,
                                                     DeviceObject);
                    if (NT_SUCCESS(ntStatus))
                    {
                        // register with PortCls for power-managment services
                        ntStatus = PcRegisterAdapterPowerManagement( (PUNKNOWN)pAdapterCommon,
                                                                     DeviceObject );
                    }
                }

                // release the IUnknown on adapter common
                pUnknownCommon->Release();
            }

            // release the adapter common resource list
            resourceListAdapter->Release();
        }

        //
        // Start the topology miniport if it exists.
        //
        if (resourceListTopology)
        {
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = InstallSubdevice( DeviceObject,
                                             Irp,
                                             L"Topology",
                                             CLSID_PortTopology,
                                             CLSID_PortTopology, // not used
                                             CreateMiniportTopologyHDA,
                                             pAdapterCommon,
                                             resourceListTopology,
                                             GUID_NULL,
                                             NULL,
                                             &unknownTopology );
            }

            // release the topology resource list
            resourceListTopology->Release();
        }

        //
        // Start the SB wave miniport if it exists.
        //
        if (resourceListWave)
        {
            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = InstallSubdevice( DeviceObject,
                                             Irp,
                                             L"Wave",
                                             CLSID_PortWaveCyclic,
                                             CLSID_PortWaveCyclic,   // not used
                                             CreateMiniportWaveCyclicHDA,
                                             pAdapterCommon,
                                             resourceListWave,
                                             IID_IPortWaveCyclic,
                                             pAdapterCommon->WavePortDriverDest(),
                                             &unknownWave );
            }

            // release the wave resource list
            resourceListWave->Release();
        }

		//everything below this line cannot exist now as it was removed from ProcessResources

        // Start the wave table miniport if it exists.
        if (resourceListWaveTable)
        {
            //
            // NOTE: The wavetable is not currently supported in this sample driver.
            //

            // release the wavetable resource list
            resourceListWaveTable->Release();
        }

        //
        // Start the FM synth miniport if it exists.
        //
        if (resourceListFmSynth)
        {
            //
            // Synth not working yet.
			// This is supposed to pass an i/o port to the built-in FMSynth miniport
			// if we don't have hardware FM this cannot work.
			// TODO: build SBEMUL like driver to trap OPL ports & route to Nuked OPL3
            //

            if (NT_SUCCESS(ntStatus))
            {
                //
                // Failure here is not fatal.
                //
                InstallSubdevice( DeviceObject,
                                  Irp,
                                  L"FMSynth",
                                  CLSID_PortMidi,
                                  CLSID_MiniportDriverFmSynth,
                                  NULL,
                                  pAdapterCommon,
                                  resourceListFmSynth,
                                  GUID_NULL,
                                  NULL,
                                  &unknownFmSynth );
            }

            // release the FM synth resource list
            resourceListFmSynth->Release();
        }

        //
        // Start the UART miniport if it exists.
        //
        if (resourceListUart)
        {
            if (NT_SUCCESS(ntStatus))
            {
                //
                // Failure here is not fatal.
                //
                InstallSubdevice( DeviceObject,
                                  Irp,
                                  L"Uart",
                                  CLSID_PortDMus,
                                  CLSID_MiniportDriverDMusUART,
                                  NULL,
                                  pAdapterCommon->GetInterruptSync(),
                                  resourceListUart,
                                  IID_IPortDMus,
                                  pAdapterCommon->MidiPortDriverDest(),
                                  NULL );   // not physically connected to anything
            }

            resourceListUart->Release();
        }

        //
        // Establish physical connections between filters as shown.
        //
        //              +------+    +------+
        //              | Wave |    | Topo |
        //  Capture <---|0    1|<===|6    2|<--- CD
        //              |      |    |      |
        //   Render --->|2    3|===>|0    3|<--- Line In
        //              +------+    |      |
        //              +------+    |     4|<--- Mic
        //              |  FM  |    |      |
        //     MIDI --->|0    1|===>|1    5|---> Line Out
        //              +------+    +------+
        //
        if (unknownTopology)
        {
            if (unknownWave)
            {
                // register wave <=> topology connections
                PcRegisterPhysicalConnection( (PDEVICE_OBJECT)DeviceObject,
                                            unknownTopology,
                                            6,
                                            unknownWave,
                                            1 );
                PcRegisterPhysicalConnection( (PDEVICE_OBJECT)DeviceObject,
                                            unknownWave,
                                            3,
                                            unknownTopology,
                                            0 );
            }

            if (unknownFmSynth)
            {
                // register fmsynth <=> topology connection
                PcRegisterPhysicalConnection( (PDEVICE_OBJECT)DeviceObject,
                                            unknownFmSynth,
                                            1,
                                            unknownTopology,
                                            1 );
            }
        }

        //
        // Release the adapter common object.  It either has other references,
        // or we need to delete it anyway.
        //
        if (pAdapterCommon)
        {
            pAdapterCommon->Release();
        }

        //
        // Release the unknowns.
        //
        if (unknownTopology)
        {
            unknownTopology->Release();
        }
        if (unknownWave)
        {
            unknownWave->Release();
        }
        if (unknownWaveTable)
        {
            unknownWaveTable->Release();
        }
        if (unknownFmSynth)
        {
            unknownFmSynth->Release();
        }

    }

    return ntStatus;
}

/*****************************************************************************
 * AssignResources()
 *****************************************************************************
 * This function assigns the list of resources to the various functions on
 * the card.  This code is specific to the adapter.  All the non-NULL resource
 * lists handed back must be released by the caller.
 */
NTSTATUS
AssignResources
(
    IN      PRESOURCELIST   ResourceList,           // All resources.
    OUT     PRESOURCELIST * ResourceListTopology,   // Topology resources.
    OUT     PRESOURCELIST * ResourceListWave,       // Wave resources.
    OUT     PRESOURCELIST * ResourceListWaveTable,  // Wave table resources.
    OUT     PRESOURCELIST * ResourceListFmSynth,    // FM synth resources.
    OUT     PRESOURCELIST * ResourceListUart,       // Uart resources.
    OUT     PRESOURCELIST * ResourceListAdapter     // For the adapter
)
{
    BOOLEAN     detectedWaveTable   = FALSE;
    BOOLEAN     detectedUart        = FALSE;
    BOOLEAN     detectedFmSynth     = FALSE;

    //
    // Get counts for the types of resources.
    //
    ULONG countIO  = ResourceList->NumberOfPorts();
    ULONG countIRQ = ResourceList->NumberOfInterrupts();
    ULONG countDMA = ResourceList->NumberOfDmas();

    //
    // Determine the type of card based on port resources.
    //
    NTSTATUS ntStatus = STATUS_SUCCESS;

    switch (countIO)
    {
	case 0:
		//
        // HD Audio doesn't use IO ports or 8237 DMA. No FM synth or UART.
		if ((countIO != 0) || (countIRQ > 1) || (countDMA != 0))
		{
			DOUT (DBG_ERROR, ("Unknown configuration!\n"
                          "   IO  count: %d\n"
                          "   IRQ count: %d\n"
                          "   DMA count: %d",
                          countIO, countIRQ, countDMA));
			ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
		} else if (countIRQ == 0){
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		} else {
			ntStatus = STATUS_SUCCESS;
		}
        //removed other cases
		break;
    default:
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        break;
    }

    //
    // Build list of resources for the topology.
    //
    *ResourceListTopology = NULL;
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus =
            PcNewResourceSublist
            (
                ResourceListTopology,
                NULL,
                PagedPool,
                ResourceList,
                countIRQ + countDMA + 1
            );

        if (NT_SUCCESS(ntStatus))
        {
            SUCCEEDS((*ResourceListTopology)->
                AddMemoryFromParent(ResourceList,0));
        }
    }

    //
    // Build the resource list for the SB wave I/O.
    //
    *ResourceListWave = NULL;
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus =
            PcNewResourceSublist
            (
                ResourceListWave,
                NULL,
                PagedPool,
                ResourceList,
                countDMA + countIRQ + 1
            );

        if (NT_SUCCESS(ntStatus))
        {
            ULONG i;

            //
            // Add the base address
            //
            SUCCEEDS((*ResourceListWave)->
                AddMemoryFromParent(ResourceList,0));

            //
            // Add the DMA channel(s).
            //
            for (i = 0; i < countDMA; i++)
            {
                SUCCEEDS((*ResourceListWave)->
                    AddDmaFromParent(ResourceList,i));
            }

            //
            // Add the IRQ lines.
            //
            for (i = 0; i < countIRQ; i++)
            {
                SUCCEEDS((*ResourceListWave)->
                    AddInterruptFromParent(ResourceList,i));
            }
        }
    }

    //
    // Build list of resources for wave table.
    //
    *ResourceListWaveTable = NULL;
    if (NT_SUCCESS(ntStatus) && detectedWaveTable)
    {
        //
        // TODO:  Assign wave table resources.
        //
    }

    //
    // Build list of resources for UART.
    //
    *ResourceListUart = NULL;
    if (NT_SUCCESS(ntStatus) && detectedUart)
    {
        ntStatus =
            PcNewResourceSublist
            (
                ResourceListUart,
                NULL,
                PagedPool,
                ResourceList,
                2
            );

        if (NT_SUCCESS(ntStatus))
        {
            SUCCEEDS((*ResourceListUart)->
                AddMemoryFromParent(ResourceList,1));
            SUCCEEDS((*ResourceListUart)->
                AddInterruptFromParent(ResourceList,0));
        }
    }

    //
    // Build list of resources for FM synth.
    //
    *ResourceListFmSynth = NULL;
    if (NT_SUCCESS(ntStatus) && detectedFmSynth)
    {
        ntStatus =
            PcNewResourceSublist
            (
                ResourceListFmSynth,
                NULL,
                PagedPool,
                ResourceList,
                1
            );

        if (NT_SUCCESS(ntStatus))
        {
            SUCCEEDS((*ResourceListFmSynth)->
                AddMemoryFromParent(ResourceList,detectedUart ? 2 : 1));
        }
    }

    //
    // Build list of resources for the adapter.
    //
    *ResourceListAdapter = NULL;
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus =
            PcNewResourceSublist
            (
                ResourceListAdapter,
                NULL,
                PagedPool,
                ResourceList,
                3
            );

        if (NT_SUCCESS(ntStatus))
        {
            //
            // The interrupt to share.
            //
            SUCCEEDS((*ResourceListAdapter)->
                AddInterruptFromParent(ResourceList,0));

            //
            // The base IO port (to tell who's interrupt it is)
            //
            SUCCEEDS((*ResourceListAdapter)->
                AddMemoryFromParent(ResourceList,0));

            if (detectedUart)
            {
                //
                // The Uart port
                //
                SUCCEEDS((*ResourceListAdapter)->
                    AddMemoryFromParent(ResourceList,1));
            }
        }
    }

    //
    // Clean up if failure occurred.
    //
    if (! NT_SUCCESS(ntStatus))
    {
        if (*ResourceListWave)
        {
            (*ResourceListWave)->Release();
            *ResourceListWave = NULL;
        }
        if (*ResourceListWaveTable)
        {
            (*ResourceListWaveTable)->Release();
            *ResourceListWaveTable = NULL;
        }
        if (*ResourceListUart)
        {
            (*ResourceListUart)->Release();
            *ResourceListUart = NULL;
        }
        if (*ResourceListFmSynth)
        {
            (*ResourceListFmSynth)->Release();
            *ResourceListFmSynth = NULL;
        }
        if(*ResourceListAdapter)
        {
            (*ResourceListAdapter)->Release();
            *ResourceListAdapter = NULL;
        }
    }


    return ntStatus;
}



#ifdef DO_RESOURCE_FILTERING

#pragma code_seg("PAGE")
/*****************************************************************************
 * AdapterDispatchPnp()
 *****************************************************************************
 * Supplying your PnP resource filtering needs.
 */
extern "C"
NTSTATUS
AdapterDispatchPnp
(
    IN      PDEVICE_OBJECT  pDeviceObject,
    IN      PIRP            pIrp
)
{
    PAGED_CODE();

    ASSERT(pDeviceObject);
    ASSERT(pIrp);

    NTSTATUS ntStatus = STATUS_SUCCESS;

    PIO_STACK_LOCATION pIrpStack =
        IoGetCurrentIrpStackLocation(pIrp);

    if( pIrpStack->MinorFunction == IRP_MN_FILTER_RESOURCE_REQUIREMENTS )
    {
        //
        // Do your resource requirements filtering here!!
        //
        _DbgPrintF(DEBUGLVL_VERBOSE,("[AdapterDispatchPnp] - IRP_MN_FILTER_RESOURCE_REQUIREMENTS"));

        // set the return status
        pIrp->IoStatus.Status = ntStatus;

    }

    //
    // Pass the IRPs on to PortCls
    //
    ntStatus = PcDispatchIrp( pDeviceObject,
                              pIrp );

    return ntStatus;
}

#endif

#pragma code_seg()

/*****************************************************************************
 * _purecall()
 *****************************************************************************
 * The C++ compiler loves me.
 * TODO: Figure out how to put this into portcls.sys
 */
int __cdecl
_purecall( void )
{
    ASSERT( !"Pure virtual function called" );
    return 0;
}
