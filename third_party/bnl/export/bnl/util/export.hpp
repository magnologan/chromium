
#ifndef BNL_UTIL_EXPORT_H
#define BNL_UTIL_EXPORT_H

#ifdef BNL_UTIL_STATIC_DEFINE
#  define BNL_UTIL_EXPORT
#  define BNL_UTIL_NO_EXPORT
#else
#  ifndef BNL_UTIL_EXPORT
#    ifdef bnl_util_EXPORTS
        /* We are building this library */
#      define BNL_UTIL_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define BNL_UTIL_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef BNL_UTIL_NO_EXPORT
#    define BNL_UTIL_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef BNL_UTIL_DEPRECATED
#  define BNL_UTIL_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef BNL_UTIL_DEPRECATED_EXPORT
#  define BNL_UTIL_DEPRECATED_EXPORT BNL_UTIL_EXPORT BNL_UTIL_DEPRECATED
#endif

#ifndef BNL_UTIL_DEPRECATED_NO_EXPORT
#  define BNL_UTIL_DEPRECATED_NO_EXPORT BNL_UTIL_NO_EXPORT BNL_UTIL_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BNL_UTIL_NO_DEPRECATED
#    define BNL_UTIL_NO_DEPRECATED
#  endif
#endif

#endif /* BNL_UTIL_EXPORT_H */
