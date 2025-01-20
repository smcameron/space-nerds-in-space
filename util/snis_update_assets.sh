#!/bin/sh

echo "Sorry, this shell script has been superseded by snis_update_assets.c" 1>&2
echo "which should be run from within bin/snis_launcher, option 8." 1>&2
exit 1

ASSET_URL="https://spacenerdsinspace.com/snis-assets"
MANIFEST_URL="$ASSET_URL/manifest.txt"
PROG="$0"
MANIFEST_FILE=/tmp/snis_asset_manifest$$.txt
up_to_date_count=0
new_count=0
update_count=0
update_fail_count=0
DESTDIR=.
FETCH="fetch"
localcopy=0
export dryrun=0

usage()
{
	echo "Usage:"
	echo "snis_update_assets.sh"
	echo "snis_update_assets.sh --localcopy --destdir some-directory"
	echo "snis_update_assets.sh --dry-run"
	echo "snis_update_assets.sh --dry-run --localcopy --destdir some-directory"
	exit 0
}

sanity_check_environment()
{
	if [ ! -d ${DESTDIR}/share/snis/solarsystems ]
	then
		echo "$PROG"': Cannot find '"${DESTDIR}"'/share/snis/solarsystems directory. Exiting.' 1>&2
		exit 1
	fi
	return 0
}

fetch_file()
{
	local URL="$1"
	local FILE="$2"
	local dryrun="$3"
	local updating_or_creating="$4"
	local localcopy="$5"

	if [ "$dryrun" != "0" ]
	then
		updating_or_creating="Not $updating_or_creating"
	fi
	echo -n 1>&2 "$updating_or_creating $FILE... "
	if [ "$dryrun" = "0" ]
	then
		if [ "$localcopy" = 0 ]
		then
			wget --quiet "$1" -O - > "$FILE"
			if [ "$?" != "0" ]
			then
				/bin/rm -f "$FILE"
				echo 1>&2
				echo "$PROG"': Failed to fetch '"$URL" 1>&2
				return 1
			fi
		else
			if [ "$1" = "$FILE" ]
			then
				echo "$1 and $FILE are the same." 1>&2
			else
				/bin/cp "$1" "$FILE"
				if [ "$?" != "0" ]
				then
					/bin/rm -f "$FILE"
					echo 1>&2
					echo "$PROG"': Failed to copy '"$URL" 1>&2
					return 1
				fi
			fi
		fi
	fi
	echo "done" 1>&2
	return 0
}

move_file()
{
	local dryrun="$1"
	local old="$2"
	local new="$3"
	if [ "$dryrun" = "0" ]
	then
		mv -f "$old" "$new"
	fi
	return $?
}

update_file()
{
	local checksum="$1"
	local filename="$2"
	local localcopy="$3"
	if [ -f "$DESTDIR/$filename" ]
	then
		localchksum=$(md5sum "$DESTDIR"/"$filename" | awk '{ print $1 }')
		if [ "$localchksum" = "$checksum" ]
		then
			up_to_date_count=$((up_to_date_count + 1))
		else
			move_file "$dryrun" "$DESTDIR"/"$filename" "$DESTDIR"/"$filename".old
			if [ "$?" != "0" ]
			then
				echo "$PROG"':Cannot move old '"$DESTDIR"/"$filename"' out of the way, skipping' 1>&2
				update_fail_count=$((update_fail_count + 1))
			else
				fetch_file "$ASSET_URL"/"$filename" "$DESTDIR"/"$filename" "$dryrun" "Updating" $localcopy 
				if [ "$?" != "0" ]
				then
					update_fail_count=$((update_fail_count + 1))
				else
					update_count=$((update_count+1))
				fi
			fi
		fi
	else
		local dname=$(dirname "$DESTDIR"/"$filename")
		if [ ! -d "$dname" ]
		then
			if [ "$dryrun" = "0" ]
			then
				mkdir -p "$dname"
				if [ "$?" != "0" ]
				then
					echo "$PROG"': Failed to create directory for '"$DESTDIR"/"$filename" 1>&2
					update_fail_count=$((update_fail_count + 1))
				fi
			fi
		fi
		if [ -d "$dname" -o "$dryrun" != "0" ]
		then
			fetch_file $ASSET_URL/$filename "$DESTDIR"/"$filename" "$dryrun" "Creating" $localcopy
			if [ "$?" = "0" ]
			then
				new_count=$((new_count+1))
			else
				update_fail_count=$((update_fail_count+1))
			fi
		fi
	fi
}

update_files()
{
	MANIFEST="$1"
	while (true)
	do
		read x
		if [ "$?" != "0" ]
		then
			break;
		fi
		update_file $x $localcopy
	done < "$MANIFEST"
}

output_statistics()
{
	if [ "$dryrun" = "0" ]
	then
		updated="updated"
		new="new files"
	else
		updated="would be updated"
		new="new files would be created"
	fi

	echo
	echo "$up_to_date_count files already up to date"
	echo "$update_count files $updated"
	echo "$new_count $new"
	echo "$update_fail_count update failures"
	echo
}

if [ "$1" = "--help" ]
then
	usage;
fi

if [ "$1" = "--dry-run" ]
then
	dryrun=1
	shift;
fi

if [ "$1" = "--localcopy" ]
then
	shift;
	ASSET_URL="./"
	localcopy=1
	MANIFEST_URL=./share/snis/original_manifest.txt
	FETCH="copy"
	echo "Local copy mode enabled"
fi

if [ "$1" = "--destdir" ]
then
	shift;
	DESTDIR="$1"
	echo "Using DESTDIR=$DESTDIR"
fi

if [ "$localcopy" = "1" -a "$DESTDIR" = "." ]
then
	echo "Cannot localcopy from . to ." 1>&2
	exit 1
fi

sanity_check_environment
fetch_file "$MANIFEST_URL" "$MANIFEST_FILE" 0 "$FETCH""ing" $localcopy
if [ "$?" != "0" ]
then
	exit 1
fi
update_files "$MANIFEST_FILE"
output_statistics

/bin/cp "$MANIFEST_FILE" "$DESTDIR"/share/snis/original_manifest.txt
/bin/rm "$MANIFEST_FILE"

