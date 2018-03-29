#!/bin/sh

usage()
{
	echo 'usage: snis_generate_asset_manifest $ASSET_DIR > manifest.txt' 1>&2
	exit 1
}

ASSET_DIR="$1"

if [ ! -d ${ASSET_DIR} ]
then
	usage
fi

generate_asset_record()
{
	while (true) do
		read filename
		if [ "$?" != "0" -o x"$filename" = "x" ]
		then
			break;
		fi
		md5sum "$filename"
	done
}

find ${ASSET_DIR} -type f -print | generate_asset_record

