/* Secondary screens; each has update(in) returning an action and draw(). */
#pragma once
#include "input.h"
#include "../list.h"

enum { ACT_NONE = 0, ACT_CLOSE, ACT_LAUNCH, ACT_OPEN_CHAT, ACT_OPEN_AI };

/* details modal */
void details_open(game_t *g);
int  details_update(const input_t *in);
void details_draw(void);
game_t *details_game(void);

/* last-played sidebar */
void sidebar_open(void);
int  sidebar_update(const input_t *in);
void sidebar_draw(float base_alpha);
game_t *sidebar_game(void);

/* Cast Chat */
void chat_open(void);
int  chat_update(const input_t *in);
void chat_draw(void);
void chat_background_poll(void);       /* keep the socket serviced from any screen */
int  chat_unread(void);

/* Cast AI */
void ai_open(void);
int  ai_update(const input_t *in);
void ai_draw(void);

/* shared connection (chat + AI share one socket) */
void chatnet_ensure(void);             /* bring network + socket up if needed */
int  chatnet_online(void);
void ai_on_reply(const char *text);    /* called by chat_background_poll on EV_AI */
