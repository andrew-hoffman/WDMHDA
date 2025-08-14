/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

// Every debug output has "Modulname text"
static char STR_MODULENAME[] = "ICH Stream: ";

#include "minwave.h"
#include "ichwave.h"


#pragma code_seg("PAGE")
/*****************************************************************************
 * CreateMiniportWaveICHStream
 *****************************************************************************
 * Creates a wave miniport stream object for the ICH audio adapter. This is
 * (nearly) like the macro STD_CREATE_BODY_ from STDUNK.H.
 */
NTSTATUS CreateMiniportWaveICHStream
(
    OUT CMiniportWaveICHStream  **WaveIchStream,
    IN  PUNKNOWN                pUnknownOuter,
    IN  POOL_TYPE               PoolType
)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CreateMiniportWaveICHStream]"));

    //
    // This is basically like the macro at stdunk with the change that we
    // don't cast to interface unknown but to interface WaveIchStream.
    //
    *WaveIchStream = new (PoolType, 'rCcP')
                        CMiniportWaveICHStream (pUnknownOuter);
    if (*WaveIchStream)
    {
        (*WaveIchStream)->AddRef ();
        return STATUS_SUCCESS;
    }

    return STATUS_INSUFFICIENT_RESOURCES;
}
   

/*****************************************************************************
 * CMiniportWaveICHStream::~CMiniportWaveICHStream
 *****************************************************************************
 * Destructor
 */
CMiniportWaveICHStream::~CMiniportWaveICHStream ()
{
    PAGED_CODE ();


    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::~CMiniportWaveICHStream]"));

    //
    // Print information about the scatter gather list.
    //
    DOUT (DBG_DMA, ("Head %d, Tail %d, Tag counter %d, Entries %d.",
                   stBDList.nHead, stBDList.nTail, stBDList.nTagCounter, 
                   stBDList.nBDEntries));

    if (Wave)
    {
        //
        // Disable interrupts and stop DMA just in case.
        //
        if (Wave->AdapterCommon)
        {
            Wave->AdapterCommon->WriteBMControlRegister (m_ulBDAddr + X_CR, (UCHAR)0);
        }
        
        //
        // Remove stream from miniport Streams array.
        //
        if (Wave->Streams[Channel] == this)
        {
            Wave->Streams[Channel] = NULL;
        }
        
        //
        // Release the scatter/gather table.
        //
        if (stBDList.pBDEntry)
        {
            HalFreeCommonBuffer (Wave->AdapterObject,
                                 PAGE_SIZE,
                                 stBDList.PhysAddr,
                                 (PVOID)stBDList.pBDEntry,
                                 FALSE);
            stBDList.pBDEntry = NULL;
        }

        //
        // Release the miniport.
        //
        Wave->Release ();
        Wave = NULL;
    }

    //
    // Release the service group.
    //
    if (ServiceGroup)
    {
        ServiceGroup->Release ();
        ServiceGroup = NULL;
    }

    //
    // Release the mapping table.
    //
    if (stBDList.pMapData)
    {
        ExFreePool (stBDList.pMapData);
        stBDList.pMapData = NULL;
    }

    //
    // Release the port stream.
    //
    if (PortStream)
    {
        PortStream->Release ();
        PortStream = NULL;
    }
}


/*****************************************************************************
 * CMiniportWaveICHStream::Init
 *****************************************************************************
 * This routine initializes the stream object, sets up the BDL, and programs
 * the buffer descriptor list base address register for the pin being
 * initialized.
 */
NTSTATUS CMiniportWaveICHStream::Init
(
    IN  CMiniportWaveICH        *Miniport_,
    IN  PPORTWAVEPCISTREAM      PortStream_,
    IN  ULONG                   Channel_,
    IN  BOOLEAN                 Capture_,
    IN  PKSDATAFORMAT           DataFormat_,
    OUT PSERVICEGROUP           *ServiceGroup_
)
{
    PAGED_CODE ();

    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::Init]"));

    ASSERT (Miniport_);
    ASSERT (PortStream_);
    ASSERT (DataFormat_);
    ASSERT (ServiceGroup_);

    //
    // The rule here is that we return when we fail without a cleanup.
    // The destructor will relase the allocated memory.
    //
    NTSTATUS ntStatus = STATUS_SUCCESS;

    //
    // Save miniport pointer and addref it.
    //
    Wave = Miniport_;
    Wave->AddRef ();

    //
    // Save portstream interface pointer and addref it.
    //
    PortStream = PortStream_;
    PortStream->AddRef ();

    //
    // Save channel ID and capture flag.
    //
    Channel = Channel_;
    Capture = Capture_;

    //
    // Store the stream pointer, it is used by the ISR.
    //
    Wave->Streams[Channel] = this;

    //
    // Save data format and current sample rate.
    //
    DataFormat = (PKSDATAFORMAT_WAVEFORMATEX)DataFormat_;
    CurrentRate = DataFormat->WaveFormatEx.nSamplesPerSec;

    //
    // Initialize the BDL spinlock.
    //
    KeInitializeSpinLock (&MapLock);

    //
    // Create a service group (a DPC abstraction/helper) to help with 
    // interrupts.
    //
    ntStatus = PcNewServiceGroup (&ServiceGroup, NULL);
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Failed to create a service group!"));
        return ntStatus;
    }

    //
    // Pass the ServiceGroup pointer to portcls.
    //
    *ServiceGroup_ = ServiceGroup;
    ServiceGroup->AddRef ();

    // 
    // Setup the Buffer Descriptor List (BDL)
    // Allocate 32 entries of 8 bytes (one BDL entry). We allocate two tables
    // because we need one table as a backup.
    // The pointer is aligned on a 8 byte boundary (that's what we need).
    //
    stBDList.pBDEntry = (tBDEntry *)HalAllocateCommonBuffer (Wave->AdapterObject,
                               MAX_BDL_ENTRIES * sizeof (tBDEntry) * 2,
                               &stBDList.PhysAddr,
                               FALSE);
    if (!stBDList.pBDEntry)
    {
        DOUT (DBG_ERROR, ("Failed HalAllocateCommonBuffer!"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // calculate the (backup) pointer.
    stBDList.pBDEntryBackup = (tBDEntry *)stBDList.pBDEntry + MAX_BDL_ENTRIES;
    //
    // Initialize BDL info.
    //
    stBDList.nHead = 0;
    stBDList.nTail = 0;
    stBDList.nTagCounter = 0;
    stBDList.nBDEntries = 0;


    //
    // Allocate a buffer for the 32 possible mappings. We allocate two tabels
    // becuse we need one table as a backup
    //
    stBDList.pMapData = 
        (tMapData *)ExAllocatePool (NonPagedPool, sizeof(tMapData) * 
                                    MAX_BDL_ENTRIES * 2);
    if (!stBDList.pMapData)
    {
        DOUT (DBG_ERROR, ("Failed to allocate the back up buffer!"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // calculate the (backup) pointer.
    stBDList.pMapDataBackup = stBDList.pMapData + MAX_BDL_ENTRIES;
        
    //
    // Store the base address of this DMA engine.
    //
    if (Capture)
    {
        //
        // could be PCM or MIC capture
        //
        if (Channel == PIN_WAVEIN_OFFSET)
        {
            // Base address for DMA registers.
            m_ulBDAddr = PI_BDBAR;
        }
        else
        {
            // Base address for DMA registers.
            m_ulBDAddr = MC_BDBAR;
        }
    }
    else    // render
    {
        // Base address for DMA registers.
        m_ulBDAddr = PO_BDBAR;
    }

    //
    // Reset the DMA and set the BD list pointer.
    //
    ResetDMA ();

    //
    // Reset the position pointers.
    //
    TotalBytesMapped   = 0;
    TotalBytesReleased = 0;

    //
    // Now set the requested sample rate. In case of a failure, the object
    // gets destroyed and releases all memory etc.
    //
    ntStatus = SetFormat (DataFormat_);
    if (!NT_SUCCESS (ntStatus))
    {
        DOUT (DBG_ERROR, ("Stream init SetFormat call failed!"));
        return ntStatus;
    }

    //
    // Initialize the device state.
    //
    m_PowerState = PowerDeviceD0;


    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::NonDelegatingQueryInterface
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::NonDelegatingQueryInterface
(
    IN  REFIID  Interface,
    OUT PVOID * Object
)
{
    PAGED_CODE ();

    ASSERT (Object);

    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::NonDelegatingQueryInterface]"));

    //
    // Convert for IID_IMiniportWavePciStream
    //
    if (IsEqualGUIDAligned (Interface, IID_IMiniportWavePciStream))
    {
        *Object = (PVOID)(PMINIPORTWAVEPCISTREAM)this;
    }
    //
    // Convert for IID_IServiceSink
    //
    else if (IsEqualGUIDAligned (Interface, IID_IServiceSink))
    {
        *Object = (PVOID)(PSERVICESINK)this;
    }
    //
    // Convert for IID_IUnknown
    //
    else if (IsEqualGUIDAligned (Interface, IID_IUnknown))
    {
        *Object = (PVOID)(PUNKNOWN)(PMINIPORTWAVEPCISTREAM)this;
    }
    else
    {
        *Object = NULL;
        return STATUS_INVALID_PARAMETER;
    }

    ((PUNKNOWN)*Object)->AddRef ();
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::GetAllocatorFraming
 *****************************************************************************
 * Returns the framing requirements for this device.
 * That is sample size (for one sample) and preferred frame (buffer) size.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::GetAllocatorFraming
(
    OUT PKSALLOCATOR_FRAMING AllocatorFraming
)
{
    PAGED_CODE ();

    ULONG SampleSize;

    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::GetAllocatorFraming]"));
    
    //
    // Determine sample size in bytes.  Always number of 
    // channels * 2 (because 16-bit).
    //
    SampleSize = DataFormat->WaveFormatEx.nChannels * 2;

    //
    // Report the minimum requirements.
    //
    AllocatorFraming->RequirementsFlags =
        KSALLOCATOR_REQUIREMENTF_SYSTEM_MEMORY | 
        KSALLOCATOR_REQUIREMENTF_PREFERENCES_ONLY;
    AllocatorFraming->Frames = 2;

    //
    // Currently, arbitrarily selecting 10ms as the frame target size.
    //
    AllocatorFraming->FrameSize = SampleSize * DataFormat->
        WaveFormatEx.nSamplesPerSec / 100;
    AllocatorFraming->FileAlignment = FILE_LONG_ALIGNMENT;
    AllocatorFraming->PoolType = NonPagedPool;

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::SetFormat
 *****************************************************************************    
 * This routine tests for proper data format (calls wave miniport) and sets
 * or changes the stream data format.
 * To figure out if the codec supports the sample rate, we just program the
 * sample rate and read it back. If it matches we return happy, if not then
 * we restore the sample rate and return unhappy.
 * We fail this routine if we are currently running (playing or recording).
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::SetFormat
(
    IN  PKSDATAFORMAT   Format
)
{
    PAGED_CODE ();

    ASSERT (Format);

    ULONG   TempRate;

    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::SetFormat]"));
    
    //
    // Change sample rate when we are in the stop or pause states - not 
    // while running!
    //
    if (DMAEngineState == DMA_ENGINE_ON)
    {
        return STATUS_UNSUCCESSFUL;
    }


    //
    // Ensure format falls in proper range and is supported.
    //
    NTSTATUS ntStatus = Wave->TestDataFormat (Format, (WavePins)(Channel));
    if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    //
    // Retrieve wave format portion.
    //
    PWAVEFORMATEX waveFormat = (PWAVEFORMATEX)(Format + 1);
    
    //
    // Save current rate in this context.
    //
    TempRate = waveFormat->nSamplesPerSec;
    
    //
    // Check for rate support by hardware.  If it is supported, then update 
    // hardware registers else return not implemented and audio stack will 
    // handle it.
    //
    if (Capture)
    {
        if (Channel == PIN_WAVEIN_OFFSET)
        {
            ntStatus = Wave->AdapterCommon->
                ProgramSampleRate (AC97REG_PCM_LR_RATE, TempRate);
        }
        else
        {
            ntStatus = Wave->AdapterCommon->
                ProgramSampleRate (AC97REG_MIC_RATE, TempRate);
        }
    }
    else
    {
        //
        // TODO: When multiple ch support is implemented we need to add 
        // handling of VRM.
        //
        ntStatus = Wave->AdapterCommon->
            ProgramSampleRate (AC97REG_PCM_FRONT_RATE, TempRate);
    }
        
    if (NT_SUCCESS (ntStatus))
    {
        //
        // print information and save the format information.
        //
        DataFormat = (PKSDATAFORMAT_WAVEFORMATEX)Format;
        CurrentRate = TempRate;
    }
    
    return ntStatus;
}
    
/*****************************************************************************
 * Non paged code begins here
 *****************************************************************************
 */

#pragma code_seg()
/*****************************************************************************
 * CMiniportWaveICHStream::PowerChangeNotify
 *****************************************************************************
 * This functions saves and maintains the stream state through power changes.
 */
NTSTATUS CMiniportWaveICHStream::PowerChangeNotify
(
    IN  POWER_STATE NewState
)
{
    PAGED_CODE ();

    KIRQL       OldIrql;
    NTSTATUS    ntStatus = STATUS_SUCCESS;
    
    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::PowerChangeNotify]"));
    
    //
    // We don't have to check the power state, that's already done by the wave
    // miniport.
    //

    DOUT (DBG_POWER, ("Changing state to D%d.",
                     (ULONG)NewState.DeviceState - (ULONG)PowerDeviceD0));
    
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
                // The scatter gather list is already arranged. A reset of the DMA
                // brings all pointers to the default state. From there we can start.
                //
                
                // Acquire the mapping spin lock
                KeAcquireSpinLock (&MapLock,&OldIrql);
                
                ntStatus = ResetDMA ();

                // Restore the remaining DMA registers, that is last valid index.
                Wave->AdapterCommon->WriteBMControlRegister (m_ulBDAddr + X_LVI,
                                                            (UCHAR)stBDList.nTail);
                
                
                // Release the mapping spin lock
                KeReleaseSpinLock (&MapLock,OldIrql);
            }
            break;

        case PowerDeviceD1:
        case PowerDeviceD2:
            // Here we do nothing. The device has still enough power to keep all
            // it's register values.
            break;

        case PowerDeviceD3:
            //
            // If we power down to D2 or D3 we might loose power, so we have to be
            // aware of the DMA engine resetting. In that case a play would start
            // with scatter gather entry 0 (the current index is read only).
            // We just rearrange the scatter gather list (like we do on
            // RevokeMappings) so that the current buffer which is played is at
            // entry 0.
            //
                
            // Acquire the mapping spin lock
            KeAcquireSpinLock (&MapLock,&OldIrql);

            // Disable interrupts and stop DMA just in case.
            Wave->AdapterCommon->WriteBMControlRegister (m_ulBDAddr + X_CR, (UCHAR)0);
    
            // Get current index 
            int nCurrentIndex = (int)Wave->AdapterCommon->
                ReadBMControlRegister8 (m_ulBDAddr + X_CIV);
    
            //
            // First move the BD list to the beginning.
            //
            // In case the DMA engine was stopped, current index may point to an
            // empty BD entry. When we start the DMA engine it would then play this
            // (undefined) entry, so we check here for that condition.
            //
            if ((nCurrentIndex == ((stBDList.nHead - 1) & BDL_MASK)) &&
               (stBDList.nBDEntries != MAX_BDL_ENTRIES - 1))
            {
                nCurrentIndex = stBDList.nHead;     // point to head
            }
    
            //
            // Move BD list to (0-((current - head) & mask)) & mask, where
            // ((current - head) & mask) is the difference between head and
            // current index, no matter where they are :)
            //
            MoveBDList (stBDList.nHead, (stBDList.nTail - 1) & BDL_MASK,
                    (0 - ((nCurrentIndex - stBDList.nHead) & BDL_MASK)) & BDL_MASK);
        
            //
            // Update structure.
            //
            stBDList.nHead = (0 - ((nCurrentIndex - stBDList.nHead) & BDL_MASK)) & 
                BDL_MASK;
            stBDList.nTail = (stBDList.nHead + stBDList.nBDEntries) & BDL_MASK;
            
            // release the mapping spin lock
            KeReleaseSpinLock (&MapLock,OldIrql);
            break;
    }
    
    //
    // Save the new state.  This local value is used to determine when to 
    // cache property accesses and when to permit the driver from accessing 
    // the hardware.
    //
    m_PowerState = NewState.DeviceState;
    DOUT (DBG_POWER, ("Entering D%d",
                      (ULONG)m_PowerState - (ULONG)PowerDeviceD0));

    return ntStatus;
}

/*****************************************************************************
 * CMiniportWaveICHStream::MoveBDList
 *****************************************************************************
 * Moves the BDList.
 * This function is used to remove entries from the scatter gather list or to
 * move the valid entries to the top. The mapping table which is hard linked
 * to the scatter gather entries is moved too.
 * The function does not change any variables in tBDList.
 * The mapping spin lock must be held when calling this routine.
 * We use this function to remove mappings (RevokeMappings) or to rearrange the
 * list for powerdown/up management (the DMA starts at position zero again).
 * Note that there is a simple way of doing this also. When you zero the buffer
 * length in the scatter gather, the DMA engine ignores the entry and continues
 * with the next. But our way is more generic and if you ever want to port the
 * driver to another DMA engine you might be thankful for this code.
 */
void CMiniportWaveICHStream::MoveBDList
(
    IN  int nFirst, 
    IN  int nLast, 
    IN  int nNewPos
)
{
    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::MoveBDList]"));
    
    //
    // Print information about the scatter gather list.
    //
    DOUT (DBG_DMA, ("Moving BD entry %d-%d to %d.", nFirst, nLast, nNewPos));

    //
    // First copy the tables to a save place.
    //
    RtlCopyMemory ((PVOID)stBDList.pBDEntryBackup,
                   (PVOID)stBDList.pBDEntry,
                   sizeof (tBDEntry) * MAX_BDL_ENTRIES);
    RtlCopyMemory ((PVOID)stBDList.pMapDataBackup,
                   (PVOID)stBDList.pMapData,
                   sizeof (tMapData) * MAX_BDL_ENTRIES);

    //
    // We move all the entries in blocks to the new position.
    //
    int nBlockCounter = 0;
    do
    {
        nBlockCounter++;
        //
        // We must copy the block when the index wraps around (ring buffer)
        // or we are at the last entry.
        //
        if (((nNewPos + nBlockCounter) == MAX_BDL_ENTRIES) ||   // wrap around
            ((nFirst + nBlockCounter) == MAX_BDL_ENTRIES) ||    // wrap around
            ((nFirst + nBlockCounter) == (nLast + 1)))          // last entry
        {
            //
            // copy one block (multiple entries).
            //
            RtlCopyMemory ((PVOID)&stBDList.pBDEntry[nNewPos],
                           (PVOID)&stBDList.pBDEntryBackup[nFirst],
                           sizeof (tBDEntry) * nBlockCounter);
            RtlCopyMemory ((PVOID)&stBDList.pMapData[nNewPos],
                           (PVOID)&stBDList.pMapDataBackup[nFirst],
                           sizeof (tMapData) * nBlockCounter);
            // adjust the index
            nNewPos = (nNewPos + nBlockCounter) & BDL_MASK;
            nFirst = (nFirst + nBlockCounter) & BDL_MASK;
            nBlockCounter = 0;
        }
    // nBlockCounter should be zero when the end condition hits.
    } while (((nFirst + nBlockCounter - 1) & BDL_MASK) != nLast);
}


#if (DBG)
/*****************************************************************************
 * CMiniportWaveICHStream::CheckBDList
 *****************************************************************************
 * Makes various sanity checks to keep track if the scatter gather list +
 * HW index pointer match and fit with the information we have.
 * Call when you have the mapping spin lock. The checks are kinda inaccurate.
 */
void CMiniportWaveICHStream::CheckBDList (void)
{
    int nCurrentIndex, nLastValidIndex;

    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::CheckBDList]"));

    //
    // Get the HW index.
    //
    nCurrentIndex = (int)Wave->AdapterCommon->
        ReadBMControlRegister8 (m_ulBDAddr + X_CIV);
    nLastValidIndex = (int)Wave->AdapterCommon->
        ReadBMControlRegister8 (m_ulBDAddr + X_LVI);

    
    //
    // Check DMA engine programming.
    //
    if (!(Wave->AdapterCommon->
         ReadBMControlRegister8 (m_ulBDAddr + X_CR) & CR_RPBM))
    {
        if (DMAEngineState == DMA_ENGINE_ON)
        {
            DOUT (DBG_ERROR, ("Wrong programmed DMA engine."));
        }
    }

    //
    // First check the number of entries.
    //
    if (stBDList.nHead <= stBDList.nTail)
    {
        if ((stBDList.nHead + stBDList.nBDEntries) != stBDList.nTail)
        {
            DOUT (DBG_ERROR, ("CIV: %d, LVI: %d, Head: %d, Tail:%d, Num: %d", 
                             nCurrentIndex, nLastValidIndex, stBDList.nHead, 
                             stBDList.nTail, stBDList.nBDEntries));
            DOUT (DBG_ERROR, ("head + num != tail"));
        }
    }
    else
    {
        if ((stBDList.nHead + stBDList.nBDEntries) != 
           (stBDList.nTail + MAX_BDL_ENTRIES))
        {
            DOUT (DBG_ERROR, ("CIV: %d, LVI: %d, Head: %d, Tail:%d, Num: %d",
                             nCurrentIndex, nLastValidIndex, stBDList.nHead, 
                             stBDList.nTail, stBDList.nBDEntries));
            DOUT (DBG_ERROR, ("head + num != tail"));
        }
    }

    //
    // Second, check if last valid index is tail or (tail - 1)
    // It is tail before any mappings are processed, else (tail - 1).
    //
    if ((nLastValidIndex != ((stBDList.nTail - 1) & BDL_MASK)) &&
        (nLastValidIndex != stBDList.nTail))
    {
        DOUT (DBG_ERROR, ("CIV: %d, LVI: %d, Head: %d, Tail:%d, Num: %d",
                         nCurrentIndex, nLastValidIndex, stBDList.nHead, 
                         stBDList.nTail, stBDList.nBDEntries));
        DOUT (DBG_ERROR, ("(tail - 1) != LVI"));
    }

    //
    // Third, check that the current index is between head and 
    // tail or (head - 1)!
    //
    int nHead = (stBDList.nHead - 1) & BDL_MASK; // optimization only - really ;))

    //
    // Check if (head - 1) is less then tail.
    //
    if (nHead < stBDList.nTail)
    {
        //
        // Then current index must be >= head and < tail.
        if ((nCurrentIndex < nHead) ||
            (nCurrentIndex > stBDList.nTail))
        {
            DOUT (DBG_ERROR, ("CIV: %d, LVI: %d, Head: %d, Tail:%d, Num: %d",
                             nCurrentIndex, nLastValidIndex, stBDList.nHead, 
                             stBDList.nTail, stBDList.nBDEntries));
            DOUT (DBG_ERROR, ("CIV <> [head,tail]"));
        }
    }
    else
    {
        // then current index must not be <head and >=tail.
        if ((nCurrentIndex < nHead) &&
            (nCurrentIndex > stBDList.nTail))
        {
            DOUT (DBG_ERROR, ("CIV: %d, LVI: %d, Head: %d, Tail:%d, Num: %d",
                             nCurrentIndex, nLastValidIndex, stBDList.nHead, 
                             stBDList.nTail, stBDList.nBDEntries));
            DOUT (DBG_ERROR, ("CIV <> [head,tail]"));
        }
    }
}
#endif


/*****************************************************************************
 * CMiniportWaveICHStream::Service
 *****************************************************************************
 * This routine is called by the port driver in response to the interrupt
 * service routine requesting service on the stream's service group.
 * Requesting service on the service group results in a DPC being scheduled
 * that calls this routine when it runs.
 */
STDMETHODIMP_(void) CMiniportWaveICHStream::Service (void)
{

    DOUT (DBG_PRINT, ("Service"));

    // acquire the mapping spin lock
    KeAcquireSpinLockAtDpcLevel (&MapLock);

    // release all mappings
    ReleaseUsedMappings ();

    // get new mappings
    GetNewMappings ();

    // release the mapping spin lock
    KeReleaseSpinLockFromDpcLevel (&MapLock);
}


/*****************************************************************************
 * CMiniportWaveICHStream::NormalizePhysicalPosition
 *****************************************************************************
 * Given a physical position based on the actual number of bytes transferred,
 * this function converts the position to a time-based value of 100ns units.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::NormalizePhysicalPosition
(
    IN OUT PLONGLONG PhysicalPosition
)
{
    ULONG SampleSize;

    DOUT (DBG_PRINT, ("NormalizePhysicalPosition"));

    //
    // Determine the sample size in bytes
    //
    SampleSize = DataFormat->WaveFormatEx.nChannels * 2;

    //
    // Calculate the time in 100ns steps.
    //
    *PhysicalPosition = (_100NS_UNITS_PER_SECOND / SampleSize * 
                         *PhysicalPosition) / CurrentRate;

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::GetPosition
 *****************************************************************************
 * Gets the stream position. This is a byte count of the current position of
 * a stream running on a particular DMA engine.  We must return a sample
 * accurate count or the WaveDrv32 wave drift tests (35.2 & 36.2) will fail.
 *
 * The position is the sum of three parts:
 *     1) The total number of bytes in released buffers
 *     2) The position in the current buffer.
 *     3) The total number of bytes in played but not yet released buffers
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::GetPosition
(
    OUT PULONGLONG   Position
)
{
    KIRQL   OldIrql;
    UCHAR   nCurrentIndex = 0;
    WORD    PositionInCurrentBuffer = 0;

    ASSERT (Position);
               
    //
    // Acquire the mapping spin lock.
    //
    KeAcquireSpinLock (&MapLock, &OldIrql);

#if (DBG)
    //
    // Check the BD list.
    //
    CheckBDList ();
#endif

    //
    // Start with TotalBytesReleased (mappings released).
    //
    *Position = TotalBytesReleased;
    
    //
    // If we have entries in the list, we may have buffers that have not been
    // released but have been at least partially played.
    //
    if (stBDList.nBDEntries)
    {
        //
        // Repeat this until we get the same reading twice.  This will prevent
        // jumps when we are near the end of the buffer.
        //
        do
        {
            nCurrentIndex = Wave->AdapterCommon->
                ReadBMControlRegister8 (m_ulBDAddr + X_CIV);
            
            PositionInCurrentBuffer = 
                (WORD)stBDList.pMapData[nCurrentIndex].ulBufferLength - 
                (WORD)(Wave->AdapterCommon->
                 ReadBMControlRegister16 (m_ulBDAddr + X_PICB) << 1);

        } while (nCurrentIndex != (int)Wave->AdapterCommon->
                ReadBMControlRegister8 (m_ulBDAddr + X_CIV));
        
        //
        // Add in our position in the current buffer.  The read returns the 
        // amount left in the buffer.
        //
        *Position += PositionInCurrentBuffer;
        
        //
        // Total any buffers that have been played and not released.
        //
        int i = stBDList.nHead;
        while (i != nCurrentIndex)
        {
            *Position += (ULONGLONG)stBDList.pMapData[i].ulBufferLength;
            i = (i + 1) & BDL_MASK;
        }
    }

    //
    // Release the mapping spin lock.
    //
    KeReleaseSpinLock (&MapLock, OldIrql);


    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::RevokeMappings
 *****************************************************************************
 * This routine is used by the port to revoke mappings previously delivered 
 * to the miniport stream that have not yet been unmapped.  This would 
 * typically be called in response to an I/O cancellation request.
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::RevokeMappings
(
    IN  PVOID     FirstTag,
    IN  PVOID     LastTag,
    OUT PULONG    MappingsRevoked
)
{
    ASSERT (MappingsRevoked);

    KIRQL   OldIrql;
    ULONG   ulOldDMAEngineState;
    int     nCurrentIndex, nSearchIndex, nFirst, nLast, nNumMappings;

    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::RevokeMappings]"));
    
    //
    // print information about the scatter gather list.
    //
    DOUT (DBG_DMA, ("Head %d, Tail %d, Tag counter %d, Entries %d.",
                   stBDList.nHead, stBDList.nTail, stBDList.nTagCounter, 
                   stBDList.nBDEntries));

    //
    // Start accessing the mappings.
    //
    KeAcquireSpinLock (&MapLock, &OldIrql);

    //
    // Save old DMA engine state.
    //
    ulOldDMAEngineState = DMAEngineState;
    
    //
    // First stop the DMA engine so it won't process the next buffer in the
    // scatter gather list which might be one of the revoked buffers.
    //
    PauseDMA ();

#if (DBG)
    // Check the BD list.
    CheckBDList ();
#endif

    // Get current index 
    nCurrentIndex = Wave->AdapterCommon->
        ReadBMControlRegister8 (m_ulBDAddr + X_CIV);

    //
    // We always rearrange the scatter gather list. That means we reset the DMA
    // engine and move the BD list so that the entry where the current index
    // pointed to is located at position 0.
    //
    ResetDMA ();

    //
    // Return immediately if we just have 1 entry in our BD list.
    //
    if (!stBDList.nBDEntries || (stBDList.nBDEntries == 1))
    {
        *MappingsRevoked = stBDList.nBDEntries;
        stBDList.nHead = stBDList.nTail = stBDList.nBDEntries = 0;
        
        //
        // CIV and LVI of DMA registers are set to 0 already.
        //
        KeReleaseSpinLock (&MapLock, OldIrql);
        return STATUS_SUCCESS;
    }

    //
    // First move the BD list to the beginning.  In case the DMA engine was 
    // stopped, current index may point to an empty BD entry.
    //
    if ((nCurrentIndex == ((stBDList.nHead - 1) & BDL_MASK)) &&
       (stBDList.nBDEntries != MAX_BDL_ENTRIES - 1))
    {
        nCurrentIndex = stBDList.nHead;     // point to head
    }

    //
    // Move BD list to (0-((current - head) & mask)) & mask
    // where ((current - head) & mask) is the difference between head and
    // current index, no matter where they are :)
    //
    MoveBDList (stBDList.nHead, (stBDList.nTail - 1) & BDL_MASK,
            (0 - ((nCurrentIndex - stBDList.nHead) & BDL_MASK)) & BDL_MASK);
    
    //
    // Update structure.
    //
    stBDList.nHead = (0 - ((nCurrentIndex - stBDList.nHead) & BDL_MASK)) & 
        BDL_MASK;
    stBDList.nTail = (stBDList.nHead + stBDList.nBDEntries) & BDL_MASK;

    //
    // Then we have to search for the tags. If we wouldn't have to rearrange the
    // scatter gather list all the time, then we could use the tag as an index
    // to the array, but the only way to clear DMA caches is a reset which has
    // the side-effect that we have to rearrange the BD list (see above).
    //
    // search for first...
    //
    nSearchIndex = stBDList.nHead;
    do
    {
        if ((void *)stBDList.pMapData[nSearchIndex].ulTag == FirstTag)
            break;
        nSearchIndex = (nSearchIndex + 1) & BDL_MASK;
    } while (nSearchIndex != stBDList.nTail);
    nFirst = nSearchIndex;
    
    //
    // Search for last...
    //
    nSearchIndex = stBDList.nHead;
    do
    {
        if ((void *)stBDList.pMapData[nSearchIndex].ulTag == LastTag)
            break;
        nSearchIndex = (nSearchIndex + 1) & BDL_MASK;
    } while (nSearchIndex != stBDList.nTail);
    nLast = nSearchIndex;


    //
    // Check search result.
    //
    if ((nFirst == stBDList.nTail) || (nLast == stBDList.nTail))
    {
        DOUT (DBG_ERROR, ("!!! Entry not found !!!"));
        
        //
        // restart DMA in case it was running
        //
        if ((ulOldDMAEngineState == DMA_ENGINE_ON) && stBDList.nBDEntries)
            ResumeDMA ();
        *MappingsRevoked = 0;
        KeReleaseSpinLock (&MapLock, OldIrql);
        return STATUS_UNSUCCESSFUL;         // one of the tags not found
    }

    // Print the index numbers found.
    DOUT (DBG_DMA, ("Removing entries %d (%d) to %d (%d).", nFirst, FirstTag,
                  nLast, LastTag));

    //
    // Calculate the entries between the indizes.
    //
    if (nLast < nFirst)
    {
        nNumMappings = ((nLast + MAX_BDL_ENTRIES) - nFirst) + 1;
    }
    else
    {
        nNumMappings = (nLast - nFirst) + 1;
    }

    //
    // Print debug inormation.
    //
    DOUT (DBG_DMA, ("Found entries: %d-%d, %d entries.", nFirst, nLast, 
                   nNumMappings));
    
    //
    // Now remove the revoked buffers.  Move the BD list and modify the 
    // status information.
    //
    if (nFirst < stBDList.nTail)
    {
        //
        // In this case, both first and last are >= the current index (0)
        //
        if (nLast != ((stBDList.nTail - 1) & BDL_MASK))
        {
            //
            // Not the last entry, so move the BD list + mappings.
            //
            MoveBDList ((nLast + 1) & BDL_MASK, (stBDList.nTail - 1) & BDL_MASK,
                        nFirst);
        }
        stBDList.nTail = (stBDList.nTail - nNumMappings) & BDL_MASK;
    }

    //
    // In this case, at least first is "<" than current index (0)
    //
    else    
    {
        //
        // Check for last.
        //
        if (nLast < stBDList.nTail)
        {                 
            //
            // Last is ">=" current index and first is "<" current index (0).
            // Remove MAX_DBL_ENTRIES - first entries in front of current index.
            //
            if (nFirst != stBDList.nHead)
            {
                //
                // Move from head towards current index.
                //
                MoveBDList (stBDList.nHead, nFirst - 1, 
                           (stBDList.nHead + (MAX_BDL_ENTRIES - nFirst)) & 
                           BDL_MASK);
            }

            //
            // Adjust head.
            //
            stBDList.nHead = (stBDList.nHead + (MAX_BDL_ENTRIES - nFirst)) & 
                BDL_MASK;

            //
            // Remove nLast entries from CIV to tail.
            //
            if (nLast != ((stBDList.nTail - 1) & BDL_MASK))
            {
                //
                // Not the last entry, so move the BD list + mappings.
                //
                MoveBDList (nLast + 1, (stBDList.nTail - 1) & BDL_MASK, 0);
            }

            //
            // Adjust tail.
            //
            stBDList.nTail = (stBDList.nTail - (nLast + 1)) & BDL_MASK;
        }

        //
        // Last is "<" current index and first is "<" current index (0).
        //
        else
        {
            //
            // Remove nNumMappings entries in front of current index.
            //
            if (nFirst != stBDList.nHead)
            {
                //
                // Move from head towards current index.
                //
                MoveBDList (stBDList.nHead, nFirst - 1, 
                           (nLast - nNumMappings) + 1);
            }
            
            //
            // Adjust head.
            //
            stBDList.nHead = (stBDList.nHead + nNumMappings) & BDL_MASK;
        }
    }

    //
    // In all cases, reduce the number of mappings.
    //
    stBDList.nBDEntries -= nNumMappings;

    //
    // Print debug information.
    //
    DOUT (DBG_DMA, ("Number of mappings is now %d, Head is %d, Tail is %d",
            stBDList.nBDEntries, stBDList.nHead, stBDList.nTail));
    
    //
    // Reprogram the last valid index only when tail != 0
    //
    if (stBDList.nTail)
    {
        Wave->AdapterCommon->WriteBMControlRegister (m_ulBDAddr + X_LVI,
                    (UCHAR)(stBDList.nTail - 1 & BDL_MASK));
    }

    //
    // Just un-pause the DMA engine if it was running before and there are 
    // still entries left and tail != 0.
    //
    if ((ulOldDMAEngineState == DMA_ENGINE_ON) && stBDList.nBDEntries 
       && stBDList.nTail)
    {
        ResumeDMA ();
    }


#if (DBG)
    //
    // Check the BD list.
    //
    CheckBDList ();
#endif

    //
    // Release the mapping spin lock and return the number of mappings we 
    // revoked.
    //
    KeReleaseSpinLock (&MapLock, OldIrql);
    *MappingsRevoked = nNumMappings;
    
    
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::MappingAvailable
 *****************************************************************************
 * This routine is called by the port driver to notify the stream that there
 * are new mappings available.  Note that this is ONLY called after the stream
 * has previously had a GetMapping() call fail due to lack of available
 * mappings.
 */
STDMETHODIMP_(void) CMiniportWaveICHStream::MappingAvailable (void)
{
    KIRQL OldIrql;

    DOUT (DBG_PRINT, ("MappingAvailable"));

    //
    // Acquire the mapping spin lock.
    //
    KeAcquireSpinLock (&MapLock,&OldIrql);

    //
    // Release processed mappings.
    //
    ReleaseUsedMappings ();

    //
    // Process the new mappings.
    //
    GetNewMappings ();

    //
    // Release the mapping spin lock.
    //
    KeReleaseSpinLock (&MapLock,OldIrql);
}


/*****************************************************************************
 * CMiniportWaveICHStream::GetNewMappings
 *****************************************************************************
 * This routine is called when new mappings are available from the port driver.
 * The routine places mappings into the input mapping queue. ICH can handle up
 * to 32 entries (descriptors). We program the DMA registers if we have at least
 * one mapping in the queue. The mapping spin lock must be held when calling
 * this routine.
 */
NTSTATUS CMiniportWaveICHStream::GetNewMappings (void)
{
    ULONG       Flags;
    NTSTATUS    ntStatus;
    int         nInsertMappings = 0;
    int         nTail;                  // aut. variable

    DOUT (DBG_PRINT, ("[GetNewMappings]"));
    
    //
    // Print debug information ...
    //
    DOUT (DBG_DMA, ("Start getting mappings at %d.", stBDList.nTail));

#if (DBG)
    //
    // Check the BD list.
    //
    CheckBDList ();
#endif

    //
    // Get available mappings up to the max of 31 that we can hold in the BDL.
    //
    while (stBDList.nBDEntries < (MAX_BDL_ENTRIES - 1))
    {
        //
        // For optimization.
        //
        nTail = stBDList.nTail;

        //
        // Try to get the mapping from the port.
        //
        ntStatus = PortStream->
            GetMapping ((PVOID)stBDList.nTagCounter,
                       (PPHYSICAL_ADDRESS)&stBDList.pMapData[nTail].PhysAddr,
                       (PVOID *)&stBDList.pMapData[nTail].pVirtAddr,
                       &stBDList.pMapData[nTail].ulBufferLength,
                       &Flags);
    
        
        //
        // Quit this loop when we run out of mappings.
        //
        if (!NT_SUCCESS (ntStatus))
        {
            break;
        }

        ASSERT (stBDList.pMapData[nTail].ulBufferLength <= 0x10000);

        //
        // Save the tag used to get the mapping so it can be revoked if not
        // needed.
        //
        stBDList.pMapData[nTail].ulTag = stBDList.nTagCounter++;
        TotalBytesMapped += (ULONGLONG)stBDList.pMapData[nTail].ulBufferLength;
        nInsertMappings++;
    
        //
        // Fill in the BDL entry with pointer to physical address and length.
        //
        stBDList.pBDEntry[nTail].dwPtrToPhyAddress =
            stBDList.pMapData[nTail].PhysAddr.LowPart;
        stBDList.pBDEntry[nTail].wLength = 
            (WORD)(stBDList.pMapData[nTail].ulBufferLength >> 1);
        stBDList.pBDEntry[nTail].wPolicyBits = BUP_SET;
        if (Flags)
        {
            stBDList.pBDEntry[nTail].wPolicyBits |= IOC_ENABLE; 
        }

    
        //
        // Update tail and number of entries.
        //
        stBDList.nTail = (stBDList.nTail + 1) & BDL_MASK;
        stBDList.nBDEntries++;
    }

    //
    // Reset the tail variable to the last valid entry.
    //
    nTail = (stBDList.nTail - 1) & BDL_MASK;

    //
    // If there were processed mappings, program the DMA registers.
    //
    if (nInsertMappings)
    {
        //
        // Print debug information ...
        //
        DOUT (DBG_DMA, ("Got %d mappings, LVI %d.", nInsertMappings, nTail));

        //
        // Set last valid index (LVI) register!
        //
        Wave->AdapterCommon->WriteBMControlRegister (m_ulBDAddr + X_LVI,
                                                     (UCHAR)nTail);
    }
#if (DBG)
    else
    {
        //
        // No mappings.
        //
        DOUT (DBG_DMA, ("Got no mappings."));

        if (Wave->AdapterCommon->ReadBMControlRegister8 (m_ulBDAddr + X_SR) &
            SR_CELV)
        {
            //
            // We starve.  :-(
            //
            DOUT (DBG_DMA, ("We starve ... Head %d, Tail %d, Entries %d.",
                    stBDList.nHead, stBDList.nTail, stBDList.nBDEntries));
        }
    }
#endif

    return ntStatus;
}


/*****************************************************************************
 * CMiniportWaveICHStream::ReleaseUsedMappings
 *****************************************************************************
 * This routine unmaps previously mapped memory that the hardware has 
 * completed processing on.  This routine is typically called at DPC level 
 * from the stream deferred procedure call that results from a stream 
 * interrupt. The mapping spin lock must be held when calling this routine.
 */
NTSTATUS CMiniportWaveICHStream::ReleaseUsedMappings (void)
{
    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::ReleaseUsedMappings]"));

#if (DBG)
    //
    // Check the BD list.
    //
    CheckBDList ();
#endif

    //
    // Get current index 
    //
    int nCurrentIndex = (int)Wave->AdapterCommon->
        ReadBMControlRegister8 (m_ulBDAddr + X_CIV);

    //
    // Print debug info.
    //
    DOUT (DBG_DMA, ("CIV is %d, Head %d, Tail %d, num %d.",
                   nCurrentIndex, stBDList.nHead, stBDList.nTail, 
                   stBDList.nBDEntries));

    //
    // Return if no maps.
    //
    if (!stBDList.nBDEntries)
    {
        DOUT (DBG_DMA, ("No mappings to unmap."));
        return STATUS_SUCCESS;
    }


    //
    // Check if CIV is between head and tail.
    //
    if (nCurrentIndex < stBDList.nHead)
    {
        //
        // Bail out if CIV is greater or equal then tail.
        //
        if ((nCurrentIndex + MAX_BDL_ENTRIES) >=
            (stBDList.nHead + stBDList.nBDEntries))
        {
            DOUT (DBG_DMA, ("Nothing to unmap."));
            return STATUS_SUCCESS;
        }
    }
    else
    {
        //
        // Bail out if CIV is greater or equal then tail.
        //
        if (nCurrentIndex >= (stBDList.nHead + stBDList.nBDEntries))
        {
            DOUT (DBG_DMA, ("Nothing to unmap."));
            return STATUS_SUCCESS;
        }
    }

    //
    // Clean up everything to that index.
    //
    while (stBDList.nBDEntries)
    {
        //
        // Check to see if we've released all the buffers.
        //
        if (stBDList.nHead == nCurrentIndex)
        {
            if (nCurrentIndex == ((stBDList.nTail - 1) & BDL_MASK))
            {
                //
                // A special case is starvation or stop of stream, when the 
                // DMA engine finished playing the buffers, CVI is equal LVI 
                // and SR_CELV is set.
                //
                if (!(Wave->AdapterCommon->
                     ReadBMControlRegister16 (m_ulBDAddr + X_SR) & SR_CELV))
                {
                    break;
                }

                //
                // In case the CVI=LVI bit is set, nBDEntries should be 1
                //
                if (stBDList.nBDEntries != 1)
                {
                    DOUT (DBG_ERROR, ("Inconsitency: tail reached and num != 1."));
                }
            }
            else
            {
                //
                // Bail out.
                //
                break;
            }
        }

        //
        // Release this entry.
        //
        PortStream->ReleaseMapping ((PVOID)stBDList.
                                   pMapData[stBDList.nHead].ulTag);

        //
        // Update the BDL.
        //
        TotalBytesReleased += 
            (ULONGLONG)stBDList.pMapData[stBDList.nHead].ulBufferLength;
        stBDList.nBDEntries--;
        stBDList.nHead = (stBDList.nHead + 1) & BDL_MASK;
    }


    //
    // Print release information and return.
    //
    DOUT (DBG_DMA, ("New Head %d, Tail %d, num %d.",
                   stBDList.nHead, stBDList.nTail, stBDList.nBDEntries));
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::SetState
 *****************************************************************************
 * This routine sets/changes the DMA engine state to play, stop, or pause
 */
STDMETHODIMP_(NTSTATUS) CMiniportWaveICHStream::SetState
(
    IN  KSSTATE State
)
{
    PAGED_CODE ();
    
    KIRQL OldIrql;
    
    DOUT (DBG_PRINT, ("[CMiniportWaveICHStream::SetState]"));
    DOUT (DBG_STREAM, ("SetState to %d", State));


    //
    // Start or stop the DMA engine dependent of the state.
    //
    switch (State)
    {
        case KSSTATE_STOP:
            // Just pause DMA. If we have mappings left in our queue,
            // portcls will call RevokeMappings to destroy them.
            PauseDMA ();

            // acquire the mapping spin lock
            KeAcquireSpinLock (&MapLock,&OldIrql);

            // Release processed mappings
            ReleaseUsedMappings ();

            // release the mapping spin lock
            KeReleaseSpinLock (&MapLock,OldIrql);

            // Reset the position counters.
            TotalBytesMapped = TotalBytesReleased = 0;
            break;
    
        case KSSTATE_ACQUIRE:
        case KSSTATE_PAUSE:
            // pause now.
            PauseDMA ();

            // acquire the mapping spin lock
            KeAcquireSpinLock (&MapLock,&OldIrql);

            // Release processed mappings
            ReleaseUsedMappings ();

            // release the mapping spin lock
            KeReleaseSpinLock (&MapLock,OldIrql);
            break;
    
        case KSSTATE_RUN:
            //
            // Let's rock.
            //
            
            // Make sure we are not running already.
            if (DMAEngineState == DMA_ENGINE_ON)
            {
                return STATUS_SUCCESS;
            }

            // Acquire the mapping spin lock.
            KeAcquireSpinLock (&MapLock,&OldIrql);

            // Release processed mappings.
            ReleaseUsedMappings ();

            // Get new mappings.
            GetNewMappings ();

            // Release the mapping spin lock.
            KeReleaseSpinLock (&MapLock,OldIrql);

            // Kick DMA again just in case.
            ResumeDMA ();
            break;
    }
    
    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::ResetDMA
 *****************************************************************************
 * This routine resets the Run/Pause bit in the control register. In addition, it
 * resets all DMA registers contents.
 * To be safe, call it with the mappings spinlock.
 */
NTSTATUS CMiniportWaveICHStream::ResetDMA (void)
{
    DOUT (DBG_PRINT, ("ResetDMA"));

	NTSTATUS ntStatus = Wave->AdapterCommon->hda_stop_stream();
	if (!NT_SUCCESS (ntStatus))
        return ntStatus;

    DMAEngineState = DMA_ENGINE_OFF;


    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::PauseDMA
 *****************************************************************************
 * This routine pauses a hardware stream by reseting the Run/Pause bit in the
 * control registers, leaving DMA registers content intact so that the stream
 * can later be resumed.
 */
NTSTATUS CMiniportWaveICHStream::PauseDMA (void)
{
    DOUT (DBG_PRINT, ("PauseDMA"));
    
    //
    // Only pause if we're actually on.
    //
    if (DMAEngineState != DMA_ENGINE_ON)
    {
        return STATUS_SUCCESS;
    }

    //
    // Turn off DMA engine by resetting the RPBM bit to 0. Don't reset any 
    // registers.
    //
    UCHAR RegisterValue = Wave->AdapterCommon->
        ReadBMControlRegister8 (m_ulBDAddr + X_CR);
    RegisterValue &= ~CR_RPBM;
    Wave->AdapterCommon->
        WriteBMControlRegister (m_ulBDAddr + X_CR, RegisterValue);

    //
    // Set the DMA engine state.
    //
    DMAEngineState = DMA_ENGINE_OFF;
    

    return STATUS_SUCCESS;
}


/*****************************************************************************
 * CMiniportWaveICHStream::ResumeDMA
 *****************************************************************************
 * This routine sets the Run/Pause bit for the particular DMA engine to resume
 * it after it's been paused. This assumes that DMA registers content have 
 * been preserved.
 */
NTSTATUS CMiniportWaveICHStream::ResumeDMA (void)
{
    DOUT (DBG_PRINT, ("ResumeDMA"));
    
    //
    // Turn DMA engine on by setting the RPBM bit to 1. Don't do anything to 
    // the registers.
    //
    UCHAR RegisterValue = Wave->AdapterCommon->
        ReadBMControlRegister8 (m_ulBDAddr + X_CR) | CR_RPBM;
    Wave->AdapterCommon->
        WriteBMControlRegister (m_ulBDAddr + X_CR, RegisterValue);
    
    //
    // Set the DMA engine state.
    //
    DMAEngineState = DMA_ENGINE_ON;
    

    return STATUS_SUCCESS;
}

