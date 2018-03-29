#!/bin/sh

ASSET_URL="http://spacenerdsinspace.com/snis-assets"
MANIFEST_URL="$ASSET_URL/manifest.txt"
PROG="$0"
MANIFEST_FILE=/tmp/snis_asset_manifest$$.txt
up_to_date_count=0
new_count=0
update_count=0
update_fail_count=0

sanity_check_environment()
{
	if [ ! -d share/snis/solarsystems ]
	then
		echo "$PROG"': Cannot find share/snis/solarsystems directory. Exiting.' 1>&2
		exit 1
	fi
	return 0
}

fetch_file()
{
	URL="$1"
	FILE="$2"
	echo -n 1>&2 "Fetching $URL... "
	wget --quiet "$1" -O - > "$FILE"
	if [ "$?" != "0" ]
	then
		/bin/rm -f "$FILE"
		echo 1>&2
		echo "$PROG"': Failed to fetch '"$URL" 1>&2
		return 1
	fi
	echo "done" 1>&2
	return 0
}

move_file()
{
	old="$1"
	new="$2"
	mv -f "$old" "$new"
	return $?
}

update_file()
{
	checksum="$1"
	filename="$2"
	if [ -f "$filename" ]
	then
		localchksum=$(md5sum $filename | awk '{ print $1 }')
		if [ "$localchksum" = "$checksum" ]
		then
			up_to_date_count=$((up_to_date_count + 1))
		else
			move_file "$filename" "$filename".old
			if [ "$?" != "0" ]
			then
				echo "$PROG"':Cannot move old $filename out of the way, skipping' 1>&2
				update_fail_count=$((update_fail_count + 1))
			else
				fetch_file "$ASSET_URL"/"$filename" "$filename"
				if [ "$?" != "0" ]
				then
					update_fail_count=$((update_fail_count + 1))
				else
					update_count=$((update_count+1))
				fi
			fi
		fi
	else
		dname=$(dirname "$filename")
		if [ ! -d "$dname" ]
		then
			mkdir -p $dname
			if [ "$?" != "0" ]
			then
				echo "$PROG"': Failed to create directory for '"$filename" 1>&2
				update_fail_count=$((update_fail_count + 1))
			fi
		fi
		if [ -d "$dname" ]
		then
			fetch_file $ASSET_URL/$filename $filename
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
		update_file $x
	done < "$MANIFEST"
}

output_statistics()
{
	echo
	echo "$up_to_date_count files already up to date"
	echo "$update_count files updated"
	echo "$new_count new files"
	echo "$update_fail_count update failures"
	echo
}

sanity_check_environment
fetch_file "$MANIFEST_URL" "$MANIFEST_FILE"
if [ "$?" != "0" ]
then
	exit 1
fi
update_files "$MANIFEST_FILE"
output_statistics

/bin/rm "$MANIFEST_FILE"

