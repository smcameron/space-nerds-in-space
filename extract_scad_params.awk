BEGIN {
	n = 0
}

/imposter_thrust\(/ {
	split($0,a,"[(),]")
	pos[n] = a[2] ", " a[3] ", " a[4]
	scale[n] = a[5]
	n++
}

END {
	if ( n  == 0 ) {
		print "{ " n "}"
	} else {
		print "{ " n ", {"
		for (i = 0; i < n; i++) {
			print "  { " scale[i] ", { { " pos[i] " } } },"
		}
		print "} }"
	}
}

