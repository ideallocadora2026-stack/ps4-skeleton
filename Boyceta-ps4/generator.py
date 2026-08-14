import os

js_code = """
const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d');

function resize() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
}
window.addEventListener('resize', resize);
resize();

const UI = {
    menusLayer: document.getElementById('menus-layer'),
    mainMenu: document.getElementById('main-menu'),
    lobbyScreen: document.getElementById('lobby-screen'),
    lobbyPlayers: document.getElementById('lobby-players'),
    lobbyStatus: document.getElementById('lobby-status'),
    pauseScreen: document.getElementById('pause-screen'),
    controlsScreen: document.getElementById('controls-screen'),
    gameOver: document.getElementById('game-over'),
    hudLayer: document.getElementById('hud-layer'),
    globalHud: document.getElementById('global-hud'),
    bossHud: document.getElementById('boss-hud'),
    bossName: document.getElementById('boss-name'),
    bossPhase: document.getElementById('boss-phase'),
    bossHpFill: document.getElementById('boss-hp-fill'),
    announce: document.getElementById('announcement'),
    announceText: document.getElementById('announce-text'),
    saveNotification: document.getElementById('save-notification'),
    finalWave: document.getElementById('final-wave'),
    finalScore: document.getElementById('final-score'),
    cfgSens: document.getElementById('cfg-sens'),
    bindingMsg: document.getElementById('binding-msg'),
    
    menuBtns: {
        'MENU': Array.from(document.getElementById('main-menu-options').querySelectorAll('.menu-btn')),
        'PAUSED': Array.from(document.getElementById('pause-menu-options').querySelectorAll('.menu-btn')),
        'GAMEOVER': Array.from(document.getElementById('go-menu-options').querySelectorAll('.menu-btn')),
        'CONTROLS': Array.from(document.getElementById('cfg-menu-options').querySelectorAll('.menu-btn'))
    }
};

const STATE = { MENU: 'MENU', LOBBY: 'LOBBY', PLAYING: 'PLAYING', PAUSED: 'PAUSED', GAMEOVER: 'GAMEOVER', CONTROLS: 'CONTROLS' };
let currentState = STATE.MENU;
let menuIndex = 0;
let pauseMenuCaller = -1;
let bindingAction = null;
let lobbyCountdown = -1;

const PlayerColors = ['#003791', '#ff003c', '#00ff00', '#b026ff'];
const GamepadNames = ['Azul', 'Vermelho', 'Verde', 'Rosa'];
const BossShapes = ['triangle', 'square', 'circle', 'cross'];

const Game = {
    lastTime: 0,
    wave: 1,
    timeSinceLastWave: 0,
    waveDuration: 25000,
    cameraShake: 0,
    map: { minX: 0, minY: 0, maxX: window.innerWidth, maxY: window.innerHeight },
    walls: [],
    players: [],
    projectiles: [],
    enemyProjectiles: [],
    enemies: [],
    boss: null,
    bossDefeated: false,
    particles: [],
    drops: [],
    floatingTexts: [],
    enemyTheme: { edges: 0, color: '#ff003c' },
    isSolo: true
};

function rumble(padIndex, duration, weakMag, strongMag) {
    const pad = navigator.getGamepads()[padIndex];
    if(pad && pad.vibrationActuator) {
        pad.vibrationActuator.playEffect('dual-rumble', {
            startDelay: 0, duration: duration, 
            weakMagnitude: weakMag, 
            strongMagnitude: strongMag
        }).catch(e=>{});
    }
}

function createFloatingText(x, y, text, color) {
    Game.floatingTexts.push(new FloatingText(x, y, text, color));
}

// Classes
class Vector {
    static dist(x1, y1, x2, y2) { return Math.hypot(x2 - x1, y2 - y1); }
    static angle(x1, y1, x2, y2) { return Math.atan2(y2 - y1, x2 - x1); }
}

class FloatingText {
    constructor(x, y, text, color) {
        this.x = x; this.y = y; this.text = text; this.color = color; this.life = 1.0;
    }
    update(dt) { this.y -= (30 * dt) / 1000; this.life -= dt / 1000; }
    draw(ctx) {
        ctx.save(); ctx.globalAlpha = Math.max(0, this.life); ctx.fillStyle = this.color;
        ctx.font = "bold 16px Rajdhani"; ctx.fillText(this.text, this.x, this.y); ctx.restore();
    }
}

class Particle {
    constructor(x, y, color, vx, vy, size, friction = 0.98) {
        this.x = x; this.y = y; this.color = color; this.vx = vx; this.vy = vy; this.size = size; this.friction = friction; this.alpha = 1;
    }
    update(dt) {
        const f = Math.pow(this.friction, dt / 16); 
        this.vx *= f; this.vy *= f; this.x += (this.vx * dt) / 16; this.y += (this.vy * dt) / 16; this.alpha -= (0.02 * dt) / 16;
    }
    draw(ctx) {
        ctx.save(); ctx.globalAlpha = Math.max(0, this.alpha); ctx.fillStyle = this.color;
        ctx.beginPath(); ctx.arc(this.x, this.y, this.size, 0, Math.PI * 2); ctx.fill(); ctx.restore();
    }
}

class Coin {
    constructor(x, y) { this.x = x; this.y = y; this.radius = 8; this.life = 10000; }
    update(dt) {
        this.life -= dt;
        let closest = null; let minDist = Infinity;
        Game.players.forEach(p => {
            if(p.hp <= 0) return;
            let d = Vector.dist(this.x, this.y, p.x, p.y);
            if(d < minDist) { minDist = d; closest = p; }
        });
        if(closest && minDist < 150) {
            const angle = Vector.angle(this.x, this.y, closest.x, closest.y);
            this.x += (Math.cos(angle) * 400 * dt) / 1000; this.y += (Math.sin(angle) * 400 * dt) / 1000;
        } 
        if (closest && minDist < closest.radius + this.radius) {
            closest.coins++;
            rumble(closest.padIndex, 50, 0.2, 0);
            this.life = 0;
            closest.updateHUD();
        }
    }
    draw(ctx) {
        ctx.save(); if(this.life < 2000 && Math.floor(Date.now() / 100) % 2 === 0) ctx.globalAlpha = 0.3;
        ctx.fillStyle = '#ffd700'; ctx.shadowColor = '#ffd700'; ctx.shadowBlur = 10;
        ctx.beginPath(); ctx.moveTo(this.x, this.y - this.radius); ctx.lineTo(this.x + this.radius, this.y); ctx.lineTo(this.x, this.y + this.radius); ctx.lineTo(this.x - this.radius, this.y); ctx.fill(); ctx.restore();
    }
}

class Wall {
    constructor(x, y, w, h) { this.x = x; this.y = y; this.w = w; this.h = h; this.hp = 40; this.maxHp = 40; }
    draw(ctx) {
        ctx.fillStyle = `rgba(0, 55, 145, ${Math.max(0.2, this.hp/this.maxHp)})`; ctx.fillRect(this.x, this.y, this.w, this.h);
        ctx.strokeStyle = '#fff'; ctx.strokeRect(this.x, this.y, this.w, this.h);
    }
}

function resolveWallCollision(entity) {
    for(let wall of Game.walls) {
        let testX = entity.x; let testY = entity.y;
        if(entity.x < wall.x) testX = wall.x; else if (entity.x > wall.x + wall.w) testX = wall.x + wall.w;
        if(entity.y < wall.y) testY = wall.y; else if (entity.y > wall.y + wall.h) testY = wall.y + wall.h;
        let distX = entity.x - testX; let distY = entity.y - testY; let dist = Math.hypot(distX, distY);
        if(dist < entity.radius) {
            if(dist === 0) { distX = 1; distY = 0; dist = 1; }
            entity.x = testX + (distX/dist)*entity.radius; entity.y = testY + (distY/dist)*entity.radius;
        }
    }
}

class Player {
    constructor(padIndex) {
        this.padIndex = padIndex;
        this.color = PlayerColors[padIndex % 4];
        this.x = canvas.width / 2 + (padIndex * 40 - 20);
        this.y = canvas.height / 2;
        this.camera = { x: this.x, y: this.y };
        this.radius = 18;
        this.hp = 100; this.maxHp = 100;
        this.coins = 0;
        this.score = 0;
        this.lastShot = 0;
        this.deadzone = 0.2;
        this.sensMove = 1.0;
        
        // Settings and Stats
        this.keys = { pause: 9, skill: 7, upg1: 4, upg2: 5, upg3: 3 }; // OPTIONS, R2, L1, R1, TRIANGLE
        this.stats = {
            fireRate: 160, damageMulti: 1, skillDuration: 3000, skillCooldownMax: 12000, skillCooldown: 0,
            costFire: 15, costMulti: 40, costSkill: 25
        };
        this.timeStopActive = false;
        this.timeStopTimer = 0;
    }
    
    update(dt) {
        if(this.hp <= 0) return;
        
        // Habilidade
        if(this.timeStopActive) {
            this.timeStopTimer -= dt;
            if(this.timeStopTimer <= 0) {
                this.timeStopActive = false;
                createFloatingText(this.x, this.y - 30, "TEMPO NORMALIZADO", "#aaa");
            }
        } else {
            if(this.stats.skillCooldown > 0) this.stats.skillCooldown -= dt;
        }
        
        const pad = navigator.getGamepads()[this.padIndex];
        if(pad) {
            const axesMag = Math.hypot(pad.axes[0], pad.axes[1]);
            if(axesMag > this.deadzone) {
                this.x += pad.axes[0] * (300 * this.sensMove) * dt / 1000;
                this.y += pad.axes[1] * (300 * this.sensMove) * dt / 1000;
            }
        }
        
        resolveWallCollision(this);
        if(this.x < Game.map.minX + this.radius) this.x = Game.map.minX + this.radius;
        if(this.x > Game.map.maxX - this.radius) this.x = Game.map.maxX - this.radius;
        if(this.y < Game.map.minY + this.radius) this.y = Game.map.minY + this.radius;
        if(this.y > Game.map.maxY - this.radius) this.y = Game.map.maxY - this.radius;
        
        if(Date.now() - this.lastShot > this.stats.fireRate && (Game.enemies.length > 0 || Game.boss)) {
            this.shoot();
        }
        
        // Camera follow
        this.camera.x += (this.x - this.camera.x) * 5 * (dt/1000);
        this.camera.y += (this.y - this.camera.y) * 5 * (dt/1000);
    }
    
    shoot() {
        this.lastShot = Date.now();
        let target = null; let minDist = Infinity;
        Game.enemies.forEach(e => {
            let d = Vector.dist(this.x, this.y, e.x, e.y);
            if(d < minDist) { minDist = d; target = {x: e.x, y: e.y}; }
        });
        if(Game.boss) {
            if(Game.boss.phase === 1) {
                Game.boss.weapons.forEach(w => {
                    if(w.active) {
                        let wx = Game.boss.x + w.relX; let wy = Game.boss.y + w.relY;
                        let d = Vector.dist(this.x, this.y, wx, wy);
                        if(d < minDist) { minDist = d; target = {x: wx, y: wy}; }
                    }
                });
            } else {
                let d = Vector.dist(this.x, this.y, Game.boss.x, Game.boss.y);
                if(d < minDist) { target = {x: Game.boss.x, y: Game.boss.y}; }
            }
        }
        if(target) {
            const baseAngle = Vector.angle(this.x, this.y, target.x, target.y);
            Game.projectiles.push({
                x: this.x, y: this.y, owner: this,
                vx: Math.cos(baseAngle) * 800, vy: Math.sin(baseAngle) * 800,
                radius: 5, color: this.color, damage: 20 * this.stats.damageMulti
            });
        }
    }
    
    draw(ctx) {
        if(this.hp <= 0) return;
        ctx.fillStyle = this.color; ctx.shadowColor = this.color; ctx.shadowBlur = 15;
        ctx.beginPath(); ctx.arc(this.x, this.y, this.radius, 0, Math.PI*2); ctx.fill(); ctx.shadowBlur = 0;
        ctx.strokeStyle = '#fff'; ctx.lineWidth = 2;
        ctx.beginPath(); ctx.arc(this.x, this.y, this.radius - 6, 0, Math.PI*2); ctx.stroke();
    }
    
    takeDamage(amount) {
        if(this.hp <= 0) return;
        this.hp -= amount;
        rumble(this.padIndex, 200, 0.8, 1.0);
        Game.cameraShake = 15;
        for(let i=0; i<10; i++) Game.particles.push(new Particle(this.x, this.y, '#ff003c', (Math.random()-0.5)*10, (Math.random()-0.5)*10, 3));
        this.updateHUD();
        if(this.hp <= 0) {
            createFloatingText(this.x, this.y, "SISTEMA DESLIGADO", this.color);
            checkGameOver();
        }
    }
    
    activateTimeStop() {
        this.timeStopActive = true;
        this.timeStopTimer = this.stats.skillDuration;
        this.stats.skillCooldown = this.stats.skillCooldownMax;
        createFloatingText(this.x, this.y - 30, "TEMPO PARADO!", this.color);
        for(let i=0; i<30; i++) Game.particles.push(new Particle(this.x, this.y, this.color, (Math.random()-0.5)*20, (Math.random()-0.5)*20, Math.random()*4+2, 0.92));
        this.updateHUD();
    }
    
    updateHUD() {
        const hud = document.getElementById(this.hudId);
        if(!hud) return;
        hud.querySelector('.score').innerText = this.score;
        hud.querySelector('.coins').innerText = this.coins;
        hud.querySelector('.hp-fill').style.width = Math.max(0, (this.hp/this.maxHp)*100) + '%';
        
        const cdFill = hud.querySelector('.cd-fill');
        if(this.timeStopActive) {
            cdFill.style.width = Math.max(0, (this.timeStopTimer/this.stats.skillDuration)*100) + '%';
            cdFill.style.background = '#ff003c';
        } else {
            cdFill.style.width = (100 - Math.min(100, (this.stats.skillCooldown/this.stats.skillCooldownMax)*100)) + '%';
            cdFill.style.background = this.color;
        }
        
        hud.querySelector('.upg1-cost').innerText = this.stats.costFire + ' C';
        hud.querySelector('.upg2-cost').innerText = this.stats.costMulti + ' C';
        hud.querySelector('.upg3-cost').innerText = this.stats.costSkill + ' C';
    }
    
    buyUpgrade(type) {
        if(type === 1 && this.coins >= this.stats.costFire) {
            this.coins -= this.stats.costFire; this.stats.fireRate = Math.max(40, this.stats.fireRate - 15);
            this.stats.costFire = Math.floor(this.stats.costFire * 1.5);
            createFloatingText(this.x, this.y, "TIRO MAIS RÁPIDO!", "#ffd700");
        } else if(type === 2 && this.coins >= this.stats.costMulti) {
            this.coins -= this.stats.costMulti; this.stats.damageMulti += 0.5;
            this.stats.costMulti = Math.floor(this.stats.costMulti * 2.2);
            createFloatingText(this.x, this.y, "+50% DANO!", "#ffd700");
        } else if(type === 3 && this.coins >= this.stats.costSkill) {
            this.coins -= this.stats.costSkill; this.stats.skillDuration += 500;
            this.stats.skillCooldownMax = Math.max(5000, this.stats.skillCooldownMax - 1000);
            this.stats.costSkill = Math.floor(this.stats.costSkill * 1.8);
            createFloatingText(this.x, this.y, "TEMPO APRIMORADO!", "#ffd700");
        }
        this.updateHUD();
    }
}

class Enemy {
    constructor(x, y, hp, speed) {
        this.x = x; this.y = y; this.hp = hp; this.maxHp = hp; this.speed = speed; 
        this.color = Game.enemyTheme.color; this.edges = Game.enemyTheme.edges; this.radius = 16;
    }
    update(dt) {
        if(Game.players.some(p => p.timeStopActive)) return; 
        
        let target = null; let minDist = Infinity;
        Game.players.forEach(p => {
            if(p.hp <= 0) return;
            let d = Vector.dist(this.x, this.y, p.x, p.y);
            if(d < minDist) { minDist = d; target = p; }
        });
        
        if(target) {
            const angle = Vector.angle(this.x, this.y, target.x, target.y);
            this.x += (Math.cos(angle) * this.speed * dt) / 1000;
            this.y += (Math.sin(angle) * this.speed * dt) / 1000;
        }
        resolveWallCollision(this);
    }
    draw(ctx) {
        ctx.fillStyle = this.color; ctx.beginPath();
        if(this.edges === 0) {
            ctx.rect(this.x - this.radius, this.y - this.radius, this.radius*2, this.radius*2);
        } else {
            for(let i=0; i<this.edges; i++) {
                const ang = (i * Math.PI * 2) / this.edges - Math.PI/2;
                if(i === 0) ctx.moveTo(this.x + Math.cos(ang)*this.radius, this.y + Math.sin(ang)*this.radius);
                else ctx.lineTo(this.x + Math.cos(ang)*this.radius, this.y + Math.sin(ang)*this.radius);
            }
            ctx.closePath();
        }
        ctx.fill();
        if(this.hp < this.maxHp) {
            ctx.fillStyle = 'rgba(255,0,0,0.5)'; ctx.fillRect(this.x - 15, this.y - 25, 30, 4);
            ctx.fillStyle = '#00ff00'; ctx.fillRect(this.x - 15, this.y - 25, 30 * (this.hp/this.maxHp), 4);
        }
    }
}

class Boss {
    constructor() {
        this.x = canvas.width / 2; this.y = -100; this.radius = 50; this.phase = 1; 
        const diff = 1 + (Game.wave / 10) * 0.5;
        this.hp = 3000 * diff; this.maxHp = 3000 * diff; this.phase3Hp = 3000 * diff; this.maxPhase3Hp = 3000 * diff;
        
        this.shape = BossShapes[Math.floor(Math.random() * BossShapes.length)];
        const bossColors = { 'triangle': '#00ff00', 'square': '#ff00ff', 'circle': '#ff003c', 'cross': '#003791' };
        this.trueColor = bossColors[this.shape];
        this.color = '#333';
        
        this.weapons = [
            { id: 1, relX: 0, relY: -90, hp: 500*diff, maxHp: 500*diff, active: true },
            { id: 2, relX: 78, relY: 45, hp: 500*diff, maxHp: 500*diff, active: true },
            { id: 3, relX: -78, relY: 45, hp: 500*diff, maxHp: 500*diff, active: true }
        ];
        this.angle = 0; this.attackTimer = 0;
        
        UI.bossHud.classList.remove('hidden');
        this.updateUI();
    }
    
    update(dt) {
        if(this.y < 150) this.y += (100 * dt) / 1000;
        if(Game.players.some(p => p.timeStopActive)) return;
        
        this.angle += (1.5 * dt) / 1000; this.attackTimer += dt;
        
        let target = null; let minDist = Infinity;
        Game.players.forEach(p => {
            if(p.hp <= 0) return;
            let d = Vector.dist(this.x, this.y, p.x, p.y);
            if(d < minDist) { minDist = d; target = p; }
        });
        
        if(this.phase === 1) {
            if(this.attackTimer > 1500) {
                this.attackTimer = 0;
                this.weapons.forEach(w => {
                    if(w.active && target) {
                        let wx = this.x + Math.cos(this.angle)*w.relX - Math.sin(this.angle)*w.relY;
                        let wy = this.y + Math.sin(this.angle)*w.relX + Math.cos(this.angle)*w.relY;
                        const ang = Vector.angle(wx, wy, target.x, target.y);
                        Game.enemyProjectiles.push({x: wx, y: wy, vx: Math.cos(ang)*300, vy: Math.sin(ang)*300, radius: 6, color: '#ff5e00'});
                    }
                });
            }
            if(this.weapons.every(w => !w.active)) this.setPhase(2);
        } else if(this.phase === 2) {
            this.x += Math.cos(this.angle) * 1.5;
            if(this.attackTimer > 300) {
                this.attackTimer = 0;
                for(let i=0; i<4; i++) {
                    const ang = this.angle * 2 + (Math.PI/2)*i;
                    Game.enemyProjectiles.push({x: this.x, y: this.y, vx: Math.cos(ang)*250, vy: Math.sin(ang)*250, radius: 8, color: this.trueColor});
                }
            }
        } else if(this.phase === 3) {
            if(target) {
                const angToPlayer = Vector.angle(this.x, this.y, target.x, target.y);
                this.x += (Math.cos(angToPlayer) * 120 * dt) / 1000; this.y += (Math.sin(angToPlayer) * 120 * dt) / 1000;
                if(this.attackTimer > 800) {
                    this.attackTimer = 0;
                    for(let i=0; i<3; i++) {
                        const spread = (i-1)*0.3;
                        Game.enemyProjectiles.push({x: this.x, y: this.y, vx: Math.cos(angToPlayer+spread)*450, vy: Math.sin(angToPlayer+spread)*450, radius: 10, color: '#ff003c'});
                    }
                }
            }
        }
    }
    
    setPhase(p) {
        this.phase = p; Game.cameraShake = 30;
        if(p === 2) {
            this.color = this.trueColor; UI.bossPhase.innerText = "FASE 2: NÚCLEO EXPOSTO"; UI.bossPhase.style.color = this.color;
            createFloatingText(this.x, this.y, "SISTEMA COMPROMETIDO!", this.color);
        } else if (p === 3) {
            this.color = '#ff003c'; UI.bossPhase.innerText = "FASE 3: PROTOCOLO DE FÚRIA"; UI.bossPhase.style.color = '#ff003c';
            createFloatingText(this.x, this.y, "MODO FÚRIA!", "#ff003c");
        }
        for(let i=0; i<50; i++) Game.particles.push(new Particle(this.x, this.y, this.color, (Math.random()-0.5)*30, (Math.random()-0.5)*30, Math.random()*5+2));
        this.updateUI();
    }
    
    takeDamage(dmg, weaponId = null) {
        if(this.phase === 1 && weaponId !== null) {
            const w = this.weapons.find(x => x.id === weaponId);
            w.hp -= dmg;
            if(w.hp <= 0 && w.active) { w.active = false; Game.cameraShake = 10; addScore(500); }
        } else if (this.phase === 2) {
            this.hp -= dmg; if(this.hp <= 0) this.setPhase(3);
        } else if (this.phase === 3) {
            this.phase3Hp -= dmg; if(this.phase3Hp <= 0) this.destroy();
        }
        this.updateUI();
    }
    
    destroy() {
        Game.cameraShake = 50; Game.players.forEach(p => rumble(p.padIndex, 1000, 1.0, 1.0));
        for(let i=0; i<100; i++) Game.particles.push(new Particle(this.x, this.y, '#ff5e00', (Math.random()-0.5)*40, (Math.random()-0.5)*40, Math.random()*8+2));
        addScore(5000); Game.boss = null; Game.enemyProjectiles = []; UI.bossHud.classList.add('hidden'); Game.bossDefeated = true;
        
        let edges = 0;
        if(this.shape === 'triangle') edges = 3; else if(this.shape === 'square') edges = 4;
        Game.enemyTheme = { edges: edges, color: this.trueColor };
        
        Game.map.minX -= 300; Game.map.maxX += 300; Game.map.minY -= 300; Game.map.maxY += 300;
        for(let w=0; w<4; w++) {
            let wx, wy, ww = 80, wh = 80;
            if(w===0) { wx = Game.map.minX + 100; wy = (Game.map.minY + Game.map.maxY)/2 - wh/2; }
            if(w===1) { wx = Game.map.maxX - 100 - ww; wy = (Game.map.minY + Game.map.maxY)/2 - wh/2; }
            if(w===2) { wx = (Game.map.minX + Game.map.maxX)/2 - ww/2; wy = Game.map.minY + 100; }
            if(w===3) { wx = (Game.map.minX + Game.map.maxX)/2 - ww/2; wy = Game.map.maxY - 100 - wh; }
            Game.walls.push(new Wall(wx, wy, ww, wh));
        }
        
        Game.players.forEach(p => createFloatingText(p.x, p.y - 50, "MAPA EXPANDIDO!", "#ffd700"));
        saveGame();
        setTimeout(() => { Game.bossDefeated = false; nextWave(); }, 3000);
    }
    
    updateUI() {
        if(this.phase === 1) {
            const totalWp = this.weapons.reduce((acc, w) => acc + w.hp, 0); const maxWp = this.weapons.reduce((acc, w) => acc + w.maxHp, 0);
            UI.bossHpFill.style.width = Math.max(0, (totalWp/maxWp)*100) + '%';
        } else if (this.phase === 2) { UI.bossHpFill.style.width = Math.max(0, (this.hp/this.maxHp)*100) + '%';
        } else if (this.phase === 3) { UI.bossHpFill.style.width = Math.max(0, (this.phase3Hp/this.maxPhase3Hp)*100) + '%'; }
    }
    
    draw(ctx) {
        ctx.save(); ctx.translate(this.x, this.y); ctx.rotate(this.angle);
        ctx.fillStyle = this.color; ctx.shadowColor = this.color; ctx.shadowBlur = 20; ctx.beginPath();
        
        if (this.phase === 1) {
            ctx.arc(0, 0, this.radius, 0, Math.PI*2);
        } else {
            if(this.shape === 'triangle') {
                for(let i=0; i<3; i++) { const ang = (i*Math.PI*2)/3 - Math.PI/2; if(i===0) ctx.moveTo(Math.cos(ang)*this.radius, Math.sin(ang)*this.radius); else ctx.lineTo(Math.cos(ang)*this.radius, Math.sin(ang)*this.radius); }
            } else if (this.shape === 'square') {
                ctx.rect(-this.radius, -this.radius, this.radius*2, this.radius*2);
            } else if (this.shape === 'circle') {
                ctx.arc(0, 0, this.radius, 0, Math.PI*2);
            } else if (this.shape === 'cross') {
                const w = this.radius * 0.4; const r = this.radius;
                ctx.moveTo(-w, -r); ctx.lineTo(w, -r); ctx.lineTo(w, -w); ctx.lineTo(r, -w); ctx.lineTo(r, w); ctx.lineTo(w, w); ctx.lineTo(w, r); ctx.lineTo(-w, r); ctx.lineTo(-w, w); ctx.lineTo(-r, w); ctx.lineTo(-r, -w); ctx.lineTo(-w, -w);
            }
        }
        ctx.closePath(); ctx.fill(); ctx.shadowBlur = 0;
        
        if(this.phase === 1) {
            this.weapons.forEach(w => {
                if(w.active) {
                    ctx.fillStyle = '#444'; ctx.strokeStyle = '#ff5e00'; ctx.lineWidth = 3; ctx.beginPath(); ctx.arc(w.relX, w.relY, 20, 0, Math.PI*2); ctx.fill(); ctx.stroke();
                    ctx.fillStyle = 'red'; ctx.fillRect(w.relX - 15, w.relY - 30, 30, 4);
                    ctx.fillStyle = 'green'; ctx.fillRect(w.relX - 15, w.relY - 30, 30*(w.hp/w.maxHp), 4);
                }
            });
        }
        ctx.restore();
    }
}

// Data and Save
function saveGame() {
    UI.saveNotification.classList.remove('hidden');
    const saveData = {
        players: Game.players.map(p => ({ padIndex: p.padIndex, stats: p.stats, keys: p.keys, sensMove: p.sensMove, score: p.score, coins: p.coins }))
    };
    localStorage.setItem('boycetaPS4', JSON.stringify(saveData));
    setTimeout(() => { UI.saveNotification.classList.add('hidden'); }, 1500);
}

function loadGame() {
    const data = localStorage.getItem('boycetaPS4');
    if(data) {
        try {
            const parsed = JSON.parse(data);
            parsed.players.forEach(savedP => {
                let p = Game.players.find(pl => pl.padIndex === savedP.padIndex);
                if(p) { p.stats = savedP.stats; p.keys = savedP.keys; p.sensMove = savedP.sensMove; p.score = savedP.score; p.coins = savedP.coins; }
            });
        } catch(e) {}
    }
}

// Game Flow
function initLobby(isSolo) {
    Game.isSolo = isSolo;
    changeState(STATE.LOBBY);
    lobbyCountdown = -1;
}

function startGame() {
    Game.wave = 1; Game.timeSinceLastWave = 0; Game.cameraShake = 0;
    Game.enemies = []; Game.projectiles = []; Game.enemyProjectiles = []; Game.particles = []; Game.drops = []; Game.floatingTexts = [];
    Game.boss = null; Game.bossDefeated = false; Game.enemyTheme = { edges: 0, color: '#ff003c' };
    Game.map = { minX: 0, minY: 0, maxX: canvas.width, maxY: canvas.height }; Game.walls = [];
    
    // Build HUDs dynamically
    UI.hudLayer.innerHTML = '';
    const numPlayers = Game.players.length;
    let gridStyle = "grid-template-columns: 1fr;";
    if(numPlayers === 2) gridStyle = "grid-template-columns: 1fr 1fr;";
    else if(numPlayers > 2) gridStyle = "grid-template-columns: 1fr 1fr; grid-template-rows: 1fr 1fr;";
    UI.hudLayer.style = `position: absolute; top: 0; left: 0; width: 100vw; height: 100vh; pointer-events: none; display: grid; ${gridStyle}`;
    
    Game.players.forEach((p, i) => {
        p.hp = p.maxHp; p.timeStopActive = false; p.hudId = `hud-p${p.padIndex}`;
        const hudHTML = `<div id="${p.hudId}" class="player-hud-container" style="border-color: ${p.color}">
            <div class="hud-wrapper">
                <div class="hud-left player-status-mini">
                    <div style="color: ${p.color}; font-weight: bold; font-size: 1.2rem;">JOGADOR ${i+1}</div>
                    <div style="font-size: 1.2rem;">SCORE: <span class="score text-cyan">0</span> | C: <span class="coins text-gold">0</span></div>
                    <div style="height: 10px; background: #333; width: 100%; border-radius: 5px; overflow: hidden;"><div class="hp-fill" style="width: 100%; height: 100%; background: ${p.color};"></div></div>
                    <div style="height: 10px; background: #333; width: 100%; border-radius: 5px; overflow: hidden;"><div class="cd-fill" style="width: 100%; height: 100%; background: ${p.color};"></div></div>
                </div>
                <div class="hud-right player-status-mini">
                    <div style="font-size: 0.9rem; color: #aaa;">LOJA DE UPGRADES</div>
                    <div>[L1] Tiro <span class="upg1-cost text-gold"></span></div>
                    <div>[R1] Dano <span class="upg2-cost text-gold"></span></div>
                    <div>[△] Hab. <span class="upg3-cost text-gold"></span></div>
                </div>
            </div>
        </div>`;
        UI.hudLayer.insertAdjacentHTML('beforeend', hudHTML);
        p.updateHUD();
    });
    
    changeState(STATE.PLAYING);
    startWaveAnnouncement();
}

function updateHUD() {
    Game.players.forEach(p => p.updateHUD());
}

function addScore(pts) { Game.players.forEach(p => p.score += pts); updateHUD(); }

function startWaveAnnouncement() {
    UI.announceText.innerText = (Game.wave % 5 === 0) ? "BOSS APROXIMANDO" : `ONDA ${Game.wave}`;
    UI.announceText.className = (Game.wave % 5 === 0) ? 'text-red' : 'text-cyan';
    UI.announce.classList.remove('hidden');
    UI.announceText.style.animation = 'none'; void UI.announceText.offsetWidth; UI.announceText.style.animation = 'pulse 2.5s forwards';
    setTimeout(() => { UI.announce.classList.add('hidden'); }, 2500);
}

function nextWave() {
    Game.wave++; Game.timeSinceLastWave = 0;
    if(Game.wave % 5 === 0) Game.boss = new Boss();
    startWaveAnnouncement();
}

function spawnEnemies(dt) {
    if(Game.boss || Game.bossDefeated || Game.players.some(p => p.timeStopActive)) return;
    Game.timeSinceLastWave += dt;
    if(Game.timeSinceLastWave > Game.waveDuration) {
        if(Game.enemies.length === 0 && !Game.boss) nextWave();
        return;
    }
    const spawnChance = (0.02 + (Game.wave * 0.005)) * Game.players.length; 
    if(Math.random() < spawnChance && Game.enemies.length < (20 + Game.wave*2) * Game.players.length) {
        const side = Math.floor(Math.random()*4); let x, y;
        if(side===0) { x = Game.map.minX + Math.random()*(Game.map.maxX - Game.map.minX); y = Game.map.minY - 30; }
        else if(side===1) { x = Game.map.maxX + 30; y = Game.map.minY + Math.random()*(Game.map.maxY - Game.map.minY); }
        else if(side===2) { x = Game.map.minX + Math.random()*(Game.map.maxX - Game.map.minX); y = Game.map.maxY + 30; }
        else { x = Game.map.minX - 30; y = Game.map.minY + Math.random()*(Game.map.maxY - Game.map.minY); }
        const hp = (40 + (Game.wave * 15)) * (1 + (Game.players.length-1)*0.5);
        const speed = 100 + (Math.random()*50) + (Game.wave*5);
        Game.enemies.push(new Enemy(x, y, hp, speed));
    }
}

function checkGameOver() {
    if(Game.players.every(p => p.hp <= 0)) {
        UI.finalWave.innerText = Game.wave;
        UI.finalScore.innerText = Game.players.reduce((sum, p) => sum + p.score, 0);
        changeState(STATE.GAMEOVER);
        saveGame();
    }
}

// Menu and States
function changeState(newState) {
    currentState = newState;
    menuIndex = 0;
    bindingAction = null;
    UI.mainMenu.classList.add('hidden');
    UI.lobbyScreen.classList.add('hidden');
    UI.pauseScreen.classList.add('hidden');
    UI.controlsScreen.classList.add('hidden');
    UI.gameOver.classList.add('hidden');
    UI.globalHud.classList.add('hidden');
    UI.hudLayer.classList.add('hidden');
    
    if(newState === STATE.MENU) UI.mainMenu.classList.remove('hidden');
    else if(newState === STATE.LOBBY) UI.lobbyScreen.classList.remove('hidden');
    else if(newState === STATE.PLAYING) { UI.globalHud.classList.remove('hidden'); UI.hudLayer.classList.remove('hidden'); Game.lastTime = performance.now(); }
    else if(newState === STATE.PAUSED) { UI.pauseScreen.classList.remove('hidden'); document.getElementById('pause-who').innerText = `Pausado pelo Jogador ${pauseMenuCaller+1}`; document.getElementById('pause-pnum').innerText = pauseMenuCaller+1; }
    else if(newState === STATE.GAMEOVER) UI.gameOver.classList.remove('hidden');
    else if(newState === STATE.CONTROLS) {
        UI.controlsScreen.classList.remove('hidden');
        document.getElementById('cfg-pnum').innerText = pauseMenuCaller+1;
        updateControlsUI();
    }
    updateMenuSelection();
}

function updateMenuSelection() {
    const btns = UI.menuBtns[currentState];
    if(!btns) return;
    btns.forEach((b, i) => {
        if(i === menuIndex) b.classList.add('selected'); else b.classList.remove('selected');
    });
}

function executeMenuAction() {
    const btns = UI.menuBtns[currentState];
    if(!btns || !btns[menuIndex]) return;
    const action = btns[menuIndex].dataset.action;
    const bind = btns[menuIndex].dataset.bind;
    
    if(bind) {
        bindingAction = bind;
        btns.forEach(b => b.classList.remove('listening'));
        btns[menuIndex].classList.add('listening');
        UI.bindingMsg.classList.remove('hidden');
        return;
    }
    
    if(currentState === STATE.MENU) {
        if(action === 'solo') initLobby(true);
        else if(action === 'local') initLobby(false);
        else if(action === 'controls') { pauseMenuCaller = 0; changeState(STATE.CONTROLS); }
    } else if(currentState === STATE.PAUSED) {
        if(action === 'resume') changeState(STATE.PLAYING);
        else if(action === 'controls') changeState(STATE.CONTROLS);
        else if(action === 'quit') changeState(STATE.MENU);
    } else if(currentState === STATE.CONTROLS) {
        if(action === 'back') changeState(Game.active ? STATE.PAUSED : STATE.MENU);
    } else if(currentState === STATE.GAMEOVER) {
        if(action === 'quit') changeState(STATE.MENU);
    }
}

function updateControlsUI() {
    let p = Game.players.find(pl => pl.padIndex === pauseMenuCaller);
    if(!p) {
        p = new Player(pauseMenuCaller); // Temp for global menu
    }
    const btns = UI.menuBtns.CONTROLS;
    btns.forEach(b => {
        if(b.dataset.bind) b.innerText = "BOTÃO " + p.keys[b.dataset.bind];
    });
    UI.cfgSens.value = p.sensMove;
}

// Gamepad Polling
let lastGamepadState = {};
function checkGamepad() {
    const pads = navigator.getGamepads();
    let numConnected = 0;
    
    // Update Lobby
    if(currentState === STATE.LOBBY) {
        let lobbyHTML = '';
        Game.players = [];
        for(let i=0; i<4; i++) {
            if(pads[i]) {
                numConnected++;
                Game.players.push(new Player(i));
                lobbyHTML += `<div style="padding: 15px; border: 2px solid ${PlayerColors[i]}; border-radius: 8px; text-align: center; background: rgba(0,0,0,0.5);">
                                <h3 style="color: ${PlayerColors[i]}">JOGADOR ${i+1}</h3>
                                <p>Pronto</p>
                              </div>`;
            }
        }
        UI.lobbyPlayers.innerHTML = lobbyHTML;
        
        if((Game.isSolo && numConnected >= 1) || (!Game.isSolo && numConnected >= 2)) {
            if(lobbyCountdown < 0) lobbyCountdown = 3000;
            lobbyCountdown -= 16;
            UI.lobbyStatus.innerText = `Iniciando em ${Math.ceil(lobbyCountdown/1000)}...`;
            UI.lobbyStatus.className = "text-cyan";
            if(lobbyCountdown <= 0) startGame();
        } else {
            lobbyCountdown = -1;
            UI.lobbyStatus.innerText = Game.isSolo ? "Conecte 1 controle" : "Conecte no mínimo 2 controles";
            UI.lobbyStatus.className = "text-red";
        }
    }
    
    // Poll Inputs
    for(let i=0; i<4; i++) {
        const pad = pads[i];
        if(!pad) continue;
        const stateKey = `pad_${i}`;
        if(!lastGamepadState[stateKey]) lastGamepadState[stateKey] = { buttons: [] };
        
        // Check binding
        if(bindingAction && i === pauseMenuCaller) {
            for(let b=0; b<pad.buttons.length; b++) {
                if(pad.buttons[b].pressed && !lastGamepadState[stateKey].buttons[b]) {
                    let p = Game.players.find(pl => pl.padIndex === i);
                    if(!p) p = new Player(i); // Fallback for global menu
                    p.keys[bindingAction] = b;
                    bindingAction = null;
                    UI.bindingMsg.classList.add('hidden');
                    updateControlsUI();
                    break;
                }
            }
        }
        
        // Navigation D-Pad (Up=12, Down=13, Cross=0, Circle=1)
        const UP = pad.buttons[12] && pad.buttons[12].pressed;
        const DOWN = pad.buttons[13] && pad.buttons[13].pressed;
        const LEFT = pad.buttons[14] && pad.buttons[14].pressed;
        const RIGHT = pad.buttons[15] && pad.buttons[15].pressed;
        const CROSS = pad.buttons[0] && pad.buttons[0].pressed;
        const CIRCLE = pad.buttons[1] && pad.buttons[1].pressed;
        
        const wasUP = lastGamepadState[stateKey].buttons[12];
        const wasDOWN = lastGamepadState[stateKey].buttons[13];
        const wasLEFT = lastGamepadState[stateKey].buttons[14];
        const wasRIGHT = lastGamepadState[stateKey].buttons[15];
        const wasCROSS = lastGamepadState[stateKey].buttons[0];
        const wasCIRCLE = lastGamepadState[stateKey].buttons[1];
        
        if(!bindingAction) {
            if([STATE.MENU, STATE.PAUSED, STATE.GAMEOVER, STATE.CONTROLS].includes(currentState)) {
                // Let any pad navigate Menu/GameOver, but only caller navigate Pause/Controls
                if(currentState === STATE.MENU || currentState === STATE.GAMEOVER || i === pauseMenuCaller) {
                    const btns = UI.menuBtns[currentState];
                    if(UP && !wasUP) { menuIndex = (menuIndex - 1 + btns.length) % btns.length; updateMenuSelection(); }
                    if(DOWN && !wasDOWN) { menuIndex = (menuIndex + 1) % btns.length; updateMenuSelection(); }
                    if(CROSS && !wasCROSS) executeMenuAction();
                    if(CIRCLE && !wasCIRCLE && currentState === STATE.CONTROLS) changeState(Game.active ? STATE.PAUSED : STATE.MENU);
                    
                    if(currentState === STATE.CONTROLS && btns[menuIndex] && btns[menuIndex].classList.contains('cfg-sens')) {
                        let p = Game.players.find(pl => pl.padIndex === pauseMenuCaller);
                        if(p) {
                            if(LEFT && !wasLEFT) { p.sensMove = Math.max(0.5, p.sensMove - 0.1); UI.cfgSens.value = p.sensMove; }
                            if(RIGHT && !wasRIGHT) { p.sensMove = Math.min(3.0, p.sensMove + 0.1); UI.cfgSens.value = p.sensMove; }
                        }
                    }
                }
            } else if(currentState === STATE.LOBBY) {
                if(CIRCLE && !wasCIRCLE) changeState(STATE.MENU);
            }
        }
        
        // In Game actions
        let p = Game.players.find(pl => pl.padIndex === i);
        if(p && currentState === STATE.PLAYING) {
            const PAUSE_BTN = pad.buttons[p.keys.pause] && pad.buttons[p.keys.pause].pressed;
            const SKILL_BTN = pad.buttons[p.keys.skill] && pad.buttons[p.keys.skill].pressed;
            const UPG1 = pad.buttons[p.keys.upg1] && pad.buttons[p.keys.upg1].pressed;
            const UPG2 = pad.buttons[p.keys.upg2] && pad.buttons[p.keys.upg2].pressed;
            const UPG3 = pad.buttons[p.keys.upg3] && pad.buttons[p.keys.upg3].pressed;
            
            if(PAUSE_BTN && !lastGamepadState[stateKey].buttons[p.keys.pause]) {
                pauseMenuCaller = i; changeState(STATE.PAUSED);
            }
            if(SKILL_BTN && !lastGamepadState[stateKey].buttons[p.keys.skill] && p.stats.skillCooldown <= 0 && !p.timeStopActive) {
                p.activateTimeStop();
            }
            if(UPG1 && !lastGamepadState[stateKey].buttons[p.keys.upg1]) p.buyUpgrade(1);
            if(UPG2 && !lastGamepadState[stateKey].buttons[p.keys.upg2]) p.buyUpgrade(2);
            if(UPG3 && !lastGamepadState[stateKey].buttons[p.keys.upg3]) p.buyUpgrade(3);
        }
        
        // Update last state
        for(let b=0; b<pad.buttons.length; b++) lastGamepadState[stateKey].buttons[b] = pad.buttons[b].pressed;
    }
}

// Render Engine
function drawViewport(ctx, vx, vy, vw, vh, p) {
    ctx.save();
    ctx.beginPath(); ctx.rect(vx, vy, vw, vh); ctx.clip();
    
    // BG Effect Time Stop
    if(Game.players.some(pl => pl.timeStopActive)) {
        ctx.fillStyle = 'rgba(0, 55, 145, 0.08)'; ctx.fillRect(vx, vy, vw, vh);
        ctx.strokeStyle = 'rgba(0, 55, 145, 0.15)'; ctx.lineWidth = 1;
        for(let x=vx; x<vx+vw; x+=50) { ctx.beginPath(); ctx.moveTo(x,vy); ctx.lineTo(x,vy+vh); ctx.stroke(); }
        for(let y=vy; y<vy+vh; y+=50) { ctx.beginPath(); ctx.moveTo(vx,y); ctx.lineTo(vx+vw,y); ctx.stroke(); }
    } else {
        ctx.fillStyle = PlayerColorsDark[p.padIndex % 4];
        ctx.fillRect(vx, vy, vw, vh);
    }
    
    ctx.translate(vx + vw/2 - p.camera.x, vy + vh/2 - p.camera.y);
    if(Game.cameraShake > 0.5) ctx.translate((Math.random()-0.5)*Game.cameraShake, (Math.random()-0.5)*Game.cameraShake);
    
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)'; ctx.lineWidth = 4;
    ctx.strokeRect(Game.map.minX, Game.map.minY, Game.map.maxX - Game.map.minX, Game.map.maxY - Game.map.minY);
    Game.walls.forEach(w => w.draw(ctx));
    
    Game.drops.forEach(d => d.draw(ctx));
    Game.particles.forEach(pt => pt.draw(ctx));
    
    Game.projectiles.forEach(pr => {
        ctx.fillStyle = pr.color; ctx.shadowColor = pr.color; ctx.shadowBlur = 10;
        ctx.beginPath(); ctx.arc(pr.x, pr.y, pr.radius, 0, Math.PI*2); ctx.fill();
    });
    ctx.shadowBlur = 0;
    
    Game.enemyProjectiles.forEach(pr => {
        ctx.fillStyle = pr.color; ctx.shadowColor = pr.color; ctx.shadowBlur = 10;
        ctx.beginPath(); ctx.arc(pr.x, pr.y, pr.radius, 0, Math.PI*2); ctx.fill();
    });
    ctx.shadowBlur = 0;
    
    Game.enemies.forEach(e => e.draw(ctx));
    if(Game.boss) Game.boss.draw(ctx);
    Game.players.forEach(pl => pl.draw(ctx));
    Game.floatingTexts.forEach(ft => ft.draw(ctx));
    
    ctx.restore();
}

function gameLoop(timestamp) {
    requestAnimationFrame(gameLoop);
    checkGamepad();
    
    if(currentState !== STATE.PLAYING) return;
    
    const dt = timestamp - Game.lastTime;
    Game.lastTime = timestamp;
    
    // Logic Update
    Game.players.forEach(p => p.update(dt));
    spawnEnemies(dt);
    if(Game.boss) Game.boss.update(dt);
    
    for(let i = Game.projectiles.length-1; i>=0; i--) {
        let p = Game.projectiles[i];
        p.x += (p.vx * dt)/1000; p.y += (p.vy * dt)/1000;
        let hit = false;
        
        if(Game.boss) {
            if(Game.boss.phase === 1) {
                for(let w of Game.boss.weapons) {
                    if(w.active) {
                        let wx = Game.boss.x + Math.cos(Game.boss.angle)*w.relX - Math.sin(Game.boss.angle)*w.relY;
                        let wy = Game.boss.y + Math.sin(Game.boss.angle)*w.relX + Math.cos(Game.boss.angle)*w.relY;
                        if(Vector.dist(p.x, p.y, wx, wy) < p.radius + 20) {
                            Game.boss.takeDamage(10 * (p.damage / 20), w.id);
                            for(let j=0; j<5; j++) Game.particles.push(new Particle(p.x, p.y, '#ff5e00', (Math.random()-0.5)*15, (Math.random()-0.5)*15, 2));
                            p.owner.score += 5;
                            hit = true; break;
                        }
                    }
                }
            } else {
                if(Vector.dist(p.x, p.y, Game.boss.x, Game.boss.y) < p.radius + Game.boss.radius) {
                    Game.boss.takeDamage(10 * (p.damage / 20));
                    for(let j=0; j<5; j++) Game.particles.push(new Particle(p.x, p.y, Game.boss.color, (Math.random()-0.5)*20, (Math.random()-0.5)*20, 2));
                    p.owner.score += 5;
                    hit = true;
                }
            }
        }
        
        if(!hit) {
            for(let j = Game.enemies.length-1; j>=0; j--) {
                let e = Game.enemies[j];
                if(Vector.dist(p.x, p.y, e.x, e.y) < p.radius + e.radius) {
                    e.hp -= p.damage; hit = true;
                    for(let k=0; k<5; k++) Game.particles.push(new Particle(p.x, p.y, e.color, (Math.random()-0.5)*10, (Math.random()-0.5)*10, 2));
                    if(e.hp <= 0) {
                        if(Math.random() < 0.4) Game.drops.push(new Coin(e.x, e.y));
                        p.owner.score += 10;
                        Game.enemies.splice(j, 1);
                    }
                    break;
                }
            }
        }
        
        if(!hit) {
            for(let w = Game.walls.length-1; w>=0; w--) {
                let wall = Game.walls[w];
                if(p.x > wall.x && p.x < wall.x + wall.w && p.y > wall.y && p.y < wall.y + wall.h) {
                    wall.hp--; hit = true; if(wall.hp <= 0) Game.walls.splice(w, 1); break;
                }
            }
        }
        
        if(hit || p.x < Game.map.minX-200 || p.x > Game.map.maxX+200 || p.y < Game.map.minY-200 || p.y > Game.map.maxY+200) Game.projectiles.splice(i, 1);
    }
    
    for(let i = Game.enemies.length-1; i>=0; i--) {
        let e = Game.enemies[i]; e.update(dt);
        if(Game.players.some(pl => pl.timeStopActive)) continue;
        for(let p of Game.players) {
            if(p.hp > 0 && Vector.dist(e.x, e.y, p.x, p.y) < e.radius + p.radius) {
                p.takeDamage(15); Game.enemies.splice(i, 1);
                for(let k=0; k<15; k++) Game.particles.push(new Particle(e.x, e.y, e.color, (Math.random()-0.5)*20, (Math.random()-0.5)*20, 3));
                break;
            }
        }
    }
    
    for(let i = Game.enemyProjectiles.length-1; i>=0; i--) {
        let pr = Game.enemyProjectiles[i];
        if(!Game.players.some(pl => pl.timeStopActive)) { pr.x += (pr.vx * dt)/1000; pr.y += (pr.vy * dt)/1000; }
        let hit = false;
        if(!Game.players.some(pl => pl.timeStopActive)) {
            for(let p of Game.players) {
                if(p.hp > 0 && Vector.dist(pr.x, pr.y, p.x, p.y) < pr.radius + p.radius) { p.takeDamage(10); hit = true; break; }
            }
        }
        if(!hit) {
            for(let w = Game.walls.length-1; w>=0; w--) {
                let wall = Game.walls[w];
                if(pr.x > wall.x && pr.x < wall.x + wall.w && pr.y > wall.y && pr.y < wall.y + wall.h) { wall.hp--; hit = true; if(wall.hp <= 0) Game.walls.splice(w, 1); break; }
            }
        }
        if(hit || pr.x < Game.map.minX-200 || pr.x > Game.map.maxX+200 || pr.y < Game.map.minY-200 || pr.y > Game.map.maxY+200) Game.enemyProjectiles.splice(i, 1);
    }
    
    for(let i = Game.drops.length-1; i>=0; i--) { Game.drops[i].update(dt); if(Game.drops[i].life <= 0) Game.drops.splice(i, 1); }
    for(let i = Game.particles.length-1; i>=0; i--) { Game.particles[i].update(dt); if(Game.particles[i].alpha <= 0) Game.particles.splice(i, 1); }
    for(let i = Game.floatingTexts.length-1; i>=0; i--) { Game.floatingTexts[i].update(dt); if(Game.floatingTexts[i].life <= 0) Game.floatingTexts.splice(i, 1); }
    if(Game.cameraShake > 0) Game.cameraShake *= 0.9;
    
    // Draw
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const num = Game.players.length;
    if(num === 1) {
        drawViewport(ctx, 0, 0, canvas.width, canvas.height, Game.players[0]);
    } else if(num === 2) {
        drawViewport(ctx, 0, 0, canvas.width/2, canvas.height, Game.players[0]);
        drawViewport(ctx, canvas.width/2, 0, canvas.width/2, canvas.height, Game.players[1]);
        ctx.strokeStyle = '#fff'; ctx.lineWidth = 4; ctx.beginPath(); ctx.moveTo(canvas.width/2, 0); ctx.lineTo(canvas.width/2, canvas.height); ctx.stroke();
    } else if(num > 2) {
        drawViewport(ctx, 0, 0, canvas.width/2, canvas.height/2, Game.players[0]);
        drawViewport(ctx, canvas.width/2, 0, canvas.width/2, canvas.height/2, Game.players[1]);
        drawViewport(ctx, 0, canvas.height/2, canvas.width/2, canvas.height/2, Game.players[2]);
        if(num === 4) drawViewport(ctx, canvas.width/2, canvas.height/2, canvas.width/2, canvas.height/2, Game.players[3]);
        
        ctx.strokeStyle = '#fff'; ctx.lineWidth = 4; 
        ctx.beginPath(); ctx.moveTo(canvas.width/2, 0); ctx.lineTo(canvas.width/2, canvas.height); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(0, canvas.height/2); ctx.lineTo(canvas.width, canvas.height/2); ctx.stroke();
    }
}

// Start
loadGame();
requestAnimationFrame(gameLoop);
"""

with open('script.js', 'w', encoding='utf-8') as f:
    f.write(js_code)
