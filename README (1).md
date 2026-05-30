# Engine2D — Moteur de jeu 2D C++17 / SDL2

Moteur 2D moderne avec architecture **ECS**, inspiré de Unity.  
**~2 400 lignes** de code source — zéro dépendance externe hors SDL2.

---

## Compilation rapide

```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev
make          # compile tout
./unity_demo  # démo principale (scènes, UI avancée, Rigidbody, Tilemap…)
./platformer  # démo platformer
./particles_demo # démo particules / tweens
```

---

## Fonctionnalités

### Core
| Module | Détail |
|---|---|
| **ECS** | Entity/Component/Scene, templates, destroy différé |
| **SceneManager** | Scènes nommées, `loadScene` / `loadSceneAsync` |
| **EventBus** | Pub/Sub typé (`subscribe<T>` / `publish<T>`) |
| **Prefab System** | `registerPrefab` / `Instantiate(name, scene, pos)` |
| **Clock** | Delta-time cappé, FPS, temps cumulé |

### Physics
| Module | Détail |
|---|---|
| **Rigidbody2D** | Dynamic/Kinematic/Static, masse, gravité, drag, bounciness |
| **PhysicsSystem** | Intégration Verlet, résolution AABB par impulsion, fixed timestep 60Hz |
| **Tilemap** | Grille de tiles, collision sweep, world↔tile coords |
| **Collider** | AABB, triggers, physics layers |

### Rendu
| Module | Détail |
|---|---|
| **Renderer** | Rects, cercles, lignes, polygones, texte TTF, drawRectEx (rotation) |
| **RenderSystem** | Tri par layer, debug draw (vélocité, colliders) |
| **Camera2D** | Follow target, deadzone, smooth, clamp, screen shake |
| **Sprite** | Pivot, flipX/Y, layer, tint |

### UI avancée
| Module | Détail |
|---|---|
| **UIRenderer** | Rounded rects, gradients V/H, glow, drop shadow, badges, progress bars, tooltips |
| **UISystem** | Canvas, Button (hover/press states), Label, ProgressBar, Panel, UIAnchor |

### Effets
| Module | Détail |
|---|---|
| **ParticleEmitter** | Burst/continu, lerp couleur/taille, gravité, rotation angulaire |
| **Animator** | Clips de frames, durées, loop/one-shot, vitesse |
| **Tween** | 12 fonctions d'easing, `reverse()`, callback `onComplete` |
| **AudioManager** | Sine/square/triangle tone synthesis, volume 3D spatial |

---

## Démo Unity Features (`./unity_demo`)

### Scènes
- **Menu** — fond étoilé animé, nébuleuses, boutons avec glow/gradient/hover
- **Jeu** — Tilemap, Rigidbody2D, Camera follow+shake, Prefabs, EventBus, HUD complet
- **Game Over** — panel animé (tween outElastic), stats, record

### Contrôles en jeu
| Touche | Action |
|---|---|
| `A` / `D` ou `←/→` | Déplacement |
| `Espace` | Saut (double saut + coyote time) |
| `E` | Spawn ennemi (Prefab) |
| `C` | Camera shake |
| `F` | Debug draw (colliders, velocités, stats) |
| `ESC` | Retour menu |
| Clic | Boutons UI |

---

## Utilisation

```cpp
#include "Engine.hpp"

int main(){
    Engine engine({"Mon Jeu", 1200, 700});

    // Enregistrer des scènes
    engine.sceneManager().registerScene("Menu", [&](Scene& sc){
        auto& e = sc.createEntity("btn");
        auto& el = e.addComponent<UIElement>();
        el.rect = {0,0,200,50}; el.anchor = UIAnchor::Center;
        auto& btn = e.addComponent<UIButton>();
        btn.label = "Jouer";
        btn.onClick = [&]{ engine.sceneManager().loadSceneAsync("Game"); };
    });

    // EventBus typé
    engine.events().subscribe<EvScoreChanged>([](const EvScoreChanged& ev){
        std::cout << "Score: " << ev.newScore << "\n";
    });

    // Prefabs
    engine.prefabs().registerPrefab("Bullet", [](Scene& sc, Vec2 pos)->Entity&{
        auto& e = sc.createEntity("bullet");
        e.addComponent<Transform>().position = pos;
        auto& sp = e.addComponent<Sprite>(); sp.size={8,8}; sp.fillColor={255,255,0,255};
        e.addComponent<Rigidbody2D>().gravityScale = 0;
        return e;
    });
    Instantiate("Bullet", engine.scene(), {400,300});

    // UIRenderer avancé
    engine.onRender = [&](){
        UIRenderer ui(engine.renderer());
        ui.drawGlow({100,100,200,50}, 20, {80,160,255,100}, 8);
        ui.drawRoundedRect({100,100,200,50}, 12, {30,50,120,220}, {100,160,255,200}, 2);
        ui.drawProgressBar({110,160,180,14}, 0.7f, {20,20,40,200},
                           {60,180,90,255},{100,240,120,255});
    };

    engine.run();
}
```

---

## Architecture

```
Engine
 ├── SceneManager        loadScene / loadSceneAsync
 │    └── Scene          entités + lifecycle
 │         └── Entity[]
 │              ├── Transform / Velocity / Sprite / Collider
 │              ├── Rigidbody2D   (masse, gravité, drag, bounciness)
 │              ├── Camera2D      (follow, shake, clamp, deadzone)
 │              ├── Tilemap       (grille, collision sweep)
 │              ├── ParticleEmitter / Animator
 │              ├── UIElement / UIButton / UILabel / UIProgressBar / UIPanel
 │              └── Script        (onStart, onUpdate, onCollision lambdas)
 ├── PhysicsSystem       Verlet + AABB + fixed timestep 60Hz
 ├── CameraSystem        smooth follow + shake
 ├── RenderSystem        tri par layer + debug draw
 ├── UISystem            layout + hover + click
 ├── ScriptSystem        appel callbacks + dispatch collisions
 ├── EventBus            pub/sub typé
 ├── PrefabRegistry      blueprints réutilisables
 ├── AudioManager        synthèse tonale (sine/square/triangle)
 ├── Renderer            SDL2 + caméra + UIRenderer
 └── Clock               delta-time + FPS
```

---

## Licence

MIT — libre pour tout usage commercial ou non.
