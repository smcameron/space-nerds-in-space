/* struct termios2 was taken from linux kernel 4.4.119 source, from include/uapi/asm-generic/termbits.h
 * Really, this should get defined by including termios.h, but on my Ubuntu/Mint system,
 * it does not. I think maybe on Fedora it does. Which means this will need some sort of
 * ifdef around it and some feature detection cruft at some point.
 */
struct termios2 {
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_line;			/* line discipline */
	cc_t c_cc[NCCS];		/* control characters */
	speed_t c_ispeed;		/* input speed */
	speed_t c_ospeed;		/* output speed */
};
