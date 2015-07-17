BEGIN {
	n = 0
}

/docking_port\(/ { model[n] = 0 }
/docking_port2\(/ { model[n] = 1 }
/docking_port3\(/ { model[n] = 2 }

/docking_port[23]*\(/ {
	split($0,a,"[(),]")
	dpos[n] = a[2] ", " a[3] ", " a[4]
	drot[n] = a[5] ", " a[6] ", " a[7] ", " a[8]
	dscale[n] = a[9]
	n++
}

END {
	if ( n  == 0 ) {
		print "{ " n "}"
	} else {
		print "{ " n ", {"
		for (i = 0; i < n; i++) {
			print "  { " model[i], dscale[i] ", { { " dpos[i] " }, { " drot[i] " } } },"
		}
		print "} }"
	}
}

