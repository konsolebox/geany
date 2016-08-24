#!/bin/bash

shopt -s extglob

BRANCHES=(
	enhance_sh_tag_detection
	add_no_new_instance_option
	allow_disable_vte_bold
	sort_tabs_v2
	open_files_recursively
	docs_close_recursively
	rework_save_as
	docs_save_as
	docs_rename
	file_rename
	clone_filename
	docs_clone
	docs_delete
	file_delete
	docs_reload_multi
	file_reload_all
)

UPSTREAM_REPO=origin
export TZ=gmt

function log {
	printf '%s\n' "$1" >&2
}

function call_s {
	"$@"
	local R=$?
	log
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

	log "> ${ARGS[*]}"
	call_s "$@"
}

function check_if_all_branches_unique {
	local -A HASH
	local B

	for B in "${BRANCHES[@]}"; do
		if [[ -n ${HASH["$B"]} ]]; then
			log "Repeated branch found: $B"
		fi

		HASH["$B"]=.
	done
}

function main {
	check_if_all_branches_unique

	[[ $1 == -r ]] && call git reset --hard

	local CURRENT_BRANCH=$(git branch | awk '/^\*/ { print $2 }') R __

	call git checkout "${UPSTREAM_REPO}/master" && call git branch -D testing && call git checkout -b testing && {
		for __ in "${BRANCHES[@]}"; do
			echo "$__" > merge_branches_to_testing.last_branch

			call git merge "refs/heads/$__" -m "Merge branch $__" && {
				[[ -e merge_branches_to_testing/$__.fix ]] && call rm -f -- "merge_branches_to_testing/$__.fix"
				[[ -e merge_branches_to_testing/$__.fix.failed ]] && call rm -f -- "merge_branches_to_testing/$__.fix.failed"
				continue
			}

			if [[ -e merge_branches_to_testing/$__.fix ]]; then
				log "> patch -p1 < \"merge_branches_to_testing/$__.fix\""

				call_s patch -p1 --dry-run < "merge_branches_to_testing/$__.fix" && \
				call_s patch -p1 --quiet < "merge_branches_to_testing/$__.fix" && {
					call git commit -m "Merge branch $__" -a && {
						log "> git diff HEAD~1 HEAD 2>&1 | grep -Fe '>>>>>>>' -e '<<<<<<<'"

						call_s git diff HEAD~1 HEAD | grep -Fe '>>>>>>>' -e '<<<<<<<' || {
							[[ -e merge_branches_to_testing/$__.fix.failed ]] && call rm -f -- "merge_branches_to_testing/$__.fix.failed"
							continue
						}

						call git reset HEAD~1
					}

					log "Something was missed in the fix."
					log
					log "> patch -p1 -R < \"merge_branches_to_testing/$__.fix\""
					call_s patch -p1 -R < "merge_branches_to_testing/$__.fix"
				}

				# call mv -- "merge_branches_to_testing/$__.fix"{,.failed}
			fi

			call git commit -m X -a
			[[ -e merge_branches_to_testing/$__.fix ]] && call_s patch -p1 < "merge_branches_to_testing/$__.fix"
			return 1
		done

		echo "It's done.  Congratulations!"

		if [[ ${CURRENT_BRANCH} != testing ]]; then
			git checkout "${CURRENT_BRANCH}" || log "Warning: Failed to change back to previous branch ${CURRENT_BRANCH}."
		fi
	}
}

main "$@"
