#ifndef COMPAT_H
#define COMPAT_H

#ifdef __WIN32__

/* Windows variants of various things */

#define JOYSTICK_DEV_OPEN_FLAGS O_RDONLY
#define srandom srand
#define random rand

/* End of Windows variants of various things */

#else
#ifdef __linux__

/* Linux variants of various things */

#define JOYSTICK_DEV_OPEN_FLAGS O_RDONLY | O_NONBLOCK
#define HAS_LINUX_JOYSTICK_INTERFACE 1
#define O_BINARY 0

/* End of Linux variants of various things */

#else

/* Generic variants of various things */

#define JOYSTICK_DEV_OPEN_FLAGS O_RDONLY | O_NONBLOCK
#define O_BINARY 0

/* End of generic variants of various things */

#endif /* __linux__ */
#endif /* __WIN32__ */
#endif /* COMPAT_H */

