//Wrapper class around IDmaChannel
//generated with help of Gemini. yes I know

class CMyDmaChannel : public IDmaChannel {
private:
    IDmaChannel* m_RealDmaChannel;
    PVOID        m_NonCachedBuffer;
    ULONG        m_BufferSize;
    LONG         m_RefCount;

public:
    CMyDmaChannel(IDmaChannel* RealChannel) {
        m_RealDmaChannel = RealChannel;
        m_RealDmaChannel->AddRef();
        m_RefCount = 1;
        m_NonCachedBuffer = NULL;
    }

    // --- IUnknown ---
    STDMETHODIMP QueryInterface(REFIID riid, PVOID* ppv) {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDmaChannel)) {
            *ppv = (PVOID)this; AddRef(); return STATUS_SUCCESS;
        }
        return m_RealDmaChannel->QueryInterface(riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_RefCount); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG ul = InterlockedDecrement(&m_RefCount);
        if (ul == 0) {
            if (m_NonCachedBuffer) { /* Cleanup if needed */ }
            m_RealDmaChannel->Release();
            delete this;
        }
        return ul;
    }

    // --- IDmaChannel Overrides ---
    STDMETHODIMP AllocateBuffer(ULONG BufferSize, PPHYSICAL_ADDRESS PhysicalAddressConstraint) {
        // Here is where the magic happens
        // 1. Let the real channel allocate the physical RAM
        NTSTATUS ntStatus = m_RealDmaChannel->AllocateBuffer(BufferSize, PhysicalAddressConstraint);
        
        if (NT_SUCCESS(ntStatus)) {
            m_BufferSize = m_RealDmaChannel->AllocatedBufferSize();
            PHYSICAL_ADDRESS phys = m_RealDmaChannel->PhysicalAddress();
            
            // 2. Use MmMapIoSpace to create a Non-Cached view of that same RAM
            m_NonCachedBuffer = MmMapIoSpace(phys, m_BufferSize, MmWriteCombined);
			if (m_NonCachedBuffer) {
				_asm wbinvd;
			}
        }
        return ntStatus;
    }
	STDMETHODIMP_(void) FreeBuffer(void) {
		// 1. Clean up our non-cached alias first
		if (m_NonCachedBuffer) {
		    MmUnmapIoSpace(m_NonCachedBuffer, m_BufferSize);
		    m_NonCachedBuffer = NULL;
		}

		// 2. Let the real channel free the actual physical memory
		m_RealDmaChannel->FreeBuffer();
		m_BufferSize = 0;
	}

    STDMETHODIMP_(PVOID) SystemAddress() {
        // If we have our non-cached alias, return that instead of the real one
        return m_NonCachedBuffer ? m_NonCachedBuffer : m_RealDmaChannel->SystemAddress();
    }

    STDMETHODIMP_(void) CopyTo(
		IN      PVOID   Destination,
        IN      PVOID   Source,
        IN      ULONG   ByteCount) {
        RtlCopyMemory(Destination, Source, ByteCount);
    }
	STDMETHODIMP_(void) CopyFrom(
		IN      PVOID   Destination,
        IN      PVOID   Source,
        IN      ULONG   ByteCount) {
        RtlCopyMemory(Destination, Source, ByteCount);
    }

    // --- Pass-through the rest ---
	STDMETHODIMP_(ULONG) TransferCount() { return m_RealDmaChannel->TransferCount();}
	STDMETHODIMP_(ULONG) MaximumBufferSize() { return m_RealDmaChannel->MaximumBufferSize(); }
    STDMETHODIMP_(ULONG) AllocatedBufferSize() { return m_RealDmaChannel->AllocatedBufferSize(); }
	STDMETHODIMP_(ULONG) BufferSize() {return m_RealDmaChannel->BufferSize();}
	STDMETHODIMP_(void) SetBufferSize(ULONG BufferSize) {m_RealDmaChannel->SetBufferSize(BufferSize);}
    STDMETHODIMP_(PHYSICAL_ADDRESS) PhysicalAddress() { return m_RealDmaChannel->PhysicalAddress(); }
    STDMETHODIMP_(PADAPTER_OBJECT) GetAdapterObject() { return m_RealDmaChannel->GetAdapterObject(); }
};

/* --- interface spec from Portcls ---

DECLARE_INTERFACE_(IDmaChannel,IUnknown)
{
    STDMETHOD(AllocateBuffer)
    (   THIS_
        IN      ULONG               BufferSize,
        IN      PPHYSICAL_ADDRESS   PhysicalAddressConstraint   OPTIONAL
    )   PURE;

    STDMETHOD_(void,FreeBuffer)
    (   THIS
    )   PURE;

    STDMETHOD_(ULONG,TransferCount)
    (   THIS
    )   PURE;

    STDMETHOD_(ULONG,MaximumBufferSize)
    (   THIS
    )   PURE;

    STDMETHOD_(ULONG,AllocatedBufferSize)
    (   THIS
    )   PURE;

    STDMETHOD_(ULONG,BufferSize)
    (   THIS
    )   PURE;

    STDMETHOD_(void,SetBufferSize)
    (   THIS_
        IN      ULONG   BufferSize
    )   PURE;

    STDMETHOD_(PVOID,SystemAddress)
    (   THIS
    )   PURE;

    STDMETHOD_(PHYSICAL_ADDRESS,PhysicalAddress)
    (   THIS
    )   PURE;

    STDMETHOD_(PADAPTER_OBJECT,GetAdapterObject)
    (   THIS
    )   PURE;

    STDMETHOD_(void,CopyTo)
    (   THIS_
        IN      PVOID   Destination,
        IN      PVOID   Source,
        IN      ULONG   ByteCount
    )   PURE;

    STDMETHOD_(void,CopyFrom)
    (   THIS_
        IN      PVOID   Destination,
        IN      PVOID   Source,
        IN      ULONG   ByteCount
    )   PURE;

*/