/*
 * ================================================================
 *   PROJET AUTO-ECOLE — Point d'entree unique
 *
 *   Commande de compilation MSYS2/MinGW-w64 :
 *
 *   gcc main.c module_simulation.c module_guide.c qcm_wrapper.c -o autoecole.exe -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lavcodec -lavformat -lavutil -lswscale -lswresample -lgdi32 -lcomdlg32 -lm -O2
 *
 *   Fichiers requis dans le meme dossier :
 *     autoecole_qcm.c  (original, non modifie)
 *     arial.ttf, DejaVuSans.ttf, vrai.png, intro.png, video.h
 *     SDL2.dll, SDL2_image.dll, SDL2_ttf.dll, SDL2_mixer.dll
 * ================================================================
 */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

/* Prototypes des modules */
int run_simulation(SDL_Window *win, SDL_Renderer *ren, int *score_out);
int run_guide(SDL_Window *win, SDL_Renderer *ren);
int run_qcm(SDL_Window *win, SDL_Renderer *ren, int *score_out, const char *username);

/* test_modifie.c contient auth + menu + main() */
#include "test_modifie.c"
