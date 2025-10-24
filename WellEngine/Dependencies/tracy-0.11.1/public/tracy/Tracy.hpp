#ifndef __TRACY_HPP__
#define __TRACY_HPP__

#include "../common/TracyColor.hpp"
#include "../common/TracySystem.hpp"

#ifndef TracyFunction
#  define TracyFunction __FUNCTION__
#endif

#ifndef TracyFile
#  define TracyFile __FILE__
#endif

#ifndef TracyLine
#  define TracyLine __LINE__
#endif

#ifndef TRACY_ENABLE

#define TracyNoop

#define ZoneNamed(x,y)
#define ZoneNamedN(x,y,z)
#define ZoneNamedC(x,y,z)
#define ZoneNamedNC(x,y,z,w)

#define ZoneTransient(x,y)
#define ZoneTransientN(x,y,z)

#define ZoneScoped
#define ZoneScopedN(x)
#define ZoneScopedC(x)
#define ZoneScopedNC(x,y)

#define ZoneText(x,y)
#define ZoneTextV(x,y,z)
#define ZoneTextF(x,...)
#define ZoneTextVF(x,y,...)
#define ZoneName(x,y)
#define ZoneNameV(x,y,z)
#define ZoneNameF(x,...)
#define ZoneNameVF(x,y,...)
#define ZoneColor(x)
#define ZoneColorV(x,y)
#define ZoneValue(x)
#define ZoneValueV(x,y)
#define ZoneIsActive false
#define ZoneIsActiveV(x) false

#define ZoneScopedNCD(x,y)
#define ZoneNamedNCD(x,y,z)

#define FrameMark
#define FrameMarkNamed(x)
#define FrameMarkStart(x)
#define FrameMarkEnd(x)

#define FrameImage(x,y,z,w,a)

#define TracyLockable( type, varname ) type varname
#define TracyLockableN( type, varname, desc ) type varname
#define TracySharedLockable( type, varname ) type varname
#define TracySharedLockableN( type, varname, desc ) type varname
#define LockableBase( type ) type
#define SharedLockableBase( type ) type
#define LockMark(x) (void)x
#define LockableName(x,y,z)

#define TracyPlot(x,y)
#define TracyPlotConfig(x,y,z,w,a)

#define TracyMessage(x,y)
#define TracyMessageL(x)
#define TracyMessageC(x,y,z)
#define TracyMessageLC(x,y)
#define TracyAppInfo(x,y)

#define TracyAlloc(x,y)
#define TracyFree(x)
#define TracySecureAlloc(x,y)
#define TracySecureFree(x)

#define TracyAllocN(x,y,z)
#define TracyFreeN(x,y)
#define TracySecureAllocN(x,y,z)
#define TracySecureFreeN(x,y)

#define ZoneNamedS(x,y,z)
#define ZoneNamedNS(x,y,z,w)
#define ZoneNamedCS(x,y,z,w)
#define ZoneNamedNCS(x,y,z,w,a)

#define ZoneTransientS(x,y,z)
#define ZoneTransientNS(x,y,z,w)

#define ZoneScopedS(x)
#define ZoneScopedNS(x,y)
#define ZoneScopedCS(x,y)
#define ZoneScopedNCS(x,y,z)

#define TracyAllocS(x,y,z)
#define TracyFreeS(x,y)
#define TracySecureAllocS(x,y,z)
#define TracySecureFreeS(x,y)

#define TracyAllocNS(x,y,z,w)
#define TracyFreeNS(x,y,z)
#define TracySecureAllocNS(x,y,z,w)
#define TracySecureFreeNS(x,y,z)

#define TracyMessageS(x,y,z)
#define TracyMessageLS(x,y)
#define TracyMessageCS(x,y,z,w)
#define TracyMessageLCS(x,y,z)

#define TracySourceCallbackRegister(x,y)
#define TracyParameterRegister(x,y)
#define TracyParameterSetup(x,y,z,w)
#define TracyIsConnected false
#define TracyIsStarted false
#define TracySetProgramName(x)

#define TracyFiberEnter(x)
#define TracyFiberEnterHint(x,y)
#define TracyFiberLeave

#define TracySetThreadNameWithHint( x, y ) 

#else

#include <string.h>

#include "../client/TracyLock.hpp"
#include "../client/TracyProfiler.hpp"
#include "../client/TracyScoped.hpp"

#define TracyNoop tracy::ProfilerAvailable()

#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK
#  define ZoneNamed( varname, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), TRACY_CALLSTACK, active )
#  define ZoneNamedN( varname, name, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), TRACY_CALLSTACK, active )
#  define ZoneNamedC( varname, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, color }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), TRACY_CALLSTACK, active )
#  define ZoneNamedNC( varname, name, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, (uint32_t)TracyLine, color }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), TRACY_CALLSTACK, active )

#  define ZoneTransient( varname, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), nullptr, 0, TRACY_CALLSTACK, active )
#  define ZoneTransientN( varname, name, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), TRACY_CALLSTACK, active )
#  define ZoneTransientNC( varname, name, color, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), color, TRACY_CALLSTACK, active )
#else
#  define ZoneNamed( varname, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), active )
#  define ZoneNamedN( varname, name, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), active )
#  define ZoneNamedC( varname, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, color }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), active )
#  define ZoneNamedNC( varname, name, color, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, (uint32_t)TracyLine, color }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), active )

#  define ZoneTransient( varname, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), nullptr, 0, active )
#  define ZoneTransientN( varname, name, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), active )
#  define ZoneTransientNC( varname, name, color, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), color, active )
#endif

#define ZoneScoped								ZoneNamed( ___tracy_scoped_zone, true )
#define ZoneScopedN( name )						ZoneNamedN( ___tracy_scoped_zone, name, true )
#define ZoneScopedC( color )					ZoneNamedC( ___tracy_scoped_zone, color, true )
#define ZoneScopedNC( name, color )				ZoneNamedNC( ___tracy_scoped_zone, name, color, true )

#define ZoneText( txt, size )					___tracy_scoped_zone.Text( txt, size )
#define ZoneTextV( varname, txt, size )			varname.Text( txt, size )
#define ZoneTextF( fmt, ... )					___tracy_scoped_zone.TextFmt( fmt, ##__VA_ARGS__ )
#define ZoneTextVF( varname, fmt, ... )			varname.TextFmt( fmt, ##__VA_ARGS__ )
#define ZoneName( txt, size )					___tracy_scoped_zone.Name( txt, size )
#define ZoneNameV( varname, txt, size )			varname.Name( txt, size )
#define ZoneNameF( fmt, ... )					___tracy_scoped_zone.NameFmt( fmt, ##__VA_ARGS__ )
#define ZoneNameVF( varname, fmt, ... )			varname.NameFmt( fmt, ##__VA_ARGS__ )
#define ZoneColor( color )						___tracy_scoped_zone.Color( color )
#define ZoneColorV( varname, color )			varname.Color( color )
#define ZoneValue( value )						___tracy_scoped_zone.Value( value )
#define ZoneValueV( varname, value )			varname.Value( value )
#define ZoneIsActive							___tracy_scoped_zone.IsActive()
#define ZoneIsActiveV( varname )				varname.IsActive()

#define ZoneScopedNCD( name, color )			ZoneNamedC( ___tracy_scoped_zone, color, true ); ZoneName( name.c_str(), strlen( name.c_str() ) )
#define ZoneNamedNCD( varname, name, color )	ZoneNamedC( varname, color, true ); ZoneName( name.c_str(), strlen( name.c_str() ) )

#define FrameMark tracy::Profiler::SendFrameMark( nullptr )
#define FrameMarkNamed( name ) tracy::Profiler::SendFrameMark( name )
#define FrameMarkStart( name ) tracy::Profiler::SendFrameMark( name, tracy::QueueType::FrameMarkMsgStart )
#define FrameMarkEnd( name ) tracy::Profiler::SendFrameMark( name, tracy::QueueType::FrameMarkMsgEnd )

#define FrameImage( image, width, height, offset, flip ) tracy::Profiler::SendFrameImage( image, width, height, offset, flip )

#define TracyLockable( type, varname ) tracy::Lockable<type> varname { [] () -> const tracy::SourceLocationData* { static constexpr tracy::SourceLocationData srcloc { nullptr, #type " " #varname, TracyFile, TracyLine, 0 }; return &srcloc; }() }
#define TracyLockableN( type, varname, desc ) tracy::Lockable<type> varname { [] () -> const tracy::SourceLocationData* { static constexpr tracy::SourceLocationData srcloc { nullptr, desc, TracyFile, TracyLine, 0 }; return &srcloc; }() }
#define TracySharedLockable( type, varname ) tracy::SharedLockable<type> varname { [] () -> const tracy::SourceLocationData* { static constexpr tracy::SourceLocationData srcloc { nullptr, #type " " #varname, TracyFile, TracyLine, 0 }; return &srcloc; }() }
#define TracySharedLockableN( type, varname, desc ) tracy::SharedLockable<type> varname { [] () -> const tracy::SourceLocationData* { static constexpr tracy::SourceLocationData srcloc { nullptr, desc, TracyFile, TracyLine, 0 }; return &srcloc; }() }
#define LockableBase( type ) tracy::Lockable<type>
#define SharedLockableBase( type ) tracy::SharedLockable<type>
#define LockMark( varname ) static constexpr tracy::SourceLocationData __tracy_lock_location_##varname { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; varname.Mark( &__tracy_lock_location_##varname )
#define LockableName( varname, txt, size ) varname.CustomName( txt, size )

#define TracyPlot( name, val ) tracy::Profiler::PlotData( name, val )
#define TracyPlotConfig( name, type, step, fill, color ) tracy::Profiler::ConfigurePlot( name, type, step, fill, color )

#define TracyAppInfo( txt, size ) tracy::Profiler::MessageAppInfo( txt, size )

#define TracySetThreadNameWithHint( name, groupHint ) tracy::SetThreadNameWithHint( name, groupHint )

#if defined TRACY_HAS_CALLSTACK && defined TRACY_CALLSTACK
#  define TracyMessage( txt, size ) tracy::Profiler::Message( txt, size, TRACY_CALLSTACK )
#  define TracyMessageL( txt ) tracy::Profiler::Message( txt, TRACY_CALLSTACK )
#  define TracyMessageC( txt, size, color ) tracy::Profiler::MessageColor( txt, size, color, TRACY_CALLSTACK )
#  define TracyMessageLC( txt, color ) tracy::Profiler::MessageColor( txt, color, TRACY_CALLSTACK )

#  define TracyAlloc( ptr, size ) tracy::Profiler::MemAllocCallstack( ptr, size, TRACY_CALLSTACK, false )
#  define TracyFree( ptr ) tracy::Profiler::MemFreeCallstack( ptr, TRACY_CALLSTACK, false )
#  define TracySecureAlloc( ptr, size ) tracy::Profiler::MemAllocCallstack( ptr, size, TRACY_CALLSTACK, true )
#  define TracySecureFree( ptr ) tracy::Profiler::MemFreeCallstack( ptr, TRACY_CALLSTACK, true )

#  define TracyAllocN( ptr, size, name ) tracy::Profiler::MemAllocCallstackNamed( ptr, size, TRACY_CALLSTACK, false, name )
#  define TracyFreeN( ptr, name ) tracy::Profiler::MemFreeCallstackNamed( ptr, TRACY_CALLSTACK, false, name )
#  define TracySecureAllocN( ptr, size, name ) tracy::Profiler::MemAllocCallstackNamed( ptr, size, TRACY_CALLSTACK, true, name )
#  define TracySecureFreeN( ptr, name ) tracy::Profiler::MemFreeCallstackNamed( ptr, TRACY_CALLSTACK, true, name )
#else
#  define TracyMessage( txt, size ) tracy::Profiler::Message( txt, size, 0 )
#  define TracyMessageL( txt ) tracy::Profiler::Message( txt, 0 )
#  define TracyMessageC( txt, size, color ) tracy::Profiler::MessageColor( txt, size, color, 0 )
#  define TracyMessageLC( txt, color ) tracy::Profiler::MessageColor( txt, color, 0 )

#  define TracyAlloc( ptr, size ) tracy::Profiler::MemAlloc( ptr, size, false )
#  define TracyFree( ptr ) tracy::Profiler::MemFree( ptr, false )
#  define TracySecureAlloc( ptr, size ) tracy::Profiler::MemAlloc( ptr, size, true )
#  define TracySecureFree( ptr ) tracy::Profiler::MemFree( ptr, true )

#  define TracyAllocN( ptr, size, name ) tracy::Profiler::MemAllocNamed( ptr, size, false, name )
#  define TracyFreeN( ptr, name ) tracy::Profiler::MemFreeNamed( ptr, false, name )
#  define TracySecureAllocN( ptr, size, name ) tracy::Profiler::MemAllocNamed( ptr, size, true, name )
#  define TracySecureFreeN( ptr, name ) tracy::Profiler::MemFreeNamed( ptr, true, name )
#endif

#ifdef TRACY_HAS_CALLSTACK
#  define ZoneNamedS( varname, depth, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), depth, active )
#  define ZoneNamedNS( varname, name, depth, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, (uint32_t)TracyLine, 0 }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), depth, active )
#  define ZoneNamedCS( varname, color, depth, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { nullptr, TracyFunction,  TracyFile, (uint32_t)TracyLine, color }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), depth, active )
#  define ZoneNamedNCS( varname, name, color, depth, active ) static constexpr tracy::SourceLocationData TracyConcat(__tracy_source_location,TracyLine) { name, TracyFunction,  TracyFile, (uint32_t)TracyLine, color }; tracy::ScopedZone varname( &TracyConcat(__tracy_source_location,TracyLine), depth, active )

#  define ZoneTransientS( varname, depth, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), nullptr, 0, depth, active )
#  define ZoneTransientNS( varname, name, depth, active ) tracy::ScopedZone varname( TracyLine, TracyFile, strlen( TracyFile ), TracyFunction, strlen( TracyFunction ), name, strlen( name ), depth, active )

#  define ZoneScopedS( depth ) ZoneNamedS( ___tracy_scoped_zone, depth, true )
#  define ZoneScopedNS( name, depth ) ZoneNamedNS( ___tracy_scoped_zone, name, depth, true )
#  define ZoneScopedCS( color, depth ) ZoneNamedCS( ___tracy_scoped_zone, color, depth, true )
#  define ZoneScopedNCS( name, color, depth ) ZoneNamedNCS( ___tracy_scoped_zone, name, color, depth, true )

#  define TracyAllocS( ptr, size, depth ) tracy::Profiler::MemAllocCallstack( ptr, size, depth, false )
#  define TracyFreeS( ptr, depth ) tracy::Profiler::MemFreeCallstack( ptr, depth, false )
#  define TracySecureAllocS( ptr, size, depth ) tracy::Profiler::MemAllocCallstack( ptr, size, depth, true )
#  define TracySecureFreeS( ptr, depth ) tracy::Profiler::MemFreeCallstack( ptr, depth, true )

#  define TracyAllocNS( ptr, size, depth, name ) tracy::Profiler::MemAllocCallstackNamed( ptr, size, depth, false, name )
#  define TracyFreeNS( ptr, depth, name ) tracy::Profiler::MemFreeCallstackNamed( ptr, depth, false, name )
#  define TracySecureAllocNS( ptr, size, depth, name ) tracy::Profiler::MemAllocCallstackNamed( ptr, size, depth, true, name )
#  define TracySecureFreeNS( ptr, depth, name ) tracy::Profiler::MemFreeCallstackNamed( ptr, depth, true, name )

#  define TracyMessageS( txt, size, depth ) tracy::Profiler::Message( txt, size, depth )
#  define TracyMessageLS( txt, depth ) tracy::Profiler::Message( txt, depth )
#  define TracyMessageCS( txt, size, color, depth ) tracy::Profiler::MessageColor( txt, size, color, depth )
#  define TracyMessageLCS( txt, color, depth ) tracy::Profiler::MessageColor( txt, color, depth )
#else
#  define ZoneNamedS( varname, depth, active ) ZoneNamed( varname, active )
#  define ZoneNamedNS( varname, name, depth, active ) ZoneNamedN( varname, name, active )
#  define ZoneNamedCS( varname, color, depth, active ) ZoneNamedC( varname, color, active )
#  define ZoneNamedNCS( varname, name, color, depth, active ) ZoneNamedNC( varname, name, color, active )

#  define ZoneTransientS( varname, depth, active ) ZoneTransient( varname, active )
#  define ZoneTransientNS( varname, name, depth, active ) ZoneTransientN( varname, name, active )

#  define ZoneScopedS( depth ) ZoneScoped
#  define ZoneScopedNS( name, depth ) ZoneScopedN( name )
#  define ZoneScopedCS( color, depth ) ZoneScopedC( color )
#  define ZoneScopedNCS( name, color, depth ) ZoneScopedNC( name, color )

#  define TracyAllocS( ptr, size, depth ) TracyAlloc( ptr, size )
#  define TracyFreeS( ptr, depth ) TracyFree( ptr )
#  define TracySecureAllocS( ptr, size, depth ) TracySecureAlloc( ptr, size )
#  define TracySecureFreeS( ptr, depth ) TracySecureFree( ptr )

#  define TracyAllocNS( ptr, size, depth, name ) TracyAllocN( ptr, size, name )
#  define TracyFreeNS( ptr, depth, name ) TracyFreeN( ptr, name )
#  define TracySecureAllocNS( ptr, size, depth, name ) TracySecureAllocN( ptr, size, name )
#  define TracySecureFreeNS( ptr, depth, name ) TracySecureFreeN( ptr, name )

#  define TracyMessageS( txt, size, depth ) TracyMessage( txt, size )
#  define TracyMessageLS( txt, depth ) TracyMessageL( txt )
#  define TracyMessageCS( txt, size, color, depth ) TracyMessageC( txt, size, color )
#  define TracyMessageLCS( txt, color, depth ) TracyMessageLC( txt, color )
#endif

#define TracySourceCallbackRegister( cb, data ) tracy::Profiler::SourceCallbackRegister( cb, data )
#define TracyParameterRegister( cb, data ) tracy::Profiler::ParameterRegister( cb, data )
#define TracyParameterSetup( idx, name, isBool, val ) tracy::Profiler::ParameterSetup( idx, name, isBool, val )
#define TracyIsConnected tracy::GetProfiler().IsConnected()
#define TracySetProgramName( name ) tracy::GetProfiler().SetProgramName( name );

#ifdef TRACY_FIBERS
#  define TracyFiberEnter( fiber ) tracy::Profiler::EnterFiber( fiber, 0 )
#  define TracyFiberEnterHint( fiber, groupHint ) tracy::Profiler::EnterFiber( fiber, groupHint )
#  define TracyFiberLeave tracy::Profiler::LeaveFiber()
#endif

#endif

// Offer toggleable macros for less important zones with the X suffix.
#ifdef TRACY_DETAILED
#define ZoneScopedX						ZoneScoped
#define ZoneScopedXN( name )			ZoneScopedN( name )
#define ZoneScopedXC( color )			ZoneScopedC( color )
#define ZoneScopedXNC( name, color )	ZoneScopedNC( name, color )

#define ZoneTextX( txt, size )				ZoneText( txt, size )
#define ZoneTextXV( varname, txt, size )	ZoneTextV( varname, txt, size )
#define ZoneTextXF( fmt, ... )				ZoneTextF( fmt, ##__VA_ARGS__ )
#define ZoneTextXVF( varname, fmt, ... )	ZoneTextVF( varname, fmt, ##__VA_ARGS__ )
#define ZoneNameX( txt, size )				ZoneName( txt, size )
#define ZoneNameXV( varname, txt, size )	ZoneNameV( varname, txt, size )
#define ZoneNameXF( fmt, ... )				ZoneNameF( fmt, ##__VA_ARGS__ )
#define ZoneNameXVF( varname, fmt, ... )	ZoneNameVF( varname, fmt, ##__VA_ARGS__ ) 
#define ZoneColorX( color ) 				ZoneColor( color ) 
#define ZoneColorXV( varname, color )		ZoneColorV( varname, color )
#define ZoneValueX( value ) 				ZoneValue( value ) 
#define ZoneValueXV( varname, value ) 		ZoneValueV( varname, value ) 
#define ZoneIsActiveX						ZoneIsActive
#define ZoneIsActiveXV( varname )			ZoneIsActiveV( varname )

#define ZoneScopedXNCD( name, color )			ZoneScopedNCD( name, color )
#define ZoneNamedXNCD( varname, name, color )	ZoneNamedNCD( varname, name, color )

#define ZoneNamedX( varname, active ) 					ZoneNamed( varname, active ) 
#define ZoneNamedXN( varname, name, active ) 			ZoneNamedN( varname, name, active ) 
#define ZoneNamedXC( varname, color, active ) 			ZoneNamedC( varname, color, active ) 
#define ZoneNamedXNC( varname, name, color, active )	ZoneNamedNC( varname, name, color, active ) 

#define ZoneTransientX( varname, active ) 					ZoneTransient( varname, active ) 
#define ZoneTransientXN( varname, name, active ) 			ZoneTransientN( varname, name, active ) 
#define ZoneTransientXNC( varname, name, color, active )	ZoneTransientNC( varname, name, color, active ) 
#else
#define ZoneScopedX	
#define ZoneScopedXN( name )
#define ZoneScopedXC( color )	
#define ZoneScopedXNC( name, color )	

#define ZoneTextX( txt, size )		
#define ZoneTextXV( varname, txt, size )	
#define ZoneTextXF( fmt, ... )				
#define ZoneTextXVF( varname, fmt, ... )	
#define ZoneNameX( txt, size )				
#define ZoneNameXV( varname, txt, size )	
#define ZoneNameXF( fmt, ... )				
#define ZoneNameXVF( varname, fmt, ... )	
#define ZoneColorX( color ) 				
#define ZoneColorXV( varname, color )		
#define ZoneValueX( value ) 				
#define ZoneValueXV( varname, value ) 		
#define ZoneIsActiveX						
#define ZoneIsActiveXV( varname )			

#define ZoneScopedXNCD( name, color )
#define ZoneNamedXNCD( varname, name, color )

#define ZoneNamedX( varname, active ) 					
#define ZoneNamedXN( varname, name, active ) 			
#define ZoneNamedXC( varname, color, active ) 			
#define ZoneNamedXNC( varname, name, color, active )	

#define ZoneTransientX( varname, active ) 				
#define ZoneTransientXN( varname, name, active ) 		
#define ZoneTransientXNC( varname, name, color, active )
#endif


// Tracy color utilities

// Tracy uses 0xBBGGRR format for colors
#define TRACY_COLOR(r, g, b) ((const uint32_t)(((r) << 0) | ((g) << 8) | ((b) << 16)))

constexpr uint32_t TracyColorF(const float r, const float g, const float b)
{
	return TRACY_COLOR((uint32_t)(r * 255.0f), (uint32_t)(g * 255.0f), (uint32_t)(b * 255.0f));
}

enum class TracyColor : uint32_t
{
	White			= TRACY_COLOR( 255, 255, 255 ),
	Black			= TRACY_COLOR(   0,   0,   0 ),
	Gray			= TRACY_COLOR( 128, 128, 128 ),
	LightGray		= TRACY_COLOR( 211, 211, 211 ),
	DarkGray		= TRACY_COLOR(  64,  64,  64 ),
	Red				= TRACY_COLOR( 255,   0,   0 ),
	Green			= TRACY_COLOR(   0, 255,   0 ),
	Blue			= TRACY_COLOR(   0,   0, 255 ),
	Orange			= TRACY_COLOR( 255, 165,   0 ),
	Yellow			= TRACY_COLOR( 255, 255,   0 ),
	Purple			= TRACY_COLOR( 128,   0, 128 ),
	Silver			= TRACY_COLOR( 192, 192, 192 ),
	Brown			= TRACY_COLOR( 165,  42,  42 ),
	Pink			= TRACY_COLOR( 255, 192, 203 ),
	Olive			= TRACY_COLOR( 128, 128,   0 ),
	Maroon			= TRACY_COLOR( 128,   0,   0 ),
	Violet			= TRACY_COLOR( 238, 130, 238 ),
	Charcoal		= TRACY_COLOR(  54,  69,  79 ),
	Magenta			= TRACY_COLOR( 255,   0, 255 ),
	Bronze			= TRACY_COLOR( 205, 127,  40 ),
	Cream			= TRACY_COLOR( 255, 253, 208 ),
	Gold			= TRACY_COLOR( 255, 215,   0 ),
	Tan				= TRACY_COLOR( 210, 180, 140 ),
	Teal			= TRACY_COLOR(   0, 128, 128 ),
	Mustard			= TRACY_COLOR( 255, 219,  88 ),
	NavyBlue		= TRACY_COLOR(   0,   0, 128 ),
	Coral			= TRACY_COLOR( 255, 127,  80 ),
	Burgundy		= TRACY_COLOR( 128,   0,  32 ),
	Lavender		= TRACY_COLOR( 230, 230, 250 ),
	Mauve			= TRACY_COLOR( 224, 176, 255 ),
	Cyan			= TRACY_COLOR( 224, 247, 250 ),
	Peach			= TRACY_COLOR( 255, 229, 180 ),
	Rust			= TRACY_COLOR( 183,  65,  14 ),
	Indigo			= TRACY_COLOR(  75,   0, 130 ),
	Ruby			= TRACY_COLOR( 224,  17,  95 ),
	LimeGreen		= TRACY_COLOR(  50, 205,  50 ),
	Salmon			= TRACY_COLOR( 250, 128, 114 ),
	Azure			= TRACY_COLOR(   0, 127, 255 ),
	Beige			= TRACY_COLOR( 245, 245, 220 ),
	CopperRose		= TRACY_COLOR( 153, 102, 102 ),
	Turquoise		= TRACY_COLOR(  64, 224, 208 ),
	Aqua			= TRACY_COLOR(   0, 255, 255 ),
	Mint			= TRACY_COLOR(  62, 180, 137 ),
	SkyBlue			= TRACY_COLOR( 135, 206, 235 ),
	Crimson			= TRACY_COLOR( 220,  20,  60 ),
	Saffron			= TRACY_COLOR( 244, 196,  48 ),
	LemonYellow		= TRACY_COLOR( 255, 244,  79 ),
	Grapevine		= TRACY_COLOR(  67,  37,  79 ),
	Fuschia			= TRACY_COLOR( 255,   0, 255 ),
	Amber			= TRACY_COLOR( 255, 191,   0 ),
	SeaGreen		= TRACY_COLOR(  46, 139,  87 ),
	DarkGreen		= TRACY_COLOR(   0, 100,   0 ),
	Pearl			= TRACY_COLOR( 234, 224, 200 ),
	Ivory			= TRACY_COLOR( 255, 255, 240 ),
	Tangerine		= TRACY_COLOR( 242, 133,   0 ),
	Garnet			= TRACY_COLOR( 115,  44,  53 ),
	CherryRed		= TRACY_COLOR( 222,  49,  99 ),
	Emerald			= TRACY_COLOR(  80, 200, 120 ),
	Brunette		= TRACY_COLOR( 102,  66,  56 ),
	Sapphire		= TRACY_COLOR(  15,  82, 186 ),
	Lilac			= TRACY_COLOR( 200, 162, 200 ),
	Rosewood		= TRACY_COLOR( 101,   0,  11 ),
	ArcticBlue		= TRACY_COLOR(   0,   0, 255 ),
	Ash				= TRACY_COLOR( 128, 128, 128 ),
	Mocha			= TRACY_COLOR( 192, 163, 146 ),
	CoffeeBrown		= TRACY_COLOR( 111,  78,  55 ),
	JetBlack		= TRACY_COLOR(  10,  10,  10 ),
	PistaGreen		= TRACY_COLOR(   0, 255,   0 ),
	Umber			= TRACY_COLOR(  99,  81,  71 )
};

#endif
