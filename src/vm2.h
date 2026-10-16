/* VM2 / VMUPro / Pico2Maple "Game ID": a vendor Maple command (33) that tells a virtual VMU which
 * save image to mount. Protocol as implemented by openMenu (megavolt85). */
#pragma once
int  vm2_send_id(const char *product, const char *name);   /* to every virtual VMU found; returns count */
int  vmu_clock_sync(void);                                 /* RTC unset -> take the time from the first VMU; 1 if set */
