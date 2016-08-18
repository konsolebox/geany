#!/bin/bash

shopt -s extglob

BRANCHES=(enhance_sh_tag_detection add_no_new_instance_option allow_disable_vte_bold sort_tabs_v2 open_files_recursively renaming close_recursively)
UPSTREAM_REPO=origin
export TZ=gmt

function call_s {
	"$@"
	local R=$?
	echo
	return "$R"
}

function call {
	local ARGS=() __

	for __; do
		if [[ $__ == *[[:blank:]]* ]]; then
			ARGS+=("\"${__//\"/\\\"}\"")
		else
			ARGS+=("$__")
		fi
	done

	printf "> %s\n" "${ARGS[*]}"
	call_s "$@"
}

function main {
	local CURRENT_BRANCH=$(git branch | awk '/^\*/ { print $2 }') __

	call git checkout "${UPSTREAM_REPO}/master" && call git branch -D testing && call git checkout -b testing && {
		for __ in "${BRANCHES[@]}"; do
			call git merge "refs/heads/$__" -m "Merge branch $__" || {
				[[ -e merge_branches_to_testing/$__.fix ]] && {
					echo "> patch -p1 < \"merge_branches_to_testing/$__.fix\""

					call_s patch -p1 < "merge_branches_to_testing/$__.fix" && {
						call git commit -m "Merge branch $__" -a
					}
				}
			}
		done

		if [[ ${CURRENT_BRANCH} != testing ]]; then
			git checkout "${CURRENT_BRANCH}" || echo "Warning: Failed to change back to previous branch ${CURRENT_BRANCH}."
		fi
	}
}

main
