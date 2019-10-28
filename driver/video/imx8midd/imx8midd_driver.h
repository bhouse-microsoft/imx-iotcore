#pragma once

#define NOMINMAX
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "Trace.h"

namespace Microsoft
{
    namespace WRL
    {
        namespace Wrappers
        {
            // Adds a wrapper for thread handles to the existing set of WRL handle wrapper classes
            typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
        }
    }
}

namespace Microsoft
{
    namespace IndirectDisp
    {
        // Forward declarations
        class SwapChainProcessor;
        class IndirectDeviceContext;

        /// <summary>
        /// Manages the creation and lifetime of a Direct3D render device.
        /// </summary>
        struct Direct3DDevice
        {
            Direct3DDevice(IndirectDeviceContext * pContext, LUID AdapterLuid);
            Direct3DDevice();
            ~Direct3DDevice();
            HRESULT Init();

            LUID m_AdapterLuid;
            Microsoft::WRL::ComPtr<IDXGIFactory5> m_DxgiFactory;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> m_Adapter;
            Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_DeviceContext;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> m_StagingTexture;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_ImmediateContext;
            IndirectDeviceContext * m_pContext;
        };

        /// <summary>
        /// Provides a sample implementation of an indirect display driver.
        /// </summary>
        class IndirectDeviceContext
        {
        public:
            IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
            virtual ~IndirectDeviceContext();

            void InitAdapter();
            void FinishInit();

            void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent);
            void UnassignSwapChain();

            NTSTATUS CommitModes(const IDARG_IN_COMMITMODES* pModes);

        protected:

            IDDCX_ADAPTER m_Adapter;
            IDDCX_MONITOR m_Monitor;

            std::unique_ptr<SwapChainProcessor> m_ProcessingThread;
            std::unique_ptr<Direct3DDevice> m_Direct3DDevice;

        public:
            static const DISPLAYCONFIG_VIDEO_SIGNAL_INFO s_KnownMonitorModes[];
            static const BYTE s_KnownMonitorEdid[];

            static IndirectDeviceContext * Get(IDDCX_MONITOR);
            static IndirectDeviceContext * Get(WDFDEVICE);
            static IndirectDeviceContext * Get(IDDCX_ADAPTER);

        public:
            bool       m_modeCommitted;
            uint32_t   m_modeWidth;
            uint32_t   m_modeHeight;
			void *	   m_pDcssBase;
			volatile uint32_t * m_pFrameBufferReg;
			uint32_t   m_uefiFrameBuffer;

			WDFDEVICE m_WdfDevice;

        };

        /// <summary>
        /// Manages a thread that consumes buffers from an indirect display swap-chain object.
        /// </summary>
        class SwapChainProcessor
        {
        public:
            SwapChainProcessor(IndirectDeviceContext * pContext, IDDCX_SWAPCHAIN hSwapChain, Direct3DDevice* pDevice, HANDLE NewFrameEvent);
            ~SwapChainProcessor();

        private:
            static DWORD CALLBACK RunThread(LPVOID Argument);

            void Run();
            void RunCore();

        public:
            IDDCX_SWAPCHAIN m_hSwapChain;
            Direct3DDevice * m_pDevice;
            HANDLE m_hAvailableBufferEvent;
            Microsoft::WRL::Wrappers::Thread m_hThread;
            Microsoft::WRL::Wrappers::Event m_hTerminateEvent;
            IndirectDeviceContext * m_pContext;
        };

    }
}

struct IndirectDeviceContextWrapper
{
    Microsoft::IndirectDisp::IndirectDeviceContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

