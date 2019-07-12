// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void Log(int level, const char * prefix, const char * msg, ...);

#define LOG_LEVEL_ERROR         0
#define LOG_LEVEL_WARNING       1
#define LOG_LEVEL_TRACE         2
#define LOG_LEVEL_INFO          3
#define LOG_LEVEL_VERBOSE       4

#define LINESTRING_(x) #x
#define LINESTRING  LINESTRING_(__LINE__)
#define PREFIX(msg) msg __FILE__ "(" LINESTRING ") :"

#define LOG_ERROR(msg, ...) Log(LOG_LEVEL_ERROR, PREFIX("ERROR"), msg, __VA_ARGS__)
#define LOG_LOW_MEMORY(msg, ...) Log(LOG_LEVEL_ERROR, PREFIX("LOW MEMORY"), msg, __VA_ARGS__)
#define LOG_WARNING(msg, ...) Log(LOG_LEVEL_WARNING, PREFIX("WARNING"), msg, __VA_ARGS__)
#define LOG_TRACE(msg, ...) Log(LOG_LEVEL_TRACE, PREFIX("TRACE"), msg, __VA_ARGS__)
#define LOG_INFORMATION(msg, ...) Log(LOG_LEVEL_INFO, PREFIX("INFO: "), msg, __VA_ARGS__)
#define LOG_PRESENT(msg, ...) Log(LOG_LEVEL_VERBOSE, PREFIX("PRESENT:"), msg, __VA_ARGS__)
 
#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
