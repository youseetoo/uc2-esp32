#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* T1.4: app entry point — main/main.cpp's app_main() renamed when it moved
 * into uc2_app (T1.4); the app component's main.c calls this from
 * app_main() (refs/component-layout.md §2). */
void uc2_app_start(void);

#ifdef __cplusplus
}
#endif
