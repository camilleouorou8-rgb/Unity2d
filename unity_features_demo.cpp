// ═══════════════════════════════════════════════════════════════════
//  Engine2D — Démo "Unity Features" — UI Améliorée
//  Scènes : MENU → GAME → GAMEOVER → MENU
//  Contrôles : A/D mouvement  ESPACE saut  E ennemi
//              C shake  F debug  ESC menu
// ═══════════════════════════════════════════════════════════════════
#include "Engine.hpp"
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cmath>

// ───── Événements EventBus ──────────────────────────────────────
struct EvPlayerDied  { int score; };
struct EvCoinPicked  { int value; EntityID playerID; };
struct EvEnemyKilled { Vec2 pos; };
struct EvScoreChanged{ int newScore; };

static int  g_score     = 0;
static int  g_highScore = 0;
static bool g_debugDraw = false;
static int  g_coins     = 0;
static int  g_enemies   = 0;

// ════════════════════════════════════════════════════════════════
//  DESSIN UTILITAIRES AVANCÉS (étoiles, nuages, bg)
// ════════════════════════════════════════════════════════════════
static void drawBackground(Renderer& r, float time, int W, int H) {
    // Ciel dégradé (simulé par bandes horizontales)
    UIRenderer ui(r);
    ui.drawGradientRect({0,0,(float)W,(float)H},{8,12,32,255},{20,30,60,255});

    // Étoiles scintillantes
    static std::vector<std::tuple<Vec2,float,float>> stars;
    if(stars.empty()){
        for(int i=0;i<120;i++){
            stars.push_back({{(float)(std::rand()%W),(float)(std::rand()%H)},
                (float)(std::rand()%300)/100.f, (float)(std::rand()%3)+1.f});
        }
    }
    for(auto&[pos,phase,sz]:stars){
        float tw=0.4f+0.6f*std::sin(time*1.2f+phase);
        Uint8 a=(Uint8)(80+160*tw);
        r.drawCircle(pos,sz*0.5f,{200,220,255,a},true);
    }
}

static void drawNebula(Renderer& r, float time, int W, int H){
    // Quelques "nuages" colorés pour le fond
    UIRenderer ui(r);
    struct Cloud{float x,y,rx,ry; Color c;};
    static std::vector<Cloud> clouds={
        {0.15f,0.3f,180,80,{60,20,120,18}},
        {0.5f, 0.5f,220,90,{20,60,140,15}},
        {0.8f, 0.2f,160,70,{100,30,80,14}},
        {0.35f,0.7f,200,85,{30,80,100,12}},
    };
    for(auto& c:clouds){
        float px=c.x*W+std::sin(time*0.18f+c.rx*0.01f)*20;
        float py=c.y*H+std::cos(time*0.14f+c.ry*0.01f)*12;
        for(int i=5;i>=1;i--){
            float ex=(float)i*8;
            Uint8 a=(Uint8)(c.c.a*(1.f-(float)i/6));
            r.drawCircle({px,py},(c.rx+ex)*0.5f,{c.c.r,c.c.g,c.c.b,a},true);
        }
    }
}

// ════════════════════════════════════════════════════════════════
//  PRÉFABS
// ════════════════════════════════════════════════════════════════
static void registerPrefabs(Engine&) {

    PrefabRegistry::instance().registerPrefab("Coin",
        [](Scene& sc, Vec2 pos) -> Entity& {
            auto& e = sc.createEntity("coin");
            auto& tr = e.addComponent<Transform>(); tr.position = pos;
            auto& sp = e.addComponent<Sprite>();
            sp.size={16,16}; sp.fillColor={255,215,0,255}; sp.layer=2;
            auto& col = e.addComponent<Collider>();
            col.bounds={-8,-8,16,16}; col.isTrigger=true;
            auto& sc_ = e.addComponent<Script>();
            sc_.onUpdate=[](Entity& self, float){
                auto& t=*self.getComponent<Transform>();
                auto& s=*self.getComponent<Sprite>();
                float bob=std::sin(SDL_GetTicks()*0.004f+t.position.x*0.04f)*2.5f;
                t.position.y+=bob*0.06f;
                float p=0.82f+0.18f*std::sin(SDL_GetTicks()*0.005f+t.position.x);
                s.size={16*p,16*p};
            };
            return e;
        });

    PrefabRegistry::instance().registerPrefab("Enemy",
        [](Scene& sc, Vec2 pos) -> Entity& {
            auto& e = sc.createEntity("enemy");
            auto& tr = e.addComponent<Transform>(); tr.position=pos;
            auto& sp = e.addComponent<Sprite>();
            sp.size={28,28}; sp.fillColor={220,60,60,255}; sp.layer=2;
            auto& rb = e.addComponent<Rigidbody2D>();
            rb.mass=1.f; rb.gravityScale=1.f; rb.linearDrag=0.02f;
            auto& col = e.addComponent<Collider>();
            col.bounds={-14,-14,28,28};
            auto& sc_ = e.addComponent<Script>();
            sc_.onUpdate=[](Entity& self, float){
                auto& rb_=*self.getComponent<Rigidbody2D>();
                auto& tr_=*self.getComponent<Transform>();
                static std::unordered_map<EntityID,float> dir;
                if(!dir.count(self.id)) dir[self.id]=(std::rand()%2)?1.f:-1.f;
                float& d=dir[self.id];
                rb_.velocity.x=d*90.f;
                if(tr_.position.x<60) d=1.f;
                if(tr_.position.x>1140) d=-1.f;
                self.getComponent<Sprite>()->fillColor=
                    d>0?Color{220,60,60,255}:Color{180,40,200,255};
            };
            return e;
        });

    PrefabRegistry::instance().registerPrefab("Explosion",
        [](Scene& sc, Vec2 pos) -> Entity& {
            auto& e = sc.createEntity("explosion");
            auto& tr = e.addComponent<Transform>(); tr.position=pos;
            auto& pe = e.addComponent<ParticleEmitter>();
            pe.config.velMin={-200,-280}; pe.config.velMax={200,-60};
            pe.config.lifeMin=0.3f; pe.config.lifeMax=0.9f;
            pe.config.sizeStart=9.f; pe.config.sizeEnd=0.f;
            pe.config.colorStart={255,160,30,255};
            pe.config.colorEnd={255,40,0,0};
            pe.config.gravity=400.f; pe.config.burstCount=40;
            pe.config.active=false; pe.burst(40);
            auto& sc_=e.addComponent<Script>();
            sc_.onStart=[](Entity& self){
                self.getComponent<Script>()->onUpdate=
                    [life=1.2f](Entity& s2,float dt)mutable{
                        life-=dt; if(life<=0)s2.active=false;
                    };
            };
            return e;
        });
}

// ════════════════════════════════════════════════════════════════
//  SCÈNE MENU  — UI avancée
// ════════════════════════════════════════════════════════════════
static void buildMenuScene(Scene& sc, Engine& engine){
    sc.name="Menu";

    // Script de rendu manuel
    auto& bgE=sc.createEntity("bg_render");
    bgE.addComponent<Transform>();
    bgE.addComponent<Script>().onUpdate=[&engine](Entity&,float){
        (void)engine;
    };

    // Particules ambiantes (lucioles)
    auto& fireflies=sc.createEntity("fireflies");
    fireflies.addComponent<Transform>().position={600,350};
    auto& ffpe=fireflies.addComponent<ParticleEmitter>();
    ffpe.config.velMin={-30,-50}; ffpe.config.velMax={30,-20};
    ffpe.config.lifeMin=2.f; ffpe.config.lifeMax=4.f;
    ffpe.config.sizeStart=3.f; ffpe.config.sizeEnd=0.f;
    ffpe.config.colorStart={180,230,255,200};
    ffpe.config.colorEnd={100,160,255,0};
    ffpe.config.gravity=-15.f; ffpe.config.rate=8.f;
    ffpe.config.posOffset={};

    // Script global menu : raccourcis
    auto& gs=sc.createEntity("menu_script");
    gs.addComponent<Transform>();
    gs.addComponent<Script>().onUpdate=[&engine](Entity&,float){
        if(engine.input().isKeyPressed(SDL_SCANCODE_RETURN))
            engine.sceneManager().loadSceneAsync("Game");
        if(engine.input().isKeyPressed(SDL_SCANCODE_ESCAPE))
            engine.quit();
    };
}

// ════════════════════════════════════════════════════════════════
//  SCÈNE JEU
// ════════════════════════════════════════════════════════════════
static void buildGameScene(Scene& sc, Engine& engine){
    sc.name="Game";
    g_score=0; g_coins=0; g_enemies=0;

    // Caméra
    auto& camE=sc.createEntity("camera");
    auto& cam=camE.addComponent<Camera2D>();
    cam.zoom=1.f; cam.smoothSpeed=6.f; cam.deadzone={80,50};
    cam.clampToWorld=true; cam.worldBounds={0,0,1200,700};

    // Tilemap
    auto& mapE=sc.createEntity("tilemap");
    auto& tmap=mapE.addComponent<Tilemap>(40,18,32,32);
    tmap.origin={0,144};
    auto solid=[](Color c)->Tile{Tile t;t.id=1;t.solid=true;t.color=c;return t;};
    auto deco =[](Color c)->Tile{Tile t;t.id=2;t.solid=false;t.color=c;return t;};
    for(int x=0;x<40;x++) tmap.setTile(x,17,solid({55,40,28,255}));
    for(int x=0;x<40;x++) tmap.setTile(x,16,solid({75,58,40,255}));
    int plat[][3]={{3,13,6},{10,11,5},{17,9,4},{23,12,6},{30,10,5},{35,14,4}};
    Color pCols[]={{70,100,180,255},{100,70,160,255},{55,125,75,255},
                   {155,75,55,255},{75,135,155,255},{115,85,45,255}};
    for(int i=0;i<6;i++)
        for(int dx=0;dx<plat[i][2];dx++){
            tmap.setTile(plat[i][0]+dx,plat[i][1],solid(pCols[i]));
            if(plat[i][1]+1<18) tmap.setTile(plat[i][0]+dx,plat[i][1]+1,solid({(Uint8)(pCols[i].r*7/10),(Uint8)(pCols[i].g*7/10),(Uint8)(pCols[i].b*7/10),255}));
        }
    for(int y=0;y<18;y++){tmap.setTile(0,y,solid({45,45,65,255}));tmap.setTile(39,y,solid({45,45,65,255}));}
    for(int x=2;x<38;x+=5) tmap.setTile(x,15,deco({80,160,70,160}));

    // Joueur
    auto& player=sc.createEntity("player");
    {
        auto& tr=player.addComponent<Transform>(); tr.position={100,400};
        auto& sp=player.addComponent<Sprite>();
        sp.size={26,34}; sp.fillColor={80,160,255,255}; sp.layer=3;
        auto& rb=player.addComponent<Rigidbody2D>();
        rb.mass=1.f; rb.gravityScale=1.f; rb.linearDrag=0.06f; rb.freezeRotation=true;
        auto& col=player.addComponent<Collider>(); col.bounds={-13,-17,26,34};
        cam.followTarget=player.id;

        auto& pe=player.addComponent<ParticleEmitter>();
        pe.config.velMin={-25,-5}; pe.config.velMax={25,40};
        pe.config.lifeMin=0.1f; pe.config.lifeMax=0.25f;
        pe.config.sizeStart=5.f; pe.config.sizeEnd=0.f;
        pe.config.colorStart={180,200,255,160};
        pe.config.colorEnd={120,140,200,0};
        pe.config.gravity=80.f; pe.config.rate=0.f;

        auto& scr=player.addComponent<Script>();
        scr.onUpdate=[&engine,&sc](Entity& self,float dt){
            auto& rb_=*self.getComponent<Rigidbody2D>();
            auto& tr_=*self.getComponent<Transform>();
            auto& sp_=*self.getComponent<Sprite>();
            auto& pe_=*self.getComponent<ParticleEmitter>();
            auto& inp=engine.input();
            static int jleft=0; static float coyote=0;
            if(rb_.isGrounded){jleft=2;coyote=0.12f;}else coyote=std::max(0.f,coyote-dt);
            float ax=inp.getAxis("Horizontal");
            rb_.velocity.x=ax*210.f;
            if(inp.isKeyPressed(SDL_SCANCODE_SPACE)&&(jleft>0||coyote>0)){
                rb_.velocity.y=-430.f; jleft--; coyote=0;
                engine.audio().playBeep(520,45,0.2f);
            }
            pe_.config.active=rb_.isGrounded&&std::abs(ax)>0.1f;
            pe_.config.rate=45.f;
            if(!rb_.isGrounded) sp_.fillColor={120,200,255,255};
            else if(std::abs(ax)>0.1f) sp_.fillColor={80,220,120,255};
            else sp_.fillColor={80,160,255,255};
            if(ax>0.1f) sp_.flipX=false;
            if(ax<-0.1f) sp_.flipX=true;
            if(tr_.position.y>750) engine.events().publish(EvPlayerDied{g_score});
        };
        scr.onCollision=[&engine,&sc](Entity& self,EntityID oid){
            for(auto* o:sc.allEntities()){
                if(o->id!=oid)continue;
                if(o->tag=="coin"){
                    g_score+=10; g_coins++;
                    engine.events().publish(EvCoinPicked{10,self.id});
                    engine.events().publish(EvScoreChanged{g_score});
                    engine.audio().playBeep(880,35,0.18f);
                    if(auto* t2=o->getComponent<Transform>()){
                        auto& ex=Instantiate("Explosion",sc,t2->position);
                        ex.getComponent<ParticleEmitter>()->config.colorStart={255,215,0,255};
                        ex.getComponent<ParticleEmitter>()->config.colorEnd={255,150,0,0};
                    }
                    sc.destroyEntity(oid);
                }
                if(o->tag=="enemy"){
                    auto& ce=*sc.findByTag("camera");
                    ce.getComponent<Camera2D>()->shake(0.45f,9.f);
                    engine.events().publish(EvPlayerDied{g_score});
                    engine.audio().playBeep(160,130,0.3f);
                }
            }
        };
    }

    // Pièces
    for(Vec2 cp:{Vec2{150,390},Vec2{260,390},Vec2{370,390},
                 Vec2{144,144+13*32-24},Vec2{176,144+13*32-24},
                 Vec2{344,144+11*32-24},Vec2{376,144+11*32-24},
                 Vec2{572,144+9*32-24}, Vec2{604,144+9*32-24},
                 Vec2{764,144+12*32-24},Vec2{796,144+12*32-24},
                 Vec2{980,144+10*32-24},Vec2{1012,144+10*32-24},
                 Vec2{600,310},{700,300},{850,330}})
        Instantiate("Coin",sc,cp);

    Instantiate("Enemy",sc,{400,460}); g_enemies++;
    Instantiate("Enemy",sc,{780,460}); g_enemies++;

    // EventBus
    engine.events().subscribe<EvPlayerDied>([&engine](const EvPlayerDied& ev){
        if(ev.score>g_highScore) g_highScore=ev.score;
        engine.sceneManager().loadSceneAsync("GameOver");
    });

    // Script global jeu
    auto& gs=sc.createEntity("global");
    gs.addComponent<Transform>();
    gs.addComponent<Script>().onUpdate=[&engine,&sc](Entity&,float){
        auto& inp=engine.input();
        if(inp.isKeyPressed(SDL_SCANCODE_ESCAPE)) engine.sceneManager().loadSceneAsync("Menu");
        if(inp.isKeyPressed(SDL_SCANCODE_E)){
            auto* pl=sc.findByTag("player");
            if(pl){
                auto* tr=pl->getComponent<Transform>();
                Instantiate("Enemy",sc,{tr->position.x+(float)((std::rand()%200)-100),tr->position.y-60});
                g_enemies++;
                engine.audio().playBeep(220,55,0.18f);
            }
        }
        if(inp.isKeyPressed(SDL_SCANCODE_C)){
            auto* ce=sc.findByTag("camera");
            if(ce) ce->getComponent<Camera2D>()->shake(0.5f,14.f);
            engine.audio().playBeep(110,90,0.12f);
        }
        if(inp.isKeyPressed(SDL_SCANCODE_F)) g_debugDraw=!g_debugDraw;
    };
}

// ════════════════════════════════════════════════════════════════
//  SCÈNE GAME OVER
// ════════════════════════════════════════════════════════════════
static void buildGameOverScene(Scene& sc, Engine& engine){
    sc.name="GameOver";
    // Particules sombres
    auto& pe_e=sc.createEntity("debris");
    pe_e.addComponent<Transform>().position={600,350};
    auto& pe=pe_e.addComponent<ParticleEmitter>();
    pe.config.velMin={-100,-120}; pe.config.velMax={100,-30};
    pe.config.lifeMin=1.f; pe.config.lifeMax=2.5f;
    pe.config.sizeStart=6.f; pe.config.sizeEnd=0.f;
    pe.config.colorStart={200,50,50,180}; pe.config.colorEnd={100,10,10,0};
    pe.config.gravity=-20.f; pe.config.rate=12.f;
    pe.config.posOffset={};

    // Script boutons clavier
    auto& gs=sc.createEntity("go_input");
    gs.addComponent<Transform>();
    gs.addComponent<Script>().onUpdate=[&engine](Entity&,float){
        if(engine.input().isKeyPressed(SDL_SCANCODE_RETURN)||
           engine.input().isKeyPressed(SDL_SCANCODE_R))
            engine.sceneManager().loadSceneAsync("Game");
        if(engine.input().isKeyPressed(SDL_SCANCODE_ESCAPE))
            engine.sceneManager().loadSceneAsync("Menu");
    };
}

// ════════════════════════════════════════════════════════════════
//  RENDU CUSTOM HUD/UI  (dessiné dans onRender)
// ════════════════════════════════════════════════════════════════
static void renderMenu(Engine& engine, float time){
    auto& r=engine.renderer();
    int W=engine.windowWidth(), H=engine.windowHeight();
    UIRenderer ui(r);

    drawBackground(r,time,W,H);
    drawNebula(r,time,W,H);

    // Particules manuelles (emitter rendu dans onRender)
    auto* ff=engine.scene().findByTag("fireflies");
    if(ff) ff->getComponent<ParticleEmitter>()->render(r,{600,350});

    // ── Logo principal ──────────────────────────────────────────
    float pulse=1.f+0.03f*std::sin(time*2.5f);
    {
        // Glow derrière le titre
        Rect logoR{(float)(W/2-200),(float)(H/2-175),400,90};
        ui.drawGlow(logoR,30,{60,120,255,120},8);
        // Panel titre gradient
        ui.drawShadow(logoR,14,4,6,8);
        ui.drawGradientRect(logoR,{15,25,70,230},{8,15,50,240});
        ui.drawRoundedRect(logoR,14,{0,0,0,0},{80,140,255,200},2);
        // Ligne de reflet en haut
        ui.drawGradientRectH({logoR.x+14,logoR.y+1,logoR.w-28,2},{0,0,0,0},{255,255,255,60});
        // Texte
        std::string title="ENGINE 2D";
        Vec2 tsz=r.measureText(title,42);
        r.drawText(title,{(float)(W/2)-tsz.x/2-1,(float)(H/2-168)},{40,80,180,180},42,true);
        r.drawText(title,{(float)(W/2)-tsz.x/2,(float)(H/2-170)},{150,210,255,255},42,true);
        // Version badge
        ui.drawBadge({(float)(W/2+180),(float)(H/2-130)},16,{40,100,200,220},{80,160,255,255});
        r.drawText("v2",{(float)(W/2+174),(float)(H/2-138)},{220,240,255,255},11,true);
    }

    // Sous-titre animé
    {
        float alpha=0.6f+0.4f*std::sin(time*1.8f);
        std::string sub="Unity-like Features Showcase";
        Vec2 sz=r.measureText(sub,15);
        r.drawText(sub,{(float)(W/2)-sz.x/2,(float)(H/2-100)},
                   {140,180,255,(Uint8)(180*alpha)},15,true);
    }

    // ── Bouton JOUER ─────────────────────────────────────────────
    {
        Rect btn{(float)(W/2-110),(float)(H/2-55),220,52};
        int mx,my; SDL_GetMouseState(&mx,&my);
        bool hover=btn.contains({(float)mx,(float)my});
        bool press=hover&&(SDL_GetMouseState(nullptr,nullptr)&SDL_BUTTON(1));

        Color topC = press?Color{30,80,160,255}:hover?Color{80,160,255,255}:Color{50,120,220,255};
        Color botC = press?Color{20,60,120,255}:hover?Color{50,110,200,255}:Color{30,80,160,255};

        if(hover) ui.drawGlow(btn,18,{60,140,255,80},6);
        ui.drawShadow(btn,12,3,5,6);
        ui.drawGradientRect(btn,topC,botC);
        ui.drawRoundedRect(btn,12,{0,0,0,0},{hover?Color{160,210,255,255}:Color{100,170,255,200}},2);
        // Shine
        ui.drawGradientRect({btn.x+10,btn.y+3,btn.w-20,btn.h/2-2},
                            {255,255,255,(Uint8)(hover?50:30)},{255,255,255,0});
        // Texte centré
        float sc_=pulse*(press?0.96f:hover?1.03f:1.f);
        std::string lbl="\xe2\x96\xb6  JOUER";  // ▶
        Vec2 tsz=r.measureText(lbl,20);
        r.drawText(lbl,{btn.x+(btn.w-tsz.x*sc_)/2, btn.y+(btn.h-tsz.y)/2},
                   {240,248,255,255},20,true);
        // Hint Entrée
        r.drawText("[Entree]",{btn.x+btn.w+10,btn.y+16},{120,160,200,160},12,true);
    }

    // ── Bouton QUITTER ───────────────────────────────────────────
    {
        Rect btn{(float)(W/2-110),(float)(H/2+10),220,44};
        int mx,my; SDL_GetMouseState(&mx,&my);
        bool hover=btn.contains({(float)mx,(float)my});
        bool press=hover&&(SDL_GetMouseState(nullptr,nullptr)&SDL_BUTTON(1));
        Color topC=press?Color{80,20,20,255}:hover?Color{200,70,70,255}:Color{140,45,45,255};
        Color botC=press?Color{50,10,10,255}:hover?Color{150,40,40,255}:Color{100,25,25,255};
        if(hover) ui.drawGlow(btn,14,{200,60,60,70},5);
        ui.drawShadow(btn,10,2,4,5);
        ui.drawGradientRect(btn,topC,botC);
        ui.drawRoundedRect(btn,10,{0,0,0,0},{hover?Color{255,140,140,220}:Color{180,80,80,180}},2);
        ui.drawGradientRect({btn.x+8,btn.y+2,btn.w-16,btn.h/2-2},{255,255,255,(Uint8)(hover?40:20)},{255,255,255,0});
        std::string lbl="\xe2\x9c\x95  QUITTER";
        Vec2 tsz=r.measureText(lbl,18);
        r.drawText(lbl,{btn.x+(btn.w-tsz.x)/2,btn.y+(btn.h-tsz.y)/2},{255,220,220,255},18,true);
    }

    // ── Stats / highscore ────────────────────────────────────────
    {
        Rect panel{(float)(W/2-130),(float)(H/2+70),260,70};
        ui.drawShadow(panel,10,2,3,5);
        ui.drawGradientRect(panel,{20,20,50,160},{10,12,35,190});
        ui.drawRoundedRect(panel,10,{0,0,0,0},{60,80,140,150},1);

        std::string hs="Meilleur score : "+std::to_string(g_highScore);
        Vec2 sz=r.measureText(hs,15);
        r.drawText(hs,{(float)(W/2)-sz.x/2,(float)(H/2+80)},{200,220,100,220},15,true);
        ui.drawBadge({(float)(W/2-sz.x/2-14),(float)(H/2+88)},6,{255,215,0,200},{255,180,0,255});

        std::string hint="Fleches/WASD  Espace  E C F";
        Vec2 sz2=r.measureText(hint,12);
        r.drawText(hint,{(float)(W/2)-sz2.x/2,(float)(H/2+104)},{120,140,180,160},12,true);
    }

    // ── Bandeau version bas ──────────────────────────────────────
    ui.drawGradientRect({0,(float)(H-22),(float)W,22},{0,0,0,0},{0,0,0,120});
    std::string ver="Engine2D C++17 / SDL2  |  ECS · Rigidbody2D · Tilemap · Camera2D · EventBus · Prefabs · Tweens · Particles";
    Vec2 vsz=r.measureText(ver,11);
    r.drawText(ver,{(float)(W/2)-vsz.x/2,(float)(H-18)},{100,120,160,180},11,true);
}

// ── HUD en jeu ───────────────────────────────────────────────────
static void renderGameHUD(Engine& engine, float time){
    auto& r=engine.renderer();
    int W=engine.windowWidth();
    UIRenderer ui(r);

    // ── Barre de score (haut gauche) ─────────────────────────────
    {
        Rect panel{10,10,230,52};
        ui.drawShadow(panel,10,2,3,5);
        ui.drawGradientRect(panel,{10,15,40,210},{8,12,30,230});
        ui.drawRoundedRect(panel,10,{0,0,0,0},{60,100,200,200},2);
        // Shine
        ui.drawGradientRect({panel.x+10,panel.y+2,panel.w-20,10},{255,255,255,35},{255,255,255,0});
        // Badge score
        ui.drawBadge({31,36},10,{255,215,0,230},{255,180,0,255});
        std::string stxt=std::to_string(g_score);
        r.drawText("Score",{48,14},{180,200,255,200},12,true);
        r.drawText(stxt,{48,29},{255,220,60,255},18,true);
        // Compteur pièces
        r.drawText(std::to_string(g_coins)+" pieces",{140,22},{200,180,100,200},12,true);
    }

    // ── Barre de vie (haut gauche, sous score) ───────────────────
    {
        Rect bar{10,68,230,14};
        float hp=0.55f+0.45f*std::sin(time*0.7f); // animée pour la démo
        Color fillA=hp>0.5f?Color{60,220,90,255}:Color{220,100,40,255};
        Color fillB=hp>0.5f?Color{100,255,130,255}:Color{255,160,60,255};
        r.drawText("HP",{12,56},{180,220,180,180},11,true);
        ui.drawProgressBar(bar,hp,{20,20,40,200},fillA,fillB,7.f);
    }

    // ── Mini-map (haut droit) ────────────────────────────────────
    {
        Rect mm{(float)(W-170),10,160,45};
        ui.drawShadow(mm,8,2,3,4);
        ui.drawGradientRect(mm,{10,15,40,200},{6,10,28,220});
        ui.drawRoundedRect(mm,8,{0,0,0,0},{60,80,160,180},1);
        r.drawText("MINI-MAP",{(float)(W-168),12},{100,130,200,180},11,true);
        // Joueur dot
        auto* pl=engine.scene().findByTag("player");
        if(pl){
            auto* tr=pl->getComponent<Transform>();
            if(tr){
                float px=mm.x+8+(tr->position.x/1200.f)*(mm.w-16);
                float py=mm.y+20+(tr->position.y/700.f)*(mm.h-24);
                ui.drawBadge({px,py},3,{80,180,255,255},{160,220,255,255});
            }
        }
        // Ennemis sur mini-map
        for(auto* e:engine.scene().allEntities()){
            if(e->tag!="enemy")continue;
            auto* tr=e->getComponent<Transform>();
            if(!tr)continue;
            float px=mm.x+8+(tr->position.x/1200.f)*(mm.w-16);
            float py=mm.y+20+(tr->position.y/700.f)*(mm.h-24);
            ui.drawBadge({px,py},2,{220,60,60,220},{255,100,100,255});
        }
    }

    // ── Infos débug (F pour toggle) ──────────────────────────────
    if(g_debugDraw){
        Rect dbg{(float)(W-200),(float)60,190,110};
        ui.drawGradientRect(dbg,{0,30,10,180},{0,20,8,200});
        ui.drawRoundedRect(dbg,8,{0,0,0,0},{40,180,80,180},1);
        auto& sc_=engine.scene();
        int n=(int)sc_.allEntities().size();
        auto rb=[&](int y,const std::string& t){ r.drawText(t,{(float)(W-195),(float)y},{160,255,180,220},12,true);};
        rb(64,"[DEBUG]");
        rb(79,"Entites : "+std::to_string(n));
        rb(93,"Scene   : "+sc_.name);
        rb(107,"Enemies : "+std::to_string(g_enemies));
        rb(121,"Coins   : "+std::to_string(g_coins));
        rb(135,"Score   : "+std::to_string(g_score));
        rb(149,"Phys 60hz fixed");
    }

    // ── Barre de contrôles (bas) ─────────────────────────────────
    {
        Rect hint{0,(float)(engine.windowHeight()-22),(float)W,22};
        ui.drawGradientRect(hint,{0,0,0,0},{0,0,0,160});
        struct Key{const char* k; const char* desc;};
        Key keys[]={{"A/D","Deplacement"},{"Espace","Saut"},{"E","Ennemi"},{"C","Shake"},{"F","Debug"},{"ESC","Menu"}};
        float x=14;
        for(auto& [k,d]:keys){
            // Touche
            Vec2 ksz=r.measureText(k,12);
            Rect kb{x-2,(float)(engine.windowHeight()-20),ksz.x+6,16};
            ui.drawRoundedRect(kb,3,{50,60,100,200},{100,120,180,220},1);
            r.drawText(k,{x,(float)(engine.windowHeight()-19)},{220,230,255,255},12,true);
            x+=ksz.x+8;
            r.drawText(d,{x,(float)(engine.windowHeight()-19)},{140,160,200,180},12,true);
            Vec2 dsz=r.measureText(d,12);
            x+=dsz.x+20;
        }
    }
}

// ── Écran Game Over ───────────────────────────────────────────────
static void renderGameOver(Engine& engine, float time){
    auto& r=engine.renderer();
    int W=engine.windowWidth(), H=engine.windowHeight();
    UIRenderer ui(r);

    // Fond sombre dégradé radial simulé
    ui.drawGradientRect({0,0,(float)W,(float)H},{15,8,20,255},{5,3,10,255});

    // Particules
    auto* debris=engine.scene().findByTag("debris");
    if(debris) debris->getComponent<ParticleEmitter>()->render(r,{600,350});

    // Vignette
    for(int i=0;i<12;i++){
        float f=(float)i/12;
        Uint8 a=(Uint8)(120*(1-f)*(1-f));
        ui.drawRoundedRect({(float)i*2,(float)i*2,(float)(W-i*4),(float)(H-i*4)},
                           0,{0,0,0,0},{0,0,0,a},1);
    }

    // Panel central
    float appear=std::min(1.f,(time+0.001f)*2.5f);
    float ea=Ease::outElastic(appear);
    float pw=440*ea, ph=320*ea;
    Rect panel{(float)(W/2)-pw/2,(float)(H/2)-ph/2,pw,ph};

    ui.drawGlow(panel,40,{180,30,30,60},10);
    ui.drawShadow(panel,18,5,8,10);
    ui.drawGradientRect(panel,{30,10,40,235},{15,5,25,245});
    ui.drawRoundedRect(panel,18,{0,0,0,0},{160,40,40,220},2);
    // Ligne déco top
    ui.drawGradientRectH({panel.x+18,panel.y+1,panel.w-36,3},{0,0,0,0},{255,80,80,180});

    if(appear>0.3f){
        float a2=std::min(1.f,(appear-0.3f)/0.7f);
        // Titre "GAME OVER"
        float glow_=0.5f+0.5f*std::sin(time*3.f);
        Rect titleR{panel.x+20,panel.y+20,panel.w-40,60};
        ui.drawGlow(titleR,20,{255,50,50,(Uint8)(60*glow_)},6);
        ui.drawGradientRect(titleR,{60,10,10,180},{40,5,5,200});
        ui.drawRoundedRect(titleR,10,{0,0,0,0},{200,40,40,160},1);
        std::string go="GAME OVER";
        Vec2 gsz=r.measureText(go,36);
        // Shadow text
        r.drawText(go,{panel.x+(panel.w-gsz.x)/2+2,panel.y+32},{100,0,0,(Uint8)(200*a2)},36,true);
        r.drawText(go,{panel.x+(panel.w-gsz.x)/2,panel.y+30},{255,(Uint8)(80*glow_),(Uint8)(80*glow_),(Uint8)(255*a2)},36,true);

        // Score
        {
            Rect sr{panel.x+30,panel.y+95,panel.w-60,44};
            ui.drawGradientRect(sr,{20,20,50,180},{12,12,35,200});
            ui.drawRoundedRect(sr,8,{0,0,0,0},{80,100,180,160},1);
            ui.drawBadge({panel.x+50,panel.y+117},9,{255,215,0,220},{255,180,0,255});
            std::string sc_txt="Score : "+std::to_string(g_score);
            Vec2 ssz=r.measureText(sc_txt,22);
            r.drawText(sc_txt,{panel.x+(panel.w-ssz.x)/2,panel.y+104},{255,220,60,(Uint8)(255*a2)},22,true);
        }
        // Highscore
        {
            bool isNew=g_score==g_highScore&&g_score>0;
            std::string hs=(isNew?"✦ NOUVEAU RECORD ✦  ":"Meilleur : ")+std::to_string(g_highScore);
            Vec2 hsz=r.measureText(hs,15);
            if(isNew){
                float gs2=0.5f+0.5f*std::sin(time*4);
                r.drawText(hs,{panel.x+(panel.w-hsz.x)/2,panel.y+148},
                           {(Uint8)(200+55*gs2),220,(Uint8)(60*gs2),(Uint8)(230*a2)},15,true);
            } else {
                r.drawText(hs,{panel.x+(panel.w-hsz.x)/2,panel.y+148},
                           {180,220,100,(Uint8)(200*a2)},15,true);
            }
        }
        // Stats coins
        {
            std::string stats=std::to_string(g_coins)+" pieces  |  "+std::to_string(g_enemies)+" ennemis spawnes";
            Vec2 ssz=r.measureText(stats,13);
            r.drawText(stats,{panel.x+(panel.w-ssz.x)/2,panel.y+172},{160,180,220,(Uint8)(180*a2)},13,true);
        }

        // ── Bouton Rejouer ──────────────────────────────────────
        {
            Rect btn{panel.x+40,panel.y+200,panel.w-80,48};
            int mx,my; SDL_GetMouseState(&mx,&my);
            bool hover=btn.contains({(float)mx,(float)my});
            Color topC=hover?Color{60,180,90,255}:Color{40,140,65,255};
            Color botC=hover?Color{40,130,65,255}:Color{25,90,40,255};
            if(hover) ui.drawGlow(btn,14,{40,200,80,80},5);
            ui.drawShadow(btn,10,2,4,5);
            ui.drawGradientRect(btn,topC,botC);
            ui.drawRoundedRect(btn,10,{0,0,0,0},{hover?Color{120,255,150,220}:Color{80,200,100,180}},2);
            ui.drawGradientRect({btn.x+10,btn.y+2,btn.w-20,btn.h/2-3},{255,255,255,(Uint8)(hover?45:25)},{255,255,255,0});
            std::string lbl="\xe2\x86\xba  REJOUER  [R]";
            Vec2 lsz=r.measureText(lbl,18);
            r.drawText(lbl,{btn.x+(btn.w-lsz.x)/2,btn.y+(btn.h-lsz.y)/2},{240,255,240,(Uint8)(255*a2)},18,true);
        }
        // Hint ESC
        std::string back="[ESC] Retour menu";
        Vec2 bsz=r.measureText(back,12);
        r.drawText(back,{panel.x+(panel.w-bsz.x)/2,panel.y+264},{140,150,200,(Uint8)(160*a2)},12,true);
    }
}

// ════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════
int main(){
    EngineConfig cfg;
    cfg.title="Engine2D — Unity Features"; cfg.width=1200; cfg.height=700;
    cfg.vsync=true; cfg.showFPS=true;
    Engine engine(cfg);
    engine.physics().gravity={0,980.f};
    registerPrefabs(engine);

    engine.sceneManager().registerScene("Menu",    [&](Scene& sc){ buildMenuScene(sc,engine); });
    engine.sceneManager().registerScene("Game",    [&](Scene& sc){ buildGameScene(sc,engine); });
    engine.sceneManager().registerScene("GameOver",[&](Scene& sc){ buildGameOverScene(sc,engine); });
    engine.sceneManager().loadScene("Menu");

    float goTime=0.f;

    engine.onUpdate=[&](float dt){
        if(engine.scene().name=="GameOver") goTime+=dt;
        else goTime=0.f;
        // Click bouton Rejouer dans game over
        if(engine.scene().name=="GameOver"){
            int mx,my; auto mb=SDL_GetMouseState(&mx,&my);
            if(mb&SDL_BUTTON(1)){
                int W=engine.windowWidth(),H=engine.windowHeight();
                float appear=std::min(1.f,goTime*2.5f);
                float ea=Ease::outElastic(appear);
                float pw=440*ea,ph=320*ea;
                Rect panel{(float)(W/2)-pw/2,(float)(H/2)-ph/2,pw,ph};
                Rect btn{panel.x+40,panel.y+200,panel.w-80,48};
                if(btn.contains({(float)mx,(float)my}))
                    engine.sceneManager().loadSceneAsync("Game");
            }
        }
        // Click boutons menu
        if(engine.scene().name=="Menu"){
            int mx,my; static bool wasDown=false;
            bool isDown=(SDL_GetMouseState(&mx,&my)&SDL_BUTTON(1))!=0;
            if(isDown&&!wasDown){
                int W=engine.windowWidth(),H=engine.windowHeight();
                Rect playBtn{(float)(W/2-110),(float)(H/2-55),220,52};
                Rect quitBtn{(float)(W/2-110),(float)(H/2+10),220,44};
                if(playBtn.contains({(float)mx,(float)my}))
                    engine.sceneManager().loadSceneAsync("Game");
                if(quitBtn.contains({(float)mx,(float)my}))
                    engine.quit();
            }
            wasDown=isDown;
        }
    };

    engine.onRender=[&](){
        float t=engine.clock().time();
        const std::string& sname=engine.scene().name;
        if(sname=="Menu")          renderMenu(engine,t);
        else if(sname=="Game")     renderGameHUD(engine,t);
        else if(sname=="GameOver") renderGameOver(engine,t);
    };

    engine.run();
    return 0;
}
