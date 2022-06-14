#ifndef __AMIGATERM_SCREEN_H__
#define __AMIGATERM_SCREEN_H__

extern	struct Window *mywindow;         /* ptr to applications window */

extern	int screen_init(void);
extern	void screen_cleanup(void);

extern	void emits(const char *str);
extern	void emit(char c);

#endif
