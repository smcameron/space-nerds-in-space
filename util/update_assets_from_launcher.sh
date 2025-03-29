#!/bin/sh

# This script is meant to be invoked only by snis_client

ask=""
localonly=""
showprogress=""

while  [ "$#" -gt "0" ]
do
	opt="$1"
	case "$opt" in
	dontask)
		ask="dontask"
		;;
	localonly)
		localonly="localonly"
		;;
	showprogress)
		showprogress="--show-progress"
		;;
	*)
		echo "Unknown option $opt" 1>&2
		exit 1;
		;;
	esac
	shift;
done

if [ "$(id -u)" = "0" ]
then
	printf "You are root. Do not run this script as root.\n" 1>&2
	exit 1
fi

if [ "$SNIS_DB_DIR" = "" ]
then
	if [ "$XDG_DATA_HOME" != "" -a -d "$XDG_DATA_HOME" ]
	then
		SNIS_DB_DIR="$XDG_DATA_HOME/space-nerds-in-space"
	else
		SNIS_DB_DIR="$HOME/.local/share/space-nerds-in-space"
	fi
	if [ ! -d "$SNIS_DB_DIR" ]
	then
		mkdir -p "$SNIS_DB_DIR"
	fi
fi
export SNIS_DB_DIR

# Hack this in for now.  Note: the Makefile edits the next two lines with sed
# so be careful if you change it that the Makefile still edits it correctly.
# Currently it matches the pattern: '^PREFIX=[.]$' and '^DESTDIR=$'
PREFIX=.
DESTDIR=

BINDIR=${DESTDIR}${PREFIX}/bin

if [ "$SNIS_ASSET_DIR" = "" ]
then
	if [ "$XDG_DATA_HOME" = "" ]
	then
		export SNIS_ASSET_DIR=$HOME/.local/share/space-nerds-in-space/share/snis
	else
		export SNIS_ASSET_DIR=$XDG_DATA_HOME/space-nerds-in-space/share/snis
	fi
	if [ ! -d "$SNIS_ASSET_DIR" ]
	then
		mkdir -p "$SNIS_ASSET_DIR"
	fi
fi

echo $SNIS_ASSET_DIR | grep '[/]share[/]snis$' > /dev/null 2>&1
if [ "$?" != "0" ]
then
	echo "Error: SNIS_ASSET_DIR must end with /share/snis" 1>&2
	echo "Current value of SNIS_ASSET_DIR is $SNIS_ASSET_DIR" 1>&2
	echo "Exiting." 1>&2
	exit 1
fi

SNIS_ASSET_DIR_ROOT=$(echo $SNIS_ASSET_DIR | sed -e 's/[/]share[/]snis$//')

DESTDIR=
PREFIX=.

SNIS_UPDATE_ASSETS=${BINDIR}/snis_update_assets

if [ -f "${SNIS_ASSET_DIR_ROOT}/last_asset_update_time.txt" ]
then
	# Make sure date supports +%s, some very old ones don't.
	date '+%s' | grep '^[0-9][0-9]*$' > /dev/null 2>&1
	if [ "$?" = "0" ]
	then

		LAST_ASSET_CHK_TIME=$(cat ${SNIS_ASSET_DIR_ROOT}/last_asset_update_time.txt)
		THIS_ASSET_CHK_TIME=$(date '+%s')
		SECONDS_SINCE=$(expr "$THIS_ASSET_CHK_TIME" - "$LAST_ASSET_CHK_TIME")

		# If less than 24 hours, gently suggest not checking again right now.

		if [ "$SECONDS_SINCE" -lt 86400 ]
		then
			if [ "$ask" != "dontask" ]
			then
				printf "\n\nYou have already checked for new assets within the past 24 hours.\n"
				printf "Do you really want to check again now? (y/n) "

				read -r x

				if [ "$x" != "Y" -a "$x" != "y" ]
				then
					exit 0;
				fi
			fi
		fi
	fi
fi

cleanup()
{
	/bin/rm -f "$SNIS_DB_DIR"/snis_downloading.txt
}

trap cleanup EXIT

# snis_client monitors for presence/absence of this file
date > "$SNIS_DB_DIR"/snis_downloading.txt

# copy local assets first
echo "Updating local assets..."
echo "Changing directory to ${DESTDIR}${PREFIX}"

cd ${DESTDIR}${PREFIX}
if [ "$?" != 0 ]
then
	echo "Failed to cd to ${DESTDIR}${PREFIX}" 1>&2
	cleanup
	exit 1
fi

${SNIS_UPDATE_ASSETS} $showprogress --force --destdir "$SNIS_ASSET_DIR_ROOT" --srcdir ./share/snis ;
# download assets from spacenerdsinspace.com

if [ "$localonly" != "localonly" ]
then
	echo "Downloading remote assets from asset server..."
	${SNIS_UPDATE_ASSETS} $showprogress --force --destdir "$SNIS_ASSET_DIR_ROOT" ;
	# only update this for internet updates, not localonly updates.
	date '+%s' > "${SNIS_ASSET_DIR_ROOT}/last_asset_update_time.txt"
fi

cleanup
exit 0

