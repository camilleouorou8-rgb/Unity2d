#include "Engine.hpp"
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cassert>
#include <sstream>
#include <iomanip>

// ═══════════════════════════════════════════════════════════
//  COLOR
// ═══════════════════════════════════════════════════════════
Color Color::fromHSV(float h,float s,float v){
    float r=v,g=v,b=v;
    if(s>0){
        h=std::fmod(h,360.f)/60.f;
        int i=(int)h;
        float f=h-i,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
        switch(i){
            case 0:r=v;g=t;b=p;break; case 1:r=q;g=v;b=p;break;
            case 2:r=p;g=v;b=t;break; case 3:r=p;g=q;b=v;break;
            case 4:r=t;g=p;b=v;break; default:r=v;g=p;b=q;
        }
    }
    return {(Uint8)(r*255),(Uint8)(g*255),(Uint8)(b*255),255};
}

// ═══════════════════════════════════════════════════════════
//  EASING
// ═══════════════════════════════════════════════════════════
float Ease::linear   (float t){return t;}
float Ease::inQuad   (float t){return t*t;}
float Ease::outQuad  (float t){return t*(2-t);}
float Ease::inOutQuad(float t){return t<.5f?2*t*t:-1+(4-2*t)*t;}
float Ease::inCubic  (float t){return t*t*t;}
float Ease::outCubic (float t){float s=t-1;return s*s*s+1;}
float Ease::inOutCubic(float t){return t<.5f?4*t*t*t:(t-1)*(2*t-2)*(2*t-2)+1;}
float Ease::inSine   (float t){return 1-std::cos(t*(float)M_PI/2);}
float Ease::outSine  (float t){return std::sin(t*(float)M_PI/2);}
float Ease::inOutSine(float t){return -(std::cos((float)M_PI*t)-1)/2;}
float Ease::outBounce(float t){
    if(t<1/2.75f)return 7.5625f*t*t;
    if(t<2/2.75f){t-=1.5f/2.75f;return 7.5625f*t*t+.75f;}
    if(t<2.5f/2.75f){t-=2.25f/2.75f;return 7.5625f*t*t+.9375f;}
    t-=2.625f/2.75f;return 7.5625f*t*t+.984375f;
}
float Ease::inBack   (float t){return t*t*(2.70158f*t-1.70158f);}
float Ease::outElastic(float t){
    if(t==0||t==1)return t;
    return std::pow(2,-10*t)*std::sin((t*10-.75f)*(float)(2*M_PI)/3)+1;
}

// ═══════════════════════════════════════════════════════════
//  TWEEN
// ═══════════════════════════════════════════════════════════
Tween::Tween(float from,float to,float dur,EaseFunc e)
    :_from(from),_to(to),_duration(dur),_ease(e){_value=from;}
void Tween::update(float dt){
    if(_done)return;
    _elapsed+=dt;
    float t=std::min(_elapsed/_duration,1.f);
    _value=_from+(_to-_from)*_ease(t);
    if(t>=1.f){_done=true;if(_onComplete)_onComplete();}
}
void Tween::reset()  {_elapsed=0;_done=false;_value=_from;}
void Tween::reverse(){std::swap(_from,_to);reset();}

// ═══════════════════════════════════════════════════════════
//  INPUT
// ═══════════════════════════════════════════════════════════
void Input::update(){
    std::memcpy(_prevKeys,_keys?_keys:_prevKeys,SDL_NUM_SCANCODES);
    _prevMouseButtons=_mouseButtons;
    _scroll={};
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT)_quit=true;
        if(e.type==SDL_MOUSEWHEEL){_scroll.x=(float)e.wheel.x;_scroll.y=(float)e.wheel.y;}
    }
    _keys=SDL_GetKeyboardState(nullptr);
    int mx,my;
    _mouseButtons=(Uint8)SDL_GetMouseState(&mx,&my);
    _mousePos={(float)mx,(float)my};
}
bool Input::isKeyDown    (SDL_Scancode k)const{return _keys&&_keys[k];}
bool Input::isKeyPressed (SDL_Scancode k)const{return _keys&&_keys[k]&&!_prevKeys[k];}
bool Input::isKeyReleased(SDL_Scancode k)const{return _keys&&!_keys[k]&&_prevKeys[k];}
bool Input::isMouseButtonDown    (int b)const{return (_mouseButtons&SDL_BUTTON(b))!=0;}
bool Input::isMouseButtonPressed (int b)const{return ((_mouseButtons&SDL_BUTTON(b))!=0)&&((_prevMouseButtons&SDL_BUTTON(b))==0);}
bool Input::isMouseButtonReleased(int b)const{return ((_mouseButtons&SDL_BUTTON(b))==0)&&((_prevMouseButtons&SDL_BUTTON(b))!=0);}
float Input::getAxis(const std::string& name)const{
    if(name=="Horizontal"){
        float v=0;
        if(_keys&&(_keys[SDL_SCANCODE_RIGHT]||_keys[SDL_SCANCODE_D]))v+=1;
        if(_keys&&(_keys[SDL_SCANCODE_LEFT] ||_keys[SDL_SCANCODE_A]))v-=1;
        return v;
    }
    if(name=="Vertical"){
        float v=0;
        if(_keys&&(_keys[SDL_SCANCODE_DOWN]||_keys[SDL_SCANCODE_S]))v+=1;
        if(_keys&&(_keys[SDL_SCANCODE_UP]  ||_keys[SDL_SCANCODE_W]))v-=1;
        return v;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════
//  EVENT BUS
// ═══════════════════════════════════════════════════════════
void EventBus::unsubscribe(std::type_index type,HandlerID id){
    auto it=_handlers.find(type);
    if(it==_handlers.end())return;
    auto& vec=it->second;
    vec.erase(std::remove_if(vec.begin(),vec.end(),
        [id](const Entry& e){return e.id==id;}),vec.end());
}

// ═══════════════════════════════════════════════════════════
//  RENDERER
// ═══════════════════════════════════════════════════════════
TTF_Font* Renderer::getFont(int sz){
    auto it=_fontCache.find(sz);
    if(it!=_fontCache.end())return it->second;
    // try bundled fallback fonts in order
    const char* paths[]={
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/opentype/unifont/unifont.otf",
        nullptr
    };
    for(int i=0;paths[i];i++){
        TTF_Font* f=TTF_OpenFont(paths[i],sz);
        if(f){_fontCache[sz]=f;return f;}
    }
    return nullptr;
}

Vec2 Renderer::worldToScreen(Vec2 w)const{
    return {(w.x-_camera.x)*_zoom+_winW/2.f,
            (w.y-_camera.y)*_zoom+_winH/2.f};
}
Vec2 Renderer::screenToWorld(Vec2 s)const{
    return {(s.x-_winW/2.f)/_zoom+_camera.x,
            (s.y-_winH/2.f)/_zoom+_camera.y};
}

void Renderer::clear(Color c){
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    SDL_RenderClear(_r);
}
void Renderer::present(){ SDL_RenderPresent(_r); }

void Renderer::drawRect(Rect rect,Color c,bool filled){
    Vec2 sp=worldToScreen({rect.x,rect.y});
    SDL_Rect sr{(int)sp.x,(int)sp.y,(int)(rect.w*_zoom),(int)(rect.h*_zoom)};
    SDL_SetRenderDrawBlendMode(_r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    if(filled)SDL_RenderFillRect(_r,&sr);
    else SDL_RenderDrawRect(_r,&sr);
}

void Renderer::drawRectEx(Rect rect,Color c,float angleDeg,bool filled){
    // Rotated rect via polygon
    Vec2 center=worldToScreen({rect.x+rect.w/2,rect.y+rect.h/2});
    float hw=rect.w*_zoom/2,hh=rect.h*_zoom/2;
    float rad=angleDeg*(float)M_PI/180.f;
    float ca=std::cos(rad),sa=std::sin(rad);
    std::vector<Vec2> pts={
        {center.x+(-hw*ca-(-hh)*sa),center.y+(-hw*sa+(-hh)*ca)},
        {center.x+( hw*ca-(-hh)*sa),center.y+( hw*sa+(-hh)*ca)},
        {center.x+( hw*ca-  hh *sa),center.y+( hw*sa+  hh *ca)},
        {center.x+(-hw*ca-  hh *sa),center.y+(-hw*sa+  hh *ca)},
    };
    drawPolygon(pts,c,filled);
}

void Renderer::drawCircle(Vec2 center,float radius,Color c,bool filled){
    Vec2 sc=worldToScreen(center);
    float r=radius*_zoom;
    SDL_SetRenderDrawBlendMode(_r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    int ri=(int)r;
    if(filled){
        for(int dy=-ri;dy<=ri;dy++){
            int dx=(int)std::sqrt(std::max(0.f,r*r-(float)(dy*dy)));
            SDL_RenderDrawLine(_r,(int)sc.x-dx,(int)sc.y+dy,(int)sc.x+dx,(int)sc.y+dy);
        }
    } else {
        int x=ri,y=0,d=1-ri;
        while(x>=y){
            SDL_RenderDrawPoint(_r,(int)sc.x+x,(int)sc.y+y);SDL_RenderDrawPoint(_r,(int)sc.x-x,(int)sc.y+y);
            SDL_RenderDrawPoint(_r,(int)sc.x+x,(int)sc.y-y);SDL_RenderDrawPoint(_r,(int)sc.x-x,(int)sc.y-y);
            SDL_RenderDrawPoint(_r,(int)sc.x+y,(int)sc.y+x);SDL_RenderDrawPoint(_r,(int)sc.x-y,(int)sc.y+x);
            SDL_RenderDrawPoint(_r,(int)sc.x+y,(int)sc.y-x);SDL_RenderDrawPoint(_r,(int)sc.x-y,(int)sc.y-x);
            y++;if(d<0)d+=2*y+1;else{x--;d+=2*(y-x)+1;}
        }
    }
}

void Renderer::drawLine(Vec2 a,Vec2 b,Color c,int thickness){
    Vec2 sa=worldToScreen(a),sb=worldToScreen(b);
    SDL_SetRenderDrawBlendMode(_r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    if(thickness<=1){SDL_RenderDrawLine(_r,(int)sa.x,(int)sa.y,(int)sb.x,(int)sb.y);return;}
    Vec2 dir=(b-a).normalized().perp();
    for(int i=-(thickness/2);i<=thickness/2;i++){
        Vec2 off=dir*(float)i;
        Vec2 pa=worldToScreen(a+off),pb=worldToScreen(b+off);
        SDL_RenderDrawLine(_r,(int)pa.x,(int)pa.y,(int)pb.x,(int)pb.y);
    }
}

void Renderer::drawPoint(Vec2 p,Color c){
    Vec2 sp=worldToScreen(p);
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    SDL_RenderDrawPoint(_r,(int)sp.x,(int)sp.y);
}

void Renderer::drawPolygon(const std::vector<Vec2>& pts,Color c,bool filled){
    if(pts.size()<2)return;
    SDL_SetRenderDrawBlendMode(_r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    if(filled){
        // Scanline fill (screen-space pts assumed)
        float minY=pts[0].y,maxY=pts[0].y;
        for(auto& p:pts){minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);}
        for(int y=(int)minY;y<=(int)maxY;y++){
            std::vector<float> xs;
            int n=(int)pts.size();
            for(int i=0;i<n;i++){
                Vec2 a=pts[i],b=pts[(i+1)%n];
                if((a.y<=y&&b.y>y)||(b.y<=y&&a.y>y)){
                    xs.push_back(a.x+(y-a.y)/(b.y-a.y)*(b.x-a.x));
                }
            }
            std::sort(xs.begin(),xs.end());
            for(int i=0;i+1<(int)xs.size();i+=2)
                SDL_RenderDrawLine(_r,(int)xs[i],y,(int)xs[i+1],y);
        }
    } else {
        for(int i=0;i<(int)pts.size();i++){
            Vec2 a=pts[i],b=pts[(i+1)%pts.size()];
            SDL_RenderDrawLine(_r,(int)a.x,(int)a.y,(int)b.x,(int)b.y);
        }
    }
}

void Renderer::drawGrid(float cellW,float cellH,Color c){
    SDL_SetRenderDrawColor(_r,c.r,c.g,c.b,c.a);
    for(float x=0;x<_winW;x+=cellW*_zoom)
        SDL_RenderDrawLine(_r,(int)x,0,(int)x,_winH);
    for(float y=0;y<_winH;y+=cellH*_zoom)
        SDL_RenderDrawLine(_r,0,(int)y,_winW,(int)y);
}

void Renderer::drawText(const std::string& text,Vec2 pos,Color c,int sz,bool screenSpace){
    if(text.empty())return;
    TTF_Font* font=getFont(sz);
    if(!font)return;
    SDL_Surface* surf=TTF_RenderUTF8_Blended(font,text.c_str(),c.toSDL());
    if(!surf)return;
    SDL_Texture* tex=SDL_CreateTextureFromSurface(_r,surf);
    SDL_FreeSurface(surf);
    if(!tex)return;
    int w,h; SDL_QueryTexture(tex,nullptr,nullptr,&w,&h);
    Vec2 sp=screenSpace?pos:worldToScreen(pos);
    SDL_Rect dst{(int)sp.x,(int)sp.y,w,h};
    SDL_RenderCopy(_r,tex,nullptr,&dst);
    SDL_DestroyTexture(tex);
}

Vec2 Renderer::measureText(const std::string& text,int sz){
    TTF_Font* font=getFont(sz);
    if(!font||text.empty())return{};
    int w=0,h=0;TTF_SizeUTF8(font,text.c_str(),&w,&h);
    return{(float)w,(float)h};
}

// ═══════════════════════════════════════════════════════════
//  SCENE
// ═══════════════════════════════════════════════════════════
Entity& Scene::createEntity(const std::string& tag){
    auto e=std::make_unique<Entity>(_nextID++);
    e->tag=tag;
    Entity& ref=*e;
    _entities.push_back(std::move(e));
    return ref;
}
void Scene::destroyEntity(EntityID id){_toDestroy.push_back(id);}
void Scene::flushDestroyed(){
    for(auto id:_toDestroy)
        _entities.erase(std::remove_if(_entities.begin(),_entities.end(),
            [id](const std::unique_ptr<Entity>& e){return e->id==id;}),_entities.end());
    _toDestroy.clear();
}
Entity* Scene::findByTag(const std::string& tag){
    for(auto& e:_entities)if(e->tag==tag&&e->active)return e.get();
    return nullptr;
}
std::vector<Entity*> Scene::findAllByTag(const std::string& tag){
    std::vector<Entity*> out;
    for(auto& e:_entities)if(e->tag==tag&&e->active)out.push_back(e.get());
    return out;
}
std::vector<Entity*> Scene::allEntities(){
    std::vector<Entity*> out;
    for(auto& e:_entities)out.push_back(e.get());
    return out;
}
std::vector<Entity*> Scene::entitiesInRadius(Vec2 center,float radius){
    std::vector<Entity*> out;
    for(auto& e:_entities){
        if(!e->active)continue;
        auto* t=e->getComponent<Transform>();
        if(t&&Vec2::distance(t->position,center)<=radius)out.push_back(e.get());
    }
    return out;
}
void Scene::clear(){_entities.clear();_toDestroy.clear();}

// ═══════════════════════════════════════════════════════════
//  SCENE MANAGER
// ═══════════════════════════════════════════════════════════
void SceneManager::registerScene(const std::string& name,std::function<void(Scene&)> loader){
    _loaders[name]=loader;
    if(!_current){
        _current=std::make_unique<Scene>();
        _current->name=name;
        _currentName=name;
        loader(*_current);
        if(_current->onLoad)_current->onLoad();
    }
}
void SceneManager::loadScene(const std::string& name){
    auto it=_loaders.find(name);
    if(it==_loaders.end()){std::cerr<<"Scene not found: "<<name<<"\n";return;}
    if(_current&&_current->onUnload)_current->onUnload();
    _current=std::make_unique<Scene>();
    _current->name=name;
    _currentName=name;
    it->second(*_current);
    if(_current->onLoad)_current->onLoad();
}
void SceneManager::loadSceneAsync(const std::string& name){_pending.push(name);}
void SceneManager::flushPending(){
    if(_pending.empty())return;
    loadScene(_pending.front());
    _pending.pop();
}

// ═══════════════════════════════════════════════════════════
//  TILEMAP
// ═══════════════════════════════════════════════════════════
Tilemap::Tilemap(int c,int r,float tw,float th):cols(c),rows(r),tileW(tw),tileH(th){
    _tiles.resize(c*r);
}
void  Tilemap::setTile(int c,int r,Tile t){if(valid(c,r))_tiles[idx(c,r)]=t;}
Tile* Tilemap::getTile(int c,int r){return valid(c,r)?&_tiles[idx(c,r)]:nullptr;}
void  Tilemap::setTileColor(int c,int r,Color col){if(valid(c,r))_tiles[idx(c,r)].color=col;}
void  Tilemap::fill(Tile t){std::fill(_tiles.begin(),_tiles.end(),t);}
void  Tilemap::clear(){Tile empty;empty.id=0;empty.solid=false;fill(empty);}
Vec2  Tilemap::tileToWorld(int c,int r)const{return {origin.x+c*tileW,origin.y+r*tileH};}
bool  Tilemap::worldToTile(Vec2 w,int& c,int& r)const{
    c=(int)((w.x-origin.x)/tileW);r=(int)((w.y-origin.y)/tileH);
    return valid(c,r);
}
bool Tilemap::isSolid(int c,int r)const{return valid(c,r)&&_tiles[idx(c,r)].solid&&_tiles[idx(c,r)].id!=0;}
bool Tilemap::isSolidAt(Vec2 w)const{int c,r;return worldToTile(w,c,r)&&isSolid(c,r);}

void Tilemap::resolveCollision(Vec2& pos,Vec2& vel,Vec2 size,bool& onGround)const{
    onGround=false;
    // Check corners
    auto check=[&](Vec2 offset)->bool{
        int c,r;
        return worldToTile(pos+offset,c,r)&&isSolid(c,r);
    };
    float hw=size.x/2,hh=size.y/2;
    // Vertical
    if(vel.y>0){
        if(check({-hw+1,hh})||check({hw-1,hh})){
            int c,r; worldToTile({pos.x,pos.y+hh},c,r);
            pos.y=origin.y+r*tileH-hh;
            vel.y=0; onGround=true;
        }
    } else if(vel.y<0){
        if(check({-hw+1,-hh})||check({hw-1,-hh})){
            int c,r; worldToTile({pos.x,pos.y-hh},c,r);
            pos.y=origin.y+(r+1)*tileH+hh;
            vel.y=0;
        }
    }
    // Horizontal
    if(vel.x>0){
        if(check({hw,-(hh-2)})||check({hw,hh-2})){
            int c,r; worldToTile({pos.x+hw,pos.y},c,r);
            pos.x=origin.x+c*tileW-hw;
            vel.x=0;
        }
    } else if(vel.x<0){
        if(check({-hw,-(hh-2)})||check({-hw,hh-2})){
            int c,r; worldToTile({pos.x-hw,pos.y},c,r);
            pos.x=origin.x+(c+1)*tileW+hw;
            vel.x=0;
        }
    }
}

void Tilemap::render(Renderer& r)const{
    for(int row=0;row<rows;row++){
        for(int col=0;col<cols;col++){
            const Tile& t=_tiles[idx(col,row)];
            if(t.id==0||!t.visible)continue;
            Vec2 wp=tileToWorld(col,row);
            Rect rect{wp.x,wp.y,tileW,tileH};
            r.drawRect(rect,t.color,true);
            Color dark{(Uint8)(t.color.r/2),(Uint8)(t.color.g/2),(Uint8)(t.color.b/2),180};
            r.drawRect(rect,dark,false);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  PARTICLE EMITTER
// ═══════════════════════════════════════════════════════════
float ParticleEmitter::randF(float lo,float hi){return lo+(hi-lo)*((float)std::rand()/RAND_MAX);}

void ParticleEmitter::burst(int n){
    int count=(n<0)?config.burstCount:n;
    for(int i=0;i<count;i++){
        Particle p;
        p.pos=config.posOffset;
        p.vel={randF(config.velMin.x,config.velMax.x),randF(config.velMin.y,config.velMax.y)};
        p.maxLife=p.life=randF(config.lifeMin,config.lifeMax);
        p.size=config.sizeStart; p.color=config.colorStart;
        p.gravity=config.gravity;
        p.angularVel=randF(-180,180);
        _particles.push_back(p);
    }
}
void ParticleEmitter::update(float dt){
    if(!config.active)return;
    if(config.burstCount==0&&config.rate>0){
        _accumulator+=config.rate*dt;
        while(_accumulator>=1.f){burst(1);_accumulator-=1.f;}
    }
    for(auto& p:_particles){
        p.life-=dt; p.vel.y+=p.gravity*dt; p.pos+=p.vel*dt; p.rotation+=p.angularVel*dt;
        float t=1.f-(p.life/p.maxLife);
        p.color=config.colorStart.lerp(config.colorEnd,t);
        p.size=config.sizeStart+(config.sizeEnd-config.sizeStart)*t;
    }
    _particles.erase(std::remove_if(_particles.begin(),_particles.end(),
        [](const Particle& p){return p.life<=0;}),_particles.end());
}
void ParticleEmitter::render(Renderer& r,Vec2 worldPos){
    for(const auto& p:_particles){
        Rect rect{worldPos.x+p.pos.x-p.size/2,worldPos.y+p.pos.y-p.size/2,p.size,p.size};
        r.drawRect(rect,p.color,true);
    }
}

// ═══════════════════════════════════════════════════════════
//  ANIMATOR
// ═══════════════════════════════════════════════════════════
void Animator::addAnimation(Animation a){std::string n=a.name;_anims[n]=std::move(a);if(_current.empty())_current=n;}
void Animator::play(const std::string& name){if(_current==name)return;_current=name;_frameIdx=0;_elapsed=0;_finished=false;}
void Animator::update(float dt){
    auto it=_anims.find(_current);
    if(it==_anims.end()||it->second.frames.empty())return;
    const auto& anim=it->second;
    _elapsed+=dt*speed;
    if(_elapsed>=anim.frames[_frameIdx].duration){
        _elapsed-=anim.frames[_frameIdx].duration;
        _frameIdx++;
        if(_frameIdx>=(int)anim.frames.size()){
            if(anim.loop)_frameIdx=0;
            else{_frameIdx=(int)anim.frames.size()-1;_finished=true;}
        }
    }
}
Rect Animator::currentFrame()const{
    auto it=_anims.find(_current);
    if(it==_anims.end()||it->second.frames.empty())return{};
    return it->second.frames[_frameIdx].src;
}

// ═══════════════════════════════════════════════════════════
//  PHYSICS SYSTEM
// ═══════════════════════════════════════════════════════════
static const float FIXED_DT=1.f/60.f;
static float physAccum=0;

void PhysicsSystem::update(Scene& scene,float dt){
    physAccum+=dt;
    while(physAccum>=FIXED_DT){
        integrateRigidbodies(scene,FIXED_DT);
        resolveCollisions(scene);
        physAccum-=FIXED_DT;
    }
}

void PhysicsSystem::integrateRigidbodies(Scene& scene,float dt){
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        auto* rb=e->getComponent<Rigidbody2D>();
        auto* tr=e->getComponent<Transform>();
        if(!rb||!tr||rb->bodyType==Rigidbody2D::BodyType::Static)continue;
        if(rb->bodyType==Rigidbody2D::BodyType::Kinematic){
            rb->_forceAccum={};rb->_torqueAccum=0;continue;
        }
        // Gravity
        Vec2 grav=gravity*rb->gravityScale;
        rb->_forceAccum+=grav*rb->mass;
        // Integrate
        Vec2 accel=rb->_forceAccum/rb->mass;
        rb->velocity+=accel*dt;
        rb->velocity*=(1.f-rb->linearDrag*dt);
        tr->position+=rb->velocity*dt;
        if(!rb->freezeRotation){
            rb->angularVelocity+=rb->_torqueAccum/rb->mass*dt;
            rb->angularVelocity*=(1.f-rb->angularDrag*dt);
            tr->rotation+=rb->angularVelocity*dt;
        }
        rb->_forceAccum={};rb->_torqueAccum=0;
        rb->isGrounded=false;
        // Tilemap collision
        auto* tm=scene.findByTag("tilemap");
        if(tm){
            auto* tilemap=tm->getComponent<Tilemap>();
            auto* sp=e->getComponent<Sprite>();
            if(tilemap&&sp){
                bool og=false;
                tilemap->resolveCollision(tr->position,rb->velocity,sp->size,og);
                if(og)rb->isGrounded=true;
            }
        }
    }
}

Vec2 PhysicsSystem::computeAABBNormal(const Rect& a,const Rect& b,float& pen){
    float overlapX=std::min(a.x+a.w,b.x+b.w)-std::max(a.x,b.x);
    float overlapY=std::min(a.y+a.h,b.y+b.h)-std::max(a.y,b.y);
    if(overlapX<overlapY){pen=overlapX;return{a.center().x<b.center().x?-1.f:1.f,0};}
    else{pen=overlapY;return{0,a.center().y<b.center().y?-1.f:1.f};}
}

void PhysicsSystem::resolveCollisions(Scene& scene){
    _collisions.clear();
    auto entities=scene.allEntities();
    for(int i=0;i<(int)entities.size();i++){
        auto* ea=entities[i];
        if(!ea->active)continue;
        auto* ca=ea->getComponent<Collider>();
        auto* ta=ea->getComponent<Transform>();
        if(!ca||!ta)continue;
        Rect ra{ta->position.x+ca->bounds.x,ta->position.y+ca->bounds.y,ca->bounds.w,ca->bounds.h};

        for(int j=i+1;j<(int)entities.size();j++){
            auto* eb=entities[j];
            if(!eb->active)continue;
            auto* cb=eb->getComponent<Collider>();
            auto* tb=eb->getComponent<Transform>();
            if(!cb||!tb)continue;
            if(ignoresLayer(ca->physicsLayer,cb->physicsLayer))continue;
            Rect rb{tb->position.x+cb->bounds.x,tb->position.y+cb->bounds.y,cb->bounds.w,cb->bounds.h};
            if(!ra.overlaps(rb))continue;

            float pen=0;
            Vec2 normal=computeAABBNormal(ra,rb,pen);
            _collisions.push_back({ea->id,eb->id,normal,pen});

            if(ca->isTrigger||cb->isTrigger)continue;
            auto* rba=ea->getComponent<Rigidbody2D>();
            auto* rbb=eb->getComponent<Rigidbody2D>();
            // Impulse resolution
            if(rba&&rba->bodyType==Rigidbody2D::BodyType::Dynamic&&
               rbb&&rbb->bodyType==Rigidbody2D::BodyType::Dynamic){
                Vec2 rv=rbb->velocity-rba->velocity;
                float velAlongNormal=rv.dot(normal);
                if(velAlongNormal>0)continue;
                float e=std::min(rba->bounciness,rbb->bounciness);
                float j=-(1+e)*velAlongNormal/(1.f/rba->mass+1.f/rbb->mass);
                Vec2 impulse=normal*j;
                rba->velocity-=impulse/rba->mass;
                rbb->velocity+=impulse/rbb->mass;
                // Positional correction
                Vec2 corr=normal*(pen/2.f);
                ta->position-=corr;
                tb->position+=corr;
            } else if(rba&&rba->bodyType==Rigidbody2D::BodyType::Dynamic){
                float velAlongNormal=rba->velocity.dot(normal);
                if(velAlongNormal>0){
                    rba->velocity-=normal*velAlongNormal*(1+rba->bounciness);
                    ta->position+=normal*pen;
                    if(normal.y<-0.5f)rba->isGrounded=true;
                }
            } else if(rbb&&rbb->bodyType==Rigidbody2D::BodyType::Dynamic){
                float velAlongNormal=rbb->velocity.dot(normal);
                if(velAlongNormal<0){
                    rbb->velocity-=normal*velAlongNormal*(1+rbb->bounciness);
                    tb->position-=normal*pen;
                    if(normal.y>0.5f)rbb->isGrounded=true;
                }
            }
        }
    }
}

bool PhysicsSystem::ignoresLayer(const std::string& a,const std::string& b)const{
    std::string key1=a+":"+b,key2=b+":"+a;
    for(auto& s:layerIgnoreMatrix)if(s==key1||s==key2)return true;
    return false;
}

// ═══════════════════════════════════════════════════════════
//  CAMERA SYSTEM
// ═══════════════════════════════════════════════════════════
void CameraSystem::update(Scene& scene,Renderer& r,float dt){
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        auto* cam=e->getComponent<Camera2D>();
        if(!cam)continue;
        Vec2 target=cam->_position;
        if(cam->followTarget){
            auto* te=scene.findByTag("");
            // find by ID
            for(auto* ent:scene.allEntities())
                if(ent->id==cam->followTarget){te=ent;break;}
            if(te){
                auto* tr=te->getComponent<Transform>();
                if(tr){
                    Vec2 desired=tr->position+cam->offset;
                    Vec2 diff=desired-cam->_position;
                    // Deadzone
                    if(std::abs(diff.x)>cam->deadzone.x||std::abs(diff.y)>cam->deadzone.y)
                        target=cam->_position.lerp(desired,std::min(1.f,cam->smoothSpeed*dt));
                    else target=cam->_position;
                }
            }
        }
        // Clamp to world bounds
        if(cam->clampToWorld){
            float hw=r.winW()/(2.f*cam->zoom),hh=r.winH()/(2.f*cam->zoom);
            target.x=std::max(cam->worldBounds.x+hw,std::min(target.x,cam->worldBounds.x+cam->worldBounds.w-hw));
            target.y=std::max(cam->worldBounds.y+hh,std::min(target.y,cam->worldBounds.y+cam->worldBounds.h-hh));
        }
        // Screen shake
        Vec2 shakeOff{};
        if(cam->_shakeTimer>0){
            cam->_shakeTimer-=dt;
            float mag=cam->shakeMagnitude*(cam->_shakeTimer/cam->shakeDuration);
            shakeOff={((float)std::rand()/RAND_MAX*2-1)*mag,((float)std::rand()/RAND_MAX*2-1)*mag};
        }
        cam->_position=target;
        r.setCamera(target+shakeOff,cam->zoom);
    }
}

// ═══════════════════════════════════════════════════════════
//  RENDER SYSTEM
// ═══════════════════════════════════════════════════════════
void RenderSystem::render(Scene& scene){
    // Collect renderables sorted by layer
    struct Renderable{ Entity* e; int layer; };
    std::vector<Renderable> list;
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        if(auto* sp=e->getComponent<Sprite>();sp&&sp->visible)
            list.push_back({e,sp->layer});
    }
    std::stable_sort(list.begin(),list.end(),[](const Renderable& a,const Renderable& b){return a.layer<b.layer;});

    for(auto& [e,layer]:list){
        auto* tr=e->getComponent<Transform>();
        auto* sp=e->getComponent<Sprite>();
        if(!tr)continue;
        Vec2 pos=tr->position;
        Vec2 sz=sp->size;
        // Pivot offset
        Vec2 pivotOff{sz.x*(0.5f-sp->pivot.x),sz.y*(0.5f-sp->pivot.y)};
        Rect rect{pos.x-sz.x*sp->pivot.x,pos.y-sz.y*sp->pivot.y,sz.x,sz.y};
        if(sp->useFill){
            if(tr->rotation!=0.f)
                _r.drawRectEx(rect,sp->tint.a<255?sp->fillColor.lerp(sp->tint,0):sp->fillColor,tr->rotation,true);
            else
                _r.drawRect(rect,sp->fillColor,true);
        }
        // Debug collider
        if(debugDraw){
            if(auto* col=e->getComponent<Collider>()){
                Rect cr{pos.x+col->bounds.x,pos.y+col->bounds.y,col->bounds.w,col->bounds.h};
                _r.drawRect(cr,col->isTrigger?Color{0,200,255,160}:Color{0,255,100,160},false);
            }
            if(auto* rb=e->getComponent<Rigidbody2D>()){
                _r.drawLine(pos,pos+rb->velocity*0.05f,{255,200,0,200},2);
            }
        }
        // Particles
        if(auto* pe=e->getComponent<ParticleEmitter>())
            pe->render(_r,pos);
    }
    // Tilemaps
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        if(auto* tm=e->getComponent<Tilemap>())tm->render(_r);
    }
}

// ═══════════════════════════════════════════════════════════
//  UI SYSTEM
// ═══════════════════════════════════════════════════════════
Vec2 UISystem::anchorOffset(UIAnchor a,Rect rect)const{
    int W=_r.winW(),H=_r.winH();
    switch(a){
        case UIAnchor::TopLeft:     return{0,0};
        case UIAnchor::Top:         return{W/2.f-rect.w/2,0};
        case UIAnchor::TopRight:    return{(float)(W-rect.w),0};
        case UIAnchor::Left:        return{0,H/2.f-rect.h/2};
        case UIAnchor::Center:      return{W/2.f-rect.w/2,H/2.f-rect.h/2};
        case UIAnchor::Right:       return{(float)(W-rect.w),H/2.f-rect.h/2};
        case UIAnchor::BottomLeft:  return{0,(float)(H-rect.h)};
        case UIAnchor::Bottom:      return{W/2.f-rect.w/2,(float)(H-rect.h)};
        case UIAnchor::BottomRight: return{(float)(W-rect.w),(float)(H-rect.h)};
    }
    return{};
}
void UISystem::update(Scene& scene){
    Vec2 mouse=_inp.mousePosition();
    bool clicked=_inp.isMouseButtonPressed(1);
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        auto* el=e->getComponent<UIElement>();
        auto* btn=e->getComponent<UIButton>();
        if(!btn||!btn->visible)continue;
        Vec2 off=el?anchorOffset(el->anchor,el->rect):Vec2{};
        Rect r=el?Rect{el->rect.x+off.x,el->rect.y+off.y,el->rect.w,el->rect.h}:Rect{};
        btn->_hovered=r.contains(mouse);
        btn->_pressed=btn->_hovered&&_inp.isMouseButtonDown(1);
        if(btn->_hovered&&clicked&&btn->onClick)btn->onClick();
    }
}
void UISystem::render(Scene& scene){
    // Sort by zOrder
    struct UIItem{Entity* e;int z;};
    std::vector<UIItem> items;
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        auto* el=e->getComponent<UIElement>();
        int z=el?el->zOrder:0;
        if(e->getComponent<UIPanel>()||e->getComponent<UIButton>()||
           e->getComponent<UILabel>()||e->getComponent<UIProgressBar>())
            items.push_back({e,z});
    }
    std::stable_sort(items.begin(),items.end(),[](const UIItem& a,const UIItem& b){return a.z<b.z;});

    for(auto& [e,z]:items){
        auto* el=e->getComponent<UIElement>();
        if(el&&!el->visible)continue;
        Vec2 off=el?anchorOffset(el->anchor,el->rect):Vec2{};
        auto applyOff=[&](Rect r)->Rect{return{r.x+off.x,r.y+off.y,r.w,r.h};};
        Rect base=el?applyOff(el->rect):Rect{};

        // Panel
        if(auto* p=e->getComponent<UIPanel>();p&&p->visible){
            SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(_r.raw(),p->bgColor.r,p->bgColor.g,p->bgColor.b,p->bgColor.a);
            SDL_Rect sr{(int)base.x,(int)base.y,(int)base.w,(int)base.h};
            SDL_RenderFillRect(_r.raw(),&sr);
            SDL_SetRenderDrawColor(_r.raw(),p->borderColor.r,p->borderColor.g,p->borderColor.b,p->borderColor.a);
            for(int i=0;i<p->borderWidth;i++){
                SDL_Rect br{(int)base.x+i,(int)base.y+i,(int)base.w-2*i,(int)base.h-2*i};
                SDL_RenderDrawRect(_r.raw(),&br);
            }
        }
        // ProgressBar
        if(auto* pb=e->getComponent<UIProgressBar>();pb&&pb->visible){
            SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
            SDL_Rect bg{(int)base.x,(int)base.y,(int)base.w,(int)base.h};
            SDL_SetRenderDrawColor(_r.raw(),pb->bgColor.r,pb->bgColor.g,pb->bgColor.b,pb->bgColor.a);
            SDL_RenderFillRect(_r.raw(),&bg);
            float filled=std::max(0.f,std::min(1.f,pb->value));
            SDL_Rect fill{(int)base.x,(int)base.y,(int)(base.w*filled),(int)base.h};
            SDL_SetRenderDrawColor(_r.raw(),pb->fillColor.r,pb->fillColor.g,pb->fillColor.b,pb->fillColor.a);
            SDL_RenderFillRect(_r.raw(),&fill);
            SDL_SetRenderDrawColor(_r.raw(),pb->borderColor.r,pb->borderColor.g,pb->borderColor.b,pb->borderColor.a);
            SDL_RenderDrawRect(_r.raw(),&bg);
        }
        // Button
        if(auto* btn=e->getComponent<UIButton>();btn&&btn->visible){
            Color col=btn->_pressed?btn->pressColor:btn->_hovered?btn->hoverColor:btn->normalColor;
            SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(_r.raw(),col.r,col.g,col.b,col.a);
            SDL_Rect sr{(int)base.x,(int)base.y,(int)base.w,(int)base.h};
            SDL_RenderFillRect(_r.raw(),&sr);
            SDL_SetRenderDrawColor(_r.raw(),180,180,200,255);
            SDL_RenderDrawRect(_r.raw(),&sr);
            Vec2 tsz=_r.measureText(btn->label,btn->fontSize);
            Vec2 tp{base.x+(base.w-tsz.x)/2,base.y+(base.h-tsz.y)/2};
            _r.drawText(btn->label,tp,btn->textColor,btn->fontSize,true);
        }
        // Label
        if(auto* lbl=e->getComponent<UILabel>();lbl&&lbl->visible){
            _r.drawText(lbl->text,{base.x,base.y},lbl->color,lbl->fontSize,true);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  SCRIPT SYSTEM
// ═══════════════════════════════════════════════════════════
void ScriptSystem::update(Scene& scene,float dt,const std::vector<CollisionEvent>& cols){
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        auto* sc=e->getComponent<Script>();
        if(!sc||!sc->enabled)continue;
        if(!sc->_started){if(sc->onStart)sc->onStart(*e);sc->_started=true;}
        if(sc->onUpdate)sc->onUpdate(*e,dt);
    }
    // Dispatch collision callbacks
    for(const auto& col:cols){
        for(auto* e:scene.allEntities()){
            if(!e->active)continue;
            auto* sc=e->getComponent<Script>();
            if(!sc)continue;
            auto* col_comp=e->getComponent<Collider>();
            if(!col_comp)continue;
            if(e->id==col.a&&sc->onCollision)sc->onCollision(*e,col.b);
            if(e->id==col.b&&sc->onCollision)sc->onCollision(*e,col.a);
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  PREFAB REGISTRY
// ═══════════════════════════════════════════════════════════
PrefabRegistry& PrefabRegistry::instance(){static PrefabRegistry r;return r;}
void PrefabRegistry::registerPrefab(const std::string& name,PrefabFactory f){_prefabs[name]=f;}
Entity& PrefabRegistry::instantiate(const std::string& name,Scene& scene,Vec2 pos){
    auto it=_prefabs.find(name);
    if(it==_prefabs.end())throw std::runtime_error("Prefab not found: "+name);
    return it->second(scene,pos);
}
bool PrefabRegistry::has(const std::string& name)const{return _prefabs.count(name)>0;}
Entity& Instantiate(const std::string& n,Scene& s,Vec2 pos){return PrefabRegistry::instance().instantiate(n,s,pos);}

// ═══════════════════════════════════════════════════════════
//  AUDIO MANAGER
// ═══════════════════════════════════════════════════════════
AudioManager::AudioManager(){_ok=init();}
AudioManager::~AudioManager(){
    for(auto* c:_managed)Mix_FreeChunk(c);
    if(_ok)Mix_CloseAudio();
}
bool AudioManager::init(){return Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,512)==0;}
void AudioManager::setMasterVolume(float v){Mix_Volume(-1,(int)(v*MIX_MAX_VOLUME));}
void AudioManager::playTone(int freq,int ms,float vol,int waveform){
    if(!_ok)return;
    int samples=44100*ms/1000;
    auto* buf=new Sint16[samples*2];
    for(int i=0;i<samples;i++){
        float t=(float)i/44100.f;
        float s=0;
        switch(waveform){
            case 0: s=std::sin(2*(float)M_PI*freq*t); break;           // sine
            case 1: s=std::sin(2*(float)M_PI*freq*t)>0?1.f:-1.f; break;// square
            case 2: s=2.f*std::abs(2.f*(freq*t-std::floor(freq*t+.5f)))-1.f; break; // triangle
        }
        // Fade out
        float env=1.f-(float)i/samples;
        buf[i*2]=buf[i*2+1]=(Sint16)(32767*vol*s*env);
    }
    Mix_Chunk* chunk=Mix_QuickLoad_RAW((Uint8*)buf,samples*2*sizeof(Sint16));
    if(chunk){_managed.push_back(chunk);Mix_PlayChannel(-1,chunk,0);}
    delete[] buf;
}
void AudioManager::playBeep(int freq,int ms,float vol){playTone(freq,ms,vol,0);}
void AudioManager::update3DAudio(Vec2 listenerPos,Scene& scene){
    for(auto* e:scene.allEntities()){
        if(!e->active)continue;
        auto* as=e->getComponent<AudioSource>();
        auto* tr=e->getComponent<Transform>();
        if(!as||!tr||!as->is3D)continue;
        float dist=Vec2::distance(tr->position,listenerPos);
        float vol=1.f-std::max(0.f,std::min(1.f,(dist-as->minDistance)/(as->maxDistance-as->minDistance)));
        (void)vol;
    }
}

// ═══════════════════════════════════════════════════════════
//  CLOCK
// ═══════════════════════════════════════════════════════════
void Clock::tick(){
    Uint64 now=SDL_GetPerformanceCounter();
    if(_prev==0)_prev=now;
    _dt=std::min((float)(now-_prev)/(float)SDL_GetPerformanceFrequency(),0.05f);
    _time+=_dt;_ticks++;_prev=now;
}

// ═══════════════════════════════════════════════════════════
//  ENGINE
// ═══════════════════════════════════════════════════════════
Engine::Engine(EngineConfig cfg):_cfg(cfg){
    std::srand((unsigned)std::time(nullptr));
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER);
    TTF_Init();
    _window=SDL_CreateWindow(cfg.title.c_str(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                             cfg.width,cfg.height,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    _sdlR=SDL_CreateRenderer(_window,-1,cfg.vsync?SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC
                                                  :SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(_sdlR,SDL_BLENDMODE_BLEND);
    _renderer=std::make_unique<Renderer>(_sdlR,cfg.width,cfg.height);
    _renderSys=new RenderSystem(*_renderer);
    _uiSys=new UISystem(*_renderer,_input);
    // Create a default scene
    _sceneMgr.registerScene("__default__",[](Scene&){});
}
Engine::~Engine(){
    delete _renderSys; delete _uiSys;
    SDL_DestroyRenderer(_sdlR);
    SDL_DestroyWindow(_window);
    TTF_Quit(); SDL_Quit();
}
void Engine::run(){
    _running=true;
    if(onStart)onStart();
    while(_running){
        _input.update();
        if(_input.quitRequested())_running=false;
        _clock.tick();
        float dt=_clock.dt();
        _sceneMgr.flushPending();
        auto& sc=_sceneMgr.current();
        _physics.update(sc,dt);
        _cameraSys.update(sc,*_renderer,dt);
        _scriptSys.update(sc,dt,_physics.collisions());
        for(auto* e:sc.allEntities()){
            if(!e->active)continue;
            if(auto* pe=e->getComponent<ParticleEmitter>())pe->update(dt);
            if(auto* an=e->getComponent<Animator>())an->update(dt);
        }
        _uiSys->update(sc);
        if(onUpdate)onUpdate(dt);
        _renderer->clear();
        _renderSys->render(sc);
        _uiSys->render(sc);
        if(onRender)onRender();
        if(_cfg.showFPS){
            std::ostringstream ss;
            ss<<std::fixed<<std::setprecision(0)<<_clock.fps()<<" fps";
            _renderer->drawText(ss.str(),{6,6},{200,200,200,180},13,true);
        }
        _renderer->present();
        sc.flushDestroyed();
    }
}

// ═══════════════════════════════════════════════════════════
//  UIRENDERER IMPLEMENTATION
// ═══════════════════════════════════════════════════════════
void UIRenderer::hline(int x1,int x2,int y,Color c){
    SDL_SetRenderDrawColor(_r.raw(),c.r,c.g,c.b,c.a);
    SDL_RenderDrawLine(_r.raw(),x1,y,x2,y);
}

void UIRenderer::drawRoundedRectFilled(Rect rect,float radius,Color c){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(_r.raw(),c.r,c.g,c.b,c.a);
    int x=(int)rect.x,y=(int)rect.y,w=(int)rect.w,h=(int)rect.h;
    int r=(int)std::min(radius,std::min(rect.w/2,rect.h/2));
    // Fill centre column
    SDL_Rect mid{x,y+r,w,h-2*r}; SDL_RenderFillRect(_r.raw(),&mid);
    // Fill left/right strips for corners
    SDL_Rect top{x+r,y,w-2*r,r}; SDL_RenderFillRect(_r.raw(),&top);
    SDL_Rect bot{x+r,y+h-r,w-2*r,r}; SDL_RenderFillRect(_r.raw(),&bot);
    // Four corners via circle quadrants
    for(int dy=0;dy<r;dy++){
        int dx=(int)std::sqrt((float)(r*r-dy*dy));
        // top-left
        SDL_RenderDrawLine(_r.raw(), x+r-dx, y+r-dy, x+r, y+r-dy);
        // top-right
        SDL_RenderDrawLine(_r.raw(), x+w-r, y+r-dy, x+w-r+dx, y+r-dy);
        // bot-left
        SDL_RenderDrawLine(_r.raw(), x+r-dx, y+h-r+dy, x+r, y+h-r+dy);
        // bot-right
        SDL_RenderDrawLine(_r.raw(), x+w-r, y+h-r+dy, x+w-r+dx, y+h-r+dy);
    }
}

void UIRenderer::drawRoundedRectOutline(Rect rect,float radius,Color c,int sw){
    for(int i=0;i<sw;i++){
        Rect r2{rect.x+i,rect.y+i,rect.w-2*i,rect.h-2*i};
        float rad=std::max(1.f,radius-i);
        int x=(int)r2.x,y=(int)r2.y,w=(int)r2.w,h=(int)r2.h;
        int ri=(int)rad;
        SDL_SetRenderDrawColor(_r.raw(),c.r,c.g,c.b,(Uint8)(c.a*(1-i*0.3f)));
        SDL_RenderDrawLine(_r.raw(),x+ri,y,x+w-ri,y);
        SDL_RenderDrawLine(_r.raw(),x+ri,y+h,x+w-ri,y+h);
        SDL_RenderDrawLine(_r.raw(),x,y+ri,x,y+h-ri);
        SDL_RenderDrawLine(_r.raw(),x+w,y+ri,x+w,y+h-ri);
        for(int dy=0;dy<=ri;dy++){
            int dx=(int)std::sqrt(std::max(0.f,(float)(ri*ri-dy*dy)));
            SDL_RenderDrawPoint(_r.raw(),x+ri-dx,y+ri-dy);
            SDL_RenderDrawPoint(_r.raw(),x+w-ri+dx,y+ri-dy);
            SDL_RenderDrawPoint(_r.raw(),x+ri-dx,y+h-ri+dy);
            SDL_RenderDrawPoint(_r.raw(),x+w-ri+dx,y+h-ri+dy);
        }
    }
}

void UIRenderer::drawRoundedRect(Rect rect,float radius,Color fill,Color stroke,int strokeW){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    if(fill.a>0) drawRoundedRectFilled(rect,radius,fill);
    if(strokeW>0&&stroke.a>0) drawRoundedRectOutline(rect,radius,stroke,strokeW);
}

void UIRenderer::drawGradientRect(Rect rect,Color top,Color bottom){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    int h=(int)rect.h;
    for(int i=0;i<h;i++){
        float t=(float)i/h;
        Color c=top.lerp(bottom,t);
        SDL_SetRenderDrawColor(_r.raw(),c.r,c.g,c.b,c.a);
        SDL_RenderDrawLine(_r.raw(),(int)rect.x,  (int)rect.y+i,
                                    (int)(rect.x+rect.w),(int)rect.y+i);
    }
}

void UIRenderer::drawGradientRectH(Rect rect,Color left,Color right){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    int w=(int)rect.w;
    for(int i=0;i<w;i++){
        float t=(float)i/w;
        Color c=left.lerp(right,t);
        SDL_SetRenderDrawColor(_r.raw(),c.r,c.g,c.b,c.a);
        SDL_RenderDrawLine(_r.raw(),(int)rect.x+i,(int)rect.y,
                                    (int)rect.x+i,(int)(rect.y+rect.h));
    }
}

void UIRenderer::drawGlow(Rect rect,float radius,Color color,int layers){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    for(int i=layers;i>=1;i--){
        float expand=radius*(float)i/layers;
        Uint8 alpha=(Uint8)(color.a*(1.f-(float)i/layers)*0.5f);
        if(alpha<4)continue;
        Color c{color.r,color.g,color.b,alpha};
        Rect expanded{rect.x-expand,rect.y-expand,rect.w+2*expand,rect.h+2*expand};
        drawRoundedRectFilled(expanded,expand+4,c);
    }
}

void UIRenderer::drawShadow(Rect rect,float radius,int ox,int oy,int blur){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    for(int i=blur;i>=1;i--){
        float ex=(float)i*0.5f;
        Uint8 a=(Uint8)(60*(1.f-(float)i/blur));
        Rect sr{rect.x+ox-ex,rect.y+oy-ex,rect.w+2*ex,rect.h+2*ex};
        drawRoundedRectFilled(sr,radius+ex,{0,0,0,a});
    }
}

void UIRenderer::drawBadge(Vec2 center,float r,Color fill,Color border){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    _r.drawCircle(center,r,fill,true);
    _r.drawCircle(center,r,border,false);
    _r.drawCircle(center,r-1,border,false);
}

void UIRenderer::drawProgressBar(Rect rect,float value,Color bg,Color fill,Color fillEnd,float cornerR){
    SDL_SetRenderDrawBlendMode(_r.raw(),SDL_BLENDMODE_BLEND);
    drawRoundedRectFilled(rect,cornerR,bg);
    float v=std::max(0.f,std::min(1.f,value));
    if(v>0){
        Rect fr{rect.x,rect.y,rect.w*v,rect.h};
        if(fr.w>cornerR*2){
            if(fillEnd.a>0){
                // gradient fill
                int w=(int)fr.w;
                for(int i=0;i<w;i++){
                    float t=(float)i/w;
                    Color c=fill.lerp(fillEnd,t);
                    SDL_SetRenderDrawColor(_r.raw(),c.r,c.g,c.b,c.a);
                    SDL_RenderDrawLine(_r.raw(),(int)fr.x+i,(int)fr.y,(int)fr.x+i,(int)(fr.y+fr.h));
                }
                // re-clip to rounded
                drawRoundedRectFilled({rect.x,rect.y,rect.w,rect.h},cornerR,{bg.r,bg.g,bg.b,0});
            } else {
                drawRoundedRectFilled(fr,cornerR,fill);
            }
        }
    }
    // Shine strip
    Rect shine{rect.x+2,rect.y+1,rect.w-4,(rect.h)*0.35f};
    drawRoundedRectFilled(shine,cornerR-1,{255,255,255,28});
    // Border
    drawRoundedRectOutline(rect,cornerR,{255,255,255,50},1);
}

void UIRenderer::drawTooltip(Vec2 pos,const std::string& text,int fontSize){
    Vec2 tsz=_r.measureText(text,fontSize);
    float pad=8;
    Rect r{pos.x,pos.y,tsz.x+pad*2,tsz.y+pad*2};
    drawShadow(r,6,2,3,4);
    drawRoundedRect(r,6,{20,20,40,230},{100,140,200,180},1);
    _r.drawText(text,{r.x+pad,r.y+pad},{220,230,255,255},(int)fontSize,true);
}
