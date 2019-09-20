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

Direct3DDevice::Direct3DDevice(IndirectDeviceContext * pContext, LUID AdapterLuid) : m_pContext(pContext), AdapterLuid(AdapterLuid)
{

}

Direct3DDevice::Direct3DDevice()
{

}

Direct3DDevice::~Direct3DDevice()
{
    AdapterDWM->CloseKernelHandle(khAdapterDWM);
}

HRESULT Direct3DDevice::Init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&Adapter));
    if (FAILED(hr))
    {
        return hr;
    }


    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

    hr = Device->QueryInterface(IID_PPV_ARGS(&WarpPrivateAPI));
    if (FAILED(hr))
    {
        return hr;
    }

    DcssBase = nullptr;
    DprBase = nullptr;
    FrameBufferReg = nullptr;
    FrameBuffer = nullptr;
    FrameBufferPhysicalAddress.QuadPart = 0;
    BiosFrameBuffer = nullptr;
    BiosFrameBufferPhysicalAddress.QuadPart = 0;

    // Get the DWM adapter which should be our DOD adapter

    hr = Adapter->QueryInterface(IID_PPV_ARGS(&AdapterDWM));
    if (FAILED(hr))
    {
        return hr;
    }

    // Get the kernel handle for the DOD adapter
    khAdapterDWM = nullptr;
    hr = AdapterDWM->OpenKernelHandle(&khAdapterDWM);
    if (FAILED(hr))
    {
        return hr;
    }

    // Call the DOD adapter's escape to get access to registers and buffers
    D3DKMT_ESCAPE KmtEscape = { 0 };
    DriverEscape escape = { NULL };

    escape.code = 0;
  
    KmtEscape.hAdapter = (D3DKMT_HANDLE) (size_t) khAdapterDWM;
    KmtEscape.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    KmtEscape.Flags.HardwareAccess = 1;
    KmtEscape.pPrivateDriverData = &escape;
    KmtEscape.PrivateDriverDataSize = sizeof(escape);

    NTSTATUS Status = D3DKMTEscape(&KmtEscape);
    if (Status == STATUS_SUCCESS)
    {
        DcssBase = (uint32_t *) escape.info.dcssBase;
        DprBase =  (uint32_t *) ((size_t) DcssBase + 0x18000);
        FrameBufferReg = (uint32_t *)((size_t) DprBase + 0xc0);
        FrameBuffer = (uint32_t *) escape.info.frameBuffer;
        FrameBufferPhysicalAddress = escape.info.frameBufferPhysicalAddress;
        BiosFrameBuffer = (uint32_t *) escape.info.biosFrameBuffer;
        BiosFrameBufferPhysicalAddress = escape.info.biosFrameBufferPhysicalAddress;
    }

    // Create the event used to ensure rendering by warp is complete
    CompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (CompletionEvent == NULL)
        return HRESULT_FROM_WIN32(GetLastError());

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = m_pContext->m_modeWidth;
    desc.Height = m_pContext->m_modeHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    hr = Device->CreateTexture2D(&desc, NULL, &StagingTexture);
    if (FAILED(hr)) return hr;

    Device->GetImmediateContext(&ImmediateContext);

    return S_OK;
}
