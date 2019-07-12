// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"

#include "imx8mdod_logging.h"

#include "imx8mdod_common.h"
#include "imx8mdod_device.h"
#include "imx8mdod_driver.h"

#include <stdarg.h>

MX6DOD_NONPAGED_SEGMENT_BEGIN; //==============================================

void Log(int level, const char * prefix, const char * msg, ...)
{
    va_list argp;
    va_start(argp, msg);
    vDbgPrintExWithPrefix(prefix, DPFLTR_IHVVIDEO_ID, level, msg, argp);
    va_end(argp);
}


MX6DOD_NONPAGED_SEGMENT_END; //================================================
