/*****************************************************************************
 * minwave.h - HDA wave miniport private definitions
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation. All Rights Reserved.
 */

#ifndef _HDAWAVE_PRIVATE_H_
#define _HDAWAVE_PRIVATE_H_

#include "stdunk.h"
#include "common.h"




 
/*****************************************************************************
 * Classes
 */

/*****************************************************************************
 * CMiniportWaveCyclicHDA
 *****************************************************************************
 * HDA wave miniport.  This object is associated with the device and is
 * created when the device is started.  The class inherits IMiniportWaveCyclic
 * so it can expose this interface and CUnknown so it automatically gets
 * reference counting and aggregation support.
 */
class CMiniportWaveCyclicHDA
:   public IMiniportWaveCyclic,
    public CUnknown
{
private:
    PADAPTERCOMMON      AdapterCommon;              // Adapter common object.
    PPORTWAVECYCLIC     Port;                       // Callback interface.
    PUCHAR              PortBase;                   // Base port address.

    ULONG               NotificationInterval;       // In milliseconds.
    ULONG               SamplingFrequency;          // Frames per second.

    BOOLEAN             AllocatedCapture;           // Capture in use.
    BOOLEAN             AllocatedRender;            // Render in use.
    BOOLEAN             Allocated8Bit;              // 8-bit DMA in use.
    BOOLEAN             Allocated16Bit;             // 16-bit DMA in use.

	PDMACHANNEL         DmaChannel; 
	PADAPTER_OBJECT     AdapterObject;

    PSERVICEGROUP       ServiceGroup;               // For notification.
    KMUTEX              SampleRateSync;             // Sync for sample rate changes.

    /*************************************************************************
     * CMiniportWaveCyclicHDA methods
     *
     * These are private member functions used internally by the object.  See
     * MINIPORT.CPP for specific descriptions.
     */
    BOOLEAN ConfigureDevice
    (
        IN      ULONG   Interrupt,
        IN      ULONG   DMA8Bit,
        IN      ULONG   DMA16Bit
    );
    NTSTATUS ProcessResources
    (
        IN      PRESOURCELIST   ResourceList
    );
    NTSTATUS ValidateFormat
    (
        IN      PKSDATAFORMAT   Format
    );

public:
    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclicHDA);

    ~CMiniportWaveCyclicHDA();

    /*************************************************************************
     * This macro is from PORTCLS.H.  It lists all the interface's functions.
     */
    IMP_IMiniportWaveCyclic;

    /*************************************************************************
     * Friends
     *
     * The miniport stream class is a friend because it needs to access the
     * private member variables of this class.
     */
    friend class CMiniportWaveCyclicStreamHDA;
};

/*****************************************************************************
 * CMiniportWaveCyclicStreamHDA
 *****************************************************************************
 * HDA wave miniport stream.  This object is associated with a streaming pin
 * and is created when a pin is created on the filter.  The class inherits
 * IMiniportWaveCyclicStream so it can expose this interface and CUnknown so
 * it automatically gets reference counting and aggregation support.
 */
class CMiniportWaveCyclicStreamHDA
:   public IMiniportWaveCyclicStream,
    public CUnknown
{
private:
    CMiniportWaveCyclicHDA *   Miniport;       // Miniport that created us.
    ULONG                       Channel;        // Index into channel list.
    BOOLEAN                     Capture;        // Capture or render.
    BOOLEAN                     Format16Bit;    // 16- or 8-bit samples.
    BOOLEAN                     FormatStereo;   // Two or one channel.
    KSSTATE                     State;          // Stop, pause, run.
    PDMACHANNEL		            DmaChannel;     // DMA channel to use.
    BOOLEAN                     RestoreInputMixer;  // Restore input mixer.
    UCHAR                       InputMixerLeft; // Cache for left input mixer.

public:
    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclicStreamHDA);

    ~CMiniportWaveCyclicStreamHDA();

    /*************************************************************************
     * This macro is from PORTCLS.H.  It lists all the interface's functions.
     */
    IMP_IMiniportWaveCyclicStream;

    NTSTATUS 
    Init
    (
        IN      CMiniportWaveCyclicHDA *   Miniport,
        IN      ULONG                       Channel,
        IN      BOOLEAN                     Capture,
        IN      PKSDATAFORMAT               DataFormat,
        OUT     PDMACHANNEL		            DmaChannel
    );
};





#endif