// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
//
// Module Name:
//
//  imx8dod_driver.h
//
// Abstract:
//
//    This is i.MX 8M Display Only Driver initialization
//
// Environment:
//
//    Kernel mode only.
//
#pragma once

extern "C" DRIVER_INITIALIZE DriverEntry;

DXGKDDI_UNLOAD MX6DodDdiUnload;
