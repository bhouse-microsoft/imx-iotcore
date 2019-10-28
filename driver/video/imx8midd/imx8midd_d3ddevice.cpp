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

Direct3DDevice::Direct3DDevice(IndirectDeviceContext * pContext, LUID AdapterLuid) : m_pContext(pContext), m_AdapterLuid(AdapterLuid)
{
}

Direct3DDevice::Direct3DDevice()
{
}

Direct3DDevice::~Direct3DDevice()
{
}

HRESULT Direct3DDevice::Init()
{	
	// The DXGI factory could be cached, but if a new render adapter appears on the system, a new factory needs to be
    // created. If caching is desired, check DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_DxgiFactory));
    if (FAILED(hr))
    {
        return hr;
    }

    // Find the specified render adapter
    hr = m_DxgiFactory->EnumAdapterByLuid(m_AdapterLuid, IID_PPV_ARGS(&m_Adapter));
    if (FAILED(hr))
    {
        return hr;
    }


    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(m_Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &m_Device, nullptr, &m_DeviceContext);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g. detachable GPU) or else the
        // system is in a transient state.
        return hr;
    }

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

    hr = m_Device->CreateTexture2D(&desc, NULL, &m_StagingTexture);
    if (FAILED(hr)) return hr;

    m_Device->GetImmediateContext(&m_ImmediateContext);

    return S_OK;
}
