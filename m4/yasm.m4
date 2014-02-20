dnl yasm support for autotool
dnl Copyright 2014 Luca Barbato
dnl
dnl Copying and distribution of this file, with or without modification,
dnl are permitted in any medium without royalty provided the copyright
dnl notice and this notice are preserved.

AC_DEFUN([AX_YASM], [
AC_REQUIRE([AC_CANONICAL_HOST])

AC_CHECK_PROGS(YASM, yasm)

x86_FLAGS=" -DARCH_X86_64=0 -m x86"
x86_64_FLAGS="-DARCH_X86_64=1 -m amd64"

AS_CASE([$host],
    [i?86-*mingw*],    [YASMFLAGS="-f win32 -DPREFIX"],
    [x86_64-*mingw*], [YASMFLAGS="-f win64"],
    [i?86*-apple-darwin*],    [YASMFLAGS="-f macho32 -DPREFIX"],
    [x86_64*-apple-darwin*], [YASMFLAGS="-f macho64 -DPREFIX"],
    [i?86*-linux*],   [YASMFLAGS="-f elf32"],
    [x86_64*-linux*], [YASMFLAGS="-f elf64"]
)

AS_CASE([$host],
    [i?86-*],   [YASMFLAGS="$YASMFLAGS -DARCH_X86_64=0 -m x86"],
    [x86_64-*], [YASMFLAGS="$YASMFLAGS -DARCH_X86_64=1 -m amd64"]
)

AC_SUBST(YASMFLAGS)

])

