/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

// Every debug output has "Modulname text"
static char STR_MODULENAME[] = "ICH Wave: ";

#include "minwave.h"
#include "ichwave.h"


/*****************************************************************************
 * PinDataRangesPCMStream
 *****************************************************************************
 * The next 3 arrays contain information about the data ranges of the pin for
 * wave capture, wave render and mic capture.
 * These arrays are filled dynamically by BuildDataRangeInformation().
 */

static KSDATARANGE_AUDIO PinDataRangesPCMStreamRender[WAVE_SAMPLERATES_TESTED];
static KSDATARANGE_AUDIO PinDataRangesPCMStreamCapture[WAVE_SAMPLERATES_TESTED];
static KSDATARANGE_AUDIO PinDataRangesMicStream[MIC_SAMPLERATES_TESTED];

static KSDATARANGE PinDataRangesAnalogBridge[] =
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
 * PinDataRangesPointersPCMStream
 *****************************************************************************
 * The next 3 arrays contain the pointers to the data range information of
 * the pin for wave capture, wave render and mic capture.
 * These arrays are filled dynamically by BuildDataRangeInformation().
 */
static PKSDATARANGE PinDataRangePointersPCMStreamRender[WAVE_SAMPLERATES_TESTED];
static PKSDATARANGE PinDataRangePointersPCMStreamCapture[WAVE_SAMPLERATES_TESTED];
static PKSDATARANGE PinDataRangePointersMicStream[MIC_SAMPLERATES_TESTED];

/*****************************************************************************
 * PinDataRangePointerAnalogStream
 *****************************************************************************
 * This structure pointers to the data range structures for the wave pins.
 */
static PKSDATARANGE PinDataRangePointersAnalogBridge[] =
{
    (PKSDATARANGE) PinDataRangesAnalogBridge
};


/*****************************************************************************
 * Wave Miniport Topology
 *========================
 *
 *                              +-----------+    
 *                              |           |    
 *    Capture (PIN_WAVEIN)  <---|2 --ADC-- 3|<=== (PIN_WAVEIN_BRIDGE)
 *                              |           |    
 *     Render (PIN_WAVEOUT) --->|0 --DAC-- 1|===> (PIN_WAVEOUT_BRIDGE)
 *                              |           |    
 *        Mic (PIN_MICIN)   <---|4 --ADC-- 5|<=== (PIN_MICIN_BRIDGE)
 *                              +-----------+    
 *
 */

/*****************************************************************************
 * MiniportPins
 *****************************************************************************
 * This structure describes pin (stream) types provided by this miniport.
 * The field that sets the number of data range entries (SIZEOF_ARRAY) is
 * overwritten by BuildDataRangeInformation().
 */
static PCPIN_DESCRIPTOR MiniportPins[] =
{
    // PIN_WAVEOUT
    {
        1,1,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersPCMStreamRender),  // DataRangesCount
            PinDataRangePointersPCMStreamRender,                // DataRanges
            KSPIN_DATAFLOW_IN,                          // DataFlow
            KSPIN_COMMUNICATION_SINK,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved            
        }
    },

    // PIN_WAVEOUT_BRIDGE
    {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersAnalogBridge),    // DataRangesCount
            PinDataRangePointersAnalogBridge,                  // DataRanges
            KSPIN_DATAFLOW_OUT,                         // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved            
        }
    },

    // PIN_WAVEIN
    {
        1,1,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersPCMStreamCapture), // DataRangesCount
            PinDataRangePointersPCMStreamCapture,               // DataRanges
            KSPIN_DATAFLOW_OUT,                         // DataFlow
            KSPIN_COMMUNICATION_SINK,                   // Communication
            (GUID *) &PINNAME_CAPTURE,                  // Category
            &KSAUDFNAME_RECORDING_CONTROL,              // Name
            0                                           // Reserved
        }
    },

    // PIN_WAVEIN_BRIDGE
    {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersAnalogBridge),    // DataRangesCount
            PinDataRangePointersAnalogBridge,                  // DataRanges
            KSPIN_DATAFLOW_IN,                          // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    },

    // PIN_MICIN
    {
        1,1,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersMicStream),// DataRangesCount
            PinDataRangePointersMicStream,              // DataRanges
            KSPIN_DATAFLOW_OUT,                         // DataFlow
            KSPIN_COMMUNICATION_SINK,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    },

    // PIN_MICIN_BRIDGE
    {
        0,0,0,  // InstanceCount
        NULL,   // AutomationTable
        {       // KsPinDescriptor
            0,                                          // InterfacesCount
            NULL,                                       // Interfaces
            0,                                          // MediumsCount
            NULL,                                       // Mediums
            SIZEOF_ARRAY(PinDataRangePointersAnalogBridge),    // DataRangesCount
            PinDataRangePointersAnalogBridge,                  // DataRanges
            KSPIN_DATAFLOW_IN,                          // DataFlow
            KSPIN_COMMUNICATION_NONE,                   // Communication
            (GUID *) &KSCATEGORY_AUDIO,                 // Category
            NULL,                                       // Name
            0                                           // Reserved
        }
    }
};

/*****************************************************************************
 * TopologyNodes
 *****************************************************************************
 * List of nodes.
 */
static PCNODE_DESCRIPTOR MiniportNodes[] =
{
    // NODE_WAVEOUT_DAC
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_DAC,        // Type
        NULL                    // Name
    },
    // NODE_WAVEIN_ADC
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    },
    //  NODE_MICIN_ADC
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    }
};

/*****************************************************************************
 * MiniportConnections
 *****************************************************************************
 * This structure identifies the connections between filter pins and
 * node pins.
 */
static PCCONNECTION_DESCRIPTOR MiniportConnections[] =
{
    //from_node             from_pin            to_node             to_pin
    { PCFILTER_NODE,        PIN_WAVEOUT,        NODE_WAVEOUT_DAC,   1},
    { NODE_WAVEOUT_DAC,     0,                  PCFILTER_NODE,      PIN_WAVEOUT_BRIDGE},
    { PCFILTER_NODE,        PIN_WAVEIN_BRIDGE,  NODE_WAVEIN_ADC,    1},
    { NODE_WAVEIN_ADC,      0,                  PCFILTER_NODE,      PIN_WAVEIN},
    { PCFILTER_NODE,        PIN_MICIN_BRIDGE,   NODE_MICIN_ADC,     1},
    { NODE_MICIN_ADC,       0,                  PCFILTER_NODE,      PIN_MICIN}
};

/*****************************************************************************
 * MiniportFilterDescriptor
 *****************************************************************************
 * Complete miniport description.
 */
static PCFILTER_DESCRIPTOR MiniportFilterDescriptor =
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

#pragma code_seg("PAGE")
/*****************************************************************************
 * CreateMiniportWaveICH
 *****************************************************************************
 * Creates a ICH wave miniport object for the ICH adapter.
 * This uses a macro from STDUNK.H to do all the work.
 */
NTSTATUS CreateMiniportWaveICH
(
    OUT PUNKNOWN   *Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN    UnknownOuter    OPTIONAL,
    IN  POOL_TYPE   PoolType
)
{
    PAGED_CODE ();

    ASSERT (Unknown);

    DOUT (DBG_PRINT, ("[CreateMiniportWaveICH]"));

	STD_CREATE_BODY_(CMiniportWaveICH,Unknown,UnknownOuter,PoolType,
                     PMINIPORTWAVECYCLIC);
}


/*****************************************************************************
 * CMiniportWaveICH::NonDelegatingQueryInterface
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICH::NonDelegatingQueryInterface
(
    IN  REFIID  Interface,
    OUT PVOID  *Object
)
{
    PAGED_CODE ();

    ASSERT (Object);

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::NonDelegatingQueryInterface]"));

    // Is it IID_IUnknown?
    if (IsEqualGUIDAligned (Interface, IID_IUnknown))
    {
        *Object = (PVOID)(PUNKNOWN)(PMINIPORTWAVECYCLIC)this;
    } 
    // or IID_IMiniport ...
    else if (IsEqualGUIDAligned (Interface, IID_IMiniport))
    {
        *Object = (PVOID)(PMINIPORT)this;
    } 
    // or IID_IMiniportWaveCyclic ...
    else if (IsEqualGUIDAligned (Interface, IID_IMiniportWaveCyclic))
    {
        *Object = (PVOID)(PMINIPORTWAVECYCLIC)this;
    } 
    // or IID_IPowerNotify ...
    else if (IsEqualGUIDAligned (Interface, IID_IPowerNotify))
    {
        *Object = (PVOID)(PPOWERNOTIFY)this;
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
    ((PUNKNOWN)(*Object))->AddRef();
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICH::~CMiniportWaveICH
 *****************************************************************************
 * Destructor.
 */
CMiniportWaveICH::~CMiniportWaveICH ()
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::~CMiniportWaveICH]"));

    //
    // Release the DMA channel.
    //
    if (DmaChannel)
    {
        DmaChannel->Release ();
        DmaChannel = NULL;
    }

    //
    // Release the interrupt sync.
    //
    if (InterruptSync)
    {
        InterruptSync->Release ();
        InterruptSync = NULL;
    }

    //
    // Release adapter common object.
    //
    if (AdapterCommon)
    {
        AdapterCommon->Release ();
        AdapterCommon = NULL;
    }

    //
    // Release the port.
    //
    if (Port)
    {
        Port->Release ();
        Port = NULL;
    }
}


/*****************************************************************************
 * CMiniportWaveICH::Init
 *****************************************************************************
 * Initializes the miniport.
 * Initializes variables and modifies the wave topology if needed.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICH::Init
(
    IN  PUNKNOWN       UnknownAdapter,
    IN  PRESOURCELIST  ResourceList,
    IN  PPORTWAVECYCLIC   Port_
    //OUT PSERVICEGROUP *ServiceGroup_
)
{
    PAGED_CODE ();

    ASSERT (UnknownAdapter);
    ASSERT (ResourceList);
    ASSERT (Port_);

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::Init]"));

    //
    // AddRef() is required because we are keeping this pointer.
    //
    Port = Port_;
    Port->AddRef ();

    //
    // No miniport service group
	//
    //*ServiceGroup_ = NULL;

    //
    // Set initial device power state
    //
    m_PowerState = PowerDeviceD0;

    NTSTATUS ntStatus = UnknownAdapter->
        QueryInterface (IID_IAdapterCommon, (PVOID *)&AdapterCommon);
    if (NT_SUCCESS (ntStatus))
    {
        //
        // Alter the topology for the wave miniport.
        //
        if (!(AdapterCommon->GetPinConfig (PINC_MICIN_PRESENT) &&
              AdapterCommon->GetPinConfig (PINC_MIC_PRESENT)))
        {
            //
		    // Remove the pins, nodes and connections for the MICIN.
            //
            MiniportFilterDescriptor.PinCount -= 2;
            MiniportFilterDescriptor.NodeCount -= 1;
            MiniportFilterDescriptor.ConnectionCount -= 2;
        }

        //
        // Process the resources.
        //
        ntStatus = ProcessResources (ResourceList);

        //
        // If we came till that point, check the CoDec for supported standard
        // sample rates. This function will then fill the data range information
        //
        if (NT_SUCCESS (ntStatus))
            ntStatus = BuildDataRangeInformation ();
    }

    //
    // If we fail we get destroyed anyway (that's where we clean up).
    //
    return ntStatus;
}


/*****************************************************************************
 * CMiniportWaveICH::ProcessResources
 *****************************************************************************
 * Processes the resource list, setting up helper objects accordingly.
 * Sets up the Interrupt + Service routine and DMA.
 */
NTSTATUS CMiniportWaveICH::ProcessResources
(
    IN  PRESOURCELIST ResourceList
)
{
    PAGED_CODE ();

    ASSERT (ResourceList);


    DOUT (DBG_PRINT, ("[CMiniportWaveICH::ProcessResources]"));


    ULONG countIRQ = ResourceList->NumberOfInterrupts ();
    if (countIRQ < 1)
    {
        DOUT (DBG_ERROR, ("Unknown configuration for wave miniport!"));
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    
    //
    // Create an interrupt sync object
    //
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ntStatus = PcNewInterruptSync (&InterruptSync,
                                   NULL,
                                   ResourceList,
                                   0,
                                   InterruptSyncModeNormal);

    if (!NT_SUCCESS (ntStatus) || !InterruptSync)
    {
        DOUT (DBG_ERROR, ("Failed to create an interrupt sync!"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Register our ISR.
    //
    ntStatus = InterruptSync->RegisterServiceRoutine (InterruptServiceRoutine,
                                                      (PVOID)this, FALSE);
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Failed to register ISR!"));
        return ntStatus;
    }

    //
    // Connect the interrupt.
    //
    ntStatus = InterruptSync->Connect ();
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Failed to connect the ISR with InterruptSync!"));
        return ntStatus;
    }

    //
    // Create the DMA Channel object.
    //
    ntStatus = Port->NewMasterDmaChannel (&DmaChannel,      // OutDmaChannel
                                          NULL,             // OuterUnknown (opt)
                                          //NonPagedPool,   // Pool Type
                                          NULL,             // ResourceList (opt)
										  16 * 1024 * 1024,	// MaximumLength
                                          //TRUE,           // ScatterGather
                                          TRUE,             // Dma32BitAddresses
                                          FALSE,            // Dma64BitAddresses
                                          //FALSE,          // IgnoreCount
                                          Width32Bits,      // DmaWidth
                                          MaximumDmaSpeed  // DmaSpeed
										  );               
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Failed on NewMasterDmaChannel!"));
        return ntStatus;
    }

    //
    // Get the DMA adapter.
    //
    AdapterObject = DmaChannel->GetAdapterObject ();

    //
    // On failure object is destroyed which cleans up.
    //
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CAdapterCommon::BuildDataRangeInformation
 *****************************************************************************
 * This function dynamically build the data range information for the pins.
 * It also connects the static arrays with the data range information
 * structure.
 * If this function returns with an error the miniport should be destroyed.
 *
 * To build the data range information, we test the most popular sample rates,
 * the functions calls ProgramSampleRate in AdapterCommon object to actually
 * program the sample rate. After probing that way for multiple sample rates,
 * the original value, which is 48KHz is, gets restored.
 * We have to test the sample rates for playback, capture and microphone
 * separately. Every time we succeed, we update the data range information and
 * the pointers that point to it.
 */
NTSTATUS CMiniportWaveICH::BuildDataRangeInformation (void)
{
    PAGED_CODE ();

    NTSTATUS    ntStatus;
    int nWavePlaybackEntries = 0;
    int nWaveRecordingEntries = 0;
    int nMicEntries = 0;
    int nLoop;

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::BuildDataRangeInformation]"));

    // Check for the render sample rates.
    for (nLoop = 0; nLoop < WAVE_SAMPLERATES_TESTED; nLoop++)
    {
        ntStatus = AdapterCommon->ProgramSampleRate (AC97REG_PCM_FRONT_RATE,
                                                     dwWaveSampleRates[nLoop]);

        // We support the sample rate?
        if (NT_SUCCESS (ntStatus))
        {
            // Add it to the PinDataRange
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.FormatSize = sizeof(KSDATARANGE_AUDIO);
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.Flags      = 0;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.SampleSize = 0;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.Reserved   = 0;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.SubFormat   = KSDATAFORMAT_SUBTYPE_PCM;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].DataRange.Specifier   = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].MaximumChannels = 2;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].MinimumBitsPerSample = 16;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].MaximumBitsPerSample = 16;
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].MinimumSampleFrequency = dwWaveSampleRates[nLoop];
            PinDataRangesPCMStreamRender[nWavePlaybackEntries].MaximumSampleFrequency = dwWaveSampleRates[nLoop];

            // Add it to the PinDataRangePointer
            PinDataRangePointersPCMStreamRender[nWavePlaybackEntries] = (PKSDATARANGE)&PinDataRangesPCMStreamRender[nWavePlaybackEntries];

            // Increase count
            nWavePlaybackEntries++;
        }
    }

    // Check for the capture sample rates.
    for (nLoop = 0; nLoop < WAVE_SAMPLERATES_TESTED; nLoop++)
    {
        ntStatus = AdapterCommon->ProgramSampleRate (AC97REG_PCM_LR_RATE, dwWaveSampleRates[nLoop]);

        // We support the sample rate?
        if (NT_SUCCESS (ntStatus))
        {
            // Add it to the PinDataRange
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.FormatSize = sizeof(KSDATARANGE_AUDIO);
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.Flags      = 0;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.SampleSize = 0;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.Reserved   = 0;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.SubFormat   = KSDATAFORMAT_SUBTYPE_PCM;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].DataRange.Specifier   = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].MaximumChannels = 2;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].MinimumBitsPerSample = 16;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].MaximumBitsPerSample = 16;
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].MinimumSampleFrequency = dwWaveSampleRates[nLoop];
            PinDataRangesPCMStreamCapture[nWaveRecordingEntries].MaximumSampleFrequency = dwWaveSampleRates[nLoop];

            // Add it to the PinDataRangePointer
            PinDataRangePointersPCMStreamCapture[nWaveRecordingEntries] = (PKSDATARANGE)&PinDataRangesPCMStreamCapture[nWaveRecordingEntries];

            // Increase count
            nWaveRecordingEntries++;
        }
    }

    // Check for the MIC sample rates.
    for (nLoop = 0; nLoop < MIC_SAMPLERATES_TESTED; nLoop++)
    {
        ntStatus = AdapterCommon->ProgramSampleRate (AC97REG_MIC_RATE, dwMicSampleRates[nLoop]);

        // We support the sample rate?
        if (NT_SUCCESS (ntStatus))
        {
            // Add it to the PinDataRange
            PinDataRangesMicStream[nMicEntries].DataRange.FormatSize = sizeof(KSDATARANGE_AUDIO);
            PinDataRangesMicStream[nMicEntries].DataRange.Flags      = 0;
            PinDataRangesMicStream[nMicEntries].DataRange.SampleSize = 0;
            PinDataRangesMicStream[nMicEntries].DataRange.Reserved   = 0;
            PinDataRangesMicStream[nMicEntries].DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
            PinDataRangesMicStream[nMicEntries].DataRange.SubFormat   = KSDATAFORMAT_SUBTYPE_PCM;
            PinDataRangesMicStream[nMicEntries].DataRange.Specifier   = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
            PinDataRangesMicStream[nMicEntries].MaximumChannels = 1;
            PinDataRangesMicStream[nMicEntries].MinimumBitsPerSample = 16;
            PinDataRangesMicStream[nMicEntries].MaximumBitsPerSample = 16;
            PinDataRangesMicStream[nMicEntries].MinimumSampleFrequency = dwMicSampleRates[nLoop];
            PinDataRangesMicStream[nMicEntries].MaximumSampleFrequency = dwMicSampleRates[nLoop];

            // Add it to the PinDataRangePointer
            PinDataRangePointersMicStream[nMicEntries] = (PKSDATARANGE)&PinDataRangesMicStream[nMicEntries];

            // Increase count
            nMicEntries++;
        }
    }

    // Now go through the pin descriptor list and change the data range entries to the actual number.
    for (nLoop = 0; nLoop < SIZEOF_ARRAY(MiniportPins); nLoop++)
    {
        if (MiniportPins[nLoop].KsPinDescriptor.DataRanges == PinDataRangePointersPCMStreamRender)
            MiniportPins[nLoop].KsPinDescriptor.DataRangesCount = nWavePlaybackEntries;
        if (MiniportPins[nLoop].KsPinDescriptor.DataRanges == PinDataRangePointersPCMStreamCapture)
            MiniportPins[nLoop].KsPinDescriptor.DataRangesCount = nWaveRecordingEntries;
        if (MiniportPins[nLoop].KsPinDescriptor.DataRanges == PinDataRangePointersMicStream)
            MiniportPins[nLoop].KsPinDescriptor.DataRangesCount = nMicEntries;
    }

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICH::NewStream
 *****************************************************************************
 * Creates a new stream.
 * This function is called when a streaming pin is created.
 * It checks if the channel is already in use, tests the data format, creates
 * and initializes the stream object.
 */
STDMETHODIMP CMiniportWaveICH::NewStream
(
    OUT PMINIPORTWAVECYCLICSTREAM *Stream,
    IN  PUNKNOWN                OuterUnknown,
    IN  POOL_TYPE               PoolType,
    //IN  PPORTWAVECYCLICSTREAM      PortStream,
    IN  ULONG                   Channel_,
    IN  BOOLEAN                 Capture,
    IN  PKSDATAFORMAT           DataFormat,
    OUT PDMACHANNEL            *DmaChannel_,
    OUT PSERVICEGROUP          *ServiceGroup
)
{
    PAGED_CODE ();

    ASSERT (Stream);
    //ASSERT (PortStream);
    ASSERT (DataFormat);
    ASSERT (DmaChannel_);
    ASSERT (ServiceGroup);

    CMiniportWaveICHStream *pWaveICHStream = 0;
	NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::NewStream]"));

    //
    // Validate the channel (pin id).
    //
    if ((Channel_ != PIN_WAVEOUT) && (Channel_ != PIN_WAVEIN) &&
       (Channel_ != PIN_MICIN))
    {
        DOUT (DBG_ERROR, ("NewStream was passed an invalid channel!"));
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Check if the pin is already in use
    //
	ULONG Channel = Channel_ >> 1;
    if (Streams[Channel])
    {
        DOUT (DBG_ERROR, ("Pin is already in use!"));
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Check parameters.
    //
    ntStatus = TestDataFormat (DataFormat, (WavePins)Channel_);
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_VSR, ("TestDataFormat failed!"));
        return ntStatus;
    }
        
    //
    // Create a new stream.
    //
    ntStatus = CreateMiniportWaveICHStream (&pWaveICHStream, OuterUnknown,
                                            PoolType);

    //
    // Return in case of an error.
    //
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Failed to create stream!"));
        return ntStatus;
    }

    //
    // Initialize the stream.
    //
    ntStatus = pWaveICHStream->Init (this,
                                    //PortStream,
                                    Channel,
                                    Capture,
                                    DataFormat,
                                    ServiceGroup);
    if (!NT_SUCCESS (ntStatus))
    {
        //
        // Release the stream and clean up.
        //
        DOUT (DBG_ERROR, ("Failed to init stream!"));
        pWaveICHStream->Release ();
        *Stream = NULL;
        return ntStatus;
    }

    // 
    // Save the pointers.
    //
    *Stream = (PMINIPORTWAVECYCLICSTREAM)pWaveICHStream;
    *DmaChannel_ = DmaChannel;
    
    
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICH::GetDescription
 *****************************************************************************
 * Gets the topology.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICH::GetDescription
(
    OUT PPCFILTER_DESCRIPTOR *OutFilterDescriptor
)
{
    PAGED_CODE ();

    ASSERT (OutFilterDescriptor);

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::GetDescription]"));

    *OutFilterDescriptor = &MiniportFilterDescriptor;


    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICH::DataRangeIntersection
 *****************************************************************************
 * Tests a data range intersection.
 * Cause the AC97 controller does not support mono render or capture, we have
 * to check the max. channel field (unfortunately, there is no MinimumChannel
 * and MaximumChannel field, just a MaximumChannel field).
 * If the MaximumChannel is 2, then we can pass this to the default handler of
 * portcls which always chooses the most (SampleFrequency, Channel, Bits etc.)
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICH::DataRangeIntersection
(
    IN      ULONG           PinId,
    IN      PKSDATARANGE    ClientsDataRange,
    IN      PKSDATARANGE    MyDataRange,
    IN      ULONG           OutputBufferLength,
    OUT     PVOID           ResultantFormat,
    OUT     PULONG          ResultantFormatLength
)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::DataRangeIntersection]"));

    //
    // If the dataformat is not PCM audio bail out.
    //
    if (!IsEqualGUIDAligned (ClientsDataRange->MajorFormat,
                             KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned (ClientsDataRange->SubFormat,
                             KSDATAFORMAT_SUBTYPE_PCM))
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // From this point, we assume that the structure passed is really a Data
    // range specifier (the ClientsDataRange->Specifier does not contain the
    // specifier for Data range (there is none))
    //
    if (PinId == PIN_MICIN)
    {
        // reject stereo format for microphone capture (is mono only).
        if (((PKSDATARANGE_AUDIO)ClientsDataRange)->MaximumChannels > 1)
        {
            return STATUS_NO_MATCH;
        }
    }
    else
    {
        // reject mono format for normal wave playback or capture.
        if (((PKSDATARANGE_AUDIO)ClientsDataRange)->MaximumChannels < 2)
        {
            return STATUS_NO_MATCH;
        }
    }

    // Let portcls do some work ...
    return STATUS_NOT_IMPLEMENTED;
}


/*****************************************************************************
 * CMiniportWaveICH::TestDataFormat
 *****************************************************************************
 * Checks if the passed data format is known to the driver and verifies that
 * the number of channels, the width of one sample match to the AC97
 * specification.
 */
NTSTATUS CMiniportWaveICH::TestDataFormat
(
    IN  PKSDATAFORMAT Format,
    IN  WavePins      Pin
)
{
    PAGED_CODE ();

    ASSERT (Format);

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::TestDataFormat]"));

    //
    // KSDATAFORMAT contains three GUIDs to support extensible format.  The
    // first two GUIDs identify the type of data.  The third indicates the
    // type of specifier used to indicate format specifics.  We are only
    // supporting PCM audio formats that use WAVEFORMATEX.
    //
    if (!IsEqualGUIDAligned (Format->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
        !IsEqualGUIDAligned (Format->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
    {
        DOUT (DBG_ERROR, ("Invalid format type!"));
        return STATUS_INVALID_PARAMETER;
    }

    //
    // A WAVEFORMATEX structure should appear after the generic KSDATAFORMAT
    // if the GUIDs turn out as we expect.
    //
    if (!IsEqualGUIDAligned (Format->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
    {
        DOUT (DBG_ERROR, ("Invalid/Unsupported specifier!"));
        return STATUS_INVALID_PARAMETER; 
    }

    //
    // If the size doesn't match, then something is screwed up.
    //
    if (Format->FormatSize < sizeof(KSDATAFORMAT_WAVEFORMATEX))
    {
        DOUT (DBG_ERROR, ("Invalid FormatSize!"));
        return STATUS_INVALID_PARAMETER;
    }
            
    //
    // Print the information.
    //
    PWAVEFORMATEX waveFormat = (PWAVEFORMATEX)(Format + 1);
    DOUT (DBG_STREAM,
         ("TestDataFormat: Frequency: %d, Channels: %d, bps: %d",
          waveFormat->nSamplesPerSec,
          waveFormat->nChannels,
          waveFormat->wBitsPerSample));
    
    //
    // We only support PCM, 16-bit, stero (mono for mic).
    //
    if (waveFormat->wFormatTag != WAVE_FORMAT_PCM ||
        waveFormat->wBitsPerSample != 16 ||
        waveFormat->nChannels != (Pin == PIN_MICIN ? 1 : 2))
    {
        DOUT (DBG_WARNING, ("HW doesn't support this format!"));
        DOUT (DBG_WARNING,
             ("TestDataFormat: Frequency: %d, Channels: %d, bps: %d",
              waveFormat->nSamplesPerSec, waveFormat->nChannels,
              waveFormat->wBitsPerSample));
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICH::PowerChangeNotify
 *****************************************************************************
 * This routine gets called as a result of hooking up the IPowerNotify 
 * interface. This interface indicates the driver's desire to receive explicit
 * notification of power state changes. The interface provides a single method
 * (or callback) that is called by the miniport's corresponding port driver in
 * response to a power state change. Using wave audio as an example, when the
 * device is requested to go to a sleep state the port driver pauses any 
 * active streams and then calls the power notify callback to inform the
 * miniport of the impending power down. The miniport then has an opportunity
 * to save any necessary context before the adapter's PowerChangeState method
 * is called. The process is reversed when the device is powering up. PortCls
 * first calls the adapter's PowerChangeState method to power up the adapter.
 * The port driver then calls the miniport's callback to allow the miniport to
 * restore its context. Finally, the port driver unpauses any previously paused
 * active audio streams.
 */
STDMETHODIMP_(void) CMiniportWaveICH::PowerChangeNotify
(
    IN  POWER_STATE NewState
) 
{
    PAGED_CODE ();
    NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CMiniportWaveICH::PowerChangeNotify]"));
    
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
    // In case we return to D0 power state from a D3 state, restore the
    // interrupt connection.
    //
    if (NewState.DeviceState == PowerDeviceD0)
    {
        if (m_PowerState == PowerDeviceD3)
        {
            ntStatus = InterruptSync->Connect ();
            if (!NT_SUCCESS (ntStatus))
            {
                DOUT (DBG_ERROR, ("Failed to connect the ISR with InterruptSync!"));
                // We can do nothing else than just continue ...
            }
        }
    }
    
    //
    // Call the stream routine which takes care of the DMA engine.
    // That's all we have to do.
    //
    for (int loop = PIN_WAVEOUT_OFFSET; loop < PIN_MICIN_OFFSET; loop++)
    {
        if (Streams[loop])
        {
            ntStatus = Streams[loop]->PowerChangeNotify (NewState);
            if (!NT_SUCCESS (ntStatus))
            {
                DOUT (DBG_ERROR, ("PowerChangeNotify D%d for the stream failed",
                              (ULONG)NewState.DeviceState - (ULONG)PowerDeviceD0));
            }
        }
    }

    //
    // In case we go to D3 we disconnect the interrupt service reoutine
    // from the interrupt.
    //
    if (NewState.DeviceState == PowerDeviceD3)
    {
        InterruptSync->Disconnect ();
    }

    //
    // Save the new state.  This local value is used to determine when to 
    // cache property accesses and when to permit the driver from accessing 
    // the hardware.
    //
    m_PowerState = NewState.DeviceState;
    DOUT (DBG_POWER, ("Entering D%d",
            (ULONG)m_PowerState - (ULONG)PowerDeviceD0));
}


/*****************************************************************************
 * Non paged code begins here
 *****************************************************************************
 */

#pragma code_seg()
/*****************************************************************************
 * CMiniportWaveICH::Service
 *****************************************************************************
 * Processing routine for dealing with miniport interrupts.  This routine is
 * called at DISPATCH_LEVEL.
 */
//STDMETHODIMP_(void) CMiniportWaveICH::Service (void)
//{
//    // not needed
//}


/*****************************************************************************
 * InterruptServiceRoutine
 *****************************************************************************
 * The task of the ISR is to clear an interrupt from this device so we don't
 * get an interrupt storm and schedule a DPC which actually does the 
 * real work.
 */
NTSTATUS CMiniportWaveICH::InterruptServiceRoutine
(
    IN  PINTERRUPTSYNC  InterruptSync,
    IN  PVOID           DynamicContext
)
{
    ASSERT (InterruptSync);
    ASSERT (DynamicContext);

    ULONG   GlobalStatus;
    USHORT  DMAStatusRegister;

    //
    // Get our context which is a pointer to class CMiniportWaveICH.
    //
    CMiniportWaveICH *that = (CMiniportWaveICH *)DynamicContext;

    //
    // Check for a valid AdapterCommon pointer.
    //
    if (!that->AdapterCommon)
    {
        //
        // In case we didn't handle the interrupt, unsuccessful tells the system
        // to call the next interrupt handler in the chain.
        //
        return STATUS_UNSUCCESSFUL;
    }

    //
    // From this point down, basically in the complete ISR, we cannot use
    // relative addresses (stream class base address + X_CR for example)
    // cause we might get called when the stream class is destroyed or
    // not existent. This doesn't make too much sense (that there is an
    // interrupt for a non-existing stream) but could happen and we have
    // to deal with the interrupt.
    //
    
    //
    // Read the global register to check the interrupt bits
    //
    GlobalStatus = that->AdapterCommon->ReadBMControlRegister32 (GLOB_STA);
    
    //
    // Check for weird return values. Could happen if the PCI device is already
    // disabled and another device that shares this interrupt generated an
    // interrupt.
    // The register should never have all bits cleared or set.
    //
    if (!GlobalStatus || (GlobalStatus == 0xFFFFFFFF))
    {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Check for PCM out interrupt.
    //
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    if (GlobalStatus & GLOB_STA_POINT)
    {
        //
        // Read PCM out DMA status registers.
        //
        DMAStatusRegister = (USHORT)that->AdapterCommon->
            ReadBMControlRegister16 (PO_SR);

        //
        // We could now check for every possible error condition
        // (like FIFO error) and monitor the different errors, but currently
        // we have the same action for every INT and therefore we simplify
        // this routine enormous with just clearing the bits.
        //
        if (that->Streams[PIN_WAVEOUT_OFFSET])
        {
            //
            // ACK the interrupt.
            //
            that->AdapterCommon->WriteBMControlRegister (PO_SR, DMAStatusRegister);
            ntStatus = STATUS_SUCCESS;

            //
            // Request DPC service for PCM out.
            //
            if ((that->Port) && (that->Streams[PIN_WAVEOUT_OFFSET]->ServiceGroup))
            {
                that->Port->Notify (that->Streams[PIN_WAVEOUT_OFFSET]->ServiceGroup);
            }
            else
            {
                //
                // Bad, bad.  Shouldn't print in an ISR!
                //
                DOUT (DBG_ERROR, ("WaveOut INT fired but no stream object there."));
            }
        }
    }
    
    //
    // Check for PCM in interrupt.
    //
    if (GlobalStatus & GLOB_STA_PIINT)
    {
        //
        // Read PCM in DMA status registers.
        //
        DMAStatusRegister = (USHORT)that->AdapterCommon->
            ReadBMControlRegister16 (PI_SR);

        //
        // We could now check for every possible error condition
        // (like FIFO error) and monitor the different errors, but currently
        // we have the same action for every INT and therefore we simplify
        // this routine enormous with just clearing the bits.
        //
        if (that->Streams[PIN_WAVEIN_OFFSET])
        {
            //
            // ACK the interrupt.
            //
            that->AdapterCommon->WriteBMControlRegister (PI_SR, DMAStatusRegister);
            ntStatus = STATUS_SUCCESS;

            //
            // Request DPC service for PCM in.
            //
            if ((that->Port) && (that->Streams[PIN_WAVEIN_OFFSET]->ServiceGroup))
            {
                that->Port->Notify (that->Streams[PIN_WAVEIN_OFFSET]->ServiceGroup);
            }
            else
            {
                //
                // Bad, bad.  Shouldn't print in an ISR!
                //
                DOUT (DBG_ERROR, ("WaveIn INT fired but no stream object there."));
            }
        }
    }

    //
    // Check for MIC in interrupt.
    //
    if (GlobalStatus & GLOB_STA_MINT)
    {
        //
        // Read MIC in DMA status registers.
        //
        DMAStatusRegister = (USHORT)that->AdapterCommon->
            ReadBMControlRegister16 (MC_SR);

        //
        // We could now check for every possible error condition
        // (like FIFO error) and monitor the different errors, but currently
        // we have the same action for every INT and therefore we simplify
        // this routine enormous with just clearing the bits.
        //
        if (that->Streams[PIN_MICIN_OFFSET])
        {
            //
            // ACK the interrupt.
            //
            that->AdapterCommon->WriteBMControlRegister (MC_SR, DMAStatusRegister);
            ntStatus = STATUS_SUCCESS;

            //
            // Request DPC service for PCM out.
            //
            if ((that->Port) && (that->Streams[PIN_MICIN_OFFSET]->ServiceGroup))
            {
                that->Port->Notify (that->Streams[PIN_MICIN_OFFSET]->ServiceGroup);
            }
            else
            {
                //
                // Bad, bad.  Shouldn't print in an ISR!
                //
                DOUT (DBG_ERROR, ("MicIn INT fired but no stream object there."));
            }
        }
    }

    return ntStatus;
}

