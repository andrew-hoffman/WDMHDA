/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

// Every debug output has "Modulname text".
static char STR_MODULENAME[] = "ICH Topology: ";

#include "mintopo.h"

/*****************************************************************************
 * AC97 topology miniport tables
 */

/*****************************************************************************
 * PinDataRangesBridge
 *****************************************************************************
 * Structures indicating range of valid format values for bridge pins.
 */
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

static KSDATARANGE_AUDIO PinDataRangesDigitalBridge[] =
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
        2,      // maximum channels
        8,      // minimum bits per sample
        16,     // maximum bits per sample
        48000,  // minimum sample rate
        48000   // maximum sample rate
    }
};

/*****************************************************************************
 * PinDataRangePointersBridge
 *****************************************************************************
 * List of pointers to structures indicating range of valid format values
 * for audio bridge pins.
 */
static PKSDATARANGE PinDataRangePointersAnalogBridge[] =
{
    (PKSDATARANGE)&PinDataRangesAnalogBridge[0]
};

static PKSDATARANGE PinDataRangePointersDigitalBridge[] =
{
    (PKSDATARANGE)&PinDataRangesDigitalBridge[0]
};


/*****************************************************************************
 * PropertiesVolume
 *****************************************************************************
 * Properties for volume controls.
 */
static PCPROPERTY_ITEM PropertiesVolume[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_VOLUMELEVEL,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        CMiniportTopologyICH::PropertyHandler_Level
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        CMiniportTopologyICH::PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationVolume
 *****************************************************************************
 * Automation table for volume controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP (AutomationVolume, PropertiesVolume);

/*****************************************************************************
 * PropertiesMute
 *****************************************************************************
 * Properties for mute controls.
 */
static PCPROPERTY_ITEM PropertiesMute[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_MUTE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET,
        CMiniportTopologyICH::PropertyHandler_OnOff
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        CMiniportTopologyICH::PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationMute
 *****************************************************************************
 * Automation table for mute controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP (AutomationMute, PropertiesMute);

/*****************************************************************************
 * PropertiesMux
 *****************************************************************************
 * Properties for mux controls.
 */
static PCPROPERTY_ITEM PropertiesMux[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_MUX_SOURCE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET,
        CMiniportTopologyICH::PropertyHandler_Ulong
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        CMiniportTopologyICH::PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationMux
 *****************************************************************************
 * Automation table for mux controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP (AutomationMux, PropertiesMux);

/*****************************************************************************
 * PropertiesSpecial
 *****************************************************************************
 * Properties for Special controls like fake loudness and fake agc.
 */
static PCPROPERTY_ITEM PropertiesSpecial[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_AGC,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET,
        CMiniportTopologyICH::PropertyHandler_OnOff
    },
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_LOUDNESS,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET,
        CMiniportTopologyICH::PropertyHandler_OnOff
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        CMiniportTopologyICH::PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationAgc
 *****************************************************************************
 * Automation table for agc controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP (AutomationSpecial, PropertiesSpecial);

/*****************************************************************************
 * PropertiesTone
 *****************************************************************************
 * Properties for tone controls.
 */
static PCPROPERTY_ITEM PropertiesTone[] =
{
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_BASS,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        CMiniportTopologyICH::PropertyHandler_Tone
    },
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_TREBLE,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        CMiniportTopologyICH::PropertyHandler_Tone
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        CMiniportTopologyICH::PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * AutomationTone
 *****************************************************************************
 * Automation table for tone controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP (AutomationTone, PropertiesTone);

/*****************************************************************************
 * Properties3D
 *****************************************************************************
 * Properties for 3D controls.
 */
static PCPROPERTY_ITEM Properties3D[] =
{
    // are faky volume controls.
    { 
        &KSPROPSETID_Audio, 
        KSPROPERTY_AUDIO_VOLUMELEVEL,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        CMiniportTopologyICH::PropertyHandler_Tone
    },
    {
        &KSPROPSETID_Audio,
        KSPROPERTY_AUDIO_CPU_RESOURCES,
        KSPROPERTY_TYPE_GET,
        CMiniportTopologyICH::PropertyHandler_CpuResources
    }
};

/*****************************************************************************
 * Automation3D
 *****************************************************************************
 * Automation table for 3D controls.
 */
DEFINE_PCAUTOMATION_TABLE_PROP (Automation3D, Properties3D);


#pragma code_seg("PAGE")

/*****************************************************************************
 * CreateMiniportTopologyICH
 *****************************************************************************
 * Creates a topology miniport object for the ICH audio adapter.  This uses a
 * macro from STDUNK.H to do all the work.
 */
NTSTATUS CreateMiniportTopologyICH
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
)
{
    PAGED_CODE ();

    ASSERT (Unknown);

    STD_CREATE_BODY (CMiniportTopologyICH, Unknown, UnknownOuter, PoolType);
}

/*****************************************************************************
 * CMiniportTopologyICH::NonDelegatingQueryInterface
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 * We basically just check any GUID we know and return this object in case we
 * know it.
 */
STDMETHODIMP_(NTSTATUS) CMiniportTopologyICH::NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE ();

    ASSERT (Object);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::NonDelegatingQueryInterface]"));

    // Is it IID_IUnknown?
    if (IsEqualGUIDAligned (Interface, IID_IUnknown))
    {
        *Object = (PVOID)(PUNKNOWN)this;
    }
    else
    // or IID_IMiniport ...
    if (IsEqualGUIDAligned (Interface, IID_IMiniport))
    {
        *Object = (PVOID)(PMINIPORT)this;
    }
    else
    // or IID_IMiniportTopology ...
    if (IsEqualGUIDAligned (Interface, IID_IMiniportTopology))
    {
        *Object = (PVOID)(PMINIPORTTOPOLOGY)this;
    }
    else
    // or maybe our IID_IMiniportTopologyICH ...
    if (IsEqualGUIDAligned (Interface, IID_IMiniportTopologyICH))
    {
        *Object = (PVOID)(PMINIPORTTOPOLOGYICH)this;
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
    ((PUNKNOWN)(*Object))->AddRef ();
    return STATUS_SUCCESS;
}

/*****************************************************************************
 * CMiniportTopologyICH::~CMiniportTopologyICH
 *****************************************************************************
 * Destructor.
 */
CMiniportTopologyICH::~CMiniportTopologyICH ()
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::~CMiniportTopologyICH]"));

    // release all the stuff that we had referenced or allocated.
    if (Port)
    {
        Port->Release ();
        Port = NULL;
    }

    if (AdapterCommon)
    {
        AdapterCommon->Release ();
        AdapterCommon = NULL;
    }

    if (FilterDescriptor)
    {
        ExFreePool (FilterDescriptor);
        FilterDescriptor = NULL;
    }

    if (ConnectionDescriptors)
    {
        ExFreePool (ConnectionDescriptors);
        ConnectionDescriptors = NULL;
    }

    if (NodeDescriptors)
    {
        ExFreePool (NodeDescriptors);
        NodeDescriptors = NULL;
    }

    if (PinDescriptors)
    {
        ExFreePool (PinDescriptors);
        PinDescriptors = NULL;
    }
}

/*****************************************************************************
 * CMiniportTopologyICH::DataRangeIntersection
 *****************************************************************************
 * Is defined in the IMiniportTopology interface. We just return
 * STATUS_NOT_IMPLEMENTED and portcls will use a default handler for this.
 */
STDMETHODIMP_(NTSTATUS) CMiniportTopologyICH::DataRangeIntersection
(
    IN      ULONG           PinId,
    IN      PKSDATARANGE    DataRange,
    IN      PKSDATARANGE    MatchingDataRange,
    IN      ULONG           OutputBufferLength,
    OUT     PVOID           ResultantFormat     OPTIONAL,
    OUT     PULONG          ResultantFormatLength
)
{
    return STATUS_NOT_IMPLEMENTED;
};

/*****************************************************************************
 * CMiniportTopologyICH::Init
 *****************************************************************************
 * Initializes the miniport.
 * We initialize the translation tables, add reference to the port driver and
 * build the topology.
 */
STDMETHODIMP_(NTSTATUS) CMiniportTopologyICH::Init
(
    IN      PUNKNOWN        UnknownAdapter,
    IN      PRESOURCELIST   ResourceList,
    IN      PPORTTOPOLOGY   Port_
)
{
    PAGED_CODE ();

    ASSERT (UnknownAdapter);
    ASSERT (Port_);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::Init]"));

    //
    // AddRef() is required because we are keeping this pointer.
    //
    Port = Port_;
    Port->AddRef ();

    // 
    // get the IAdapterCommon interface from the adapter.
    //
    NTSTATUS ntStatus = UnknownAdapter->QueryInterface (IID_IAdapterCommon,
                                                       (PVOID *) &AdapterCommon);

    //
    // initialize translation tables
    //
    for (int i = 0; i < PIN_TOP_ELEMENT; i++)
    {
        stPinTrans[i].PinDef = PIN_INVALID;
        stPinTrans[i].PinNr = -1;
    }

    for (i = 0; i < NODE_TOP_ELEMENT; i++)
    {
        stNodeTrans[i].NodeDef = NODE_INVALID;
        stNodeTrans[i].NodeNr = -1;
    }

    // build the topology (means register pins, nodes, connections).
    if (NT_SUCCESS (ntStatus))
    {
        ntStatus = BuildTopology ();
    }
    
    if (!NT_SUCCESS (ntStatus))
    {
        //
        // clean up our mess
        //

        // clean up AdapterCommon
        if (AdapterCommon)
        {
            AdapterCommon->Release ();
            AdapterCommon = NULL;
        }

        // release the port
        Port->Release ();
        Port = NULL;
    }

    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyICH::GetDescription
 *****************************************************************************
 * Gets/returns the topology to the system.
 */
STDMETHODIMP_(NTSTATUS) CMiniportTopologyICH::GetDescription
(
    OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
{
    PAGED_CODE ();

    ASSERT (OutFilterDescriptor);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::GetDescription]"));

#if (DBG)
    // Dump it here. The system requests the topology only once.
    DumpTopology ();
#endif

    if (FilterDescriptor)
    {
        *OutFilterDescriptor = FilterDescriptor;
        return STATUS_SUCCESS;
    }
    else
        return STATUS_DEVICE_CONFIGURATION_ERROR;
}

#if (DBG)
/*****************************************************************************
 * CMiniportTopologyICH::DumpTopology
 *****************************************************************************
 * Dumps the topology for debugging.
 * See the defines at the beginning of this file?
 */
void CMiniportTopologyICH::DumpTopology (void)
{
    PAGED_CODE ();

    if (FilterDescriptor)
    {
        // dump the pins
        DOUT (DBG_PINS, ("TOPOLOGY MINIPORT PINS"));
        for(ULONG index = 0; index < FilterDescriptor->PinCount; index++)
        {
            DOUT (DBG_PINS, ("  %2d %s", index,
                             TopoPinStrings[TransPinNrToPinDef (index)]));
        }

        // dump the nodes
        DOUT (DBG_NODES, ("TOPOLOGY MINIPORT NODES"));
        for(index = 0; index < FilterDescriptor->NodeCount; index++)
        {
            DOUT (DBG_NODES, ("  %2d %s", index,
                              NodeStrings[TransNodeNrToNodeDef (index)]));
        }

        // dump the connections
        DOUT (DBG_CONNS, ("TOPOLOGY MINIPORT CONNECTIONS"));
        for(index = 0; index < FilterDescriptor->ConnectionCount; index++)
        {
            DOUT (DBG_CONNS, ("  %2d (%d,%d)->(%d,%d)", index,
                FilterDescriptor->Connections[index].FromNode,
                FilterDescriptor->Connections[index].FromNodePin,
                FilterDescriptor->Connections[index].ToNode,
                FilterDescriptor->Connections[index].ToNodePin));
        }
    }
}
#endif


/*****************************************************************************
 * Miniport Topology               V = Volume, M = Mute, L = Loudness, A = AGC
 *====================             T = Treble, B = Bass, 0-9 = PinNr of node
 *
 * PCBEEP ---> V ---> M -----------------------------\
 * PHONE  ---> V ---> M ----------------------------\ \
 *                                                   \ \
 * WaveOut ------> V --> M ---------------> 1+-----+  \ \
 * MIC1 or MIC2 --> L --> A --> V --> M --> 2| SUM |   \ \
 * LineIn -------> V --> M ---------------> 3|     |    \ \
 * CD     -------> V --> M ---------------> 4|     |0--->SUM--> T --> B --> L --> A +-> V --> M (MasterOut)
 * Video  -------> V --> M ---------------> 5|     |                                |
 * AUX    -------> V --> M ---------------> 6|     |                                +-> V --> M (Headphone)
 * 3D Depth  -------> V --> M ------------> 7|     |
 * 3D Center -------> V --> M ------------> 8+-----+
 *
 * 
 * virt. Pin: Tone mix mono   ---> V --> M ---> 7+-------+
 * virt. Pin: Tone mix stereo ---> V --> M ---> 6|       |
 * Phone                      ---> V --> M ---> 8|   M   |
 * Mic (after AGC)            ---> V --> M ---> 1|       |0-----> (WaveIn)
 * LineIn                     ---> V --> M ---> 5|   U   |
 * CD                         ---> V --> M ---> 2|       |
 * Video                      ---> V --> M ---> 3|   X   |
 * AUX                        ---> V --> M ---> 4+-------+
 *
 *
 * virt. Pin: 3D mix mono ---> V ---> M ---> 1+-----+
 *                                            | MUX |0----> (MonoOut)
 * Mic (after AGC)        ---> V ---> M ---> 2+-----+
 *
 *
 * Mic (after AGC) ----> V ----> M -----> (MicIn)
 *
 *----------------------------------------------------------------------------
 *
 * As you can see, the exposed topology is somewhat different from the real AC97
 * topology. This is because the system that translates the topology to "mixer
 * lines" gets confused if it has to deal with all the mess. So we have to make it
 * plain and simple for the system.
 * Some issues block us from exposing a nice plain and simple topology. The prg.
 * which displayes the "Volume Control Panel" (sndvol32) does _only_ display
 * Volumes, Mutes (only one "in a row"), Treble, Bass, Loudness and AGC under
 * Advanced control panel. We don't have 3D controls, and before we go into a
 * Muxer, there has to be Volume controls in front.
 * So what do we do?
 * 1) We fake 3D controls as Volume controls. The Mutes represent 3D bypass and
 *    3D on/off
 * 2) All inputs (including the 3D controls) go staight into a SUM. Important is
 *    that there are not 2 Volumes, Mutes in a row, e.g. ---> V ---> M ---> V ---> M
 *    In that case, only one Volume/Mute would be displayed.
 * 3) We can't make a connection from the tone controls to the Wave In muxer (even
 *    with Volumes in front), so we create fake pins that we name user friendly.
 *    Same with the connection from the 3D mixer to the Mono output.
 * 4) We discard all supermixer controls that would convert stereo to mono or vice
 *    versa. Instead, we just connect the lines and when the control is queried we
 *    fail a right channel request (mono has only left channel).
 * 5) We have to make virtual volume and mute controls in front of each muxer.
 *    As you can see, these controls can be mono or stereo and there is only one
 *    HW register for them, so we have to cache the values and prg. the register
 *    each time the select changes or the selected volume control changes.
 */

/*****************************************************************************
 * CMiniportTopologyICH::BuildTopology
 *****************************************************************************
 * Builds the topology descriptors based on hardware configuration info
 * obtained from the adapter.
 */
NTSTATUS CMiniportTopologyICH::BuildTopology (void)
{
    PAGED_CODE ();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::BuildTopology]"));

    // allocate our filter descriptor
    FilterDescriptor = (PPCFILTER_DESCRIPTOR) ExAllocatePool (PagedPool,
                        sizeof(PCFILTER_DESCRIPTOR));
    if (FilterDescriptor)
    {
        // clear out the filter descriptor
        RtlZeroMemory (FilterDescriptor, sizeof(PCFILTER_DESCRIPTOR));

        // build the pin list
        ntStatus = BuildPinDescriptors ();
        if (NT_SUCCESS (ntStatus))
        {
            // build the node list
            ntStatus = BuildNodeDescriptors ();
            if (NT_SUCCESS (ntStatus))
            {
                // build the connection list
                ntStatus = BuildConnectionDescriptors ();
            }
        }
    }
    else
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    // that's whatever one of these build... functions returned.
    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyICH::BuildPinDescriptors
 *****************************************************************************
 * Builds the topology pin descriptors.
 */
NTSTATUS CMiniportTopologyICH::BuildPinDescriptors (void)
{
// Improvement would be to not use a Macro, use (inline) function instead.
#define INIT_PIN( pin, pinptr, category, name, index )      \
    pinptr->KsPinDescriptor.Category = (GUID*) category;    \
    pinptr->KsPinDescriptor.Name = (GUID*) name;            \
    SetPinTranslation (index++, pin);                       \
    pinptr++   

    PAGED_CODE ();

    ULONG               Index;
    PPCPIN_DESCRIPTOR   CurrentPin;

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::BuildPinDescriptors]"));

    // allocate our descriptor memory
    PinDescriptors = PPCPIN_DESCRIPTOR (ExAllocatePool (PagedPool,
                                PIN_TOP_ELEMENT * sizeof(PCPIN_DESCRIPTOR)));
    if (!PinDescriptors)
        return STATUS_INSUFFICIENT_RESOURCES;

    //
    // set default pin descriptor parameters
    //
    RtlZeroMemory (PinDescriptors, PIN_TOP_ELEMENT * sizeof(PCPIN_DESCRIPTOR));
    
    // spend some more time and set the pin descriptors to expected values.
    for (CurrentPin = PinDescriptors, Index = 0; Index < PIN_TOP_ELEMENT;
         CurrentPin++, Index++)
    {
        CurrentPin->KsPinDescriptor.DataRangesCount = SIZEOF_ARRAY(PinDataRangePointersAnalogBridge);
        CurrentPin->KsPinDescriptor.DataRanges      = PinDataRangePointersAnalogBridge;
        CurrentPin->KsPinDescriptor.DataFlow        = KSPIN_DATAFLOW_IN;
        CurrentPin->KsPinDescriptor.Communication   = KSPIN_COMMUNICATION_NONE;
    }

    //
    // modify the individual pin descriptors
    //
    CurrentPin  = PinDescriptors;
    Index       = 0;

    // add the PIN_WAVEOUT_SOURCE pin descriptor (not optional)
    INIT_PIN (PIN_WAVEOUT_SOURCE,
              CurrentPin,
              &KSCATEGORY_AUDIO,
              NULL,
              Index);

    // add the PIN_PCBEEP_SOURCE pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_PCBEEP_PRESENT))
    {
        INIT_PIN (PIN_PCBEEP_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_SPEAKER,
                  &KSAUDFNAME_PC_SPEAKER,
                  Index);
    }

    // add the PIN_PHONE_SOURCE pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_PHONE_PRESENT))
    {
        INIT_PIN (PIN_PHONE_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_PHONE_LINE,
                  NULL,
                  Index);
    }

    // add the PIN_MIC_SOURCE pin descriptor (could be disabled)
    if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT))
    {
        INIT_PIN (PIN_MIC_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_MICROPHONE,
                  NULL,
                  Index);
    }

    // add the PIN_LINEIN_SOURCE pin descriptor (could be disabled)
    if (AdapterCommon->GetPinConfig (PINC_LINEIN_PRESENT))
    {
        INIT_PIN (PIN_LINEIN_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_LINE_CONNECTOR,
                  &KSAUDFNAME_LINE_IN,
                  Index);
    }

    // add the PIN_CD_SOURCE pin descriptor (could be disabled)
    if (AdapterCommon->GetPinConfig (PINC_CD_PRESENT))
    {
        INIT_PIN (PIN_CD_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_CD_PLAYER,
                  NULL,
                  Index);
    }

    // add the PIN_VIDEO_SOURCE pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_VIDEO_PRESENT))
    {
        INIT_PIN (PIN_VIDEO_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_ANALOG_CONNECTOR,
                  &KSAUDFNAME_VIDEO,
                  Index);
    }

    // add the PIN_AUX_SOURCE pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_AUX_PRESENT))
    {
        INIT_PIN (PIN_AUX_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_ANALOG_CONNECTOR,
                  &KSAUDFNAME_AUX,
                  Index);
    }

    // add the PIN_VIRT_TONE_MIX_SOURCE pin descriptor (not optional)
    INIT_PIN (PIN_VIRT_TONE_MIX_SOURCE,
              CurrentPin,
              &KSNODETYPE_ANALOG_CONNECTOR,
              &KSAUDFNAME_STEREO_MIX,
              Index);

    // add the PIN_VIRT_TONE_MIX_MONO_SOURCE pin descriptor (not optional)
    INIT_PIN (PIN_VIRT_TONE_MIX_MONO_SOURCE,
              CurrentPin,
              &KSNODETYPE_ANALOG_CONNECTOR,
              &KSAUDFNAME_MONO_MIX,
              Index);

    // create a virt. pin for the 3D controls
    if (AdapterCommon->GetNodeConfig (NODEC_3D_PRESENT))
    {
        if (AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE))
        {
            INIT_PIN (PIN_VIRT_3D_CENTER_SOURCE,
                      CurrentPin,
                      &KSNODETYPE_ANALOG_CONNECTOR,
                      &KSAUDFNAME_3D_CENTER,
                      Index);
        }

        // A weird way would be to have 3D but only fixed sliders. In that case,
        // display one fixed slider (3D depth).
        if (AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE) ||
           (!AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE) &&
            !AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE)))
        {
            INIT_PIN (PIN_VIRT_3D_DEPTH_SOURCE,
                      CurrentPin,
                      &KSNODETYPE_ANALOG_CONNECTOR,
                      &KSAUDFNAME_3D_DEPTH,
                      Index);
        }
    }

    // add the PIN_MASTEROUT_DEST pin descriptor (not optional)
    CurrentPin->KsPinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
    INIT_PIN (PIN_MASTEROUT_DEST,
              CurrentPin,
              &KSNODETYPE_SPEAKER,
              &KSAUDFNAME_VOLUME_CONTROL,
              Index);

    // add the PIN_HPOUT_DEST pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_HPOUT_PRESENT))
    {
        CurrentPin->KsPinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
        INIT_PIN (PIN_HPOUT_DEST,
                  CurrentPin,
                  &KSNODETYPE_HEADPHONES,
                  NULL,
                  Index);
    }

    // add the PIN_WAVEIN_DEST pin descriptor (not optional)
    CurrentPin->KsPinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
    INIT_PIN (PIN_WAVEIN_DEST,
              CurrentPin,
              &KSCATEGORY_AUDIO,
              NULL,
              Index);

    // add the PIN_MICIN_DEST pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_MICIN_PRESENT) &&
        AdapterCommon->GetPinConfig (PINC_MIC_PRESENT))
    {
        CurrentPin->KsPinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
        INIT_PIN (PIN_MICIN_DEST,
                  CurrentPin,
                  &KSCATEGORY_AUDIO,
                  NULL,
                  Index);
    }

    // add the PIN_MONOOUT_DEST pin descriptor (optional)
    if (AdapterCommon->GetPinConfig (PINC_MONOOUT_PRESENT))
    {
        // add the PIN_VIRT_3D_MIX_MONO_SOURCE pin descriptor
        INIT_PIN (PIN_VIRT_3D_MIX_MONO_SOURCE,
                  CurrentPin,
                  &KSNODETYPE_ANALOG_CONNECTOR,
                  &KSAUDFNAME_MONO_MIX,
                  Index);

        CurrentPin->KsPinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
        // and the normal output pin
        INIT_PIN (PIN_MONOOUT_DEST,
                  CurrentPin,
                  &KSNODETYPE_PHONE_LINE,
                  &KSAUDFNAME_MONO_OUT,
                  Index);
    }

    // add the pin descriptor informatin to the filter descriptor
    FilterDescriptor->PinCount = Index;
    FilterDescriptor->PinSize = sizeof (PCPIN_DESCRIPTOR);
    FilterDescriptor->Pins = PinDescriptors;


    return STATUS_SUCCESS;

#undef INIT_PIN
}

/*****************************************************************************
 * CMiniportTopologyICH::BuildNodeDescriptors
 *****************************************************************************
 * Builds the topology node descriptors.
 */
NTSTATUS CMiniportTopologyICH::BuildNodeDescriptors (void)
{
// Improvement would be to not use a Macro, use (inline) function instead.
#define INIT_NODE( node, nodeptr, type, name, automation, index )   \
    nodeptr->Type = (GUID*) type;                                   \
    nodeptr->Name = (GUID*) name;                                   \
    nodeptr->AutomationTable = automation;                          \
    SetNodeTranslation (index++, node);                             \
    nodeptr++   

    PAGED_CODE ();

    NTSTATUS ntStatus = STATUS_SUCCESS;
    
    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::BuildNodeDescriptors]"));

    // allocate our descriptor memory
    NodeDescriptors = PPCNODE_DESCRIPTOR (ExAllocatePool (PagedPool,
                                NODE_TOP_ELEMENT * sizeof(PCNODE_DESCRIPTOR)));
    if (NodeDescriptors)
    {
        PPCNODE_DESCRIPTOR  CurrentNode = NodeDescriptors;
        ULONG               Index = 0;

        //
        // set default node descriptor parameters
        //
        RtlZeroMemory (NodeDescriptors, NODE_TOP_ELEMENT *
                       sizeof(PCNODE_DESCRIPTOR));

        // We don't have loopback mode currently. It is only used for testing anyway.

        // add the NODE_WAVEOUT_VOLUME node
        INIT_NODE (NODE_WAVEOUT_VOLUME,
                   CurrentNode,
                   &KSNODETYPE_VOLUME,
                   &KSAUDFNAME_WAVE_VOLUME,
                   &AutomationVolume,
                   Index);

        // add the NODE_WAVEOUT_MUTE node
        INIT_NODE (NODE_WAVEOUT_MUTE,
                   CurrentNode,
                   &KSNODETYPE_MUTE,
                   &KSAUDFNAME_WAVE_MUTE,
                   &AutomationMute,
                   Index);

        // add the PCBEEP nodes
        if (AdapterCommon->GetPinConfig (PINC_PCBEEP_PRESENT))
        {
            // add the NODE_PCBEEP_VOLUME node
            INIT_NODE (NODE_PCBEEP_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_PC_SPEAKER_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_PCBEEP_MUTE node
            INIT_NODE (NODE_PCBEEP_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &KSAUDFNAME_PC_SPEAKER_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the PHONE nodes
        if (AdapterCommon->GetPinConfig (PINC_PHONE_PRESENT))
        {
            // add the NODE_PHONE_VOLUME node
            INIT_NODE (NODE_PHONE_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &ICHFNAME_PHONE_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_PHONE_MUTE node
            INIT_NODE (NODE_PHONE_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &ICHFNAME_PHONE_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the MIC nodes
        if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT))
        {
            if (AdapterCommon->GetPinConfig (PINC_MIC2_PRESENT))
            {
                // add the NODE_MIC_SELECT node
                INIT_NODE (NODE_MIC_SELECT,
                           CurrentNode,
                           &KSNODETYPE_LOUDNESS,
                           &KSAUDFNAME_ALTERNATE_MICROPHONE,
                           &AutomationSpecial,
                           Index);
            }

            // add the NODE_MIC_BOOST node
            INIT_NODE (NODE_MIC_BOOST,
                       CurrentNode,
                       &KSNODETYPE_AGC,
                       &KSAUDFNAME_MICROPHONE_BOOST,
                       &AutomationSpecial,
                       Index);

            // add the NODE_MIC_VOLUME node
            INIT_NODE (NODE_MIC_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_MIC_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_MIC_MUTE node
            INIT_NODE (NODE_MIC_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &KSAUDFNAME_MIC_MUTE,
                       &AutomationMute,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_LINEIN_PRESENT))
        {
            // add the NODE_LINEIN_VOLUME node
            INIT_NODE (NODE_LINEIN_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_LINE_IN_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_LINEIN_MUTE node
            INIT_NODE (NODE_LINEIN_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &ICHFNAME_LINEIN_MUTE,
                       &AutomationMute,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_CD_PRESENT))
        {
            // add the NODE_CD_VOLUME node
            INIT_NODE (NODE_CD_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_CD_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_CD_MUTE node
            INIT_NODE (NODE_CD_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &KSAUDFNAME_CD_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the VIDEO nodes
        if (AdapterCommon->GetPinConfig (PINC_VIDEO_PRESENT))
        {
            // add the NODE_VIDEO_VOLUME node
            INIT_NODE (NODE_VIDEO_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_VIDEO_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_VIDEO_MUTE node
            INIT_NODE (NODE_VIDEO_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &KSAUDFNAME_VIDEO_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the AUX nodes
        if (AdapterCommon->GetPinConfig (PINC_AUX_PRESENT))
        {
            // add the NODE_AUX_VOLUME node
            INIT_NODE (NODE_AUX_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_AUX_VOLUME,
                       &AutomationVolume,
                       Index);

            // add the NODE_AUX_MUTE node
            INIT_NODE (NODE_AUX_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &KSAUDFNAME_AUX_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the NODE_MAIN_MIX node
        INIT_NODE (NODE_MAIN_MIX,
                   CurrentNode,
                   &KSNODETYPE_SUM,
                   &ICHFNAME_MAIN_MIX,
                   NULL,
                   Index);

        // add the 3D nodes
        if (AdapterCommon->GetNodeConfig (NODEC_3D_PRESENT))
        {
            if (AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE))
            {
                // add the NODE_VIRT_3D_CENTER node
                INIT_NODE (NODE_VIRT_3D_CENTER,
                           CurrentNode,
                           &KSNODETYPE_VOLUME,
                           &KSAUDFNAME_3D_CENTER,
                           &Automation3D,
                           Index);
            }

            if (AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE) ||
               (!AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE) &&
                !AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE)))
            {
                // add the NODE_VIRT_3D_DEPTH node
                INIT_NODE (NODE_VIRT_3D_DEPTH,
                           CurrentNode,
                           &KSNODETYPE_VOLUME,
                           &KSAUDFNAME_3D_DEPTH,
                           &Automation3D,
                           Index);
            }
            
            // add the NODE_VIRT_3D_ENABLE node
            INIT_NODE (NODE_VIRT_3D_ENABLE,
                       CurrentNode,
                       &KSNODETYPE_LOUDNESS,
                       &ICHFNAME_3D_ENABLE,
                       &AutomationSpecial,
                       Index);

            // add the NODE_VIRT_WAVEOUT_3D_BYPASS node
            INIT_NODE (NODE_VIRT_WAVEOUT_3D_BYPASS,
                       CurrentNode,
                       &KSNODETYPE_AGC,
                       &ICHFNAME_WAVEOUT_3D_BYPASS,
                       &AutomationSpecial,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_PCBEEP_PRESENT) ||
            AdapterCommon->GetPinConfig (PINC_PHONE_PRESENT))
        {
            // add the NODE_BEEP_MIX node
            INIT_NODE (NODE_BEEP_MIX,
                       CurrentNode,
                       &KSNODETYPE_SUM,
                       &ICHFNAME_BEEP_MIX,
                       NULL,
                       Index);
        }

        // add the tone nodes
        if (AdapterCommon->GetNodeConfig (NODEC_TONE_PRESENT))
        {
            // add the NODE_BASS node
            INIT_NODE (NODE_BASS,
                       CurrentNode,
                       &KSNODETYPE_TONE,
                       &KSAUDFNAME_BASS,
                       &AutomationTone,
                       Index);

            // add the NODE_TREBLE node
            INIT_NODE (NODE_TREBLE,
                       CurrentNode,
                       &KSNODETYPE_TONE,
                       &KSAUDFNAME_TREBLE,
                       &AutomationTone,
                       Index);

            if (AdapterCommon->GetNodeConfig (NODEC_LOUDNESS_PRESENT))
            {
                // add the NODE_LOUDNESS node
                INIT_NODE (NODE_LOUDNESS,
                           CurrentNode,
                           &KSNODETYPE_LOUDNESS,
                           NULL,
                           &AutomationSpecial,
                           Index);
            }
            
            if (AdapterCommon->GetNodeConfig (NODEC_SIMUL_STEREO_PRESENT))
            {
                // add the NODE_SIMUL_STEREO node
                INIT_NODE (NODE_SIMUL_STEREO,
                           CurrentNode,
                           &KSNODETYPE_AGC,
                           &ICHFNAME_SIMUL_STEREO,
                           &AutomationSpecial,
                           Index);
            }
        }

        // add the NODE_MASTEROUT_VOLUME node
        INIT_NODE (NODE_MASTEROUT_VOLUME,
                   CurrentNode,
                   &KSNODETYPE_VOLUME,
                   &KSAUDFNAME_MASTER_VOLUME,
                   &AutomationVolume,
                   Index);

        // add the NODE_MASTEROUT_MUTE node
        INIT_NODE (NODE_MASTEROUT_MUTE,
                   CurrentNode,
                   &KSNODETYPE_MUTE,
                   &KSAUDFNAME_MASTER_MUTE,
                   &AutomationMute,
                   Index);

        // add the HPOUT nodes
        if (AdapterCommon->GetPinConfig (PINC_HPOUT_PRESENT))
        {
            // add the NODE_HPOUT_VOLUME node
            INIT_NODE (NODE_HPOUT_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &ICHFNAME_HPOUT_VOLUME,
                       &AutomationVolume,
                       Index);
    
            // add the NODE_HPOUT_MUTE node
            INIT_NODE (NODE_HPOUT_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &ICHFNAME_HPOUT_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the NODE_WAVEIN_SELECT node
        INIT_NODE (NODE_WAVEIN_SELECT,
                   CurrentNode,
                   &KSNODETYPE_MUX,
                   &ICHFNAME_WAVEIN_SELECT,
                   &AutomationMux,
                   Index);

        if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT))
        {
            // add the NODE_VIRT_MASTER_INPUT_VOLUME1 node
            INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME1,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_MIC_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_CD_PRESENT))
        {
            // add the NODE_VIRT_MASTER_INPUT_VOLUME2 node
            INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME2,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_CD_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_VIDEO_PRESENT))
        {
            // add the NODE_VIRT_MASTER_INPUT_VOLUME3 node
            INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME3,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_VIDEO_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_AUX_PRESENT))
        {
            // add the NODE_VIRT_MASTER_INPUT_VOLUME4 node
            INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME4,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_AUX_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        if (AdapterCommon->GetPinConfig (PINC_LINEIN_PRESENT))
        {
            // add the NODE_VIRT_MASTER_INPUT_VOLUME5 node
            INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME5,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_LINE_IN_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        // add the NODE_VIRT_MASTER_INPUT_VOLUME6 node
        INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME6,
                   CurrentNode,
                   &KSNODETYPE_VOLUME,
                   &KSAUDFNAME_STEREO_MIX_VOLUME,
                   &AutomationVolume,
                   Index);

        // add the NODE_VIRT_MASTER_INPUT_VOLUME7 node
        INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME7,
                   CurrentNode,
                   &KSNODETYPE_VOLUME,
                   &KSAUDFNAME_MONO_MIX_VOLUME,
                   &AutomationVolume,
                   Index);

        if (AdapterCommon->GetPinConfig (PINC_PHONE_PRESENT))
        {
            // add the NODE_VIRT_MASTER_INPUT_VOLUME8 node
            INIT_NODE (NODE_VIRT_MASTER_INPUT_VOLUME8,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &ICHFNAME_MASTER_INPUT_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        // add the MICIN nodes
        if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT) &&
            AdapterCommon->GetPinConfig (PINC_MICIN_PRESENT))
        {
            // add the NODE_MICIN_VOLUME node
            INIT_NODE (NODE_MICIN_VOLUME,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &ICHFNAME_MICIN_VOLUME,
                       &AutomationVolume,
                       Index);
    
            // add the NODE_MICIN_MUTE node
            INIT_NODE (NODE_MICIN_MUTE,
                       CurrentNode,
                       &KSNODETYPE_MUTE,
                       &ICHFNAME_MICIN_MUTE,
                       &AutomationMute,
                       Index);
        }

        // add the MONOOUT nodes
        if (AdapterCommon->GetPinConfig (PINC_MONOOUT_PRESENT))
        {
            // add the NODE_MONOOUT_SELECT node
            INIT_NODE (NODE_MONOOUT_SELECT,
                       CurrentNode,
                       &KSNODETYPE_MUX,
                       &ICHFNAME_MONOOUT_SELECT,
                       &AutomationMux,
                       Index);

            // add the NODE_VIRT_MONOOUT_VOLUME1 node
            INIT_NODE (NODE_VIRT_MONOOUT_VOLUME1,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_MONO_MIX_VOLUME,
                       &AutomationVolume,
                       Index);
    
            // add the NODE_VIRT_MONOOUT_VOLUME2 node
            INIT_NODE (NODE_VIRT_MONOOUT_VOLUME2,
                       CurrentNode,
                       &KSNODETYPE_VOLUME,
                       &KSAUDFNAME_MIC_VOLUME,
                       &AutomationVolume,
                       Index);
        }

        // add the nodes to the filter descriptor
        FilterDescriptor->NodeCount = Index;
        FilterDescriptor->NodeSize = sizeof(PCNODE_DESCRIPTOR);
        FilterDescriptor->Nodes = NodeDescriptors;
    }
    else
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;

#undef INIT_NODE
}

/*****************************************************************************
 * CMiniportTopologyICH::BuildConnectionDescriptors
 *****************************************************************************
 * Builds the topology connection descriptors.
 */
NTSTATUS CMiniportTopologyICH::BuildConnectionDescriptors (void)
{
// Improvement would be to not use a Macro, use (inline) function instead.

// for node to node connections
#define INIT_NN_CONN( cptr, fnode, fpin, tnode, tpin )  \
    cptr->FromNode = TransNodeDefToNodeNr (fnode);      \
    cptr->FromNodePin = fpin;                           \
    cptr->ToNode = TransNodeDefToNodeNr (tnode);        \
    cptr->ToNodePin = tpin;                             \
    cptr++,ConnectionCount++

// for filter pin to node connections
#define INIT_FN_CONN( cptr, fpin, tnode, tpin )         \
    cptr->FromNode = KSFILTER_NODE;                     \
    cptr->FromNodePin = TransPinDefToPinNr (fpin);      \
    cptr->ToNode = TransNodeDefToNodeNr (tnode);        \
    cptr->ToNodePin = tpin;                             \
    cptr++,ConnectionCount++

// for node to filter pin connections
#define INIT_NF_CONN( cptr, fnode, fpin, tpin )         \
    cptr->FromNode = TransNodeDefToNodeNr (fnode);      \
    cptr->FromNodePin = fpin;                           \
    cptr->ToNode = KSFILTER_NODE;                       \
    cptr->ToNodePin = TransPinDefToPinNr (tpin);        \
    cptr++,ConnectionCount++

    PAGED_CODE ();

    NTSTATUS    ntStatus            = STATUS_SUCCESS;
    ULONG       ConnectionCount     = 0;

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::BuildConnectionDescriptors]"));
    
    // allocate our descriptor memory
    ConnectionDescriptors = PPCCONNECTION_DESCRIPTOR (ExAllocatePool (PagedPool,
                            TOPO_MAX_CONNECTIONS * sizeof(PCCONNECTION_DESCRIPTOR)));
    if (ConnectionDescriptors)
    {
        PPCCONNECTION_DESCRIPTOR  CurrentConnection = ConnectionDescriptors;

        // build the wave out (coming in) path

        // PIN_WAVEOUT_SOURCE -> NODE_WAVEOUT_VOLUME
        INIT_FN_CONN (CurrentConnection, PIN_WAVEOUT_SOURCE, NODE_WAVEOUT_VOLUME, 1);

        // NODE_WAVEOUT_VOLUME -> NODE_WAVEOUT_MUTE
        INIT_NN_CONN (CurrentConnection, NODE_WAVEOUT_VOLUME, 0, NODE_WAVEOUT_MUTE, 1);

        // NODE_WAVEOUT_MUTE -> NODE_MAIN_MIX
        INIT_NN_CONN (CurrentConnection, NODE_WAVEOUT_MUTE, 0, NODE_MAIN_MIX, 1);

        // build the PC beeper path
        if (AdapterCommon->GetPinConfig (PINC_PCBEEP_PRESENT))
        {
            // PIN_PCBEEP_SOURCE -> NODE_PCBEEP_VOLUME
            INIT_FN_CONN (CurrentConnection, PIN_PCBEEP_SOURCE, NODE_PCBEEP_VOLUME, 1);
            
            // NODE_PCBEEP_VOLUME -> NODE_PCBEEP_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_PCBEEP_VOLUME, 0, NODE_PCBEEP_MUTE, 1);

            // NODE_PCBEEP_MUTE -> NODE_BEEP_MIX
            INIT_NN_CONN (CurrentConnection, NODE_PCBEEP_MUTE, 0, NODE_BEEP_MIX, 2);
        }

        // build the phone path
        if (AdapterCommon->GetPinConfig (PINC_PHONE_PRESENT))
        {
            // PIN_PHONE_SOURCE -> NODE_PHONE_VOLUME
            INIT_FN_CONN (CurrentConnection, PIN_PHONE_SOURCE, NODE_PHONE_VOLUME, 1);
            
            // NODE_PHONE_VOLUME -> NODE_PHONE_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_PHONE_VOLUME, 0, NODE_PHONE_MUTE, 1);

            // NODE_PHONE_MUTE -> LINEOUT_BEEP_MIX
            INIT_NN_CONN (CurrentConnection, NODE_PHONE_MUTE, 0, NODE_BEEP_MIX, 3);
            
            // PIN_PHONE_SOURCE pin -> NODE_VIRT_MASTER_INPUT_VOLUME8
            INIT_FN_CONN (CurrentConnection, PIN_PHONE_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME8, 1);

            // NODE_VIRT_MASTER_INPUT_VOLUME8 -> NODE_WAVEIN_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME8, 0, NODE_WAVEIN_SELECT, 8);
        }

        // build MIC path
        if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT))
        {
            // build the MIC selector in case we have 2 MICs
            if (AdapterCommon->GetPinConfig (PINC_MIC2_PRESENT))
            {
                // PIN_MIC_SOURCE pin -> NODE_MIC_SELECT
                INIT_FN_CONN (CurrentConnection, PIN_MIC_SOURCE, NODE_MIC_SELECT, 1);

                // NODE_MIC_SELECT -> NODE_MIC_BOOST
                INIT_NN_CONN (CurrentConnection, NODE_MIC_SELECT, 0, NODE_MIC_BOOST, 1);
            }
            else
            {
                // PIN_MIC_SOURCE pin -> NODE_MIC_SELECT
                INIT_FN_CONN (CurrentConnection, PIN_MIC_SOURCE, NODE_MIC_BOOST, 1);
            }

            // NODE_MIC_BOOST -> NODE_MIC_VOLUME
            INIT_NN_CONN (CurrentConnection, NODE_MIC_BOOST, 0, NODE_MIC_VOLUME, 1);

            // NODE_MIC_VOLUME -> NODE_MIC_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_MIC_VOLUME, 0, NODE_MIC_MUTE, 1);

            // NODE_MIC_MUTE -> NODE_MAIN_MIX
            INIT_NN_CONN (CurrentConnection, NODE_MIC_MUTE, 0, NODE_MAIN_MIX, 2);

            // NODE_MIC_BOOST -> NODE_VIRT_MASTER_INPUT_VOLUME1
            INIT_NN_CONN (CurrentConnection, NODE_MIC_BOOST, 0, NODE_VIRT_MASTER_INPUT_VOLUME1, 1);

            // NODE_VIRT_MASTER_INPUT_VOLUME1 -> NODE_WAVEIN_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME1, 0, NODE_WAVEIN_SELECT, 1);
        }

        // build the line in path
        if (AdapterCommon->GetPinConfig (PINC_LINEIN_PRESENT))
        {
            // PIN_LINEIN_SOURCE -> NODE_LINEIN_VOLUME
            INIT_FN_CONN (CurrentConnection, PIN_LINEIN_SOURCE, NODE_LINEIN_VOLUME, 1);

            // NODE_LINEIN_VOLUME -> NODE_LINEIN_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_LINEIN_VOLUME, 0, NODE_LINEIN_MUTE, 1);

            // NODE_LINEIN_MUTE -> NODE_MAIN_MIX
            INIT_NN_CONN (CurrentConnection, NODE_LINEIN_MUTE, 0, NODE_MAIN_MIX, 3);

            // PIN_LINEIN_SOURCE pin -> NODE_VIRT_MASTER_INPUT_VOLUME5
            INIT_FN_CONN (CurrentConnection, PIN_LINEIN_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME5, 1);

            // NODE_VIRT_MASTER_INPUT_VOLUME5 -> NODE_WAVEIN_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME5, 0, NODE_WAVEIN_SELECT, 5);
        }

        // build the CD path
        if (AdapterCommon->GetPinConfig (PINC_CD_PRESENT))
        {
            // PIN_CD_SOURCE -> NODE_CD_VOLUME
            INIT_FN_CONN (CurrentConnection, PIN_CD_SOURCE, NODE_CD_VOLUME, 1);

            // NODE_CD_VOLUME -> NODE_CD_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_CD_VOLUME, 0, NODE_CD_MUTE, 1);

            // NODE_CD_MUTE -> NODE_MAIN_MIX
            INIT_NN_CONN (CurrentConnection, NODE_CD_MUTE, 0, NODE_MAIN_MIX, 4);

            // PIN_CD_SOURCE pin -> NODE_VIRT_MASTER_INPUT_VOLUME2
            INIT_FN_CONN (CurrentConnection, PIN_CD_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME2, 1);

            // NODE_VIRT_MASTER_INPUT_VOLUME2 -> NODE_WAVEIN_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME2, 0, NODE_WAVEIN_SELECT, 2);
        }

        // build the video path
        if (AdapterCommon->GetPinConfig (PINC_VIDEO_PRESENT))
        {
            // PIN_VIDEO_SOURCE -> NODE_VIDEO_VOLUME
            INIT_FN_CONN (CurrentConnection, PIN_VIDEO_SOURCE, NODE_VIDEO_VOLUME, 1);
    
            // NODE_VIDEO_VOLUME -> NODE_VIDEO_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_VIDEO_VOLUME, 0, NODE_VIDEO_MUTE, 1);
    
            // NODE_VIDEO_MUTE -> NODE_MAIN_MIX
            INIT_NN_CONN (CurrentConnection, NODE_VIDEO_MUTE, 0, NODE_MAIN_MIX, 5);
            
            // PIN_VIDEO_SOURCE pin -> NODE_VIRT_MASTER_INPUT_VOLUME3
            INIT_FN_CONN (CurrentConnection, PIN_VIDEO_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME3, 1);

            // NODE_VIRT_MASTER_INPUT_VOLUME3 -> NODE_WAVEIN_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME3, 0, NODE_WAVEIN_SELECT, 3);
        }

        // build the AUX path
        if (AdapterCommon->GetPinConfig (PINC_AUX_PRESENT))
        {
            // PIN_AUX_SOURCE pin -> NODE_AUX_VOLUME
            INIT_FN_CONN (CurrentConnection, PIN_AUX_SOURCE, NODE_AUX_VOLUME, 1);
    
            // NODE_AUX_VOLUME -> NODE_AUX_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_AUX_VOLUME, 0, NODE_AUX_MUTE, 1);
    
            // NODE_AUX_MUTE -> NODE_MAIN_MIX
            INIT_NN_CONN (CurrentConnection, NODE_AUX_MUTE, 0, NODE_MAIN_MIX, 6);
            
            // PIN_AUX_SOURCE pin -> NODE_VIRT_MASTER_INPUT_VOLUME4
            INIT_FN_CONN (CurrentConnection, PIN_AUX_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME4, 1);

            // NODE_VIRT_MASTER_INPUT_VOLUME4 -> NODE_WAVEIN_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME4, 0, NODE_WAVEIN_SELECT, 4);
        }
        
        // helper node variable.
        TopoNodes     ConnectFromNode;

        // build the 3D path
        if (AdapterCommon->GetNodeConfig (NODEC_3D_PRESENT))
        {
            // Figure out what the main 3D line is.
            if (AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE))
            {
                if (AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE))
                {
                    // PIN_VIRT_3D_DEPTH_SOURCE -> NODE_VIRT_3D_ENABLE
                    INIT_FN_CONN (CurrentConnection, PIN_VIRT_3D_DEPTH_SOURCE, NODE_VIRT_3D_ENABLE, 1);

                    // NODE_VIRT_3D_ENABLE -> NODE_VIRT_WAVEOUT_3D_BYPASS
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_ENABLE, 0, NODE_VIRT_WAVEOUT_3D_BYPASS, 1);
                    
                    // NODE_VIRT_WAVEOUT_3D_BYPASS -> NODE_VIRT_3D_DEPTH
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_WAVEOUT_3D_BYPASS, 0, NODE_VIRT_3D_DEPTH, 1);
                                  
                    // NODE_VIRT_3D_DEPTH -> NODE_MAIN_MIX              
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_DEPTH, 0, NODE_MAIN_MIX, 7);

                    // PIN_VIRT_3D_CENTER_SOURCE -> NODE_VIRT_3D_CENTER
                    INIT_FN_CONN (CurrentConnection, PIN_VIRT_3D_CENTER_SOURCE, NODE_VIRT_3D_CENTER, 1);
                    
                    // NODE_VIRT_3D_CENTER -> NODE_MAIN_MIX              
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_CENTER, 0, NODE_MAIN_MIX, 8);
                }
                else
                {
                    // PIN_VIRT_3D_CENTER_SOURCE -> NODE_VIRT_3D_ENABLE
                    INIT_FN_CONN (CurrentConnection, PIN_VIRT_3D_CENTER_SOURCE, NODE_VIRT_3D_ENABLE, 1);

                    // NODE_VIRT_3D_ENABLE -> NODE_VIRT_WAVEOUT_3D_BYPASS
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_ENABLE, 0, NODE_VIRT_WAVEOUT_3D_BYPASS, 1);
                    
                    // NODE_VIRT_WAVEOUT_3D_BYPASS -> NODE_VIRT_3D_CENTER
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_WAVEOUT_3D_BYPASS, 0, NODE_VIRT_3D_CENTER, 1);
                                  
                    // NODE_VIRT_3D_CENTER -> NODE_MAIN_MIX              
                    INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_CENTER, 0, NODE_MAIN_MIX, 8);
                }
            }
            else
            {
                // PIN_VIRT_3D_DEPTH_SOURCE -> NODE_VIRT_3D_ENABLE
                INIT_FN_CONN (CurrentConnection, PIN_VIRT_3D_DEPTH_SOURCE, NODE_VIRT_3D_ENABLE, 1);

                // NODE_VIRT_3D_ENABLE -> NODE_VIRT_WAVEOUT_3D_BYPASS
                INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_ENABLE, 0, NODE_VIRT_WAVEOUT_3D_BYPASS, 1);

                // NODE_VIRT_WAVEOUT_3D_BYPASS -> NODE_VIRT_3D_DEPTH
                INIT_NN_CONN (CurrentConnection, NODE_VIRT_WAVEOUT_3D_BYPASS, 0, NODE_VIRT_3D_DEPTH, 1);

                // NODE_VIRT_3D_DEPTH -> NODE_MAIN_MIX              
                INIT_NN_CONN (CurrentConnection, NODE_VIRT_3D_DEPTH, 0, NODE_MAIN_MIX, 7);
            }
        }

        // all connections go from this one
        ConnectFromNode = NODE_MAIN_MIX;

        // build the beeper & phone mix
        if (AdapterCommon->GetPinConfig (PINC_PCBEEP_PRESENT) ||
            AdapterCommon->GetPinConfig (PINC_PHONE_PRESENT))
        {
            // last node -> NODE_BEEP_MIX
            INIT_NN_CONN (CurrentConnection, ConnectFromNode, 0, NODE_BEEP_MIX, 1);

            // next connection from this point.
            ConnectFromNode = NODE_BEEP_MIX;
        }

        // build the tone control path
        if (AdapterCommon->GetNodeConfig (NODEC_TONE_PRESENT))
        {
            // last node -> NODE_BASS
            INIT_NN_CONN (CurrentConnection, ConnectFromNode, 0, NODE_BASS, 1);

            // NODE_BASS -> NODE_TREBLE
            INIT_NN_CONN (CurrentConnection, NODE_BASS, 0, NODE_TREBLE, 1);

            // remember the last node
            ConnectFromNode = NODE_TREBLE;

            // build the loudness control
            if (AdapterCommon->GetNodeConfig (NODEC_LOUDNESS_PRESENT))
            {
                // last node -> NODE_LOUDNESS
                INIT_NN_CONN (CurrentConnection, ConnectFromNode, 0, NODE_LOUDNESS, 1);

                // remember the last node
                ConnectFromNode = NODE_LOUDNESS;
            }
            
            // build the simulated stereo control
            if (AdapterCommon->GetNodeConfig (NODEC_SIMUL_STEREO_PRESENT))
            {
                // last node -> NODE_SIMUL_STEREO
                INIT_NN_CONN (CurrentConnection, ConnectFromNode, 0, NODE_SIMUL_STEREO, 1);

                // remember the last node
                ConnectFromNode = NODE_SIMUL_STEREO;
            }
        }

        //build the master volume output.

        // build the connection from whatever to NODE_MASTEROUT_VOLUME
        INIT_NN_CONN (CurrentConnection, ConnectFromNode, 0, NODE_MASTEROUT_VOLUME, 1);

        // NODE_MASTEROUT_VOLUME -> NODE_MASTEROUT_MUTE
        INIT_NN_CONN (CurrentConnection, NODE_MASTEROUT_VOLUME, 0, NODE_MASTEROUT_MUTE, 1);

        // NODE_MASTEROUT_MUTE -> PIN_MASTEROUT_DEST pin
        INIT_NF_CONN( CurrentConnection, NODE_MASTEROUT_MUTE, 0, PIN_MASTEROUT_DEST);

        // and build the head phone output
        if (AdapterCommon->GetPinConfig (PINC_HPOUT_PRESENT))
        {
            // from whatever -> NODE_HPOUT_VOLUME
            INIT_NN_CONN (CurrentConnection, ConnectFromNode, 0, NODE_HPOUT_VOLUME, 1);

            // NODE_HPOUT_VOLUME -> NODE_HPOUT_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_HPOUT_VOLUME, 0, NODE_HPOUT_MUTE, 1);

            // NODE_HPOUT_MUTE -> PIN_HPOUT_DEST pin
            INIT_NF_CONN( CurrentConnection, NODE_HPOUT_MUTE, 0, PIN_HPOUT_DEST);
        }

        // now complete the input muxer path

        // PIN_VIRT_TONE_MIX_MONO_SOURCE -> NODE_VIRT_MASTER_INPUT_VOLUME7
        INIT_FN_CONN (CurrentConnection, PIN_VIRT_TONE_MIX_MONO_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME7, 1);

        // NODE_VIRT_MASTER_INPUT_VOLUME7 -> NODE_WAVEIN_SELECT
        INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME7, 0, NODE_WAVEIN_SELECT, 7);

        // PIN_VIRT_TONE_MIX_SOURCE -> NODE_VIRT_MASTER_INPUT_VOLUME6
        INIT_FN_CONN (CurrentConnection, PIN_VIRT_TONE_MIX_SOURCE, NODE_VIRT_MASTER_INPUT_VOLUME6, 1);

        // NODE_VIRT_MASTER_INPUT_VOLUME6 -> NODE_WAVEIN_SELECT
        INIT_NN_CONN (CurrentConnection, NODE_VIRT_MASTER_INPUT_VOLUME6, 0, NODE_WAVEIN_SELECT, 6);

        // NODE_WAVEIN_SELECT -> PIN_WAVEIN_DEST
        INIT_NF_CONN( CurrentConnection, NODE_WAVEIN_SELECT, 0, PIN_WAVEIN_DEST);

        // build the mic output path (for record control)
        if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT) &&
            AdapterCommon->GetPinConfig (PINC_MICIN_PRESENT))
        {
            // NODE_MIC_BOOST -> NODE_MICIN_VOLUME
            INIT_NN_CONN (CurrentConnection, NODE_MIC_BOOST, 0, NODE_MICIN_VOLUME, 1);

            // NODE_MICIN_VOLUME -> NODE_MICIN_MUTE
            INIT_NN_CONN (CurrentConnection, NODE_MICIN_VOLUME, 0, NODE_MICIN_MUTE, 1);

            // NODE_MICIN_MUTE -> PIN_MICIN_DEST
            INIT_NF_CONN( CurrentConnection, NODE_MICIN_MUTE, 0, PIN_MICIN_DEST);
        }

        // build the mono path
        if (AdapterCommon->GetPinConfig (PINC_MONOOUT_PRESENT))
        {
            // PIN_VIRT_3D_MIX_MONO_SOURCE -> NODE_MONOOUT_SMIX
            INIT_FN_CONN (CurrentConnection, PIN_VIRT_3D_MIX_MONO_SOURCE, NODE_VIRT_MONOOUT_VOLUME1, 1);

            // NODE_VIRT_MONOOUT_VOLUME1 -> NODE_MONOOUT_SELECT
            INIT_NN_CONN (CurrentConnection, NODE_VIRT_MONOOUT_VOLUME1, 0, NODE_MONOOUT_SELECT, 1);

            if (AdapterCommon->GetPinConfig (PINC_MIC_PRESENT))
            {
                // NODE_MIC_BOOST -> NODE_VIRT_MONOOUT_VOLUME2
                INIT_NN_CONN (CurrentConnection, NODE_MIC_BOOST, 0, NODE_VIRT_MONOOUT_VOLUME2, 1);

                // NODE_VIRT_MONOOUT_VOLUME2 -> NODE_MONOOUT_SELECT
                INIT_NN_CONN (CurrentConnection, NODE_VIRT_MONOOUT_VOLUME2, 0, NODE_MONOOUT_SELECT, 2);
            }

            // NODE_MONOOUT_SELECT -> PIN_MONOOUT_DEST
            INIT_NF_CONN( CurrentConnection, NODE_MONOOUT_SELECT, 0, PIN_MONOOUT_DEST);
        }

        // add the connections to the filter descriptor
        FilterDescriptor->ConnectionCount = ConnectionCount;
        FilterDescriptor->Connections = ConnectionDescriptors;
    } else
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;

#undef INIT_NN_CONN
#undef INIT_FN_CONN
#undef INIT_NF_CONN
}


/*****************************************************************************
 * CMiniportTopologyICH::GetPhysicalConnectionPins
 *****************************************************************************
 * Returns the system pin IDs of the bridge pins that are connected with the
 * wave miniport.
 * If one pin is not used, the value is -1, that could only happen for MinInDest.
 */
STDMETHODIMP CMiniportTopologyICH::GetPhysicalConnectionPins
(
    OUT PULONG  WaveOutSource,
    OUT PULONG  WaveInDest,
    OUT PULONG  MicInDest
)
{
    PAGED_CODE ();

    ASSERT (WaveOutSource);
    ASSERT (WaveInDest);
    ASSERT (MicInDest);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::GetPhysicalConnectionPins]"));

    // set the pin IDs.
    *WaveOutSource = TransPinDefToPinNr (PIN_WAVEOUT_SOURCE);
    *WaveInDest = TransPinDefToPinNr (PIN_WAVEIN_DEST);
    // this is optional
    if (AdapterCommon->GetPinConfig (PINC_MICIN_PRESENT))
        *MicInDest = TransPinDefToPinNr (PIN_MICIN_DEST);
    else
        *MicInDest = -1;

    return STATUS_SUCCESS;
}
