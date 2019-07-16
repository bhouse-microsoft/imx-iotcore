// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"

#include "imx8mdod_logging.h"
#include "imx8mdod_common.h"
#include "imx8mdod_device.h"
#include "imx8mdod_driver.h"

NONPAGED_SEGMENT_BEGIN; //==============================================

//
// Placement new and delete operators
//

_Use_decl_annotations_
void* operator new ( size_t, void* Ptr ) throw ()
{
    return Ptr;
}

void operator delete ( void*, void* ) throw ()
{}

_Use_decl_annotations_
void* operator new[] ( size_t, void* Ptr ) throw ()
{
    return Ptr;
}

void operator delete[] ( void*, void* ) throw ()
{}

NONPAGED_SEGMENT_END; //================================================
PAGED_SEGMENT_BEGIN; //=================================================

namespace { // static

    // This must be stored as a global variable since DxgkDdiUnload does not
    // supply a driver object pointer or context pointer.
    DRIVER_OBJECT* mx6GlobalDriverObjectPtr;

} // namespace "static"

_Use_decl_annotations_
void MX6DodDdiUnload ()
{
    PAGED_CODE();
    ASSERT_MAX_IRQL(PASSIVE_LEVEL);

    NT_ASSERT(mx6GlobalDriverObjectPtr);
    mx6GlobalDriverObjectPtr = nullptr;
}

PAGED_SEGMENT_END; //=====================================================
INIT_SEGMENT_BEGIN; //====================================================

_Use_decl_annotations_
NTSTATUS
DriverEntry (
    DRIVER_OBJECT* DriverObjectPtr,
    UNICODE_STRING* RegistryPathPtr
    )
{
    PAGED_CODE();
     
    LOG_INFORMATION(
        "(DriverObjectPtr = 0x%p, RegistryPathPtr = 0x%p)",
        DriverObjectPtr,
        RegistryPathPtr);

    // Initialize DDI function pointers and dxgkrnl
    auto dodInit = KMDDOD_INITIALIZATION_DATA();
    dodInit.Version = DXGKDDI_INTERFACE_VERSION;

    dodInit.DxgkDdiUnload = MX6DodDdiUnload;

    dodInit.DxgkDdiAddDevice = DEVICE::DdiAddDevice;
    dodInit.DxgkDdiStartDevice = DEVICE::DdiStartDevice;
    dodInit.DxgkDdiStopDevice = DEVICE::DdiStopDevice;
    dodInit.DxgkDdiResetDevice = DEVICE::DdiResetDevice;
    dodInit.DxgkDdiRemoveDevice = DEVICE::DdiRemoveDevice;
    dodInit.DxgkDdiDispatchIoRequest = DEVICE::DdiDispatchIoRequest;

    dodInit.DxgkDdiQueryChildRelations = DEVICE::DdiQueryChildRelations;
    dodInit.DxgkDdiQueryChildStatus = DEVICE::DdiQueryChildStatus;
    dodInit.DxgkDdiQueryDeviceDescriptor = DEVICE::DdiQueryDeviceDescriptor;
    dodInit.DxgkDdiSetPowerState = DEVICE::DdiSetPowerState;

    dodInit.DxgkDdiQueryAdapterInfo = DEVICE::DdiQueryAdapterInfo;
    dodInit.DxgkDdiSetPointerPosition = DEVICE::DdiSetPointerPosition;
    dodInit.DxgkDdiSetPointerShape = DEVICE::DdiSetPointerShape;

    dodInit.DxgkDdiIsSupportedVidPn = DEVICE::DdiIsSupportedVidPn;
    dodInit.DxgkDdiRecommendFunctionalVidPn = DEVICE::DdiRecommendFunctionalVidPn;
    dodInit.DxgkDdiEnumVidPnCofuncModality = DEVICE::DdiEnumVidPnCofuncModality;
    dodInit.DxgkDdiSetVidPnSourceVisibility = DEVICE::DdiSetVidPnSourceVisibility;
    dodInit.DxgkDdiCommitVidPn = DEVICE::DdiCommitVidPn;
    dodInit.DxgkDdiUpdateActiveVidPnPresentPath = DEVICE::DdiUpdateActiveVidPnPresentPath;

    dodInit.DxgkDdiRecommendMonitorModes = DEVICE::DdiRecommendMonitorModes;
    dodInit.DxgkDdiQueryVidPnHWCapability = DEVICE::DdiQueryVidPnHWCapability;
    dodInit.DxgkDdiPresentDisplayOnly = DEVICE::DdiPresentDisplayOnly;
    dodInit.DxgkDdiSetPowerComponentFState = DEVICE::DdiSetPowerComponentFState;
    dodInit.DxgkDdiStopDeviceAndReleasePostDisplayOwnership = DEVICE::DdiStopDeviceAndReleasePostDisplayOwnership;
    dodInit.DxgkDdiSystemDisplayEnable = DEVICE::DdiSystemDisplayEnable;
    dodInit.DxgkDdiSystemDisplayWrite = DEVICE::DdiSystemDisplayWrite;
    dodInit.DxgkDdiPowerRuntimeControlRequest = DEVICE::DdiPowerRuntimeControlRequest;

    NTSTATUS status = DxgkInitializeDisplayOnlyDriver(
            DriverObjectPtr,
            RegistryPathPtr,
            &dodInit);
            
    if (!NT_SUCCESS(status)) {
        LOG_ERROR(
            "DxgkInitializeDisplayOnlyDriver failed. (status = %!STATUS!, DriverObjectPtr = %p)",
            status,
            DriverObjectPtr);
            
        return status;
    }

    NT_ASSERT(mx6GlobalDriverObjectPtr == nullptr);
    mx6GlobalDriverObjectPtr = DriverObjectPtr;
    NT_ASSERT(status == STATUS_SUCCESS);
    return status;
} // DriverEntry (...)

INIT_SEGMENT_END; //====================================================

