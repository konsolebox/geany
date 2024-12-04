dnl GEANY_CHECK_REVISION_DATE([action-if-found], [action-if-not-found])
dnl Checks for the Git revision date and assigns it to GEANY_REVISION_DATE
AC_DEFUN([GEANY_CHECK_REVISION_DATE],
[
	if test "x${GEANY_REVISION_DATE}" == "x"; then
		GEANY_REVISION_DATE=""
		AC_MSG_CHECKING([for Git revision date])
		GIT=`which git 2>/dev/null`

		if test -d "$srcdir/.git" -a "x${GIT}" != "x" -a -x "${GIT}"; then
			GEANY_REVISION_DATE=`
				exec 2>/dev/null && cd "$srcdir" || exit
				TZ=UTC "${GIT}" show -s  --format=%as "${GEANY_REVISION:-HEAD}" | tr -d -
			`
		fi

		if test "x${GEANY_REVISION_DATE}" != "x"; then
			AC_MSG_RESULT([${GEANY_REVISION_DATE}])
			GEANY_STATUS_ADD([Compiling Git revision], [${GEANY_REVISION} (${GEANY_REVISION_DATE})])
			$1
		else
			AC_MSG_RESULT([none])
			$2
		fi
	else
		AC_MSG_NOTICE([using ${GEANY_REVISION_DATE} as Git revision date])
	fi

	AC_DEFINE_UNQUOTED([GEANY_REVISION_DATE], "${GEANY_REVISION_DATE}", [git revision date])
])
