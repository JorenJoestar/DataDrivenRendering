#pragma once

// Macros //////////////////////////////////////////////////////////////////

#define ArraySize(array)        ( sizeof(array)/sizeof((array)[0]) )


#if defined (_MSC_VER)
#define HY_PERFHUD
#define HY_INLINE                               inline
#define HY_FINLINE                              __forceinline
#define HY_DEBUG_BREAK                          __debugbreak();
#define HY_DISABLE_WARNING(warning_number)      __pragma( warning( disable : warning_number ) )
#else

#endif // MSVC

#define HY_STRINGIZE( L )                       #L 
#define HY_MAKESTRING( L )                      HY_STRINGIZE( L )
#define HY_CONCAT_OPERATOR(x, y)                x##y
#define HY_CONCAT(x, y)                         HY_CONCAT_OPERATOR(x, y)
#define HY_LINE_STRING                          HY_MAKESTRING( __LINE__ ) 
#define HY_FILELINE(MESSAGE)                    __FILE__ "(" HY_LINE_STRING ") : " MESSAGE

// Unique names
#define HY_UNIQUE_SUFFIX(PARAM)                 HY_CONCAT(PARAM, __LINE__ )

