//=============================================================================
// FILE: svrProfile.h
//                  Copyright (c) 2015 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
//
//==============================================================================
#ifndef _SVR_API_PROFILE_H_
#define _SVR_API_PROFILE_H_

#include "svrUtil.h"

#if defined(SVR_PROFILING_ENABLED)

#if defined(SVR_PROFILE_TELEMETRY) && defined(SVR_PROFILE_ATRACE)
#error "Cannot enable both ATrace and Telemetry profiling simultaneously"
#endif

#if defined(SVR_PROFILE_TELEMETRY)

#define TM_API_PTR gTelemetryAPI
#include "rad_tm.h"

extern tm_api* gTelemetryAPI;

#define ERROR_BUILD_DISABLED    REQUIRED_REPLACE(TMERR_DISABLED, (TmErrorCode)0x0001)

#define LOG_EXCLAMATION         REQUIRED_REPLACE(TMMF_ICON_EXCLAMATION, 0)
#define LOG_NOTE                REQUIRED_REPLACE(TMMF_ICON_NOTE, 0)
#define LOG_WTF                 REQUIRED_REPLACE(TMMF_ICON_WTF, 0)

#define LOG_ZONE_SUBLABEL       REQUIRED_REPLACE(TMMF_ZONE_SUBLABEL, 0)
#define LOG_ZONE_LABEL          REQUIRED_REPLACE(TMMF_ZONE_LABEL, 0)

#define REQUIRED(required_code) required_code
#define REQUIRED_REPLACE(required_code, replacement_code) required_code

bool InitializeTelemetry();
void ShutdownTelemetry();

#define GROUP_GENERIC               REQUIRED_REPLACE(1 << 0, 0)
#define GROUP_SENSORS               REQUIRED_REPLACE(1 << 1, 0)
#define GROUP_TIMEWARP              REQUIRED_REPLACE(1 << 2, 0)
#define GROUP_WORLDRENDER           REQUIRED_REPLACE(1 << 3, 0)

#define PROFILE_FASTTIME() REQUIRED_REPLACE(tmFastTime(), 0);

#define PROFILE_INITIALIZE() REQUIRED_REPLACE(InitializeTelemetry(), ERROR_BUILD_DISABLED)

#define PROFILE_SHUTDOWN() REQUIRED(ShutdownTelemetry())

#define PROFILE_SCOPE_FILTERED(mask, threshold, flags, format, ...) REQUIRED(tmZoneFiltered(mask, threshold, flags, format, ##__VA_ARGS__))

#define PROFILE_SCOPE(mask, flags, format, ...) REQUIRED(tmZone(mask, flags, format, ##__VA_ARGS__))

#define PROFILE_SCOPE_DEFAULT(mask) PROFILE_SCOPE(mask, TMZF_NONE, __FUNCTION__)
#define PROFILE_SCOPE_IDLE(mask) PROFILE_SCOPE(mask, TMZF_IDLE, __FUNCTION__)
#define PROFILE_SCOPE_STALL(mask) PROFILE_SCOPE(mask, TMZF_STALL, __FUNCTION__)

#define PROFILE_TICK() REQUIRED(if (!gTelemetryAPI) tmTick(0))

#define PROFILE_EXIT(mask) REQUIRED(tmLeave(mask))

#define PROFILE_EXIT_EX(mask, match_id, thread_id, filename, line) REQUIRED(tmLeaveEx(mask, match_id, thread_id, filename, line))

#define PROFILE_TRY_LOCK(mask, ptr, lock_name, ... ) REQUIRED(tmTryLock(mask, ptr, lock_name, ##__VA_ARGS__ ))

#define PROFILE_TRY_LOCK_EX(mask, matcher, threshold, filename, line, ptr, lock_name, ... ) REQUIRED(tmTryLockEx(mask, matcher, threshold, filename, line, ptr, lock_name, ##__VA_ARGS__))

#define PROFILE_END_TRY_LOCK(mask, ptr, result ) REQUIRED( tmEndTryLock(mask, ptr, result ) )

#define PROFILE_END_TRY_LOCK_EX(mask, match_id, filename, line, ptr, result ) REQUIRED( tmEndTryLockEx(mask, match_id, filename, line, ptr, result ) )

#define PROFILE_BEGIN_TIME_SPAN(mask, id, flags, name_format, ... ) REQUIRED( tmBeginTimeSpan(mask, id, flags, name_format, ##__VA_ARGS__ ) )

#define PROFILE_END_TIME_SPAN(mask, id, flags, name_format, ... ) REQUIRED( tmEndTimeSpan(mask, id, flags, name_format, ##__VA_ARGS__ ) )

#define PROFILE_BEGIN_TIME_SPAN_AT(mask, id, flags, timestamp, name_format, ... ) REQUIRED( tmBeginTimeSpanAt(mask, id, flags, timestamp, name_format, ##__VA_ARGS__ ) )

#define PROFILE_END_TIME_SPAN_AT(mask, id, flags, timestamp, name_format, ... ) REQUIRED( tmEndTimeSpanAt(mask, id, flags, timestamp, name_format, ##__VA_ARGS__ ) )

#define PROFILE_SIGNAL_LOCK_COUNT(mask, ptr, count, description, ... ) REQUIRED( tmSignalLockCount(mask, ptr, count, description, ##__VA_ARGS__ ) )

#define PROFILE_SET_LOCK_STATE(mask, ptr, state, description, ... ) REQUIRED( tmSetLockState(mask, ptr, state, description, ##__VA_ARGS__ ) )

#define PROFILE_SET_LOCK_STATE_EX(mask, filename, line, ptr, state, description, ... ) REQUIRED( tmSetLockStateEx(mask, filename, line, ptr, state, description, ##__VA_ARGS__ ) )

#define PROFILE_SET_LOCK_STATE_MIN_TIME(mask, buf, ptr, state, description, ... ) REQUIRED( tmSetLockStateMinTime(mask, buf, ptr, state, description, ##__VA_ARGS__ ) )

#define PROFILE_SET_LOCK_STATE_MIN_TIME_EX(mask, buf, filename, line, ptr, state, description, ... ) REQUIRED(tmSetLockStateMinTimeEx(mask, buf, filename, line, ptr, state, description, ##__VA_ARGS__ ) )

#define PROFILE_THREAD_NAME(mask, thread_id, name_format, ... ) REQUIRED( tmThreadName(mask, thread_id, name_format, ##__VA_ARGS__ ) )

#define PROFILE_LOCK_NAME(mask, ptr, name_format, ... ) REQUIRED(tmLockName(mask, ptr, name_format, ##__VA_ARGS__ ) )

#define PROFILE_EMIT_ACCUMULATION_ZONE(mask, zone_flags, start, count, total, zone_format, ... ) REQUIRED(tmEmitAccumulationZone(mask, zone_flags, start, count, total, zone_format, ##__VA_ARGS__ ) )

#define PROFILE_SET_VARIABLE(mask, key, value_format, ... ) REQUIRED(tmSetVariable(mask, key, value_format, ##__VA_ARGS__ ) )

#define PROFILE_SET_TIMELINE_SECTION_NAME(mask, name_format, ... ) REQUIRED(tmSetTimelineSectionName(mask, name_format, ##__VA_ARGS__ ) )

#define PROFILE_ENTER(mask, flags, zone_name, ... ) REQUIRED( tmEnter(mask, flags, zone_name, ##__VA_ARGS__ ) )

#define PROFILE_ENTER_EX(mask, match_id, thread_id, threshold, filename, line, flags, zone_name, ... ) REQUIRED(tmEnterEx(mask, match_id, thread_id, threshold, filename, line, flags, zone_name, ##__VA_ARGS__ ) )

#define PROFILE_ALLOC(mask, ptr, size, description, ... ) REQUIRED( tmAlloc(mask, ptr, size, description, ##__VA_ARGS__ ) )

#define PROFILE_ALLOC_EX(mask, filename, line_number, ptr, size, description, ... ) REQUIRED(tmAllocEx(mask, filename, line_number, ptr, size, description, ##__VA_ARGS__ ) )

#define PROFILE_FREE(mask, ptr) REQUIRED(tmFree(mask, ptr))

#define PROFILE_MESSAGE(mask, flags, format_string, ... ) REQUIRED(tmMessage(mask, flags, format_string, __VA_ARGS__ ))
#define PROFILE_LOG(mask, flags, format_string, ... ) PROFILE_MESSAGE(mask, TMMF_SEVERITY_LOG|flags, format_string, __VA_ARGS__ )
#define PROFILE_WARNING(mask, flags, format_string, ... ) PROFILE_MESSAGE(mask, TMMF_SEVERITY_WARNING|flags, format_string, __VA_ARGS__ )
#define PROFILE_ERROR(mask, flags, format_string, ... ) PROFILE_MESSAGE(mask, TMMF_SEVERITY_ERROR|flags, format_string, __VA_ARGS__ )

#define PROFILE_PLOT(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlot(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_F32(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlotF32(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_F64(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlotF64(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_I32(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlotI32(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_U32(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlotU32(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_I64(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlotI64(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_U64(mask, type, flags, value, name_format, ... ) REQUIRED( tmPlotU64(mask, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_PLOT_AT(mask, timestamp, type, flags, value, name_format, ... ) REQUIRED ( tmPlotAt(mask, timestamp, type, flags, value, name_format, ##__VA_ARGS__ ) )

#define PROFILE_BLOB(mask, data, data_size, plugin_identifier, blob_name, ...) REQUIRED( tmBlob(mask, data, data_size, plugin_identifier, blob_name, ##__VA_ARGS__ ) )

#define PROFILE_DISJOINT_BLOB(mask, num_pieces, data, data_sizes, plugin_identifier, blob_name, ... ) REQUIRED( tmDisjointBlob(mask, num_pieces, data, data_sizes, plugin_identifier, blob_name, ##__VA_ARGS__ ) )

#define PROFILE_SEND_CALLSTACK(mask, callstack) REQUIRED(tmSendCallStack(mask, callstack))

//End SVR_PROFILE_TELEMETRY
#elif defined(SVR_PROFILE_ATRACE)

#include <time.h>
//#include "android/trace.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

extern void *(*ATrace_beginSection) (const char* sectionName);
extern void *(*ATrace_endSection) (void);
extern void *(*ATrace_isEnabled) (void);

void InitializeATrace();
void ShutdownATrace();

#define ATRACE_INT(name,val)
#define ATRACE_INT64(name,val)

#define GROUP_GENERIC
#define GROUP_SENSORS
#define GROUP_TIMEWARP
#define GROUP_WORLDRENDER

#define PROFILE_FASTTIME()

//PROFILE_INITIALIZE
#define PROFILE_INITIALIZE() InitializeATrace()

//PROFILE_SHUTDOWN
#define PROFILE_SHUTDOWN() ShutdownATrace()

#define PROFILE_SCOPE_FILTERED(context, threshold, flags, format, ...)

//PROFILE_SCOPE

#define SVR_ATRACE_BEGIN(format, ...) \
   { \
       static char atrace_begin_buf[1024]; \
	   if (strchr(format,'%')){ \
		   snprintf(atrace_begin_buf, 1024, format,##__VA_ARGS__); \
		   ATrace_beginSection(atrace_begin_buf); \
	   }else{ \
	       ATrace_beginSection(format); \
	   } \
   }

#define PROFILE_SCOPE(context, flags, format, ...) SVR_ATRACE_BEGIN(format,##__VA_ARGS__)

#define PROFILE_SCOPE_END() ATrace_endSection()

//PROFILE_SCOPE_DEFAULT
#define PROFILE_SCOPE_DEFAULT(context) PROFILE_SCOPE(context, TMZF_NONE, __FUNCTION__)
#define PROFILE_SCOPE_IDLE(context)
#define PROFILE_SCOPE_STALL(context)

//PROFILE_TICK
#define PROFILE_TICK()
/*
{ \
    struct timespec now; \
    clock_gettime(CLOCK_MONOTONIC, &now); \
    ATRACE_INT64("svr_tick_ns" ,(int64_t) now.tv_sec*1000000000LL + now.tv_nsec); \
    }
*/

// PROFILE_EXIT
#define PROFILE_EXIT(context) ATrace_endSection()

#define PROFILE_EXIT_EX(context, match_id, thread_id, filename, line)

#define PROFILE_TRY_LOCK(context, ptr, lock_name, ... )

#define PROFILE_TRY_LOCK_EX(context, matcher, threshold, filename, line, ptr, lock_name, ... )

#define PROFILE_END_TRY_LOCK(context, ptr, result )

#define PROFILE_END_TRY_LOCK_EX(context, match_id, filename, line, ptr, result )

// PROFILE_BEGIN_TIME_SPAN_AT
#define PROFILE_BEGIN_TIME_SPAN(context, id, flags, name_format, ... )

// PROFILE_END_TIME_SPAN
#define PROFILE_END_TIME_SPAN(context, id, flags, name_format, ... )

#define PROFILE_BEGIN_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... )

#define PROFILE_END_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... )

#define PROFILE_SIGNAL_LOCK_COUNT(context, ptr, count, description, ... )

#define PROFILE_SET_LOCK_STATE(context, ptr, state, description, ... )

#define PROFILE_SET_LOCK_STATE_EX(context, filename, line, ptr, state, description, ... )

#define PROFILE_SET_LOCK_STATE_MIN_TIME(context, buf, ptr, state, description, ... )

#define PROFILE_SET_LOCK_STATE_MIN_TIME_EX(context, buf, filename, line, ptr, state, description, ... )

// PROFILE_THREAD_NAME
#define PROFILE_THREAD_NAME(context, thread_id, name_format, ... ) ATRACE_INT(name_format, thread_id)

#define PROFILE_LOCK_NAME(context, ptr, name_format, ... )

#define PROFILE_EMIT_ACCUMULATION_ZONE(context, zone_flags, start, count, total, zone_format, ... )

#define PROFILE_SET_VARIABLE(context, key, value_format, ... )

#define PROFILE_SET_TIMELINE_SECTION_NAME(context, name_format, ... )

//PROFILE_ENTER
#define PROFILE_ENTER(context, flags, zone_name, ... ) SVR_ATRACE_BEGIN(zone_name,##__VA_ARGS__)

#define PROFILE_ENTER_EX(context, match_id, thread_id, threshold, filename, line, flags, zone_name, ... )

#define PROFILE_ALLOC(context, ptr, size, description, ... )

#define PROFILE_ALLOC_EX(context, filename, line_number, ptr, size, description, ... )

#define PROFILE_FREE(context, ptr) ATrace_endSection()

//PROFILE_MESSAGE
#define PROFILE_MESSAGE(context, flags, format_string, ... )
#define PROFILE_LOG(context, flags, format_string, ... )
#define PROFILE_WARNING(context, flags, format_string, ... )
#define PROFILE_ERROR(context, flags, format_string, ... )

#define PROFILE_PLOT(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_F32(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_F64(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_I32(context, type, flags, value, name_format, ... ) ATRACE_INT(name_format, value)

#define PROFILE_PLOT_U32(context, type, flags, value, name_format, ... ) ATRACE_INT(name_format, (int)value)

#define PROFILE_PLOT_I64(context, type, flags, value, name_format, ... ) ATRACE_INT64(name_format, value)

#define PROFILE_PLOT_U64(context, type, flags, value, name_format, ... ) ATRACE_INT64(name_format, (int64_t)value)

#define PROFILE_PLOT_AT(context, timestamp, type, flags, value, name_format, ... )

#define PROFILE_BLOB(context, data, data_size, plugin_identifier, blob_name, ...)

#define PROFILE_DISJOINT_BLOB(context, num_pieces, data, data_sizes, plugin_identifier, blob_name, ... )

#define PROFILE_SEND_CALLSTACK(context, callstack)

#endif //defined(SVR_PROFILE_ATRACE)

#else  //defined(SVR_PROFILING_ENABLED)

#define GROUP_GENERIC              
#define GROUP_SENSORS              
#define GROUP_TIMEWARP              
#define GROUP_WORLDRENDER           

#define PROFILE_FASTTIME() 

#define PROFILE_INITIALIZE() 

#define PROFILE_SHUTDOWN() 

#define PROFILE_SCOPE_FILTERED(context, threshold, flags, format, ...) 

#define PROFILE_SCOPE(context, flags, format, ...) 

#define PROFILE_SCOPE_DEFAULT(context) 
#define PROFILE_SCOPE_IDLE(context)
#define PROFILE_SCOPE_STALL(context) 

#define PROFILE_TICK()

#define PROFILE_EXIT(context) 

#define PROFILE_EXIT_EX(context, match_id, thread_id, filename, line)

#define PROFILE_TRY_LOCK(context, ptr, lock_name, ... ) 

#define PROFILE_TRY_LOCK_EX(context, matcher, threshold, filename, line, ptr, lock_name, ... ) 

#define PROFILE_END_TRY_LOCK(context, ptr, result ) 

#define PROFILE_END_TRY_LOCK_EX(context, match_id, filename, line, ptr, result )

#define PROFILE_BEGIN_TIME_SPAN(context, id, flags, name_format, ... ) 

#define PROFILE_END_TIME_SPAN(context, id, flags, name_format, ... ) 

#define PROFILE_BEGIN_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... ) 

#define PROFILE_END_TIME_SPAN_AT(context, id, flags, timestamp, name_format, ... ) 

#define PROFILE_SIGNAL_LOCK_COUNT(context, ptr, count, description, ... ) 

#define PROFILE_SET_LOCK_STATE(context, ptr, state, description, ... ) 

#define PROFILE_SET_LOCK_STATE_EX(context, filename, line, ptr, state, description, ... ) 

#define PROFILE_SET_LOCK_STATE_MIN_TIME(context, buf, ptr, state, description, ... ) 

#define PROFILE_SET_LOCK_STATE_MIN_TIME_EX(context, buf, filename, line, ptr, state, description, ... ) 

#define PROFILE_THREAD_NAME(context, thread_id, name_format, ... ) 

#define PROFILE_LOCK_NAME(context, ptr, name_format, ... ) 

#define PROFILE_EMIT_ACCUMULATION_ZONE(context, zone_flags, start, count, total, zone_format, ... ) 

#define PROFILE_SET_VARIABLE(context, key, value_format, ... ) 

#define PROFILE_SET_TIMELINE_SECTION_NAME(context, name_format, ... ) 

#define PROFILE_ENTER(context, flags, zone_name, ... ) 

#define PROFILE_ENTER_EX(context, match_id, thread_id, threshold, filename, line, flags, zone_name, ... ) 

#define PROFILE_ALLOC(context, ptr, size, description, ... ) 

#define PROFILE_ALLOC_EX(context, filename, line_number, ptr, size, description, ... ) 

#define PROFILE_FREE(context, ptr) 

#define PROFILE_MESSAGE(context, flags, format_string, ... ) 
#define PROFILE_LOG(context, flags, format_string, ... ) 
#define PROFILE_WARNING(context, flags, format_string, ... ) 
#define PROFILE_ERROR(context, flags, format_string, ... ) 

#define PROFILE_PLOT(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_F32(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_F64(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_I32(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_U32(context, type, flags, value, name_format, ... )

#define PROFILE_PLOT_I64(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_U64(context, type, flags, value, name_format, ... ) 

#define PROFILE_PLOT_AT(context, timestamp, type, flags, value, name_format, ... ) 

#define PROFILE_BLOB(context, data, data_size, plugin_identifier, blob_name, ...) 

#define PROFILE_DISJOINT_BLOB(context, num_pieces, data, data_sizes, plugin_identifier, blob_name, ... )

#define PROFILE_SEND_CALLSTACK(context, callstack) 

#endif //defined(SVR_PROFILING_ENABLED)

#endif //_SVR_API_PROFILE_H_