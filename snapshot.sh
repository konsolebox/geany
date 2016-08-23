#!/bin/sh
TZ=gmt exec git commit -m "$(date '+%Y/%m/%d')" "$@"
