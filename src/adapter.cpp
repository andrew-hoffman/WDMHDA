/*****************************************************************************
 * adapter.cpp - HDA adapter driver implementation.
 *****************************************************************************
 * Original Sample Code:
 * Copyright (c) 1997-1999 Microsoft Corporation.  All rights reserved.
 * Released under MIT license as part of Windows Driver Kit 7.1
 * HD Audio driver code bt Drew Hoffman. copyright 2025-2026
 * Released under MIT license, see license file for details
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

// PortCls reserves device-extension slots 4-7 for adapter-driver use.
// Store the per-device PDO in slot 4 (offset 16 on i386).
#define ADAPTER_PDO_SLOT(DeviceObject) \
    (((PDEVICE_OBJECT *)(DeviceObject)->DeviceExtension)[4])

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


#pragma code_seg("INIT")

/*****************************************************************************
 * DriverEntry()
 *****************************************************************************
 * This function is called by the operating system when the driver is loaded.
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
 * note that PcAddAdapterDevice takes the PDO but only returns a NTSTATUS
 * and doesn't let us get at the DeviceExtension on the new DeviceObject
 * that it created. 
 * Hence the terrible workaround to allow for more than one in flight
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

    ASSERT(PhysicalDeviceObject != NULL);
    DOUT(DBG_SYSINFO, ("PDO 0x%X", PhysicalDeviceObject));
    DOUT(DBG_SYSINFO, ("DriverObject 0x%X", DriverObject));

    // Tell the class driver to add the device.
    NTSTATUS ntStatus = PcAddAdapterDevice( DriverObject,
                                            PhysicalDeviceObject,
                                            PCPFNSTARTDEVICE( StartDevice ),
                                            MAX_MINIPORTS,
                                            0 );
    if (NT_SUCCESS(ntStatus))
    {
        // PcAddAdapterDevice has attached its FDO to this PDO. Save the PDO
        // in PortCls' per-device extension for the later StartDevice call.
        PDEVICE_OBJECT fdo = IoGetAttachedDeviceReference(PhysicalDeviceObject);
        if (fdo != NULL)
        {
            ADAPTER_PDO_SLOT(fdo) = PhysicalDeviceObject;
            ObDereferenceObject(fdo);
        }
        else
        {
            ntStatus = STATUS_NO_SUCH_DEVICE;
        }
    }

    return ntStatus;


}

/*****************************************************************************
 * InstallSubdevice()
 *****************************************************************************/
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

            miniport->Release();
        }
        else
        {
            _DbgPrintF(DEBUGLVL_TERSE, ("InstallSubdevice: PcNewMiniport failed"));
        }

        if (NT_SUCCESS(ntStatus))
        {
            if (OutPortUnknown)
            {
                (void) port->QueryInterface
                (
                    IID_IUnknown,
                    (PVOID *) OutPortUnknown
                );
            }
        }
        else
        {
            if (OutPortInterface && (*OutPortInterface))
            {
                (*OutPortInterface)->Release();
                *OutPortInterface = NULL;
            }
        }

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
 */
NTSTATUS
StartDevice
(
    IN  PDEVICE_OBJECT  DeviceObject,           // Device object.
    IN  PIRP            Irp,                    // IO request packet.
    IN  PRESOURCELIST   ResourceList            // List of hardware resources.
)
{
    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(ResourceList);

    //
    // These are the sub-lists of resources that will be handed to the miniports.
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
    // Assign resources to individual miniports.
    //
    NTSTATUS ntStatus = AssignResources( ResourceList,
                                         &resourceListTopology,
                                         &resourceListWave,
                                         &resourceListWaveTable,
                                         &resourceListFmSynth,
                                         &resourceListUart,
                                         &resourceListAdapter );

    if(NT_SUCCESS(ntStatus))
    {
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
                     // Retrieve this adapter's PDO from its PortCls extension.
                    PDEVICE_OBJECT pdo = ADAPTER_PDO_SLOT(DeviceObject);
                    if (pdo != NULL)
                    {
                        ntStatus = pAdapterCommon->Init( resourceListAdapter,
                                                         DeviceObject, pdo );
                    }
                    else
                    {
                        ntStatus = STATUS_NO_SUCH_DEVICE;
                    }
                    if (NT_SUCCESS(ntStatus))
                    {
                        // register with PortCls for power-management services
                        ntStatus = PcRegisterAdapterPowerManagement( (PUNKNOWN)pAdapterCommon,
                                                                     DeviceObject );
                    }
                }

                pUnknownCommon->Release();
            }

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
                                             CLSID_PortTopology,
                                             CreateMiniportTopologyHDA,
                                             pAdapterCommon,
                                             resourceListTopology,
                                             GUID_NULL,
                                             NULL,
                                             &unknownTopology );
            }

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
                                             CLSID_PortWaveCyclic,
                                             CreateMiniportWaveCyclicHDA,
                                             pAdapterCommon,
                                             resourceListWave,
                                             IID_IPortWaveCyclic,
                                             pAdapterCommon->WavePortDriverDest(),
                                             &unknownWave );
            }

            resourceListWave->Release();
        }

        if (resourceListWaveTable)
        {
            resourceListWaveTable->Release();
        }

        if (resourceListFmSynth)
        {
            if (NT_SUCCESS(ntStatus))
            {
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

            resourceListFmSynth->Release();
        }

        if (resourceListUart)
        {
            if (NT_SUCCESS(ntStatus))
            {
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
                                  NULL );
            }

            resourceListUart->Release();
        }

        if (unknownTopology)
        {
            if (unknownWave)
            {
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
                PcRegisterPhysicalConnection( (PDEVICE_OBJECT)DeviceObject,
                                            unknownFmSynth,
                                            1,
                                            unknownTopology,
                                            1 );
            }
        }

        if (pAdapterCommon)
        {
            pAdapterCommon->Release();
        }

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
 *****************************************************************************/
NTSTATUS
AssignResources
(
    IN      PRESOURCELIST   ResourceList,
    OUT     PRESOURCELIST * ResourceListTopology,
    OUT     PRESOURCELIST * ResourceListWave,
    OUT     PRESOURCELIST * ResourceListWaveTable,
    OUT     PRESOURCELIST * ResourceListFmSynth,
    OUT     PRESOURCELIST * ResourceListUart,
    OUT     PRESOURCELIST * ResourceListAdapter
)
{
    BOOLEAN     detectedWaveTable   = FALSE;
    BOOLEAN     detectedUart        = FALSE;
    BOOLEAN     detectedFmSynth     = FALSE;

    ULONG countIO  = ResourceList->NumberOfPorts();
    ULONG countIRQ = ResourceList->NumberOfInterrupts();
    ULONG countDMA = ResourceList->NumberOfDmas();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    switch (countIO)
    {
    case 0:
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
        break;
    default:
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
        break;
    }

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

            SUCCEEDS((*ResourceListWave)->
                AddMemoryFromParent(ResourceList,0));

            for (i = 0; i < countDMA; i++)
            {
                SUCCEEDS((*ResourceListWave)->
                    AddDmaFromParent(ResourceList,i));
            }

            for (i = 0; i < countIRQ; i++)
            {
                SUCCEEDS((*ResourceListWave)->
                    AddInterruptFromParent(ResourceList,i));
            }
        }
    }

    *ResourceListWaveTable = NULL;
    if (NT_SUCCESS(ntStatus) && detectedWaveTable)
    {
        // TODO: Assign wave table resources.
    }

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
            SUCCEEDS((*ResourceListAdapter)->
                AddInterruptFromParent(ResourceList,0));

            SUCCEEDS((*ResourceListAdapter)->
                AddMemoryFromParent(ResourceList,0));

            if (detectedUart)
            {
                SUCCEEDS((*ResourceListAdapter)->
                    AddMemoryFromParent(ResourceList,1));
            }
        }
    }

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
 *****************************************************************************/
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

    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

    if( pIrpStack->MinorFunction == IRP_MN_FILTER_RESOURCE_REQUIREMENTS )
    {
        _DbgPrintF(DEBUGLVL_VERBOSE,("[AdapterDispatchPnp] - IRP_MN_FILTER_RESOURCE_REQUIREMENTS"));
        pIrp->IoStatus.Status = ntStatus;
    }

    ntStatus = PcDispatchIrp( pDeviceObject, pIrp );

    return ntStatus;
}

#endif

#pragma code_seg()

/*****************************************************************************
 * _purecall()
 *****************************************************************************/
int __cdecl
_purecall( void )
{
    ASSERT( !"Pure virtual function called" );
    return 0;
}