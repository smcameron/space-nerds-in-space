#!/bin/sh

DRYRUN=0
if [ "$1" = "--dry-run" ]
then
	DRYRUN=1
	shift;
fi

usage()
{
cat << EOF 2>&1
usage: ./killem.sh [--dry-run] [ client | server ]

If no arguments are given any snis_client, snis_server, snis_multiverse and ssgl_server
processes will be killed.

If "client" is specified, only snis_client processes will be killed.

If "server" is specified, only snis_server, snis_multiverse, and ssgl_server processes
will be killed.

If --dry-run is specified, no processes will be killed.  --dry-run must be the
first argument to have any effect.

EOF
}

kill_pattern()
{
	pattern="$1"
	p=$(ps aux | egrep "$pattern" | grep -v 'grep')
	if [ "$p" = "" ]
	then
		echo "There are no processes to kill."
		return
	else
		echo "$p"
	fi
	if [ "$DRYRUN" != "1" ]
	then
		ps aux | egrep "$pattern" | grep -v 'grep' | awk '{ printf("kill %s\n", $2); }' | /bin/sh
		echo "Killed the above processes."
	else
		echo "Dry run mode (would have killed the above processes.)"
	fi
}

# Default is to Killem all
if [ "$1" = "" ]
then
	kill_pattern 'ssgl_server|snis_client|snis_server|snis_multiverse'
elif [ "$1" = "client" ]
then
	kill_pattern 'snis_client'
elif [ "$1" = "server" ]
then
	kill_pattern 'snis_server|snis_multiverse|ssgl_server'
else
	usage
fi
