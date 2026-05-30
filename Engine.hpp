#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <typeindex>
#include <any>
#include <queue>
#include <variant>

// ═══════════════════════════════════════════════════════════
//  MATH
// ═══════════════════════════════════════════════════════════
struct Vec2 {
    float x=0, y=0;
    Vec2() = default;
    Vec2(float x,float y):x(x),y(y){}
    Vec2 operator+(const Vec2& o) const { return {x+o.x,y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x,y-o.y}; }
    Vec2 operator*(float s)       const { return {x*s,y*s}; }
    Vec2 operator/(float s)       const { return {x/s,y/s}; }
    Vec2& operator+=(const Vec2& o){ x+=o.x;y+=o.y;return *this; }
    Vec2& operator-=(const Vec2& o){ x-=o.x;y-=o.y;return *this; }
    Vec2& operator*=(float s)     { x*=s;y*=s;return *this; }
    bool  operator==(const Vec2& o) const { return x==o.x&&y==o.y; }
    float length()    const { return std::sqrt(x*x+y*y); }
    float lengthSq()  const { return x*x+y*y; }
    Vec2  normalized()const { float l=length(); return l>0?Vec2{x/l,y/l}:Vec2{}; }
    float dot(const Vec2& o)  const { return x*o.x+y*o.y; }
    float cross(const Vec2& o)const { return x*o.y-y*o.x; }
    Vec2  perp()  const { return {-y,x}; }
    Vec2  lerp(const Vec2& b,float t) const { return *this*(1-t)+b*t; }
    static float distance(const Vec2& a,const Vec2& b){ return (b-a).length(); }
    static Vec2  lerp(const Vec2& a,const Vec2& b,float t){ return a*(1-t)+b*t; }
};

struct Rect {
    float x=0,y=0,w=0,h=0;
    Vec2  center() const { return {x+w/2,y+h/2}; }
    bool  overlaps(const Rect& o) const {
        return x<o.x+o.w&&x+w>o.x&&y<o.y+o.h&&y+h>o.y;
    }
    bool  contains(Vec2 p) const {
        return p.x>=x&&p.x<=x+w&&p.y>=y&&p.y<=y+h;
    }
    SDL_Rect toSDL() const { return {(int)x,(int)y,(int)w,(int)h}; }
};

struct Color {
    Uint8 r=255,g=255,b=255,a=255;
    SDL_Color toSDL() const { return {r,g,b,a}; }
    Color lerp(const Color& o,float t) const {
        return { (Uint8)(r+(o.r-r)*t),(Uint8)(g+(o.g-g)*t),
                 (Uint8)(b+(o.b-b)*t),(Uint8)(a+(o.a-a)*t) };
    }
    static Color fromHSV(float h,float s,float v);
};

// ═══════════════════════════════════════════════════════════
//  EASING & TWEEN
// ═══════════════════════════════════════════════════════════
namespace Ease {
    float linear(float t);
    float inQuad(float t);   float outQuad(float t);   float inOutQuad(float t);
    float inCubic(float t);  float outCubic(float t);  float inOutCubic(float t);
    float outBounce(float t);float inBack(float t);    float outElastic(float t);
    float inSine(float t);   float outSine(float t);   float inOutSine(float t);
}

class Tween {
public:
    using EaseFunc = std::function<float(float)>;
    Tween() = default;
    Tween(float from,float to,float dur,EaseFunc e=Ease::linear);
    void  update(float dt);
    float value()  const { return _value; }
    bool  done()   const { return _done; }
    float progress()const{ return _duration>0?std::min(_elapsed/_duration,1.f):1.f; }
    void  reset();
    void  reverse();
    Tween& onComplete(std::function<void()> cb){ _onComplete=cb; return *this; }
private:
    float _from=0,_to=1,_duration=1,_elapsed=0,_value=0;
    bool  _done=false;
    EaseFunc _ease=Ease::linear;
    std::function<void()> _onComplete;
};

// ═══════════════════════════════════════════════════════════
//  INPUT
// ═══════════════════════════════════════════════════════════
class Input {
public:
    void update();
    bool isKeyDown(SDL_Scancode k)     const;
    bool isKeyPressed(SDL_Scancode k)  const;
    bool isKeyReleased(SDL_Scancode k) const;
    bool isMouseButtonDown(int b)      const;
    bool isMouseButtonPressed(int b)   const;
    bool isMouseButtonReleased(int b)  const;
    Vec2 mousePosition()               const { return _mousePos; }
    Vec2 mouseScroll()                 const { return _scroll; }
    bool quitRequested()               const { return _quit; }
    // Named axes (like Unity Input.GetAxis)
    float getAxis(const std::string& name) const;
private:
    const Uint8* _keys=nullptr;
    Uint8 _prevKeys[SDL_NUM_SCANCODES]{};
    Uint8 _mouseButtons=0,_prevMouseButtons=0;
    Vec2  _mousePos{},_scroll{};
    bool  _quit=false;
};

// ═══════════════════════════════════════════════════════════
//  EVENT SYSTEM  (like Unity's UnityEvent / SendMessage)
// ═══════════════════════════════════════════════════════════
class EventBus {
public:
    using HandlerID = uint32_t;
    template<typename T>
    HandlerID subscribe(std::function<void(const T&)> handler){
        auto& vec = _handlers[std::type_index(typeid(T))];
        HandlerID id = _nextID++;
        vec.push_back({id,[handler](const std::any& e){
            handler(std::any_cast<const T&>(e));
        }});
        return id;
    }
    template<typename T>
    void publish(const T& event){
        auto it = _handlers.find(std::type_index(typeid(T)));
        if(it==_handlers.end()) return;
        for(auto& [id,fn]:it->second) fn(std::any(event));
    }
    void unsubscribe(std::type_index type,HandlerID id);
private:
    struct Entry { HandlerID id; std::function<void(const std::any&)> fn; };
    std::unordered_map<std::type_index,std::vector<Entry>> _handlers;
    HandlerID _nextID=1;
};

// ═══════════════════════════════════════════════════════════
//  RENDERER
// ═══════════════════════════════════════════════════════════
class Renderer {
public:
    Renderer(SDL_Renderer* r,int w,int h):_r(r),_winW(w),_winH(h){}
    void clear(Color c={20,20,30,255});
    void present();

    void drawRect    (Rect rect,Color c,bool filled=false);
    void drawRectEx  (Rect rect,Color c,float angleDeg,bool filled=false);
    void drawCircle  (Vec2 center,float radius,Color c,bool filled=false);
    void drawLine    (Vec2 a,Vec2 b,Color c,int thickness=1);
    void drawPoint   (Vec2 p,Color c);
    void drawPolygon (const std::vector<Vec2>& pts,Color c,bool filled=false);
    void drawGrid    (float cellW,float cellH,Color c);
    void drawText    (const std::string& text,Vec2 pos,Color c,int sz=16,bool screenSpace=false);
    Vec2 measureText (const std::string& text,int sz=16);

    // Camera
    void setCamera(Vec2 pos,float zoom=1.f){_camera=pos;_zoom=zoom;}
    Vec2  camera()    const{return _camera;}
    float zoom()      const{return _zoom;}
    Vec2  worldToScreen(Vec2 w) const;
    Vec2  screenToWorld(Vec2 s) const;

    int winW() const{return _winW;}
    int winH() const{return _winH;}
    SDL_Renderer* raw() const{return _r;}

private:
    SDL_Renderer* _r;
    int _winW,_winH;
    Vec2  _camera{};
    float _zoom=1.f;
    std::unordered_map<int,TTF_Font*> _fontCache;
    TTF_Font* getFont(int sz);
};

// ═══════════════════════════════════════════════════════════
//  ECS
// ═══════════════════════════════════════════════════════════
using EntityID = uint32_t;

class Component {
public:
    virtual ~Component()=default;
    EntityID ownerID=0;
    bool enabled=true;
};

class Entity {
public:
    explicit Entity(EntityID id):id(id){}
    EntityID    id;
    std::string tag;
    std::string layer{"Default"};
    bool        active=true;

    template<typename T,typename...Args>
    T& addComponent(Args&&...args){
        auto c=std::make_unique<T>(std::forward<Args>(args)...);
        c->ownerID=id;
        T& ref=*c;
        _components[typeid(T).name()]=std::move(c);
        return ref;
    }
    template<typename T> T* getComponent(){
        auto it=_components.find(typeid(T).name());
        return it!=_components.end()?static_cast<T*>(it->second.get()):nullptr;
    }
    template<typename T> const T* getComponent() const{
        auto it=_components.find(typeid(T).name());
        return it!=_components.end()?static_cast<const T*>(it->second.get()):nullptr;
    }
    template<typename T> bool hasComponent() const{
        return _components.count(typeid(T).name())>0;
    }
    template<typename T> void removeComponent(){
        _components.erase(typeid(T).name());
    }
private:
    std::unordered_map<std::string,std::unique_ptr<Component>> _components;
};

// ═══════════════════════════════════════════════════════════
//  BUILT-IN COMPONENTS
// ═══════════════════════════════════════════════════════════
struct Transform : Component {
    Vec2  position{};
    Vec2  scale{1,1};
    float rotation=0;      // degrees
    // Hierarchy
    EntityID parent=0;
    Vec2 worldPosition() const { return position; } // extended if parented
};

struct Velocity : Component {
    Vec2  linear{};
    float angular=0;
    float damping=0;
};

struct Sprite : Component {
    Rect   src{};
    Color  tint{255,255,255,255};
    bool   visible=true;
    int    layer=0;
    Color  fillColor{100,180,255,255};
    bool   useFill=true;
    Vec2   size{32,32};
    Vec2   pivot{0.5f,0.5f};  // normalized pivot (0.5=center)
    bool   flipX=false,flipY=false;
};

struct Collider : Component {
    Rect   bounds{};
    bool   isTrigger=false;
    std::string physicsLayer{"Default"};
    bool   _colliding=false;
    EntityID _collidingWith=0;
};

struct Script : Component {
    std::function<void(Entity&,float)> onUpdate;
    std::function<void(Entity&)>       onStart;
    std::function<void(Entity&,EntityID)> onCollision;
    std::function<void(Entity&,EntityID)> onTrigger;
    bool _started=false;
};

// ═══════════════════════════════════════════════════════════
//  RIGIDBODY 2D  (Unity-like)
// ═══════════════════════════════════════════════════════════
struct Rigidbody2D : Component {
    enum class BodyType { Dynamic, Kinematic, Static };

    BodyType bodyType  = BodyType::Dynamic;
    float    mass      = 1.f;
    float    gravityScale = 1.f;
    float    linearDrag   = 0.f;   // air resistance
    float    angularDrag  = 0.05f;
    float    bounciness   = 0.f;   // restitution [0-1]
    float    friction     = 0.3f;

    Vec2  velocity{};
    float angularVelocity=0;
    bool  freezeRotation=false;
    bool  isGrounded=false;

    // Accumulated forces this frame
    Vec2  _forceAccum{};
    float _torqueAccum=0;

    void addForce(Vec2 f)         { _forceAccum+=f; }
    void addImpulse(Vec2 imp)     { velocity+=imp/mass; }
    void addTorque(float t)       { _torqueAccum+=t; }
    void setVelocity(Vec2 v)      { velocity=v; }
};

// ═══════════════════════════════════════════════════════════
//  CAMERA 2D  (Unity-like)
// ═══════════════════════════════════════════════════════════
struct Camera2D : Component {
    float zoom        = 1.f;
    float smoothSpeed = 5.f;   // lerp speed toward target
    EntityID followTarget = 0;
    Vec2  offset{};
    Vec2  deadzone{60,40};     // pixels — no movement inside deadzone
    bool  clampToWorld=false;
    Rect  worldBounds{};       // used if clampToWorld
    // Shake
    float shakeDuration=0;
    float shakeMagnitude=0;
    float _shakeTimer=0;
    Vec2  _shakeOffset{};
    // Computed position
    Vec2  _position{};

    void shake(float duration,float magnitude){
        shakeDuration=duration;shakeMagnitude=magnitude;_shakeTimer=duration;
    }
};

// ═══════════════════════════════════════════════════════════
//  PARTICLE EMITTER
// ═══════════════════════════════════════════════════════════
struct Particle {
    Vec2  pos{},vel{};
    Color color{};
    float life=1.f,maxLife=1.f,size=4.f,gravity=0.f,rotation=0.f,angularVel=0.f;
};

class ParticleEmitter : public Component {
public:
    struct Config {
        Vec2  posOffset{};
        Vec2  velMin{-50,-100},velMax{50,-200};
        Color colorStart{255,200,50,255};
        Color colorEnd  {255,50,0,0};
        float lifeMin=0.5f,lifeMax=1.2f;
        float sizeStart=6.f,sizeEnd=1.f;
        float gravity=200.f;
        int   burstCount=0;
        float rate=30.f;
        bool  active=true;
        bool  worldSpace=true;
    } config;

    void burst(int n=-1);
    void update(float dt);
    void render(Renderer& r,Vec2 worldPos);
    int  count() const { return (int)_particles.size(); }

private:
    std::vector<Particle> _particles;
    float _accumulator=0;
    static float randF(float lo,float hi);
};

// ═══════════════════════════════════════════════════════════
//  ANIMATOR
// ═══════════════════════════════════════════════════════════
struct AnimFrame { Rect src; float duration; };
struct Animation { std::string name; std::vector<AnimFrame> frames; bool loop=true; };

class Animator : public Component {
public:
    void addAnimation(Animation a);
    void play(const std::string& name);
    void update(float dt);
    Rect currentFrame() const;
    const std::string& currentAnim() const { return _current; }
    bool finished() const { return _finished; }
    float speed=1.f;
private:
    std::unordered_map<std::string,Animation> _anims;
    std::string _current;
    int   _frameIdx=0;
    float _elapsed=0;
    bool  _finished=false;
};

// ═══════════════════════════════════════════════════════════
//  TILEMAP  (Unity Tilemap-like)
// ═══════════════════════════════════════════════════════════
struct Tile {
    int   id=0;       // 0 = empty
    Color color{120,120,130,255};
    bool  solid=true;
    bool  visible=true;
};

class Tilemap : public Component {
public:
    Tilemap(int cols,int rows,float tileW,float tileH);

    void  setTile(int col,int row,Tile t);
    Tile* getTile(int col,int row);
    void  setTileColor(int col,int row,Color c);
    void  fill(Tile t);
    void  clear();

    // World ↔ tile coords
    Vec2  tileToWorld(int col,int row) const;
    bool  worldToTile(Vec2 worldPos,int& col,int& row) const;
    bool  isSolid(int col,int row) const;
    bool  isSolidAt(Vec2 worldPos) const;
    // AABB vs tilemap sweep
    void  resolveCollision(Vec2& pos,Vec2& vel,Vec2 size,bool& onGround) const;

    int cols,rows;
    float tileW,tileH;
    Vec2  origin{};          // world offset

    void render(Renderer& r) const;

private:
    std::vector<Tile> _tiles;
    int idx(int c,int r) const { return r*cols+c; }
    bool valid(int c,int r) const { return c>=0&&c<cols&&r>=0&&r<rows; }
};

// ═══════════════════════════════════════════════════════════
//  UI SYSTEM  (Unity uGUI-like)
// ═══════════════════════════════════════════════════════════
enum class UIAnchor { TopLeft,Top,TopRight,Left,Center,Right,BottomLeft,Bottom,BottomRight };

struct UIElement : Component {
    Rect   rect{};           // position + size (screen space)
    UIAnchor anchor=UIAnchor::TopLeft;
    bool   visible=true;
    int    zOrder=0;
};

struct UILabel : Component {
    std::string text;
    Color       color{255,255,255,255};
    int         fontSize=16;
    bool        visible=true;
};

struct UIButton : Component {
    std::string             label;
    Color                   normalColor {80,100,160,255};
    Color                   hoverColor  {100,130,200,255};
    Color                   pressColor  {60, 80,140,255};
    Color                   textColor   {255,255,255,255};
    int                     fontSize=16;
    bool                    visible=true;
    std::function<void()>   onClick;
    // runtime state
    bool _hovered=false,_pressed=false;
};

struct UIProgressBar : Component {
    float value=0.f;          // [0..1]
    Color bgColor  {40,40,50,255};
    Color fillColor{80,200,120,255};
    Color borderColor{180,180,180,200};
    bool  visible=true;
};

struct UIPanel : Component {
    Color  bgColor{30,30,45,220};
    Color  borderColor{100,100,150,255};
    int    borderWidth=1;
    bool   visible=true;
};

// ═══════════════════════════════════════════════════════════
//  AUDIO SOURCE / LISTENER
// ═══════════════════════════════════════════════════════════
struct AudioSource : Component {
    float volume=1.f;
    float pitch=1.f;
    float minDistance=100.f;   // full volume within
    float maxDistance=600.f;   // silent beyond
    bool  loop=false;
    bool  playOnAwake=false;
    bool  is3D=false;          // spatial audio
    // runtime
    bool  _playing=false;
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();
    bool  init();
    void  playBeep(int freq=440,int ms=50,float vol=0.5f);
    void  playTone(int freq,int ms,float vol,int waveform=0); // 0=sine,1=square,2=triangle
    void  setMasterVolume(float v);
    void  update3DAudio(Vec2 listenerPos,class Scene& scene);
private:
    bool _ok=false;
    std::vector<Mix_Chunk*> _managed;
};

// ═══════════════════════════════════════════════════════════
//  PREFAB SYSTEM  (like Unity Prefabs)
// ═══════════════════════════════════════════════════════════
using PrefabFactory = std::function<Entity&(class Scene&, Vec2)>;

class PrefabRegistry {
public:
    static PrefabRegistry& instance();
    void   registerPrefab(const std::string& name, PrefabFactory factory);
    Entity& instantiate(const std::string& name, class Scene& scene, Vec2 pos={});
    bool    has(const std::string& name) const;
private:
    std::unordered_map<std::string, PrefabFactory> _prefabs;
};

// Shorthand (like Unity Instantiate)
Entity& Instantiate(const std::string& prefabName, class Scene& scene, Vec2 pos={});

// ═══════════════════════════════════════════════════════════
//  SCENE
// ═══════════════════════════════════════════════════════════
class Scene {
public:
    Entity& createEntity(const std::string& tag="");
    void    destroyEntity(EntityID id);
    Entity* findByTag(const std::string& tag);
    std::vector<Entity*> findAllByTag(const std::string& tag);
    std::vector<Entity*> allEntities();
    std::vector<Entity*> entitiesInRadius(Vec2 center,float radius);
    void    clear();
    std::string name;
    std::function<void()> onLoad;
    std::function<void()> onUnload;
private:
    std::vector<std::unique_ptr<Entity>> _entities;
    EntityID _nextID=1;
    std::vector<EntityID> _toDestroy;
    friend class Engine;
    void flushDestroyed();
};

// ═══════════════════════════════════════════════════════════
//  SCENE MANAGER  (Unity SceneManager)
// ═══════════════════════════════════════════════════════════
class SceneManager {
public:
    void registerScene(const std::string& name, std::function<void(Scene&)> loader);
    void loadScene(const std::string& name);   // immediate
    void loadSceneAsync(const std::string& name); // queued (next frame)
    Scene& current() { return *_current; }
    const std::string& currentName() const { return _currentName; }
    bool hasPending() const { return !_pending.empty(); }
    void flushPending();
private:
    std::unordered_map<std::string,std::function<void(Scene&)>> _loaders;
    std::unique_ptr<Scene> _current;
    std::string _currentName;
    std::queue<std::string> _pending;
};

// ═══════════════════════════════════════════════════════════
//  PHYSICS SYSTEM  (extended Rigidbody2D support)
// ═══════════════════════════════════════════════════════════
struct CollisionEvent {
    EntityID a,b;
    Vec2 normal;
    float penetration;
};

class PhysicsSystem {
public:
    Vec2  gravity{0,980.f};   // pixels/s² (like Unity's 9.8 m/s²)
    std::vector<std::string> layerIgnoreMatrix; // "LayerA:LayerB"

    void update(Scene& scene,float dt);
    const std::vector<CollisionEvent>& collisions() const { return _collisions; }
    bool ignoresLayer(const std::string& a,const std::string& b) const;

private:
    std::vector<CollisionEvent> _collisions;
    void integrateRigidbodies(Scene& scene,float dt);
    void resolveCollisions(Scene& scene);
    Vec2 computeAABBNormal(const Rect& a,const Rect& b,float& penetration);
};

// ═══════════════════════════════════════════════════════════
//  CAMERA SYSTEM
// ═══════════════════════════════════════════════════════════
class CameraSystem {
public:
    void update(Scene& scene,Renderer& r,float dt);
};

// ═══════════════════════════════════════════════════════════
//  RENDER SYSTEM
// ═══════════════════════════════════════════════════════════
class RenderSystem {
public:
    RenderSystem(Renderer& r):_r(r){}
    void render(Scene& scene);
    bool debugDraw=false;
private:
    Renderer& _r;
};

// ═══════════════════════════════════════════════════════════
//  UI SYSTEM (renderer + input handling)
// ═══════════════════════════════════════════════════════════
class UISystem {
public:
    UISystem(Renderer& r,Input& inp):_r(r),_inp(inp){}
    void update(Scene& scene);
    void render(Scene& scene);
private:
    Renderer& _r;
    Input&    _inp;
    Vec2  anchorOffset(UIAnchor a,Rect rect) const;
};

// ═══════════════════════════════════════════════════════════
//  SCRIPT SYSTEM
// ═══════════════════════════════════════════════════════════
class ScriptSystem {
public:
    void update(Scene& scene,float dt,const std::vector<CollisionEvent>& cols);
};

// ═══════════════════════════════════════════════════════════
//  CLOCK
// ═══════════════════════════════════════════════════════════
class Clock {
public:
    void  tick();
    float dt()     const { return _dt; }
    float fps()    const { return _dt>0?1.f/_dt:0; }
    float time()   const { return _time; }
    Uint64 ticks() const { return _ticks; }
private:
    Uint64 _prev=0;
    float  _dt=0,_time=0;
    Uint64 _ticks=0;
};

// ═══════════════════════════════════════════════════════════
//  ENGINE CONFIG
// ═══════════════════════════════════════════════════════════
struct EngineConfig {
    std::string title  = "Engine2D";
    int         width  = 800;
    int         height = 600;
    int         targetFPS = 60;
    bool        vsync  = true;
    bool        showFPS= true;
};

// ═══════════════════════════════════════════════════════════
//  ENGINE
// ═══════════════════════════════════════════════════════════
class Engine {
public:
    explicit Engine(EngineConfig cfg={});
    ~Engine();

    void run();
    void quit() { _running=false; }

    Scene&        scene()        { return _sceneMgr.current(); }
    Input&        input()        { return _input; }
    Renderer&     renderer()     { return *_renderer; }
    Clock&        clock()        { return _clock; }
    PhysicsSystem&physics()      { return _physics; }
    SceneManager& sceneManager() { return _sceneMgr; }
    AudioManager& audio()        { return _audio; }
    EventBus&     events()       { return _events; }
    PrefabRegistry& prefabs()    { return PrefabRegistry::instance(); }

    std::function<void()>      onStart;
    std::function<void(float)> onUpdate;
    std::function<void()>      onRender;

    int windowWidth()  const { return _cfg.width; }
    int windowHeight() const { return _cfg.height; }

private:
    EngineConfig  _cfg;
    SDL_Window*   _window  =nullptr;
    SDL_Renderer* _sdlR    =nullptr;
    bool          _running =false;

    SceneManager  _sceneMgr;
    Input         _input;
    std::unique_ptr<Renderer> _renderer;
    Clock         _clock;
    PhysicsSystem _physics;
    CameraSystem  _cameraSys;
    RenderSystem* _renderSys=nullptr;
    UISystem*     _uiSys    =nullptr;
    ScriptSystem  _scriptSys;
    AudioManager  _audio;
    EventBus      _events;
};

// ═══════════════════════════════════════════════════════════
//  ADVANCED UI RENDERING  (rounded rects, gradients, glow)
// ═══════════════════════════════════════════════════════════
class UIRenderer {
public:
    explicit UIRenderer(Renderer& r) : _r(r) {}

    // Rounded rectangle (filled + optional stroke)
    void drawRoundedRect(Rect rect, float radius, Color fill,
                         Color stroke={0,0,0,0}, int strokeW=0);
    // Vertical gradient rect
    void drawGradientRect(Rect rect, Color top, Color bottom);
    // Horizontal gradient rect
    void drawGradientRectH(Rect rect, Color left, Color right);
    // Glow ring around rect
    void drawGlow(Rect rect, float radius, Color color, int layers=6);
    // Drop shadow
    void drawShadow(Rect rect, float radius, int offsetX=3, int offsetY=4, int blur=6);
    // Icon-style circle badge
    void drawBadge(Vec2 center, float r, Color fill, Color border);
    // Progress bar with rounded ends + gradient
    void drawProgressBar(Rect rect, float value, Color bg, Color fill,
                         Color fillEnd={0,0,0,0}, float cornerR=6.f);
    // Tooltip box
    void drawTooltip(Vec2 pos, const std::string& text, int fontSize=13);

private:
    Renderer& _r;
    void drawRoundedRectFilled(Rect rect, float radius, Color c);
    void drawRoundedRectOutline(Rect rect, float radius, Color c, int w);
    void hline(int x1,int x2,int y,Color c);
};
