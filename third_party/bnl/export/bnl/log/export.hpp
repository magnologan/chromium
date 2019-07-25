
#ifndef BNL_LOG_EXPORT_H
#define BNL_LOG_EXPORT_H

#ifdef BNL_LOG_STATIC_DEFINE
#  define BNL_LOG_EXPORT
#  define BNL_LOG_NO_EXPORT
#else
#  ifndef BNL_LOG_EXPORT
#    ifdef bnl_log_EXPORTS
        /* We are building this library */
#      define BNL_LOG_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define BNL_LOG_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef BNL_LOG_NO_EXPORT
#    define BNL_LOG_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef BNL_LOG_DEPRECATED
#  define BNL_LOG_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef BNL_LOG_DEPRECATED_EXPORT
#  define BNL_LOG_DEPRECATED_EXPORT BNL_LOG_EXPORT BNL_LOG_DEPRECATED
#endif

#ifndef BNL_LOG_DEPRECATED_NO_EXPORT
#  define BNL_LOG_DEPRECATED_NO_EXPORT BNL_LOG_NO_EXPORT BNL_LOG_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BNL_LOG_NO_DEPRECATED
#    define BNL_LOG_NO_DEPRECATED
#  endif
#endif

#endif /* BNL_LOG_EXPORT_H */
