#ifndef TRACE_LOG
#define TRACE_LOG

#ifdef _DEBUG

#define TRACE_DEBUG( n )        TraceDebug( n )
void FASTCALL TraceDebug( LPCSTR );

#else

#define TRACE_DEBUG(n)

#ifdef _DEBUG
void FASTCALL TraceOutputDebug( LPCSTR );
void FASTCALL TraceLogFile( LPCSTR );
#endif

#endif
#endif
