#pragma once
/* 3D-ish cover carousel: center cover, receding neighbours, floor reflection. */
void coverflow_reset(int cursor);
void coverflow_draw(int cursor, float *pos /* animated position, lerped toward cursor */);
