#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "video.h"

int SCREEN_W    = 1280;
int SCREEN_H    = 720;
int PLEIN_ECRAN = 0;

#define LOADING_DURATION 25000
#define MAX_USERS        50
#define MAX_STR          64
#define MAX_INPUT        63
#define MAX_PATH_PHOTO   512
#define FICHIER_USERS    "users.dat"
#define NB_AVATARS       6
#define NB_ASTUCES       10

typedef enum {
    ETAT_VIDEO,
    ETAT_CHARGEMENT,
    ETAT_AUTH,
    ETAT_MENU,
    ETAT_PARAMETRES   // ← nouveau
} EtatApp;

const char* ASTUCES[NB_ASTUCES] = {
    "Respectez toujours la priorite a droite !",
    "La prudence est la meilleure des assurances.",
    "Verifiez vos angles morts avant chaque manoeuvre.",
    "Gardez toujours vos distances de securite.",
    "Ne doublez jamais sur une ligne continue.",
    "Par temps de pluie, doublez vos distances de freinage.",
    "Le telephone au volant multiplie le risque par 4.",
    "La ceinture de securite est obligatoire pour tous.",
    "Respectez les limitations : elles sauvent des vies.",
    "Un conducteur repose conduit mieux et plus prudemment."
};

// ════════════════════════════════════════════════════════════════
//  STRUCTURES
// ════════════════════════════════════════════════════════════════
typedef struct {
    char username[MAX_STR];
    char password[MAX_STR];
    char nom_complet[MAX_STR];
    char role[MAX_STR];
    int  nb_connexions;
    char derniere_connexion[MAX_STR];
    int  score_theo;
    int  score_prat;
    int  tentatives_echouees;
    int  avatar_id;
    char photo_path[MAX_PATH_PHOTO];
    int  a_photo;
} Utilisateur;

typedef struct {
    Utilisateur users[MAX_USERS];
    int         nb_users;
    int         user_connecte;
} BaseAuth;

typedef struct {
    char     texte[MAX_INPUT+1];
    int      longueur, actif, est_mdp;
    int      mdp_visible;   // 1 = montrer mdp
    SDL_Rect rect;
} ChampTexte;

// ── Paramètres modifiables ────────────────────────────────────
typedef struct {
    char nouveau_nom[MAX_STR];
    char nouveau_mdp[MAX_STR];
    char confirm_mdp[MAX_STR];
    ChampTexte ch_nom;
    ChampTexte ch_mdp;
    ChampTexte ch_confirm;
    int  page;          // 0 = infos, 1 = modifier
    char message[256];
    int  msg_ok;
} EcranParametres;

typedef struct {
    ChampTexte champ_user, champ_pass;
    ChampTexte reg_nom, reg_user, reg_pass;
    int        mode;        // 0=co 1=ins 2=photo 3=retour depuis photo
    int        avatar_selec;
    char       message[256];
    int        msg_ok;
    int        anime;
    BaseAuth*  base;
    char       tmp_nom[MAX_STR];
    char       tmp_user[MAX_STR];
    char       tmp_pass[MAX_STR];
    char       tmp_photo[MAX_PATH_PHOTO];
    int        tmp_a_photo;
    SDL_Texture* preview_tex;
} EcranAuth;

typedef struct {
    SDL_Rect    rect;
    const char* titre;
    const char* sous_titre;
    int         survol;
} Bouton;

SDL_Texture* g_photo_tex  = NULL;
int          g_photo_user = -1;

// ════════════════════════════════════════════════════════════════
//  AUTH — fonctions base
// ════════════════════════════════════════════════════════════════
void hasher_mdp(const char* mdp, char* out) {
    int len = strlen(mdp);
    for (int i = 0; i < len; i++) out[i] = (char)((mdp[i] ^ 0x5A) + 3);
    out[len] = '\0';
}
int verifier_mdp(const char* s, const char* h) {
    char tmp[MAX_STR]; hasher_mdp(s, tmp); return strcmp(tmp, h) == 0;
}
void get_date_str(char* out) {
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    strftime(out, MAX_STR, "%d/%m/%Y a %Hh%M", tm);
}
void auth_sauvegarder(BaseAuth* b) {
    FILE* f = fopen(FICHIER_USERS, "wb");
    if (!f) return;
    fwrite(b, sizeof(BaseAuth), 1, f); fclose(f);
}
void creer_base_defaut(BaseAuth* b) {
    memset(b, 0, sizeof(BaseAuth));
    b->nb_users = 1; b->user_connecte = -1;
    strcpy(b->users[0].username,           "admin");
    hasher_mdp("admin123",                  b->users[0].password);
    strcpy(b->users[0].nom_complet,        "Administrateur");
    strcpy(b->users[0].role,               "admin");
    strcpy(b->users[0].derniere_connexion, "Jamais");
    b->users[0].avatar_id = 0; b->users[0].a_photo = 0;
    auth_sauvegarder(b);
    printf("[INFO] Base creee — admin / admin123\n");
}
void auth_charger(BaseAuth* b) {
    FILE* f = fopen(FICHIER_USERS, "rb");
    if (!f) { creer_base_defaut(b); return; }
    fseek(f, 0, SEEK_END); long taille = ftell(f); fclose(f);
    if (taille != (long)sizeof(BaseAuth)) {
        remove(FICHIER_USERS); creer_base_defaut(b); return;
    }
    f = fopen(FICHIER_USERS, "rb");
    fread(b, sizeof(BaseAuth), 1, f); fclose(f);
    b->user_connecte = -1;
    if (b->nb_users < 0 || b->nb_users > MAX_USERS) {
        remove(FICHIER_USERS); creer_base_defaut(b); return;
    }
    printf("[INFO] %d utilisateur(s) charge(s)\n", b->nb_users);
}

typedef enum { LOGIN_OK, LOGIN_INCONNU, LOGIN_MDP_FAUX, LOGIN_BLOQUE } ResultatLogin;

ResultatLogin auth_connecter(BaseAuth* b, const char* u, const char* p) {
    for (int i = 0; i < b->nb_users; i++) {
        if (strcmp(b->users[i].username, u) == 0) {
            Utilisateur* usr = &b->users[i];
            if (usr->tentatives_echouees >= 5) return LOGIN_BLOQUE;
            if (!verifier_mdp(p, usr->password)) {
                usr->tentatives_echouees++; auth_sauvegarder(b); return LOGIN_MDP_FAUX;
            }
            usr->tentatives_echouees = 0; usr->nb_connexions++;
            get_date_str(usr->derniere_connexion);
            b->user_connecte = i; auth_sauvegarder(b); return LOGIN_OK;
        }
    }
    return LOGIN_INCONNU;
}

void auth_message_bienvenue(BaseAuth* base, char* out, int taille) {
    if (base->user_connecte < 0) { strncpy(out, "", taille); return; }
    Utilisateur* u = &base->users[base->user_connecte];
    if (u->nb_connexions == 1)
        snprintf(out, taille, "Bienvenue %s ! Premiere connexion.", u->nom_complet);
    else if (u->score_theo >= 80 && u->score_prat >= 80)
        snprintf(out, taille, "Bon retour %s ! Theo %d%% | Prat %d%%",
                 u->nom_complet, u->score_theo, u->score_prat);
    else
        snprintf(out, taille, "Bon retour %s ! Derniere co: %s",
                 u->nom_complet, u->derniere_connexion);
}

// ════════════════════════════════════════════════════════════════
//  UTILITAIRES SDL
// ════════════════════════════════════════════════════════════════
void fixer_repertoire() {
    char path[512]; GetModuleFileNameA(NULL, path, sizeof(path));
    char* s = strrchr(path, '\\');
    if (s) { *s = '\0'; SetCurrentDirectoryA(path); }
    char cwd[512]; GetCurrentDirectoryA(512, cwd);
    printf("[INFO] Repertoire : %s\n", cwd);
}
SDL_Texture* charger_texture(SDL_Renderer* ren, const char* nom) {
    char ch[512]; GetCurrentDirectoryA(512, ch);
    strncat(ch, "\\", sizeof(ch)-strlen(ch)-1);
    strncat(ch, nom,  sizeof(ch)-strlen(ch)-1);
    SDL_Surface* s = IMG_Load(ch);
    if (!s) { printf("[ERREUR IMAGE] %s\n", IMG_GetError()); return NULL; }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s); return t;
}
SDL_Texture* charger_texture_chemin(SDL_Renderer* ren, const char* chemin) {
    SDL_Surface* s = IMG_Load(chemin);
    if (!s) return NULL;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s); return t;
}
void dessiner_texte(SDL_Renderer* ren, TTF_Font* font, const char* txt,
                    SDL_Color col, int x, int y, int centre) {
    if (!font||!txt||!txt[0]) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(font, txt, col); if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect d = {centre?x-s->w/2:x, y, s->w, s->h};
    SDL_FreeSurface(s); SDL_RenderCopy(ren,t,NULL,&d); SDL_DestroyTexture(t);
}
void dessiner_cercle_plein(SDL_Renderer* ren, int cx, int cy, int r,
                            SDL_Color c, int alpha) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, alpha);
    for (int dy=-r; dy<=r; dy++) {
        int dx=(int)sqrt((double)(r*r-dy*dy));
        SDL_RenderDrawLine(ren,cx-dx,cy+dy,cx+dx,cy+dy);
    }
}
void dessiner_cercle_contour(SDL_Renderer* ren, int cx, int cy, int r, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
    for (int deg=0; deg<360; deg++) {
        double rad = deg*3.14159265/180.0;
        SDL_RenderDrawPoint(ren, cx+(int)(r*cos(rad)), cy+(int)(r*sin(rad)));
    }
}
void dessiner_photo_cercle(SDL_Renderer* ren, SDL_Texture* tex,
                            int cx, int cy, int r) {
    if (!tex) return;
    int tw, th; SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    int side = tw < th ? tw : th;
    SDL_Rect src = {(tw-side)/2, (th-side)/2, side, side};
    SDL_Rect dst = {cx-r, cy-r, r*2, r*2};
    SDL_RenderCopy(ren, tex, &src, &dst);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 8, 8, 20, 255);
    for (int py=cy-r-1; py<=cy+r+1; py++)
        for (int px=cx-r-1; px<=cx+r+1; px++) {
            int dx=px-cx, dy=py-cy;
            if (dx*dx+dy*dy > r*r) SDL_RenderDrawPoint(ren, px, py);
        }
}

SDL_Color AVATAR_COULEURS[NB_AVATARS] = {
    {100,149,237,255},{60,179,113,255},{220,80,80,255},
    {180,100,220,255},{240,150,50,255},{70,200,200,255}
};
const char* AVATAR_NOMS[NB_AVATARS] = {
    "Bleu","Vert","Rouge","Violet","Orange","Turquoise"
};
void dessiner_avatar(SDL_Renderer* ren, TTF_Font* font,
                     int cx, int cy, int r, int avatar_id, const char* ini) {
    SDL_Color col={AVATAR_COULEURS[avatar_id%NB_AVATARS].r,
                   AVATAR_COULEURS[avatar_id%NB_AVATARS].g,
                   AVATAR_COULEURS[avatar_id%NB_AVATARS].b,255};
    SDL_Color jaune={240,192,64,255}, blanc={255,255,255,255};
    dessiner_cercle_plein(ren,cx,cy,r,col,220);
    dessiner_cercle_contour(ren,cx,cy,r,jaune);
    if(ini&&ini[0]) dessiner_texte(ren,font,ini,blanc,cx,cy-10,1);
}

int ouvrir_explorateur_photo(char* chemin_out, int taille) {
    OPENFILENAMEA ofn; char buf[MAX_PATH_PHOTO]={0};
    ZeroMemory(&ofn,sizeof(ofn));
    ofn.lStructSize=sizeof(ofn); ofn.lpstrFile=buf; ofn.nMaxFile=sizeof(buf);
    ofn.lpstrFilter="Images\0*.png;*.jpg;*.jpeg;*.bmp\0Tous\0*.*\0";
    ofn.nFilterIndex=1; ofn.lpstrTitle="Choisissez votre photo de profil";
    ofn.Flags=OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
    if(GetOpenFileNameA(&ofn)){strncpy(chemin_out,buf,taille-1);chemin_out[taille-1]='\0';return 1;}
    return 0;
}
int capturer_webcam_windows(const char* username, char* chemin_out, int taille) {
    CreateDirectoryA("photos",NULL);
    char nom[256]; snprintf(nom,sizeof(nom),"photos\\%s_profil.bmp",username);
    HDC hdcScreen=GetDC(NULL); HDC hdcMem=CreateCompatibleDC(hdcScreen);
    int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
    HBITMAP hbm=CreateCompatibleBitmap(hdcScreen,320,320);
    SelectObject(hdcMem,hbm);
    BitBlt(hdcMem,0,0,320,320,hdcScreen,sw/2-160,sh/2-160,SRCCOPY);
    BITMAPFILEHEADER bfh; BITMAPINFOHEADER bih;
    bih.biSize=sizeof(BITMAPINFOHEADER);bih.biWidth=320;bih.biHeight=-320;
    bih.biPlanes=1;bih.biBitCount=24;bih.biCompression=BI_RGB;
    bih.biSizeImage=320*320*3;bih.biXPelsPerMeter=0;bih.biYPelsPerMeter=0;
    bih.biClrUsed=0;bih.biClrImportant=0;
    bfh.bfType=0x4D42;
    bfh.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
    bfh.bfSize=bfh.bfOffBits+bih.biSizeImage;
    bfh.bfReserved1=0;bfh.bfReserved2=0;
    unsigned char* pixels=(unsigned char*)malloc(bih.biSizeImage);
    if(pixels){
        GetDIBits(hdcMem,hbm,0,320,pixels,(BITMAPINFO*)&bih,DIB_RGB_COLORS);
        FILE* fout=fopen(nom,"wb");
        if(fout){fwrite(&bfh,sizeof(bfh),1,fout);fwrite(&bih,sizeof(bih),1,fout);
                 fwrite(pixels,bih.biSizeImage,1,fout);fclose(fout);}
        free(pixels);
    }
    DeleteObject(hbm);DeleteDC(hdcMem);ReleaseDC(NULL,hdcScreen);
    strncpy(chemin_out,nom,taille-1);chemin_out[taille-1]='\0';
    return 1;
}

// ════════════════════════════════════════════════════════════════
//  CHAMPS TEXTE
// ════════════════════════════════════════════════════════════════
void init_champ(ChampTexte* c, int x, int y, int w, int h, int mdp) {
    memset(c,0,sizeof(ChampTexte)); c->est_mdp=mdp; c->rect=(SDL_Rect){x,y,w,h};
}
void champ_input(ChampTexte* c, SDL_Keycode k, const char* txt) {
    if(!c->actif) return;
    if(k==SDLK_BACKSPACE&&c->longueur>0) c->texte[--c->longueur]='\0';
    else if(txt&&txt[0]>=32&&c->longueur<MAX_INPUT){
        c->texte[c->longueur++]=txt[0]; c->texte[c->longueur]='\0';
    }
}
void activer_champs(ChampTexte** arr, int n, int x, int y) {
    for(int i=0;i<n;i++){SDL_Point p={x,y};arr[i]->actif=SDL_PointInRect(&p,&arr[i]->rect);}
}

// ── Dessine un œil (icône afficher/masquer mdp) ───────────────
void dessiner_oeil(SDL_Renderer* ren, int cx, int cy, int visible) {
    SDL_Color col = visible ? (SDL_Color){240,192,64,255} : (SDL_Color){120,120,140,255};
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
    // Forme ovale externe (ellipse approximée)
    for (int deg=0; deg<360; deg++) {
        double rad = deg*3.14159265/180.0;
        int px = cx+(int)(13*cos(rad));
        int py = cy+(int)(7*sin(rad));
        SDL_RenderDrawPoint(ren, px, py);
    }
    // Pupille (cercle central)
    for (int dy=-3; dy<=3; dy++) {
        int dx=(int)sqrt((double)(9-dy*dy));
        SDL_RenderDrawLine(ren, cx-dx, cy+dy, cx+dx, cy+dy);
    }
    // Barre oblique si masqué
    if (!visible) {
        SDL_SetRenderDrawColor(ren, 200, 80, 80, 255);
        SDL_RenderDrawLine(ren, cx-10, cy-8, cx+10, cy+8);
        SDL_RenderDrawLine(ren, cx-11, cy-8, cx+9,  cy+8);
    }
}

void dessiner_champ(SDL_Renderer* ren, TTF_Font* fm, TTF_Font* fp,
                    ChampTexte* c, const char* label) {
    SDL_Color blanc={255,255,255,255},gris={150,150,150,255};
    dessiner_texte(ren,fp,label,gris,c->rect.x,c->rect.y-18,0);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,255,255,255,c->actif?20:10);
    SDL_RenderFillRect(ren,&c->rect);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    if(c->actif) SDL_SetRenderDrawColor(ren,240,192,64,255);
    else         SDL_SetRenderDrawColor(ren,80,80,100,255);
    SDL_RenderDrawRect(ren,&c->rect);
    // Texte (masqué ou visible)
    char aff[MAX_INPUT+1]={0};
    int afficher_texte = !c->est_mdp || c->mdp_visible;
    if(!afficher_texte){for(int i=0;i<c->longueur;i++) aff[i]='*';aff[c->longueur]=0;}
    else strncpy(aff,c->texte,MAX_INPUT);
    if(c->longueur>0) dessiner_texte(ren,fm,aff,blanc,c->rect.x+12,c->rect.y+8,0);
    // Curseur
    if(c->actif&&(SDL_GetTicks()/500)%2==0){
        SDL_SetRenderDrawColor(ren,240,192,64,255);
        int cx2=c->rect.x+12+c->longueur*9;
        if(cx2>c->rect.x+c->rect.w-10) cx2=c->rect.x+c->rect.w-10;
        SDL_RenderDrawLine(ren,cx2,c->rect.y+7,cx2,c->rect.y+c->rect.h-7);
    }
    // Icône œil pour les champs mot de passe
    if(c->est_mdp) {
        int oeil_cx = c->rect.x + c->rect.w - 22;
        int oeil_cy = c->rect.y + c->rect.h/2;
        dessiner_oeil(ren, oeil_cx, oeil_cy, c->mdp_visible);
    }
}

// Retourne 1 si clic sur l'œil
int clic_oeil(ChampTexte* c, int mx, int my) {
    if (!c->est_mdp) return 0;
    int oeil_cx = c->rect.x + c->rect.w - 22;
    int oeil_cy = c->rect.y + c->rect.h/2;
    int dx=mx-oeil_cx, dy=my-oeil_cy;
    return (dx*dx + dy*dy <= 15*15);
}

void dessiner_force_mdp(SDL_Renderer* ren, TTF_Font* fp,
                         const char* mdp, int x, int y, int larg) {
    int score=0,len=strlen(mdp);
    if(len>=6) score++; if(len>=10) score++;
    int maj=0,chif=0,spec=0;
    for(int i=0;i<len;i++){
        if(mdp[i]>='A'&&mdp[i]<='Z') maj=1;
        if(mdp[i]>='0'&&mdp[i]<='9') chif=1;
        if((mdp[i]<'0')||(mdp[i]>'9'&&mdp[i]<'A')||
           (mdp[i]>'Z'&&mdp[i]<'a')||mdp[i]>'z') spec=1;
    }
    score+=maj+chif+spec;
    SDL_Rect fond={x,y,larg,5};
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,40,40,55,255); SDL_RenderFillRect(ren,&fond);
    if(len>0){
        SDL_Rect fill={x,y,(score*larg)/5,5};
        if(score<=1) SDL_SetRenderDrawColor(ren,226,75,74,255);
        else if(score<=3) SDL_SetRenderDrawColor(ren,239,159,39,255);
        else SDL_SetRenderDrawColor(ren,29,158,117,255);
        SDL_RenderFillRect(ren,&fill);
    }
    const char* labs[]={"","Faible","Faible","Moyen","Fort","Tres fort"};
    SDL_Color gris={110,110,130,255};
    if(score>0&&score<=5) dessiner_texte(ren,fp,labs[score],gris,x+larg+8,y-3,0);
}

// ════════════════════════════════════════════════════════════════
//  BOUTON RETOUR (flèche)
// ════════════════════════════════════════════════════════════════
void dessiner_btn_retour_fleche(SDL_Renderer* ren, TTF_Font* fp,
                                 int x, int y, int survol) {
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_Color col = survol ? (SDL_Color){240,192,64,255} : (SDL_Color){120,120,140,255};
    // Cercle de fond
    SDL_SetRenderDrawColor(ren, 30, 30, 50, survol?220:160);
    SDL_Rect fond={x-20,y-20,40,40}; SDL_RenderFillRect(ren,&fond);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,col.r,col.g,col.b,255);
    SDL_RenderDrawRect(ren,&fond);
    // Flèche ←
    SDL_RenderDrawLine(ren, x-8, y,   x+8, y);    // trait horizontal
    SDL_RenderDrawLine(ren, x-8, y,   x-2, y-6);  // branche haute
    SDL_RenderDrawLine(ren, x-8, y,   x-2, y+6);  // branche basse
}

int clic_btn_retour_fleche(int x, int y, int mx, int my) {
    return (mx>=x-20&&mx<=x+20&&my>=y-20&&my<=y+20);
}

// ════════════════════════════════════════════════════════════════
//  REINIT AUTH
// ════════════════════════════════════════════════════════════════
void reinit_auth(EcranAuth* auth, BaseAuth* base) {
    if(auth->preview_tex) SDL_DestroyTexture(auth->preview_tex);
    memset(auth,0,sizeof(EcranAuth));
    auth->base=base; auth->mode=0; auth->avatar_selec=0;
    int pw=430, px=SCREEN_W/2-pw/2;
    init_champ(&auth->champ_user,px+20,SCREEN_H/2-65,pw-40,38,0);
    init_champ(&auth->champ_pass,px+20,SCREEN_H/2+25,pw-40,38,1);
    init_champ(&auth->reg_nom,   px+20,SCREEN_H/2-95,pw-40,38,0);
    init_champ(&auth->reg_user,  px+20,SCREEN_H/2+0, pw-40,38,0);
    init_champ(&auth->reg_pass,  px+20,SCREEN_H/2+95,pw-40,38,1);
}

// ════════════════════════════════════════════════════════════════
//  REINIT PARAMETRES
// ════════════════════════════════════════════════════════════════
void reinit_parametres(EcranParametres* p, BaseAuth* base) {
    memset(p,0,sizeof(EcranParametres));
    p->page=0;
    int pw=400, px=SCREEN_W/2-pw/2;
    init_champ(&p->ch_nom,    px+20, SCREEN_H/2-60, pw-40, 38, 0);
    init_champ(&p->ch_mdp,    px+20, SCREEN_H/2+20, pw-40, 38, 1);
    init_champ(&p->ch_confirm,px+20, SCREEN_H/2+100,pw-40, 38, 1);
    // Pré-remplir nom
    if(base->user_connecte>=0){
        strncpy(p->ch_nom.texte,
                base->users[base->user_connecte].nom_complet, MAX_INPUT);
        p->ch_nom.longueur=strlen(p->ch_nom.texte);
    }
}

// ════════════════════════════════════════════════════════════════
//  RENDU VIDEO OVERLAY
// ════════════════════════════════════════════════════════════════
void rendu_video_overlay(SDL_Renderer* ren, TTF_Font* fp) {
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,100);
    SDL_Rect bas={0,SCREEN_H-40,SCREEN_W,40};
    SDL_RenderFillRect(ren,&bas);
    SDL_Color gris={180,180,180,255};
    dessiner_texte(ren,fp,"Appuyez sur ESPACE ou ECHAP pour passer",
                   gris,SCREEN_W/2,SCREEN_H-28,1);
}

// ════════════════════════════════════════════════════════════════
//  RENDU CHARGEMENT
// ════════════════════════════════════════════════════════════════
void rendu_chargement(SDL_Renderer* ren, SDL_Texture* tex_intro,
                      TTF_Font* fg, TTF_Font* fm, TTF_Font* fp,
                      Uint32 tps_debut, const char* msg_bvn) {
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    if(tex_intro){SDL_Rect d={0,0,SCREEN_W,SCREEN_H};SDL_RenderCopy(ren,tex_intro,NULL,&d);}
    else{SDL_SetRenderDrawColor(ren,15,15,35,255);SDL_RenderClear(ren);}
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,150);
    SDL_Rect pl={0,0,SCREEN_W,SCREEN_H}; SDL_RenderFillRect(ren,&pl);
    if(msg_bvn&&msg_bvn[0]){
        SDL_SetRenderDrawColor(ren,0,0,0,170);
        SDL_Rect band={0,0,SCREEN_W,36}; SDL_RenderFillRect(ren,&band);
        SDL_Color j2={240,192,64,255};
        dessiner_texte(ren,fp,msg_bvn,j2,SCREEN_W/2,10,1);
    }
    SDL_Color blanc={255,255,255,255},jaune={240,192,64,255};
    dessiner_texte(ren,fg,"Bienvenu dans votre",     blanc,SCREEN_W/2,SCREEN_H/2-80,1);
    dessiner_texte(ren,fg,"Simulateur d'Auto-Ecole", jaune,SCREEN_W/2,SCREEN_H/2-30,1);
    Uint32 ecoule=SDL_GetTicks()-tps_debut;
    float pct=(float)ecoule/(float)LOADING_DURATION;
    if(pct>1.0f) pct=1.0f;
    int pi=(int)(pct*100.0f);
    char buf[8]; snprintf(buf,8,"%d%%",pi);
    dessiner_texte(ren,fg,buf,jaune,SCREEN_W/2,SCREEN_H/2+80,1);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,160);
    SDL_Rect fb={55,SCREEN_H-120,SCREEN_W-110,36}; SDL_RenderFillRect(ren,&fb);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,90,90,110,255); SDL_RenderDrawRect(ren,&fb);
    if(pi>0){
        SDL_Rect fill={55,SCREEN_H-120,(int)((SCREEN_W-110)*pct),36};
        SDL_SetRenderDrawColor(ren,240,192,64,255); SDL_RenderFillRect(ren,&fill);
    }
    SDL_Color gris={200,200,200,255};
    dessiner_texte(ren,fm,"Chargement en cours...",gris,SCREEN_W/2,SCREEN_H-70,1);
}

// ════════════════════════════════════════════════════════════════
//  RENDU AUTH — avec œil + flèche retour sur page photo
// ════════════════════════════════════════════════════════════════
void rendu_auth(SDL_Renderer* ren, EcranAuth* auth,
                TTF_Font* fg, TTF_Font* fm, TTF_Font* fp,
                int mx, int my) {
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,8,8,20,255); SDL_RenderClear(ren);
    auth->anime++;
    srand(42);
    for(int i=0;i<16;i++){
        int px=(rand()%(SCREEN_W-20))+10,vit=1+rand()%3;
        int py=SCREEN_H-((auth->anime*vit+i*53)%(SCREEN_H+30));
        int sz=2+rand()%4,al=20+rand()%60;
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,240,192,64,al);
        SDL_Rect pr={px,py,sz,sz}; SDL_RenderFillRect(ren,&pr);
    }
    SDL_Color jaune={240,192,64,255},blanc={255,255,255,255};
    SDL_Color gris={155,155,165,255},rouge={226,100,100,255};
    SDL_Color vert={80,200,120,255},noir={10,8,0,255};

    // ── MODE 2 : CHOIX PHOTO/AVATAR ──────────────────────────
    if(auth->mode==2){
        int apw=520,aph=440,apx=SCREEN_W/2-260,apy=SCREEN_H/2-220;

        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,15,15,38,240);
        SDL_Rect pan={apx,apy,apw,aph}; SDL_RenderFillRect(ren,&pan);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,240,192,64,255);
        SDL_RenderDrawRect(ren,&pan);
        SDL_Rect bande={apx,apy,apw,6}; SDL_RenderFillRect(ren,&bande);

        // ── Flèche retour (haut gauche du panneau) ────────────
        int fleche_x=apx+30, fleche_y=apy+32;
        int fleche_survol=clic_btn_retour_fleche(fleche_x,fleche_y,mx,my);
        dessiner_btn_retour_fleche(ren,fp,fleche_x,fleche_y,fleche_survol);
        SDL_Color gris_h={120,120,140,255};
        dessiner_texte(ren,fp,"Retour",gris_h,apx+60,apy+24,0);

        dessiner_texte(ren,fg,"Photo de profil",jaune,SCREEN_W/2,apy+18,1);
        dessiner_texte(ren,fp,"Choisissez votre photo ou un avatar",
                       gris,SCREEN_W/2,apy+54,1);

        int prev_cx=SCREEN_W/2,prev_cy=apy+140,prev_r=55;
        if(auth->tmp_a_photo&&auth->preview_tex){
            dessiner_photo_cercle(ren,auth->preview_tex,prev_cx,prev_cy,prev_r);
            dessiner_cercle_contour(ren,prev_cx,prev_cy,prev_r,jaune);
        } else {
            char ini[2]={auth->tmp_nom[0],'\0'};
            dessiner_avatar(ren,fm,prev_cx,prev_cy,prev_r,auth->avatar_selec,ini[0]?ini:"?");
        }
        SDL_Color gris2={140,140,150,255};
        dessiner_texte(ren,fp,auth->tmp_a_photo?"Photo selectionnee !":"Avatar par defaut",
                       auth->tmp_a_photo?vert:gris2,prev_cx,prev_cy+prev_r+10,1);

        SDL_SetRenderDrawColor(ren,60,60,80,255);
        SDL_RenderDrawLine(ren,apx+20,apy+215,apx+apw-20,apy+215);

        SDL_Rect btn_f={apx+20,apy+228,apw/2-30,42};
        SDL_Rect btn_w={apx+apw/2+10,apy+228,apw/2-30,42};
        SDL_Point souris={mx,my};
        int s1=SDL_PointInRect(&souris,&btn_f),s2=SDL_PointInRect(&souris,&btn_w);

        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,29,158,117,s1?200:140); SDL_RenderFillRect(ren,&btn_f);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,29,200,140,255); SDL_RenderDrawRect(ren,&btn_f);
        dessiner_texte(ren,fp,"Parcourir fichier",blanc,btn_f.x+btn_f.w/2,btn_f.y+13,1);

        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,100,149,237,s2?200:140); SDL_RenderFillRect(ren,&btn_w);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,130,170,255,255); SDL_RenderDrawRect(ren,&btn_w);
        dessiner_texte(ren,fp,"Capture ecran",blanc,btn_w.x+btn_w.w/2,btn_w.y+13,1);

        dessiner_texte(ren,fp,"— ou choisissez un avatar —",gris,SCREEN_W/2,apy+282,1);

        int av_r=22,av_y=apy+322,total_w=NB_AVATARS*(av_r*2+12)-12;
        int start_x=SCREEN_W/2-total_w/2+av_r;
        int av_xs[NB_AVATARS];
        for(int i=0;i<NB_AVATARS;i++) av_xs[i]=start_x+i*(av_r*2+12);
        for(int i=0;i<NB_AVATARS;i++){
            if(!auth->tmp_a_photo&&auth->avatar_selec==i){
                dessiner_cercle_contour(ren,av_xs[i],av_y,av_r+4,jaune);
                dessiner_cercle_contour(ren,av_xs[i],av_y,av_r+5,jaune);
            }
            char ini2[2]={AVATAR_NOMS[i][0],'\0'};
            dessiner_avatar(ren,fm,av_xs[i],av_y,av_r,i,ini2);
        }

        SDL_Rect btn_c={apx+apw/2-90,apy+aph-52,180,40};
        int survc=SDL_PointInRect(&souris,&btn_c);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,survc?255:220,survc?205:175,survc?80:50,255);
        SDL_RenderFillRect(ren,&btn_c);
        dessiner_texte(ren,fm,"CONFIRMER",noir,btn_c.x+btn_c.w/2,btn_c.y+10,1);

        if(auth->message[0]){
            SDL_Color coul=auth->msg_ok?vert:rouge;
            dessiner_texte(ren,fp,auth->message,coul,SCREEN_W/2,apy+aph-12,1);
        }
        return;
    }

    // ── CONNEXION / INSCRIPTION ───────────────────────────────
    int ph=(auth->mode==0)?400:480;
    int pw=440,px=SCREEN_W/2-pw/2,py=SCREEN_H/2-ph/2;

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,15,15,38,235);
    SDL_Rect pan={px,py,pw,ph}; SDL_RenderFillRect(ren,&pan);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,240,192,64,255);
    SDL_RenderDrawRect(ren,&pan);
    SDL_Rect bande={px,py,pw,6}; SDL_RenderFillRect(ren,&bande);

    // Logo + titre
    dessiner_texte(ren,fg,"AUTO-ECOLE SIM",jaune,SCREEN_W/2,py+14,1);
    dessiner_texte(ren,fp,"Systeme d'authentification securise",
                   gris,SCREEN_W/2,py+52,1);
    SDL_SetRenderDrawColor(ren,60,60,80,255);
    SDL_RenderDrawLine(ren,px+20,py+76,px+pw-20,py+76);

    SDL_Rect tab_co ={px+20,           py+84,(pw-40)/2,32};
    SDL_Rect tab_ins={px+20+(pw-40)/2, py+84,(pw-40)/2,32};
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,240,192,64,auth->mode==0?45:8); SDL_RenderFillRect(ren,&tab_co);
    SDL_SetRenderDrawColor(ren,240,192,64,auth->mode==1?45:8); SDL_RenderFillRect(ren,&tab_ins);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,80,80,100,255);
    SDL_RenderDrawRect(ren,&tab_co); SDL_RenderDrawRect(ren,&tab_ins);
    dessiner_texte(ren,fm,"Connexion",
                   auth->mode==0?jaune:gris,tab_co.x+tab_co.w/2,  tab_co.y+7, 1);
    dessiner_texte(ren,fm,"Inscription",
                   auth->mode==1?jaune:gris,tab_ins.x+tab_ins.w/2,tab_ins.y+7,1);

    if(auth->mode==0){
        // Champs connexion
        dessiner_champ(ren,fm,fp,&auth->champ_user,"IDENTIFIANT");
        dessiner_champ(ren,fm,fp,&auth->champ_pass,"MOT DE PASSE");

        // Bouton connexion
        SDL_Rect btn={px+20,py+310,pw-40,46};
        SDL_Point s={mx,my}; int surv=SDL_PointInRect(&s,&btn);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,surv?255:220,surv?205:175,surv?80:50,255);
        SDL_RenderFillRect(ren,&btn);
        dessiner_texte(ren,fm,"SE CONNECTER",noir,btn.x+btn.w/2,btn.y+13,1);

        SDL_SetRenderDrawColor(ren,55,55,72,255);
        SDL_RenderDrawLine(ren,px+40,py+368,px+pw-40,py+368);
        dessiner_texte(ren,fp,"ou",gris,SCREEN_W/2,py+360,1);
        dessiner_texte(ren,fp,"Pas encore de compte ? Inscrivez-vous",
                       gris,SCREEN_W/2,py+376,1);
    } else {
        dessiner_champ(ren,fm,fp,&auth->reg_nom, "NOM COMPLET");
        dessiner_champ(ren,fm,fp,&auth->reg_user,"IDENTIFIANT");
        dessiner_champ(ren,fm,fp,&auth->reg_pass,"MOT DE PASSE");
        dessiner_force_mdp(ren,fp,auth->reg_pass.texte,
                           auth->reg_pass.rect.x,
                           auth->reg_pass.rect.y+auth->reg_pass.rect.h+6,
                           auth->reg_pass.rect.w-90);
        SDL_Rect btn={px+20,py+400,pw-40,46};
        SDL_Point s={mx,my}; int surv=SDL_PointInRect(&s,&btn);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,surv?255:220,surv?205:175,surv?80:50,255);
        SDL_RenderFillRect(ren,&btn);
        dessiner_texte(ren,fm,"SUIVANT : PHOTO DE PROFIL  →",
                       noir,btn.x+btn.w/2,btn.y+13,1);
        dessiner_texte(ren,fp,"Deja un compte ? Connectez-vous",
                       gris,SCREEN_W/2,py+458,1);
    }
    if(auth->message[0]){
        SDL_Color coul=auth->msg_ok?vert:rouge;
        dessiner_texte(ren,fp,auth->message,coul,SCREEN_W/2,py+ph-14,1);
    }
}

// ════════════════════════════════════════════════════════════════
//  GESTION EVENTS AUTH
// ════════════════════════════════════════════════════════════════
int auth_handle_event(SDL_Event* ev, EcranAuth* auth,
                      int mx, int my, EtatApp* etat, SDL_Renderer* ren) {

    // ── MODE 2 : photo/avatar ─────────────────────────────────
    if(auth->mode==2){
        int apw=520,aph=440,apx=SCREEN_W/2-260,apy=SCREEN_H/2-220;
        int av_r=22,av_y=apy+322,total_w=NB_AVATARS*(av_r*2+12)-12;
        int start_x=SCREEN_W/2-total_w/2+av_r;
        int av_xs[NB_AVATARS];
        for(int i=0;i<NB_AVATARS;i++) av_xs[i]=start_x+i*(av_r*2+12);
        SDL_Rect btn_f={apx+20,apy+228,apw/2-30,42};
        SDL_Rect btn_w={apx+apw/2+10,apy+228,apw/2-30,42};
        SDL_Rect btn_c={apx+apw/2-90,apy+aph-52,180,40};
        int fleche_x=apx+30, fleche_y=apy+32;

        if(ev->type==SDL_MOUSEBUTTONDOWN&&ev->button.button==SDL_BUTTON_LEFT){
            SDL_Point p={mx,my};

            // Flèche retour → revient à l'inscription
            if(clic_btn_retour_fleche(fleche_x,fleche_y,mx,my)){
                auth->mode=1;
                auth->message[0]=0;
                return 0;
            }

            // Clic avatar
            for(int i=0;i<NB_AVATARS;i++){
                int dx=mx-av_xs[i],dy=my-av_y;
                if(dx*dx+dy*dy<=av_r*av_r){
                    auth->avatar_selec=i; auth->tmp_a_photo=0;
                    if(auth->preview_tex){SDL_DestroyTexture(auth->preview_tex);auth->preview_tex=NULL;}
                    memset(auth->tmp_photo,0,MAX_PATH_PHOTO);
                }
            }

            // Parcourir fichier
            if(SDL_PointInRect(&p,&btn_f)){
                char ch[MAX_PATH_PHOTO]={0};
                if(ouvrir_explorateur_photo(ch,MAX_PATH_PHOTO)){
                    strncpy(auth->tmp_photo,ch,MAX_PATH_PHOTO-1);
                    auth->tmp_a_photo=1;
                    if(auth->preview_tex) SDL_DestroyTexture(auth->preview_tex);
                    auth->preview_tex=charger_texture_chemin(ren,ch);
                    snprintf(auth->message,256,"Photo chargee !"); auth->msg_ok=1;
                }
            }

            // Capture écran
            if(SDL_PointInRect(&p,&btn_w)){
                char ch[MAX_PATH_PHOTO]={0};
                if(capturer_webcam_windows(auth->tmp_user,ch,MAX_PATH_PHOTO)){
                    strncpy(auth->tmp_photo,ch,MAX_PATH_PHOTO-1);
                    auth->tmp_a_photo=1;
                    if(auth->preview_tex) SDL_DestroyTexture(auth->preview_tex);
                    auth->preview_tex=charger_texture_chemin(ren,ch);
                    snprintf(auth->message,256,"Capture effectuee !"); auth->msg_ok=1;
                }
            }

            // Confirmer
            if(SDL_PointInRect(&p,&btn_c)){
                if(!auth->tmp_nom[0]||!auth->tmp_user[0]||!auth->tmp_pass[0]){
                    snprintf(auth->message,256,"Erreur : donnees manquantes.");
                    auth->msg_ok=0; auth->mode=1; return 0;
                }
                if(auth->base->nb_users>=MAX_USERS){
                    snprintf(auth->message,256,"Nombre max atteint.");
                    auth->msg_ok=0; auth->mode=1; return 0;
                }
                int idx=auth->base->nb_users;
                Utilisateur* usr=&auth->base->users[idx];
                memset(usr,0,sizeof(Utilisateur));
                strncpy(usr->username,   auth->tmp_user,MAX_STR-1);
                hasher_mdp(auth->tmp_pass,usr->password);
                strncpy(usr->nom_complet,auth->tmp_nom, MAX_STR-1);
                strcpy(usr->role,"eleve");
                strcpy(usr->derniere_connexion,"Jamais");
                usr->avatar_id=auth->avatar_selec;
                usr->a_photo=auth->tmp_a_photo;
                usr->nb_connexions=0; usr->score_theo=0;
                usr->score_prat=0; usr->tentatives_echouees=0;
                if(auth->tmp_a_photo && auth->tmp_photo[0]) {
                    CreateDirectoryA("photos", NULL);
                    const char *dot=strrchr(auth->tmp_photo,'.');
                    char ext[8]=".bmp"; if(dot) strncpy(ext,dot,7);
                    char dest[MAX_PATH_PHOTO];
                    snprintf(dest,sizeof(dest),"photos\\%s_profil%s",usr->username,ext);
                    FILE *fin=fopen(auth->tmp_photo,"rb"); FILE *fout=fopen(dest,"wb");
                    if(fin&&fout){unsigned char buf[4096];size_t n;
                        while((n=fread(buf,1,sizeof(buf),fin))>0) fwrite(buf,1,n,fout);}
                    if(fin)fclose(fin); if(fout)fclose(fout);
                    strncpy(usr->photo_path,dest,MAX_PATH_PHOTO-1);
                } else if(auth->tmp_a_photo){
                    strncpy(usr->photo_path,auth->tmp_photo,MAX_PATH_PHOTO-1);
                }
                auth->base->nb_users=idx+1;
                auth_sauvegarder(auth->base);
                printf("[OK] Compte cree : %s\n",usr->nom_complet);
                if(auth->preview_tex){SDL_DestroyTexture(auth->preview_tex);auth->preview_tex=NULL;}
                memset(auth->tmp_nom,0,MAX_STR); memset(auth->tmp_user,0,MAX_STR);
                memset(auth->tmp_pass,0,MAX_STR); memset(auth->tmp_photo,0,MAX_PATH_PHOTO);
                int pw2=440,px2=SCREEN_W/2-pw2/2;
                init_champ(&auth->champ_user,px2+20,SCREEN_H/2-65,pw2-40,38,0);
                init_champ(&auth->champ_pass,px2+20,SCREEN_H/2+25,pw2-40,38,1);
                init_champ(&auth->reg_nom,   px2+20,SCREEN_H/2-95,pw2-40,38,0);
                init_champ(&auth->reg_user,  px2+20,SCREEN_H/2+0, pw2-40,38,0);
                init_champ(&auth->reg_pass,  px2+20,SCREEN_H/2+95,pw2-40,38,1);
                snprintf(auth->message,256,"Compte cree ! Connectez-vous.");
                auth->msg_ok=1; auth->mode=0;
            }
        }
        return 0;
    }

    // ── CONNEXION / INSCRIPTION ───────────────────────────────
    int ph=(auth->mode==0)?400:480;
    int pw=440,px=SCREEN_W/2-pw/2,py=SCREEN_H/2-ph/2;
    SDL_Rect tab_co ={px+20,           py+84,(pw-40)/2,32};
    SDL_Rect tab_ins={px+20+(pw-40)/2, py+84,(pw-40)/2,32};
    SDL_Rect btn_co ={px+20,py+310,pw-40,46};
    SDL_Rect btn_ins={px+20,py+400,pw-40,46};
    ChampTexte* fco[] ={&auth->champ_user,&auth->champ_pass};
    ChampTexte* fins[]={&auth->reg_nom,&auth->reg_user,&auth->reg_pass};

    if(ev->type==SDL_MOUSEBUTTONDOWN&&ev->button.button==SDL_BUTTON_LEFT){
        SDL_Point p={mx,my};

        // Onglets
        if(SDL_PointInRect(&p,&tab_co))  {auth->mode=0;auth->message[0]=0;}
        if(SDL_PointInRect(&p,&tab_ins)) {auth->mode=1;auth->message[0]=0;}

        // Activation champs
        if(auth->mode==0) activer_champs(fco, 2,mx,my);
        else              activer_champs(fins,3,mx,my);

        // Clic sur l'œil des champs mdp
        if(auth->mode==0 && clic_oeil(&auth->champ_pass,mx,my))
            auth->champ_pass.mdp_visible=!auth->champ_pass.mdp_visible;
        if(auth->mode==1 && clic_oeil(&auth->reg_pass,mx,my))
            auth->reg_pass.mdp_visible=!auth->reg_pass.mdp_visible;

        // Bouton connexion
        if(auth->mode==0&&SDL_PointInRect(&p,&btn_co)){
            if(!auth->champ_user.texte[0]||!auth->champ_pass.texte[0]){
                snprintf(auth->message,256,"Remplissez tous les champs."); auth->msg_ok=0;
            } else {
                ResultatLogin r=auth_connecter(auth->base,
                    auth->champ_user.texte,auth->champ_pass.texte);
                if(r==LOGIN_OK){printf("[OK] Connexion\n");*etat=ETAT_MENU;return 1;}
                else if(r==LOGIN_BLOQUE){snprintf(auth->message,256,"Compte bloque (5 tentatives).");auth->msg_ok=0;}
                else{snprintf(auth->message,256,"Identifiant ou mot de passe incorrect.");auth->msg_ok=0;}
            }
        }

        // Bouton inscription → photo
        if(auth->mode==1&&SDL_PointInRect(&p,&btn_ins)){
            if(!auth->reg_nom.texte[0]||!auth->reg_user.texte[0]||!auth->reg_pass.texte[0]){
                snprintf(auth->message,256,"Tous les champs sont obligatoires."); auth->msg_ok=0;
            } else {
                int existe=0;
                for(int i=0;i<auth->base->nb_users;i++)
                    if(strcmp(auth->base->users[i].username,auth->reg_user.texte)==0) existe=1;
                if(existe){snprintf(auth->message,256,"Identifiant deja utilise.");auth->msg_ok=0;}
                else{
                    strncpy(auth->tmp_nom, auth->reg_nom.texte, MAX_STR-1);
                    strncpy(auth->tmp_user,auth->reg_user.texte,MAX_STR-1);
                    strncpy(auth->tmp_pass,auth->reg_pass.texte,MAX_STR-1);
                    auth->tmp_nom[MAX_STR-1]=auth->tmp_user[MAX_STR-1]=auth->tmp_pass[MAX_STR-1]='\0';
                    auth->avatar_selec=0; auth->tmp_a_photo=0; auth->message[0]=0; auth->mode=2;
                }
            }
        }
    }

    if(ev->type==SDL_KEYDOWN||ev->type==SDL_TEXTINPUT){
        SDL_Keycode k=(ev->type==SDL_KEYDOWN)?ev->key.keysym.sym:0;
        const char* t=(ev->type==SDL_TEXTINPUT)?ev->text.text:NULL;

        /* Haut/Bas : naviguer entre les champs */
        if(k==SDLK_DOWN||k==SDLK_UP||k==SDLK_TAB){
            int dir=(k==SDLK_UP)?-1:1;
            if(auth->mode==0){
                ChampTexte* fc[]={&auth->champ_user,&auth->champ_pass};
                int actif=-1;
                for(int i=0;i<2;i++) if(fc[i]->actif) actif=i;
                if(actif<0) actif=0;
                fc[actif]->actif=0;
                fc[((actif+dir)+2)%2]->actif=1;
            } else if(auth->mode==1){
                ChampTexte* fi[]={&auth->reg_nom,&auth->reg_user,&auth->reg_pass};
                int actif=-1;
                for(int i=0;i<3;i++) if(fi[i]->actif) actif=i;
                if(actif<0) actif=0;
                fi[actif]->actif=0;
                fi[((actif+dir)+3)%3]->actif=1;
            }
            return 0;
        }

        /* Entrée : valider le formulaire actif */
        if(k==SDLK_RETURN||k==SDLK_KP_ENTER){
            if(auth->mode==0){
                /* Simuler clic sur bouton Connexion */
                if(!auth->champ_user.texte[0]||!auth->champ_pass.texte[0]){
                    snprintf(auth->message,256,"Remplissez tous les champs."); auth->msg_ok=0;
                } else {
                    ResultatLogin r=auth_connecter(auth->base,
                        auth->champ_user.texte,auth->champ_pass.texte);
                    if(r==LOGIN_OK){printf("[OK] Connexion\n");*etat=ETAT_MENU;return 1;}
                    else if(r==LOGIN_BLOQUE){snprintf(auth->message,256,"Compte bloque (5 tentatives).");auth->msg_ok=0;}
                    else{snprintf(auth->message,256,"Identifiant ou mot de passe incorrect.");auth->msg_ok=0;}
                }
            } else if(auth->mode==1){
                /* Simuler clic sur bouton Inscription */
                if(!auth->reg_nom.texte[0]||!auth->reg_user.texte[0]||!auth->reg_pass.texte[0]){
                    snprintf(auth->message,256,"Tous les champs sont obligatoires."); auth->msg_ok=0;
                } else {
                    int existe=0;
                    for(int i=0;i<auth->base->nb_users;i++)
                        if(strcmp(auth->base->users[i].username,auth->reg_user.texte)==0) existe=1;
                    if(existe){snprintf(auth->message,256,"Identifiant deja utilise.");auth->msg_ok=0;}
                    else{
                        strncpy(auth->tmp_nom, auth->reg_nom.texte, MAX_STR-1);
                        strncpy(auth->tmp_user,auth->reg_user.texte,MAX_STR-1);
                        strncpy(auth->tmp_pass,auth->reg_pass.texte,MAX_STR-1);
                        auth->tmp_nom[MAX_STR-1]=auth->tmp_user[MAX_STR-1]=auth->tmp_pass[MAX_STR-1]='\0';
                        auth->avatar_selec=0; auth->tmp_a_photo=0; auth->message[0]=0; auth->mode=2;
                    }
                }
            } else if(auth->mode==2){
                /* Mode photo : Entrée = confirmer */
                SDL_Event fake; memset(&fake,0,sizeof(fake));
                int apw=520,apx=SCREEN_W/2-260,apy=SCREEN_H/2-220,aph=440;
                SDL_Rect btn_c={apx+apw/2-90,apy+aph-52,180,40};
                fake.type=SDL_MOUSEBUTTONDOWN; fake.button.button=SDL_BUTTON_LEFT;
                fake.button.x=btn_c.x+btn_c.w/2; fake.button.y=btn_c.y+btn_c.h/2;
                SDL_PushEvent(&fake);
            }
            return 0;
        }

        /* Saisie normale dans les champs */
        if(auth->mode==0){champ_input(&auth->champ_user,k,t);champ_input(&auth->champ_pass,k,t);}
        else if(auth->mode==1){champ_input(&auth->reg_nom,k,t);champ_input(&auth->reg_user,k,t);champ_input(&auth->reg_pass,k,t);}
    }
    return 0;
}

// ════════════════════════════════════════════════════════════════
//  RENDU PARAMETRES
// ════════════════════════════════════════════════════════════════
void rendu_parametres(SDL_Renderer* ren, EcranParametres* prm,
                      TTF_Font* fg, TTF_Font* fm, TTF_Font* fp,
                      BaseAuth* base, int mx, int my) {
    // Fond sombre
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,200);
    SDL_Rect fond={0,0,SCREEN_W,SCREEN_H}; SDL_RenderFillRect(ren,&fond);

    int pw=480,ph=520,ppx=SCREEN_W/2-pw/2,ppy=SCREEN_H/2-ph/2;

    SDL_SetRenderDrawColor(ren,12,12,30,245);
    SDL_Rect pan={ppx,ppy,pw,ph}; SDL_RenderFillRect(ren,&pan);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,240,192,64,255);
    SDL_RenderDrawRect(ren,&pan);
    SDL_Rect bande={ppx,ppy,pw,6}; SDL_RenderFillRect(ren,&bande);

    SDL_Color jaune={240,192,64,255},blanc={255,255,255,255};
    SDL_Color gris={155,155,165,255},rouge={226,100,100,255};
    SDL_Color vert={80,200,120,255},noir={10,8,0,255};

    // Flèche retour
    int fleche_x=ppx+30, fleche_y=ppy+35;
    int fleche_surv=clic_btn_retour_fleche(fleche_x,fleche_y,mx,my);
    dessiner_btn_retour_fleche(ren,fp,fleche_x,fleche_y,fleche_surv);
    SDL_Color gh={120,120,140,255};
    dessiner_texte(ren,fp,"Retour au menu",gh,ppx+60,ppy+28,0);

    // Titre
    dessiner_texte(ren,fg,"Parametres",jaune,SCREEN_W/2,ppy+18,1);

    SDL_SetRenderDrawColor(ren,60,60,80,255);
    SDL_RenderDrawLine(ren,ppx+20,ppy+65,ppx+pw-20,ppy+65);

    if(base->user_connecte>=0){
        Utilisateur* u=&base->users[base->user_connecte];

        // ── Page 0 : infos ────────────────────────────────────
        if(prm->page==0){
            // Avatar / photo
            int av_cx=SCREEN_W/2, av_cy=ppy+130, av_r=50;
            if(u->a_photo&&g_photo_tex&&g_photo_user==base->user_connecte){
                dessiner_photo_cercle(ren,g_photo_tex,av_cx,av_cy,av_r);
                dessiner_cercle_contour(ren,av_cx,av_cy,av_r,jaune);
            } else {
                char ini[2]={u->nom_complet[0],'\0'};
                dessiner_avatar(ren,fm,av_cx,av_cy,av_r,u->avatar_id,ini);
            }

            // Infos
            SDL_Color gris2={155,155,165,255};
            char ligne[128];

            snprintf(ligne,sizeof(ligne),"Nom : %s",u->nom_complet);
            dessiner_texte(ren,fm,ligne,blanc,SCREEN_W/2,ppy+200,1);

            snprintf(ligne,sizeof(ligne),"Identifiant : %s",u->username);
            dessiner_texte(ren,fp,ligne,gris2,SCREEN_W/2,ppy+235,1);

            snprintf(ligne,sizeof(ligne),"Role : %s",u->role);
            dessiner_texte(ren,fp,ligne,gris2,SCREEN_W/2,ppy+258,1);

            snprintf(ligne,sizeof(ligne),"Sessions : %d",u->nb_connexions);
            dessiner_texte(ren,fp,ligne,gris2,SCREEN_W/2,ppy+281,1);

            snprintf(ligne,sizeof(ligne),"Derniere connexion : %s",u->derniere_connexion);
            dessiner_texte(ren,fp,ligne,gris2,SCREEN_W/2,ppy+304,1);

            // Barres scores
            dessiner_texte(ren,fp,"Theorique :",gris2,ppx+40,ppy+335,0);
            SDL_Rect bt={ppx+140,ppy+338,pw-180,10};
            SDL_SetRenderDrawColor(ren,40,40,55,255); SDL_RenderFillRect(ren,&bt);
            int lth=(u->score_theo*(pw-180))/100;
            if(lth>0){SDL_Rect r2={ppx+140,ppy+338,lth,10};SDL_SetRenderDrawColor(ren,29,158,117,255);SDL_RenderFillRect(ren,&r2);}
            char sth[8]; snprintf(sth,8,"%d%%",u->score_theo);
            dessiner_texte(ren,fp,sth,jaune,ppx+pw-35,ppy+332,0);

            dessiner_texte(ren,fp,"Pratique :",gris2,ppx+40,ppy+360,0);
            SDL_Rect bp={ppx+140,ppy+363,pw-180,10};
            SDL_SetRenderDrawColor(ren,40,40,55,255); SDL_RenderFillRect(ren,&bp);
            int lpr=(u->score_prat*(pw-180))/100;
            if(lpr>0){SDL_Rect r2={ppx+140,ppy+363,lpr,10};SDL_SetRenderDrawColor(ren,100,149,237,255);SDL_RenderFillRect(ren,&r2);}
            char spr[8]; snprintf(spr,8,"%d%%",u->score_prat);
            dessiner_texte(ren,fp,spr,jaune,ppx+pw-35,ppy+357,0);

            // Bouton Modifier
            SDL_Rect btn_mod={ppx+pw/2-80,ppy+ph-65,160,44};
            SDL_Point souris={mx,my};
            int surv=SDL_PointInRect(&souris,&btn_mod);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,240,192,64,surv?220:160);
            SDL_RenderFillRect(ren,&btn_mod);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(ren,240,192,64,255);
            SDL_RenderDrawRect(ren,&btn_mod);
            dessiner_texte(ren,fm,"Modifier mes infos",noir,
                           btn_mod.x+btn_mod.w/2,btn_mod.y+12,1);
        }

        // ── Page 1 : modifier ─────────────────────────────────
        else {
            dessiner_texte(ren,fm,"Modifier mes informations",jaune,SCREEN_W/2,ppy+80,1);
            dessiner_texte(ren,fp,"Laissez vide pour ne pas modifier",gris,SCREEN_W/2,ppy+108,1);

            dessiner_champ(ren,fm,fp,&prm->ch_nom,    "NOUVEAU NOM COMPLET");
            dessiner_champ(ren,fm,fp,&prm->ch_mdp,    "NOUVEAU MOT DE PASSE");
            dessiner_champ(ren,fm,fp,&prm->ch_confirm,"CONFIRMER MOT DE PASSE");

            if(prm->ch_mdp.longueur>0)
                dessiner_force_mdp(ren,fp,prm->ch_mdp.texte,
                                   prm->ch_mdp.rect.x,
                                   prm->ch_mdp.rect.y+prm->ch_mdp.rect.h+5,
                                   prm->ch_mdp.rect.w-80);

            // Bouton Changer photo
            SDL_Rect btn_photo={ppx+pw/2-90,ppy+ph-120,180,36};
            { SDL_Point sp2={mx,my}; int sv2=SDL_PointInRect(&sp2,&btn_photo);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,80,60,160,sv2?220:160);
            SDL_RenderFillRect(ren,&btn_photo);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(ren,120,90,220,255);
            SDL_RenderDrawRect(ren,&btn_photo);
            dessiner_texte(ren,fp,"Changer ma photo",blanc,
                           btn_photo.x+btn_photo.w/2,btn_photo.y+10,1); }

            // Bouton Sauvegarder
            SDL_Rect btn_sav={ppx+pw/2-90,ppy+ph-70,180,44};
            SDL_Point souris={mx,my};
            int surv=SDL_PointInRect(&souris,&btn_sav);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,29,158,117,surv?220:160);
            SDL_RenderFillRect(ren,&btn_sav);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(ren,29,200,117,255);
            SDL_RenderDrawRect(ren,&btn_sav);
            dessiner_texte(ren,fm,"Sauvegarder",blanc,
                           btn_sav.x+btn_sav.w/2,btn_sav.y+12,1);

            if(prm->message[0]){
                SDL_Color coul=prm->msg_ok?vert:rouge;
                dessiner_texte(ren,fp,prm->message,coul,SCREEN_W/2,ppy+ph-15,1);
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════
//  GESTION EVENTS PARAMETRES
// ════════════════════════════════════════════════════════════════
int param_handle_event(SDL_Event* ev, EcranParametres* prm,
                       BaseAuth* base, int mx, int my,
                       EtatApp* etat, SDL_Renderer* ren) {
    int pw=480,ph=520,ppx=SCREEN_W/2-pw/2,ppy=SCREEN_H/2-ph/2;
    int fleche_x=ppx+30, fleche_y=ppy+35;

    ChampTexte* chp[]={&prm->ch_nom,&prm->ch_mdp,&prm->ch_confirm};

    if(ev->type==SDL_MOUSEBUTTONDOWN&&ev->button.button==SDL_BUTTON_LEFT){
        SDL_Point p={mx,my};

        // Flèche retour
        if(clic_btn_retour_fleche(fleche_x,fleche_y,mx,my)){
            if(prm->page==1) prm->page=0;
            else *etat=ETAT_MENU;
            return 0;
        }

        if(prm->page==0){
            // Bouton modifier
            SDL_Rect btn_mod={ppx+pw/2-80,ppy+ph-65,160,44};
            if(SDL_PointInRect(&p,&btn_mod)) prm->page=1;
        } else {
            // Activation champs
            activer_champs(chp,3,mx,my);

            // Clic bouton Changer photo
            SDL_Rect btn_photo={ppx+pw/2-90,ppy+ph-120,180,36};
            if(SDL_PointInRect(&p,&btn_photo)){
                char chemin[512]={0};
                if(ouvrir_explorateur_photo(chemin,512)){
                    Utilisateur *u=&base->users[base->user_connecte];
                    CreateDirectoryA("photos",NULL);
                    const char *dot=strrchr(chemin,'.');
                    char ext[8]=".bmp"; if(dot) strncpy(ext,dot,7);
                    char dest[512];
                    snprintf(dest,sizeof(dest),"photos\\%s_profil%s",u->username,ext);
                    FILE *fin=fopen(chemin,"rb"); FILE *fout=fopen(dest,"wb");
                    if(fin&&fout){unsigned char buf[4096];size_t n;
                        while((n=fread(buf,1,sizeof(buf),fin))>0) fwrite(buf,1,n,fout);}
                    if(fin)fclose(fin); if(fout)fclose(fout);
                    strncpy(u->photo_path,dest,MAX_PATH_PHOTO-1); u->a_photo=1;
                    auth_sauvegarder(base);
                    if(g_photo_tex){SDL_DestroyTexture(g_photo_tex);g_photo_tex=NULL;}
                    g_photo_tex=charger_texture_chemin(ren,dest);
                    g_photo_user=base->user_connecte;
                    snprintf(prm->message,256,"Photo mise a jour !"); prm->msg_ok=1;
                } return 0;
            }

            // Clic œil mdp
            if(clic_oeil(&prm->ch_mdp,mx,my))
                prm->ch_mdp.mdp_visible=!prm->ch_mdp.mdp_visible;
            if(clic_oeil(&prm->ch_confirm,mx,my))
                prm->ch_confirm.mdp_visible=!prm->ch_confirm.mdp_visible;

            // Bouton sauvegarder
            SDL_Rect btn_sav={ppx+pw/2-90,ppy+ph-70,180,44};
            if(SDL_PointInRect(&p,&btn_sav)){
                Utilisateur* u=&base->users[base->user_connecte];
                int modif=0;

                // Modifier nom
                if(prm->ch_nom.longueur>0){
                    strncpy(u->nom_complet,prm->ch_nom.texte,MAX_STR-1);
                    modif=1;
                }
                // Modifier mdp
                if(prm->ch_mdp.longueur>0){
                    if(strcmp(prm->ch_mdp.texte,prm->ch_confirm.texte)!=0){
                        snprintf(prm->message,256,"Les mots de passe ne correspondent pas.");
                        prm->msg_ok=0; return 0;
                    }
                    if(prm->ch_mdp.longueur<6){
                        snprintf(prm->message,256,"Mot de passe trop court (min 6 caracteres).");
                        prm->msg_ok=0; return 0;
                    }
                    hasher_mdp(prm->ch_mdp.texte,u->password);
                    modif=1;
                }
                if(modif){
                    auth_sauvegarder(base);
                    snprintf(prm->message,256,"Informations mises a jour !");
                    prm->msg_ok=1;
                    // Remet les champs à zéro
                    memset(prm->ch_mdp.texte,0,MAX_INPUT+1);
                    prm->ch_mdp.longueur=0;
                    memset(prm->ch_confirm.texte,0,MAX_INPUT+1);
                    prm->ch_confirm.longueur=0;
                    // Mise à jour nom dans champ
                    strncpy(prm->ch_nom.texte,u->nom_complet,MAX_INPUT);
                    prm->ch_nom.longueur=strlen(prm->ch_nom.texte);
                } else {
                    snprintf(prm->message,256,"Aucune modification effectuee.");
                    prm->msg_ok=0;
                }
            }
        }
    }

    if(ev->type==SDL_KEYDOWN||ev->type==SDL_TEXTINPUT){
        if(prm->page==1){
            SDL_Keycode k=(ev->type==SDL_KEYDOWN)?ev->key.keysym.sym:0;
            const char* t=(ev->type==SDL_TEXTINPUT)?ev->text.text:NULL;
            champ_input(&prm->ch_nom,    k,t);
            champ_input(&prm->ch_mdp,    k,t);
            champ_input(&prm->ch_confirm,k,t);
        }
    }
    return 0;
}

// ════════════════════════════════════════════════════════════════
//  RENDU MENU
// ════════════════════════════════════════════════════════════════
static void calc_rects_menu(Bouton* boutons, Bouton* btn_retour){
    int larg_b=230,haut_b=210,total_b=larg_b*3+40;
    int sxb=(SCREEN_W-total_b)/2, yb=SCREEN_H/2-80;
    boutons[0].rect=(SDL_Rect){sxb,              yb,larg_b,haut_b};
    boutons[1].rect=(SDL_Rect){sxb+larg_b+20,    yb,larg_b,haut_b};
    boutons[2].rect=(SDL_Rect){sxb+larg_b*2+40,  yb,larg_b,haut_b};
    btn_retour->rect=(SDL_Rect){SCREEN_W/2-90,SCREEN_H-130,180,50};
}

void rendu_menu(SDL_Renderer* ren, SDL_Texture* tex_fond,
                TTF_Font* fg, TTF_Font* fm, TTF_Font* fp,
                Bouton* boutons, int nb, Bouton* btn_retour,
                BaseAuth* base, int mx, int my) {

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    if(tex_fond){SDL_Rect d={0,0,SCREEN_W,SCREEN_H};SDL_RenderCopy(ren,tex_fond,NULL,&d);}
    else{SDL_SetRenderDrawColor(ren,20,20,50,255);SDL_RenderClear(ren);}
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,140);
    SDL_Rect pl={0,0,SCREEN_W,SCREEN_H}; SDL_RenderFillRect(ren,&pl);

    SDL_Color jaune={240,192,64,255},blanc={255,255,255,255},gris={190,190,190,255};
    dessiner_texte(ren,fg,"Menu Principal",                    jaune,SCREEN_W/2,30,1);
    dessiner_texte(ren,fp,"Choisissez une section ci-dessous", gris, SCREEN_W/2,80,1);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,240,192,64,255);
    SDL_RenderDrawLine(ren,SCREEN_W/2-140,115,SCREEN_W/2+140,115);

    // Boutons sections — rects déjà calculés par calc_rects_menu()

    for(int i=0;i<nb;i++){
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,boutons[i].survol?240:255,boutons[i].survol?192:255,
                               boutons[i].survol?64:255,boutons[i].survol?70:35);
        SDL_RenderFillRect(ren,&boutons[i].rect);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,boutons[i].survol?240:210,boutons[i].survol?192:210,
                               boutons[i].survol?64:210,255);
        SDL_RenderDrawRect(ren,&boutons[i].rect);
        int cx=boutons[i].rect.x+boutons[i].rect.w/2,cy=boutons[i].rect.y+boutons[i].rect.h/2;
        SDL_Color coul=boutons[i].survol?jaune:blanc;
        dessiner_texte(ren,fm,boutons[i].titre,     coul,cx,cy-28,1);
        dessiner_texte(ren,fp,boutons[i].sous_titre,gris,cx,cy+10,1);
    }

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,btn_retour->survol?200:160,btn_retour->survol?50:30,
                           btn_retour->survol?50:30,btn_retour->survol?190:130);
    SDL_RenderFillRect(ren,&btn_retour->rect);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren,255,100,100,255); SDL_RenderDrawRect(ren,&btn_retour->rect);
    SDL_Color rouge2={255,130,130,255};
    dessiner_texte(ren,fm,"<-- Retour",rouge2,
                   btn_retour->rect.x+btn_retour->rect.w/2,btn_retour->rect.y+13,1);

    // ── CARTE JOUEUR ─────────────────────────────────────────
    if(base->user_connecte>=0){
        Utilisateur* u=&base->users[base->user_connecte];
        int cw=215,ch=270,cx_c=SCREEN_W-cw-10,cy_c=SCREEN_H-ch-46;

        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,6,6,20,225);
        SDL_Rect card={cx_c,cy_c,cw,ch}; SDL_RenderFillRect(ren,&card);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,240,192,64,255); SDL_RenderDrawRect(ren,&card);
        SDL_Rect bcard={cx_c,cy_c,cw,22}; SDL_RenderFillRect(ren,&bcard);
        SDL_Color noir={10,8,0,255};
        dessiner_texte(ren,fp,"PROFIL CONDUCTEUR",noir,cx_c+cw/2,cy_c+4,1);

        // ── Icône engrenage (paramètres) ──────────────────────
        int gear_cx=cx_c+cw-14, gear_cy=cy_c+11;
        SDL_Rect gear_zone={cx_c+cw-28,cy_c+2,26,20};
        SDL_Point souris={mx,my};
        int gear_surv=SDL_PointInRect(&souris,&gear_zone);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,gear_surv?240:160,gear_surv?192:160,gear_surv?64:160,200);
        SDL_RenderFillRect(ren,&gear_zone);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        // Dessin engrenage simple (cercles + dents)
        SDL_SetRenderDrawColor(ren,gear_surv?255:200,gear_surv?220:200,gear_surv?80:200,255);
        for(int deg=0;deg<360;deg+=45){
            double rad=deg*3.14159265/180.0;
            SDL_RenderDrawPoint(ren,gear_cx+(int)(7*cos(rad)),gear_cy+(int)(7*sin(rad)));
            SDL_RenderDrawPoint(ren,gear_cx+(int)(6*cos(rad)),gear_cy+(int)(6*sin(rad)));
        }
        // Cercle central
        for(int deg=0;deg<360;deg++){
            double rad=deg*3.14159265/180.0;
            SDL_RenderDrawPoint(ren,gear_cx+(int)(3*cos(rad)),gear_cy+(int)(3*sin(rad)));
        }
        // Tooltip
        if(gear_surv){
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,30,30,50,220);
            SDL_Rect tip={cx_c-60,cy_c,80,20}; SDL_RenderFillRect(ren,&tip);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            SDL_Color tj={240,192,64,255};
            dessiner_texte(ren,fp,"Parametres",tj,cx_c-20,cy_c+3,1);
        }

        // Avatar
        int av_cx=cx_c+cw/2,av_cy=cy_c+72,av_r=30;
        if(u->a_photo&&g_photo_tex&&g_photo_user==base->user_connecte){
            dessiner_photo_cercle(ren,g_photo_tex,av_cx,av_cy,av_r);
            dessiner_cercle_contour(ren,av_cx,av_cy,av_r,jaune);
        } else {
            char ini[2]={u->nom_complet[0],'\0'};
            dessiner_avatar(ren,fm,av_cx,av_cy,av_r,u->avatar_id,ini);
        }

        char nom20[22]={0}; strncpy(nom20,u->nom_complet,20);
        dessiner_texte(ren,fp,nom20,blanc,cx_c+cw/2,cy_c+110,1);

        SDL_SetRenderDrawColor(ren,60,60,80,255);
        SDL_RenderDrawLine(ren,cx_c+10,cy_c+126,cx_c+cw-10,cy_c+126);

        SDL_Color gris2={155,155,165,255};
        dessiner_texte(ren,fp,"Derniere co.:",gris2,cx_c+8,cy_c+132,0);
        char dc[22]={0}; strncpy(dc,u->derniere_connexion,20);
        dessiner_texte(ren,fp,dc,blanc,cx_c+8,cy_c+146,0);

        dessiner_texte(ren,fp,"Theorique :",gris2,cx_c+8,cy_c+164,0);
        SDL_Rect bt_f={cx_c+8,cy_c+178,cw-16,8};
        SDL_SetRenderDrawColor(ren,40,40,55,255); SDL_RenderFillRect(ren,&bt_f);
        int lth=(u->score_theo*(cw-16))/100;
        if(lth>0){SDL_Rect r2={cx_c+8,cy_c+178,lth,8};SDL_SetRenderDrawColor(ren,29,158,117,255);SDL_RenderFillRect(ren,&r2);}
        char sth[8]; snprintf(sth,8,"%d%%",u->score_theo);
        dessiner_texte(ren,fp,sth,jaune,cx_c+cw-30,cy_c+162,0);

        dessiner_texte(ren,fp,"Pratique :",gris2,cx_c+8,cy_c+192,0);
        SDL_Rect bp_f={cx_c+8,cy_c+206,cw-16,8};
        SDL_SetRenderDrawColor(ren,40,40,55,255); SDL_RenderFillRect(ren,&bp_f);
        int lpr=(u->score_prat*(cw-16))/100;
        if(lpr>0){SDL_Rect r2={cx_c+8,cy_c+206,lpr,8};SDL_SetRenderDrawColor(ren,100,149,237,255);SDL_RenderFillRect(ren,&r2);}
        char spr[8]; snprintf(spr,8,"%d%%",u->score_prat);
        dessiner_texte(ren,fp,spr,jaune,cx_c+cw-30,cy_c+190,0);

        char sess[32]; snprintf(sess,32,"Sessions : %d",u->nb_connexions);
        SDL_Color gris3={140,140,150,255};
        dessiner_texte(ren,fp,sess,gris3,cx_c+8,cy_c+224,0);

        SDL_Color bc=u->a_photo?(SDL_Color){29,158,117,255}:(SDL_Color){100,149,237,255};
        SDL_Rect badge={cx_c+cw-68,cy_c+24,60,16};
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,bc.r,bc.g,bc.b,180); SDL_RenderFillRect(ren,&badge);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        dessiner_texte(ren,fp,u->a_photo?"PHOTO":"AVATAR",blanc,cx_c+cw-38,cy_c+26,1);

        // Astuce
        Uint32 idx=(SDL_GetTicks()/8000)%NB_ASTUCES;
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,0,0,0,170);
        SDL_Rect ast={0,SCREEN_H-36,SCREEN_W,36}; SDL_RenderFillRect(ren,&ast);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren,240,192,64,255);
        SDL_RenderDrawLine(ren,0,SCREEN_H-36,SCREEN_W,SCREEN_H-36);
        dessiner_texte(ren,fp,"Astuce :",jaune,10,SCREEN_H-26,0);
        SDL_Color blanc3={220,220,220,255};
        dessiner_texte(ren,fp,ASTUCES[idx],blanc3,82,SCREEN_H-26,0);
    }
}

// ════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════
/* ── Prototypes modules ── */
int run_simulation(SDL_Window *win, SDL_Renderer *ren, int *score_out);
int run_guide(SDL_Window *win, SDL_Renderer *ren);
int run_qcm(SDL_Window *win, SDL_Renderer *ren, int *score_out, const char *username);

int main(int argc, char* argv[]) {
    fixer_repertoire();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0){printf("[ERREUR SDL] %s\n",SDL_GetError());return 1;}
    if(!(IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG)&(IMG_INIT_PNG|IMG_INIT_JPG))){printf("[ERREUR IMG] %s\n",IMG_GetError());return 1;}
    if(TTF_Init()!=0){printf("[ERREUR TTF] %s\n",TTF_GetError());return 1;}
    SDL_StartTextInput();

    SDL_Window* fen=SDL_CreateWindow(
        "DriveSix — Simulateur Auto-Ecole",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        1280,720,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_MAXIMIZED);
    SDL_Renderer* ren=SDL_CreateRenderer(fen,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    /* Forcer la résolution réelle de la fenêtre (maximisée au démarrage) */
    SDL_GetRendererOutputSize(ren,&SCREEN_W,&SCREEN_H);
    printf("[INFO] Resolution : %dx%d\n",SCREEN_W,SCREEN_H);
    /* Recalculer après un court délai pour que la maximisation soit effective */
    SDL_Delay(50);
    SDL_GetRendererOutputSize(ren,&SCREEN_W,&SCREEN_H);
    printf("[INFO] Resolution reelle : %dx%d\n",SCREEN_W,SCREEN_H);

    TTF_Font* fg=TTF_OpenFont("DejaVuSans.ttf",30);
    TTF_Font* fm=TTF_OpenFont("DejaVuSans.ttf",18);
    TTF_Font* fp=TTF_OpenFont("DejaVuSans.ttf",13);
    if(!fg) printf("[ERREUR POLICE] %s\n",TTF_GetError());

    SDL_Texture* tex_fond =charger_texture(ren,"vrai.png");
    SDL_Texture* tex_intro=charger_texture(ren,"intro.png");

    VideoPlayer video;
    char chemin_video[512];
    GetCurrentDirectoryA(512,chemin_video);
    strncat(chemin_video,"\\intro.mp4",sizeof(chemin_video)-strlen(chemin_video)-1);
    int video_ok=video_init(&video,ren,chemin_video);
    if(!video_ok) printf("[WARN] intro.mp4 non trouvee\n");

    BaseAuth base; auth_charger(&base);
    EcranAuth auth; reinit_auth(&auth,&base);
    EcranParametres prm; memset(&prm,0,sizeof(prm));

    Bouton boutons[3]={
        {{0,0,230,210},"Partie Theorique","Code de la route",   0},
        {{0,0,230,210},"Partie Pratique", "Simulation conduite",0},
        {{0,0,230,210},"Partie Consigne", "Regles & securite",  0}
    };
    Bouton btn_retour={{SCREEN_W/2-90,SCREEN_H-130,180,50},"Retour","",0};

    EtatApp  etat     =video_ok?ETAT_VIDEO:ETAT_CHARGEMENT;
    Uint32   tps_debut=SDL_GetTicks();
    SDL_bool quitter  =SDL_FALSE;
    SDL_Event ev;
    char msg_bvn[256]={0};

    calc_rects_menu(boutons,&btn_retour);

    while(!quitter){
        int mx,my; SDL_GetMouseState(&mx,&my);

        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) quitter=SDL_TRUE;

            // Redimensionnement
            if(ev.type==SDL_WINDOWEVENT){
                if(ev.window.event==SDL_WINDOWEVENT_RESIZED   ||
                   ev.window.event==SDL_WINDOWEVENT_MAXIMIZED ||
                   ev.window.event==SDL_WINDOWEVENT_RESTORED){
                    SDL_GetRendererOutputSize(ren,&SCREEN_W,&SCREEN_H);
                    calc_rects_menu(boutons,&btn_retour);
                    reinit_auth(&auth,&base);
                    btn_retour.rect=(SDL_Rect){SCREEN_W/2-90,SCREEN_H-130,180,50};
                }
            }

            // Skip vidéo
            if(etat==ETAT_VIDEO&&ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_SPACE||
                   ev.key.keysym.sym==SDLK_RETURN||
                   ev.key.keysym.sym==SDLK_ESCAPE){
                    video_free(&video);
                    etat=ETAT_CHARGEMENT; tps_debut=SDL_GetTicks();
                }
            }

            if(etat==ETAT_AUTH){
                int ok=auth_handle_event(&ev,&auth,mx,my,&etat,ren);
                if(ok){
                    auth_message_bienvenue(&base,msg_bvn,sizeof(msg_bvn));
                    if(g_photo_tex){SDL_DestroyTexture(g_photo_tex);g_photo_tex=NULL;}
                    Utilisateur* u=&base.users[base.user_connecte];
                    if(u->a_photo&&u->photo_path[0]){
                        g_photo_tex=charger_texture_chemin(ren,u->photo_path);
                        g_photo_user=base.user_connecte;
                    }
                }
            }
            else if(etat==ETAT_MENU){
                /* Navigation clavier menu */
                if(ev.type==SDL_KEYDOWN){
                    SDL_Keycode km=ev.key.keysym.sym;
                    /* Compter combien de boutons sont "actifs" = 3 + retour */
                    static int menu_sel=-1; /* -1=aucun, 0-2=bouton, 3=retour */
                    if(km==SDLK_RIGHT||km==SDLK_TAB){
                        menu_sel=(menu_sel+1)%4;
                        for(int i=0;i<3;i++) boutons[i].survol=(menu_sel==i);
                        btn_retour.survol=(menu_sel==3);
                    } else if(km==SDLK_LEFT){
                        menu_sel=(menu_sel+3)%4;
                        for(int i=0;i<3;i++) boutons[i].survol=(menu_sel==i);
                        btn_retour.survol=(menu_sel==3);
                    } else if(km==SDLK_RETURN||km==SDLK_KP_ENTER){
                        if(menu_sel==3&&btn_retour.survol){
                            /* Retour = déconnexion */
                            if(g_photo_tex){SDL_DestroyTexture(g_photo_tex);g_photo_tex=NULL;g_photo_user=-1;}
                            base.user_connecte=-1; etat=ETAT_AUTH;
                            reinit_auth(&auth,&base); msg_bvn[0]=0; menu_sel=-1;
                        } else if(menu_sel>=0&&menu_sel<3&&boutons[menu_sel].survol){
                            /* Simuler clic sur le bouton sélectionné */
                            SDL_Event fake; memset(&fake,0,sizeof(fake));
                            fake.type=SDL_MOUSEBUTTONDOWN;
                            fake.button.button=SDL_BUTTON_LEFT;
                            fake.button.x=boutons[menu_sel].rect.x+boutons[menu_sel].rect.w/2;
                            fake.button.y=boutons[menu_sel].rect.y+boutons[menu_sel].rect.h/2;
                            SDL_PushEvent(&fake);
                        }
                    }
                }
                if(ev.type==SDL_MOUSEBUTTONDOWN&&ev.button.button==SDL_BUTTON_LEFT){
                    // Clic engrenage (paramètres)
                    if(base.user_connecte>=0){
                        int cw=215,cx_c=SCREEN_W-cw-10,cy_c=SCREEN_H-270-46;
                        SDL_Rect gear_zone={cx_c+cw-28,cy_c+2,26,20};
                        SDL_Point pp={mx,my};
                        if(SDL_PointInRect(&pp,&gear_zone)){
                            reinit_parametres(&prm,&base);
                            etat=ETAT_PARAMETRES;
                        }
                    }
                    if(btn_retour.survol){
                        if(g_photo_tex){SDL_DestroyTexture(g_photo_tex);g_photo_tex=NULL;g_photo_user=-1;}
                        base.user_connecte=-1; etat=ETAT_AUTH;
                        reinit_auth(&auth,&base); msg_bvn[0]=0;
                    }
                    for(int i=0;i<3;i++){
                        if(boutons[i].survol){
                            printf("[CLIC] %s\n",boutons[i].titre);
                            int score_result = 0;
                            if(i==0){
                                run_qcm(fen, ren, &score_result,
                                    (base.user_connecte>=0) ? base.users[base.user_connecte].username : "");
                                if(base.user_connecte>=0){
                                    char savefile[128]="save.dat";
                                    snprintf(savefile,sizeof(savefile),"save_%s.dat",
                                             base.users[base.user_connecte].username);
                                    int unlocked=1;
                                    FILE *fsav=fopen(savefile,"r");
                                    if(fsav){char ln[64];
                                        while(fgets(ln,sizeof(ln),fsav)){
                                            int v; if(sscanf(ln,"unlocked=%d",&v)==1) unlocked=v;
                                        } fclose(fsav);}
                                    int pct=((unlocked-1)*100)/10;
                                    if(pct>100) pct=100;
                                    base.users[base.user_connecte].score_theo=pct;
                                    auth_sauvegarder(&base);
                                }
                            } else if(i==1){
                                run_simulation(fen, ren, &score_result);
                                if(base.user_connecte>=0){
                                    base.users[base.user_connecte].score_prat=score_result;
                                    auth_sauvegarder(&base);
                                }
                            } else if(i==2){
                                run_guide(fen, ren);
                            }
                            /* ── Nettoyage complet après sortie module ── */
                            SDL_SetWindowFullscreen(fen, 0);
                            SDL_RenderSetLogicalSize(ren, 0, 0);
                            SDL_RenderSetViewport(ren, NULL);
                            SDL_RenderSetClipRect(ren, NULL);
                            SDL_SetRenderDrawColor(ren,0,0,0,255);
                            SDL_RenderClear(ren);
                            SDL_RenderPresent(ren);
                            SDL_RaiseWindow(fen);
                            SDL_GetRendererOutputSize(ren,&SCREEN_W,&SCREEN_H);
                            calc_rects_menu(boutons,&btn_retour);
                            reinit_auth(&auth,&base);
                            /* Recharger photo profil si nécessaire */
                            if(base.user_connecte>=0){
                                Utilisateur *ur=&base.users[base.user_connecte];
                                if(ur->a_photo && ur->photo_path[0]){
                                    if(g_photo_tex){SDL_DestroyTexture(g_photo_tex);g_photo_tex=NULL;}
                                    g_photo_tex=charger_texture_chemin(ren,ur->photo_path);
                                    g_photo_user=base.user_connecte;
                                }
                            }
                        }
                    }
                }
            }
            else if(etat==ETAT_PARAMETRES){
                param_handle_event(&ev,&prm,&base,mx,my,&etat,ren);
                // Si on recharge la photo après modif
                if(base.user_connecte>=0){
                    Utilisateur* u=&base.users[base.user_connecte];
                    if(u->a_photo&&u->photo_path[0]&&g_photo_user!=base.user_connecte){
                        if(g_photo_tex) SDL_DestroyTexture(g_photo_tex);
                        g_photo_tex=charger_texture_chemin(ren,u->photo_path);
                        g_photo_user=base.user_connecte;
                    }
                }
            }
        }

        /* Mise à jour survol boutons menu — à chaque frame, pas seulement sur événement */
        if(etat==ETAT_MENU){
            SDL_Point s_frame={mx,my};
            for(int i=0;i<3;i++) boutons[i].survol=SDL_PointInRect(&s_frame,&boutons[i].rect);
            btn_retour.survol=SDL_PointInRect(&s_frame,&btn_retour.rect);
        }

        if(etat==ETAT_CHARGEMENT&&SDL_GetTicks()-tps_debut>=LOADING_DURATION)
            etat=ETAT_AUTH;

        // Rendu
        /* Toujours s'assurer que SCREEN_W/H = taille réelle du renderer */
        SDL_GetRendererOutputSize(ren,&SCREEN_W,&SCREEN_H);
        if(etat==ETAT_VIDEO){
            SDL_RenderSetLogicalSize(ren,0,0);
            SDL_RenderSetViewport(ren,NULL);
            SDL_SetRenderDrawColor(ren,0,0,0,255);
            SDL_RenderClear(ren);
            int ok=video_update(&video,ren,SCREEN_W,SCREEN_H);
            if(!ok||video.termine){
                video_free(&video); etat=ETAT_CHARGEMENT; tps_debut=SDL_GetTicks();
            } else { rendu_video_overlay(ren,fp); }
        }
        else if(etat==ETAT_CHARGEMENT)
            rendu_chargement(ren,tex_intro,fg,fm,fp,tps_debut,msg_bvn);
        else if(etat==ETAT_AUTH)
            rendu_auth(ren,&auth,fg,fm,fp,mx,my);
        else if(etat==ETAT_MENU)
            rendu_menu(ren,tex_fond,fg,fm,fp,boutons,3,&btn_retour,&base,mx,my);
        else if(etat==ETAT_PARAMETRES){
            // Affiche le menu en dessous
            rendu_menu(ren,tex_fond,fg,fm,fp,boutons,3,&btn_retour,&base,mx,my);
            // Puis le panneau paramètres par-dessus
            rendu_parametres(ren,&prm,fg,fm,fp,&base,mx,my);
        }

        SDL_RenderPresent(ren); SDL_Delay(16);
    }

    SDL_StopTextInput();
    if(video.fmt_ctx) video_free(&video);
    if(g_photo_tex)   SDL_DestroyTexture(g_photo_tex);
    if(tex_intro)     SDL_DestroyTexture(tex_intro);
    if(tex_fond)      SDL_DestroyTexture(tex_fond);
    TTF_CloseFont(fg); TTF_CloseFont(fm); TTF_CloseFont(fp);
    TTF_Quit(); IMG_Quit();
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(fen); SDL_Quit();
    return 0;
}