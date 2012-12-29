/* Gnome developers "fixed" some badly named defines by 
 * renaming them, thus breaking old code, instead of just
 * deprecating them and giving you a warning.
 */
#ifndef GDK_KEY_Delete
#define GDK_KEY_Delete GDK_Delete
#endif

#ifndef GDK_KEY_Left
#define GDK_KEY_Left GDK_Left
#endif

#ifndef GDK_KEY_Right
#define GDK_KEY_Right GDK_Right
#endif

#ifndef GDK_KEY_BackSpace
#define GDK_KEY_BackSpace GDK_BackSpace
#endif
