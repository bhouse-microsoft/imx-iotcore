// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void Log(int level, const char * prefix, char * file, int line, const char * msg, ...);

#define LOG_LEVEL_ERROR         0
#define LOG_LEVEL_WARNING       1
#define LOG_LEVEL_TRACE         2
#define LOG_LEVEL_INFO          3
#define LOG_LEVEL_VERBOSE       4

#define LOG_ERROR(msg, ...) Log(LOG_LEVEL_ERROR, "ERROR", __FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_LOW_MEMORY(msg, ...) Log(LOG_LEVEL_ERROR, "LOW MEMORY", __FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_WARNING(msg, ...) Log(LOG_LEVEL_WARNING, "WARNING", __FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_TRACE(msg, ...) Log(LOG_LEVEL_TRACE, "TRACE", __FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_INFORMATION(msg, ...) Log(LOG_LEVEL_INFO, "INFO", __FILE__, __LINE__, msg, __VA_ARGS__)
#define LOG_PRESENT(msg, ...) Log(LOG_LEVEL_VERBOSE, "PRESENT", __FILE__, __LINE__, msg, __VA_ARGS__)
 
#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
