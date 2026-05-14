/*
 * ================================================================
 *  qcm_wrapper.c  —  Adaptateur leger pour autoecole_qcm.c
 *
 *  N'IMPORTE PAS autoecole_qcm.c — ne jamais modifier ce fichier.
 *  Remplace uniquement :
 *    main()     -> run_qcm(win, ren, score_out, username)
 *    save.dat   -> save_[username].dat
 *    creation fenetre SDL -> fenetre unique du projet
 * ================================================================
 */

/* Masquer main() de l'original avant inclusion */
#define main qcm_original_main__unused__

#include "autoecole_qcm.c"

#undef main

/* ── Fichier de save par utilisateur (global statique) ── */
static char gQcmSaveFile[128] = "save.dat";

static void saveProgress_user(void) {
    FILE *f = fopen(gQcmSaveFile, "w");
    if (!f) return;
    fprintf(f, "unlocked=%d\n", gUnlocked);
    fprintf(f, "exam=%d\n",     gExamUnlocked);
    fclose(f);
}

static void loadProgress_user(void) {
    FILE *f = fopen(gQcmSaveFile, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int val;
        if (sscanf(line, "unlocked=%d", &val) == 1)
            if (val >= 1 && val <= NUM_CHAPTERS) gUnlocked = val;
        if (sscanf(line, "exam=%d", &val) == 1)
            gExamUnlocked = (val == 1) ? 1 : 0;
    }
    fclose(f);
}

/* ── Point d'entree expose au projet ── */
int run_qcm(SDL_Window *_qcm_win, SDL_Renderer *_qcm_ren,
            int *_qcm_score_out, const char *username)
{
    memset(gQcmSaveFile, 0, sizeof(gQcmSaveFile));
    strcpy(gQcmSaveFile, "save.dat");
    if (username && username[0])
        snprintf(gQcmSaveFile, sizeof(gQcmSaveFile), "save_%s.dat", username);


    srand((unsigned)time(NULL));

    { int fsz[5]={14,20,28,36,52};
      for(int fi=0;fi<5;fi++){
        gFonts[fi]=TTF_OpenFont("arial.ttf",fsz[fi]);
        if(!gFonts[fi]){fprintf(stderr,"Erreur arial.ttf: %s\n",TTF_GetError());return 1;}
      }
    }

    gWin = _qcm_win;
    gRen = _qcm_ren;
    SDL_SetWindowTitle(gWin, "Auto-Ecole QCM - Permis B");
    SDL_SetRenderDrawBlendMode(gRen, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(gRen, WIN_W, WIN_H);

    /* Charger les panneaux depuis les donnees integrees */
    for (int pi = 0; pi < 50; pi++) {
        SDL_RWops *rw = SDL_RWFromConstMem(PDATA[pi], PSIZES[pi]);
        if (rw) {
            SDL_Surface *s = SDL_LoadBMP_RW(rw, 1);
            if (s) { gPanneaux[pi] = SDL_CreateTextureFromSurface(gRen, s); SDL_FreeSurface(s); }
        }
    }
    loadProgress_user();
    initStars();
    gPhaseStart = SDL_GetTicks();
    gLastBlink  = SDL_GetTicks();

    while (gState != S_QUIT) {

        /* ── Transition REST→QUESTION/RESULT ── */
        if (gState == S_REST) {
            Uint32 now  = SDL_GetTicks();
            int elapsed = (int)((now - gRestStart) / 1000);
            if (elapsed >= REST_TIME) {
                gQIdx++;
                if (gQIdx >= gTotal) {
                    gState = S_RESULT;
                } else {
                    gPhaseStart = now;
                    gAnswered   = 0;
                    gUserAnswer = -1;
                    gState      = S_QUESTION;
                }
            }
        }

        /* ── Timeout question (sans event) ── */
        if (gState == S_QUESTION && !gAnswered) {
            Uint32 now  = SDL_GetTicks();
            int remain  = QUESTION_TIME - (int)((now - gPhaseStart)/1000);
            if (remain <= 0) {
                gAnswered      = 1;
                gUserAnswer    = -1;
                gFeedbackStart = now;
                gState         = S_FEEDBACK;
            }
        }

        /* ── Transition FEEDBACK→EXPLAIN ou REST ── */
        if (gState == S_FEEDBACK) {
            Uint32 now  = SDL_GetTicks();
            if ((int)((now - gFeedbackStart) / 1000) >= FEEDBACK_TIME) {
                int correct  = ALL_Q[gOrder[gQIdx]].ok;
                int is_wrong = (gUserAnswer != correct);
                int is_chap  = (gTotal != EXAM_TOTAL);
                if (is_wrong && is_chap && ALL_Q[gOrder[gQIdx]].expl != NULL) {
                    gExplainStart = now;
                    gState        = S_EXPLAIN;
                } else {
                    gRestStart = now;
                    gState     = S_REST;
                }
            }
        }

        /* ── Transition EXPLAIN→REST apres EXPLAIN_TIME secondes ── */
        if (gState == S_EXPLAIN) {
            Uint32 now = SDL_GetTicks();
            if ((int)((now - gExplainStart) / 1000) >= EXPLAIN_TIME) {
                gRestStart = now;
                gState     = S_REST;
            }
        }

        /* ── Events ── */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { gState = S_QUIT; break; }
            if (e.type == SDL_WINDOWEVENT &&
                (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                 e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
                SDL_RenderSetLogicalSize(gRen, WIN_W, WIN_H);
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                if (gState == S_QUESTION || gState == S_REST || gState == S_FEEDBACK || gState == S_EXPLAIN) gConfirmQuit = 1;
                else if (gState == S_UNLOCK) { gState = S_CHAPTER; }
                else if (gState == S_EXAMWIN) { gState = S_MENU; }
                else if (gState == S_CHAPTER) { gState = S_MENU; }
                else if (gState == S_MENU) { gState = S_QUIT; }
                else gState = S_MENU;
            }

            if (gConfirmQuit) { handleConfirm(&e); continue; }

            switch (gState) {
                case S_MENU:     handleMenu(&e);     break;
                case S_CHAPTER:  handleChapter(&e);  break;
                case S_QUESTION: handleQuestion(&e); break;
                case S_RESULT:   handleResult(&e);   break;
                case S_UNLOCK:   handleUnlock(&e);   break;
                case S_EXAMWIN:  handleExamWin(&e);  break;
                default: break;
            }
        }

        /* ── Rendu ── */
        SDL_SetRenderDrawColor(gRen, 8, 10, 28, 255);
        SDL_RenderClear(gRen);

        switch (gState) {
            case S_MENU:      renderMenu();      break;
            case S_CHAPTER:   renderChapter();   break;
            case S_COUNTDOWN: renderCountdown(); break;
            case S_QUESTION:  renderQuestion();  break;
            case S_FEEDBACK:  renderFeedback();  break;
            case S_EXPLAIN:   renderExplain();   break;
            case S_REST:      renderRest();      break;
            case S_RESULT:    renderResult();    break;
            case S_UNLOCK:    renderUnlock();    break;
            case S_EXAMWIN:   renderExamWin();   break;
            default: break;
        }

        if (gConfirmQuit) renderConfirm();

        SDL_RenderPresent(gRen);
        SDL_Delay(16);
    }

    saveProgress_user();
    if (_qcm_score_out) {
        *_qcm_score_out = (gTotal > 0) ? (int)((float)gScore / gTotal * 100.0f) : 0;
    }
    free(gOrder); gOrder = NULL;
    gWin = NULL; gRen = NULL;
    for(int pi=0;pi<50;pi++){if(gPanneaux[pi]){SDL_DestroyTexture(gPanneaux[pi]);gPanneaux[pi]=NULL;}}
    for(int fi=0;fi<5;fi++){if(gFonts[fi]){TTF_CloseFont(gFonts[fi]);gFonts[fi]=NULL;}}
    gScore=0; gTotal=0; gUnlocked=1; gState=S_MENU;
    return 0;

}
