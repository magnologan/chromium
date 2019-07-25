
#ifndef BNL_QUIC_EXPORT_H
#define BNL_QUIC_EXPORT_H

#ifdef BNL_QUIC_STATIC_DEFINE
#  define BNL_QUIC_EXPORT
#  define BNL_QUIC_NO_EXPORT
#else
#  ifndef BNL_QUIC_EXPORT
#    ifdef bnl_quic_EXPORTS
        /* We are building this library */
#      define BNL_QUIC_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define BNL_QUIC_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef BNL_QUIC_NO_EXPORT
#    define BNL_QUIC_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef BNL_QUIC_DEPRECATED
#  define BNL_QUIC_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef BNL_QUIC_DEPRECATED_EXPORT
#  define BNL_QUIC_DEPRECATED_EXPORT BNL_QUIC_EXPORT BNL_QUIC_DEPRECATED
#endif

#ifndef BNL_QUIC_DEPRECATED_NO_EXPORT
#  define BNL_QUIC_DEPRECATED_NO_EXPORT BNL_QUIC_NO_EXPORT BNL_QUIC_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef BNL_QUIC_NO_DEPRECATED
#    define BNL_QUIC_NO_DEPRECATED
#  endif
#endif

#endif /* BNL_QUIC_EXPORT_H */
