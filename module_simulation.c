/* ===== MODULE SIMULATION — run_simulation() ===== */
/*
 * AUTO-ECOLE SIMULATION - SDL2 / C
 * gcc autoecole.c -o autoecole.exe -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
 *
 * CHAP1 CONTROLES :
 *   HAUT/BAS        = accelerer / freiner
 *   GAUCHE/DROITE   = changer de voie (lateral)
 *   C               = regulateur de vitesse (cruise control)
 *   N               = mode nuit on/off
 *   V               = pleins phares (nuit)
 *   B               = veilleuses (nuit)
 *   A               = clignotant gauche
 *   D               = clignotant droit
 *   ECHAP           = retour menu
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ================================================================
   DIMENSIONS FENETRE
================================================================ */
#define WIDTH      1200
#define HEIGHT     700
#define FPS_DELAY  16

/* ================================================================
   CHAP1 – ROUTE
   Vue du conducteur : le joueur monte vers le haut.
   Voie DROITE  (x 300..600) : voitures descendent ↓ (même sens = trafic venant de derrière sur la droite)
   ATTENTION : En France on roule à droite.
     - Voie droite (relative à la route) = coté droit en montant
       Dans notre repere ecran avec le joueur qui monte :
         sa voie = x in [300..600]  → les autres dans ce sens DESCENDENT (viennent en face, non)
       
   LOGIQUE RETENUE (confirmee par l'utilisateur) :
     Voie DROITE (x 300..600)  : IA descendent ↓  (viennent en face du joueur)
     Voie GAUCHE (x 600..900)  : IA montent  ↑  (meme sens que le joueur)
     Le JOUEUR monte ↑ et peut etre sur n'importe quelle voie.
     Sa VOIE CORRECTE = voie gauche (x 600..900), meme sens que lui.
     La VOIE OPPOSEE  = voie droite (x 300..600), circulation descendante.
================================================================ */
#define C1_ROAD_X    300
#define C1_ROAD_W    600
#define C1_MID_X     (C1_ROAD_X + C1_ROAD_W/2)   /* 600 */
/* Voie droite  : x in [300..600], IA descendent */
#define C1_LANE_R_CX (C1_ROAD_X + C1_ROAD_W/4)   /* 450 */
/* Voie gauche  : x in [600..900], IA + joueur montent */
#define C1_LANE_L_CX (C1_ROAD_X + 3*C1_ROAD_W/4) /* 750 */

/* Sous-voies dans la voie gauche (pour depasser sans collision) */
#define C1_SUBLANE_A  720   /* sous-voie A (droite de la voie gauche) */
#define C1_SUBLANE_B  780   /* sous-voie B (gauche de la voie gauche) */

#define C1_NUM_DESC  4   /* IA voie droite, descendent */
#define C1_NUM_ASCE  4   /* IA voie gauche, montent (meme sens joueur) */
#define C1_NUM_AI    (C1_NUM_DESC + C1_NUM_ASCE)

/* Dimensions voiture */
#define CAR_W   62
#define CAR_H   112

/* Vitesses (unites internes, *9 = km/h fictif) */
#define SPEED_LIMIT   5.5f    /* ~50 km/h */
#define SPEED_MAX     8.5f    /* ~76 km/h hard cap */
#define SPEED_REVERSE 2.0f

/* Espacement minimal entre IA de meme sens */
#define MIN_GAP_SAME  200.0f
/* Distance a partir de laquelle une IA evite le joueur */
#define AVOID_DIST    240.0f

/* ================================================================
   CHAP4
================================================================ */
#define CX         (WIDTH/2)
#define CY         (HEIGHT/2)
#define R_ISLAND   90
#define R_INNER    130
#define R_OUTER    230
#define ROAD_HALF  90
#define NUM_AI4    4
#define AI4_SPD    2.5f
#define AI4_CIRC   180.0f

/* ================================================================
   TYPES
================================================================ */
typedef enum { ACCUEIL, MENU, CHAP1, CHAP4 } Etat;
typedef struct { float x, y, angle, speed; } Car;

/* Etat clignotant */
typedef enum { CLIG_OFF, CLIG_LEFT, CLIG_RIGHT } CligState;

/* Mode phares */
typedef enum { LIGHT_OFF, LIGHT_VEILLEUSE, LIGHT_PLEIN } LightMode;

/* IA Chap1 */
typedef enum { IA_NORMAL, IA_AVOID_LEFT, IA_AVOID_RIGHT } IAAvoid;
typedef struct {
    float  x;               /* position x ecran */
    float  worldY;          /* position y monde */
    float  speed;           /* vitesse (toujours >= 0) */
    float  targetX;         /* x cible (pour evitement) */
    int    goingUp;         /* 1=monte (meme sens joueur), 0=descend */
    int    subLane;         /* 0 ou 1 : sous-voie dans la voie gauche */
    Uint8  colorR,colorG,colorB;
    IAAvoid avoidState;
    CligState cligIA;       /* clignotant de l'IA */
    Uint32    cligIAEnd;    /* timestamp fin du clignotant IA */
    int    honkReact;       /* 1 = reagit au klaxon (se decale) */
} AICarC1;

/* IA Chap4 */
typedef enum { AI4_APPROCHE,AI4_ATTENTE,AI4_GIRATOIRE,AI4_SORTIE } AI4State;
typedef struct {
    float x,y,angle,speed;
    AI4State state;
    float entreeAngle,sortieAngle,circAngle;
} AICarC4;

/* ================================================================
   UTILS
================================================================ */
static float normAngle(float a){
    while(a> (float)M_PI) a-=2*(float)M_PI;
    while(a<-(float)M_PI) a+=2*(float)M_PI;
    return a;
}
static float vdist(float ax,float ay,float bx,float by){
    float dx=ax-bx,dy=ay-by; return sqrtf(dx*dx+dy*dy);
}
static float fclamp(float v,float lo,float hi){
    return v<lo?lo:v>hi?hi:v;
}
static float flerp(float a,float b,float t){
    return a+(b-a)*fclamp(t,0,1);
}
/* Random float [lo,hi] */
static float frand(float lo,float hi){
    return lo+(hi-lo)*(float)rand()/(float)RAND_MAX;
}

/* ================================================================
   RENDU TEXTE
================================================================ */
static SDL_Texture* makeText(SDL_Renderer* rnd,TTF_Font* f,
                              const char* t,SDL_Color c){
    SDL_Surface* s=TTF_RenderUTF8_Blended(f,t,c);
    if(!s)return NULL;
    SDL_Texture* tx=SDL_CreateTextureFromSurface(rnd,s);
    SDL_FreeSurface(s);
    return tx;
}
static void dtC(SDL_Renderer* rnd,TTF_Font* f,const char* t,
                SDL_Color c,int cx,int y){
    SDL_Texture* tx=makeText(rnd,f,t,c);
    if(!tx)return;
    int w,h;SDL_QueryTexture(tx,NULL,NULL,&w,&h);
    SDL_Rect dst={cx-w/2,y,w,h};
    SDL_RenderCopy(rnd,tx,NULL,&dst);SDL_DestroyTexture(tx);
}
static void dtL(SDL_Renderer* rnd,TTF_Font* f,const char* t,
                SDL_Color c,int x,int y){
    SDL_Texture* tx=makeText(rnd,f,t,c);
    if(!tx)return;
    int w,h;SDL_QueryTexture(tx,NULL,NULL,&w,&h);
    SDL_Rect dst={x,y,w,h};
    SDL_RenderCopy(rnd,tx,NULL,&dst);SDL_DestroyTexture(tx);
}

/* ================================================================
   DESSIN VOITURE avec texture + teinte couleur
================================================================ */
static void drawCarTex(SDL_Renderer* rnd,SDL_Texture* tex,
                       float x,float y,float angle,
                       Uint8 r,Uint8 g,Uint8 b){
    SDL_SetTextureColorMod(tex,r,g,b);
    SDL_SetTextureAlphaMod(tex,255);
    SDL_Rect dst={(int)x-CAR_W/2,(int)y-CAR_H/2,CAR_W,CAR_H};
    double deg=angle*180.0/M_PI+90.0;
    SDL_RenderCopyEx(rnd,tex,NULL,&dst,deg,NULL,SDL_FLIP_NONE);
    SDL_SetTextureColorMod(tex,255,255,255);
}

/* Convertit worldY en screenY - DOIT ETRE DEFINI AVANT drawRoadC1 */
static float w2s(float worldY, float worldOffset){
    return (float)(HEIGHT-120) + (worldY - worldOffset);
}

/* Phares (deux rectangles lumineux devant la voiture) */
static void drawHeadlights(SDL_Renderer* rnd,float x,float y,
                            float angle,LightMode lm,int isNight){
    if(!isNight||lm==LIGHT_OFF) return;
    float ca=cosf(angle),sa=sinf(angle);
    /* Offset vers l'avant de la voiture */
    float fx=x+ca*(-(float)CAR_H/2-4);
    float fy=y+sa*(-(float)CAR_H/2-4);
    /* Deux phares lateraux */
    float lx=fx+(-sa)*12, ly=fy+ca*12;
    float rx=fx+(-sa)*(-12), ry=fy+ca*(-12);

    if(lm==LIGHT_PLEIN){
        /* Cone de lumiere (triangle) */
        SDL_SetRenderDrawColor(rnd,255,255,180,30);
        for(int k=0;k<60;k++){
            float t=(float)k/60.0f;
            float cone=80.0f+t*120.0f;
            float spread=t*55.0f;
            for(float s=-spread;s<=spread;s+=2){
                int px=(int)(fx+ca*cone+(-sa)*s);
                int py=(int)(fy+sa*cone+ca*s);
                SDL_RenderDrawPoint(rnd,px,py);
            }
        }
        SDL_SetRenderDrawColor(rnd,255,255,200,255);
    }else{
        SDL_SetRenderDrawColor(rnd,255,220,100,200);
    }
    SDL_Rect hl={(int)lx-3,(int)ly-3,6,6};
    SDL_Rect hr={(int)rx-3,(int)ry-3,6,6};
    SDL_RenderFillRect(rnd,&hl);
    SDL_RenderFillRect(rnd,&hr);
}

/* Clignotants
   La voiture pointe dans la direction `angle`.
   angle = -PI/2 => pointe vers le HAUT de l'ecran.
   Vecteur "avant" de la voiture : (cos(angle+PI/2-PI/2), ...) 
   En SDL : l'avant du sprite correspond a l'angle angle.
   - Avant  : (sinf(-angle), -cosf(-angle)) = direction de deplacement
   - Gauche : perpendiculaire gauche = tourner de -90deg l'avant

   Pour angle=-PI/2 :
     avant  = (0, -1)  => vers le haut
     gauche = (-1, 0)  => vers la gauche de l'ecran (correct)
     droite = (+1, 0)  => vers la droite de l'ecran (correct)

   Vecteur avant ecran : forward_x = -sinf(angle), forward_y = cosf(angle)
     => pour angle=-PI/2: forward_x=sin(PI/2)=1, forward_y=cos(-PI/2)=0  FAUX

   METHODE CORRECTE :
   La texture du sprite a le capot en haut du rectangle, pointe angle.
   SDL_RenderCopyEx tourne le sprite de (angle*180/PI + 90) degres.
   Donc la direction REELLE du capot dans le monde :
     dir_x = sin(angle)  (apres la rotation +90 de SDL)
     dir_y = -cos(angle)
   Pour angle=-PI/2 : dir_x = sin(-PI/2) = -1  TOUJOURS FAUX

   APPROCHE SIMPLE : on sait que pour nos voitures, angle=-PI/2 toujours.
   Le capot est en HAUT (y diminue). Clignotant gauche = x diminue.
   On calcule les coins directement depuis x,y de la voiture.
*/
static void drawBlinkers(SDL_Renderer* rnd, float x, float y, float angle,
                          CligState clig, Uint32 tick){
    if(clig==CLIG_OFF) return;
    if((tick/400)%2 == 0) return;

    /* Vecteur avant de la voiture dans l'espace ecran.
       La rotation SDL est: degres = angle*180/PI + 90
       Donc l'axe du sprite (capot = haut du rectangle) apres rotation :
         avant_x = -sinf(angle + PI/2) = -cosf(angle)  [NON]

       Raisonnement direct :
       Sans rotation (deg=0), SDL oriente le rectangle tel quel.
       Le sprite est dessine capot en haut => apres rotation de deg=angle*180/PI+90 :
       Le capot (qui etait en haut, direction (0,-1)) est maintenant dans la
       direction rotee de deg. En radians : direction capot = -(deg)*PI/180
         = -(angle + PI/2) en radians.
       Donc :
         avant_x = cos(-(angle + PI/2)) = cos(-angle - PI/2) =  sin(angle) ... 

       On va simplement decomposer :
         - La voiture pointe physiquement dans la direction `angle` (convention physique)
         - Vecteur avant : (cosf(angle_physique), sinf(angle_physique))
           avec angle_physique = angle - PI/2 (correction du offset +90 de SDL)
       Pour angle = -PI/2 : angle_physique = -PI => avant = (cos(-PI), sin(-PI)) = (-1, 0) FAUX

       SOLUTION FINALE : utiliser directement la geometrie connue.
       angle = -PI/2 pour le joueur (monte). Avant = haut = (0,-1).
       Calcul : avant = (-sinf(angle), cosf(angle))
         angle=-PI/2 : avant = (-sin(-PI/2), cos(-PI/2)) = (1, 0) ... toujours faux.

       La vraie formule apres analyse de drawCarTex (rotation +90 deg) :
         avant_x = sinf(angle)
         avant_y = -cosf(angle)
       angle=-PI/2 : avant=(sin(-PI/2), -cos(-PI/2)) = (-1, 0)  VERS LA GAUCHE ?

       Testons : SDL_RenderCopyEx avec deg=angle*180/PI+90.
       angle=-PI/2 => deg = -90+90 = 0 => pas de rotation => sprite intact.
       Sprite intact = capot en haut = direction (0,-1). Donc avant=(0,-1).
       Formule : avant_x = -sinf(angle+PI/2)... 
         = -sinf(0) = 0 ✓ ; avant_y = -cosf(angle+PI/2) = -cos(0) = -1 ✓
       DONC : avant_x = -sinf(angle + PI/2f)
              avant_y = -cosf(angle + PI/2f)
       Simplifié : avant_x = cosf(angle), avant_y = sinf(angle)  [formule trig]
         angle=-PI/2 : avant_x=cos(-PI/2)=0, avant_y=sin(-PI/2)=-1 ✓ VERS LE HAUT

       Lateral gauche ecran (x diminue) perpendiculaire a l'avant :
         gauche_x = -avant_y =  sinf(angle)  (pour -PI/2 : = 1)  VERS LA DROITE ?
       Hmm... lateral gauche = rotation +90 de l'avant :
         gauche_x = -avant_y = -sinf(angle)  => pour -PI/2 : -(-1)= 1 (droite ecran)
       NON. Lateral GAUCHE de l'ecran = x decroit = gauche_x < 0.
       On veut : gauche_x = -avant_y = sinf(angle)
         angle=-PI/2 : sinf(-PI/2)=-1 => gauche = (-1, ...) ✓ vers gauche ecran
    */

    /* Vecteur avant (direction du capot) */
    float ax =  cosf(angle);   /* pour -PI/2: 0  */
    float ay =  sinf(angle);   /* pour -PI/2: -1 => haut ✓ */

    /* Vecteur lateral gauche de l'ecran (perpendiculaire, rotation -90 de avant) */
    /* gauche = (ay, -ax) => pour -PI/2: (-1, 0) => vers gauche ✓ */
    float lx_v =  ay;    /* composante x du vecteur "gauche ecran" */
    float ly_v = -ax;    /* composante y */

    /* Position de l'avant de la voiture */
    float frontX = x + ax * (float)(CAR_H/2 - 6);
    float frontY = y + ay * (float)(CAR_H/2 - 6);

    /* Offset lateral : CLIG_LEFT = gauche ecran, CLIG_RIGHT = droite ecran */
    float side = (clig == CLIG_LEFT) ? 1.0f : -1.0f;
    /* gauche ecran => lx_v negatif pour angle=-PI/2, donc *side=+1 => x += lx_v*offset < 0 ✓ */

    float bx = frontX + lx_v * side * (float)(CAR_W/2 - 4);
    float by = frontY + ly_v * side * (float)(CAR_W/2 - 4);

    SDL_SetRenderDrawColor(rnd, 255,160,0,255);
    SDL_Rect r={(int)bx-5,(int)by-5,10,10};
    SDL_RenderFillRect(rnd,&r);

    /* Clignotant arriere aussi (meme cote, a l'arriere) */
    float backX = x - ax * (float)(CAR_H/2 - 6);
    float backY = y - ay * (float)(CAR_H/2 - 6);
    float bx2 = backX + lx_v * side * (float)(CAR_W/2 - 4);
    float by2 = backY + ly_v * side * (float)(CAR_W/2 - 4);
    SDL_SetRenderDrawColor(rnd, 255,140,0,200);
    SDL_Rect r2={(int)bx2-4,(int)by2-4,8,8};
    SDL_RenderFillRect(rnd,&r2);
}

/* ================================================================
   EFFETS NUIT
================================================================ */
static void applyNight(SDL_Renderer* rnd,LightMode lm){
    Uint8 alpha;
    switch(lm){
    case LIGHT_OFF:       alpha=210; break;
    case LIGHT_VEILLEUSE: alpha=150; break;
    case LIGHT_PLEIN:     alpha=90;  break;
    default:              alpha=180; break;
    }
    SDL_SetRenderDrawColor(rnd,0,0,20,alpha);
    SDL_Rect full={0,0,WIDTH,HEIGHT};
    SDL_RenderFillRect(rnd,&full);
}

/* ================================================================
   DESSIN ARBRE  (tronc + feuillage circulaire)
================================================================ */
static void drawTree(SDL_Renderer* r, int tx, int ty,
                     int trunkH, int crownR, int night){
    int gr = night?12:48, gg = night?40:130, gb = night?12:50;
    /* Tronc */
    SDL_SetRenderDrawColor(r, 90, 60, 30, 255);
    SDL_Rect trunk = {tx-4, ty-trunkH, 8, trunkH};
    SDL_RenderFillRect(r, &trunk);
    /* Feuillage */
    SDL_SetRenderDrawColor(r, gr, gg, gb, 255);
    for(int dy = -crownR; dy <= crownR; dy++){
        int dx = (int)sqrtf((float)(crownR*crownR - dy*dy));
        SDL_RenderDrawLine(r, tx-dx, ty-trunkH+dy, tx+dx, ty-trunkH+dy);
    }
}

/* ================================================================
   DESSIN MAISON SIMPLE
================================================================ */
static void drawHouse(SDL_Renderer* r, int hx, int hy,
                      int w, int h,
                      Uint8 wr, Uint8 wg, Uint8 wb, int night){
    Uint8 br = night?wr/3:wr, bg = night?wg/3:wg, bb = night?wb/3:wb;
    /* Corps */
    SDL_SetRenderDrawColor(r, br, bg, bb, 255);
    SDL_Rect body = {hx, hy-h, w, h};
    SDL_RenderFillRect(r, &body);
    /* Toit */
    SDL_SetRenderDrawColor(r, night?50:120, night?25:55, night?25:45, 255);
    SDL_Point roof[4] = {{hx,hy-h},{hx+w,hy-h},{hx+w/2,hy-h-h*2/3},{hx,hy-h}};
    SDL_RenderDrawLines(r, roof, 4);
    /* Fenetre (orange la nuit = lumiere interieure) */
    if(night) SDL_SetRenderDrawColor(r, 255, 200, 80, 220);
    else      SDL_SetRenderDrawColor(r, 180, 215, 245, 255);
    SDL_Rect win = {hx+w/4, hy-h+h/4, w*2/5, h*2/5};
    SDL_RenderFillRect(r, &win);
}

/* ================================================================
   PANNEAU DE LIMITATION (cercle rouge + chiffre)
================================================================ */
static void drawSpeedSign(SDL_Renderer* r, TTF_Font* fS,
                           int px, int py, int limit){
    /* Fond blanc */
    SDL_SetRenderDrawColor(r, 255,255,255,255);
    for(int dy=-26;dy<=26;dy++){
        int dx=(int)sqrtf((float)(26*26-dy*dy));
        SDL_RenderDrawLine(r,px-dx,py+dy,px+dx,py+dy);
    }
    /* Bord rouge */
    SDL_SetRenderDrawColor(r,210,30,30,255);
    for(int dy=-26;dy<=26;dy++){
        int dxO=(int)sqrtf((float)(26*26-dy*dy));
        int dxI=(int)sqrtf((float)(22*22-dy*dy));
        SDL_RenderDrawLine(r,px-dxO,py+dy,px-dxI,py+dy);
        SDL_RenderDrawLine(r,px+dxI,py+dy,px+dxO,py+dy);
    }
    /* Chiffre */
    char buf[8]; snprintf(buf,sizeof(buf),"%d",limit);
    SDL_Color noir={15,15,15,255};
    dtC(r,fS,buf,noir,px,py-11);
    /* Poteau */
    SDL_SetRenderDrawColor(r,110,110,110,255);
    SDL_RenderDrawLine(r,px,py+26,px,py+60);
}

/* ================================================================
   ROUTE CHAP1  (version complète avec décor, zones, panneaux)
================================================================ */
static void drawRoadC1(SDL_Renderer* rnd, float worldOffset,
                        TTF_Font* fS, int isNight, int speedLimit){
    /* --- Fond herbe --- */
    if(isNight) SDL_SetRenderDrawColor(rnd, 6,18,6,255);
    else        SDL_SetRenderDrawColor(rnd, 36,108,36,255);
    SDL_RenderClear(rnd);

    /* -------------------------------------------------------
       DECOR LATERAL
       On parcourt le monde à reculons : worldOffset est la
       position du joueur. Plus worldOffset est grand, plus
       on est loin. Le décor est ancré dans le monde.

       Pour un objet en position monde worldYobj :
         screenY = (H-120) + (worldYobj - worldOffset)
       Si worldYobj < worldOffset => objet est DEVANT (en haut)
       Si worldYobj > worldOffset => objet est DERRIERE (en bas)

       On génère des objets à intervalles fixes dans le monde,
       en utilisant worldOffset pour calculer le bon offset écran.
    ------------------------------------------------------- */
    {
        int segH = 180;  /* espacement monde entre objets */
        /* Premier segment visible : le plus en haut de l'ecran
           screenY = 0 => worldYobj = worldOffset - (H-120)
           On commence un peu avant */
        float topWorldY = worldOffset - (float)(HEIGHT);
        int segStart = (int)(topWorldY / (float)segH) - 1;
        int segEnd   = (int)((worldOffset + HEIGHT) / (float)segH) + 1;

        for(int s = segStart; s <= segEnd; s++){
            float worldYobj = (float)(s * segH);
            int ty = (int)w2s(worldYobj, worldOffset);  /* Y ecran */

            if(ty < -100 || ty > HEIGHT+100) continue;

            /* Graine deterministe pour ce segment */
            unsigned int seed = (unsigned int)((s + 100000) * 2246822519u);
            int type = (int)((seed >> 28) & 0xF) % 6;
            int varA = (int)((seed >> 16) & 0xFF);
            int varB = (int)((seed >>  8) & 0xFF);

            /* Positions X fixes hors de la route */
            int lx = C1_ROAD_X - 70 - (varA % 30);
            int rx = C1_ROAD_X + C1_ROAD_W + 55 + (varB % 25);

            switch(type){
            case 0: case 1: case 3: {  /* Arbres */
                int th = 38 + varA%30;
                int cr = 22 + varB%18;
                drawTree(rnd, lx, ty, th, cr, isNight);
                drawTree(rnd, rx, ty, th+(varB%14), cr+(varA%10), isNight);
                break;
            }
            case 2: {  /* Maison */
                Uint8 mr=(Uint8)(155+varA%70),mg=(Uint8)(143+varB%50),mb=(Uint8)(125+varA%40);
                drawHouse(rnd, lx-40, ty, 74, 58, mr,mg,mb, isNight);
                drawHouse(rnd, rx, ty, 66, 52,
                          (Uint8)(165+varB%55),(Uint8)(150+varA%45),(Uint8)(135+varB%40),
                          isNight);
                break;
            }
            case 4: {  /* Poteau electrique */
                SDL_SetRenderDrawColor(rnd,isNight?70:115,isNight?70:115,isNight?70:100,255);
                SDL_RenderDrawLine(rnd, lx, ty, lx, ty-75);
                SDL_RenderDrawLine(rnd, lx-20, ty-60, lx+20, ty-60);
                SDL_RenderDrawLine(rnd, rx, ty, rx, ty-75);
                SDL_RenderDrawLine(rnd, rx-20, ty-60, rx+20, ty-60);
                SDL_SetRenderDrawColor(rnd,50,50,50,150);
                SDL_RenderDrawLine(rnd, lx, ty-60, rx, ty-60);
                drawTree(rnd, lx-55, ty, 30+varA%20, 17+varB%12, isNight);
                break;
            }
            case 5: {  /* Groupe d'arbres */
                drawTree(rnd, lx,    ty, 30+varA%22, 17+varB%14, isNight);
                drawTree(rnd, lx+45, ty, 24+varB%16, 14+varA%10, isNight);
                drawTree(rnd, rx,    ty, 30+varB%22, 17+varA%14, isNight);
                drawTree(rnd, rx-45, ty, 24+varA%16, 14+varB%10, isNight);
                /* Buisson bas */
                SDL_SetRenderDrawColor(rnd,isNight?10:52,isNight?32:118,isNight?10:38,255);
                SDL_Rect bush={rx+32, ty-8, 28,14};
                SDL_RenderFillRect(rnd,&bush);
                break;
            }
            }
        }
    }

    /* --- Asphalte --- */
    Uint8 aR=isNight?40:58, aB=isNight?46:64;
    SDL_SetRenderDrawColor(rnd, aR, aR, aB, 255);
    SDL_Rect road = {C1_ROAD_X, 0, C1_ROAD_W, HEIGHT};
    SDL_RenderFillRect(rnd, &road);

    /* Accotements */
    SDL_SetRenderDrawColor(rnd, isNight?55:88, isNight?55:86, isNight?50:78, 255);
    SDL_Rect acL={C1_ROAD_X, 0, 14, HEIGHT};
    SDL_Rect acR={C1_ROAD_X+C1_ROAD_W-14, 0, 14, HEIGHT};
    SDL_RenderFillRect(rnd, &acL);
    SDL_RenderFillRect(rnd, &acR);

    /* Bordures blanches */
    SDL_SetRenderDrawColor(rnd, 215,215,215,255);
    SDL_Rect bl={C1_ROAD_X+14,0,5,HEIGHT};
    SDL_Rect br={C1_ROAD_X+C1_ROAD_W-19,0,5,HEIGHT};
    SDL_RenderFillRect(rnd, &bl);
    SDL_RenderFillRect(rnd, &br);

    /* Ligne centrale jaune continue */
    SDL_SetRenderDrawColor(rnd, 238,198,0,255);
    SDL_Rect lc={C1_MID_X-3,0,6,HEIGHT};
    SDL_RenderFillRect(rnd, &lc);

    /* Pointillés blancs :
       Le joueur monte vers le haut => worldOffset augmente.
       Pour que les pointillés descendent (impression d'avancer),
       on soustrait : off = step - (worldOffset % step).
       Ainsi quand worldOffset augmente, off diminue => lignes descendent. */
    {
        int step=80, dash=42;
        /* Coordonnée monde du premier pointillé visible en haut */
        float topY  = worldOffset - (float)(HEIGHT-120);
        int   first = (int)(topY / step) - 1;
        int   last  = first + HEIGHT/step + 3;
        SDL_SetRenderDrawColor(rnd, 205,205,205,165);
        for(int k=first; k<=last; k++){
            float wY = (float)(k * step);
            int sy   = (int)w2s(wY, worldOffset);
            /* Pointillé sur voie droite */
            SDL_RenderDrawLine(rnd, C1_LANE_R_CX, sy, C1_LANE_R_CX, sy+dash);
            /* Pointillé sur voie gauche */
            SDL_RenderDrawLine(rnd, C1_LANE_L_CX, sy, C1_LANE_L_CX, sy+dash);
        }
    }

    /* Passage piéton (toutes les 900 unités monde) */
    {
        int cwStep = 900;
        float topY  = worldOffset - (float)(HEIGHT-120);
        int   first = (int)(topY / cwStep) - 1;
        int   last  = first + 3;
        SDL_SetRenderDrawColor(rnd,228,228,228,210);
        for(int k=first; k<=last; k++){
            float wY = (float)(k * cwStep + cwStep/2);
            int sy   = (int)w2s(wY, worldOffset);
            if(sy < -24 || sy > HEIGHT+24) continue;
            for(int stripe=0; stripe<6; stripe++){
                SDL_Rect s={C1_ROAD_X+stripe*100+4, sy, 90, 22};
                SDL_RenderFillRect(rnd, &s);
            }
        }
    }

    /* Panneau vitesse (fixe, bord gauche) */
    drawSpeedSign(rnd, fS, C1_ROAD_X-68, 110, speedLimit);

    /* Légende basse */
    SDL_SetRenderDrawColor(rnd, 0,0,0,198);
    SDL_Rect leg={0,HEIGHT-46,WIDTH,46};
    SDL_RenderFillRect(rnd, &leg);
    SDL_Color blanc={255,255,255,255};
    dtC(rnd, fS,
        "HAUT/BAS=vitesse  G/D=voie  C=cruise  N=nuit  V=pleins ph.  B=veilleuses  A/D=clignotants",
        blanc, WIDTH/2, HEIGHT-40);
}

/* ================================================================
   BANDEAU MESSAGE
================================================================ */
static void drawBandeau(SDL_Renderer* rnd,TTF_Font* f,
                         const char* msg,SDL_Color col){
    if(!msg||!msg[0])return;
    /* Calcul largeur du texte pour adapter la boite */
    SDL_Surface* s=TTF_RenderUTF8_Blended(f,msg,col);
    int tw=600,th=36;
    if(s){tw=s->w;th=s->h;SDL_FreeSurface(s);}
    int boxW=tw+40; if(boxW>WIDTH-20)boxW=WIDTH-20;
    SDL_SetRenderDrawColor(rnd,0,0,0,210);
    SDL_Rect box={WIDTH/2-boxW/2,5,boxW,th+16};
    SDL_RenderFillRect(rnd,&box);
    SDL_SetRenderDrawColor(rnd,col.r,col.g,col.b,160);
    SDL_RenderDrawRect(rnd,&box);
    dtC(rnd,f,msg,col,WIDTH/2,13);
}

/* ================================================================
   BOUTON RETOUR
================================================================ */
static void drawRetour(SDL_Renderer* rnd,TTF_Font* f){
    SDL_SetRenderDrawColor(rnd,30,30,30,225);
    SDL_Rect btn={14,14,114,38};SDL_RenderFillRect(rnd,&btn);
    SDL_SetRenderDrawColor(rnd,180,180,180,255);SDL_RenderDrawRect(rnd,&btn);
    SDL_Color b={255,255,255,255};
    dtC(rnd,f,"< Retour",b,71,22);
}

/* ================================================================
   HUD CHAP1
================================================================ */
static void drawHUDC1(SDL_Renderer* rnd,TTF_Font* fS,
                      float speed,int infract,int collision,
                      int cruise,float cruiseSpeed,
                      CligState clig,LightMode lm,int isNight,
                      float distance,Uint32 now,int speedLimit){
    /* Fond HUD */
    SDL_SetRenderDrawColor(rnd,0,0,0,220);
    SDL_Rect hud={WIDTH-255,10,242,isNight?230:200};
    SDL_RenderFillRect(rnd,&hud);
    SDL_SetRenderDrawColor(rnd,80,80,80,200);
    SDL_RenderDrawRect(rnd,&hud);

    SDL_Color BLANC={255,255,255,255};
    SDL_Color ROUGE={255,60,60,255};
    SDL_Color VERT={60,210,60,255};
    SDL_Color ORANG={255,150,0,255};
    SDL_Color JAUNE={255,220,0,255};
    SDL_Color GRIS={160,160,160,255};

    int kmh=(int)(speed*9.0f); if(kmh<0)kmh=0;
    char buf[80];
    float zoneSpeedF = (float)speedLimit / 9.0f;  /* unites internes */

    /* Vitesse */
    snprintf(buf,sizeof(buf),"Vitesse : %d km/h",kmh);
    SDL_Color vc=(kmh > speedLimit)?ROUGE:BLANC;
    dtL(rnd,fS,buf,vc,WIDTH-248,20);

    /* Barre vitesse */
    int bW=220;
    float ratio=fclamp(speed/SPEED_MAX,0,1);
    SDL_SetRenderDrawColor(rnd,50,50,50,255);
    SDL_Rect barBg={WIDTH-248,46,bW,12};SDL_RenderFillRect(rnd,&barBg);
    SDL_Color bc=(speed > zoneSpeedF)?ROUGE:(speed > zoneSpeedF*0.85f)?ORANG:VERT;
    SDL_SetRenderDrawColor(rnd,bc.r,bc.g,bc.b,255);
    SDL_Rect barFill={WIDTH-248,46,(int)(ratio*bW),12};
    SDL_RenderFillRect(rnd,&barFill);
    /* Repere limite : position proportionnelle a la limite de zone */
    int limX=WIDTH-248+(int)(zoneSpeedF/SPEED_MAX*bW);
    SDL_SetRenderDrawColor(rnd,255,60,60,255);
    SDL_RenderDrawLine(rnd,limX,42,limX,62);
    char limBuf[8]; snprintf(limBuf,sizeof(limBuf),"%d",speedLimit);
    dtL(rnd,fS,limBuf,ROUGE,limX-10,62);

    /* Cruise control */
    if(cruise){
        snprintf(buf,sizeof(buf),"CRUISE : %d km/h",(int)(cruiseSpeed*9.0f));
        dtL(rnd,fS,buf,JAUNE,WIDTH-248,78);
    }else{
        dtL(rnd,fS,"CRUISE : OFF",GRIS,WIDTH-248,78);
    }

    /* Infractions */
    snprintf(buf,sizeof(buf),"Infractions : %d",infract);
    dtL(rnd,fS,buf,(infract>0)?ROUGE:VERT,WIDTH-248,100);

    /* Collision */
    if(collision)dtL(rnd,fS,"!! COLLISION !!",ROUGE,WIDTH-248,122);

    /* Clignotants */
    if(clig==CLIG_LEFT){
        SDL_Color cy=(now/400)%2?JAUNE:GRIS;
        dtL(rnd,fS,"<< Clignotant G",cy,WIDTH-248,144);
    }else if(clig==CLIG_RIGHT){
        SDL_Color cy=(now/400)%2?JAUNE:GRIS;
        dtL(rnd,fS,"Clignotant D >>",cy,WIDTH-248,144);
    }

    /* Phares (nuit) */
    if(isNight){
        const char* lmStr=(lm==LIGHT_OFF)?"Phares : ETEINTS":
                          (lm==LIGHT_VEILLEUSE)?"Phares : Veilleuses":"Phares : Pleins feux";
        SDL_Color lc=(lm==LIGHT_OFF)?ROUGE:(lm==LIGHT_VEILLEUSE)?ORANG:JAUNE;
        dtL(rnd,fS,lmStr,lc,WIDTH-248,166);

        if(lm==LIGHT_OFF){
            dtL(rnd,fS,"! Allumez vos phares !",ROUGE,WIDTH-248,188);
        }
    }

    /* Distance (coin bas gauche) */
    snprintf(buf,sizeof(buf),"Distance : %.0f m",distance*5.0f);
    SDL_SetRenderDrawColor(rnd,0,0,0,185);
    SDL_Rect dr={14,HEIGHT-96,195,36};SDL_RenderFillRect(rnd,&dr);
    dtL(rnd,fS,buf,BLANC,22,HEIGHT-90);
}

/* ================================================================
   INIT IA CHAP1
================================================================ */
static void initAIsC1(AICarC1 ai[C1_NUM_AI], float worldOffset){
    static const Uint8 palR[]={220, 50, 50,190,140, 80,200, 40,170,200, 60,160};
    static const Uint8 palG[]={ 50,190, 50,190, 50,190,130,170, 70, 70,140, 80};
    static const Uint8 palB[]={ 50, 50,200, 50,200, 50, 50,190,190, 90,200,120};
    const int nPal=12;

    /* IA descendantes (voie droite) :
       Elles VIENNENT D'EN HAUT et descendent vers le bas.
       worldY < worldOffset => apparait EN HAUT de l'ecran. */
    for(int i=0; i<C1_NUM_DESC; i++){
        ai[i].goingUp  = 0;
        ai[i].x        = (float)C1_ROAD_X + 20.0f
                         + frand(10.0f, (float)(C1_ROAD_W/2 - CAR_W - 20));
        /* Espacees devant le joueur (worldY plus petit = plus haut sur l'ecran) */
        ai[i].worldY   = worldOffset - 150.0f - (float)i * frand(180.0f, 320.0f);
        ai[i].speed    = frand(1.5f, 2.8f);  /* vitesse propre : 13-25 km/h */
        ai[i].targetX  = ai[i].x;
        ai[i].avoidState = IA_NORMAL;
        ai[i].subLane  = 0;
        ai[i].cligIA   = CLIG_OFF;
        ai[i].cligIAEnd= 0;
        ai[i].honkReact= 0;
        int ci = (i * 3) % nPal;
        ai[i].colorR=palR[ci]; ai[i].colorG=palG[ci]; ai[i].colorB=palB[ci];
    }

    /* IA ascendantes (voie gauche, meme sens joueur) :
       Vitesse INFERIEURE au joueur -> il les rattrapera progressivement. */
    for(int i=0; i<C1_NUM_ASCE; i++){
        int idx = C1_NUM_DESC + i;
        ai[idx].goingUp   = 1;
        ai[idx].subLane   = i % 2;
        ai[idx].x         = (float)(ai[idx].subLane==0 ? C1_SUBLANE_A : C1_SUBLANE_B)
                            + frand(-15.0f, 15.0f);
        ai[idx].targetX   = ai[idx].x;
        /* Devant le joueur, bien espaces */
        ai[idx].worldY    = worldOffset - (float)(i+1) * frand(220.0f, 380.0f);
        ai[idx].speed     = frand(1.0f, 2.5f);  /* 9-22 km/h, LENTES */
        ai[idx].avoidState = IA_NORMAL;
        ai[idx].cligIA    = CLIG_OFF;
        ai[idx].cligIAEnd = 0;
        ai[idx].honkReact = 0;
        int ci = (i * 2 + 5) % nPal;
        ai[idx].colorR=palR[ci]; ai[idx].colorG=palG[ci]; ai[idx].colorB=palB[ci];
    }
}

/* ================================================================
   UPDATE IA CHAP1

   PRINCIPE CLÉ pour les IA descendantes :
   - Le joueur avance → worldOffset augmente de `playerSpeed * dt` par frame.
   - screenY = (H-120) + (worldY - worldOffset)
   - Pour qu'une IA DESCENDE à l'écran (screenY croisse), il faut que
     (worldY - worldOffset) croisse, donc worldY doit croître PLUS VITE
     que worldOffset.
   - Déplacement monde IA desc = (vitesse_propre + playerSpeed) * dt
     => screenSpeed = vitesse_propre  (indep de playerSpeed) ✓

   Pour les IA ascendantes (même sens) :
   - Elles montent à vitesse propre, le joueur les rattrape.
   - worldY -= vitesse_propre * dt  (inchangé)
================================================================ */
static void updateAIsC1(AICarC1 ai[C1_NUM_AI],
                         float worldOffset, float dt,
                         float playerX, float playerWorldY,
                         float playerSpeed, Uint32 now, int playerHonked){
    /* --------------------------------------------------------
       IA DESCENDANTES
    -------------------------------------------------------- */
    for(int i=0; i<C1_NUM_DESC; i++){
        float ownSpeed = ai[i].speed;

        /* Anti-collision entre IA descendantes */
        for(int j=0; j<C1_NUM_DESC; j++){
            if(i==j) continue;
            /* j est "devant" i (plus bas a l'ecran) si worldY[j] > worldY[i] */
            float gap = ai[j].worldY - ai[i].worldY;
            if(gap > 0 && gap < MIN_GAP_SAME && fabsf(ai[i].x-ai[j].x)<(float)CAR_W){
                ownSpeed *= (gap / MIN_GAP_SAME) * 0.8f;
            }
        }
        if(ownSpeed < 0) ownSpeed = 0;

        /* Deplacement monde :
           worldOffset diminue quand le joueur avance (nouveau sens).
           screenY_desc = (H-120) + (worldY - worldOffset).
           Pour que screenY augmente (descend ecran) :
             d(worldY)/dt > d(worldOffset)/dt = -playerSpeed
             => worldY doit croitre de (ownSpeed + playerSpeed).
           Vitesse propre ecran = ownSpeed (independant du joueur). */
        float ps = (playerSpeed > 0) ? playerSpeed : 0.0f;
        ai[i].worldY += (ownSpeed + ps) * dt;

        /* Quand sort en bas (screenY > H) => worldY > worldOffset + (H-120) */
        float scY = w2s(ai[i].worldY, worldOffset);
        if(scY > (float)(HEIGHT + CAR_H + 40)){
            /* Reapparait en haut (devant joueur = worldY < worldOffset) */
            float topY = worldOffset - frand(150.0f, 400.0f);
            for(int j=0; j<C1_NUM_DESC; j++){
                if(j==i) continue;
                if(fabsf(ai[j].worldY - topY) < MIN_GAP_SAME)
                    topY -= MIN_GAP_SAME;
            }
            ai[i].worldY = topY;
            ai[i].x      = (float)C1_ROAD_X + 20.0f
                          + frand(10.0f, (float)(C1_ROAD_W/2 - CAR_W - 20));
            ai[i].speed  = frand(1.5f, 2.8f);
        }
        /* Garde-fou : worldY completement deconnecte */
        if(ai[i].worldY > worldOffset + (float)(HEIGHT * 4)
        || ai[i].worldY < worldOffset - (float)(HEIGHT * 6)){
            ai[i].worldY = worldOffset - frand(100.0f, 500.0f);
            ai[i].x      = (float)C1_ROAD_X + 20.0f
                          + frand(10.0f, (float)(C1_ROAD_W/2 - CAR_W - 20));
            ai[i].speed  = frand(1.5f, 2.8f);
        }
    }

    /* --------------------------------------------------------
       IA ASCENDANTES (même sens joueur, mais plus lentes)
    -------------------------------------------------------- */
    for(int i=0; i<C1_NUM_ASCE; i++){
        int idx = C1_NUM_DESC + i;
        float targetSpeed = ai[idx].speed;

        /* Le joueur approche par derriere ? */
        float gapPlayerBehind = playerWorldY - ai[idx].worldY;
        /* >0 => IA est devant le joueur (normal), <0 => derriere (anormal) */
        int playerNearBehind = (gapPlayerBehind > 0
                                && gapPlayerBehind < AVOID_DIST
                                && fabsf(ai[idx].x - playerX) < (float)(CAR_W * 2));

        /* Reaction au klaxon : si le joueur klaxonne et qu'on est devant lui */
        if(playerHonked && gapPlayerBehind > 0 && gapPlayerBehind < AVOID_DIST * 1.5f
           && fabsf(ai[idx].x - playerX) < (float)(CAR_W * 3)){
            ai[idx].honkReact = 1;
        }

        if(playerNearBehind || ai[idx].honkReact){
            /* Se decaler lateralement vers la sous-voie opposee */
            if(ai[idx].avoidState == IA_NORMAL){
                float altX = (ai[idx].x < (float)C1_LANE_L_CX)
                             ? (float)C1_SUBLANE_B : (float)C1_SUBLANE_A;
                /* Choisir le cote vers lequel on va se decaler */
                int goRight = (altX > ai[idx].x);
                ai[idx].targetX    = altX;
                ai[idx].avoidState = goRight ? IA_AVOID_RIGHT : IA_AVOID_LEFT;
                /* Activer le clignotant du bon cote pendant 2.5s */
                ai[idx].cligIA    = goRight ? CLIG_RIGHT : CLIG_LEFT;
                ai[idx].cligIAEnd = now + 2500;
            }
            /* Accelerer un peu si tres proche */
            if(gapPlayerBehind < 80.0f && gapPlayerBehind > 0){
                targetSpeed = fclamp(targetSpeed + 0.4f, 0, SPEED_LIMIT - 0.3f);
            }
        } else {
            float homeX = (ai[idx].subLane==0) ? (float)C1_SUBLANE_A : (float)C1_SUBLANE_B;
            if(fabsf(ai[idx].x - homeX) < 10.0f){
                ai[idx].avoidState = IA_NORMAL;
                ai[idx].honkReact  = 0;
            }
            if(ai[idx].avoidState == IA_NORMAL) ai[idx].targetX = homeX;
        }

        /* Extinction automatique du clignotant IA */
        if(ai[idx].cligIA != CLIG_OFF && now > ai[idx].cligIAEnd)
            ai[idx].cligIA = CLIG_OFF;

        /* Anti-collision entre IA ascendantes */
        for(int j=0; j<C1_NUM_ASCE; j++){
            int jdx = C1_NUM_DESC + j;
            if(idx==jdx) continue;
            /* jdx devant idx si worldY[jdx] < worldY[idx] */
            float ahead = ai[idx].worldY - ai[jdx].worldY;
            if(ahead > 0 && ahead < MIN_GAP_SAME
               && fabsf(ai[idx].x - ai[jdx].x) < (float)(CAR_W + 8)){
                targetSpeed *= (ahead / MIN_GAP_SAME) * 0.85f;
                if(targetSpeed < 0.05f) targetSpeed = 0.0f;
            }
        }

        /* Deplacement lateral */
        float ldx = ai[idx].targetX - ai[idx].x;
        if(fabsf(ldx) > 1.0f) ai[idx].x += fclamp(ldx, -2.0f, 2.0f) * dt;

        /* Monte (worldY decroit) */
        ai[idx].worldY -= targetSpeed * dt;

        float scY = w2s(ai[idx].worldY, worldOffset);

        /* Trop loin devant : reapparait derriere le joueur */
        if(scY < -(float)(HEIGHT + CAR_H)){
            float backY = worldOffset + frand(MIN_GAP_SAME, MIN_GAP_SAME * 2.5f);
            ai[idx].worldY  = backY;
            ai[idx].x       = (float)(ai[idx].subLane==0 ? C1_SUBLANE_A : C1_SUBLANE_B)
                              + frand(-15.0f, 15.0f);
            ai[idx].targetX = ai[idx].x;
            ai[idx].avoidState = IA_NORMAL;
            ai[idx].speed   = frand(1.0f, 2.5f);
        }
        /* Trop loin derriere : reapparait devant */
        if(scY > (float)(HEIGHT + CAR_H)){
            float minY = worldOffset;
            for(int j=0; j<C1_NUM_ASCE; j++){
                int jdx = C1_NUM_DESC + j;
                if(jdx != idx && ai[jdx].worldY < minY) minY = ai[jdx].worldY;
            }
            ai[idx].worldY  = minY - frand(MIN_GAP_SAME, MIN_GAP_SAME * 2.0f);
            ai[idx].speed   = frand(1.0f, 2.5f);
        }
    }
}

/* ================================================================
   CHAP4 – ROUTE
================================================================ */
static void drawRoadC4(SDL_Renderer* rnd){
    SDL_SetRenderDrawColor(rnd,34,110,34,255);SDL_RenderClear(rnd);
    SDL_SetRenderDrawColor(rnd,60,60,60,255);
    SDL_Rect bv={CX-ROAD_HALF,0,2*ROAD_HALF,HEIGHT};
    SDL_Rect bh={0,CY-ROAD_HALF,WIDTH,2*ROAD_HALF};
    SDL_RenderFillRect(rnd,&bv);SDL_RenderFillRect(rnd,&bh);
    for(int dy=-R_OUTER;dy<=R_OUTER;dy++){
        float dxO=sqrtf((float)(R_OUTER*R_OUTER-dy*dy));
        float di2=(float)(R_INNER*R_INNER-dy*dy);
        if(di2<0){
            SDL_RenderDrawLine(rnd,CX-(int)dxO,CY+dy,CX+(int)dxO,CY+dy);
        }else{
            float dxI=sqrtf(di2);
            SDL_RenderDrawLine(rnd,CX-(int)dxO,CY+dy,CX-(int)dxI,CY+dy);
            SDL_RenderDrawLine(rnd,CX+(int)dxI,CY+dy,CX+(int)dxO,CY+dy);
        }
    }
    SDL_SetRenderDrawColor(rnd,0,140,0,255);
    for(int dy=-R_ISLAND;dy<=R_ISLAND;dy++){
        int dx=(int)sqrtf((float)(R_ISLAND*R_ISLAND-dy*dy));
        SDL_RenderDrawLine(rnd,CX-dx,CY+dy,CX+dx,CY+dy);
    }
    SDL_SetRenderDrawColor(rnd,255,200,0,255);
    for(int y=0;y<CY-R_OUTER;y+=30)SDL_RenderDrawLine(rnd,CX,y,CX,y+15);
    for(int y=CY+R_OUTER;y<HEIGHT;y+=30)SDL_RenderDrawLine(rnd,CX,y,CX,y+15);
    for(int x=0;x<CX-R_OUTER;x+=30)SDL_RenderDrawLine(rnd,x,CY,x+15,CY);
    for(int x=CX+R_OUTER;x<WIDTH;x+=30)SDL_RenderDrawLine(rnd,x,CY,x+15,CY);
}

/* ================================================================
   IA CHAP4
================================================================ */
static void resetAI4(AICarC4* ai,int idx){
    ai->speed=AI4_SPD;ai->state=AI4_APPROCHE;
    switch(idx){
    case 0:ai->x=(float)(CX-30);ai->y=-60;ai->angle=(float)M_PI/2;
           ai->entreeAngle=-(float)M_PI/2;ai->sortieAngle=0;break;
    case 1:ai->x=(float)(CX+30);ai->y=(float)(HEIGHT+60);ai->angle=-(float)M_PI/2;
           ai->entreeAngle=(float)M_PI/2;ai->sortieAngle=(float)M_PI;break;
    case 2:ai->x=-60;ai->y=(float)(CY-30);ai->angle=0;
           ai->entreeAngle=(float)M_PI;ai->sortieAngle=(float)M_PI/2;break;
    case 3:ai->x=(float)(WIDTH+60);ai->y=(float)(CY+30);ai->angle=(float)M_PI;
           ai->entreeAngle=0;ai->sortieAngle=-(float)M_PI/2;break;
    }
    ai->circAngle=ai->entreeAngle;
}
static void updateAI4(AICarC4* ai,int idx,AICarC4* all,int n){
    int occ=0;
    for(int j=0;j<n;j++){if(&all[j]==ai)continue;if(all[j].state==AI4_GIRATOIRE){occ=1;break;}}
    switch(ai->state){
    case AI4_APPROCHE:
        ai->x+=cosf(ai->angle)*ai->speed;ai->y+=sinf(ai->angle)*ai->speed;
        if(vdist(ai->x,ai->y,(float)CX,(float)CY)<R_OUTER+60){
            if(occ)ai->state=AI4_ATTENTE;
            else{ai->circAngle=atan2f(ai->y-CY,ai->x-CX);ai->state=AI4_GIRATOIRE;}
        }
        break;
    case AI4_ATTENTE:
        if(!occ){ai->circAngle=atan2f(ai->y-CY,ai->x-CX);ai->state=AI4_GIRATOIRE;}
        break;
    case AI4_GIRATOIRE:{
        ai->circAngle-=ai->speed/AI4_CIRC;
        float tx=(float)CX+AI4_CIRC*cosf(ai->circAngle);
        float ty=(float)CY+AI4_CIRC*sinf(ai->circAngle);
        ai->x+=(tx-ai->x)*0.35f;ai->y+=(ty-ai->y)*0.35f;
        ai->angle=ai->circAngle-(float)M_PI/2;
        if(fabsf(normAngle(ai->circAngle-ai->sortieAngle))<0.18f){
            ai->state=AI4_SORTIE;ai->angle=ai->sortieAngle;
        }
        break;}
    case AI4_SORTIE:
        ai->x+=cosf(ai->angle)*ai->speed;ai->y+=sinf(ai->angle)*ai->speed;
        if(ai->x<-150||ai->x>(float)(WIDTH+150)||ai->y<-150||ai->y>(float)(HEIGHT+150))
            resetAI4(ai,idx);
        break;
    }
}

/* ================================================================
   COLLISION ROUTE CHAP4
================================================================ */
static int surRoute4(float px,float py){
    float d=vdist(px,py,(float)CX,(float)CY);
    if(d>=R_INNER&&d<=R_OUTER)return 1;
    if(fabsf(px-CX)<=ROAD_HALF)return 1;
    if(fabsf(py-CY)<=ROAD_HALF)return 1;
    return 0;
}
static void collideRoute4(Car* car){
    float dx=car->x-(float)CX,dy=car->y-(float)CY;
    float d=vdist(car->x,car->y,(float)CX,(float)CY);
    if(!surRoute4(car->x,car->y)){
        float bD=9999,bX=car->x,bY=car->y;
        float bvX=fclamp(car->x,(float)(CX-ROAD_HALF+5),(float)(CX+ROAD_HALF-5));
        if(fabsf(car->x-bvX)<bD){bD=fabsf(car->x-bvX);bX=bvX;bY=car->y;}
        float bhY=fclamp(car->y,(float)(CY-ROAD_HALF+5),(float)(CY+ROAD_HALF-5));
        if(fabsf(car->y-bhY)<bD){bD=fabsf(car->y-bhY);bX=car->x;bY=bhY;}
        if(d>0){float a=atan2f(dy,dx);
            float cr=(d<R_INNER)?R_INNER+5:R_OUTER-5;
            float gx=(float)CX+cosf(a)*cr,gy=(float)CY+sinf(a)*cr;
            if(vdist(car->x,car->y,gx,gy)<bD){bX=gx;bY=gy;}}
        car->x=bX;car->y=bY;car->speed*=-0.3f;
    }
    if(d<R_ISLAND+(float)CAR_H/2){
        float a=atan2f(dy,dx);
        car->x=(float)CX+cosf(a)*(R_ISLAND+(float)CAR_H/2+2);
        car->y=(float)CY+sinf(a)*(R_ISLAND+(float)CAR_H/2+2);
        car->speed*=-0.3f;
    }
}

/* ================================================================
   ZONES DE VITESSE (global, espacees de longues distances)
================================================================ */
typedef struct { int distM; int limit; const char* name; } ZoneInfo;
#define N_ZONES_C1 13
static const ZoneInfo ZONES[N_ZONES_C1]={
    {     0,  50,"Zone urbaine"},
    {  2500,  70,"Sortie de ville"},
    {  6000,  90,"Route departementale"},
    { 12000, 110,"Route nationale"},
    { 20000, 130,"Voie rapide"},
    { 28000,  90,"Zone de ralentissement"},
    { 34000,  50,"Entree agglomeration"},
    { 40000,  70,"Zone periurbaine"},
    { 48000,  90,"Route secondaire"},
    { 56000, 110,"Nationale"},
    { 65000, 130,"Autoroute"},
    { 75000,  90,"Travaux"},
    { 82000,  50,"Ville"},
};

/* ================================================================
   MAIN
================================================================ */
int run_simulation(SDL_Window *_ext_win, SDL_Renderer *_ext_rnd, int *score_out){
    if(score_out) *score_out=0;
    srand((unsigned)time(NULL));

    /* Audio */
    int audioOK = (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) >= 0);
    if(!audioOK) fprintf(stderr,"Mix_OpenAudio: %s (sons desactives)\n",Mix_GetError());

    Mix_Music* sndMusique   = NULL;
    Mix_Chunk* sndCollision = NULL;
    Mix_Chunk* sndCligno    = NULL;
    Mix_Chunk* sndClaxon    = NULL;
    int  clignoPlaying = 0;

    if(audioOK){
        sndMusique   = Mix_LoadMUS("musique.ogg");
        sndCollision = Mix_LoadWAV("collision.wav");
        sndCligno    = Mix_LoadWAV("cligno.wav");
        sndClaxon    = Mix_LoadWAV("claxon.mpeg");
        if(!sndMusique)   fprintf(stderr,"musique.ogg: %s\n",Mix_GetError());
        if(!sndCollision) fprintf(stderr,"collision.wav: %s\n",Mix_GetError());
        if(!sndCligno)    fprintf(stderr,"cligno.wav: %s\n",Mix_GetError());
        if(!sndClaxon)    fprintf(stderr,"claxon.mpeg: %s\n",Mix_GetError());
    }

    /* Fenetre unique reçue du main */
    SDL_Window* win = _ext_win;
    SDL_Renderer* rnd = _ext_rnd;
    SDL_SetWindowTitle(win, "Auto-Ecole Simulation");
    SDL_SetRenderDrawBlendMode(rnd,SDL_BLENDMODE_BLEND);
    /* Résolution logique fixe — tout le rendu est calculé pour WIDTH x HEIGHT */
    SDL_RenderSetLogicalSize(rnd, WIDTH, HEIGHT);

    int isFullscreen = 0; /* toggle plein ecran */

    /* Polices */
    const char* fp[]={"DejaVuSans.ttf",
        "C:/msys64/mingw64/share/fonts/TTF/DejaVuSans.ttf",
        "/mingw64/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",NULL};
    TTF_Font *fontBig=NULL,*fontSmall=NULL;
    for(int i=0;fp[i]&&(!fontBig||!fontSmall);i++){
        if(!fontBig)  fontBig  =TTF_OpenFont(fp[i],28);
        if(!fontSmall)fontSmall=TTF_OpenFont(fp[i],16);
    }
    if(!fontBig||!fontSmall){
        fprintf(stderr,"Police manquante: %s\nPlacez DejaVuSans.ttf dans le meme dossier.\n",TTF_GetError());
        return 1;
    }

    /* Textures */
    SDL_Texture *texAccueil=NULL,*texMenu=NULL,*texJaune=NULL,*texBleu=NULL;
    {
        SDL_Surface* s;
        s=IMG_Load("accueil.png");if(s){texAccueil=SDL_CreateTextureFromSurface(rnd,s);SDL_FreeSurface(s);}
        s=IMG_Load("fond.png");   if(s){texMenu   =SDL_CreateTextureFromSurface(rnd,s);SDL_FreeSurface(s);}
        s=IMG_Load("jaune.png");  if(s){texJaune  =SDL_CreateTextureFromSurface(rnd,s);SDL_FreeSurface(s);}
        s=IMG_Load("bleu.png");   if(s){texBleu   =SDL_CreateTextureFromSurface(rnd,s);SDL_FreeSurface(s);}
    }
#define MKTEX(t,r,g,b) if(!(t)){SDL_Surface* s=SDL_CreateRGBSurface(0,CAR_W,CAR_H,24,0,0,0,0);\
    SDL_FillRect(s,NULL,SDL_MapRGB(s->format,r,g,b));(t)=SDL_CreateTextureFromSurface(rnd,s);SDL_FreeSurface(s);}
    MKTEX(texJaune,255,220,0)
    MKTEX(texBleu,60,100,220)
#undef MKTEX

    /* Chapitres */
    const char* chaps[5]={"Chapitre 1 : Circulation routiere",
        "Chapitre 2 : Les priorites","Chapitre 3 : Le depassement",
        "Chapitre 4 : Les ronds-points","Chapitre 5 : Les autoroutes"};
    int selection=0;
    Etat etat=ACCUEIL;

    /* ---- Etat joueur ---- */
    Car car={(float)C1_LANE_L_CX,(float)(HEIGHT-120),-(float)M_PI/2,0.0f};

    /* ---- Chap1 state ---- */
    float     worldOffset=0.0f;
    AICarC1   aiC1[C1_NUM_AI];
    initAIsC1(aiC1,worldOffset);

    int       c1Infract=0;
    int       c1Collision=0;
    Uint32    c1ColEnd=0;
    float     c1Dist=0.0f;

    /* Cruise control */
    int       cruiseOn=0;
    float     cruiseSpeed=0.0f;
    Uint32    cruiseFlash=0;

    /* Phares / nuit */
    int       isNight=0;
    LightMode lightMode=LIGHT_OFF;

    /* Clignotants */
    CligState cligState=CLIG_OFF;
    Uint32    cligTimer=0;   /* auto-extinction apres 5s */

    /* Detection mauvaise utilisation clignotant */
    Uint32    lastLaneChange=0;
    int       cligUsedBeforeChange=0;

    /* ---- Zones de vitesse ---- */
    int   c1ZoneIdx   = 0;
    int   c1SpeedLimit= 50;
    int   c1PrevZone  = -1;

    /* ---- Chap4 state ---- */
    AICarC4 aiC4[NUM_AI4];
    for(int i=0;i<NUM_AI4;i++)resetAI4(&aiC4[i],i);
    int c4Fautes=0;

    /* Messages */
    char   msgBuf[300]="";
    Uint32 msgEnd=0;

    SDL_Color BLANC={255,255,255,255};
    SDL_Color ROUGE={255,60,60,255};
    SDL_Color VERT={60,210,60,255};
    SDL_Color JAUNE={255,220,0,255};
    SDL_Color ORANG={255,150,0,255};

    int running=1;
    SDL_Event ev;
    Uint32 lastTick=SDL_GetTicks();

    while(running){
        Uint32 now=SDL_GetTicks();
        float dt=(now-lastTick)/16.0f;
        if(dt>4.0f)dt=4.0f;
        lastTick=now;

        /* ============================================================
           EVENTS
        ============================================================ */
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT)running=0;

            /* Plein ecran : F11 ou double-clic sur la barre de titre */
            if(ev.type==SDL_KEYDOWN && ev.key.keysym.sym==SDLK_F11){
                isFullscreen=!isFullscreen;
                SDL_SetWindowFullscreen(win,
                    isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            }
            /* Bouton maximiser Windows : intercepte l'evenement */
            if(ev.type==SDL_WINDOWEVENT){
                if(ev.window.event==SDL_WINDOWEVENT_MAXIMIZED){
                    isFullscreen=1;
                    SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                if(ev.window.event==SDL_WINDOWEVENT_RESTORED){
                    isFullscreen=0;
                    SDL_SetWindowFullscreen(win,0);
                }
            }

            if(ev.type==SDL_KEYDOWN){
                SDL_Keycode k=ev.key.keysym.sym;

                if(etat==ACCUEIL&&k==SDLK_RETURN)etat=MENU;
                else if(etat==MENU){
                    if(k==SDLK_UP)  selection=(selection-1+5)%5;
                    if(k==SDLK_DOWN)selection=(selection+1)%5;
                    if(k==SDLK_RETURN){
                        if(selection==0){
                            car.x=(float)C1_LANE_L_CX;
                            car.y=(float)(HEIGHT-120);
                            car.angle=-(float)M_PI/2;
                            car.speed=0;
                            worldOffset=0;c1Infract=0;c1Collision=0;
                            c1Dist=0;cruiseOn=0;
                            isNight=0;lightMode=LIGHT_OFF;
                            cligState=CLIG_OFF;
                            c1ZoneIdx=0;c1SpeedLimit=50;c1PrevZone=-1;
                            msgBuf[0]='\0';
                            initAIsC1(aiC1,worldOffset);
                            etat=CHAP1;
                            /* Demarrer la musique en boucle */
                            if(audioOK && sndMusique && !Mix_PlayingMusic())
                                Mix_PlayMusic(sndMusique, -1);
                        }
                        if(selection==3){
                            car.x=(float)(CX-30);car.y=(float)(HEIGHT+80);
                            car.angle=-(float)M_PI/2;car.speed=0;
                            for(int i=0;i<NUM_AI4;i++)resetAI4(&aiC4[i],i);
                            c4Fautes=0;msgBuf[0]='\0';etat=CHAP4;
                            if(audioOK && sndMusique && !Mix_PlayingMusic())
                                Mix_PlayMusic(sndMusique, -1);
                        }
                    }
                }

                /* Chap1 touches speciales */
                if(etat==CHAP1){
                    /* Cruise control */
                    if(k==SDLK_c){
                        if(!cruiseOn&&car.speed>0.5f){
                            cruiseOn=1;
                            cruiseSpeed=car.speed;
                            cruiseFlash=now+800;
                            snprintf(msgBuf,sizeof(msgBuf),
                                "Cruise control ACTIVE a %d km/h - Appuyez C ou freinez pour desactiver",
                                (int)(cruiseSpeed*9));
                            msgEnd=now+3000;
                        }else{
                            cruiseOn=0;
                            snprintf(msgBuf,sizeof(msgBuf),"Cruise control DESACTIVE");
                            msgEnd=now+1500;
                        }
                    }
                    /* Nuit */
                    if(k==SDLK_n){
                        isNight=!isNight;
                        if(isNight){
                            lightMode=LIGHT_OFF;
                            snprintf(msgBuf,sizeof(msgBuf),
                                "Mode nuit : Allumez vos phares ! (V=pleins feux, B=veilleuses)");
                            msgEnd=now+4000;
                        }else{
                            lightMode=LIGHT_OFF;
                            snprintf(msgBuf,sizeof(msgBuf),"Mode jour");
                            msgEnd=now+1500;
                        }
                    }
                    /* Phares */
                    if(k==SDLK_v&&isNight){
                        lightMode=(lightMode==LIGHT_PLEIN)?LIGHT_OFF:LIGHT_PLEIN;
                    }
                    if(k==SDLK_b&&isNight){
                        lightMode=(lightMode==LIGHT_VEILLEUSE)?LIGHT_OFF:LIGHT_VEILLEUSE;
                    }
                    /* Clignotants */
                    if(k==SDLK_a){
                        cligState=(cligState==CLIG_LEFT)?CLIG_OFF:CLIG_LEFT;
                        cligTimer=now+5000;
                        cligUsedBeforeChange=1;
                    }
                    if(k==SDLK_d&&k!=SDLK_RIGHT){
                        /* Note: on evite conflit avec flèche droite */
                    }
                    /* Klaxon */
                    if(k==SDLK_k){
                        if(sndClaxon && audioOK) Mix_PlayChannel(2, sndClaxon, 0);
                    }
                }

                if(k==SDLK_ESCAPE||k==SDLK_BACKSPACE){
                    if(etat==CHAP1||etat==CHAP4){
                        if(audioOK) Mix_HaltMusic();
                    }
                    running=0; /* Retour immédiat au menu principal */
                }
            }

            /* Touche D pour clignotant droit (via KEYDOWN avec scancode) */
            if(ev.type==SDL_KEYDOWN&&etat==CHAP1){
                if(ev.key.keysym.scancode==SDL_SCANCODE_D){
                    cligState=(cligState==CLIG_RIGHT)?CLIG_OFF:CLIG_RIGHT;
                    cligTimer=now+5000;
                    cligUsedBeforeChange=1;
                }
            }

            if(ev.type==SDL_MOUSEBUTTONDOWN){
                int mx=ev.button.x,my=ev.button.y;
                if(mx>=14&&mx<=128&&my>=14&&my<=52){
                    if(etat==CHAP1||etat==CHAP4){
                        etat=MENU;msgBuf[0]='\0';
                        if(audioOK) Mix_HaltMusic();
                    }
                    else if(etat==MENU)etat=ACCUEIL;
                    else if(etat==ACCUEIL)running=0; /* Retour au menu principal */
                }
            }
        }

        const Uint8* ks=SDL_GetKeyboardState(NULL);

        /* ============================================================
           ACCUEIL
        ============================================================ */
        if(etat==ACCUEIL){
            if(texAccueil)SDL_RenderCopy(rnd,texAccueil,NULL,NULL);
            else{SDL_SetRenderDrawColor(rnd,10,40,100,255);SDL_RenderClear(rnd);
                dtC(rnd,fontBig,"AUTO-ECOLE SIMULATION",BLANC,WIDTH/2,HEIGHT/3);}
            dtC(rnd,fontBig,"Appuyez sur ENTREE",JAUNE,WIDTH/2,HEIGHT-110);
        }

        /* ============================================================
           MENU
        ============================================================ */
        else if(etat==MENU){
            if(texMenu)SDL_RenderCopy(rnd,texMenu,NULL,NULL);
            else{SDL_SetRenderDrawColor(rnd,15,15,40,255);SDL_RenderClear(rnd);}
            dtC(rnd,fontBig,"Choisissez un chapitre",BLANC,WIDTH/2,55);
            for(int i=0;i<5;i++){
                SDL_Rect rect={WIDTH/2-380,130+i*100,760,80};
                int actif=(i==selection),dispo=(i==0||i==3);
                if(!dispo)SDL_SetRenderDrawColor(rnd,70,70,70,150);
                else if(actif)SDL_SetRenderDrawColor(rnd,200,90,0,230);
                else SDL_SetRenderDrawColor(rnd,0,70,150,190);
                SDL_RenderFillRect(rnd,&rect);
                SDL_SetRenderDrawColor(rnd,255,255,255,actif?220:60);
                SDL_RenderDrawRect(rnd,&rect);
                char buf2[128];SDL_Color tc=dispo?BLANC:(SDL_Color){110,110,110,255};
                if(!dispo)snprintf(buf2,sizeof(buf2),"%s  [bientot]",chaps[i]);
                dtC(rnd,fontSmall,dispo?chaps[i]:buf2,tc,rect.x+rect.w/2,rect.y+rect.h/2-10);
            }
            dtC(rnd,fontSmall,"Fleches + ENTREE  |  ECHAP = retour",
                (SDL_Color){180,180,180,255},WIDTH/2,HEIGHT-35);
            drawRetour(rnd,fontSmall);
        }

        /* ============================================================
           CHAP1 – Circulation routiere
        ============================================================ */
        else if(etat==CHAP1){

            /* ------ Zones de vitesse ------ */
            {
                int distM = (int)(c1Dist * 5.0f);
                for(int zi = N_ZONES_C1-1; zi >= 0; zi--){
                    if(distM >= ZONES[zi].distM){
                        c1ZoneIdx   = zi;
                        c1SpeedLimit= ZONES[zi].limit;
                        break;
                    }
                }
                if(c1ZoneIdx != c1PrevZone){
                    c1PrevZone = c1ZoneIdx;
                    if(now >= msgEnd){
                        snprintf(msgBuf,sizeof(msgBuf),
                            "%s – Limite : %d km/h",
                            ZONES[c1ZoneIdx].name, c1SpeedLimit);
                        msgEnd = now + 4000;
                    }
                }
            }
            float zoneLimit = (float)c1SpeedLimit / 9.0f; /* unites internes */

            /* ------ Physique joueur ------ */

            /* Freinage desactive le cruise */
            if(ks[SDL_SCANCODE_DOWN]&&cruiseOn){
                cruiseOn=0;
                snprintf(msgBuf,sizeof(msgBuf),"Cruise control desactive (freinage)");
                msgEnd=now+2000;
            }

            if(cruiseOn){
                /* Maintien de la vitesse de croisiere */
                float err=cruiseSpeed-car.speed;
                car.speed+=err*0.04f*dt;
            }else{
                if(ks[SDL_SCANCODE_UP])   car.speed+=0.08f*dt;
                if(ks[SDL_SCANCODE_DOWN]) car.speed-=0.09f*dt;
                if(!ks[SDL_SCANCODE_UP]&&!ks[SDL_SCANCODE_DOWN]){
                    float fric=0.022f*dt;
                    if(car.speed>0){car.speed-=fric;if(car.speed<0)car.speed=0;}
                    else if(car.speed<0){car.speed+=fric;if(car.speed>0)car.speed=0;}
                }
            }

            if(car.speed>SPEED_MAX)car.speed=SPEED_MAX;
            if(car.speed<-SPEED_REVERSE)car.speed=-SPEED_REVERSE;

            /* Bride progressive selon la zone courante */
            if(car.speed > zoneLimit){
                car.speed -= 0.016f*dt*(car.speed - zoneLimit + 1.0f);
                if(cruiseOn && cruiseSpeed > zoneLimit)
                    cruiseSpeed = zoneLimit;
            }

            /* Deplacement lateral */
            float prevX=car.x;
            float lat=3.2f*dt;
            if(ks[SDL_SCANCODE_LEFT]) car.x-=lat;
            if(ks[SDL_SCANCODE_RIGHT])car.x+=lat;

            /* Detection changement de voie */
            int changedLane=0;
            int wasOnLeft=(prevX>=(float)C1_MID_X);
            int nowOnLeft=(car.x>=(float)C1_MID_X);
            if(wasOnLeft!=nowOnLeft){
                changedLane=1;
                /* Verifier si clignotant etait allume */
                if(!cligUsedBeforeChange&&now>=msgEnd){
                    snprintf(msgBuf,sizeof(msgBuf),
                        "Oubli de clignotant ! Signalez toujours un changement de voie.");
                    msgEnd=now+3000;c1Infract++;
                }
                /* Verifier sens du clignotant */
                if(cligUsedBeforeChange){
                    int goingLeft=(car.x<prevX);
                    if(goingLeft&&cligState==CLIG_RIGHT&&now>=msgEnd){
                        snprintf(msgBuf,sizeof(msgBuf),
                            "Clignotant du mauvais cote ! Gauche pour aller a gauche.");
                        msgEnd=now+3000;c1Infract++;
                    }else if(!goingLeft&&cligState==CLIG_LEFT&&now>=msgEnd){
                        snprintf(msgBuf,sizeof(msgBuf),
                            "Clignotant du mauvais cote ! Droit pour aller a droite.");
                        msgEnd=now+3000;c1Infract++;
                    }
                }
                cligUsedBeforeChange=0;
                /* Eteindre clignotant apres changement effectue */
                cligState=CLIG_OFF;
                lastLaneChange=now;
            }

            /* Angle visuel */
            if(ks[SDL_SCANCODE_LEFT])      car.angle=-(float)M_PI/2-0.12f;
            else if(ks[SDL_SCANCODE_RIGHT]) car.angle=-(float)M_PI/2+0.12f;
            else                            car.angle=-(float)M_PI/2;

            /* Scroll monde :
               Le joueur avance vers le HAUT de l'ecran.
               worldOffset represente la position monde du joueur.
               Quand on avance, worldOffset DIMINUE (on remonte dans le monde).
               => Les objets de worldY fixe ont screenY = (H-120)+(worldY-worldOffset)
               qui AUGMENTE quand worldOffset diminue => ils descendent ✓ */
            worldOffset -= car.speed * dt;
            if(car.speed > 0) c1Dist += car.speed * dt;

            /* Auto-extinction clignotant apres 5s */
            if(cligState!=CLIG_OFF&&now>cligTimer){
                cligState=CLIG_OFF;
                if(now>=msgEnd){
                    snprintf(msgBuf,sizeof(msgBuf),"Clignotant eteint automatiquement.");
                    msgEnd=now+1500;
                }
            }

            /* Bornes route */
            int hR=0;
            if(car.x<C1_ROAD_X+(float)CAR_W/2+4){
                car.x=(float)(C1_ROAD_X+CAR_W/2+4);car.speed*=0.3f;hR=1;}
            if(car.x>C1_ROAD_X+C1_ROAD_W-(float)CAR_W/2-4){
                car.x=(float)(C1_ROAD_X+C1_ROAD_W-CAR_W/2-4);car.speed*=0.3f;hR=1;}

            /* Voie incorrecte (voie droite = zone de descente) */
            int surVoieOpposee=(car.x<(float)C1_MID_X-5);

            /* Klaxon : detecte si K est appuye ce frame */
            int playerHonked = ks[SDL_SCANCODE_K];

            /* Update IA */
            updateAIsC1(aiC1,worldOffset,dt,car.x,worldOffset,car.speed,now,playerHonked);

            /* Collision joueur / IA */
            int wasColliding = c1Collision;
            c1Collision=(now<c1ColEnd)?1:0;
            for(int i=0;i<C1_NUM_AI;i++){
                float scY=w2s(aiC1[i].worldY,worldOffset);
                if(scY<-(float)CAR_H*2||scY>(float)(HEIGHT+CAR_H*2))continue;
                float ddx=car.x-aiC1[i].x;
                float ddy=(float)(HEIGHT-120)-scY;
                if(fabsf(ddx)<(float)(CAR_W-6)&&fabsf(ddy)<(float)(CAR_H-6)){
                    c1Collision=1;c1ColEnd=now+2500;
                    car.speed*=-0.5f;
                    car.x+=(ddx>0?12.0f:-12.0f);
                    /* Son collision (joue une seule fois au debut) */
                    if(!wasColliding && sndCollision)
                        Mix_PlayChannel(-1, sndCollision, 0);
                }
            }

            /* ------ AUDIO ------ */
            /* Son collision : une seule fois au moment de l'impact */
            if(!wasColliding && c1Collision && sndCollision && audioOK)
                Mix_PlayChannel(-1, sndCollision, 0);

            /* Clignotant : joue le son au moment ou le flash s'ALLUME
               flash passe de 0 a 1 => clignoPlaying==0 => on joue le son
               On ne rejoue pas tant que flash reste a 1 (clignoPlaying==1) */
            if(sndCligno && audioOK && cligState != CLIG_OFF){
                int flash = (now / 400) % 2;
                if(flash == 1 && clignoPlaying == 0){
                    Mix_PlayChannel(1, sndCligno, 0);
                    clignoPlaying = 1;
                } else if(flash == 0){
                    clignoPlaying = 0;  /* reset : pret pour le prochain flash */
                }
            } else {
                clignoPlaying = 0;
            }

            /* Messages */
            if(hR&&now>=msgEnd){
                snprintf(msgBuf,sizeof(msgBuf),"Sortie de route ! Restez sur la chaussee.");
                msgEnd=now+2000;c1Infract++;
            }else if(surVoieOpposee&&car.speed>0.3f&&now>=msgEnd){
                snprintf(msgBuf,sizeof(msgBuf),
                    "DANGER ! Vous etes sur la voie opposee (sens contraire) !");
                msgEnd=now+1500;c1Infract++;
            }else if(c1Collision&&now<c1ColEnd&&now>=msgEnd){
                snprintf(msgBuf,sizeof(msgBuf),
                    "COLLISION ! Respectez les distances de securite.");
                msgEnd=now+2500;c1Infract++;
            }else if(car.speed>zoneLimit+0.4f&&now>=msgEnd){
                snprintf(msgBuf,sizeof(msgBuf),
                    "Vitesse %d km/h depasse limite %d km/h ! Ralentissement auto.",
                    (int)(car.speed*9), c1SpeedLimit);
                msgEnd=now+1200;
            }else if(isNight&&lightMode==LIGHT_OFF&&car.speed>0.2f&&now>=msgEnd){
                snprintf(msgBuf,sizeof(msgBuf),
                    "Conduite de nuit sans eclairage ! Allumez vos phares (V ou B).");
                msgEnd=now+2500;c1Infract++;
            }

            /* ------ RENDU ------ */
            drawRoadC1(rnd,worldOffset,fontSmall,isNight,c1SpeedLimit);

            /* IA */
            for(int i=0;i<C1_NUM_AI;i++){
                float scY=w2s(aiC1[i].worldY,worldOffset);
                if(scY<-(float)CAR_H||scY>(float)(HEIGHT+CAR_H))continue;
                float ang=aiC1[i].goingUp?-(float)M_PI/2:(float)M_PI/2;
                drawCarTex(rnd,texBleu,aiC1[i].x,scY,ang,
                           aiC1[i].colorR,aiC1[i].colorG,aiC1[i].colorB);
                /* Clignotants IA */
                if(aiC1[i].cligIA != CLIG_OFF)
                    drawBlinkers(rnd,aiC1[i].x,scY,ang,aiC1[i].cligIA,now);
            }

            /* Phares IA si nuit */
            if(isNight){
                for(int i=0;i<C1_NUM_AI;i++){
                    float scY=w2s(aiC1[i].worldY,worldOffset);
                    if(scY<-(float)CAR_H||scY>(float)(HEIGHT+CAR_H))continue;
                    float ang=aiC1[i].goingUp?-(float)M_PI/2:(float)M_PI/2;
                    drawHeadlights(rnd,aiC1[i].x,scY,ang,LIGHT_PLEIN,1);
                }
            }

            /* Joueur */
            drawCarTex(rnd,texJaune,car.x,(float)(HEIGHT-120),car.angle,255,220,0);
            drawHeadlights(rnd,car.x,(float)(HEIGHT-120),car.angle,lightMode,isNight);
            drawBlinkers(rnd,car.x,(float)(HEIGHT-120),car.angle,cligState,now);

            /* Overlay nuit */
            if(isNight)applyNight(rnd,lightMode);

            /* HUD */
            drawHUDC1(rnd,fontSmall,car.speed,c1Infract,c1Collision,
                      cruiseOn,cruiseSpeed,cligState,lightMode,isNight,c1Dist,now,c1SpeedLimit);
            /* Afficher la limite de zone dans le HUD */
            {
                char zlbuf[40];
                snprintf(zlbuf,sizeof(zlbuf),"Zone : %d km/h",c1SpeedLimit);
                SDL_Color zc={255,220,0,255};
                SDL_SetRenderDrawColor(rnd,0,0,0,185);
                SDL_Rect zr={WIDTH-255,HEIGHT-85,242,36};SDL_RenderFillRect(rnd,&zr);
                dtL(rnd,fontSmall,zlbuf,zc,WIDTH-248,HEIGHT-80);
                dtL(rnd,fontSmall,ZONES[c1ZoneIdx].name,
                    (SDL_Color){180,180,180,255},WIDTH-248,HEIGHT-58);
            }

            /* Indicateur voie */
            {
                int onGood=(car.x>=(float)C1_MID_X);
                SDL_Color vc2=onGood?VERT:ROUGE;
                const char* voieStr=onGood?"Voie : CORRECTE (gauche)":"Voie : OPPOSEE (danger !)";
                SDL_SetRenderDrawColor(rnd,0,0,0,185);
                SDL_Rect vr={14,HEIGHT-136,220,36};SDL_RenderFillRect(rnd,&vr);
                dtL(rnd,fontSmall,voieStr,vc2,22,HEIGHT-130);

                char dbuf[60];
                snprintf(dbuf,sizeof(dbuf),"Distance : %.0f m",c1Dist*5.0f);
                SDL_SetRenderDrawColor(rnd,0,0,0,185);
                SDL_Rect dr={14,HEIGHT-96,195,36};SDL_RenderFillRect(rnd,&dr);
                dtL(rnd,fontSmall,dbuf,BLANC,22,HEIGHT-90);
            }

            /* Bandeau */
            if(now<msgEnd){
                int bon=(strstr(msgBuf,"ACTIVE")||strstr(msgBuf,"Bien"));
                drawBandeau(rnd,fontSmall,msgBuf,bon?VERT:ROUGE);
            }else if(car.speed<0.1f){
                drawBandeau(rnd,fontSmall,"Appuyez sur HAUT pour demarrer",JAUNE);
            }else if(car.x>=(float)C1_MID_X&&car.speed>0.3f){
                drawBandeau(rnd,fontSmall,
                    "Bien ! Gardez votre voie et respectez les 50 km/h",VERT);
            }

            drawRetour(rnd,fontSmall);
        }

        /* ============================================================
           CHAP4 – Rond-point
        ============================================================ */
        else if(etat==CHAP4){
            drawRoadC4(rnd);
            for(int i=0;i<NUM_AI4;i++)updateAI4(&aiC4[i],i,aiC4,NUM_AI4);

            if(ks[SDL_SCANCODE_UP])   car.speed+=0.09f*dt;
            if(ks[SDL_SCANCODE_DOWN]) car.speed-=0.09f*dt;
            if(!ks[SDL_SCANCODE_UP]&&!ks[SDL_SCANCODE_DOWN])car.speed*=0.97f;
            car.speed=fclamp(car.speed,-2.5f,6.0f);
            float steer=0.033f*dt*(car.speed>0?1:-1);
            if(ks[SDL_SCANCODE_LEFT]) car.angle-=steer;
            if(ks[SDL_SCANCODE_RIGHT])car.angle+=steer;
            car.x+=cosf(car.angle)*car.speed*dt;
            car.y+=sinf(car.angle)*car.speed*dt;

            int wH=!surRoute4(car.x,car.y);
            collideRoute4(&car);
            if(wH&&!surRoute4(car.x,car.y)&&now>=msgEnd){
                snprintf(msgBuf,sizeof(msgBuf),"Hors de la route !");
                msgEnd=now+2000;c4Fautes++;
            }
            {float dj=vdist(car.x,car.y,(float)CX,(float)CY);
             int oc=0;for(int i=0;i<NUM_AI4;i++)if(aiC4[i].state==AI4_GIRATOIRE){oc=1;break;}
             if(dj<R_OUTER+40&&dj>R_INNER&&oc&&now>=msgEnd){
                 snprintf(msgBuf,sizeof(msgBuf),"Cedez le passage aux vehicules engages !");
                 msgEnd=now+2000;
             }
            }

            SDL_Texture* tex4[NUM_AI4]={texBleu,texBleu,texJaune,texBleu};
            Uint8 cr4[NUM_AI4][3]={{220,50,50},{50,100,220},{50,180,50},{220,160,40}};
            for(int i=0;i<NUM_AI4;i++)
                drawCarTex(rnd,tex4[i],aiC4[i].x,aiC4[i].y,aiC4[i].angle,
                           cr4[i][0],cr4[i][1],cr4[i][2]);
            drawCarTex(rnd,texJaune,car.x,car.y,car.angle,255,220,0);

            SDL_SetRenderDrawColor(rnd,0,0,0,185);
            SDL_Rect leg={0,HEIGHT-82,WIDTH,82};SDL_RenderFillRect(rnd,&leg);
            dtC(rnd,fontSmall,
                "HAUT/BAS = vitesse  |  GAUCHE/DROITE = direction  |  Circulez en sens anti-horaire",
                BLANC,WIDTH/2,HEIGHT-76);
            dtC(rnd,fontSmall,
                "Vehicule JAUNE = vous  |  Cedez le passage a ceux deja engages dans le giratoire",
                JAUNE,WIDTH/2,HEIGHT-52);
            char fb[60];snprintf(fb,sizeof(fb),"Infractions : %d",c4Fautes);
            dtC(rnd,fontSmall,fb,c4Fautes>0?ROUGE:VERT,WIDTH/2,HEIGHT-26);

            if(now<msgEnd)drawBandeau(rnd,fontBig,msgBuf,
                (strstr(msgBuf,"Bien")||strstr(msgBuf,"Circulez"))?VERT:ROUGE);

            drawRetour(rnd,fontSmall);
        }

        SDL_RenderPresent(rnd);
        SDL_Delay(FPS_DELAY);
    }

    TTF_CloseFont(fontBig);TTF_CloseFont(fontSmall);
    SDL_DestroyTexture(texJaune);SDL_DestroyTexture(texBleu);
    if(texAccueil)SDL_DestroyTexture(texAccueil);
    if(texMenu)SDL_DestroyTexture(texMenu);
    /* Fenetre appartient au main */
    /* Audio cleanup */
    if(sndMusique)   Mix_FreeMusic(sndMusique);
    if(sndCollision) Mix_FreeChunk(sndCollision);
    if(sndCligno)    Mix_FreeChunk(sndCligno);
    if(sndClaxon)    Mix_FreeChunk(sndClaxon);
    if(audioOK)      Mix_CloseAudio();
    /* Score pratique: 100 - (infractions*10), min 0 */
    if(score_out){
        int total_infract = c1Infract + c4Fautes;
        int sc = 100 - total_infract * 10;
        if(sc < 0) sc = 0;
        *score_out = sc;
    }
    /* Remettre LogicalSize à 0 — le main remet ensuite son propre état */
    SDL_RenderSetLogicalSize(rnd, 0, 0);
    /* TTF_Quit/IMG_Quit/Mix_Quit gérés par main() — ne pas appeler ici */
    /* Réinitialisation pour permettre un 2e appel propre */
    win=NULL; rnd=NULL;
    return 0;
}
