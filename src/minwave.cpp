/*****************************************************************************
 * miniport.cpp - HDA wave miniport implementation
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation. (Later released under MIT License)
 * Copyright 2025-2026 Drew Hoffman (Released under MIT License)
 */

#include "minwave.h"
#include "mydma.h"

#define STR_MODULENAME "HDAwave: "



#pragma code_seg("PAGE")

/*****************************************************************************
 * CreateMiniportWaveCyclicHDA()
 *****************************************************************************
 * Creates a cyclic wave miniport object for the HDA adapter.  This uses a
 * macro from STDUNK.H to do all the work.
 */
NTSTATUS
CreateMiniportWaveCyclicHDA
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
)
{
    PAGED_CODE();

    ASSERT(Unknown);

    STD_CREATE_BODY(CMiniportWaveCyclicHDA,Unknown,UnknownOuter,PoolType);
}

/*****************************************************************************
 * MapUsingTable()
 *****************************************************************************
 * Performs a table-based mapping, returning the table index of the indicated
 * value.  -1 is returned if the value is not found.
 */
int
MapUsingTable
(
    IN      ULONG   Value,
    IN      PULONG  Map,
    IN      ULONG   MapSize
)
{
    PAGED_CODE();

    ASSERT(Map);

    for (int result = 0; result < int(MapSize); result++)
    {
        if (*Map++ == Value)
        {
            return result;
        }
    }

    return -1;
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::ProcessResources()
 *****************************************************************************
 * Processes the resource list, setting up helper objects accordingly.
 */
NTSTATUS
CMiniportWaveCyclicHDA::
ProcessResources
(
    IN      PRESOURCELIST   ResourceList
)
{
    PAGED_CODE();

    ASSERT(ResourceList);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::ProcessResources]"));

    //
    // Get counts for the types of resources.
    //
    ULONG   countMemory = ResourceList->NumberOfMemories();
    ULONG   countIRQ    = ResourceList->NumberOfInterrupts();

	//
    // Make sure we have the expected number of resources.
	// 
	NTSTATUS ntStatus = STATUS_SUCCESS;

	if  ( (countMemory < 1) || (countIRQ < 1) ) {
        _DbgPrintF(DEBUGLVL_TERSE,("Expected device resources not found!"));
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

	//Create the two DMA channel objects
	ntStatus = CreateDmaChannel(ResourceList, FALSE);
	
	if(!NT_SUCCESS(ntStatus)){
		return ntStatus;
	}

	ntStatus = CreateDmaChannel(ResourceList, TRUE);
	
    return ntStatus;
}

NTSTATUS CMiniportWaveCyclicHDA::CreateDmaChannel (
	IN      PRESOURCELIST   ResourceList,
	IN		BOOLEAN			isCapture
)
{
	PAGED_CODE();
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PDMACHANNEL DmaChannel;

	//
    // Create the DMA Channel object.
    //
	PDMACHANNEL	RealChannel; 
    
	ntStatus = Port->NewMasterDmaChannel (&RealChannel,      // OUT DmaChannel
                                          NULL,             // OuterUnknown (opt)
                                          ResourceList,      // ResourceList (opt)
										  MAXLEN_DMA_BUFFER,// MaxLength
                                          TRUE,             // Dma32BitAddresses
                                          FALSE,            // Dma64BitAddresses
                                          MaximumDmaWidth,  // DmaWidth
                                          MaximumDmaSpeed   // DmaSpeed
                                          );

	//PcNewDmaChannel is missing from Win2K DDK so i can't use my own DeviceDescription

    if (!NT_SUCCESS (ntStatus)) {
        DOUT (DBG_ERROR, ("Failed on NewMasterDmaChannel!"));
        return ntStatus;
    }

    //
    // Get the DMA adapter object.
    //
    AdapterObject = RealChannel->GetAdapterObject ();

	//now immediately wrap that IDmaChannel in a wrapper class
	if (NT_SUCCESS(ntStatus)) {
		DmaChannel = new (NonPagedPool, 'myDA') CMyDmaChannel(RealChannel);
		RealChannel->Release(); // Our wrapper holds its own AddRef
    
		if (!DmaChannel) ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	}

	//
    // Allocate the buffer. start MUST be aligned to 128 bytes
	// this may fail or return a smaller buffer than requested
    //

    if (NT_SUCCESS(ntStatus)) {
        ULONG  lDMABufferLength = MAXLEN_DMA_BUFFER;
            
        do {
			ntStatus = DmaChannel->AllocateBuffer(lDMABufferLength,NULL);
			lDMABufferLength -= PAGE_SIZE;
        } while (!NT_SUCCESS(ntStatus) && (lDMABufferLength > (PAGE_SIZE)));

		DOUT (DBG_SYSINFO, ("Allocated DMA buffer of size %d", DmaChannel->AllocatedBufferSize() ));
		DOUT (DBG_SYSINFO, ("Physical address %X", DmaChannel->PhysicalAddress().LowPart ));
		
		//require 128 byte alignment
		if ( (DmaChannel->PhysicalAddress().LowPart & 0x7F) != 0) {
			DOUT (DBG_ERROR, ("DMA Buffer not properly aligned!" ));
			ntStatus = STATUS_BUFFER_TOO_SMALL;
		}
    }
	if (NT_SUCCESS(ntStatus)) {
		PVOID pSystemAddress = DmaChannel->SystemAddress();
		ULONG bufferSize = DmaChannel->AllocatedBufferSize();

	} else {
		//
		// Release instantiated objects in case of failure.
		//
		_DbgPrintF(DEBUGLVL_TERSE,("NewMasterDmaChannel Failure %X", ntStatus ));
		if (DmaChannel)
        {
            DmaChannel->Release();
            DmaChannel = NULL;
        }
	}

	//assign to the correct places in the object
	
	if(isCapture) {
		DmaChannelCapture = DmaChannel;
	} else {
		DmaChannelRender = DmaChannel;
	}

	return ntStatus;
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::ValidateFormat()
 *****************************************************************************
 * Validates a wave format.
 */
NTSTATUS
CMiniportWaveCyclicHDA::
ValidateFormat
(
    IN      PKSDATAFORMAT   Format
)
{
    PAGED_CODE();

    ASSERT(Format);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::ValidateFormat]"));

    NTSTATUS ntStatus;

    //
    // A WAVEFORMATEX structure should appear after the generic KSDATAFORMAT
    // if the GUIDs turn out as we expect.
    //
    PWAVEFORMATEX waveFormat = PWAVEFORMATEX(Format + 1);

    //
    // KSDATAFORMAT contains three GUIDs to support extensible format.  The
    // first two GUIDs identify the type of data.  The third indicates the
    // type of specifier used to indicate format specifics.  We are only
    // supporting PCM audio formats that use WAVEFORMATEX.
    //
	// Limiting this to 8000-48000 because Windows's kernel mixer will
	// try to resample anything lower to the highest supported multiple
	// native 8-11khz if even supported by the codec causes problems with buffering
	//
	// 8 and 16-bit formats only supported for now. TODO: add other bit depths

    if  (   (Format->FormatSize >= sizeof(KSDATAFORMAT_WAVEFORMATEX))
        &&  IsEqualGUIDAligned(Format->MajorFormat,KSDATAFORMAT_TYPE_AUDIO)
        &&  IsEqualGUIDAligned(Format->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)
        &&  IsEqualGUIDAligned
            (
                Format->Specifier,
                KSDATAFORMAT_SPECIFIER_WAVEFORMATEX
            )
        &&  (waveFormat->wFormatTag == WAVE_FORMAT_PCM)
        &&  (   (waveFormat->wBitsPerSample == 8) ||
				(waveFormat->wBitsPerSample == 16)
            )
        &&  (   (waveFormat->nChannels == 1) ||
				(waveFormat->nChannels == 2)
				
            )
        &&  (   (waveFormat->nSamplesPerSec >= 8000)
            &&  (waveFormat->nSamplesPerSec <= 48000)
            )
        )
    {
        ntStatus = STATUS_SUCCESS;

		if (AdapterCommon->hda_is_supported_sample_rate(waveFormat->nSamplesPerSec)) {
			ntStatus = STATUS_SUCCESS;
		} else {
			ntStatus = STATUS_UNSUCCESSFUL;
		}

		//do not actually try to set the sample rate now.
    }
    else
    {
        ntStatus = STATUS_INVALID_PARAMETER;
    }

    return ntStatus;
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
STDMETHODIMP
CMiniportWaveCyclicHDA::
NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::NonDelegatingQueryInterface]"));

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniportWaveCyclic))
    {
        *Object = PVOID(PMINIPORTWAVECYCLIC(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        //
        // We reference the interface for the caller.
        //
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::~CMiniportWaveCyclicHDA()
 *****************************************************************************
 * Destructor.
 */
CMiniportWaveCyclicHDA::
~CMiniportWaveCyclicHDA
(   void
)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::~CMiniportWaveCyclicHDA]"));

    if (Port)
    {
        Port->Release();
    }
	if (DmaChannelCapture)
    {
        DmaChannelCapture->Release();
    }

	if (DmaChannelRender)
    {
        DmaChannelRender->Release();
    }

    if (ServiceGroup)
    {
        ServiceGroup->Release();
    }
    if (AdapterCommon)
    {
        AdapterCommon->Release();
    }
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::Init()
 *****************************************************************************
 * Initializes a the miniport.
 */
STDMETHODIMP
CMiniportWaveCyclicHDA::
Init
(
    IN      PUNKNOWN        UnknownAdapter,
    IN      PRESOURCELIST   ResourceList,
    IN      PPORTWAVECYCLIC Port_
)
{
    PAGED_CODE();

    ASSERT(UnknownAdapter);
    ASSERT(ResourceList);
    ASSERT(Port_);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::init]"));

    //
    // AddRef() is required because we are keeping this pointer.
    //
    Port = Port_;
    Port->AddRef();

    //
    // We want the IAdapterCommon interface on the adapter common object,
    // which is given to us as a IUnknown.  The QueryInterface call gives us
    // an AddRefed pointer to the interface we want.
    //
    NTSTATUS ntStatus =
        UnknownAdapter->QueryInterface
        (
            IID_IAdapterCommon,
            (PVOID *) &AdapterCommon
        );

    //
    // We need a service group for notifications.  We will bind all the
    // streams that are created to this single service group.  All interrupt
    // notifications ask for service on this group, so all streams will get
    // serviced.  The PcNewServiceGroup() call returns an AddRefed pointer.
    // The adapter needs a copy of the service group since it is doing the
    // ISR.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = PcNewServiceGroup(&ServiceGroup,NULL);
        if (NT_SUCCESS(ntStatus))
        {
            AdapterCommon->SetWaveServiceGroup(ServiceGroup);
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ProcessResources(ResourceList);
    }

    if( !NT_SUCCESS(ntStatus) )
    {
        //
        // clean up our mess
        //

        // clean up AdapterCommon
        if( AdapterCommon )
        {
            // clean up the service group
            if( ServiceGroup )
            {
                AdapterCommon->SetWaveServiceGroup(NULL);
                ServiceGroup->Release();
                ServiceGroup = NULL;
            }

            AdapterCommon->Release();
            AdapterCommon = NULL;
        }

        // release the port
        Port->Release();
        Port = NULL;
    }

    return ntStatus;
}

/*****************************************************************************
 * PinDataRangesStream
 *****************************************************************************
 * Structures indicating range of valid format values for streaming pins.
 */
static
KSDATARANGE_AUDIO PinDataRangesStream[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        2,      // Max number of channels.
        16,      // Minimum number of bits per sample.
        16,     // Maximum number of bits per channel.
        8000,   // Minimum rate. 
        48000   // Maximum rate.
    }
};

/*****************************************************************************
 * PinDataRangePointersStream
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for streaming pins.
 */
static
PKSDATARANGE PinDataRangePointersStream[] =
{
    PKSDATARANGE(&PinDataRangesStream[0])
};

/*****************************************************************************
 * PinDataRangesBridge
 *****************************************************************************
 * Structures indicating range of valid format values for bridge pins.
 */
static
KSDATARANGE PinDataRangesBridge[] =
{
   {
      sizeof(KSDATARANGE),
      0,
      0,
      0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
   }
};

/*****************************************************************************
 * PinDataRangePointersBridge
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for bridge pins.
 */
static
PKSDATARANGE PinDataRangePointersBridge[] =
{
    &PinDataRangesBridge[0]
};

/*****************************************************************************
 * MiniportPins
 *****************************************************************************
 * List of pins.
 */
static
PCPIN_DESCRIPTOR 
MiniportPins[] =
{
    // Wave In Streaming Pin (Capture)
    {
        1,1,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(PinDataRangePointersStream),
            PinDataRangePointersStream,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            (GUID *) &PINNAME_CAPTURE,
            &KSAUDFNAME_RECORDING_CONTROL,  // this name shows up as the recording panel name in SoundVol.
            0
        }
    },
    // Wave In Bridge Pin (Capture - From Topology)
    {
        0,0,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(PinDataRangePointersBridge),
            PinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            (GUID *) &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Streaming Pin (Renderer)
    {
        1,1,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(PinDataRangePointersStream),
            PinDataRangePointersStream,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            (GUID *) &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Bridge Pin (Renderer)
    {
        0,0,0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(PinDataRangePointersBridge),
            PinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            (GUID *) &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    }
};

/*****************************************************************************
 * TopologyNodes
 *****************************************************************************
 * List of nodes.
 */
static
PCNODE_DESCRIPTOR MiniportNodes[] =
{
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    },
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_DAC,        // Type
        NULL                    // Name
    }
};

/*****************************************************************************
 * MiniportConnections
 *****************************************************************************
 * List of connections.
 */
static
PCCONNECTION_DESCRIPTOR MiniportConnections[] =
{
    { PCFILTER_NODE,  1,  0,                1 },    // Bridge in to ADC.
    { 0,              0,  PCFILTER_NODE,    0 },    // ADC to stream pin (capture).
    { PCFILTER_NODE,  2,  1,                1 },    // Stream in to DAC.
    { 1,              0,  PCFILTER_NODE,    3 }     // DAC to Bridge.
};

/*****************************************************************************
 * MiniportFilterDescriptor
 *****************************************************************************
 * Complete miniport description.
 */
static
PCFILTER_DESCRIPTOR 
MiniportFilterDescriptor =
{
    0,                                  // Version
    NULL,                               // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),           // PinSize
    SIZEOF_ARRAY(MiniportPins),         // PinCount
    MiniportPins,                       // Pins
    sizeof(PCNODE_DESCRIPTOR),          // NodeSize
    SIZEOF_ARRAY(MiniportNodes),        // NodeCount
    MiniportNodes,                      // Nodes
    SIZEOF_ARRAY(MiniportConnections),  // ConnectionCount
    MiniportConnections,                // Connections
    0,                                  // CategoryCount
    NULL                                // Categories
};

/*****************************************************************************
 * CMiniportWaveCyclicHDA::GetDescription()
 *****************************************************************************
 * Gets the topology.
 */
STDMETHODIMP
CMiniportWaveCyclicHDA::
GetDescription
(
    OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::GetDescription]"));

    *OutFilterDescriptor = &MiniportFilterDescriptor;

    return STATUS_SUCCESS;
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::DataRangeIntersection()
 *****************************************************************************
 * Tests a data range intersection.
 */
STDMETHODIMP 
CMiniportWaveCyclicHDA::
DataRangeIntersection
(   
    IN      ULONG           PinId,
    IN      PKSDATARANGE    MatchingDataRange,
    IN      PKSDATARANGE    DataRange,
    IN      ULONG           OutputBufferLength,
    OUT     PVOID           ResultantFormat,
    OUT     PULONG          ResultantFormatLength
)
{
    BOOLEAN                         DigitalAudio;
    NTSTATUS                        Status;
    ULONG                           RequiredSize;
    ULONG                           SampleFrequency;
    USHORT                          BitsPerSample;
    
    //
    // Let's do the complete work here.
    //
    if (!IsEqualGUIDAligned( 
            MatchingDataRange->Specifier, 
            KSDATAFORMAT_SPECIFIER_NONE )) {
            
        //
        // The miniport did not resolve this format.  If the dataformat
        // is not PCM audio and requires a specifier, bail out.
        //
        if (!IsEqualGUIDAligned( 
                MatchingDataRange->MajorFormat, KSDATAFORMAT_TYPE_AUDIO ) ||
            !IsEqualGUIDAligned(     
               MatchingDataRange->SubFormat, KSDATAFORMAT_SUBTYPE_PCM )) {
            return STATUS_INVALID_PARAMETER;
        }
        DigitalAudio = TRUE;
        
        //
        // wired enough, the specifier here does not define the format of MatchingDataRange
        // but the format that is expected to be returned in ResultantFormat.
        //
        if (IsEqualGUIDAligned( 
                MatchingDataRange->Specifier, 
                KSDATAFORMAT_SPECIFIER_DSOUND )) {
            RequiredSize = sizeof(KSDATAFORMAT_DSOUND);
        } else {
            RequiredSize = sizeof( KSDATAFORMAT_WAVEFORMATEX );
        }            
    } else {
        DigitalAudio = FALSE;
        RequiredSize = sizeof( KSDATAFORMAT );
    }
            
    //
    // Validate return buffer size, if the request is only for the
    // size of the resultant structure, return it now.
    //
    if (!OutputBufferLength) {
        *ResultantFormatLength = RequiredSize;
        return STATUS_BUFFER_OVERFLOW;
    } else if (OutputBufferLength < RequiredSize) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    // There was a specifier ...
    if (DigitalAudio) {     
        PKSDATARANGE_AUDIO  AudioRange;
        PWAVEFORMATEX       WaveFormatEx;
        
        AudioRange = (PKSDATARANGE_AUDIO) DataRange;
        
        // Fill the structure
        if (IsEqualGUIDAligned( 
                MatchingDataRange->Specifier, 
                KSDATAFORMAT_SPECIFIER_DSOUND )) {
            PKSDATAFORMAT_DSOUND    DSoundFormat;
            
            DSoundFormat = (PKSDATAFORMAT_DSOUND) ResultantFormat;
            
            _DbgPrintF( 
                DEBUGLVL_VERBOSE, 
                ("returning KSDATAFORMAT_DSOUND format intersection") );
            
            DSoundFormat->BufferDesc.Flags = 0 ;
            DSoundFormat->BufferDesc.Control = 0 ;
            DSoundFormat->DataFormat = *MatchingDataRange;
            DSoundFormat->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_DSOUND;
            DSoundFormat->DataFormat.FormatSize = RequiredSize;
            WaveFormatEx = &DSoundFormat->BufferDesc.WaveFormatEx;
            *ResultantFormatLength = RequiredSize;
        } else {
            PKSDATAFORMAT_WAVEFORMATEX  WaveFormat;
        
            WaveFormat = (PKSDATAFORMAT_WAVEFORMATEX) ResultantFormat;
            
            _DbgPrintF( 
                DEBUGLVL_VERBOSE, 
                ("returning KSDATAFORMAT_WAVEFORMATEX format intersection") );
        
            WaveFormat->DataFormat = *MatchingDataRange;
            WaveFormat->DataFormat.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
            WaveFormat->DataFormat.FormatSize = RequiredSize;
            WaveFormatEx = &WaveFormat->WaveFormatEx;
            *ResultantFormatLength = RequiredSize;
        }
        
        //
        // Return a format that intersects the given audio range, 
        // using our maximum support as the "best" format.
        // 
        
        WaveFormatEx->wFormatTag = WAVE_FORMAT_PCM;
        WaveFormatEx->nChannels = 
            (USHORT) min( AudioRange->MaximumChannels, 
                          ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumChannels );
        
        //
        // Check if the pin is still free
        //
        if (!PinId)
        {
            if (AllocatedCapture)
            {
                return STATUS_NO_MATCH;
            }
        }
        else
        {
            if (AllocatedRender)
            {
                return STATUS_NO_MATCH;
            }
        }
		
		// removed - HDA is full duplex
		
        SampleFrequency = min( AudioRange->MaximumSampleFrequency,
                 ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumSampleFrequency );



        WaveFormatEx->nSamplesPerSec = SampleFrequency;
		
		// removed - HDA doesn't need to budget DMA channels

        BitsPerSample = (USHORT) min( AudioRange->MaximumBitsPerSample,
                          ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumBitsPerSample );


        WaveFormatEx->wBitsPerSample = BitsPerSample;
        
        WaveFormatEx->nBlockAlign = 
            (WaveFormatEx->wBitsPerSample * WaveFormatEx->nChannels) / 8;
        WaveFormatEx->nAvgBytesPerSec = 
            (WaveFormatEx->nSamplesPerSec * WaveFormatEx->nBlockAlign);
        WaveFormatEx->cbSize = 0;
        ((PKSDATAFORMAT) ResultantFormat)->SampleSize = 
            WaveFormatEx->nBlockAlign;
        
        _DbgPrintF( 
            DEBUGLVL_VERBOSE, 
            ("Channels = %d", WaveFormatEx->nChannels) );
        _DbgPrintF( 
            DEBUGLVL_VERBOSE, 
            ("Samples/sec = %d", WaveFormatEx->nSamplesPerSec) );
        _DbgPrintF( 
            DEBUGLVL_VERBOSE, 
            ("Bits/sample = %d", WaveFormatEx->wBitsPerSample) );
        
    } else {    // There was no specifier. Return only the KSDATAFORMAT structure.
        //
        // Copy the data format structure.
        //
        _DbgPrintF( 
            DEBUGLVL_VERBOSE, 
            ("returning default format intersection") );
            
        RtlCopyMemory( 
            ResultantFormat, MatchingDataRange, sizeof( KSDATAFORMAT ) );
        *ResultantFormatLength = sizeof( KSDATAFORMAT );
    } 
    
    return STATUS_SUCCESS;
}

/*****************************************************************************
 * CMiniportWaveCyclicHDA::NewStream()
 *****************************************************************************
 * Creates a new stream.  This function is called when a streaming pin is
 * created.
 */
STDMETHODIMP
CMiniportWaveCyclicHDA::
NewStream
(
    OUT     PMINIPORTWAVECYCLICSTREAM * OutStream,
    IN      PUNKNOWN                    OuterUnknown,
    IN      POOL_TYPE                   PoolType,
    IN      ULONG                       Channel,
    IN      BOOLEAN                     isCapture,
    IN      PKSDATAFORMAT               DataFormat,
    OUT     PDMACHANNEL *               OutDmaChannel,
    OUT     PSERVICEGROUP *             OutServiceGroup
)
{
    PAGED_CODE();

    ASSERT(OutStream);
    ASSERT(DataFormat);
    ASSERT(OutDmaChannel);
    ASSERT(OutServiceGroup);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicHDA::NewStream]"));

    NTSTATUS ntStatus = STATUS_SUCCESS;
	PDMACHANNEL dmaChannel = NULL;
	PWAVEFORMATEX       waveFormat = PWAVEFORMATEX(DataFormat + 1);

    //
    // Make sure the hardware is not already in use.
	// Get the required DMA channel if it's not already in use.
    //
    if (isCapture) {
        if (AllocatedCapture){
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        } else {
			dmaChannel = DmaChannelCapture;
		}
    } else {
        if (AllocatedRender) {
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        } else {
			dmaChannel = DmaChannelRender;
		}
    }

	if (!NT_SUCCESS(ntStatus)){
		return ntStatus;
	}

    //
    // Determine if the data format is valid.
    //

    ntStatus = ValidateFormat(DataFormat);

	if (!NT_SUCCESS(ntStatus)){
		_DbgPrintF( DEBUGLVL_VERBOSE, 
            ("Data format is invalid, stream not created") );
		return ntStatus;
	}
    	
    if (! dmaChannel) {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    } else {
        //
        // Instantiate a stream.
        //
        CMiniportWaveCyclicStreamHDA *stream =
            new(PoolType) CMiniportWaveCyclicStreamHDA(OuterUnknown);

        if (stream)
        {
            stream->AddRef();

            ntStatus =
                stream->Init
                (
                    this,
                    Channel,
                    isCapture,
                    DataFormat,
                    dmaChannel
                );

            if (NT_SUCCESS(ntStatus))
            {
                if (isCapture)
                {
                    AllocatedCapture = TRUE;
                }
                else
                {
                    AllocatedRender = TRUE;
                }

                *OutStream = PMINIPORTWAVECYCLICSTREAM(stream);
                stream->AddRef();

                *OutDmaChannel = dmaChannel;
                dmaChannel->AddRef();

                *OutServiceGroup = ServiceGroup;
                ServiceGroup->AddRef();

                //
                // The stream, the DMA channel, and the service group have
                // references now for the caller.  The caller expects these
                // references to be there.
                //
            }

            //
            // This is our private reference to the stream.  The caller has
            // its own, so we can release in any case.
            //
            stream->Release();
        }
        else
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
STDMETHODIMP
CMiniportWaveCyclicStreamHDA::
NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::NonDelegatingQueryInterface]"));

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniportWaveCyclicStream))
    {
        *Object = PVOID(PMINIPORTWAVECYCLICSTREAM(this));
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
 * CMiniportWaveCyclicStreamHDA::~CMiniportWaveCyclicStreamHDA()
 *****************************************************************************
 * Destructor.
 */
CMiniportWaveCyclicStreamHDA::
~CMiniportWaveCyclicStreamHDA
(   void
)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::~CMiniportWaveCyclicStreamHDA]"));

    if (DmaChannel)
    {
        DmaChannel->Release();
    }

    if (Miniport)
    {
        //
        // Clear allocation flags in the miniport.
        //
        if (isCapture)
        {
            Miniport->AllocatedCapture = FALSE;
        }
        else
        {
            Miniport->AllocatedRender = FALSE;
        }
		
		//stop hw streams before destruction
		Miniport->AdapterCommon->hda_stop_stream(TRUE);
		Miniport->AdapterCommon->hda_stop_stream(FALSE);

        Miniport->AdapterCommon->SaveMixerSettingsToRegistry();
        Miniport->Release();
    }
}

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::Init()
 *****************************************************************************
 * Initializes a stream.
 */
NTSTATUS
CMiniportWaveCyclicStreamHDA::
Init
(
    IN      CMiniportWaveCyclicHDA *   Miniport_,
    IN      ULONG                       Channel_,
    IN      BOOLEAN                     Capture_,
    IN      PKSDATAFORMAT               DataFormat,
    IN      PDMACHANNEL		            DmaChannel_
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
    PAGED_CODE();
	
	_DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::Init]"));

	//TODO: Temporary block on creating capture streams
	
	if(Capture_){
		_DbgPrintF(DEBUGLVL_TERSE,("Capture stream unsupported"));
		return STATUS_UNSUCCESSFUL;
	}
	

    ASSERT(Miniport_);
    ASSERT(DataFormat);
	//is this assert valid? crashes on trying to capture in Sound Recorder
	//as well as on virtualbox.
    //ASSERT(NT_SUCCESS(Miniport_->ValidateFormat(DataFormat)));
    ASSERT(DmaChannel_);

    PWAVEFORMATEX waveFormat = PWAVEFORMATEX(DataFormat + 1);

    //
    // We must add references because the caller will not do it for us.
    //
    Miniport = Miniport_;
    Miniport->AddRef();

    DmaChannel = DmaChannel_;
    DmaChannel->AddRef();

    Channel         = Channel_;
    isCapture         = Capture_;
	
	FormatSampleRate	= waveFormat->nSamplesPerSec;
    FormatChannels		= waveFormat->nChannels;
    FormatBitDepth		= waveFormat->wBitsPerSample;
    State				= KSSTATE_STOP;
	
	StreamDescriptorValid = FALSE;
    RestoreInputMixer = FALSE;


    FormatSampleRate = waveFormat->nSamplesPerSec;
	
	/*
	if(NT_SUCCESS(ntStatus)){
			//if everything is ok, set up the stream
			ntStatus = Miniport->AdapterCommon->hda_setup_stream_descriptor(DmaChannel, isCapture);
			if (NT_SUCCESS(ntStatus)) {
				StreamDescriptorValid = TRUE;
			}
	}
    */

    SetFormat( DataFormat );

    return ntStatus;
}

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::SetNotificationFreq()
 *****************************************************************************
 * Sets the notification frequency. 
 * the port driver calls this. will only ask for ~10 ms interrupts
 */
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamHDA::
SetNotificationFreq
(
    IN      ULONG   Interval,
    OUT     PULONG  FramingSize    
)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::SetNotificationFreq]"));

    Miniport->NotificationInterval = Interval;

	ULONG bytes_per_bit_depth = ((FormatBitDepth <= 8) ? 1 
		: (FormatBitDepth <= 16) ? 2 
		: (FormatBitDepth <= 32) ? 4 
		: 8
		);

	ULONG target_chunk_bytes = FormatChannels * bytes_per_bit_depth
            * FormatSampleRate * Interval / 1000;
	DOUT(DBG_ERROR, ("target chunk bytes %d", target_chunk_bytes));

	// Align chunk size to 256 bytes AND integer frame boundaries
    //    First align down to 256 bytes
    ULONG aligned_chunk_bytes = target_chunk_bytes & ~(255);
	DOUT(DBG_ERROR, ("aligned chunk bytes %d", aligned_chunk_bytes));

    //    Ensure aligned_chunk_bytes is an exact multiple of PCM frame size
    //if (aligned_chunk_bytes % bytes_per_frame != 0) {
    //    aligned_chunk_bytes -= (aligned_chunk_bytes % bytes_per_frame);
    //}

    // Fallback safeguard: if 10ms is tiny, enforce at least 256 bytes
    if (aligned_chunk_bytes < 256) {
        aligned_chunk_bytes = 256;
        // Align up to frame boundary if necessary
        //if (aligned_chunk_bytes % bytes_per_frame != 0) {
        //    aligned_chunk_bytes += (bytes_per_frame - (aligned_chunk_bytes % bytes_per_frame));
        //}
    }

	//write aligned frame size to out pointer
    *FramingSize = aligned_chunk_bytes;

	//we could recalculate the actual interval in terms of our aligned chunk sizes but it is not used
    return Miniport->NotificationInterval;
}

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::SetFormat()
 *****************************************************************************
 * Sets the wave format.
 */
STDMETHODIMP
CMiniportWaveCyclicStreamHDA::
SetFormat
(
    IN      PKSDATAFORMAT   Format
)
{
    PAGED_CODE();

    ASSERT(Format);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::SetFormat]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if(State != KSSTATE_RUN)
    {
        ntStatus = Miniport->ValidateFormat(Format);
    
        PWAVEFORMATEX waveFormat = PWAVEFORMATEX(Format + 1);

		//Removed - unnecessary
        // check for full-duplex sample rate sync
    
        // TODO:  Validate sample size.
    
        if (NT_SUCCESS(ntStatus))
        {
            PWAVEFORMATEX waveFormat = PWAVEFORMATEX(Format + 1);

            FormatSampleRate = waveFormat->nSamplesPerSec;   

            _DbgPrintF(DEBUGLVL_VERBOSE,("  SampleRate: %d",waveFormat->nSamplesPerSec));
			
			//don't actually set the sample rate on the controller, this is done right when going into Run
			//Miniport->AdapterCommon->ProgramDataFormat(FormatSampleRate, waveFormat->nChannels, waveFormat->wBitsPerSample, isCapture);
			
			FormatDirty = TRUE;
			StreamDescriptorValid = FALSE;
        }

    }

    return ntStatus;
}

#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::GetPosition()
 *****************************************************************************
 * Gets the current stream position from the hardware.
 */
STDMETHODIMP
CMiniportWaveCyclicStreamHDA::
GetPosition
(
    OUT     PULONG  Position
)
{
    // Not PAGED_CODE().  May be called at dispatch level.

	ASSERT(Position);

    if (DmaChannel)
    {
		if (State == KSSTATE_RUN){
			//bias the stream position forward when in Run mode
			//to account for the DMA engine block size and the codec's buffer.
			//or don't? may be causing a wraparound problem starting short sounds in the last ~4k of the buffer
			*Position = (Miniport->AdapterCommon->hda_get_actual_stream_position( isCapture ) //+ 128 + 16 
				)
				% DmaChannel->BufferSize()
				; 
		}
		else {
			*Position = Miniport->AdapterCommon->hda_get_actual_stream_position( isCapture )
				% DmaChannel->BufferSize()
				;
		}
    }
    else
    {
        *Position = 0;
    }

   return STATUS_SUCCESS;
}

STDMETHODIMP
CMiniportWaveCyclicStreamHDA::NormalizePhysicalPosition(
    IN OUT PLONGLONG PhysicalPosition
)

/*++

Routine Description:
    Given a physical position based on the actual number of bytes transferred,
    this function converts the position to a time-based value of 100ns units.

Arguments:
    IN OUT PLONGLONG PhysicalPosition -
        value to convert.

Return:
    STATUS_SUCCESS or an appropriate error code.

--*/

{   
	_DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::NormalizePhysicalPosition] %d", PhysicalPosition));

	ULONG bytes_per_bit_depth = ((FormatBitDepth <= 8) ? 1 
		: (FormatBitDepth <= 16) ? 2 
		: (FormatBitDepth <= 32) ? 4 
		: 8
		);

    *PhysicalPosition =
            ( _100NS_UNITS_PER_SECOND / 
                (bytes_per_bit_depth * FormatChannels) * *PhysicalPosition) / 
                    FormatSampleRate;
    return STATUS_SUCCESS;
}
    
#pragma code_seg("PAGE")

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::SetState()
 *****************************************************************************
 * Sets the state of the channel
 */

STDMETHODIMP
CMiniportWaveCyclicStreamHDA::SetState (IN KSSTATE NewState){
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_TERSE,("CMiniportWaveCyclicStreamHDA[%p]::SetState(%d)", this, NewState));

    NTSTATUS ntStatus = STATUS_SUCCESS;

	if (! DmaChannel) {
		//bail out early
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (State == NewState) {
		return ntStatus;
	}

    switch (NewState) {
		
		case KSSTATE_STOP:
			// don't destroy the stream descriptor,
			// this is done anyway when the WaveCyclicStream is destroyed.
			
			// do clear out the buffer

            Silence(DmaChannel->SystemAddress(), DmaChannel->BufferSize());
            break;

		case KSSTATE_ACQUIRE:
			break;

        case KSSTATE_PAUSE:
            if (State == KSSTATE_RUN) { 
				
				//Run -> Pause

                ULONG bufferSize = DmaChannel->BufferSize();
                ULONG position = bufferSize ?
                        (Miniport->AdapterCommon->hda_get_actual_stream_position( isCapture ) % bufferSize) :
                        0;

                // If playback pauses with the hardware pointer in the
                // final 20% of the cyclic buffer, reset the stream and
                // rebuild the descriptor now.  Doing this work while
                // entering Pause keeps the later transition back to Run
                // lightweight enough for short system sounds.

                BOOLEAN recreateDescriptor = (!bufferSize ||
						(position >= (bufferSize - (bufferSize / 5))));
                
                if (recreateDescriptor) {

                    ntStatus = Miniport->AdapterCommon->hda_stop_stream(isCapture);
                    StreamDescriptorValid = FALSE;

					if (!NT_SUCCESS(ntStatus)) {
                        break;
                    }

                    ntStatus = Miniport->AdapterCommon->hda_setup_stream_descriptor(
						DmaChannel,
						FormatSampleRate, 
						FormatChannels, 
						FormatBitDepth,
						isCapture);

                    if (NT_SUCCESS(ntStatus)) {
						StreamDescriptorValid = TRUE;
                    } else {
                        break;
                    }

                } else {
                    // Stop DMA but keep the programmed descriptor when the
                    // pointer is safely away from the buffer wrap point.
                    Miniport->AdapterCommon->hda_stop_sound(isCapture);

					// do clear out the buffer
                    Silence(DmaChannel->SystemAddress(), DmaChannel->BufferSize());
                }
            } else {
				// Acquire -> Pause
			}

            break;

        case KSSTATE_RUN: // Pause -> Run
			
			//setup stream descriptor if necessary

			if (!StreamDescriptorValid) {
                ntStatus = Miniport->AdapterCommon->hda_setup_stream_descriptor(
							DmaChannel,
							FormatSampleRate, 
							FormatChannels, 
							FormatBitDepth,
							isCapture);
                if (NT_SUCCESS(ntStatus)) {				
                    StreamDescriptorValid = TRUE;
					FormatDirty = FALSE;
                } 
            }
                
			Miniport->AdapterCommon->ProgramDataFormat(
				FormatSampleRate, 
				FormatChannels, 
				FormatBitDepth, 
				isCapture);

			// Start DMA.
			Miniport->AdapterCommon->hda_start_sound(isCapture);
                      
            break;
    }

    State = NewState;  
    return ntStatus;
}


/* States always progress in in the order of:
DmaChannel created -> KSSTATE_STOP -> KSSTATE_ACQUIRE -> KSSTATE_PAUSE -> KSSTATE_RUN (Playing)
(Playing) KSSTATE_RUN -> KSSTATE_PAUSE -> KSSTATE_ACQUIRE -> KSSTATE_STOP -> DmaChannel Destroyed
*/

//the KMixer will often take the stream up to Pause and then back down again as a dry-run before playing sound

//new (assisted by Gemini)
//TODO: keep previous stream descriptor if it is valid, same sample rate etc. 

/*

STDMETHODIMP CMiniportWaveCyclicStreamHDA::SetState(IN KSSTATE NewState)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    switch (NewState)
    {
    case KSSTATE_STOP:
        // 1. Stop DMA
        ntStatus = Miniport->AdapterCommon->hda_stop_stream(isCapture);
        State = KSSTATE_STOP;
        break;

    case KSSTATE_ACQUIRE:
        // Just state tracking
        State = KSSTATE_ACQUIRE;
        break;

    case KSSTATE_PAUSE:
        if (State == KSSTATE_RUN) {
            // Transitioning from RUN -> PAUSE: Pause DMA (clear RUN bit)
            Miniport->AdapterCommon->hda_stop_sound();
        } else {
            // Transitioning from ACQUIRE -> PAUSE:
            // Format is 100% final now. Generate and program BDL cleanly.
			if (DmaChannel) {
					Miniport->AdapterCommon->ProgramDataFormat(
						FormatSampleRate, 
						FormatChannels, 
						FormatBitDepth, 
						isCapture);
			}

            // Write BDL base address, CBL, and LVI into controller registers
            ntStatus = Miniport->AdapterCommon->hda_setup_stream_descriptor(
							DmaChannel,
							FormatSampleRate, 
							FormatChannels, 
							FormatBitDepth,
							isCapture);

			if (NT_SUCCESS(ntStatus)) {				
                    StreamDescriptorValid = TRUE;					
            }
        }
        State = KSSTATE_PAUSE;
        break;

    case KSSTATE_RUN:
        // Fast start: Just enable the RUN bit
        Miniport->AdapterCommon->hda_start_sound(isCapture);
        State = KSSTATE_RUN;
        break;
    }

    return ntStatus;
}
*/

#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::Silence()
 *****************************************************************************
 * Fills a buffer with silence.
 */
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamHDA::
Silence
(
    IN      PVOID   Buffer,
    IN      ULONG   ByteCount
)
{
    RtlFillMemory(Buffer,ByteCount, (FormatBitDepth > 8) ? 0 : 0x7f);
}
