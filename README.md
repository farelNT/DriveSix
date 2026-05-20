# 🚗 DriveSix — Simulateur Auto-École

<div align="center">

![C](https://img.shields.io/badge/Language-C-blue?style=for-the-badge&logo=c)
![SDL2](https://img.shields.io/badge/SDL2-2.x-green?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey?style=for-the-badge&logo=windows)
![Build](https://img.shields.io/badge/Build-MinGW--w64%20%2F%20MSYS2-orange?style=for-the-badge)

**Application éducative complète de préparation au permis de conduire**  
*Simulation de conduite · QCM théorique · Guide des consignes · Gestion des profils*

</div>

---

## 📋 Table des matières

- [Présentation](#-présentation)
- [Captures d'écran](#-captures-décran)
- [Fonctionnalités](#-fonctionnalités)
- [Architecture du projet](#-architecture-du-projet)
- [Prérequis](#-prérequis)
- [Installation et compilation](#-installation-et-compilation)
- [Structure des fichiers](#-structure-des-fichiers)
- [Modules en détail](#-modules-en-détail)
  - [Authentification & Menu](#-authentification--menu)
  - [Simulation de conduite](#-simulation-de-conduite)
  - [Test théorique QCM](#-test-théorique-qcm)
  - [Guide des consignes](#-guide-des-consignes)
- [Système de progression](#-système-de-progression)
- [Contrôles & Raccourcis](#-contrôles--raccourcis)
- [Données et persistance](#-données-et-persistance)
- [Équipe](#-équipe)

---

## 🎯 Présentation

**DriveSix** est un simulateur d'auto-école développé entièrement en **C** avec la bibliothèque **SDL2**. Il réunit dans une seule application tout ce dont un apprenant a besoin pour préparer son permis de conduire : un environnement de conduite interactif, un système de QCM progressif, un guide des règles du code de la route, et un système de gestion de profils multi-utilisateurs avec suivi de progression.

Le projet a été développé en équipe dans le cadre d'un projet académique, chaque membre prenant en charge un module indépendant, puis intégrés dans une application unifiée à fenêtre unique.

---

## ✨ Fonctionnalités

### Système d'authentification
- ✅ Création de compte avec nom complet, identifiant et mot de passe
- ✅ Connexion sécurisée avec blocage après 5 tentatives échouées
- ✅ Photo de profil personnalisée (importée depuis le PC, copiée dans `photos/`)
- ✅ Modification des informations personnelles depuis les paramètres
- ✅ Jusqu'à **50 utilisateurs** simultanément sur la même machine

### Menu principal
- ✅ Carte de profil avec photo, nom, et barres de progression théorique/pratique
- ✅ Accès aux 3 modules depuis les boutons du menu
- ✅ Navigation complète au clavier (flèches + Entrée)
- ✅ Panneau de paramètres pour modifier son profil et sa photo
- ✅ Astuces de conduite affichées aléatoirement

### Simulation de conduite
- ✅ Deux chapitres de conduite distincts (route nationale + rond-point)
- ✅ Véhicules IA avec comportement autonome
- ✅ Gestion de la vitesse, des infractions, des dépassements
- ✅ Mode jour/nuit avec phares, veilleuses et pleins phares
- ✅ Clignotants, régulateur de vitesse (cruise control)
- ✅ Score de conduite calculé à partir des infractions

### Test théorique (QCM)
- ✅ **10 chapitres** progressifs (déblocage un par un)
- ✅ **200 questions** disponibles (mode examen blanc)
- ✅ Système de déblocage : réussir un chapitre débloque le suivant
- ✅ Sauvegarde de progression **par utilisateur** (`save_[username].dat`)
- ✅ Panneaux de signalisation illustrés
- ✅ Feedback immédiat avec explication des bonnes réponses

### Guide des consignes
- ✅ 5 sections : Présentation, Examen, Simulation, Fonctionnement, Conseils
- ✅ Navigation par pages
- ✅ Plein écran adaptatif

---

## 🏗️ Architecture du projet

```
DriveSix/
│
├── main.c                  ← Point d'entrée unique (24 lignes)
├── test_modifie.c          ← Authentification + Menu principal (~1670 lignes)
├── module_simulation.c     ← Simulation de conduite (~1749 lignes)
├── module_guide.c          ← Guide des consignes (~455 lignes)
├── qcm_wrapper.c           ← Adaptateur QCM (~206 lignes)
├── autoecole_qcm.c         ← Moteur QCM complet (~62900 lignes, NE PAS MODIFIER)
│
├── video.h                 ← Lecteur vidéo FFmpeg (intro.mp4)
│
├── arial.ttf               ← Police principale
├── DejaVuSans.ttf          ← Police secondaire (HUD simulation)
├── vrai.png                ← Icône de validation QCM
├── intro.png               ← Image écran de chargement
├── intro.mp4               ← Vidéo d'introduction (optionnelle)
│
├── SDL2.dll                ┐
├── SDL2_image.dll          │
├── SDL2_ttf.dll            ├─ DLLs SDL2 (Windows)
├── SDL2_mixer.dll          │
├── avcodec-61.dll          │
├── avformat-61.dll         ┘
│
├── users.dat               ← Base d'utilisateurs (généré automatiquement)
├── save_[username].dat     ← Progression QCM par utilisateur (généré auto)
└── photos/                 ← Photos de profil (généré automatiquement)
    └── [username]_profil.ext
```

### Principe d'intégration

Chaque module est une **fenêtre unique partagée** : `main.c` crée une seule fenêtre SDL2 et un seul renderer, puis les passe en paramètre à chaque module au moment de son exécution. Quand un module se termine (Échap ou fermeture), on revient proprement au menu principal sans recréer de fenêtre.

```
main.c
  └── test_modifie.c (auth + menu) ← point d'entrée réel
        ├── run_simulation(win, ren, &score)   → module_simulation.c
        ├── run_qcm(win, ren, &score, username) → qcm_wrapper.c → autoecole_qcm.c
        └── run_guide(win, ren)                → module_guide.c
```

---

## 🔧 Prérequis

### Environnement de développement
- **MSYS2** avec **MinGW-w64** (compilateur GCC ≥ 12)
- Toutes les bibliothèques installables via `pacman` :

```bash
pacman -S mingw-w64-x86_64-SDL2
pacman -S mingw-w64-x86_64-SDL2_image
pacman -S mingw-w64-x86_64-SDL2_ttf
pacman -S mingw-w64-x86_64-SDL2_mixer
pacman -S mingw-w64-x86_64-ffmpeg
```

### Bibliothèques utilisées

| Bibliothèque | Version | Utilisation |
|---|---|---|
| SDL2 | 2.x | Fenêtre, rendu, événements, audio |
| SDL2_image | 2.x | Chargement PNG/JPG (photos profil, panneaux) |
| SDL2_ttf | 2.x | Rendu de texte (toutes les polices) |
| SDL2_mixer | 2.x | Musique et effets sonores (simulation) |
| libavcodec/avformat | FFmpeg | Décodage vidéo intro.mp4 |
| libswscale/swresample | FFmpeg | Conversion pixels/audio pour la vidéo |

---

## 🚀 Installation et compilation

### 1. Cloner le dépôt

```bash
git clone https://github.com/[votre-repo]/DriveSix.git
cd DriveSix
```

### 2. Compiler (une seule commande)

Ouvrir le terminal **MSYS2 MinGW-w64** et exécuter :

```bash
gcc main.c module_simulation.c module_guide.c qcm_wrapper.c -o autoecole.exe \
    -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
    -lavcodec -lavformat -lavutil -lswscale -lswresample \
    -lgdi32 -lcomdlg32 -lm -O2
```

### 3. Lancer l'application

```bash
./autoecole.exe
```

> **Note :** Toutes les DLLs SDL2 et FFmpeg doivent être présentes dans le même dossier que `autoecole.exe`.

---

## 📁 Structure des fichiers

### Fichiers sources

| Fichier | Rôle | Taille |
|---|---|---|
| `main.c` | Point d'entrée, prototypes, inclusion | ~1 KB |
| `test_modifie.c` | Auth, menu, paramètres, rendu UI | ~76 KB |
| `module_simulation.c` | Moteur de conduite 2D complet | ~73 KB |
| `module_guide.c` | Guide des consignes, navigation pages | ~13 KB |
| `qcm_wrapper.c` | Adaptateur QCM (fenêtre unique, save par user) | ~7 KB |
| `autoecole_qcm.c` | Moteur QCM complet avec 50 panneaux BMP | ~5 MB |
| `video.h` | Lecteur vidéo FFmpeg inline | ~5 KB |

### Fichiers générés au runtime

| Fichier | Description |
|---|---|
| `users.dat` | Base de données des utilisateurs (texte brut) |
| `save_[username].dat` | Progression QCM propre à chaque utilisateur |
| `photos/[username]_profil.ext` | Copie locale de la photo de profil |

---

## 📦 Modules en détail

### 🔐 Authentification & Menu

**Fichier :** `test_modifie.c`

Ce module gère l'intégralité du cycle de vie de l'application : vidéo d'intro, écran de chargement, authentification, menu principal, et paramètres.

#### États de l'application

```
ETAT_VIDEO        → Lecture de intro.mp4 en plein écran
ETAT_CHARGEMENT   → Barre de progression + astuces (~25 secondes)
ETAT_AUTH         → Connexion / Inscription / Sélection avatar
ETAT_MENU         → Menu principal avec carte profil
ETAT_PARAMETRES   → Modification des informations personnelles
```

#### Structure d'un utilisateur

```c
typedef struct {
    char username[64];           // Identifiant de connexion
    char password[64];           // Mot de passe
    char nom_complet[64];        // Nom affiché
    char role[64];               // Rôle (élève, moniteur...)
    int  nb_connexions;          // Compteur de connexions
    char derniere_connexion[64]; // Horodatage dernière connexion
    int  score_theo;             // Progression théorique (0-100%)
    int  score_prat;             // Progression pratique (0-100%)
    int  tentatives_echouees;    // Blocage après 5 tentatives
    int  avatar_id;              // Avatar par défaut sélectionné
    char photo_path[512];        // Chemin relatif vers la photo
    int  a_photo;                // 1 si photo personnalisée
} Utilisateur;
```

#### Barres de progression (carte profil)

| Barre | Source de données | Calcul |
|---|---|---|
| Théorique | `save_[username].dat` → `unlocked` | `(unlocked - 1) × 10 %` |
| Pratique | Infractions en simulation | `100 - (nb_infractions × 10) %` |

---

### 🚗 Simulation de conduite

**Fichier :** `module_simulation.c`

Moteur de simulation 2D avec rendu en temps réel. La scène est dessinée en coordonnées logiques **1200×700** et mise à l'échelle automatiquement sur la fenêtre réelle via `SDL_RenderSetLogicalSize`.

#### Chapitre 1 — Route nationale

- Route rectiligne avec voies montantes et descendantes
- Zones de limitation de vitesse (50 km/h, 76 km/h)
- Plusieurs véhicules IA avec comportements autonomes (dépassement, ralentissement)
- Effets visuels : mode nuit, phares, pluie, brouillard
- Système d'infractions comptabilisées

| Paramètre | Valeur |
|---|---|
| Largeur de la route | 600 px (logiques) |
| Position de la route | x = 300 |
| Nombre de voitures IA | Variable selon la zone |
| Taille voiture joueur | 62 × 112 px |

#### Chapitre 4 — Rond-point

- Intersection circulaire avec îlot central
- 4 véhicules IA en circulation autonome sur la trajectoire circulaire
- Gestion de la priorité dans le rond-point
- Compteur de fautes spécifique

| Paramètre | Valeur |
|---|---|
| Rayon îlot central | 90 px |
| Rayon intérieur voie | 130 px |
| Rayon extérieur voie | 230 px |
| Vitesse IA | 2.5 unités/frame |

#### Système de score pratique

```
score_prat = 100 - (nb_infractions × 10)
score_prat ≥ 0 dans tous les cas
```

---

### 📝 Test théorique QCM

**Fichiers :** `autoecole_qcm.c` (moteur) + `qcm_wrapper.c` (adaptateur)

Le moteur QCM est le module le plus volumineux du projet (~5 MB, ~62 900 lignes). Il embarque directement **50 panneaux de signalisation** sous forme de données binaires BMP pour ne dépendre d'aucun fichier image externe.

#### Structure du QCM

```
10 chapitres progressifs
  └── Chaque chapitre = série de questions sur un thème
        └── Réussir un chapitre débloque le suivant
              └── Tous les chapitres réussis → Mode examen (200 questions)
```

#### Système de déblocage

- Au démarrage : seul le **Chapitre 1** est accessible
- Chaque chapitre réussi débloque le suivant
- La progression est sauvegardée dans `save_[username].dat`
- Chaque utilisateur a **sa propre progression indépendante**

```
# Format de save_[username].dat
unlocked=3     # 3 chapitres débloqués
exam=0         # Mode examen pas encore accessible
```

#### Adaptateur `qcm_wrapper.c`

`autoecole_qcm.c` n'est **jamais modifié**. `qcm_wrapper.c` l'inclut via `#include` et :
- Renomme `main()` en `qcm_original_main__unused__` via `#define`
- Expose `run_qcm(win, ren, score_out, username)` à la place
- Redirige `saveProgress()` / `loadProgress()` vers les fichiers par utilisateur
- Utilise la fenêtre et le renderer partagés (pas de création de nouvelle fenêtre)

---

### 📖 Guide des consignes

**Fichier :** `module_guide.c`

Module de consultation des règles du code de la route, organisé en 5 sections naviguables.

#### Sections disponibles

| Section | Contenu |
|---|---|
| **PRÉSENTATION** | Introduction à l'application et ses objectifs |
| **EXAMEN** | Règles du QCM, système de notation |
| **SIMULATION** | Commandes de conduite, règles à respecter |
| **FONCTIONNEMENT** | Comment utiliser les différents modules |
| **CONSEILS** | Astuces pratiques pour l'examen |

Le guide s'adapte à la taille réelle de la fenêtre via `SDL_GetRendererOutputSize` — pas de letterbox, contenu sur tout l'écran.

---

## 📊 Système de progression

La progression de chaque élève est persistée localement et visible sur sa carte de profil :

```
┌─────────────────────────────┐
│  [Photo]  Nom Complet       │
│           @username         │
│                             │
│  Théorique  ████████░░  80% │
│  Pratique   █████░░░░░  50% │
└─────────────────────────────┘
```

- **Barre théorique** : mise à jour après chaque session QCM, reflète le nombre de chapitres débloqués
- **Barre pratique** : mise à jour après chaque session de simulation, calculée sur les infractions commises

---

## ⌨️ Contrôles & Raccourcis

### Menu principal

| Touche | Action |
|---|---|
| `←` / `→` | Naviguer entre les boutons |
| `Entrée` | Valider la sélection |
| `Souris` | Navigation et clic directs |

### Connexion / Inscription

| Touche | Action |
|---|---|
| `↑` / `↓` | Passer au champ précédent / suivant |
| `Tab` | Champ suivant |
| `Entrée` | Valider le formulaire |
| `Backspace` | Effacer un caractère |

### Simulation — Chapitre 1 (Route)

| Touche | Action |
|---|---|
| `↑` / `↓` | Accélérer / Freiner |
| `←` / `→` | Changer de voie |
| `C` | Régulateur de vitesse (Cruise Control) |
| `N` | Basculer mode nuit |
| `V` | Pleins phares (nuit uniquement) |
| `B` | Veilleuses (nuit uniquement) |
| `A` | Clignotant gauche |
| `D` | Clignotant droit |
| `F11` | Plein écran |
| `Échap` | Retour au menu |

### Simulation — Chapitre 4 (Rond-point)

| Touche | Action |
|---|---|
| `↑` / `↓` | Accélérer / Freiner |
| `←` / `→` | Tourner |
| `Échap` | Retour au menu |

### Guide des consignes

| Touche | Action |
|---|---|
| `←` / `→` | Page précédente / suivante |
| `Échap` | Retour au menu |

---

## 💾 Données et persistance

### `users.dat`

Fichier texte stockant tous les profils utilisateurs. Créé automatiquement au premier lancement. Format interne géré par les fonctions `auth_charger()` et `auth_sauvegarder()`.

- Jusqu'à **50 utilisateurs**
- Mot de passe stocké tel quel (pas de hachage — projet académique)
- Blocage automatique après **5 tentatives de connexion échouées**

### `save_[username].dat`

Un fichier par utilisateur, créé à la première session QCM.

```
unlocked=4    # Chapitres 1 à 4 débloqués
exam=0        # Examen blanc non encore accessible
```

### `photos/`

Dossier créé automatiquement. Quand un utilisateur sélectionne une photo de profil, elle est **copiée** dans ce dossier sous le nom `[username]_profil.ext`. Cela garantit que les photos restent disponibles même si le fichier original est déplacé ou supprimé, et que le projet reste portable tel quel.

---

## 👥 Équipe

Projet développé en équipe dans le cadre d'un projet académique à **ISEN Ouest Nantes (Carquefou)**.

| Membre | Module développé |
|---|---|
| **[Membre 1]** | Authentification, Menu principal, Système de profils |
| **[Membre 2]** | Simulation de conduite (Chapitres 1 & 4) |
| **[Membre 3]** | Test théorique QCM (10 chapitres, 200 questions) |
| **[Membre 4]** | Guide des consignes |

*Intégration multi-modules, architecture fenêtre unique, navigation clavier — réalisées en post-développement.*

---

## 📄 Licence

Ce projet est développé dans un cadre académique. Tous droits réservés aux auteurs.

---

<div align="center">
  <i>DriveSix — Préparez votre permis, simulez la route.</i>
</div>
