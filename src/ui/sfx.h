/* Short UI sounds: move / select / back. Built-in ones live in the romdisk; a theme can
 * override them with move.wav, select.wav, back.wav in its folder. */
#pragma once
typedef enum { SFX_MOVE, SFX_SELECT, SFX_BACK, SFX_COUNT } sfx_t;
void sfx_init(void);
void sfx_theme(const char *theme_dir);   /* NULL = built-in set */
void sfx_play(sfx_t s);
