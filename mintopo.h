/*****************************************************************************
 * mintopo.h - HDA topology miniport private definitions
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation. All Rights Reserved.
 */

#ifndef _HDATOPO_PRIVATE_H_
#define _HDATOPO_PRIVATE_H_

#include "stdunk.h"
#include "common.h"

#define ELTS_IN_ARRAY(a) (sizeof(a) / sizeof((a)[0]))





/*****************************************************************************
 * Classes
 */

/*****************************************************************************
 * CMiniportTopologyHDA
 *****************************************************************************
 * HDA topology miniport.  This object is associated with the device and is
 * created when the device is started.  The class inherits IMiniportTopology
 * so it can expose this interface and CUnknown so it automatically gets
 * reference counting and aggregation support.
 */
class CMiniportTopologyHDA 
:   public IMiniportTopology, 
    public CUnknown
{
private:
    PADAPTERCOMMON      AdapterCommon;      // Adapter common object.
    PPORTTOPOLOGY       Port;               // Callback interface.
    PUCHAR              PortBase;           // Base port address.

    BOOLEAN             BoardNotResponsive; // Indicates dead hardware.

    /*************************************************************************
     * CMiniportTopologyHDA methods
     *
     * These are private member functions used internally by the object.  See
     * MINIPORT.CPP for specific descriptions.
     */
    NTSTATUS ProcessResources
    (
        IN      PRESOURCELIST   ResourceList
    );
    BYTE ReadBitsFromMixer
    (
        BYTE Reg,
        BYTE Bits,
        BYTE Shift
    );
    void WriteBitsToMixer
    (
        BYTE Reg,
        BYTE Bits,
        BYTE Shift,
        BYTE Value
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
    DEFINE_STD_CONSTRUCTOR(CMiniportTopologyHDA);

    ~CMiniportTopologyHDA();

    /*************************************************************************
     * IMiniport methods
     */
    STDMETHODIMP 
    GetDescription
    (   OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
    );
    STDMETHODIMP 
    DataRangeIntersection
    (   IN      ULONG           PinId
    ,   IN      PKSDATARANGE    DataRange
    ,   IN      PKSDATARANGE    MatchingDataRange
    ,   IN      ULONG           OutputBufferLength
    ,   OUT     PVOID           ResultantFormat     OPTIONAL
    ,   OUT     PULONG          ResultantFormatLength
    )
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    /*************************************************************************
     * IMiniportTopology methods
     */
    STDMETHODIMP Init
    (
        IN      PUNKNOWN        UnknownAdapter,
        IN      PRESOURCELIST   ResourceList,
        IN      PPORTTOPOLOGY   Port
    );

    /*************************************************************************
     * Friends
     */
    friend
    NTSTATUS
    PropertyHandler_OnOff
    (
        IN      PPCPROPERTY_REQUEST PropertyRequest
    );
    friend
    NTSTATUS
    PropertyHandler_Level
    (
        IN      PPCPROPERTY_REQUEST PropertyRequest
    );
    friend
    NTSTATUS
    PropertyHandler_SuperMixCaps
    (
        IN      PPCPROPERTY_REQUEST PropertyRequest
    );
    friend
    NTSTATUS
    PropertyHandler_SuperMixTable
    (
        IN      PPCPROPERTY_REQUEST PropertyRequest
    );
    friend
    NTSTATUS
    PropertyHandler_CpuResources
    (
        IN      PPCPROPERTY_REQUEST PropertyRequest
    );
};

#include "tables.h"





#endif
