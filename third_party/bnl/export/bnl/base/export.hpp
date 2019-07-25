
#ifndef BNL_BASE_EXPORT_H
#define BNL_BASE_EXPORT_H

#ifdef BNL_BASE_STATIC_DEFINE
#  define BNL_BASE_EXPORT
#  define BNL_BASE_NO_EXPORT
#else
#  ifndef BNL_BASE_EXPORT
#    ifdef bnl_base_EXPORTS
        /* We are building this library */
#      define BNL_BASE_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define BNL_BASE_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef BNL_BASE_NO_EXPORT
#    define BNL_BASE_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef BNL_BASE_DEPRECATED
#  define BNL_BASE_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef BNL_BASE_DEPRECATED_EXPORT
#  define BNL_BASE_DEPRECATED_EXPORT BNL_BASE_EXPORT BNL_BASE_DEPRECATED
#endif

#ifndef BNL_BASE_DEPRECATED_NO_EXPORT
#  define BNL_BASE_DEPRECATED_NO_EXPORT BNL_BASE_NO_EXPORT BNL_BASE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BNL_BASE_NO_DEPRECATED
#    define BNL_BASE_NO_DEPRECATED
#  endif
#endif

#endif /* BNL_BASE_EXPORT_H */
