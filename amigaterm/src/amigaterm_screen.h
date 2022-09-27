#ifndef __AMIGATERM_SCREEN_H__
#define __AMIGATERM_SCREEN_H__

#define	AMIGATERM_SCREEN_BACKGROUND_PEN	0
#define	AMIGATERM_SCREEN_TEXT_PEN	1
#define	AMIGATERM_SCREEN_CURSOR_PEN	3

extern	struct Window *mywindow;         /* ptr to applications window */

extern	int screen_init(void);
extern	void screen_cleanup(void);

extern	void emits(const char *str);
extern	void emit(char c);

extern	void draw_cursor(char pen, bool do_xor);

#endif
