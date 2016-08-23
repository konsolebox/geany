#!/bin/bash
read LAST_BRANCH < merge_branches_to_testing.last_branch && [[ -n $LAST_BRANCH ]] && git diff > "merge_branches_to_testing/$LAST_BRANCH".fix
