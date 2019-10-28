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

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;
EVT_WDF_DEVICE_PREPARE_HARDWARE IddSampleDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE IddSampleDeviceReleaseHardware;

EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hInstance,
    _In_ UINT dwReason,
    _In_opt_ LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT  pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    WDF_DRIVER_CONFIG Config;
    NTSTATUS Status;

    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

    WDF_DRIVER_CONFIG_INIT(&Config,
        IddSampleDeviceAdd
    );

	Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit)
{
    NTSTATUS Status = STATUS_SUCCESS;
    WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);

	// Register for power callbacks - in this sample only power-on is needed
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
    PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
	PnpPowerCallbacks.EvtDevicePrepareHardware = IddSampleDevicePrepareHardware;
	PnpPowerCallbacks.EvtDeviceReleaseHardware = IddSampleDeviceReleaseHardware;
	WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

    IDD_CX_CLIENT_CONFIG IddConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // IddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;

    IddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;

    IddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
    IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes = IddSampleMonitorGetDefaultModes;
    IddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
    IddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
    IddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
    IddConfig.EvtIddCxMonitorUnassignSwapChain = IddSampleMonitorUnassignSwapChain;

    Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    WDF_OBJECT_ATTRIBUTES Attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
    Attr.EvtCleanupCallback = [](WDFOBJECT Object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
        if (pContext)
        {
            pContext->Cleanup();
        }
    };

    WDFDEVICE WdfDevice = nullptr;
    Status = WdfDeviceCreate(&pDeviceInit, &Attr, &WdfDevice);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Status = IddCxDeviceInitialize(WdfDevice);

    // Create a new device context object and attach it to the WDF device object
    auto* pContextWrapper = WdfObjectGet_IndirectDeviceContextWrapper(WdfDevice);
    pContextWrapper->pContext = new IndirectDeviceContext(WdfDevice);

    return Status;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceD0Entry(WDFDEVICE WdfDevice, WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);

    // This function is called by WDF to start the device in the fully-on power state.

	IndirectDeviceContext * pContext = IndirectDeviceContext::Get(WdfDevice);
    pContext->InitAdapter();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleDevicePrepareHardware(WDFDEVICE WdfDevice, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated)
{
	UNREFERENCED_PARAMETER(ResourcesRaw);
	UNREFERENCED_PARAMETER(ResourcesTranslated);

	IndirectDeviceContext * pContext = IndirectDeviceContext::Get(WdfDevice);

	ULONG resourceCount = WdfCmResourceListGetCount(ResourcesTranslated);

	pContext->m_pDcssBase = NULL;
	pContext->m_pFrameBufferReg = NULL;
	pContext->m_uefiFrameBuffer = 0;

	for (ULONG i = 0; i < resourceCount; i++)
	{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);

		if (desc->Type == CmResourceTypeMemory && desc->u.Memory.Start.QuadPart == 0x32e00000ULL && desc->u.Memory.Length == 0x40000) {
			NTSTATUS status = WdfDeviceMapIoSpace(WdfDevice, desc->u.Memory.Start, desc->u.Memory.Length, MmNonCached, &pContext->m_pDcssBase);
			if (status == STATUS_SUCCESS)
			{
				pContext->m_pFrameBufferReg = (uint32_t *)((uint8_t *) pContext->m_pDcssBase + 0x180c0);
				pContext->m_uefiFrameBuffer = WDF_READ_REGISTER_ULONG(WdfDevice, (PULONG) pContext->m_pFrameBufferReg);
			}
		}
	}

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleDeviceReleaseHardware(WDFDEVICE WdfDevice, WDFCMRESLIST ResourcesTranslated)
{
	UNREFERENCED_PARAMETER(ResourcesTranslated);

	IndirectDeviceContext * pContext = IndirectDeviceContext::Get(WdfDevice);

	if (pContext->m_pDcssBase != NULL)
		WdfDeviceUnmapIoSpace(WdfDevice, pContext->m_pDcssBase, 0x40000);

	return STATUS_SUCCESS;
}


#pragma region DDI Callbacks

_Use_decl_annotations_
NTSTATUS IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject, const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs)
{
	// This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.

    if (NT_SUCCESS(pInArgs->AdapterInitStatus))
    {
        IndirectDeviceContext * pContext = IndirectDeviceContext::Get(AdapterObject);
        pContext->FinishInit();
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject, const IDARG_IN_COMMITMODES* pInArgs)
{	
	return IndirectDeviceContext::Get(AdapterObject)->CommitModes(pInArgs);
}

_Use_decl_annotations_
NTSTATUS IddSampleParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs, IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs)
{
#if 0
    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for an EDID by parsing it. In
    // this sample driver, we hard-code the EDID, so this function can generate known modes.
    // ==============================

    pOutArgs->MonitorModeBufferOutputCount = ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes);

    if (pInArgs->MonitorModeBufferInputCount < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes))
    {
        // Return success if there was no buffer, since the caller was only asking for a count of modes
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }
    else
    {
        // Copy the known modes to the output buffer
        for (DWORD ModeIndex = 0; ModeIndex < ARRAYSIZE(IndirectDeviceContext::s_KnownMonitorModes); ModeIndex++)
        {
            pInArgs->pMonitorModes[ModeIndex].Size = sizeof(IDDCX_MONITOR_MODE);
            pInArgs->pMonitorModes[ModeIndex].Origin = IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR;
            pInArgs->pMonitorModes[ModeIndex].MonitorVideoSignalInfo = IndirectDeviceContext::s_KnownMonitorModes[ModeIndex];
        }

        // Set the preferred mode as represented in the EDID
        pOutArgs->PreferredMonitorModeIdx = 0;

        return STATUS_SUCCESS;
    }
#else
    pInArgs; // unused

    pOutArgs->MonitorModeBufferOutputCount = 0;
    pOutArgs->PreferredMonitorModeIdx = 0;

    return STATUS_SUCCESS;
#endif
}

/// <summary>
/// Creates a target mode from the fundamental mode attributes.
/// </summary>
void CreateTargetMode(DISPLAYCONFIG_VIDEO_SIGNAL_INFO& Mode, UINT Width, UINT Height, UINT VSync)
{
    Mode.totalSize.cx = Mode.activeSize.cx = Width;
    Mode.totalSize.cy = Mode.activeSize.cy = Height;
    Mode.AdditionalSignalInfo.vSyncFreqDivider = 1;
    Mode.AdditionalSignalInfo.videoStandard = 255;
    Mode.vSyncFreq.Numerator = VSync;
    Mode.vSyncFreq.Denominator = Mode.hSyncFreq.Denominator = 1;
    Mode.hSyncFreq.Numerator = VSync * Height;
    Mode.scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    Mode.pixelRate = VSync * Width * Height;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorGetDefaultModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs, IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);
    UNREFERENCED_PARAMETER(pInArgs);
    UNREFERENCED_PARAMETER(pOutArgs);

	// Should never be called since we create a single monitor with a known EDID in this sample driver.

    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    // ==============================
    if (0 == pInArgs->DefaultMonitorModeBufferInputCount)
    {
        //
        // Report to IddCx number of default modes
        //
        pOutArgs->DefaultMonitorModeBufferOutputCount = 1;
    }
    else
    {
        IDDCX_MONITOR_MODE * defaultMode = pInArgs->pDefaultMonitorModes;
        defaultMode->Size = sizeof(IDDCX_MONITOR_MODE);
        defaultMode->Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER;

        CreateTargetMode(defaultMode->MonitorVideoSignalInfo, 1280, 720, 60);
        pOutArgs->DefaultMonitorModeBufferOutputCount = (UINT)1;
        pOutArgs->PreferredMonitorModeIdx = 0;

        // vSyncFreqDivider must be zero for monitor modes
        defaultMode->MonitorVideoSignalInfo.AdditionalSignalInfo.vSyncFreqDivider = 0;
    }

    return STATUS_SUCCESS;
}

void CreateTargetMode(IDDCX_TARGET_MODE& Mode, UINT Width, UINT Height, UINT VSync)
{
    Mode.Size = sizeof(Mode);
    CreateTargetMode(Mode.TargetVideoSignalInfo.targetVideoSignalInfo, Width, Height, VSync);
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject, const IDARG_IN_QUERYTARGETMODES* pInArgs, IDARG_OUT_QUERYTARGETMODES* pOutArgs)
{
    UNREFERENCED_PARAMETER(MonitorObject);

	// For now, we will report only only mode which matches the mode set by UEFI

    // TODO: Make escape down to display only driver to get boot mode settings
    //       For now, we hard code 1280 x 720

    vector<IDDCX_TARGET_MODE> TargetModes(1);

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.
    CreateTargetMode(TargetModes[0], 1280, 720, 60);

    pOutArgs->TargetModeBufferOutputCount = (UINT)TargetModes.size();

    if (pInArgs->TargetModeBufferInputCount >= TargetModes.size())
    {
        copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject, const IDARG_IN_SETSWAPCHAIN* pInArgs)
{
    IndirectDeviceContext * pContext = IndirectDeviceContext::Get(MonitorObject);
    pContext->AssignSwapChain(pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject)
{
    IndirectDeviceContext * pContext = IndirectDeviceContext::Get(MonitorObject);
    pContext->UnassignSwapChain();
    return STATUS_SUCCESS;
}

#pragma endregion