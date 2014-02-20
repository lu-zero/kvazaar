AC_PREREQ([2.53])

AC_INIT([kvazaar], [m4_esyscmd([./package_version.sh])],
   [https://github.com/ultravideo/kvazaar/issues], [kvazaar],
   [http://ultravideo.cs.tut.fi/#encoder])
AC_CONFIG_SRCDIR([src/global.h])
AM_INIT_AUTOMAKE([1.6 foreign subdir-objects tar-ustar dist-zip])
m4_ifdef([AM_SILENT_RULES], [
    AM_SILENT_RULES([yes])
])

AC_CANONICAL_HOST

AC_PROG_SED
AC_PROG_AWK

KVA_ABI_VERSION=m4_esyscmd([./abi_version.sh])
KVA_PACKAGE_VERSION=m4_esyscmd([./package_version.sh])
KVA_LIBTOOL_VERSION=m4_esyscmd([./abi_version.sh -libtool])

AC_SUBST(KVA_ABI_VERSION)
AC_SUBST(KVA_PACKAGE_VERSION)
AC_SUBST(KVA_LIBTOOL_VERSION)

AC_MSG_NOTICE([kvazaar package version: $KVA_PACKAGE_VERSION])
AC_MSG_NOTICE([kvazaar ABI version: $KVA_ABI_VERSION])

AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
    [Enable debugging information [default=no]])])

if test "x$enable_debug" = "xyes"; then
    ORIG_CFLAGS="${CFLAGS:-none}"
    if test "x$ORIG_CFLAGS" != "xnone"; then
        CFLAGS="-g -O0 ${CFLAGS} ${ORIG_CFLAGS}"
    fi
    AC_DEFINE([KVA_DEBUG])
fi

AC_ARG_ENABLE([doc],
    AS_HELP_STRING([--enable-doc], [Build documentation [default=no]])
)

AS_IF([test x"$enable_doc" = "xyes"], [
    AS_IF([test ! -d "doc"], [
        AC_PROG_MKDIR_P
        AS_MKDIR_P("doc")
    ])
])
AM_CONDITIONAL([DOC], [test x"$enable_doc" != "xyes" ])

AC_ARG_ENABLE([tools],
    AS_HELP_STRING([--enable-tools], [Build kvazaar executable [default=yes]])
)

AM_CONDITIONAL([TOOLS], [test x"$enable_tools" != "xno" ])

AX_PTHREAD([
  LIBS="$PTHREAD_LIBS $LIBS"
  CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
  CC="$PTHREAD_CC"
])

AC_SEARCH_LIBS([pow], [m], [], [
  AC_MSG_ERROR([unable to find the pow() function])
])

AX_YASM

LT_INIT

DOLT

AC_CONFIG_MACRO_DIR([m4])

AC_OUTPUT([
Makefile
libkvazaar.pc
])

