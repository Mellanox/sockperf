dnl Kind of like AC_ARG_ENABLE, but enforces that the optional argument is
dnl either "yes" or "no". Returns the result in a variable called have_NAME.
dnl
dnl SP_ARG_ENABLE_BOOL(NAME, HELP-STRING)

AC_DEFUN([SP_ARG_ENABLE_BOOL], [
	have_$1=no
	AC_ARG_ENABLE([$1], [$2], [
		AS_IF([test "x$enableval" = "xno"],  [have_$1=no],
	              [test "x$enableval" = "xyes"], [have_$1=yes],
		      [AC_MSG_ERROR([bad value $enableval for --enable-$1])])])
])
