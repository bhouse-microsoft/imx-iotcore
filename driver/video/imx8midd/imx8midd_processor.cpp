/*++

Copyright (c) Microsoft Corporation

Abstract:

    This module contains a sample implementation of an indirect display driver. See the included README.md file and the
    various TODO blocks throughout this file and all accompanying files for information on building a production driver.

    MSDN documentation on indirect displays can be found at https://msdn.microsoft.com/en-us/library/windows/hardware/mt761968(v=vs.85).aspx.

Environment:

    User Mode, UMDF

--*/

#include "imx8midd_driver.h"

#include <d3dkmthk.h>

using namespace std;
using namespace Microsoft::IndirectDisp;
using namespace Microsoft::WRL;

SwapChainProcessor::SwapChainProcessor(IndirectDeviceContext * pContext, IDDCX_SWAPCHAIN hSwapChain, Direct3DDevice * pDevice, HANDLE NewFrameEvent)
    : m_pContext(pContext), m_hSwapChain(hSwapChain), m_pDevice(pDevice), m_hAvailableBufferEvent(NewFrameEvent)
{
    m_hTerminateEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(m_hTerminateEvent.Get());

    if (m_hThread.Get())
    {
        // Wait for the thread to terminate
        WaitForSingleObject(m_hThread.Get(), INFINITE);
    }
}

DWORD CALLBACK SwapChainProcessor::RunThread(LPVOID Argument)
{
    reinterpret_cast<SwapChainProcessor*>(Argument)->Run();
    return 0;
}

void SwapChainProcessor::Run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristics(L"Distribution", &AvTask);
    if (AvTaskHandle) {
        RunCore();

        // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
        // provide a new swap-chain if necessary.
        WdfObjectDelete((WDFOBJECT)m_hSwapChain);
        m_hSwapChain = nullptr;

        AvRevertMmThreadCharacteristics(AvTaskHandle);
    }

}

void SwapChainProcessor::RunCore()
{
    // Get the DXGI device interface
    ComPtr<IDXGIDevice2> DxgiDevice;
    HRESULT hr = m_pDevice->m_Device.As(&DxgiDevice);
    if (FAILED(hr))
    {
        return;
    }

    IDARG_IN_SWAPCHAINSETDEVICE SetDevice = {};
    SetDevice.pDevice = DxgiDevice.Get();

    hr = IddCxSwapChainSetDevice(m_hSwapChain, &SetDevice);
    if (FAILED(hr))
    {
        return;
    }

    // Acquire and release buffers in a loop
    for (;;)
    {
        ComPtr<IDXGIResource> AcquiredBuffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER Buffer = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_hSwapChain, &Buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            HANDLE WaitHandles [] =
            {
                m_hAvailableBufferEvent,
                m_hTerminateEvent.Get()
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            }
            else if (WaitResult == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(WaitResult);
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            // Since we provided the "prefer physically contiguous" flag, we will get the physical address of the surface
            // if the system was able to allocate the surface physically contiguous
            PHYSICAL_ADDRESS address;
            address.QuadPart = (LONGLONG) Buffer.MetaData.PresentDisplayQPCTime;

            // Physical address is valid if non-zero.  We expect the high part of the address to be zero as we have
            // initialized the video memory system to reserve the physical heap only within the lower 4G of memory.
            if (address.HighPart == 0 && address.LowPart != 0 && m_pContext->m_pFrameBufferReg != NULL) {

                // Flip to the frame
				WDF_WRITE_REGISTER_ULONG(m_pContext->m_WdfDevice, (PULONG)m_pContext->m_pFrameBufferReg, address.LowPart);

            } else {
                // We need to copy our acquired surface to a staging buffer and then copy the staging buffer to the
                // bios frame buffer.
                // NOTE: we could reduce the amount we need to copy by using the dirty rect
                // information.  Since this path is not the exepcted path, we are keeping this simple for now and thus
                // copy the entire buffer.
                AcquiredBuffer.Attach(Buffer.MetaData.pSurface);

                ID3D11Resource * source = nullptr;
                hr = AcquiredBuffer->QueryInterface(IID_PPV_ARGS(&source));

                m_pDevice->m_ImmediateContext->CopyResource(m_pDevice->m_StagingTexture.Get(), source);

                D3D11_MAPPED_SUBRESOURCE mapping;
                hr = m_pDevice->m_ImmediateContext->Map(m_pDevice->m_StagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapping);

                if (SUCCEEDED(hr)) {
                    size_t rowBytes = m_pContext->m_modeWidth * sizeof(uint32_t);

                    // We expect our row pitch to match
                    if (mapping.RowPitch == rowBytes) {
#if 0
						// TODO: map frame buffer into driver's address space
						//       this requires that we also:
						//           report the frame buffer address range as a resource in ACPI
						//           add a mechanism to DXGKRNL that will allow device to report that it is not boot device
                        size_t totalBytes = rowBytes * m_pContext->m_modeHeight;
                        uint8_t * dst = (uint8_t *) m_pDevice->m_BiosFrameBuffer;
                        memcpy(dst, mapping.pData, totalBytes);
#endif

                        m_pDevice->m_ImmediateContext->Unmap(m_pDevice->m_StagingTexture.Get(), 0);
                    } else {
                        // unexpected ... perhaps we should paint the frame buffer blue to indicate the situation
                    }
                }

				if (m_pContext->m_pFrameBufferReg != NULL)
				{
					if (WDF_READ_REGISTER_ULONG(m_pContext->m_WdfDevice, (PULONG)m_pContext->m_pFrameBufferReg) != m_pContext->m_uefiFrameBuffer)
						WDF_WRITE_REGISTER_ULONG(m_pContext->m_WdfDevice, (PULONG)m_pContext->m_pFrameBufferReg, m_pContext->m_uefiFrameBuffer);
				}

                AcquiredBuffer->Release();

            }

            hr = IddCxSwapChainFinishedProcessingFrame(m_hSwapChain);
            if (FAILED(hr))
            {
                break;
            }

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}
