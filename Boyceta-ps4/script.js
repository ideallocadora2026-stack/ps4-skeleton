// =============================================
//  NEON GEOMETRY — PS4 EDITION
//  script.js  (full rewrite)
// =============================================

const canvas = document.getElementById('gameCanvas');
const ctx    = canvas.getContext('2d');

const QUALITY_PROFILES = {
    HIGH:   { label:'ALTO',  renderScale:1, glow:1.00, stars:58, gridAlpha:.070, particleStep:1, maxParticles:520, scanlines:true },
    MEDIUM: { label:'MÉDIO', renderScale:1, glow:.62, stars:28, gridAlpha:.045, particleStep:2, maxParticles:260, scanlines:false },
    LOW:    { label:'BAIXO', renderScale:1, glow:.20, stars:0,  gridAlpha:.025, particleStep:3, maxParticles:130, scanlines:false }
};
let graphicsQuality = 'HIGH';

function quality(){ return QUALITY_PROFILES[graphicsQuality] || QUALITY_PROFILES.HIGH; }
function effectBlur(value){ return Math.round(value * quality().glow); }
function resize(){
    const scale=quality().renderScale;
    canvas.width=Math.max(1,Math.floor(window.innerWidth*scale));
    canvas.height=Math.max(1,Math.floor(window.innerHeight*scale));
    canvas.style.width=window.innerWidth+'px';
    canvas.style.height=window.innerHeight+'px';
    ctx.imageSmoothingEnabled=graphicsQuality!=='LOW';
}
window.addEventListener('resize', ()=>{ resize(); if(currentState===STATE.PLAYING) rebuildHUD(); });
resize();

// ── UI REFS ──────────────────────────────────
const UI = {
    mainMenu:       document.getElementById('main-menu'),
    lobbyScreen:    document.getElementById('lobby-screen'),
    lobbyPlayers:   document.getElementById('lobby-players'),
    lobbyStatus:    document.getElementById('lobby-status'),
    pauseScreen:    document.getElementById('pause-screen'),
    pauseWho:       document.getElementById('pause-who'),
    controlsScreen: document.getElementById('controls-screen'),
    cfgPnum:        document.getElementById('cfg-pnum'),
    skinShop:       document.getElementById('skin-shop-screen'),
    gameOver:       document.getElementById('game-over'),
    hudLayer:       document.getElementById('hud-layer'),
    globalHud:      document.getElementById('global-hud'),
    bossHud:        document.getElementById('boss-hud'),
    bossPhase:      document.getElementById('boss-phase'),
    bossHpFill:     document.getElementById('boss-hp-fill'),
    announce:       document.getElementById('announcement'),
    announceText:   document.getElementById('announce-text'),
    saveNote:       document.getElementById('save-notification'),
    finalWave:      document.getElementById('final-wave'),
    finalScore:     document.getElementById('final-score'),
    cfgSens:        document.getElementById('cfg-sens'),
    cfgRumble:      document.getElementById('cfg-rumble'),
    bindingMsg:     document.getElementById('binding-msg'),
    shopGold:       document.getElementById('shop-gold'),
    shopSilver:     document.getElementById('shop-silver'),
    skinGrid:       document.getElementById('skin-grid'),
    previewCanvas:  document.getElementById('character-preview'),
    previewSkin:    document.getElementById('preview-skin'),
    previewHat:     document.getElementById('preview-hat'),
    menuBtns: {
        MENU:     ()=> Array.from(document.querySelectorAll('#main-menu-options .menu-btn')),
        PAUSED:   ()=> Array.from(document.querySelectorAll('#pause-menu-options .menu-btn')),
        GAMEOVER: ()=> Array.from(document.querySelectorAll('#go-menu-options .menu-btn')),
        CONTROLS: ()=> Array.from(document.querySelectorAll('#cfg-menu-options .menu-btn, #cfg-menu-options .cfg-btn')),
        SKINS:    ()=> Array.from(document.querySelectorAll('#skin-tabs-menu .menu-btn, #skin-grid .skin-card, #skin-shop-screen [data-action="back-skin"]')),
    }
};

// ── STATE ────────────────────────────────────
const STATE = { MENU:'MENU', LOBBY:'LOBBY', PLAYING:'PLAYING', PAUSED:'PAUSED', GAMEOVER:'GAMEOVER', CONTROLS:'CONTROLS', SKINS:'SKINS' };
let currentState   = STATE.MENU;
let menuIndex      = 0;
let pauseCallerIdx = 0;
let bindingAction  = null;
let lobbyCountdown = -1;
let lobbyPadSignature = '';
let skinTab        = 'skins';
let settingsReturnState = STATE.MENU;

// ── CONSTANTS ────────────────────────────────
const PColors    = ['#2277ff','#ff2244','#00ff88','#bb44ff'];
const BossShapes = ['triangle','square','circle','cross'];
const BTN_NAME   = {0:'CRUZ',1:'CIRCULO',2:'QUADRADO',3:'TRIANGULO',4:'L1',5:'R1',6:'L2',7:'R2',8:'SHARE',9:'OPTIONS',10:'L3',11:'R3',12:'CIMA',13:'BAIXO',14:'ESQUERDA',15:'DIREITA'};
const DEFAULT_KEYS = {pause:9, skill:7, upg1:4, upg2:5, upg3:3};

// O navegador do PS4 pode expor a API antiga com o prefixo webkit.
function getConnectedGamepads(){
    const getPads = navigator.getGamepads || navigator.webkitGetGamepads;
    return getPads ? getPads.call(navigator) : [];
}

// ── COSMETICS ────────────────────────────────
const ALL_SKINS = [
    {id:'default',  name:'NÚCLEO PADRÃO', cost:0},
    {id:'neon',     name:'ANEL NEON',     cost:12},
    {id:'aura',     name:'AURA VIVA',     cost:20},
    {id:'shield',   name:'ESCUDO ÍON',    cost:30},
    {id:'spike',    name:'PUAS ROTATIVAS',cost:38},
    {id:'ghost',    name:'FANTASMA',      cost:48},
    {id:'plasma',   name:'TRAJE PLASMA',  cost:58},
    {id:'matrix',   name:'MALHA MATRIX',  cost:68},
    {id:'ember',    name:'BRASA VIVA',    cost:80},
    {id:'crystal',  name:'CRISTAL AZUL',  cost:95},
    {id:'void',     name:'VÉU DO VAZIO',  cost:110},
    {id:'spectrum', name:'ESPECTRO RGB',  cost:130},
];
const ALL_HATS = [
    {id:'none',       name:'SEM ACESSÓRIO', cost:0},
    {id:'visor',      name:'VISOR CYBER',   cost:10},
    {id:'cap',        name:'BONÉ NEON',     cost:16},
    {id:'antenna',    name:'ANTENA',        cost:22},
    {id:'crown',      name:'COROA',         cost:30},
    {id:'tophat',     name:'CARTOLA',       cost:36},
    {id:'headphones', name:'HEADSET',        cost:44},
    {id:'halo',       name:'HALO',          cost:54},
    {id:'horns',      name:'CHIFRES',       cost:62},
    {id:'helmet',     name:'CAPACETE',      cost:72},
    {id:'flower',     name:'FLOR CÓSMICA',  cost:84},
    {id:'angel',      name:'ANEL CELESTE',  cost:100},
];

// ── GAME STATE ───────────────────────────────
const Game = {
    wave:1, timeSinceLastWave:0, waveDuration:25000, cameraShake:0,
    map:{minX:0,minY:0,maxX:0,maxY:0},
    walls:[], players:[], projectiles:[], enemyProjectiles:[],
    enemies:[], boss:null, bossDefeated:false,
    particles:[], drops:[], floatingTexts:[],
    enemyTheme:{edges:0, color:'#ff2244'},
    isSolo:true, lastTime:0
};

let cosmeticStore = {ownedSkins:['default'],ownedHats:['none'],activeSkin:'default',activeHat:'none',wallet:{silver:0}};
let defaultControls = {keys:{...DEFAULT_KEYS},sensMove:1.0,rumbleStr:1.0};
let rumbleStrength = 1.0;

// ── UTILS ────────────────────────────────────
function rumble(pi, dur, w, s){
    if(rumbleStrength <= 0) return;
    const p = getConnectedGamepads()[pi];
    if(p && p.vibrationActuator)
        p.vibrationActuator.playEffect('dual-rumble',{startDelay:0,duration:dur,
            weakMagnitude:Math.min(1,w*rumbleStrength),
            strongMagnitude:Math.min(1,s*rumbleStrength)}).catch(()=>{});
}

function floatText(x, y, txt, color){ Game.floatingTexts.push(new FloatingText(x,y,txt,color)); }

function btnN(i){ return BTN_NAME[i] || ('BTN'+i); }

function getWallet(){
    if(!cosmeticStore.wallet || typeof cosmeticStore.wallet!=='object') cosmeticStore.wallet={silver:0};
    cosmeticStore.wallet.silver=Math.max(0,Number(cosmeticStore.wallet.silver)||0);
    return cosmeticStore.wallet;
}

function normalizeCosmeticStore(data){
    const skinIds=new Set(ALL_SKINS.map(item=>item.id));
    const hatIds=new Set(ALL_HATS.map(item=>item.id));
    const clean=(values,allowed,fallback)=>Array.from(new Set((Array.isArray(values)?values:[]).filter(id=>allowed.has(id)).concat(fallback)));
    const store=data && typeof data==='object' ? data : {};
    const ownedSkins=clean(store.ownedSkins,skinIds,['default']);
    const ownedHats=clean(store.ownedHats,hatIds,['none']);
    return {
        ownedSkins,
        ownedHats,
        activeSkin:ownedSkins.includes(store.activeSkin)?store.activeSkin:'default',
        activeHat:ownedHats.includes(store.activeHat)?store.activeHat:'none',
        wallet:{silver:Math.max(0,Math.floor(Number(store.wallet && store.wallet.silver)||0))}
    };
}

function setGraphicsQuality(next){
    graphicsQuality=QUALITY_PROFILES[next] ? next : 'HIGH';
    document.body.dataset.graphics=graphicsQuality.toLowerCase();
    resize();
    if(currentState===STATE.PLAYING) rebuildHUD();
}

function getConfigTarget(){ return Game.players.find(p=>p.padIndex===pauseCallerIdx) || defaultControls; }

function copyToDefaultControls(source){
    defaultControls={
        keys:{...DEFAULT_KEYS,...(source.keys||{})},
        sensMove:Math.min(3,Math.max(.5,Number(source.sensMove)||1)),
        rumbleStr:Math.min(2,Math.max(0,Number(source.rumbleStr)||1))
    };
    rumbleStrength=defaultControls.rumbleStr;
}

function changeConfigSetting(setting,direction=1){
    const target=getConfigTarget();
    if(setting==='graphics'){
        const levels=['HIGH','MEDIUM','LOW'];
        const current=levels.indexOf(graphicsQuality);
        setGraphicsQuality(levels[(current+direction+levels.length)%levels.length]);
    } else if(setting==='sensitivity'){
        target.sensMove=Math.min(3,Math.max(.5,+((Number(target.sensMove)||1)+direction*.1).toFixed(1)));
    } else if(setting==='rumble'){
        target.rumbleStr=Math.min(2,Math.max(0,+((Number(target.rumbleStr)||1)+direction*.1).toFixed(1)));
    }
    copyToDefaultControls(target);
    refreshCfg();
    saveGame();
}

function addSilver(amount){
    getWallet().silver+=Math.max(0,Math.floor(amount));
    saveGame(false);
}

class Vect {
    static dist(x1,y1,x2,y2){ return Math.hypot(x2-x1,y2-y1); }
    static angle(x1,y1,x2,y2){ return Math.atan2(y2-y1,x2-x1); }
}

// ── ENTITY CLASSES ───────────────────────────

class FloatingText {
    constructor(x,y,t,c){ this.x=x; this.y=y; this.text=t; this.color=c; this.life=1.5; this.vy=-40; }
    update(dt){ this.y+=this.vy*dt/1000; this.vy*=0.93; this.life-=dt/1000; }
    draw(c){
        c.save(); c.globalAlpha=Math.max(0,Math.min(1,this.life));
        c.fillStyle=this.color; c.shadowColor=this.color; c.shadowBlur=6;
        c.font='bold 15px Rajdhani'; c.fillText(this.text,this.x,this.y);
        c.restore();
    }
}

class Particle {
    constructor(x,y,color,vx,vy,size,f=0.97){
        this.x=x; this.y=y; this.color=color; this.vx=vx; this.vy=vy; this.size=size; this.friction=f; this.alpha=1;
    }
    update(dt){ const f=Math.pow(this.friction,dt/16); this.vx*=f; this.vy*=f; this.x+=this.vx*dt/16; this.y+=this.vy*dt/16; this.alpha-=0.025*dt/16; }
    draw(c){
        c.save(); c.globalAlpha=Math.max(0,this.alpha);
        c.fillStyle=this.color; c.shadowColor=this.color; c.shadowBlur=effectBlur(4);
        c.beginPath(); c.arc(this.x,this.y,this.size,0,Math.PI*2); c.fill();
        c.restore();
    }
}

class Coin {
    constructor(x,y,isSilver=false){
        this.x=x; this.y=y; this.radius=8; this.life=9000;
        this.isSilver=isSilver; this.color=isSilver?'#b8c4d4':'#ffd700';
        this.bob=Math.random()*Math.PI*2;
    }
    update(dt){
        this.life-=dt; this.bob+=dt/400;
        let cl=null, md=Infinity;
        Game.players.forEach(p=>{ if(p.hp<=0)return; const d=Vect.dist(this.x,this.y,p.x,p.y); if(d<md){md=d;cl=p;} });
        if(cl && md<160){ const a=Vect.angle(this.x,this.y,cl.x,cl.y); this.x+=Math.cos(a)*450*dt/1000; this.y+=Math.sin(a)*450*dt/1000; }
        if(cl && md < cl.radius+this.radius){
            if(this.isSilver) addSilver(1); else cl.coins++;
            rumble(cl.padIndex,60,0.2,0); this.life=0; cl.updateHUD();
        }
    }
    draw(c){
        c.save();
        if(this.life<2000 && Math.floor(Date.now()/120)%2===0) c.globalAlpha=0.3;
        const b=Math.sin(this.bob)*3;
        c.fillStyle=this.color; c.shadowColor=this.color; c.shadowBlur=effectBlur(12);
        c.beginPath(); c.arc(this.x,this.y+b,this.radius,0,Math.PI*2); c.fill();
        // Highlight
        c.fillStyle='rgba(255,255,255,0.45)';
        c.beginPath(); c.arc(this.x-2,this.y+b-2,this.radius*0.35,0,Math.PI*2); c.fill();
        c.restore();
    }
}

class Wall {
    constructor(x,y,w,h){ this.x=x; this.y=y; this.w=w; this.h=h; this.hp=40; this.maxHp=40; }
    draw(c){
        const r=Math.max(0.1,this.hp/this.maxHp);
        c.fillStyle=`rgba(20,60,120,${r*0.7})`; c.strokeStyle=`rgba(0,180,255,${r*0.7})`; c.lineWidth=2;
        c.fillRect(this.x,this.y,this.w,this.h); c.strokeRect(this.x,this.y,this.w,this.h);
        c.shadowColor='#00d4ff'; c.shadowBlur=effectBlur(r*8); c.strokeRect(this.x,this.y,this.w,this.h); c.shadowBlur=0;
    }
}

function resolveWall(e){
    for(const w of Game.walls){
        let tx=e.x, ty=e.y;
        if(e.x<w.x)tx=w.x; else if(e.x>w.x+w.w)tx=w.x+w.w;
        if(e.y<w.y)ty=w.y; else if(e.y>w.y+w.h)ty=w.y+w.h;
        const dx=e.x-tx, dy=e.y-ty, d=Math.hypot(dx,dy)||1;
        if(d<e.radius){ e.x=tx+(dx/d)*e.radius; e.y=ty+(dy/d)*e.radius; }
    }
}

// ── PLAYER ───────────────────────────────────
class Player {
    constructor(pi){
        this.padIndex=pi; this.color=PColors[pi%4];
        this.x=canvas.width/2+(pi*50-75); this.y=canvas.height/2;
        this.camera={x:this.x, y:this.y};
        this.radius=18; this.hp=100; this.maxHp=100;
        this.coins=0; this.score=0;
        this.lastShot=0; this.deadzone=0.18; this.sensMove=defaultControls.sensMove; this.rumbleStr=defaultControls.rumbleStr;
        this.hudId='hud-p'+pi;
        this.activeSkin=cosmeticStore.activeSkin; this.activeHat=cosmeticStore.activeHat;
        this.skinAnim=0;
        this.keys={...defaultControls.keys};
        this.stats={fireRate:160, damageMulti:1, skillDuration:3000, skillCooldownMax:12000, skillCooldown:0, costFire:15, costMulti:40, costSkill:25};
        this.timeStopActive=false; this.timeStopTimer=0; this.invincible=0;
    }

    update(dt){
        if(this.hp<=0) return;
        if(this.invincible>0) this.invincible-=dt;
        if(this.timeStopActive){ this.timeStopTimer-=dt; if(this.timeStopTimer<=0){this.timeStopActive=false; floatText(this.x,this.y-30,'TEMPO NORMALIZADO','#aaa');} }
        else if(this.stats.skillCooldown>0) this.stats.skillCooldown-=dt;
        this.skinAnim=(this.skinAnim+dt/1000)%(Math.PI*2);

        const pad=getConnectedGamepads()[this.padIndex];
        if(pad){ const ax=pad.axes[0]||0, ay=pad.axes[1]||0, m=Math.hypot(ax,ay); if(m>this.deadzone){const s=300*this.sensMove; this.x+=ax*s*dt/1000; this.y+=ay*s*dt/1000;} }
        resolveWall(this);
        this.x=Math.max(Game.map.minX+this.radius, Math.min(Game.map.maxX-this.radius, this.x));
        this.y=Math.max(Game.map.minY+this.radius, Math.min(Game.map.maxY-this.radius, this.y));
        if(Date.now()-this.lastShot>this.stats.fireRate && (Game.enemies.length>0||Game.boss)) this.shoot();
        this.camera.x+=(this.x-this.camera.x)*5*(dt/1000);
        this.camera.y+=(this.y-this.camera.y)*5*(dt/1000);
        this.updateHUD();
    }

    shoot(){
        this.lastShot=Date.now(); let t=null, md=Infinity;
        Game.enemies.forEach(e=>{ const d=Vect.dist(this.x,this.y,e.x,e.y); if(d<md){md=d;t={x:e.x,y:e.y};} });
        if(Game.boss){
            if(Game.boss.phase===1){ Game.boss.weapons.forEach(w=>{ if(!w.active)return; const wx=Game.boss.x+w.relX,wy=Game.boss.y+w.relY,d=Vect.dist(this.x,this.y,wx,wy); if(d<md){md=d;t={x:wx,y:wy};} }); }
            else { const d=Vect.dist(this.x,this.y,Game.boss.x,Game.boss.y); if(d<md)t={x:Game.boss.x,y:Game.boss.y}; }
        }
        if(t){ const a=Vect.angle(this.x,this.y,t.x,t.y); Game.projectiles.push({x:this.x,y:this.y,owner:this,vx:Math.cos(a)*820,vy:Math.sin(a)*820,radius:5,color:this.color,damage:20*this.stats.damageMulti}); }
    }

    draw(c){
        if(this.hp<=0) return;
        if(this.invincible>0 && Math.floor(Date.now()/60)%2===0) return;
        c.save(); c.translate(this.x,this.y);
        const sk=this.activeSkin, ht=this.activeHat;

        // Aura
        if(sk==='aura'){ const g=c.createRadialGradient(0,0,this.radius,0,0,this.radius*2.5); g.addColorStop(0,this.color+'55'); g.addColorStop(1,'transparent'); c.fillStyle=g; c.beginPath(); c.arc(0,0,this.radius*2.5,0,Math.PI*2); c.fill(); }
        if(sk==='ghost') c.globalAlpha=0.5;

        // Body
        if(sk==='plasma'){
            const pg=c.createRadialGradient(-6,-8,2,0,0,this.radius);
            pg.addColorStop(0,'#ffffff'); pg.addColorStop(.32,'#55e8ff'); pg.addColorStop(1,this.color);
            c.fillStyle=pg;
        } else if(sk==='void') c.fillStyle='#080511';
        else if(sk==='spectrum'){
            const sg=c.createLinearGradient(-this.radius,-this.radius,this.radius,this.radius);
            sg.addColorStop(0,'#00eaff'); sg.addColorStop(.34,'#bc4dff'); sg.addColorStop(.68,'#ff3a9d'); sg.addColorStop(1,'#ffe66d'); c.fillStyle=sg;
        } else c.fillStyle=this.color;
        c.shadowColor=this.color; c.shadowBlur=effectBlur(20);
        c.beginPath(); c.arc(0,0,this.radius,0,Math.PI*2); c.fill(); c.shadowBlur=0;
        // Inner ring
        c.strokeStyle='rgba(255,255,255,0.5)'; c.lineWidth=2;
        c.beginPath(); c.arc(0,0,this.radius-6,0,Math.PI*2); c.stroke();
        // Direction dot
        c.fillStyle='rgba(255,255,255,0.9)'; c.beginPath(); c.arc(0,-this.radius+4,3,0,Math.PI*2); c.fill();

        // Skins
        if(sk==='neon'){ c.strokeStyle=this.color; c.lineWidth=3; c.shadowColor=this.color; c.shadowBlur=effectBlur(12); c.beginPath(); c.arc(0,0,this.radius+5,0,Math.PI*2); c.stroke(); c.shadowBlur=0; }
        if(sk==='shield'){ const a=this.skinAnim; c.strokeStyle=this.color+'cc'; c.lineWidth=4; c.shadowColor=this.color; c.shadowBlur=effectBlur(10); c.beginPath(); c.arc(0,0,this.radius+8,a,a+Math.PI); c.stroke(); c.beginPath(); c.arc(0,0,this.radius+8,a+Math.PI,a+Math.PI*2); c.stroke(); c.shadowBlur=0; }
        if(sk==='spike'){ c.fillStyle=this.color; c.shadowColor=this.color; c.shadowBlur=effectBlur(8); for(let i=0;i<6;i++){ const a=this.skinAnim+i*Math.PI*2/6; c.beginPath(); c.moveTo(Math.cos(a)*(this.radius+1),Math.sin(a)*(this.radius+1)); c.lineTo(Math.cos(a+0.2)*(this.radius-3),Math.sin(a+0.2)*(this.radius-3)); c.lineTo(Math.cos(a-0.2)*(this.radius-3),Math.sin(a-0.2)*(this.radius-3)); c.fill(); } c.shadowBlur=0; }
        if(sk==='plasma'){ c.fillStyle='#9df8ff'; c.shadowColor='#32dfff'; c.shadowBlur=effectBlur(12); for(let i=0;i<4;i++){const a=this.skinAnim*1.8+i*Math.PI/2;c.beginPath();c.arc(Math.cos(a)*(this.radius+6),Math.sin(a)*(this.radius+6),3,0,Math.PI*2);c.fill();}c.shadowBlur=0;}
        if(sk==='matrix'){ c.save(); c.beginPath(); c.arc(0,0,this.radius-1,0,Math.PI*2); c.clip(); c.strokeStyle='rgba(120,255,170,.76)'; c.lineWidth=1.4; for(let i=-12;i<=12;i+=6){c.beginPath();c.moveTo(i,-this.radius);c.lineTo(i,this.radius);c.stroke();c.beginPath();c.moveTo(-this.radius,i);c.lineTo(this.radius,i);c.stroke();}c.restore(); }
        if(sk==='ember'){ c.fillStyle='#ff9f1c'; c.shadowColor='#ff4d00'; c.shadowBlur=effectBlur(14); for(let i=0;i<7;i++){const a=this.skinAnim*.7+i*Math.PI*2/7;c.beginPath();c.moveTo(Math.cos(a)*(this.radius-4),Math.sin(a)*(this.radius-4));c.lineTo(Math.cos(a+.16)*(this.radius+11),Math.sin(a+.16)*(this.radius+11));c.lineTo(Math.cos(a-.16)*(this.radius+11),Math.sin(a-.16)*(this.radius+11));c.fill();}c.shadowBlur=0; }
        if(sk==='crystal'){ c.strokeStyle='#aeeaff'; c.lineWidth=3; c.shadowColor='#49b8ff'; c.shadowBlur=effectBlur(12); c.beginPath(); for(let i=0;i<6;i++){const a=this.skinAnim*.22+i*Math.PI/3;if(!i)c.moveTo(Math.cos(a)*(this.radius+7),Math.sin(a)*(this.radius+7));else c.lineTo(Math.cos(a)*(this.radius+7),Math.sin(a)*(this.radius+7));}c.closePath();c.stroke();c.shadowBlur=0; }
        if(sk==='void'){ c.fillStyle='#200438'; c.beginPath(); c.arc(0,0,this.radius-7,0,Math.PI*2); c.fill(); c.strokeStyle='#d64cff';c.lineWidth=2;c.shadowColor='#9a1fff';c.shadowBlur=effectBlur(14);c.beginPath();c.arc(0,0,this.radius+5,0,Math.PI*2);c.stroke();c.shadowBlur=0; }
        if(sk==='spectrum'){ c.strokeStyle='#fff';c.lineWidth=2;c.shadowColor='#fff';c.shadowBlur=effectBlur(12);c.beginPath();c.arc(0,0,this.radius+5,0,Math.PI*2);c.stroke();c.shadowBlur=0; }

        // Hats
        if(ht==='crown'){ c.fillStyle='#ffd700'; c.shadowColor='#ffd700'; c.shadowBlur=8; c.beginPath(); c.moveTo(-12,-this.radius-2); c.lineTo(-12,-this.radius-14); c.lineTo(-6,-this.radius-8); c.lineTo(0,-this.radius-14); c.lineTo(6,-this.radius-8); c.lineTo(12,-this.radius-14); c.lineTo(12,-this.radius-2); c.fill(); c.shadowBlur=0; }
        if(ht==='tophat'){ c.fillStyle='#111'; c.strokeStyle='#555'; c.lineWidth=1; c.fillRect(-12,-this.radius-20,24,16); c.strokeRect(-12,-this.radius-20,24,16); c.fillRect(-15,-this.radius-4,30,4); c.strokeRect(-15,-this.radius-4,30,4); }
        if(ht==='halo'){ c.strokeStyle='#ffd700'; c.lineWidth=3; c.shadowColor='#ffd700'; c.shadowBlur=14; c.beginPath(); c.ellipse(0,-this.radius-8,10,4,0,0,Math.PI*2); c.stroke(); c.shadowBlur=0; }
        if(ht==='horns'){ c.fillStyle='#cc2222'; c.shadowColor='#f00'; c.shadowBlur=8; c.beginPath(); c.moveTo(-8,-this.radius); c.lineTo(-11,-this.radius-14); c.lineTo(-4,-this.radius-2); c.fill(); c.beginPath(); c.moveTo(8,-this.radius); c.lineTo(11,-this.radius-14); c.lineTo(4,-this.radius-2); c.fill(); c.shadowBlur=0; }
        if(ht==='angel'){ c.strokeStyle='#fff'; c.lineWidth=2.5; c.shadowColor='#fff'; c.shadowBlur=16; c.beginPath(); c.ellipse(0,-this.radius-8,11,4.5,0,0,Math.PI*2); c.stroke(); c.shadowBlur=0; }
        if(ht==='visor'){ c.fillStyle='#071b32';c.strokeStyle='#00d4ff';c.lineWidth=2;c.shadowColor='#00d4ff';c.shadowBlur=effectBlur(8);c.fillRect(-this.radius+3,-5,(this.radius-3)*2,8);c.strokeRect(-this.radius+3,-5,(this.radius-3)*2,8);c.shadowBlur=0; }
        if(ht==='cap'){ c.fillStyle='#163b9b';c.beginPath();c.arc(-2,-this.radius+2,13,Math.PI,Math.PI*2);c.fill();c.fillRect(-14,-this.radius+1,23,4); }
        if(ht==='antenna'){ c.strokeStyle='#d8f8ff';c.lineWidth=2;c.beginPath();c.moveTo(0,-this.radius);c.lineTo(0,-this.radius-17);c.stroke();c.fillStyle='#ff44c7';c.shadowColor='#ff44c7';c.shadowBlur=effectBlur(8);c.beginPath();c.arc(0,-this.radius-19,4,0,Math.PI*2);c.fill();c.shadowBlur=0; }
        if(ht==='headphones'){ c.strokeStyle='#e448ff';c.lineWidth=4;c.shadowColor='#b72dff';c.shadowBlur=effectBlur(8);c.beginPath();c.arc(0,0,this.radius+4,Math.PI,0);c.stroke();c.fillStyle='#231043';c.fillRect(-this.radius-5,-4,5,11);c.fillRect(this.radius,-4,5,11);c.shadowBlur=0; }
        if(ht==='helmet'){ c.strokeStyle='#b7ebff';c.lineWidth=3;c.shadowColor='#1fb9ff';c.shadowBlur=effectBlur(10);c.beginPath();c.arc(0,0,this.radius+5,Math.PI*.82,Math.PI*2.18);c.stroke();c.shadowBlur=0; }
        if(ht==='flower'){ c.fillStyle='#ff6fd8';c.shadowColor='#ff6fd8';c.shadowBlur=effectBlur(8);for(let i=0;i<5;i++){const a=i*Math.PI*2/5;c.beginPath();c.arc(Math.cos(a)*7,-this.radius-9+Math.sin(a)*7,5,0,Math.PI*2);c.fill();}c.fillStyle='#ffd700';c.beginPath();c.arc(0,-this.radius-9,4,0,Math.PI*2);c.fill();c.shadowBlur=0; }
        c.restore();
    }

    takeDamage(a){
        if(this.hp<=0||this.invincible>0) return;
        this.hp-=a; this.invincible=400;
        rumble(this.padIndex,250,0.7,1.0); Game.cameraShake=14;
        for(let i=0;i<12;i++) Game.particles.push(new Particle(this.x,this.y,'#ff2244',(Math.random()-.5)*14,(Math.random()-.5)*14,3,0.95));
        this.updateHUD();
        if(this.hp<=0){ floatText(this.x,this.y,'SISTEMA DESLIGADO',this.color); checkGameOver(); }
    }

    activateTimeStop(){
        this.timeStopActive=true; this.timeStopTimer=this.stats.skillDuration; this.stats.skillCooldown=this.stats.skillCooldownMax;
        floatText(this.x,this.y-30,'TEMPO PARADO!',this.color);
        for(let i=0;i<40;i++) Game.particles.push(new Particle(this.x,this.y,this.color,(Math.random()-.5)*25,(Math.random()-.5)*25,Math.random()*4+1,0.90));
        rumble(this.padIndex,500,0.4,0.6); this.updateHUD();
    }

    updateHUD(){
        const h=document.getElementById(this.hudId); if(!h) return;
        const q=s=>h.querySelector(s);
        if(q('.hud-score')) q('.hud-score').textContent=this.score;
        if(q('.hud-gold'))  q('.hud-gold').textContent=this.coins;
        if(q('.hud-silver'))q('.hud-silver').textContent=getWallet().silver;
        if(q('.hp-fill'))   q('.hp-fill').style.width=Math.max(0,(this.hp/this.maxHp)*100)+'%';

        // ─ COOLDOWN BAR (fixed logic) ─
        const cdF=q('.cd-fill');
        if(cdF){
            let pct;
            if(this.timeStopActive){
                // Active: bar drains from 100% → 0%
                pct = Math.max(0,(this.timeStopTimer/this.stats.skillDuration)*100);
                cdF.style.background='#ff2244';
                cdF.style.boxShadow='0 0 6px #ff2244';
            } else {
                // Recharging: bar fills 0% → 100%
                pct = this.stats.skillCooldownMax>0
                    ? Math.max(0, 100-(this.stats.skillCooldown/this.stats.skillCooldownMax)*100)
                    : 100;
                cdF.style.background=this.color;
                cdF.style.boxShadow='0 0 6px '+this.color;
            }
            cdF.style.width=pct+'%';
        }

        if(q('.upg1-cost')) q('.upg1-cost').textContent=this.stats.costFire+' G';
        if(q('.upg2-cost')) q('.upg2-cost').textContent=this.stats.costMulti+' G';
        if(q('.upg3-cost')) q('.upg3-cost').textContent=this.stats.costSkill+' G';
    }

    buyUpgrade(t){
        if(t===1&&this.coins>=this.stats.costFire){ this.coins-=this.stats.costFire; this.stats.fireRate=Math.max(40,this.stats.fireRate-15); this.stats.costFire=Math.floor(this.stats.costFire*1.55); floatText(this.x,this.y,'TIRO RÁPIDO!','#ffd700'); rumble(this.padIndex,120,0.3,0.4); }
        else if(t===2&&this.coins>=this.stats.costMulti){ this.coins-=this.stats.costMulti; this.stats.damageMulti+=0.5; this.stats.costMulti=Math.floor(this.stats.costMulti*2.2); floatText(this.x,this.y,'+50% DANO!','#ffd700'); rumble(this.padIndex,120,0.3,0.5); }
        else if(t===3&&this.coins>=this.stats.costSkill){ this.coins-=this.stats.costSkill; this.stats.skillDuration+=500; this.stats.skillCooldownMax=Math.max(5000,this.stats.skillCooldownMax-1000); this.stats.costSkill=Math.floor(this.stats.costSkill*1.8); floatText(this.x,this.y,'HAB MELHORADA!','#ffd700'); rumble(this.padIndex,120,0.3,0.4); }
        this.updateHUD();
    }
}

// ── ENEMY ────────────────────────────────────
class Enemy {
    constructor(x,y,hp,spd){
        this.x=x; this.y=y; this.hp=hp; this.maxHp=hp; this.speed=spd;
        this.color=Game.enemyTheme.color; this.edges=Game.enemyTheme.edges;
        this.radius=15+Math.random()*4; this.angle=Math.random()*Math.PI*2; this.rotSpd=(Math.random()-.5)*2;
    }
    update(dt){
        if(Game.players.some(p=>p.timeStopActive)) return;
        let t=null, md=Infinity;
        Game.players.forEach(p=>{ if(p.hp<=0)return; const d=Vect.dist(this.x,this.y,p.x,p.y); if(d<md){md=d;t=p;} });
        if(t){ const a=Vect.angle(this.x,this.y,t.x,t.y); this.x+=Math.cos(a)*this.speed*dt/1000; this.y+=Math.sin(a)*this.speed*dt/1000; }
        this.angle+=this.rotSpd*dt/1000; resolveWall(this);
    }
    draw(c){
        c.save(); c.translate(this.x,this.y); c.rotate(this.angle);
        c.shadowColor=this.color; c.shadowBlur=effectBlur(15); c.fillStyle=this.color; c.beginPath();
        if(this.edges===0){ c.rect(-this.radius,-this.radius,this.radius*2,this.radius*2); }
        else{ for(let i=0;i<this.edges;i++){ const a=i*Math.PI*2/this.edges-Math.PI/2; if(i===0)c.moveTo(Math.cos(a)*this.radius,Math.sin(a)*this.radius); else c.lineTo(Math.cos(a)*this.radius,Math.sin(a)*this.radius); } c.closePath(); }
        c.fill(); c.shadowBlur=0;
        // Inner highlight
        c.fillStyle='rgba(255,255,255,0.12)'; c.beginPath(); c.arc(-this.radius*.2,-this.radius*.2,this.radius*.3,0,Math.PI*2); c.fill();
        c.restore();
        // HP bar
        if(this.hp<this.maxHp){ const bw=this.radius*2,bh=4,bx=this.x-this.radius,by=this.y-this.radius-10; c.fillStyle='rgba(0,0,0,.6)'; c.fillRect(bx,by,bw,bh); c.fillStyle='hsl('+(120*(this.hp/this.maxHp))+',100%,50%)'; c.fillRect(bx,by,bw*(this.hp/this.maxHp),bh); }
    }
}

// ── BOSS ─────────────────────────────────────
class Boss {
    constructor(){
        this.x=canvas.width/2; this.y=-120; this.radius=55; this.phase=1;
        const d=1+(Game.wave/10)*.5;
        this.hp=3000*d; this.maxHp=3000*d; this.p3hp=3000*d; this.maxP3hp=3000*d;
        this.shape=BossShapes[Math.floor(Math.random()*BossShapes.length)];
        const clrs={'triangle':'#00ff88','square':'#ff00ff','circle':'#ff2244','cross':'#2277ff'};
        this.trueColor=clrs[this.shape]; this.color='#333'; this.angle=0; this.atk=0;
        this.weapons=[
            {id:1,relX:0,relY:-95,hp:500*d,maxHp:500*d,active:true},
            {id:2,relX:82,relY:47,hp:500*d,maxHp:500*d,active:true},
            {id:3,relX:-82,relY:47,hp:500*d,maxHp:500*d,active:true}
        ];
        UI.bossHud.classList.remove('hidden'); this.updateUI();
    }
    update(dt){
        if(this.y<160) this.y+=110*dt/1000;
        if(Game.players.some(p=>p.timeStopActive)) return;
        this.angle+=1.6*dt/1000; this.atk+=dt;
        let t=null, md=Infinity;
        Game.players.forEach(p=>{ if(p.hp<=0)return; const d=Vect.dist(this.x,this.y,p.x,p.y); if(d<md){md=d;t=p;} });
        if(this.phase===1){
            if(this.atk>1400){ this.atk=0; this.weapons.forEach(w=>{ if(!w.active||!t)return; const wx=this.x+Math.cos(this.angle)*w.relX-Math.sin(this.angle)*w.relY, wy=this.y+Math.sin(this.angle)*w.relX+Math.cos(this.angle)*w.relY, a=Vect.angle(wx,wy,t.x,t.y); Game.enemyProjectiles.push({x:wx,y:wy,vx:Math.cos(a)*320,vy:Math.sin(a)*320,radius:7,color:'#ff7700'}); }); }
            if(this.weapons.every(w=>!w.active)) this.setPhase(2);
        } else if(this.phase===2){
            this.x+=Math.cos(this.angle)*1.8;
            if(this.atk>280){ this.atk=0; for(let i=0;i<4;i++){ const a=this.angle*2+Math.PI/2*i; Game.enemyProjectiles.push({x:this.x,y:this.y,vx:Math.cos(a)*260,vy:Math.sin(a)*260,radius:9,color:this.trueColor}); } }
        } else if(this.phase===3){
            if(t){ const a=Vect.angle(this.x,this.y,t.x,t.y); this.x+=Math.cos(a)*130*dt/1000; this.y+=Math.sin(a)*130*dt/1000; }
            if(this.atk>750){ this.atk=0; const a=t?Vect.angle(this.x,this.y,t.x,t.y):0; for(let i=0;i<3;i++) Game.enemyProjectiles.push({x:this.x,y:this.y,vx:Math.cos(a+(i-1)*.28)*470,vy:Math.sin(a+(i-1)*.28)*470,radius:11,color:'#ff2244'}); }
        }
    }
    setPhase(p){
        this.phase=p; Game.cameraShake=30;
        if(p===2){ this.color=this.trueColor; UI.bossPhase.textContent='FASE 2: NÚCLEO EXPOSTO'; floatText(this.x,this.y,'SISTEMA COMPROMETIDO!',this.color); }
        else if(p===3){ this.color='#ff2244'; UI.bossPhase.textContent='FASE 3: PROTOCOLO DE FÚRIA'; floatText(this.x,this.y,'MODO FÚRIA!','#ff2244'); }
        for(let i=0;i<60;i++) Game.particles.push(new Particle(this.x,this.y,this.color,(Math.random()-.5)*35,(Math.random()-.5)*35,Math.random()*5+2));
        Game.players.forEach(p=>rumble(p.padIndex,600,.8,1.0)); this.updateUI();
    }
    takeDamage(dmg,wid=null){
        if(this.phase===1&&wid!==null){ const w=this.weapons.find(x=>x.id===wid); if(w){w.hp-=dmg;if(w.hp<=0&&w.active){w.active=false;Game.cameraShake=10;Game.players.forEach(p=>p.score+=500);}} }
        else if(this.phase===2){ this.hp-=dmg; if(this.hp<=0) this.setPhase(3); }
        else if(this.phase===3){ this.p3hp-=dmg; if(this.p3hp<=0) this.destroy(); }
        this.updateUI();
    }
    destroy(){
        Game.cameraShake=55;
        for(let i=0;i<120;i++) Game.particles.push(new Particle(this.x,this.y,'#ff7700',(Math.random()-.5)*50,(Math.random()-.5)*50,Math.random()*8+2));
        Game.players.forEach(p=>{ p.score+=5000; rumble(p.padIndex,1200,1.0,1.0); floatText(p.x,p.y-50,'BOSS DESTRUÍDO! +15 PRATA','#ffd700'); });
        addSilver(15);
        Game.boss=null; Game.enemyProjectiles=[]; UI.bossHud.classList.add('hidden'); Game.bossDefeated=true;
        let edges=0; if(this.shape==='triangle')edges=3; else if(this.shape==='square')edges=4;
        Game.enemyTheme={edges,color:this.trueColor};
        Game.map.minX-=300; Game.map.maxX+=300; Game.map.minY-=300; Game.map.maxY+=300;
        for(let i=0;i<4;i++){
            let wx,wy,ww=80,wh=80; const cx=(Game.map.minX+Game.map.maxX)/2, cy=(Game.map.minY+Game.map.maxY)/2;
            if(i===0){wx=Game.map.minX+100;wy=cy-wh/2;} else if(i===1){wx=Game.map.maxX-100-ww;wy=cy-wh/2;}
            else if(i===2){wx=cx-ww/2;wy=Game.map.minY+100;} else {wx=cx-ww/2;wy=Game.map.maxY-100-wh;}
            Game.walls.push(new Wall(wx,wy,ww,wh));
        }
        saveGame();
        setTimeout(()=>{ Game.bossDefeated=false; nextWave(); }, 3500);
    }
    updateUI(){
        let pct=0;
        if(this.phase===1){ const t=this.weapons.reduce((a,w)=>a+w.hp,0), m=this.weapons.reduce((a,w)=>a+w.maxHp,0); pct=m?t/m:0; }
        else if(this.phase===2) pct=this.maxHp?this.hp/this.maxHp:0;
        else pct=this.maxP3hp?this.p3hp/this.maxP3hp:0;
        UI.bossHpFill.style.width=Math.max(0,pct*100)+'%';
    }
    draw(c){
        c.save(); c.translate(this.x,this.y); c.rotate(this.angle);
        c.shadowColor=this.color; c.shadowBlur=effectBlur(30); c.fillStyle=this.color; c.beginPath();
        if(this.phase===1){ c.arc(0,0,this.radius,0,Math.PI*2); }
        else if(this.shape==='triangle'){ for(let i=0;i<3;i++){const a=i*Math.PI*2/3-Math.PI/2;if(i===0)c.moveTo(Math.cos(a)*this.radius,Math.sin(a)*this.radius);else c.lineTo(Math.cos(a)*this.radius,Math.sin(a)*this.radius);} }
        else if(this.shape==='square'){ c.rect(-this.radius,-this.radius,this.radius*2,this.radius*2); }
        else if(this.shape==='circle'){ c.arc(0,0,this.radius,0,Math.PI*2); }
        else if(this.shape==='cross'){ const w=this.radius*.38,r=this.radius; c.moveTo(-w,-r);c.lineTo(w,-r);c.lineTo(w,-w);c.lineTo(r,-w);c.lineTo(r,w);c.lineTo(w,w);c.lineTo(w,r);c.lineTo(-w,r);c.lineTo(-w,w);c.lineTo(-r,w);c.lineTo(-r,-w);c.lineTo(-w,-w); }
        c.closePath(); c.fill();
        c.fillStyle='rgba(255,255,255,.1)'; c.beginPath(); c.arc(-this.radius*.2,-this.radius*.25,this.radius*.28,0,Math.PI*2); c.fill();
        c.shadowBlur=0;
        if(this.phase===1) this.weapons.forEach(w=>{ if(!w.active)return; c.fillStyle='#222'; c.strokeStyle='#ff7700'; c.lineWidth=3; c.shadowColor='#ff7700'; c.shadowBlur=12; c.beginPath(); c.arc(w.relX,w.relY,22,0,Math.PI*2); c.fill(); c.stroke(); c.shadowBlur=0; c.fillStyle='rgba(0,0,0,.6)'; c.fillRect(w.relX-16,w.relY-28,32,5); c.fillStyle='#ff4400'; c.fillRect(w.relX-16,w.relY-28,32*(w.hp/w.maxHp),5); });
        c.restore();
    }
}

// ── SAVE / LOAD ──────────────────────────────
function saveGame(showNotice=true){
    try{
        localStorage.setItem('ngps4v3', JSON.stringify({
            players: Game.players.map(p=>({padIndex:p.padIndex,stats:p.stats,keys:p.keys,sensMove:p.sensMove,rumbleStr:p.rumbleStr})),
            cosmetics: normalizeCosmeticStore(cosmeticStore),
            controlDefaults: defaultControls,
            settings:{graphicsQuality}
        }));
        if(showNotice){
            UI.saveNote.classList.remove('hidden');
            setTimeout(()=>UI.saveNote.classList.add('hidden'), 1500);
        }
    }catch(e){
        // O jogo continua funcionando se o navegador bloquear armazenamento local.
    }
}
function loadSave(){
    try{
        const d=JSON.parse(localStorage.getItem('ngps4v3'));
        if(!d) return;
        cosmeticStore=normalizeCosmeticStore(d.cosmetics);
        if(d.controlDefaults) copyToDefaultControls(d.controlDefaults);
        if(d.settings && d.settings.graphicsQuality) setGraphicsQuality(d.settings.graphicsQuality);
        loadSave._data=d;
    }catch(e){}
}
loadSave();
function applyPlayerSave(p){
    const d=loadSave._data; if(!d||!d.players) return;
    const s=d.players.find(x=>x.padIndex===p.padIndex);
    if(s){
        p.stats={...p.stats,...(s.stats||{})};
        p.keys={...DEFAULT_KEYS,...(s.keys||{})};
        p.sensMove=Math.min(3,Math.max(.5,Number(s.sensMove)||p.sensMove));
        p.rumbleStr=Math.min(2,Math.max(0,Number(s.rumbleStr)||p.rumbleStr));
    }
}

// ── GAME FLOW ────────────────────────────────
function initLobby(isSolo){ Game.isSolo=isSolo; lobbyCountdown=-1; lobbyPadSignature=''; changeState(STATE.LOBBY); }

function startGame(){
    Game.wave=1; Game.timeSinceLastWave=0; Game.cameraShake=0;
    Game.enemies=[]; Game.projectiles=[]; Game.enemyProjectiles=[]; Game.particles=[]; Game.drops=[]; Game.floatingTexts=[];
    Game.boss=null; Game.bossDefeated=false; Game.enemyTheme={edges:0,color:'#ff2244'};
    Game.map={minX:0,minY:0,maxX:canvas.width,maxY:canvas.height}; Game.walls=[];
    Game.players.forEach(p=>{ p.hp=p.maxHp; p.timeStopActive=false; p.stats.skillCooldown=0; p.activeSkin=cosmeticStore.activeSkin; p.activeHat=cosmeticStore.activeHat; });
    rebuildHUD(); changeState(STATE.PLAYING); startWaveAnn();
}

function rebuildHUD(){
    UI.hudLayer.innerHTML='';
    const n=Game.players.length;
    let gc='grid-template-columns:1fr;';
    if(n===2) gc='grid-template-columns:1fr 1fr;';
    else if(n>2) gc='grid-template-columns:1fr 1fr;grid-template-rows:1fr 1fr;';
    UI.hudLayer.style.cssText='position:absolute;top:0;left:0;width:100vw;height:100vh;z-index:10;pointer-events:none;display:grid;'+gc;

    Game.players.forEach((p,i)=>{
        p.hudId='hud-p'+p.padIndex;
        const h=document.createElement('div');
        h.id=p.hudId; h.className='player-hud-container';
        h.style.borderColor=p.color+'44';
        h.innerHTML=
            '<div class="hud-panel">'+
              '<div class="hud-left">'+
                '<div class="hud-card">'+
                  '<div class="hud-player-name" style="color:'+p.color+'">JOGADOR '+(i+1)+'</div>'+
                  '<div class="hud-stat">&#11088; <span class="hud-score">0</span></div>'+
                  '<div class="hud-stat" style="color:#ffd700">&#9711; <span class="hud-gold">0</span> <span style="color:#b8c4d4;margin-left:8px;">&#9830; <span class="hud-silver">0</span></span></div>'+
                '</div>'+
                '<div class="hud-card">'+
                  '<div class="hud-bar-label">INTEGRIDADE</div>'+
                  '<div class="hud-bar-wrap"><div class="hud-bar-fill hp-fill" style="width:100%"></div></div>'+
                  '<div class="hud-bar-label">HABILIDADE ['+btnN(p.keys.skill)+']</div>'+
                  '<div class="hud-bar-wrap"><div class="hud-bar-fill cd-fill" style="width:0%;background:'+p.color+'"></div></div>'+
                '</div>'+
              '</div>'+
              '<div class="hud-right">'+
                '<div class="hud-card shop-card">'+
                  '<div class="shop-title">UPGRADES</div>'+
                  '<div class="shop-item"><span class="shop-key">['+btnN(p.keys.upg1)+']</span><span>Tiro</span><span class="upg1-cost" style="color:#ffd700">15 G</span></div>'+
                  '<div class="shop-item"><span class="shop-key">['+btnN(p.keys.upg2)+']</span><span>Dano</span><span class="upg2-cost" style="color:#ffd700">40 G</span></div>'+
                  '<div class="shop-item"><span class="shop-key">['+btnN(p.keys.upg3)+']</span><span>Hab.</span><span class="upg3-cost" style="color:#ffd700">25 G</span></div>'+
                '</div>'+
              '</div>'+
            '</div>';
        UI.hudLayer.appendChild(h);
        p.updateHUD();
    });
}

function startWaveAnn(){
    const isBoss=Game.wave%5===0;
    UI.announceText.textContent=isBoss?'BOSS APROXIMANDO':'ONDA '+Game.wave;
    UI.announceText.style.color=isBoss?'#ff2244':'#00d4ff';
    UI.announceText.style.textShadow=isBoss?'0 0 30px #ff2244':'0 0 30px #00d4ff';
    UI.announce.classList.remove('hidden');
    UI.announceText.style.animation='none'; void UI.announceText.offsetWidth;
    UI.announceText.style.animation='announce-anim 2.5s forwards';
    setTimeout(()=>UI.announce.classList.add('hidden'),2500);
}

function nextWave(){ Game.wave++; Game.timeSinceLastWave=0; if(Game.wave%5===0) Game.boss=new Boss(); startWaveAnn(); }

function spawnEnemies(dt){
    if(Game.boss||Game.bossDefeated||Game.players.some(p=>p.timeStopActive)) return;
    Game.timeSinceLastWave+=dt;
    if(Game.timeSinceLastWave>Game.waveDuration){ if(Game.enemies.length===0) nextWave(); return; }
    const ch=(0.02+Game.wave*.005)*Game.players.length;
    if(Math.random()<ch && Game.enemies.length<(20+Game.wave*2)*Game.players.length){
        const sd=Math.floor(Math.random()*4); let x,y;
        if(sd===0){x=Game.map.minX+Math.random()*(Game.map.maxX-Game.map.minX);y=Game.map.minY-35;}
        else if(sd===1){x=Game.map.maxX+35;y=Game.map.minY+Math.random()*(Game.map.maxY-Game.map.minY);}
        else if(sd===2){x=Game.map.minX+Math.random()*(Game.map.maxX-Game.map.minX);y=Game.map.maxY+35;}
        else {x=Game.map.minX-35;y=Game.map.minY+Math.random()*(Game.map.maxY-Game.map.minY);}
        Game.enemies.push(new Enemy(x,y,(40+Game.wave*15)*(1+(Game.players.length-1)*.4),100+Math.random()*60+Game.wave*5));
    }
}

function checkGameOver(){
    if(Game.players.every(p=>p.hp<=0)){
        UI.finalWave.textContent=Game.wave;
        UI.finalScore.textContent=Game.players.reduce((s,p)=>s+p.score,0);
        changeState(STATE.GAMEOVER); saveGame();
    }
}

// ── STATE MACHINE ────────────────────────────
const PANELS=['main-menu','lobby-screen','pause-screen','controls-screen','skin-shop-screen','game-over','global-hud','hud-layer'];

function changeState(ns){
    currentState=ns; menuIndex=0; bindingAction=null;
    PANELS.forEach(id=>{ const e=document.getElementById(id); if(e) e.classList.add('hidden'); });

    switch(ns){
        case STATE.MENU:     UI.mainMenu.classList.remove('hidden'); break;
        case STATE.LOBBY:    UI.lobbyScreen.classList.remove('hidden'); break;
        case STATE.PLAYING:  UI.globalHud.classList.remove('hidden'); UI.hudLayer.classList.remove('hidden'); Game.lastTime=performance.now(); break;
        case STATE.PAUSED:   UI.pauseScreen.classList.remove('hidden'); UI.globalHud.classList.remove('hidden'); UI.hudLayer.classList.remove('hidden'); UI.pauseWho.textContent='JOGADOR '+(pauseCallerIdx+1)+' PAUSOU'; break;
        case STATE.GAMEOVER: UI.gameOver.classList.remove('hidden'); break;
        case STATE.CONTROLS: UI.controlsScreen.classList.remove('hidden'); UI.cfgPnum.textContent=pauseCallerIdx+1; refreshCfg(); break;
        case STATE.SKINS:    UI.skinShop.classList.remove('hidden'); refreshSkins(); break;
    }
    updSel();
}

function getBtns(){ const f=UI.menuBtns[currentState]; return f ? f().filter(b=>b.tagName==='BUTTON') : []; }
function updSel(){ const b=getBtns(); b.forEach((x,i)=>{ if(i===menuIndex)x.classList.add('selected'); else x.classList.remove('selected'); }); }
function menuNav(d){ const b=getBtns(); if(!b.length)return; menuIndex=(menuIndex+d+b.length)%b.length; updSel(); }
function navigateSkinMenu(direction){
    const buttons=getBtns();
    if(buttons.length<4) return;
    const firstCard=2, lastCard=buttons.length-2, back=buttons.length-1;
    let next=menuIndex;
    if(menuIndex<firstCard){
        if(direction==='left'||direction==='right') next=menuIndex===0?1:0;
        else if(direction==='down') next=firstCard;
    } else if(menuIndex===back){
        if(direction==='up'||direction==='left'||direction==='right') next=lastCard;
    } else {
        if(direction==='left') next=Math.max(firstCard,menuIndex-1);
        if(direction==='right') next=Math.min(lastCard,menuIndex+1);
        if(direction==='up') next=menuIndex-3>=firstCard?menuIndex-3:(menuIndex%3===2?0:1);
        if(direction==='down') next=menuIndex+3<=lastCard?menuIndex+3:back;
    }
    menuIndex=next;
    const target=buttons[menuIndex];
    if(target && target.scrollIntoView) target.scrollIntoView({block:'nearest',inline:'nearest'});
    updSel();
}

function openSettingsScreen(state){
    settingsReturnState=currentState===STATE.PAUSED ? STATE.PAUSED : STATE.MENU;
    changeState(state);
}

function closeSettingsScreen(){ changeState(settingsReturnState); }

function menuConfirm(){
    const b=getBtns(); if(!b[menuIndex]) return;
    const btn=b[menuIndex], act=btn.dataset.action, bind=btn.dataset.bind, setting=btn.dataset.setting;
    if(btn.dataset.tab){
        skinTab=btn.dataset.tab;
        refreshSkins();
        menuIndex=btn.dataset.tab==='skins'?0:1;
        updSel();
        return;
    }
    if(btn.dataset.cosmeticId){
        selectCosmetic(btn.dataset.cosmeticType,btn.dataset.cosmeticId);
        return;
    }
    if(setting){
        changeConfigSetting(setting,1);
        return;
    }
    if(bind){
        // Start listening for button press
        bindingAction=bind;
        document.querySelectorAll('.cfg-btn').forEach(x=>x.classList.remove('listening'));
        btn.classList.add('listening');
        UI.bindingMsg.classList.remove('hidden');
        return;
    }
    switch(act){
        case 'solo':      initLobby(true); break;
        case 'local':     initLobby(false); break;
        case 'controls':  pauseCallerIdx=Game.players.length?Game.players[0].padIndex:0; openSettingsScreen(STATE.CONTROLS); break;
        case 'skins':     openSettingsScreen(STATE.SKINS); break;
        case 'exit':      window.close(); break;
        case 'resume':    changeState(STATE.PLAYING); break;
        case 'quit':      changeState(STATE.MENU); break;
        case 'back':      closeSettingsScreen(); break;
        case 'back-skin': closeSettingsScreen(); break;
    }
}

function refreshCfg(){
    const p=getConfigTarget();
    document.querySelectorAll('.cfg-btn[data-bind]').forEach(b=>{ b.textContent=btnN(p.keys[b.dataset.bind]); b.classList.remove('listening'); });
    const graphicsBtn=document.querySelector('.cfg-btn[data-setting="graphics"]');
    const sensBtn=document.querySelector('.cfg-btn[data-setting="sensitivity"]');
    const rumbleBtn=document.querySelector('.cfg-btn[data-setting="rumble"]');
    if(graphicsBtn) graphicsBtn.textContent=quality().label;
    if(sensBtn) sensBtn.textContent=(Number(p.sensMove)||1).toFixed(1)+'x';
    if(rumbleBtn) rumbleBtn.textContent=Math.round((Number(p.rumbleStr)||0)*100)+'%';
    UI.bindingMsg.classList.add('hidden'); bindingAction=null;
}

// ── SKIN SHOP ────────────────────────────────
function refreshSkins(){
    const items=skinTab==='skins'?ALL_SKINS:ALL_HATS;
    const ownedL=skinTab==='skins'?cosmeticStore.ownedSkins:cosmeticStore.ownedHats;
    const activeId=skinTab==='skins'?cosmeticStore.activeSkin:cosmeticStore.activeHat;
    if(UI.shopSilver) UI.shopSilver.textContent=getWallet().silver;
    UI.skinGrid.innerHTML='';

    items.forEach(item=>{
        const owned=ownedL.includes(item.id), active=activeId===item.id;
        const card=document.createElement('button');
        card.type='button';
        card.dataset.cosmeticType=skinTab;
        card.dataset.cosmeticId=item.id;
        card.className='skin-card'+(owned?' owned':'')+(active?' equipped':'');
        const statusLabel=active?'✓ EQUIPADO':owned?'✕ EQUIPAR':item.cost===0?'GRÁTIS':item.cost+' PRATA';
        card.innerHTML='<canvas class="skin-preview" width="56" height="56"></canvas>'+
            '<div class="skin-name">'+item.name+'</div>'+
            '<div class="skin-cost" style="'+(active?'color:#00ff88':owned?'color:#ffd700':'')+'">'+statusLabel+'</div>';
        drawPrev(card.querySelector('canvas'), skinTab==='skins'?item.id:'default', skinTab==='hats'?item.id:'none');
        UI.skinGrid.appendChild(card);
    });
    renderCharacterPreview();
}

function selectCosmetic(type,id){
    const isSkin=type==='skins';
    const items=isSkin?ALL_SKINS:ALL_HATS;
    const item=items.find(entry=>entry.id===id);
    if(!item) return;
    const owned=isSkin?cosmeticStore.ownedSkins:cosmeticStore.ownedHats;
    const activeKey=isSkin?'activeSkin':'activeHat';

    if(!owned.includes(id)){
        if(getWallet().silver<item.cost){
            if(UI.bindingMsg){
                UI.bindingMsg.textContent='PRATA INSUFICIENTE';
                UI.bindingMsg.classList.remove('hidden');
                setTimeout(()=>UI.bindingMsg.classList.add('hidden'),1200);
            }
            return;
        }
        getWallet().silver-=item.cost;
        owned.push(id);
    }
    cosmeticStore[activeKey]=id;
    Game.players.forEach(player=>{
        player.activeSkin=cosmeticStore.activeSkin;
        player.activeHat=cosmeticStore.activeHat;
        player.updateHUD();
    });
    saveGame();
    refreshSkins();
    const next=getBtns().findIndex(button=>button.dataset.cosmeticId===id);
    if(next>=0) menuIndex=next;
    updSel();
}

function renderCharacterPreview(){
    const cv=UI.previewCanvas;
    if(!cv) return;
    const c=cv.getContext('2d');
    c.clearRect(0,0,cv.width,cv.height);
    const bg=c.createRadialGradient(125,112,8,125,112,125);
    bg.addColorStop(0,'rgba(44,26,100,.42)'); bg.addColorStop(1,'rgba(1,4,15,.08)');
    c.fillStyle=bg; c.fillRect(0,0,cv.width,cv.height);
    c.strokeStyle='rgba(135,95,255,.18)'; c.lineWidth=1;
    for(let i=18;i<250;i+=24){c.beginPath();c.moveTo(i,0);c.lineTo(i,250);c.stroke();c.beginPath();c.moveTo(0,i);c.lineTo(250,i);c.stroke();}
    const avatar=Object.create(Player.prototype);
    avatar.x=125; avatar.y=132; avatar.radius=48; avatar.hp=100; avatar.invincible=0;
    avatar.color='#2277ff'; avatar.activeSkin=cosmeticStore.activeSkin; avatar.activeHat=cosmeticStore.activeHat;
    avatar.skinAnim=Date.now()/900;
    avatar.draw(c);
    const skin=ALL_SKINS.find(item=>item.id===cosmeticStore.activeSkin);
    const hat=ALL_HATS.find(item=>item.id===cosmeticStore.activeHat);
    if(UI.previewSkin) UI.previewSkin.textContent='TRAJE: '+(skin?skin.name:'NÚCLEO PADRÃO');
    if(UI.previewHat) UI.previewHat.textContent='ACESSÓRIO: '+(hat?hat.name:'SEM ACESSÓRIO');
}

function drawPrev(cv,skId,htId){
    const c=cv.getContext('2d'); c.clearRect(0,0,56,56);
    const avatar=Object.create(Player.prototype);
    avatar.x=28; avatar.y=31; avatar.radius=13; avatar.hp=100; avatar.invincible=0;
    avatar.color='#2277ff'; avatar.activeSkin=skId; avatar.activeHat=htId; avatar.skinAnim=.45;
    avatar.draw(c);
}

// ── GAMEPAD POLLING ──────────────────────────
let lastPS = {};

function checkGamepad(){
    const pads=getConnectedGamepads();

    // LOBBY: watch for connected controllers
    if(currentState===STATE.LOBBY){
        const connected=[];
        for(let i=0;i<4;i++) if(pads[i]) connected.push(i);
        const signature=connected.join(',');
        if(signature!==lobbyPadSignature){
            lobbyPadSignature=signature;
            Game.players=connected.map(i=>{ const player=new Player(i); applyPlayerSave(player); return player; });
            let html='';
            connected.forEach(i=>{
                html+='<div style="padding:16px 20px;border-radius:10px;text-align:center;background:rgba(0,0,0,.5);border:2px solid '+PColors[i]+';color:'+PColors[i]+';min-width:110px;">'+
                      '<div style="font-family:Orbitron,sans-serif;font-size:.75rem;letter-spacing:2px;margin-bottom:5px;">P'+(i+1)+'</div>'+
                      '<div style="color:#aaa;font-size:.72rem;">PRONTO</div></div>';
            });
            UI.lobbyPlayers.innerHTML=html||'<p style="color:#444;letter-spacing:2px;font-size:.9rem;">NENHUM CONTROLE DETECTADO</p>';
            lobbyCountdown=-1;
        }
        const nd=Game.isSolo?1:2;
        if(Game.players.length>=nd){
            if(lobbyCountdown<0) lobbyCountdown=3000;
            lobbyCountdown-=16;
            UI.lobbyStatus.textContent='INICIANDO EM '+Math.ceil(lobbyCountdown/1000)+'...';
            UI.lobbyStatus.style.color='#00ff88';
            if(lobbyCountdown<=0) startGame();
        } else {
            lobbyCountdown=-1;
            UI.lobbyStatus.textContent='CONECTE '+(nd===1?'1':'NO MÍNIMO '+nd)+' CONTROLE'+(nd>1?'S':'');
            UI.lobbyStatus.style.color='#ff2244';
        }
    }

    for(let i=0;i<4;i++){
        const pad=pads[i]; if(!pad) continue;
        const k='p'+i;
        if(!lastPS[k]) lastPS[k]={b:[]};
        const pv=lastPS[k].b;
        const pr=idx=>!!(pad.buttons[idx]&&pad.buttons[idx].pressed);
        const jt=idx=>pr(idx)&&!pv[idx];

        // ─ BINDING MODE ─
        if(bindingAction && i===pauseCallerIdx){
            for(let b=0;b<pad.buttons.length;b++){
                if(pad.buttons[b].pressed && !pv[b]){
                    if(b===1){ bindingAction=null; UI.bindingMsg.classList.add('hidden'); break; }
                    const p=getConfigTarget();
                    p.keys[bindingAction]=b;
                    copyToDefaultControls(p);
                    saveGame();
                    // Update label immediately
                    document.querySelectorAll('.cfg-btn[data-bind="'+bindingAction+'"]').forEach(el=>{
                        el.textContent=btnN(b);
                        el.classList.remove('listening');
                    });
                    UI.bindingMsg.classList.add('hidden');
                    bindingAction=null;
                    break;
                }
            }
            for(let b=0;b<pad.buttons.length;b++) pv[b]=pr(b);
            continue;
        }

        const UP=jt(12),DN=jt(13),LT=jt(14),RT=jt(15);
        const CRS=jt(0), CRC=jt(1), L2=jt(6), R2=jt(7);

        const inMenu=[STATE.MENU,STATE.PAUSED,STATE.GAMEOVER,STATE.CONTROLS,STATE.SKINS].includes(currentState);
        const isCaller=currentState===STATE.MENU||currentState===STATE.GAMEOVER||i===pauseCallerIdx;

        // A sala não faz parte dos menus navegáveis, mas ○ deve cancelar a espera.
        if(currentState===STATE.LOBBY && CRC){
            changeState(STATE.MENU);
        }

        if(inMenu && isCaller){
            if(currentState===STATE.SKINS){
                if(UP) navigateSkinMenu('up');
                if(DN) navigateSkinMenu('down');
                if(LT) navigateSkinMenu('left');
                if(RT) navigateSkinMenu('right');
            } else {
                if(UP) menuNav(-1);
                if(DN) menuNav(1);
            }
            if(CRS) menuConfirm();
            if(CRC){
                if(currentState===STATE.CONTROLS||currentState===STATE.SKINS)
                    closeSettingsScreen();
                else if(currentState===STATE.PAUSED) changeState(STATE.PLAYING);
            }
            // Ajustes são todos acessíveis pelo D-PAD, sem precisar de mouse.
            if(currentState===STATE.CONTROLS){
                const selected=getBtns()[menuIndex];
                if(selected && selected.dataset.setting){
                    if(LT) changeConfigSetting(selected.dataset.setting,-1);
                    if(RT) changeConfigSetting(selected.dataset.setting,1);
                }
            }
        }

        // In-game actions
        if(currentState===STATE.PLAYING){
            const p=Game.players.find(pl=>pl.padIndex===i);
            if(p){
                if(jt(p.keys.pause)){ pauseCallerIdx=i; changeState(STATE.PAUSED); }
                if(jt(p.keys.skill)&&p.stats.skillCooldown<=0&&!p.timeStopActive) p.activateTimeStop();
                if(jt(p.keys.upg1)) p.buyUpgrade(1);
                if(jt(p.keys.upg2)) p.buyUpgrade(2);
                if(jt(p.keys.upg3)) p.buyUpgrade(3);
            }
        }

        for(let b=0;b<pad.buttons.length;b++) pv[b]=pr(b);
    }
}

// ── RENDERING ────────────────────────────────
function drawBackground(c,vx,vy,vw,vh){
    const q=quality();
    const bg=c.createRadialGradient(vx+vw*.48,vy+vh*.42,0,vx+vw*.5,vy+vh*.5,Math.max(vw,vh)*.82);
    bg.addColorStop(0,q===QUALITY_PROFILES.HIGH?'#0b1530':'#070c18');
    bg.addColorStop(.52,'#050810'); bg.addColorStop(1,'#02040a');
    c.fillStyle=bg; c.fillRect(vx,vy,vw,vh);
    if(!q.stars) return;
    const now=Date.now()/1000;
    for(let i=0;i<q.stars;i++){
        const x=vx+((i*83+17)%997)/997*vw;
        const y=vy+((i*173+71)%991)/991*vh;
        const pulse=.32+((Math.sin(now*1.5+i*2.7)+1)*.18);
        c.fillStyle=i%7===0?'rgba(160,105,255,'+pulse+')':'rgba(110,215,255,'+pulse+')';
        const size=i%9===0?1.8:1;
        c.fillRect(x,y,size,size);
    }
    if(q===QUALITY_PROFILES.HIGH){
        const haze=c.createRadialGradient(vx+vw*.2,vy+vh*.8,0,vx+vw*.2,vy+vh*.8,vw*.55);
        haze.addColorStop(0,'rgba(70,20,170,.10)'); haze.addColorStop(1,'rgba(70,20,170,0)');
        c.fillStyle=haze; c.fillRect(vx,vy,vw,vh);
    }
}

function drawScanlines(c,vx,vy,vw,vh){
    if(!quality().scanlines) return;
    c.fillStyle='rgba(110,180,255,.025)';
    for(let y=vy;y<vy+vh;y+=4) c.fillRect(vx,y,vw,1);
}

function drawViewport(c,vx,vy,vw,vh,p){
    c.save(); c.beginPath(); c.rect(vx,vy,vw,vh); c.clip();

    drawBackground(c,vx,vy,vw,vh);

    // Time stop blue tint
    if(Game.players.some(pl=>pl.timeStopActive)){ c.fillStyle='rgba(0,100,220,.07)'; c.fillRect(vx,vy,vw,vh); }

    const sh=Game.cameraShake>.5?{x:(Math.random()-.5)*Game.cameraShake,y:(Math.random()-.5)*Game.cameraShake}:{x:0,y:0};
    c.translate(vx+vw/2-p.camera.x+sh.x, vy+vh/2-p.camera.y+sh.y);

    // Grid
    const cs=80, sx=Math.floor((p.camera.x-vw/2)/cs)*cs, sy=Math.floor((p.camera.y-vh/2)/cs)*cs;
    c.strokeStyle='rgba(0,180,255,'+quality().gridAlpha+')'; c.lineWidth=1;
    for(let x=sx;x<p.camera.x+vw/2;x+=cs){ c.beginPath(); c.moveTo(x,p.camera.y-vh/2); c.lineTo(x,p.camera.y+vh/2); c.stroke(); }
    for(let y=sy;y<p.camera.y+vh/2;y+=cs){ c.beginPath(); c.moveTo(p.camera.x-vw/2,y); c.lineTo(p.camera.x+vw/2,y); c.stroke(); }

    // Map border glow
    c.strokeStyle='rgba(0,180,255,.2)'; c.lineWidth=3; c.shadowColor='#00d4ff'; c.shadowBlur=effectBlur(10);
    c.strokeRect(Game.map.minX,Game.map.minY,Game.map.maxX-Game.map.minX,Game.map.maxY-Game.map.minY); c.shadowBlur=0;

    Game.walls.forEach(w=>w.draw(c));
    Game.drops.forEach(d=>d.draw(c));
    for(let i=0;i<Game.particles.length;i+=quality().particleStep) Game.particles[i].draw(c);

    // Player projectiles
    Game.projectiles.forEach(pr=>{ c.fillStyle=pr.color; c.shadowColor=pr.color; c.shadowBlur=effectBlur(14); c.beginPath(); c.arc(pr.x,pr.y,pr.radius,0,Math.PI*2); c.fill(); c.shadowBlur=0; });
    // Enemy projectiles
    Game.enemyProjectiles.forEach(pr=>{ c.fillStyle=pr.color; c.shadowColor=pr.color; c.shadowBlur=effectBlur(14); c.beginPath(); c.arc(pr.x,pr.y,pr.radius,0,Math.PI*2); c.fill(); c.shadowBlur=0; });

    Game.enemies.forEach(e=>e.draw(c));
    if(Game.boss) Game.boss.draw(c);
    Game.players.forEach(pl=>pl.draw(c));
    Game.floatingTexts.forEach(ft=>ft.draw(c));
    c.restore();
    drawScanlines(c,vx,vy,vw,vh);
}

// ── MAIN LOOP ────────────────────────────────
function gameLoop(ts){
    requestAnimationFrame(gameLoop);
    checkGamepad();
    if(currentState!==STATE.PLAYING) return;

    const dt=Math.min(ts-Game.lastTime, 80);
    Game.lastTime=ts;

    Game.players.forEach(p=>p.update(dt));
    spawnEnemies(dt);
    if(Game.boss) Game.boss.update(dt);

    // ─ Player projectiles ─
    for(let i=Game.projectiles.length-1;i>=0;i--){
        const p=Game.projectiles[i]; p.x+=p.vx*dt/1000; p.y+=p.vy*dt/1000; let hit=false;
        if(Game.boss){
            if(Game.boss.phase===1){ for(const w of Game.boss.weapons){ if(!w.active)continue; const wx=Game.boss.x+Math.cos(Game.boss.angle)*w.relX-Math.sin(Game.boss.angle)*w.relY,wy=Game.boss.y+Math.sin(Game.boss.angle)*w.relX+Math.cos(Game.boss.angle)*w.relY; if(Vect.dist(p.x,p.y,wx,wy)<p.radius+22){Game.boss.takeDamage(10*(p.damage/20),w.id);for(let j=0;j<5;j++)Game.particles.push(new Particle(p.x,p.y,'#ff7700',(Math.random()-.5)*15,(Math.random()-.5)*15,2));if(p.owner)p.owner.score+=5;hit=true;break;} } }
            else { if(Vect.dist(p.x,p.y,Game.boss.x,Game.boss.y)<p.radius+Game.boss.radius){Game.boss.takeDamage(10*(p.damage/20));for(let j=0;j<5;j++)Game.particles.push(new Particle(p.x,p.y,Game.boss.color,(Math.random()-.5)*18,(Math.random()-.5)*18,2));if(p.owner)p.owner.score+=5;hit=true;} }
        }
        if(!hit){
            for(let j=Game.enemies.length-1;j>=0;j--){
                const e=Game.enemies[j];
                if(Vect.dist(p.x,p.y,e.x,e.y) >= p.radius+e.radius) continue;

                e.hp-=p.damage;
                for(let k=0;k<5;k++){
                    Game.particles.push(new Particle(p.x,p.y,e.color,(Math.random()-.5)*10,(Math.random()-.5)*10,2));
                }
                if(e.hp<=0){
                    if(Math.random()<.4) Game.drops.push(new Coin(e.x,e.y));
                    if(Math.random()<.18) Game.drops.push(new Coin(e.x,e.y,true)); // Prata para a loja persistente
                    if(p.owner) p.owner.score+=10;
                    Game.enemies.splice(j,1);
                }
                hit=true;
                break;
            }
        }
        if(!hit){ for(let w=Game.walls.length-1;w>=0;w--){ const wall=Game.walls[w]; if(p.x>wall.x&&p.x<wall.x+wall.w&&p.y>wall.y&&p.y<wall.y+wall.h){wall.hp--;if(wall.hp<=0)Game.walls.splice(w,1);hit=true;break;} } }
        if(hit||p.x<Game.map.minX-200||p.x>Game.map.maxX+200||p.y<Game.map.minY-200||p.y>Game.map.maxY+200) Game.projectiles.splice(i,1);
    }

    // ─ Enemies vs players ─
    const anyStop=Game.players.some(pl=>pl.timeStopActive);
    for(let i=Game.enemies.length-1;i>=0;i--){
        const e=Game.enemies[i]; e.update(dt);
        if(anyStop) continue;
        for(const p of Game.players){ if(p.hp>0&&Vect.dist(e.x,e.y,p.x,p.y)<e.radius+p.radius){ p.takeDamage(15); Game.enemies.splice(i,1); for(let k=0;k<14;k++)Game.particles.push(new Particle(e.x,e.y,e.color,(Math.random()-.5)*20,(Math.random()-.5)*20,3)); break; } }
    }

    // ─ Enemy projectiles ─
    for(let i=Game.enemyProjectiles.length-1;i>=0;i--){
        const pr=Game.enemyProjectiles[i];
        if(!anyStop){ pr.x+=pr.vx*dt/1000; pr.y+=pr.vy*dt/1000; }
        let hit=false;
        if(!anyStop){ for(const p of Game.players){ if(p.hp>0&&Vect.dist(pr.x,pr.y,p.x,p.y)<pr.radius+p.radius){p.takeDamage(10);hit=true;break;} } }
        if(!hit){ for(let w=Game.walls.length-1;w>=0;w--){ const wall=Game.walls[w]; if(pr.x>wall.x&&pr.x<wall.x+wall.w&&pr.y>wall.y&&pr.y<wall.y+wall.h){wall.hp--;if(wall.hp<=0)Game.walls.splice(w,1);hit=true;break;} } }
        if(hit||pr.x<Game.map.minX-200||pr.x>Game.map.maxX+200||pr.y<Game.map.minY-200||pr.y>Game.map.maxY+200) Game.enemyProjectiles.splice(i,1);
    }

    // ─ Drops / Particles / Texts ─
    for(let i=Game.drops.length-1;i>=0;i--){ Game.drops[i].update(dt); if(Game.drops[i].life<=0) Game.drops.splice(i,1); }
    for(let i=Game.particles.length-1;i>=0;i--){ Game.particles[i].update(dt); if(Game.particles[i].alpha<=0) Game.particles.splice(i,1); }
    for(let i=Game.floatingTexts.length-1;i>=0;i--){ Game.floatingTexts[i].update(dt); if(Game.floatingTexts[i].life<=0) Game.floatingTexts.splice(i,1); }
    const maxParticles=quality().maxParticles;
    if(Game.particles.length>maxParticles) Game.particles.splice(0,Game.particles.length-maxParticles);
    if(Game.cameraShake>0) Game.cameraShake*=.88;

    // ─ Draw split-screen ─
    ctx.clearRect(0,0,canvas.width,canvas.height);
    const n=Game.players.length; if(n===0) return;
    if(n===1){
        drawViewport(ctx,0,0,canvas.width,canvas.height,Game.players[0]);
    } else if(n===2){
        const hw=canvas.width/2;
        drawViewport(ctx,0,0,hw,canvas.height,Game.players[0]);
        drawViewport(ctx,hw,0,hw,canvas.height,Game.players[1]);
        ctx.strokeStyle='rgba(255,255,255,.2)'; ctx.lineWidth=3;
        ctx.beginPath(); ctx.moveTo(hw,0); ctx.lineTo(hw,canvas.height); ctx.stroke();
    } else {
        const hw=canvas.width/2, hh=canvas.height/2;
        drawViewport(ctx,0,0,hw,hh,Game.players[0]);
        drawViewport(ctx,hw,0,hw,hh,Game.players[1]);
        drawViewport(ctx,0,hh,hw,hh,Game.players[2]);
        if(n>=4) drawViewport(ctx,hw,hh,hw,hh,Game.players[3]);
        ctx.strokeStyle='rgba(255,255,255,.2)'; ctx.lineWidth=3;
        ctx.beginPath(); ctx.moveTo(hw,0); ctx.lineTo(hw,canvas.height); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0,hh); ctx.lineTo(canvas.width,hh); ctx.stroke();
    }
}

// ── BOOT ─────────────────────────────────────
requestAnimationFrame(gameLoop);

// ── MOUSE & KEYBOARD FALLBACK FOR MENUS ──────
document.addEventListener('click', e => {
    const btn = e.target.closest('.menu-btn, .skin-card');
    if(!btn) return;
    const btns = getBtns();
    const idx = btns.indexOf(btn);
    if(idx !== -1){
        menuIndex = idx;
        updSel();
        menuConfirm();
    }
});

window.addEventListener('keydown', e => {
    const inMenu = [STATE.MENU,STATE.PAUSED,STATE.GAMEOVER,STATE.CONTROLS,STATE.SKINS].includes(currentState);
    if(inMenu){
        if(e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') { currentState===STATE.SKINS?navigateSkinMenu('up'):menuNav(-1); e.preventDefault(); }
        if(e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') { currentState===STATE.SKINS?navigateSkinMenu('down'):menuNav(1); e.preventDefault(); }
        if(e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A') {
            if(currentState===STATE.SKINS) navigateSkinMenu('left');
            else if(currentState===STATE.CONTROLS){ const button=getBtns()[menuIndex]; if(button && button.dataset.setting) changeConfigSetting(button.dataset.setting,-1); }
            e.preventDefault();
        }
        if(e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') {
            if(currentState===STATE.SKINS) navigateSkinMenu('right');
            else if(currentState===STATE.CONTROLS){ const button=getBtns()[menuIndex]; if(button && button.dataset.setting) changeConfigSetting(button.dataset.setting,1); }
            e.preventDefault();
        }
        if(e.key === 'Enter' || e.key === ' ') { menuConfirm(); e.preventDefault(); }
        if(e.key === 'Escape') {
            if(currentState===STATE.CONTROLS||currentState===STATE.SKINS) closeSettingsScreen();
            else if(currentState===STATE.PAUSED) changeState(STATE.PLAYING);
        }
    } else if (currentState === STATE.LOBBY) {
        if(e.key === 'Escape') changeState(STATE.MENU);
    } else if (currentState === STATE.PLAYING) {
        if(e.key === 'Escape') { pauseCallerIdx=0; changeState(STATE.PAUSED); }
    }
});
