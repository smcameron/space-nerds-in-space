#!/bin/sh

ps aux | egrep 'ssgl|snis' | grep -v 'grep'

if [ "$?" = "0" ]
then
	ps aux | egrep 'ssgl|snis' | grep -v 'grep' | awk '{ printf("kill %s\n", $2); }' | /bin/sh
	echo "Killed the above processes...." 
fi



