
#ifndef BNL_HTTP3_EXPORT_H
#define BNL_HTTP3_EXPORT_H

#ifdef BNL_HTTP3_STATIC_DEFINE
#  define BNL_HTTP3_EXPORT
#  define BNL_HTTP3_NO_EXPORT
#else
#  ifndef BNL_HTTP3_EXPORT
#    ifdef bnl_http3_EXPORTS
        /* We are building this library */
#      define BNL_HTTP3_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define BNL_HTTP3_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef BNL_HTTP3_NO_EXPORT
#    define BNL_HTTP3_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef BNL_HTTP3_DEPRECATED
#  define BNL_HTTP3_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef BNL_HTTP3_DEPRECATED_EXPORT
#  define BNL_HTTP3_DEPRECATED_EXPORT BNL_HTTP3_EXPORT BNL_HTTP3_DEPRECATED
#endif

#ifndef BNL_HTTP3_DEPRECATED_NO_EXPORT
#  define BNL_HTTP3_DEPRECATED_NO_EXPORT BNL_HTTP3_NO_EXPORT BNL_HTTP3_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BNL_HTTP3_NO_DEPRECATED
#    define BNL_HTTP3_NO_DEPRECATED
#  endif
#endif

#endif /* BNL_HTTP3_EXPORT_H */
