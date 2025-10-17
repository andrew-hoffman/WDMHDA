/*****************************************************************************
 * miniport.cpp - HDA wave miniport implementation
 *****************************************************************************
 * Copyright (c) Microsoft Corporation 1997-1999.  All rights reserved.
 */

#include "minwave.h"

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

    ULONG   intNumber   = ULONG(-1);
    ULONG   dma8Bit     = ULONG(-1);
    ULONG   dma16Bit    = ULONG(-1);

    //
    // Get counts for the types of resources.
    //
    ULONG   countMemory    = ResourceList->NumberOfMemories();
    ULONG   countIRQ    = ResourceList->NumberOfInterrupts();

#if (DBG)
    _DbgPrintF(DEBUGLVL_VERBOSE,("Starting HDA wave on IRQ %d",
        ResourceList->FindUntranslatedInterrupt(0)->u.Interrupt.Level) );

#endif

	//
    // Make sure we have the expected number of resources.
	// 
    //
	NTSTATUS ntStatus = STATUS_SUCCESS;

	if  (   (countMemory < 1)
        ||  (countIRQ < 1)
        )
    {
        _DbgPrintF(DEBUGLVL_TERSE,("unknown configuraton; check your code!"));
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
    }

	//
    // Create the DMA Channel object.
    //
    ntStatus = Port->NewMasterDmaChannel (&DmaChannel,      // OutDmaChannel
                                          NULL,             // OuterUnknown (opt)
                                          NULL,             // ResourceList (opt)
										  MAXLEN_DMA_BUFFER,// MaxLength
                                          TRUE,             // Dma32BitAddresses
                                          FALSE,            // Dma64BitAddresses
                                          MaximumDmaWidth,  // DmaWidth
                                          MaximumDmaSpeed   // DmaSpeed
                                          );              
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Failed on NewMasterDmaChannel!"));
        return ntStatus;
    }

    //
    // Get the DMA adapter object.
    //
    AdapterObject = DmaChannel->GetAdapterObject ();

	//
    // Allocate the buffer. start MUST be aligned to 128 bytes
	// and i think it should be as long as we allocate more than a page.
    //
    if (NT_SUCCESS(ntStatus)) {
        ULONG  lDMABufferLength = MAXLEN_DMA_BUFFER;
            
        do {
			ntStatus = DmaChannel->AllocateBuffer(lDMABufferLength,NULL);
			lDMABufferLength >>= 1;
        } while (!NT_SUCCESS(ntStatus) && (lDMABufferLength > (PAGE_SIZE)));
		DOUT (DBG_SYSINFO, ("Allocated DMA buffer of size %d", DmaChannel->AllocatedBufferSize() ));
		DOUT (DBG_SYSINFO, ("Physical address %X", DmaChannel->PhysicalAddress().LowPart ));

		ASSERT((DmaChannel->PhysicalAddress().LowPart & 0x7F) == 0); //require 128 byte alignment
    }
	if (NT_SUCCESS(ntStatus))
    {
        intNumber = ResourceList->
                    FindUntranslatedInterrupt(0)->u.Interrupt.Level;
		//give it some fake DMA channel numbers in case those are needed
		dma8Bit = 1;
		dma16Bit = 5;
		//it's showtime then

		ntStatus = AdapterCommon->hda_showtime(DmaChannel);
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
    if  (   (Format->FormatSize >= sizeof(KSDATAFORMAT_WAVEFORMATEX))
        &&  IsEqualGUIDAligned(Format->MajorFormat,KSDATAFORMAT_TYPE_AUDIO)
        &&  IsEqualGUIDAligned(Format->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)
        &&  IsEqualGUIDAligned
            (
                Format->Specifier,
                KSDATAFORMAT_SPECIFIER_WAVEFORMATEX
            )
        &&  (waveFormat->wFormatTag == WAVE_FORMAT_PCM)
        &&  (   (waveFormat->wBitsPerSample == 8)
            ||  (waveFormat->wBitsPerSample == 16)
            )
        &&  (   (waveFormat->nChannels == 2)

            )
        &&  (   (waveFormat->nSamplesPerSec >= 44100)
            &&  (waveFormat->nSamplesPerSec <= 44100)
            )
        )
    {
        ntStatus = STATUS_SUCCESS;
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
	if (DmaChannel)
    {
        DmaChannel->Release();
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
        KeInitializeMutex(&SampleRateSync,1);
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
        8,      // Minimum number of bits per sample.
        16,     // Maximum number of bits per channel.
        44100,   // Minimum rate. //TODO support more
        44100   // Maximum rate.
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

        //
        // Check if one pin is in use -> use same sample frequency.
        //
        if (AllocatedCapture || AllocatedRender)
        {
            SampleFrequency = SamplingFrequency;
            if ((SampleFrequency > ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumSampleFrequency) ||
                (SampleFrequency < ((PKSDATARANGE_AUDIO) MatchingDataRange)->MinimumSampleFrequency))
            {
                return STATUS_NO_MATCH;
            }
        }
        else
        {
            SampleFrequency = min( AudioRange->MaximumSampleFrequency,
                 ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumSampleFrequency );

        }

        WaveFormatEx->nSamplesPerSec = SampleFrequency;

        //
        // Check if one pin is in use -> use other bits per sample.
        //
        if (AllocatedCapture || AllocatedRender)
        {
            if (Allocated8Bit)
            {
                BitsPerSample = 16;
            }
            else
            {
                BitsPerSample = 8;
            }

            if ((BitsPerSample > ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumBitsPerSample) ||
                (BitsPerSample < ((PKSDATARANGE_AUDIO) MatchingDataRange)->MinimumBitsPerSample))
            {
                return STATUS_NO_MATCH;
            }
        }
        else
        {
            BitsPerSample = (USHORT) min( AudioRange->MaximumBitsPerSample,
                          ((PKSDATARANGE_AUDIO) MatchingDataRange)->MaximumBitsPerSample );
        }


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
    IN      BOOLEAN                     Capture,
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

    //
    // Make sure the hardware is not already in use.
    //
    if (Capture)
    {
        if (AllocatedCapture)
        {
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        }
    }
    else
    {
        if (AllocatedRender)
        {
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    //
    // Determine if the format is valid.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ValidateFormat(DataFormat);
    }

    if(NT_SUCCESS(ntStatus))
    {
        // if we're trying to start a full-duplex stream.
        if(AllocatedCapture || AllocatedRender)
        {
            // make sure the requested sampling rate is the
            // same as the currently running one...
            PWAVEFORMATEX waveFormat = PWAVEFORMATEX(DataFormat + 1);
            if( SamplingFrequency != waveFormat->nSamplesPerSec )
            {
                // Bad format....
                ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
    }

    PDMACHANNEL    dmaChannel = NULL;
    PWAVEFORMATEX       waveFormat = PWAVEFORMATEX(DataFormat + 1);

    //
    // Get the required DMA channel if it's not already in use.
    //
    if (NT_SUCCESS(ntStatus))
    {
        if (waveFormat->wBitsPerSample == 8)
        {
            if (! Allocated8Bit)
            {
                dmaChannel = DmaChannel;
            }
        }
        else
        {
            if (! Allocated16Bit)
            {
                dmaChannel = DmaChannel;
            }
        }
    }

    if (! dmaChannel)
    {
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }
    else
    {
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
                    Capture,
                    DataFormat,
                    dmaChannel
                );

            if (NT_SUCCESS(ntStatus))
            {
                if (Capture)
                {
                    AllocatedCapture = TRUE;
                }
                else
                {
                    AllocatedRender = TRUE;
                }

                if (waveFormat->wBitsPerSample == 8)
                {
                    Allocated8Bit = TRUE;
                }
                else
                {
                    Allocated16Bit = TRUE;
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
        if (Capture)
        {
            Miniport->AllocatedCapture = FALSE;
        }
        else
        {
            Miniport->AllocatedRender = FALSE;
        }

        if (Format16Bit)
        {
            Miniport->Allocated16Bit = FALSE;
        }
        else
        {
            Miniport->Allocated8Bit = FALSE;
        }

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
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportWaveCyclicStreamHDA::Init]"));

    ASSERT(Miniport_);
    ASSERT(DataFormat);
    ASSERT(NT_SUCCESS(Miniport_->ValidateFormat(DataFormat)));
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
    Capture         = Capture_;
    FormatStereo    = (waveFormat->nChannels == 2);
    Format16Bit     = (waveFormat->wBitsPerSample == 16);
    State           = KSSTATE_STOP;

    RestoreInputMixer = FALSE;

    KeWaitForSingleObject
    (
        &Miniport->SampleRateSync,
        Executive,
        KernelMode,
        FALSE,
        NULL
    );
    Miniport->SamplingFrequency = waveFormat->nSamplesPerSec;
    KeReleaseMutex(&Miniport->SampleRateSync,FALSE);
    
    SetFormat( DataFormat );

    return STATUS_SUCCESS;
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
    *FramingSize = 
        (1 << (FormatStereo + Format16Bit)) * 
            Miniport->SamplingFrequency * Interval / 1000;

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

        KeWaitForSingleObject
        (
            &Miniport->SampleRateSync,
            Executive,
            KernelMode,
            FALSE,
            NULL
        );
    
        // check for full-duplex stuff
        if( NT_SUCCESS(ntStatus)
            && Miniport->AllocatedCapture
            && Miniport->AllocatedRender
        )
        {
            // no new formats.... bad...
            if( Miniport->SamplingFrequency != waveFormat->nSamplesPerSec )
            {
                // Bad format....
                ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
    
        // TODO:  Validate sample size.
    
        if (NT_SUCCESS(ntStatus))
        {
            PWAVEFORMATEX waveFormat = PWAVEFORMATEX(Format + 1);

            Miniport->SamplingFrequency = waveFormat->nSamplesPerSec;
    
            BYTE command =
                (   Capture
                ?   DSP_CMD_SETADCRATE
                :   DSP_CMD_SETDACRATE
                );
    
            Miniport->AdapterCommon->WriteController
            (
                command
            );
    
            Miniport->AdapterCommon->WriteController
            (
                (BYTE)(waveFormat->nSamplesPerSec >> 8)
            );
    
            Miniport->AdapterCommon->WriteController
            (
                (BYTE) waveFormat->nSamplesPerSec
            );

            _DbgPrintF(DEBUGLVL_VERBOSE,("  SampleRate: %d",waveFormat->nSamplesPerSec));
        }

        KeReleaseMutex(&Miniport->SampleRateSync,FALSE);
    }

    return ntStatus;
}

#pragma code_seg()

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::GetPosition()
 *****************************************************************************
 * Gets the current position.
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
	//TODO: adding fns to IAdapterCommon properly so i can call this
	//FIXME ???

	*Position = Miniport->AdapterCommon->hda_get_actual_stream_position() % DmaChannel->AllocatedBufferSize();	

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
    *PhysicalPosition =
            (_100NS_UNITS_PER_SECOND / 
                (1 << (FormatStereo + Format16Bit)) * *PhysicalPosition) / 
                    Miniport->SamplingFrequency;
    return STATUS_SUCCESS;
}
    
#pragma code_seg("PAGE")

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA::SetState()
 *****************************************************************************
 * Sets the state of the channel. should this be nonpaged?
 */
STDMETHODIMP
CMiniportWaveCyclicStreamHDA::
SetState
(
    IN      KSSTATE     NewState
)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("CMiniportWaveCyclicStreamHDA[%p]::SetState(%d)", this, NewState));

    NTSTATUS ntStatus = STATUS_SUCCESS;

    //
    // The acquire state is not distinguishable from the stop state for our
    // purposes.
    //
    if (NewState == KSSTATE_ACQUIRE)
    {
        NewState = KSSTATE_PAUSE;
    }

    if (State != NewState)
    {
        switch (NewState)
        {
        case KSSTATE_PAUSE:
            if (State == KSSTATE_RUN)
            {

				//TODO: adding fns to IAdapterCommon properly so i can call this
				//FIXME what if we just never stop? do we hear anything different
                Miniport->AdapterCommon->hda_stop_sound();

            }
            break;

        case KSSTATE_RUN:
            {
                // Start DMA.
                // TODO: something
				//FIXME
				//

				Miniport->AdapterCommon->hda_start_sound();
            }
            break;

        case KSSTATE_STOP:
            break;
        }

        State = NewState;
    }

    return ntStatus;
}

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
    RtlFillMemory(Buffer,ByteCount,Format16Bit ? 0 : 0x7f);
}
