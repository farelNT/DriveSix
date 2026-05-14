/* ===== MODULE GUIDE — run_guide() ===== */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define GUIDE_W   900
#define GUIDE_H   650
#define GUIDE_MAX_L  50
#define GUIDE_LPP    10

static SDL_Window   *g_guide_win  = NULL;
static SDL_Renderer *g_guide_ren  = NULL;
static TTF_Font     *g_guide_font = NULL;
static int           g_win_w      = 900;
static int           g_win_h      = 650;




// ================= FOND =================
void drawBackground(){
    for(int i=0;i<g_win_h;i++){
        SDL_SetRenderDrawColor(g_guide_ren, 20+i/12, 20+i/15, 80+i/5,255);
        SDL_RenderDrawLine(g_guide_ren,0,i,g_win_w,i);
    }
}

// ================= TEXTE ANIMÉ =================
void drawTextAnimated(const char *text,int y,int visibleChars){
    char buffer[512];
    int len=strlen(text);

    if(visibleChars>len) visibleChars=len;

    strncpy(buffer,text,visibleChars);
    buffer[visibleChars]='\0';

    SDL_Color color={240,240,255};
    SDL_Surface *surf=TTF_RenderText_Blended(g_guide_font,buffer,color);
    SDL_Texture *tex=SDL_CreateTextureFromSurface(g_guide_ren,surf);

    SDL_Rect rect={(g_win_w-surf->w)/2,y,surf->w,surf->h};
    SDL_RenderCopy(g_guide_ren,tex,NULL,&rect);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

// ================= TITRE =================
SDL_Color getTitleColor(int mode){
    switch(mode){
        case 1: return (SDL_Color){255,100,100};
        case 2: return (SDL_Color){100,255,100};
        case 3: return (SDL_Color){100,150,255};
        case 4: return (SDL_Color){255,200,100};
        case 5: return (SDL_Color){200,100,255};
        default: return (SDL_Color){255,255,255};
    }
}

void drawTitleBox(const char *text,int y,SDL_Color color){
    SDL_Surface *surf=TTF_RenderText_Blended(g_guide_font,text,color);
    SDL_Texture *tex=SDL_CreateTextureFromSurface(g_guide_ren,surf);

    SDL_Rect rect={(g_win_w-surf->w)/2,y,surf->w,surf->h};
    SDL_Rect box={rect.x-20,rect.y-10,rect.w+40,rect.h+20};

    SDL_SetRenderDrawColor(g_guide_ren,0,0,0,200);
    SDL_RenderFillRect(g_guide_ren,&box);

    SDL_SetRenderDrawColor(g_guide_ren,color.r,color.g,color.b,255);
    SDL_RenderDrawRect(g_guide_ren,&box);

    SDL_RenderCopy(g_guide_ren,tex,NULL,&rect);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

// ================= VOITURE =================
void drawFilledCircle(int cx, int cy, int r){
    for(int dy = -r; dy <= r; dy++){
        for(int dx = -r; dx <= r; dx++){
            if(dx*dx + dy*dy <= r*r){
                SDL_RenderDrawPoint(g_guide_ren, cx + dx, cy + dy);
            }
        }
    }
}

void drawCar(int x){

    int y = 520;

    // ===== OMBRE =====
    SDL_SetRenderDrawColor(g_guide_ren, 0, 0, 0, 80);
    SDL_Rect shadow = {x+10, y+60, 200, 15};
    SDL_RenderFillRect(g_guide_ren, &shadow);

    // ===== CARROSSERIE BASSE =====
    SDL_SetRenderDrawColor(g_guide_ren, 200, 30, 30, 255);
    SDL_Rect base = {x, y, 200, 50};
    SDL_RenderFillRect(g_guide_ren, &base);

    // ===== CARROSSERIE HAUTE (forme plus naturelle) =====
    SDL_Rect top = {x+30, y-30, 140, 50};
    SDL_RenderFillRect(g_guide_ren, &top);

    // ===== PARE-BRISE (incliné) =====
    SDL_SetRenderDrawColor(g_guide_ren, 120, 200, 255, 255);
    SDL_Rect windshield = {x+40, y-25, 40, 35};
    SDL_Rect rearGlass  = {x+100, y-25, 50, 35};
    SDL_RenderFillRect(g_guide_ren, &windshield);
    SDL_RenderFillRect(g_guide_ren, &rearGlass);

    // ===== CONTOUR =====
    SDL_SetRenderDrawColor(g_guide_ren, 0, 0, 0, 255);
    SDL_RenderDrawRect(g_guide_ren, &base);
    SDL_RenderDrawRect(g_guide_ren, &top);

    // ===== PORTE =====
    SDL_RenderDrawLine(g_guide_ren, x+95, y, x+95, y+50);

    // ===== POIGNÉE =====
    SDL_RenderDrawLine(g_guide_ren, x+85, y+20, x+95, y+20);

    // ===== PHARE AVANT =====
    SDL_SetRenderDrawColor(g_guide_ren, 255, 255, 100, 255);
    SDL_Rect headlight = {x+190, y+10, 10, 10};
    SDL_RenderFillRect(g_guide_ren, &headlight);

    // ===== FEU ARRIÈRE =====
    SDL_SetRenderDrawColor(g_guide_ren, 255, 50, 50, 255);
    SDL_Rect taillight = {x-5, y+10, 10, 10};
    SDL_RenderFillRect(g_guide_ren, &taillight);

    // ===== ROUES =====
    SDL_SetRenderDrawColor(g_guide_ren, 20, 20, 20, 255);
    drawFilledCircle(x+50, y+55, 20);
    drawFilledCircle(x+150, y+55, 20);

    // ===== JANTES =====
    SDL_SetRenderDrawColor(g_guide_ren, 180, 180, 180, 255);
    drawFilledCircle(x+50, y+55, 10);
    drawFilledCircle(x+150, y+55, 10);
}
   

// ================= MENU =================
void menu(int carX){
    drawBackground();

    drawTitleBox("=== GUIDE D'UTILISATION ===",70,(SDL_Color){255,255,0});

    drawTitleBox("1 - Presentation",180,(SDL_Color){255,100,100});
    drawTitleBox("2 - Examen theorique",230,(SDL_Color){100,255,100});
    drawTitleBox("3 - Simulation",280,(SDL_Color){100,150,255});
    drawTitleBox("4 - Fonctionnement",330,(SDL_Color){255,200,100});
    drawTitleBox("5 - Conseils",380,(SDL_Color){200,100,255});

    drawTitleBox("ESC - Quitter",450,(SDL_Color){255,255,255});

    drawCar(carX);
    SDL_RenderPresent(g_guide_ren);
}

// ================= PAGE =================
void page(const char *title,const char *lines[],int total,int p,int carX,int mode){

    drawBackground();

    drawTitleBox(title,50,getTitleColor(mode));

    int start=p*GUIDE_LPP;
    int y=150;

    static int charCount=0;
    charCount+=2;

    for(int i=start;i<start+GUIDE_LPP && i<total;i++){
        drawTextAnimated(lines[i],y,charCount);
        y+=40;
    }

    char pageInfo[50];
    sprintf(pageInfo,"Page %d",p+1);
    drawTextAnimated(pageInfo,600,50);

    drawCar(carX);
    SDL_RenderPresent(g_guide_ren);
}

// ================= MAIN =================
int run_guide(SDL_Window *_ext_win, SDL_Renderer *_ext_ren){

    g_guide_win = _ext_win;
    g_guide_ren = _ext_ren;

    /* Adapter à la taille réelle de la fenêtre */
    SDL_GetRendererOutputSize(g_guide_ren, &g_win_w, &g_win_h);

    g_guide_font=TTF_OpenFont("arial.ttf",18);
    if(!g_guide_font){
        printf("Erreur police\n");
        return 1;
    }

    int run=1,mode=0,pageIndex=0,carX=0;
    SDL_Event e;

    // ================= DONNÉES (35 lignes chacune) =================

    const char *presentation[GUIDE_MAX_L]={
"L'application simule une auto-ecole moderne.",
"Elle permet d'apprendre la conduite facilement.",
"Elle guide l'utilisateur pas a pas.",
"Elle rend l'apprentissage simple.",
"Elle facilite la comprehension du code.",
"Elle propose une methode efficace.",
"Elle aide a developper les reflexes.",
"Elle renforce les connaissances.",
"Elle favorise la memorisation.",
"Elle structure l'apprentissage.",
"Elle rend l'utilisateur autonome.",
"Elle facilite la progression.",
"Elle encourage la pratique.",
"Elle rend le systeme intuitif.",
"Elle donne confiance.",
"Elle prepare aux examens.",
"Elle aide a comprendre les situations.",
"Elle favorise la concentration.",
"Elle developpe la logique.",
"Elle ameliore la reflexion.",
"Elle permet de progresser rapidement.",
"Elle facilite la navigation.",
"Elle rend l'utilisation simple.",
"Elle ameliore la comprehension globale.",
"Elle constitue une base solide.",
"Elle renforce les acquis.",
"Elle permet une experience fluide.",
"Elle aide a eviter les erreurs.",
"Elle encourage la repetition.",
"Elle rend l'apprentissage interessant.",
"Elle favorise la reussite.",
"Elle ameliore les performances.",
"Elle structure les connaissances.",
"Elle optimise le temps d'apprentissage.",
"Elle rend l'experience agreable."
};

    const char *examen[GUIDE_MAX_L]={
"L'examen theorique teste vos connaissances.",
"Il se presente sous forme de QCM.",
"Chaque question a une seule bonne reponse.",
"Il faut bien analyser chaque proposition.",
"Le test porte sur le code de la route.",
"Il inclut les panneaux de signalisation.",
"Les situations de circulation sont evaluees.",
"Les reponses sont enregistrees automatiquement.",
"Un score est calcule a la fin.",
"Il permet de mesurer le niveau.",
"Un bon resultat permet de progresser.",
"Un mauvais resultat encourage a recommencer.",
"Il aide a corriger les erreurs.",
"Il renforce la memoire.",
"Il developpe la reflexion.",
"Il favorise la concentration.",
"Il impose une certaine rigueur.",
"Il permet une evaluation rapide.",
"Il donne un resultat immediat.",
"Il encourage la perseverance.",
"Il developpe la logique.",
"Il aide a anticiper les erreurs.",
"Il permet une progression continue.",
"Il valide les acquis.",
"Il facilite la memorisation.",
"Il renforce les bases.",
"Il favorise l'apprentissage actif.",
"Il stimule l'attention.",
"Il renforce les competences.",
"Il prepare a l'examen reel.",
"Il developpe la vigilance.",
"Il aide a prendre de bonnes decisions.",
"Il constitue un outil pedagogique.",
"Il assure une base theorique solide.",
"Il permet de s'ameliorer constamment."
};

    const char *simulation[GUIDE_MAX_L]={
"La simulation permet de pratiquer la conduite.",
"Elle reproduit un environnement realiste.",
"L'utilisateur agit comme un conducteur.",
"Il doit respecter les regles.",
"Il analyse les situations en temps reel.",
"Il prend des decisions rapides.",
"Il rencontre des obstacles.",
"Il observe la signalisation.",
"Il adapte son comportement.",
"Il developpe ses reflexes.",
"Il renforce sa reaction.",
"Il apprend a anticiper les dangers.",
"Il evite les erreurs courantes.",
"Il comprend ses actions.",
"Il agit avec prudence.",
"Il developpe la vigilance.",
"Il renforce la securite.",
"Il apprend a rester calme.",
"Il controle ses actions.",
"Il observe son environnement.",
"Il corrige ses erreurs.",
"Il s'entraine regulierement.",
"Il developpe la logique.",
"Il gagne en confiance.",
"Il ameliore sa precision.",
"Il maitrise la conduite.",
"Il simule des situations reelles.",
"Il apprend progressivement.",
"Il teste ses competences.",
"Il applique la theorie.",
"Il devient autonome.",
"Il comprend les risques.",
"Il adopte une conduite responsable.",
"Il evite les accidents.",
"Il renforce la concentration."
};

    const char *fonctionnement[GUIDE_MAX_L]={
"Le programme commence par un ecran d'accueil.",
"Le menu principal est affiche.",
"Il contient plusieurs options.",
"Chaque option correspond a une section.",
"L'utilisateur choisit une rubrique.",
"Le programme affiche le contenu.",
"Les informations sont organisees.",
"Le texte est lisible.",
"L'utilisateur change de page.",
"Il utilise les fleches.",
"Il peut revenir au menu.",
"Le systeme est simple.",
"Il est intuitif.",
"Il facilite la navigation.",
"Il guide l'utilisateur.",
"Il structure les informations.",
"Il rend la lecture agreable.",
"Il favorise la comprehension.",
"Il est accessible a tous.",
"Il est interactif.",
"Il est stable.",
"Il fonctionne correctement.",
"Il permet une repetition facile.",
"Il offre une bonne experience.",
"Il simplifie l'apprentissage.",
"Il optimise le temps.",
"Il rend l'interface claire.",
"Il organise les actions.",
"Il simplifie les etapes.",
"Il rend le programme pratique.",
"Il assure une bonne organisation.",
"Il facilite la progression.",
"Il garantit la lisibilite.",
"Il assure une bonne ergonomie.",
"Il rend le systeme efficace."
};

    const char *conseils[GUIDE_MAX_L]={
"Lisez attentivement les instructions.",
"Prenez le temps de comprendre.",
"Ne vous precipitez pas.",
"Analysez chaque situation.",
"Reflechissez avant de repondre.",
"Evitez de repondre au hasard.",
"Entrainez-vous regulierement.",
"Corrigez vos erreurs.",
"Apprenez de vos fautes.",
"Revisez les notions importantes.",
"Restez concentre.",
"Evitez les distractions.",
"Respectez les etapes.",
"Suivez une methode.",
"Adoptez une attitude serieuse.",
"Faites preuve de patience.",
"Ne vous decouragez pas.",
"Cherchez a vous ameliorer.",
"Travaillez avec rigueur.",
"Faites preuve de discipline.",
"Memorisez les regles.",
"Observez les situations.",
"Ameliorez vos reflexes.",
"Restez calme.",
"Evitez le stress.",
"Concentrez-vous sur vos objectifs.",
"Pratiquez souvent.",
"Ameliorez vos performances.",
"Apprenez progressivement.",
"Analysez vos erreurs.",
"Developpez votre logique.",
"Faites preuve de prudence.",
"Respectez la securite.",
"Soyez responsable.",
"Continuez a progresser."
};

    while(run){

        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT) run=0;

            if(e.type==SDL_KEYDOWN){

                if(mode==0){
                    switch(e.key.keysym.sym){
                        case SDLK_1: mode=1;pageIndex=0;break;
                        case SDLK_2: mode=2;pageIndex=0;break;
                        case SDLK_3: mode=3;pageIndex=0;break;
                        case SDLK_4: mode=4;pageIndex=0;break;
                        case SDLK_5: mode=5;pageIndex=0;break;
                        case SDLK_ESCAPE: run=0;break;
                    }
                }
                else{
                    int maxPages=(35+GUIDE_LPP-1)/GUIDE_LPP;

                    if(e.key.keysym.sym==SDLK_RIGHT && pageIndex<maxPages-1)
                        pageIndex++;

                    if(e.key.keysym.sym==SDLK_LEFT && pageIndex>0)
                        pageIndex--;

                    if(e.key.keysym.sym==SDLK_ESCAPE)
                        mode=0;
                }
            }
        }

        carX+=2;
        if(carX>g_win_w) carX=-200;

        switch(mode){
            case 0: menu(carX); break;
            case 1: page("PRESENTATION",presentation,35,pageIndex,carX,mode); break;
            case 2: page("EXAMEN",examen,35,pageIndex,carX,mode); break;
            case 3: page("SIMULATION",simulation,35,pageIndex,carX,mode); break;
            case 4: page("FONCTIONNEMENT",fonctionnement,35,pageIndex,carX,mode); break;
            case 5: page("CONSEILS",conseils,35,pageIndex,carX,mode); break;
        }

        SDL_Delay(16);
    }

    TTF_CloseFont(g_guide_font); g_guide_font=NULL;
    /* Fenetre et renderer appartiennent au main — on ne les détruit pas */
    g_guide_win=NULL; g_guide_ren=NULL;

    return 0;
}