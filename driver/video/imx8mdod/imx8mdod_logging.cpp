// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "precomp.h"

#include "imx8mdod_logging.h"

#include "imx8mdod_common.h"
#include "imx8mdod_device.h"
#include "imx8mdod_driver.h"

#include <stdarg.h>
#include <ntstrsafe.h>

NONPAGED_SEGMENT_BEGIN; //==============================================

void Log(int level, const char * prefix, char * filepath, int line, const char * msg, ...)
{
    char messageBuffer[128];
    char * filename = strrchr(filepath, '\\');

    filename = (filename == NULL ? filepath : filename);

    va_list argp;
    va_start(argp, msg);
    RtlStringCbVPrintfA(messageBuffer, sizeof(messageBuffer), msg, argp);
    va_end(argp);

    DbgPrintEx(DPFLTR_IHVVIDEO_ID, level, "%s:%s [%s:%d]\n", prefix, messageBuffer, filename, line);
}

NONPAGED_SEGMENT_END; //================================================
