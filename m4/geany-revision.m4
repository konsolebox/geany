dnl GEANY_CHECK_REVISION([action-if-found], [action-if-not-found])
dnl Checks for the Git revision and assigns it to GEANY_REVISION
AC_DEFUN([GEANY_CHECK_REVISION],
[
	if test "x${GEANY_REVISION}" == "x"; then
		GEANY_REVISION=""
		AC_MSG_CHECKING([for Git revision])
		GIT=`which git 2>/dev/null`

		if test -d "$srcdir/.git" -a "x${GIT}" != "x" -a -x "${GIT}"; then
			GEANY_REVISION=`
				exec 2>/dev/null && cd "$srcdir" || exit
				"${GIT}" rev-parse --short --revs-only HEAD
			`
		fi

		if test "x${GEANY_REVISION}" != "x"; then
			AC_MSG_RESULT([$GEANY_REVISION])
			GEANY_STATUS_ADD([Compiling Git revision], [$GEANY_REVISION])
			$1
		else
			AC_MSG_RESULT([none])
			$2
		fi
	else
		AC_MSG_NOTICE([using ${GEANY_REVISION} as Git revision])
	fi

	AC_DEFINE_UNQUOTED([GEANY_REVISION], "${GEANY_REVISION}", [git revision hash])
])
