// Wrapper class around IDmaChannel
// generated with help of Gemini and chatGPT
// so not copyrightable. 
// yes I know, bad for society, but i'm no x86 ASM expert

class CMyDmaChannel : public IDmaChannel {
private:
    IDmaChannel* m_RealDmaChannel;
    LONG         m_RefCount;
	BOOLEAN		 g_bHasClFlush;
	BOOLEAN      m_UseAlignedView;
	ULONG        m_RequestedBufferSize;
	PVOID        m_AlignedSystemAddress;
	PHYSICAL_ADDRESS m_AlignedPhysicalAddress;

	enum { DMA_BUFFER_ALIGNMENT = 128 };

public:
    CMyDmaChannel(IDmaChannel* RealChannel) {
        m_RealDmaChannel = RealChannel;
        m_RealDmaChannel->AddRef();
        m_RefCount = 1;

		// Check for SSE2 / CLFLUSH support
		ULONG edx_feat;
		__asm {
			pushad
			mov eax, 1
			cpuid
			mov edx_feat, edx
			popad
        }	
		g_bHasClFlush = (edx_feat & (1 << 19)) != 0;
		m_UseAlignedView = FALSE;
		m_RequestedBufferSize = 0;
		m_AlignedSystemAddress = NULL;
		m_AlignedPhysicalAddress.QuadPart = 0;
		DbgPrint("SSE2 %d \n", g_bHasClFlush);
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
            m_RealDmaChannel->Release();
            delete this;
        }
        return ul;
    }

    // --- IDmaChannel Overrides ---
    STDMETHODIMP AllocateBuffer(ULONG BufferSize, PPHYSICAL_ADDRESS PhysicalAddressConstraint) {
		if (BufferSize > (MAXULONG - (DMA_BUFFER_ALIGNMENT - 1))) {
			return STATUS_INVALID_BUFFER_SIZE;
		}
		ULONG realBufferSize = BufferSize + (DMA_BUFFER_ALIGNMENT - 1);
        NTSTATUS ntStatus = m_RealDmaChannel->AllocateBuffer(realBufferSize, PhysicalAddressConstraint);
		if (NT_SUCCESS(ntStatus)) {
			PHYSICAL_ADDRESS realPhysicalAddress = m_RealDmaChannel->PhysicalAddress();
			PVOID realSystemAddress = m_RealDmaChannel->SystemAddress();
			ULONG_PTR alignedOffset;
			ULONG_PTR alignedVirtual;

			alignedOffset = (ULONG_PTR)((DMA_BUFFER_ALIGNMENT - (realPhysicalAddress.QuadPart & (DMA_BUFFER_ALIGNMENT - 1)))
				& (DMA_BUFFER_ALIGNMENT - 1));
			alignedVirtual = (ULONG_PTR)realSystemAddress + alignedOffset;

			m_AlignedPhysicalAddress.QuadPart = realPhysicalAddress.QuadPart + alignedOffset;
			m_AlignedSystemAddress = (PVOID)alignedVirtual;
			m_RequestedBufferSize = BufferSize;
			m_UseAlignedView = TRUE;

			if ((alignedVirtual & (DMA_BUFFER_ALIGNMENT - 1)) != 0 ||
				(m_AlignedPhysicalAddress.QuadPart & (DMA_BUFFER_ALIGNMENT - 1)) != 0 ||
				(m_RealDmaChannel->AllocatedBufferSize() < (alignedOffset + BufferSize))) {
				DbgPrint("Aligned DMA view invalid (off=%lu req=%lu alloc=%lu)\n",
					(ULONG)alignedOffset,
					BufferSize,
					m_RealDmaChannel->AllocatedBufferSize());
				m_RealDmaChannel->FreeBuffer();
				m_AlignedSystemAddress = NULL;
				m_AlignedPhysicalAddress.QuadPart = 0;
				m_RequestedBufferSize = 0;
				m_UseAlignedView = FALSE;
				ntStatus = STATUS_DATATYPE_MISALIGNMENT;
			}
		}
        return ntStatus;
    }
	STDMETHODIMP_(void) FreeBuffer(void) {
		m_AlignedSystemAddress = NULL;
		m_AlignedPhysicalAddress.QuadPart = 0;
		m_RequestedBufferSize = 0;
		m_UseAlignedView = FALSE;
		m_RealDmaChannel->FreeBuffer();
	}

    STDMETHODIMP_(PVOID) SystemAddress() {
		if (m_UseAlignedView) {
			return m_AlignedSystemAddress;
		}
        return m_RealDmaChannel->SystemAddress();
    }

    STDMETHODIMP_(void) CopyTo(
		IN      PVOID   Destination,
        IN      PVOID   Source,
        IN      ULONG   ByteCount) {

		// 1. Let the real PortCls buffer copy happen first.
		// This moves data from the "S-Buffer" to the "C-Buffer" (Destination).
		m_RealDmaChannel->CopyTo(Destination, Source, ByteCount);

		// 2. Manual Cache Invalidation for AMD 6xx-9xx Coherency.
		// We flush the 'Destination' because that is the DMA-accessible RAM.
	    if (g_bHasClFlush) {        
	        // We use a 64-byte stride (standard x86 cache line size)
			ULONG_PTR start = (ULONG_PTR)Destination & ~((ULONG_PTR)63);
			ULONG_PTR end = ((ULONG_PTR)Destination + ByteCount + 63) & ~((ULONG_PTR)63);
		    for (ULONG_PTR p = start; p < end; p += 64) {
			    __asm {
				    mov eax, p
	                _emit 0x0F // CLFLUSH [eax]
		            _emit 0xAE
			        _emit 0x38
				}
			}

			// 3. Memory Fence: Ensures all flushes are globally visible 
			// before the ISR/DPC returns and the HDA controller starts reading.
			__asm {
			    _emit 0x0F // MFENCE
			    _emit 0xAE
			    _emit 0xF0
			}
		}
	}

	STDMETHODIMP_(void) CopyFrom(
		IN      PVOID   Destination,
        IN      PVOID   Source,
        IN      ULONG   ByteCount) {
		//should not need to flush on copies out of the buffer?
		//revisit this when adding capture / recording support.
        m_RealDmaChannel->CopyFrom(Destination, Source, ByteCount);
    }

    // --- Pass-through the rest ---
	STDMETHODIMP_(ULONG) TransferCount() { return m_RealDmaChannel->TransferCount();}
	STDMETHODIMP_(ULONG) MaximumBufferSize() { return m_RealDmaChannel->MaximumBufferSize(); }
    STDMETHODIMP_(ULONG) AllocatedBufferSize() {
		if (m_UseAlignedView) {
			return m_RequestedBufferSize;
		}
		return m_RealDmaChannel->AllocatedBufferSize();
	}
	STDMETHODIMP_(ULONG) BufferSize() {
		if (m_UseAlignedView) {
			return m_RequestedBufferSize;
		}
		return m_RealDmaChannel->BufferSize();
	}
	STDMETHODIMP_(void) SetBufferSize(ULONG BufferSize) {
		m_RequestedBufferSize = BufferSize;
		m_RealDmaChannel->SetBufferSize(BufferSize);
	}
    STDMETHODIMP_(PHYSICAL_ADDRESS) PhysicalAddress() {
		if (m_UseAlignedView) {
			return m_AlignedPhysicalAddress;
		}
		return m_RealDmaChannel->PhysicalAddress();
	}
    STDMETHODIMP_(PADAPTER_OBJECT) GetAdapterObject() { return m_RealDmaChannel->GetAdapterObject(); }
};
