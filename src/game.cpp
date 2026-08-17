#include "game.hpp"

#include "draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gw
{
namespace
{
const int SCREEN_W = 1920;
const int SCREEN_H = 1080;
const float PI = 3.14159265358979323846f;

const Color BG = {3, 5, 15, 255};
const Color BG_BLUE = {8, 17, 38, 255};
const Color CYAN = {0, 212, 255, 255};
const Color CYAN_DIM = {0, 88, 120, 190};
const Color WHITE = {235, 246, 255, 255};
const Color MUTED = {135, 155, 185, 255};
const Color RED = {255, 34, 68, 255};
const Color PINK = {255, 55, 135, 255};
const Color GOLD = {255, 215, 0, 255};
const Color SILVER = {184, 196, 212, 255};
const Color PURPLE = {187, 68, 255, 255};
const Color GREEN = {0, 255, 136, 255};
const Color ORANGE = {255, 119, 0, 255};
const Color PANEL = {2, 8, 24, 232};
const Color PLAYER_COLORS[4] = {
    {34, 119, 255, 255}, {255, 34, 68, 255}, {0, 255, 136, 255}, {187, 68, 255, 255}
};

enum PadButton
{
    PAD_CROSS = 0,
    PAD_CIRCLE = 1,
    PAD_SQUARE = 2,
    PAD_TRIANGLE = 3,
    PAD_L1 = 4,
    PAD_R1 = 5,
    PAD_OPTIONS = 9,
    PAD_L3 = 11,
    PAD_R3 = 12,
    PAD_UP = 13,
    PAD_DOWN = 14,
    PAD_LEFT = 15,
    PAD_RIGHT = 16,
    PAD_TOUCH = 17,
    PAD_L2 = 18,
    PAD_R2 = 19,
    PAD_BUTTON_COUNT = 24
};

enum class Screen
{
    Menu,
    Lobby,
    Playing,
    Paused,
    Controls,
    Shop,
    GameOver
};

enum class DropType
{
    Gold,
    Silver,
    Heart
};

struct Vec2
{
    float x;
    float y;
};

struct Viewport
{
    int x;
    int y;
    int w;
    int h;
};

struct Pad
{
    SDL_Joystick* handle;
    bool current[PAD_BUTTON_COUNT];
    bool previous[PAD_BUTTON_COUNT];
    float axes[4];

    Pad() : handle(nullptr)
    {
        std::memset(current, 0, sizeof(current));
        std::memset(previous, 0, sizeof(previous));
        std::memset(axes, 0, sizeof(axes));
    }
};

struct Player
{
    int pad;
    Color color;
    float x;
    float y;
    float cameraX;
    float cameraY;
    float radius;
    float hp;
    float maxHp;
    int coins;
    int score;
    int kills;
    float fireRate;
    float fireTimer;
    float damageMultiplier;
    float skillDuration;
    float skillCooldownMax;
    float skillCooldown;
    float timeStop;
    float invincible;
    int fireCost;
    int damageCost;
    int skillCost;
    bool pendingRevive;
};

struct Projectile
{
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    float damage;
    int owner;
    Color color;
};

struct Enemy
{
    float x;
    float y;
    float hp;
    float maxHp;
    float speed;
    float radius;
    float angle;
    float rotationSpeed;
    int edges;
    Color color;
};

struct Drop
{
    float x;
    float y;
    float life;
    float phase;
    DropType type;
};

struct Particle
{
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float maxLife;
    float size;
    Color color;
};

struct Wall
{
    float x;
    float y;
    float w;
    float h;
    float hp;
};

struct BossWeapon
{
    float relX;
    float relY;
    float hp;
    float maxHp;
    bool active;
};

struct Boss
{
    bool active;
    int phase;
    int shape;
    float x;
    float y;
    float radius;
    float hp;
    float maxHp;
    float rageHp;
    float maxRageHp;
    float angle;
    float attackTimer;
    float teleportTimer;
    float spiralAngle;
    Color color;
    Color trueColor;
    BossWeapon weapons[3];
};

float clampf(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

float distanceSquared(float x1, float y1, float x2, float y2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return dx * dx + dy * dy;
}

float distance(float x1, float y1, float x2, float y2)
{
    return std::sqrt(distanceSquared(x1, y1, x2, y2));
}

float angleTo(float x1, float y1, float x2, float y2)
{
    return std::atan2(y2 - y1, x2 - x1);
}

std::string number(int value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    return std::string(buffer);
}

Color withAlpha(Color value, int alpha)
{
    value.a = static_cast<Uint8>(clampf(static_cast<float>(alpha), 0.0f, 255.0f));
    return value;
}
}

struct Game::Impl
{
    explicit Impl(SDL_Renderer* value)
        : renderer(value), active(true), screen(Screen::Menu), returnScreen(Screen::Menu), menuIndex(0),
          pausePad(0), controllerCount(0), playerCount(0), lastTick(0), fpsTimer(0), fpsFrames(0), fpsValue(0), rng(0xc0ffee11u),
          mapMinX(0), mapMinY(0), mapMaxX(SCREEN_W), mapMaxY(SCREEN_H), wave(1),
          waveElapsed(0), spawnTimer(0), announcementTimer(0), bossDelay(0), cameraShake(0),
          totalKills(0), profileSilver(0), lobbyTimer(3.0f), localRequested(false), enemyEdges(0),
          enemyColor(RED)
    {
        std::memset(&boss, 0, sizeof(boss));
    }

    ~Impl()
    {
        for (int i = 0; i < controllerCount; ++i)
            if (pads[i].handle) SDL_JoystickClose(pads[i].handle);
    }

    SDL_Renderer* renderer;
    bool active;
    Screen screen;
    Screen returnScreen;
    int menuIndex;
    int pausePad;
    Pad pads[4];
    int controllerCount;
    Player players[4];
    int playerCount;
    uint32_t lastTick;
    uint32_t fpsTimer;
    int fpsFrames;
    int fpsValue;
    uint32_t rng;

    float mapMinX;
    float mapMinY;
    float mapMaxX;
    float mapMaxY;
    int wave;
    float waveElapsed;
    float spawnTimer;
    float announcementTimer;
    float bossDelay;
    float cameraShake;
    int totalKills;
    int profileSilver;
    float lobbyTimer;
    bool localRequested;
    int enemyEdges;
    Color enemyColor;

    std::vector<Projectile> projectiles;
    std::vector<Projectile> enemyProjectiles;
    std::vector<Enemy> enemies;
    std::vector<Drop> drops;
    std::vector<Particle> particles;
    std::vector<Wall> walls;
    Boss boss;

    bool initialize();
    void sampleInput();
    bool down(int pad, int button) const;
    bool pressed(int pad, int button) const;
    float axis(int pad, int index) const;
    void rumble(int pad, float strength, uint32_t milliseconds);
    uint32_t randomNext();
    float random01();
    int randomInt(int minimum, int maximum);
    void tick(uint32_t now);
    void updateMenu(float dt);
    void updateLobby(float dt);
    void updatePlaying(float dt);
    void startGame(int count);
    void resetWorld();
    void nextWave();
    void spawnEnemy();
    void spawnBoss();
    void updateBoss(float dt);
    void bossWeaponPosition(int index, float& x, float& y) const;
    void damageBoss(float damage, int weaponIndex);
    void defeatBoss();
    void addEnemyProjectile(float x, float y, float angle, float speed, float radius, Color color);
    void shoot(Player& player, int playerIndex);
    bool anyTimeStop() const;
    void addParticles(float x, float y, Color color, int count, float speed, float life = 0.7f);
    void addDrop(float x, float y, DropType type);
    void damagePlayer(Player& player, float damage);
    void buyUpgrade(Player& player, int type);
    void resolveWalls(float& x, float& y, float radius);
    void handleProjectileCollisions(float dt);
    void handleEnemies(float dt);
    void handleDrops(float dt);
    void updateParticles(float dt);
    void checkGameOver();

    void render();
    void renderBackdrop();
    void renderMenu();
    void renderLobby();
    void renderControls();
    void renderShop();
    void renderGameOver();
    void renderPause();
    void renderPlaying();
    void renderViewport(const Viewport& viewport, const Player& cameraPlayer, int playerIndex);
    void renderWorldEntityCircle(const Viewport& viewport, const Player& cameraPlayer, float x, float y, float radius, Color color, int glow);
    Vec2 toScreen(const Viewport& viewport, const Player& cameraPlayer, float x, float y) const;
    void renderEnemy(const Viewport& viewport, const Player& cameraPlayer, const Enemy& enemy);
    void renderBoss(const Viewport& viewport, const Player& cameraPlayer);
    void renderPlayer(const Viewport& viewport, const Player& cameraPlayer, const Player& player, int index);
    void renderHud(const Viewport& viewport, const Player& player, int index);
    void renderMenuOptions(const std::vector<std::string>& options, int startY, int scale);
};

bool Game::Impl::initialize()
{
    SDL_JoystickEventState(SDL_ENABLE);
    controllerCount = std::min(4, SDL_NumJoysticks());
    for (int i = 0; i < controllerCount; ++i)
    {
        pads[i].handle = SDL_JoystickOpen(i);
        if (!pads[i].handle)
        {
            std::printf("Falha ao abrir controle %d: %s\n", i, SDL_GetError());
            controllerCount = i;
            break;
        }
    }
    return true;
}

void Game::Impl::sampleInput()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT) active = false;

    SDL_JoystickUpdate();
    for (int p = 0; p < controllerCount; ++p)
    {
        for (int button = 0; button < PAD_BUTTON_COUNT; ++button)
        {
            pads[p].previous[button] = pads[p].current[button];
            pads[p].current[button] = SDL_JoystickGetButton(pads[p].handle, button) != 0;
        }
        const int axisCount = SDL_JoystickNumAxes(pads[p].handle);
        for (int a = 0; a < 4; ++a)
            pads[p].axes[a] = a < axisCount ? SDL_JoystickGetAxis(pads[p].handle, a) / 32767.0f : 0.0f;
    }
}

bool Game::Impl::down(int pad, int button) const
{
    return pad >= 0 && pad < controllerCount && button >= 0 && button < PAD_BUTTON_COUNT && pads[pad].current[button];
}

bool Game::Impl::pressed(int pad, int button) const
{
    return down(pad, button) && !pads[pad].previous[button];
}

float Game::Impl::axis(int pad, int index) const
{
    if (pad < 0 || pad >= controllerCount || index < 0 || index >= 4) return 0.0f;
    const float value = pads[pad].axes[index];
    return std::fabs(value) < 0.18f ? 0.0f : value;
}

void Game::Impl::rumble(int pad, float strength, uint32_t milliseconds)
{
    // The OpenOrbis SDL2 build has its generic haptic backend disabled.
    // Leave rumble off until native video/input stability is confirmed on hardware.
    (void)pad;
    (void)strength;
    (void)milliseconds;
}

uint32_t Game::Impl::randomNext()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

float Game::Impl::random01()
{
    return (randomNext() & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

int Game::Impl::randomInt(int minimum, int maximum)
{
    if (maximum <= minimum) return minimum;
    return minimum + static_cast<int>(randomNext() % static_cast<uint32_t>(maximum - minimum + 1));
}

void Game::Impl::tick(uint32_t now)
{
    ++fpsFrames;
    if (fpsTimer == 0) fpsTimer = now;
    else if (now - fpsTimer >= 500)
    {
        fpsValue = static_cast<int>(fpsFrames * 1000u / (now - fpsTimer));
        fpsFrames = 0;
        fpsTimer = now;
    }

    sampleInput();
    if (!active) return;

    float dt = lastTick == 0 ? 1.0f / 60.0f : (now - lastTick) / 1000.0f;
    lastTick = now;
    dt = clampf(dt, 0.0f, 0.08f);

    switch (screen)
    {
        case Screen::Menu:
        case Screen::Paused:
        case Screen::Controls:
        case Screen::Shop:
        case Screen::GameOver:
            updateMenu(dt);
            break;
        case Screen::Lobby:
            updateLobby(dt);
            break;
        case Screen::Playing:
            updatePlaying(dt);
            break;
    }
    render();
}

void Game::Impl::updateMenu(float)
{
    const int pad = screen == Screen::Paused ? pausePad : 0;
    if (screen == Screen::Controls || screen == Screen::Shop)
    {
        if (pressed(pad, PAD_CIRCLE) || pressed(pad, PAD_CROSS))
        {
            screen = returnScreen;
            menuIndex = 0;
        }
        return;
    }

    int optionCount = 0;
    if (screen == Screen::Menu) optionCount = 5;
    else if (screen == Screen::Paused) optionCount = 3;
    else if (screen == Screen::GameOver) optionCount = 1;

    if (pressed(pad, PAD_UP)) menuIndex = (menuIndex + optionCount - 1) % optionCount;
    if (pressed(pad, PAD_DOWN)) menuIndex = (menuIndex + 1) % optionCount;
    if (screen == Screen::Paused && pressed(pad, PAD_CIRCLE))
    {
        screen = Screen::Playing;
        return;
    }
    if (!pressed(pad, PAD_CROSS)) return;

    if (screen == Screen::Menu)
    {
        if (menuIndex == 0)
        {
            if (controllerCount > 0) startGame(1);
        }
        else if (menuIndex == 1)
        {
            localRequested = true;
            lobbyTimer = 3.0f;
            screen = Screen::Lobby;
        }
        else if (menuIndex == 2)
        {
            returnScreen = Screen::Menu;
            screen = Screen::Shop;
        }
        else if (menuIndex == 3)
        {
            returnScreen = Screen::Menu;
            screen = Screen::Controls;
        }
        else active = false;
        menuIndex = 0;
    }
    else if (screen == Screen::Paused)
    {
        if (menuIndex == 0) screen = Screen::Playing;
        else if (menuIndex == 1)
        {
            returnScreen = Screen::Paused;
            screen = Screen::Controls;
        }
        else
        {
            screen = Screen::Menu;
            menuIndex = 0;
        }
    }
    else if (screen == Screen::GameOver)
    {
        screen = Screen::Menu;
        menuIndex = 0;
    }
}

void Game::Impl::updateLobby(float dt)
{
    if (pressed(0, PAD_CIRCLE))
    {
        screen = Screen::Menu;
        menuIndex = 0;
        return;
    }
    if (controllerCount >= 2)
    {
        lobbyTimer -= dt;
        if (pressed(0, PAD_CROSS) || lobbyTimer <= 0.0f) startGame(controllerCount);
    }
    else lobbyTimer = 3.0f;
}

void Game::Impl::resetWorld()
{
    projectiles.clear();
    enemyProjectiles.clear();
    enemies.clear();
    drops.clear();
    particles.clear();
    walls.clear();
    std::memset(&boss, 0, sizeof(boss));
    mapMinX = 0.0f;
    mapMinY = 0.0f;
    mapMaxX = static_cast<float>(SCREEN_W);
    mapMaxY = static_cast<float>(SCREEN_H);
    wave = 1;
    waveElapsed = 0.0f;
    spawnTimer = 0.25f;
    announcementTimer = 2.5f;
    bossDelay = 0.0f;
    cameraShake = 0.0f;
    totalKills = 0;
    enemyEdges = 0;
    enemyColor = RED;
}

void Game::Impl::startGame(int count)
{
    resetWorld();
    playerCount = std::max(1, std::min(4, std::min(count, controllerCount)));
    for (int i = 0; i < playerCount; ++i)
    {
        Player& player = players[i];
        player.pad = i;
        player.color = PLAYER_COLORS[i];
        player.x = SCREEN_W * 0.5f + (i - (playerCount - 1) * 0.5f) * 70.0f;
        player.y = SCREEN_H * 0.56f;
        player.cameraX = player.x;
        player.cameraY = player.y;
        player.radius = 18.0f;
        player.hp = 100.0f;
        player.maxHp = 100.0f;
        player.coins = 0;
        player.score = 0;
        player.kills = 0;
        player.fireRate = 0.16f;
        player.fireTimer = 0.0f;
        player.damageMultiplier = 1.0f;
        player.skillDuration = 3.0f;
        player.skillCooldownMax = 12.0f;
        player.skillCooldown = 0.0f;
        player.timeStop = 0.0f;
        player.invincible = 1.0f;
        player.fireCost = 15;
        player.damageCost = 40;
        player.skillCost = 25;
        player.pendingRevive = false;
    }
    screen = Screen::Playing;
    menuIndex = 0;
}

bool Game::Impl::anyTimeStop() const
{
    for (int i = 0; i < playerCount; ++i)
        if (players[i].hp > 0.0f && players[i].timeStop > 0.0f) return true;
    return false;
}

void Game::Impl::addParticles(float x, float y, Color colorValue, int count, float speed, float life)
{
    for (int i = 0; i < count && particles.size() < 500; ++i)
    {
        const float angle = random01() * PI * 2.0f;
        const float velocity = speed * (0.35f + random01() * 0.65f);
        Particle particle;
        particle.x = x;
        particle.y = y;
        particle.vx = std::cos(angle) * velocity;
        particle.vy = std::sin(angle) * velocity;
        particle.life = life * (0.55f + random01() * 0.7f);
        particle.maxLife = particle.life;
        particle.size = 2.0f + random01() * 4.0f;
        particle.color = colorValue;
        particles.push_back(particle);
    }
}

void Game::Impl::addDrop(float x, float y, DropType type)
{
    Drop drop;
    drop.x = x;
    drop.y = y;
    drop.life = type == DropType::Heart ? 12.0f : 9.0f;
    drop.phase = random01() * PI * 2.0f;
    drop.type = type;
    drops.push_back(drop);
}

void Game::Impl::spawnEnemy()
{
    const int side = randomInt(0, 3);
    Enemy enemy;
    if (side == 0)
    {
        enemy.x = mapMinX + random01() * (mapMaxX - mapMinX);
        enemy.y = mapMinY - 35.0f;
    }
    else if (side == 1)
    {
        enemy.x = mapMaxX + 35.0f;
        enemy.y = mapMinY + random01() * (mapMaxY - mapMinY);
    }
    else if (side == 2)
    {
        enemy.x = mapMinX + random01() * (mapMaxX - mapMinX);
        enemy.y = mapMaxY + 35.0f;
    }
    else
    {
        enemy.x = mapMinX - 35.0f;
        enemy.y = mapMinY + random01() * (mapMaxY - mapMinY);
    }
    const float hpFactor = 1.0f + (playerCount - 1) * 0.25f;
    enemy.hp = (40.0f + wave * 15.0f) * hpFactor;
    enemy.maxHp = enemy.hp;
    enemy.speed = 100.0f + random01() * 60.0f + wave * 5.0f;
    enemy.radius = 15.0f + random01() * 4.0f;
    enemy.angle = random01() * PI * 2.0f;
    enemy.rotationSpeed = (random01() - 0.5f) * 2.0f;
    enemy.edges = enemyEdges;
    enemy.color = enemyColor;
    enemies.push_back(enemy);
}

void Game::Impl::nextWave()
{
    ++wave;
    waveElapsed = 0.0f;
    spawnTimer = 0.3f;
    announcementTimer = 2.5f;
    for (int i = 0; i < playerCount; ++i)
    {
        Player& player = players[i];
        if (player.hp <= 0.0f && player.pendingRevive)
        {
            player.hp = player.maxHp * 0.5f;
            player.pendingRevive = false;
            player.invincible = 2.0f;
            addParticles(player.x, player.y, player.color, 24, 260.0f, 1.0f);
            rumble(player.pad, 0.7f, 400);
        }
    }
    if (wave % 5 == 0) spawnBoss();
}

void Game::Impl::spawnBoss()
{
    std::memset(&boss, 0, sizeof(boss));
    boss.active = true;
    boss.phase = 1;
    boss.shape = randomInt(0, 3);
    boss.x = (mapMinX + mapMaxX) * 0.5f;
    boss.y = mapMinY - 120.0f;
    boss.radius = 55.0f;
    const float difficulty = 1.0f + (wave / 10.0f) * 0.5f;
    boss.hp = 3000.0f * difficulty;
    boss.maxHp = boss.hp;
    boss.rageHp = 3000.0f * difficulty;
    boss.maxRageHp = boss.rageHp;
    boss.angle = 0.0f;
    boss.attackTimer = 0.0f;
    boss.teleportTimer = 4.0f;
    boss.spiralAngle = 0.0f;
    const Color shapeColors[4] = {GREEN, PURPLE, RED, PLAYER_COLORS[0]};
    boss.trueColor = shapeColors[boss.shape];
    boss.color = {55, 55, 64, 255};
    const float weaponX[3] = {0.0f, 82.0f, -82.0f};
    const float weaponY[3] = {-95.0f, 47.0f, 47.0f};
    for (int i = 0; i < 3; ++i)
    {
        boss.weapons[i].relX = weaponX[i];
        boss.weapons[i].relY = weaponY[i];
        boss.weapons[i].hp = 500.0f * difficulty;
        boss.weapons[i].maxHp = boss.weapons[i].hp;
        boss.weapons[i].active = true;
    }
    enemies.clear();
}

void Game::Impl::bossWeaponPosition(int index, float& x, float& y) const
{
    const BossWeapon& weapon = boss.weapons[index];
    const float c = std::cos(boss.angle);
    const float s = std::sin(boss.angle);
    x = boss.x + c * weapon.relX - s * weapon.relY;
    y = boss.y + s * weapon.relX + c * weapon.relY;
}

void Game::Impl::addEnemyProjectile(float x, float y, float angle, float speed, float radius, Color colorValue)
{
    Projectile projectile;
    projectile.x = x;
    projectile.y = y;
    projectile.vx = std::cos(angle) * speed;
    projectile.vy = std::sin(angle) * speed;
    projectile.radius = radius;
    projectile.damage = 10.0f;
    projectile.owner = -1;
    projectile.color = colorValue;
    enemyProjectiles.push_back(projectile);
}

void Game::Impl::updateBoss(float dt)
{
    if (!boss.active) return;
    if (boss.y < mapMinY + 160.0f) boss.y += 110.0f * dt;
    if (anyTimeStop()) return;

    boss.angle += 1.6f * dt;
    boss.attackTimer += dt;
    Player* target = nullptr;
    float nearest = 1e30f;
    for (int i = 0; i < playerCount; ++i)
    {
        if (players[i].hp <= 0.0f) continue;
        const float d = distanceSquared(boss.x, boss.y, players[i].x, players[i].y);
        if (d < nearest)
        {
            nearest = d;
            target = &players[i];
        }
    }
    if (!target) return;

    if (boss.phase == 1)
    {
        if (boss.attackTimer >= 1.4f)
        {
            boss.attackTimer = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                if (!boss.weapons[i].active) continue;
                float x, y;
                bossWeaponPosition(i, x, y);
                addEnemyProjectile(x, y, angleTo(x, y, target->x, target->y), 320.0f, 7.0f, ORANGE);
            }
        }
        bool allDestroyed = true;
        for (int i = 0; i < 3; ++i) allDestroyed = allDestroyed && !boss.weapons[i].active;
        if (allDestroyed)
        {
            boss.phase = 2;
            boss.color = boss.trueColor;
            boss.attackTimer = 0.0f;
            cameraShake = 30.0f;
            addParticles(boss.x, boss.y, boss.color, 70, 380.0f, 1.2f);
        }
    }
    else if (boss.phase == 2)
    {
        if (boss.shape == 0 && boss.attackTimer >= 0.9f)
        {
            boss.attackTimer = 0.0f;
            const float base = angleTo(boss.x, boss.y, target->x, target->y);
            for (int i = -1; i <= 1; ++i) addEnemyProjectile(boss.x, boss.y, base + i * 0.35f, 340.0f, 8.0f, boss.trueColor);
        }
        else if (boss.shape == 1 && boss.attackTimer >= 0.7f)
        {
            boss.attackTimer = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                const float angle = PI * 0.5f * i;
                addEnemyProjectile(boss.x, boss.y, angle, 280.0f, 9.0f, boss.trueColor);
                addEnemyProjectile(boss.x, boss.y, angle, 200.0f, 7.0f, boss.trueColor);
            }
        }
        else if (boss.shape == 2 && boss.attackTimer >= 0.28f)
        {
            boss.attackTimer = 0.0f;
            for (int i = 0; i < 4; ++i) addEnemyProjectile(boss.x, boss.y, boss.angle * 2.0f + PI * 0.5f * i, 260.0f, 9.0f, boss.trueColor);
        }
        else if (boss.shape == 3 && boss.attackTimer >= 1.1f)
        {
            boss.attackTimer = 0.0f;
            for (int row = -1; row <= 1; row += 2)
                for (int i = -5; i <= 5; ++i)
                    addEnemyProjectile(boss.x + i * 80.0f, boss.y, row > 0 ? PI * 0.5f : -PI * 0.5f, 300.0f, 8.0f, boss.trueColor);
        }
    }
    else
    {
        const float targetAngle = angleTo(boss.x, boss.y, target->x, target->y);
        const float chaseSpeed = boss.shape == 2 ? 130.0f : (boss.shape == 0 ? 100.0f : 85.0f);
        boss.x += std::cos(targetAngle) * chaseSpeed * dt;
        boss.y += std::sin(targetAngle) * chaseSpeed * dt;
        boss.x = clampf(boss.x, mapMinX + boss.radius, mapMaxX - boss.radius);
        boss.y = clampf(boss.y, mapMinY + boss.radius, mapMaxY - boss.radius);

        if (boss.shape == 0)
        {
            boss.teleportTimer -= dt;
            if (boss.teleportTimer <= 0.0f)
            {
                boss.teleportTimer = 4.0f;
                const float angle = random01() * PI * 2.0f;
                boss.x = clampf(target->x + std::cos(angle) * 180.0f, mapMinX + boss.radius, mapMaxX - boss.radius);
                boss.y = clampf(target->y + std::sin(angle) * 180.0f, mapMinY + boss.radius, mapMaxY - boss.radius);
                cameraShake = 20.0f;
                addParticles(boss.x, boss.y, boss.trueColor, 26, 360.0f, 0.8f);
            }
            if (boss.attackTimer >= 0.7f)
            {
                boss.attackTimer = 0.0f;
                for (int i = -1; i <= 1; ++i) addEnemyProjectile(boss.x, boss.y, targetAngle + i * 0.28f, 490.0f, 11.0f, RED);
            }
        }
        else if (boss.shape == 1)
        {
            boss.spiralAngle += 4.5f * dt;
            if (boss.attackTimer >= 0.2f)
            {
                boss.attackTimer = 0.0f;
                addEnemyProjectile(boss.x, boss.y, boss.spiralAngle, 400.0f, 9.0f, boss.trueColor);
                addEnemyProjectile(boss.x, boss.y, boss.spiralAngle + PI, 400.0f, 9.0f, boss.trueColor);
            }
        }
        else if (boss.shape == 2 && boss.attackTimer >= 0.65f)
        {
            boss.attackTimer = 0.0f;
            for (int i = -2; i <= 2; ++i) addEnemyProjectile(boss.x, boss.y, targetAngle + i * 0.22f, 470.0f, 10.0f, RED);
        }
        else if (boss.shape == 3 && boss.attackTimer >= 0.8f)
        {
            boss.attackTimer = 0.0f;
            for (int i = 0; i < 8; ++i) addEnemyProjectile(boss.x, boss.y, i * PI * 0.25f, 440.0f, 10.0f, boss.trueColor);
        }
    }
}

void Game::Impl::damageBoss(float damage, int weaponIndex)
{
    if (!boss.active) return;
    if (boss.phase == 1 && weaponIndex >= 0 && weaponIndex < 3)
    {
        BossWeapon& weapon = boss.weapons[weaponIndex];
        if (!weapon.active) return;
        weapon.hp -= damage;
        if (weapon.hp <= 0.0f)
        {
            weapon.hp = 0.0f;
            weapon.active = false;
            cameraShake = 12.0f;
            for (int i = 0; i < playerCount; ++i) players[i].score += 500;
            float x, y;
            bossWeaponPosition(weaponIndex, x, y);
            addParticles(x, y, ORANGE, 36, 430.0f, 1.0f);
        }
    }
    else if (boss.phase == 2)
    {
        boss.hp -= damage;
        if (boss.hp <= 0.0f)
        {
            boss.hp = 0.0f;
            boss.phase = 3;
            boss.color = RED;
            boss.attackTimer = 0.0f;
            cameraShake = 30.0f;
            addParticles(boss.x, boss.y, RED, 80, 430.0f, 1.3f);
        }
    }
    else if (boss.phase == 3)
    {
        boss.rageHp -= damage;
        if (boss.rageHp <= 0.0f) defeatBoss();
    }
}

void Game::Impl::defeatBoss()
{
    const int shape = boss.shape;
    const Color theme = boss.trueColor;
    addParticles(boss.x, boss.y, ORANGE, 140, 620.0f, 1.6f);
    cameraShake = 55.0f;
    for (int i = 0; i < playerCount; ++i)
    {
        players[i].score += 5000;
        rumble(players[i].pad, 1.0f, 1200);
    }
    profileSilver += 15;
    boss.active = false;
    enemyProjectiles.clear();
    bossDelay = 3.5f;
    enemyEdges = shape == 0 ? 3 : (shape == 1 ? 4 : 0);
    enemyColor = theme;
    mapMinX -= 300.0f;
    mapMinY -= 300.0f;
    mapMaxX += 300.0f;
    mapMaxY += 300.0f;
    const float cx = (mapMinX + mapMaxX) * 0.5f;
    const float cy = (mapMinY + mapMaxY) * 0.5f;
    walls.push_back({mapMinX + 100.0f, cy - 40.0f, 80.0f, 80.0f, 40.0f});
    walls.push_back({mapMaxX - 180.0f, cy - 40.0f, 80.0f, 80.0f, 40.0f});
    walls.push_back({cx - 40.0f, mapMinY + 100.0f, 80.0f, 80.0f, 40.0f});
    walls.push_back({cx - 40.0f, mapMaxY - 180.0f, 80.0f, 80.0f, 40.0f});
}

void Game::Impl::shoot(Player& player, int playerIndex)
{
    float targetX = 0.0f;
    float targetY = 0.0f;
    float nearest = 1e30f;
    bool found = false;
    for (unsigned i = 0; i < enemies.size(); ++i)
    {
        const float d = distanceSquared(player.x, player.y, enemies[i].x, enemies[i].y);
        if (d < nearest)
        {
            nearest = d;
            targetX = enemies[i].x;
            targetY = enemies[i].y;
            found = true;
        }
    }
    if (boss.active)
    {
        if (boss.phase == 1)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (!boss.weapons[i].active) continue;
                float x, y;
                bossWeaponPosition(i, x, y);
                const float d = distanceSquared(player.x, player.y, x, y);
                if (d < nearest)
                {
                    nearest = d;
                    targetX = x;
                    targetY = y;
                    found = true;
                }
            }
        }
        else
        {
            const float d = distanceSquared(player.x, player.y, boss.x, boss.y);
            if (d < nearest)
            {
                targetX = boss.x;
                targetY = boss.y;
                found = true;
            }
        }
    }
    if (!found) return;

    const float angle = angleTo(player.x, player.y, targetX, targetY);
    Projectile projectile;
    projectile.x = player.x;
    projectile.y = player.y;
    projectile.vx = std::cos(angle) * 820.0f;
    projectile.vy = std::sin(angle) * 820.0f;
    projectile.radius = 5.0f;
    projectile.damage = 20.0f * player.damageMultiplier;
    projectile.owner = playerIndex;
    projectile.color = player.color;
    projectiles.push_back(projectile);
    player.fireTimer = player.fireRate;
}

void Game::Impl::buyUpgrade(Player& player, int type)
{
    if (type == 1 && player.coins >= player.fireCost)
    {
        player.coins -= player.fireCost;
        player.fireRate = std::max(0.04f, player.fireRate - 0.015f);
        player.fireCost = static_cast<int>(player.fireCost * 1.55f);
        rumble(player.pad, 0.35f, 120);
    }
    else if (type == 2 && player.coins >= player.damageCost)
    {
        player.coins -= player.damageCost;
        player.damageMultiplier += 0.5f;
        player.damageCost = static_cast<int>(player.damageCost * 2.2f);
        rumble(player.pad, 0.45f, 120);
    }
    else if (type == 3 && player.coins >= player.skillCost)
    {
        player.coins -= player.skillCost;
        player.skillDuration += 0.5f;
        player.skillCooldownMax = std::max(5.0f, player.skillCooldownMax - 1.0f);
        player.skillCost = static_cast<int>(player.skillCost * 1.8f);
        rumble(player.pad, 0.4f, 120);
    }
}

void Game::Impl::resolveWalls(float& x, float& y, float radius)
{
    for (unsigned i = 0; i < walls.size(); ++i)
    {
        const Wall& wall = walls[i];
        const float closestX = clampf(x, wall.x, wall.x + wall.w);
        const float closestY = clampf(y, wall.y, wall.y + wall.h);
        float dx = x - closestX;
        float dy = y - closestY;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < radius)
        {
            if (d < 0.001f)
            {
                dx = 1.0f;
                dy = 0.0f;
                d = 1.0f;
            }
            x = closestX + dx / d * radius;
            y = closestY + dy / d * radius;
        }
    }
}

void Game::Impl::damagePlayer(Player& player, float damage)
{
    if (player.hp <= 0.0f || player.invincible > 0.0f) return;
    player.hp -= damage;
    player.invincible = 0.4f;
    cameraShake = 14.0f;
    addParticles(player.x, player.y, RED, 16, 300.0f, 0.8f);
    rumble(player.pad, 0.9f, 250);
    if (player.hp <= 0.0f)
    {
        player.hp = 0.0f;
        player.pendingRevive = true;
        addParticles(player.x, player.y, player.color, 34, 420.0f, 1.2f);
        checkGameOver();
    }
}

void Game::Impl::handleProjectileCollisions(float dt)
{
    for (int i = static_cast<int>(projectiles.size()) - 1; i >= 0; --i)
    {
        Projectile& projectile = projectiles[i];
        projectile.x += projectile.vx * dt;
        projectile.y += projectile.vy * dt;
        bool hit = false;

        if (boss.active)
        {
            if (boss.phase == 1)
            {
                for (int weapon = 0; weapon < 3; ++weapon)
                {
                    if (!boss.weapons[weapon].active) continue;
                    float x, y;
                    bossWeaponPosition(weapon, x, y);
                    const float radius = projectile.radius + 22.0f;
                    if (distanceSquared(projectile.x, projectile.y, x, y) <= radius * radius)
                    {
                        damageBoss(projectile.damage * 0.5f, weapon);
                        if (projectile.owner >= 0) players[projectile.owner].score += 5;
                        hit = true;
                        break;
                    }
                }
            }
            else
            {
                const float radius = projectile.radius + boss.radius;
                if (distanceSquared(projectile.x, projectile.y, boss.x, boss.y) <= radius * radius)
                {
                    damageBoss(projectile.damage * 0.5f, -1);
                    if (projectile.owner >= 0) players[projectile.owner].score += 5;
                    hit = true;
                }
            }
        }

        if (!hit)
        {
            for (int e = static_cast<int>(enemies.size()) - 1; e >= 0; --e)
            {
                Enemy& enemy = enemies[e];
                const float radius = projectile.radius + enemy.radius;
                if (distanceSquared(projectile.x, projectile.y, enemy.x, enemy.y) > radius * radius) continue;
                enemy.hp -= projectile.damage;
                addParticles(projectile.x, projectile.y, enemy.color, 6, 190.0f, 0.45f);
                if (enemy.hp <= 0.0f)
                {
                    const float x = enemy.x;
                    const float y = enemy.y;
                    const Color colorValue = enemy.color;
                    if (random01() < 0.40f) addDrop(x, y, DropType::Gold);
                    if (random01() < 0.18f) addDrop(x, y, DropType::Silver);
                    if (projectile.owner >= 0 && projectile.owner < playerCount)
                    {
                        players[projectile.owner].score += 10;
                        players[projectile.owner].kills++;
                    }
                    ++totalKills;
                    if (totalKills > 0 && totalKills % 50 == 0) addDrop(x, y, DropType::Heart);
                    addParticles(x, y, colorValue, 18, 320.0f, 0.8f);
                    enemies.erase(enemies.begin() + e);
                }
                hit = true;
                break;
            }
        }

        if (!hit)
        {
            for (int wallIndex = static_cast<int>(walls.size()) - 1; wallIndex >= 0; --wallIndex)
            {
                Wall& wall = walls[wallIndex];
                if (projectile.x < wall.x || projectile.x > wall.x + wall.w || projectile.y < wall.y || projectile.y > wall.y + wall.h) continue;
                wall.hp -= 1.0f;
                if (wall.hp <= 0.0f) walls.erase(walls.begin() + wallIndex);
                hit = true;
                break;
            }
        }

        if (hit || projectile.x < mapMinX - 200.0f || projectile.x > mapMaxX + 200.0f || projectile.y < mapMinY - 200.0f || projectile.y > mapMaxY + 200.0f)
            projectiles.erase(projectiles.begin() + i);
    }
}

void Game::Impl::handleEnemies(float dt)
{
    const bool stopped = anyTimeStop();
    for (int i = static_cast<int>(enemies.size()) - 1; i >= 0; --i)
    {
        Enemy& enemy = enemies[i];
        if (!stopped)
        {
            Player* target = nullptr;
            float nearest = 1e30f;
            for (int p = 0; p < playerCount; ++p)
            {
                if (players[p].hp <= 0.0f) continue;
                const float d = distanceSquared(enemy.x, enemy.y, players[p].x, players[p].y);
                if (d < nearest)
                {
                    nearest = d;
                    target = &players[p];
                }
            }
            if (target)
            {
                const float angle = angleTo(enemy.x, enemy.y, target->x, target->y);
                enemy.x += std::cos(angle) * enemy.speed * dt;
                enemy.y += std::sin(angle) * enemy.speed * dt;
            }
            enemy.angle += enemy.rotationSpeed * dt;
            resolveWalls(enemy.x, enemy.y, enemy.radius);
        }

        bool consumed = false;
        if (!stopped)
        {
            for (int p = 0; p < playerCount; ++p)
            {
                if (players[p].hp <= 0.0f) continue;
                const float radius = enemy.radius + players[p].radius;
                if (distanceSquared(enemy.x, enemy.y, players[p].x, players[p].y) <= radius * radius)
                {
                    damagePlayer(players[p], 15.0f);
                    addParticles(enemy.x, enemy.y, enemy.color, 16, 330.0f, 0.8f);
                    enemies.erase(enemies.begin() + i);
                    consumed = true;
                    break;
                }
            }
        }
        if (consumed) continue;
    }

    for (int i = static_cast<int>(enemyProjectiles.size()) - 1; i >= 0; --i)
    {
        Projectile& projectile = enemyProjectiles[i];
        if (!stopped)
        {
            projectile.x += projectile.vx * dt;
            projectile.y += projectile.vy * dt;
        }
        bool hit = false;
        if (!stopped)
        {
            for (int p = 0; p < playerCount; ++p)
            {
                if (players[p].hp <= 0.0f) continue;
                const float radius = projectile.radius + players[p].radius;
                if (distanceSquared(projectile.x, projectile.y, players[p].x, players[p].y) <= radius * radius)
                {
                    damagePlayer(players[p], projectile.damage);
                    hit = true;
                    break;
                }
            }
        }
        if (!hit)
        {
            for (int wallIndex = static_cast<int>(walls.size()) - 1; wallIndex >= 0; --wallIndex)
            {
                Wall& wall = walls[wallIndex];
                if (projectile.x < wall.x || projectile.x > wall.x + wall.w || projectile.y < wall.y || projectile.y > wall.y + wall.h) continue;
                wall.hp -= 1.0f;
                if (wall.hp <= 0.0f) walls.erase(walls.begin() + wallIndex);
                hit = true;
                break;
            }
        }
        if (hit || projectile.x < mapMinX - 200.0f || projectile.x > mapMaxX + 200.0f || projectile.y < mapMinY - 200.0f || projectile.y > mapMaxY + 200.0f)
            enemyProjectiles.erase(enemyProjectiles.begin() + i);
    }
}

void Game::Impl::handleDrops(float dt)
{
    for (int i = static_cast<int>(drops.size()) - 1; i >= 0; --i)
    {
        Drop& drop = drops[i];
        drop.life -= dt;
        drop.phase += dt * 2.5f;
        Player* target = nullptr;
        float nearest = 1e30f;
        for (int p = 0; p < playerCount; ++p)
        {
            if (players[p].hp <= 0.0f) continue;
            const float d = distance(drop.x, drop.y, players[p].x, players[p].y);
            if (d < nearest)
            {
                nearest = d;
                target = &players[p];
            }
        }
        const float attraction = drop.type == DropType::Heart ? 200.0f : 160.0f;
        if (target && nearest < attraction)
        {
            const float angle = angleTo(drop.x, drop.y, target->x, target->y);
            const float speed = drop.type == DropType::Heart ? 350.0f : 450.0f;
            drop.x += std::cos(angle) * speed * dt;
            drop.y += std::sin(angle) * speed * dt;
        }
        if (target && nearest < target->radius + 13.0f)
        {
            if (drop.type == DropType::Gold) target->coins++;
            else if (drop.type == DropType::Silver) profileSilver++;
            else target->hp = std::min(target->maxHp, target->hp + target->maxHp * 0.30f);
            addParticles(drop.x, drop.y, drop.type == DropType::Heart ? PINK : (drop.type == DropType::Silver ? SILVER : GOLD), 10, 190.0f, 0.5f);
            rumble(target->pad, 0.2f, 70);
            drops.erase(drops.begin() + i);
        }
        else if (drop.life <= 0.0f) drops.erase(drops.begin() + i);
    }
}

void Game::Impl::updateParticles(float dt)
{
    for (int i = static_cast<int>(particles.size()) - 1; i >= 0; --i)
    {
        Particle& particle = particles[i];
        particle.x += particle.vx * dt;
        particle.y += particle.vy * dt;
        const float friction = std::pow(0.04f, dt);
        particle.vx *= friction;
        particle.vy *= friction;
        particle.life -= dt;
        if (particle.life <= 0.0f) particles.erase(particles.begin() + i);
    }
}

void Game::Impl::checkGameOver()
{
    for (int i = 0; i < playerCount; ++i)
        if (players[i].hp > 0.0f) return;
    screen = Screen::GameOver;
    menuIndex = 0;
}

void Game::Impl::updatePlaying(float dt)
{
    for (int i = 0; i < playerCount; ++i)
    {
        if (pressed(players[i].pad, PAD_OPTIONS))
        {
            pausePad = players[i].pad;
            screen = Screen::Paused;
            menuIndex = 0;
            return;
        }
    }

    for (int i = 0; i < playerCount; ++i)
    {
        Player& player = players[i];
        if (player.hp <= 0.0f) continue;
        player.invincible = std::max(0.0f, player.invincible - dt);
        if (player.timeStop > 0.0f) player.timeStop = std::max(0.0f, player.timeStop - dt);
        else player.skillCooldown = std::max(0.0f, player.skillCooldown - dt);
        player.fireTimer = std::max(0.0f, player.fireTimer - dt);

        float moveX = axis(player.pad, 0);
        float moveY = axis(player.pad, 1);
        const float magnitude = std::sqrt(moveX * moveX + moveY * moveY);
        if (magnitude > 1.0f)
        {
            moveX /= magnitude;
            moveY /= magnitude;
        }
        player.x += moveX * 300.0f * dt;
        player.y += moveY * 300.0f * dt;
        resolveWalls(player.x, player.y, player.radius);
        player.x = clampf(player.x, mapMinX + player.radius, mapMaxX - player.radius);
        player.y = clampf(player.y, mapMinY + player.radius, mapMaxY - player.radius);
        player.cameraX += (player.x - player.cameraX) * std::min(1.0f, dt * 5.0f);
        player.cameraY += (player.y - player.cameraY) * std::min(1.0f, dt * 5.0f);

        if (player.fireTimer <= 0.0f && (!enemies.empty() || boss.active)) shoot(player, i);
        if (pressed(player.pad, PAD_R2) && player.skillCooldown <= 0.0f && player.timeStop <= 0.0f)
        {
            player.timeStop = player.skillDuration;
            player.skillCooldown = player.skillCooldownMax;
            addParticles(player.x, player.y, player.color, 44, 390.0f, 1.0f);
            rumble(player.pad, 0.6f, 500);
        }
        if (pressed(player.pad, PAD_L1)) buyUpgrade(player, 1);
        if (pressed(player.pad, PAD_R1)) buyUpgrade(player, 2);
        if (pressed(player.pad, PAD_TRIANGLE)) buyUpgrade(player, 3);
    }

    if (announcementTimer > 0.0f) announcementTimer -= dt;
    if (bossDelay > 0.0f)
    {
        bossDelay -= dt;
        if (bossDelay <= 0.0f) nextWave();
    }
    else if (!boss.active)
    {
        if (!anyTimeStop()) waveElapsed += dt;
        if (waveElapsed >= 25.0f)
        {
            if (enemies.empty()) nextWave();
        }
        else if (!anyTimeStop())
        {
            spawnTimer -= dt;
            const int cap = static_cast<int>(14 + wave * 1.5f + (playerCount - 1) * 6);
            if (spawnTimer <= 0.0f && static_cast<int>(enemies.size()) < cap)
            {
                spawnEnemy();
                const float factor = 1.0f + (playerCount - 1) * 0.30f;
                spawnTimer = std::max(0.08f, (0.52f - wave * 0.014f) / factor);
            }
        }
    }

    updateBoss(dt);
    handleProjectileCollisions(dt);
    handleEnemies(dt);
    handleDrops(dt);
    updateParticles(dt);
    cameraShake *= std::pow(0.001f, dt);
}

Vec2 Game::Impl::toScreen(const Viewport& viewport, const Player& cameraPlayer, float x, float y) const
{
    Vec2 result;
    result.x = viewport.x + viewport.w * 0.5f + x - cameraPlayer.cameraX;
    result.y = viewport.y + viewport.h * 0.5f + y - cameraPlayer.cameraY;
    if (cameraShake > 0.5f)
    {
        const float phase = lastTick * 0.037f;
        result.x += std::sin(phase) * cameraShake * 0.45f;
        result.y += std::cos(phase * 1.31f) * cameraShake * 0.45f;
    }
    return result;
}

void Game::Impl::renderWorldEntityCircle(const Viewport& viewport, const Player& cameraPlayer, float x, float y, float radius, Color colorValue, int glow)
{
    const Vec2 point = toScreen(viewport, cameraPlayer, x, y);
    if (point.x < viewport.x - radius - glow || point.x > viewport.x + viewport.w + radius + glow || point.y < viewport.y - radius - glow || point.y > viewport.y + viewport.h + radius + glow) return;
    draw::glowCircle(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(radius), colorValue, glow);
}

void Game::Impl::renderBackdrop()
{
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, BG);
    const float time = lastTick / 1000.0f;
    for (int band = 0; band < 8; ++band)
    {
        const int margin = band * 90;
        draw::fillRect(renderer, margin, margin / 2, SCREEN_W - margin * 2, SCREEN_H - margin, {4, static_cast<Uint8>(8 + band), static_cast<Uint8>(20 + band * 3), 30});
    }
    for (int i = 0; i < 100; ++i)
    {
        const int x = (i * 83 + 17) % 1919;
        const int y = (i * 173 + 71) % 1079;
        const int alpha = 50 + static_cast<int>((std::sin(time * (1.2f + (i % 5) * 0.4f) + i) + 1.0f) * 45.0f);
        const Color star = i % 7 == 0 ? withAlpha(PURPLE, alpha) : (i % 5 == 0 ? withAlpha(GOLD, alpha) : withAlpha(CYAN, alpha));
        draw::fillRect(renderer, x, y, i % 9 == 0 ? 3 : 2, i % 9 == 0 ? 3 : 2, star);
    }
}

void Game::Impl::renderMenuOptions(const std::vector<std::string>& options, int startY, int scale)
{
    for (unsigned i = 0; i < options.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == menuIndex;
        const int width = 720;
        const int height = 58;
        const int x = (SCREEN_W - width) / 2;
        const int y = startY + static_cast<int>(i) * 72;
        if (selected)
        {
            draw::fillRect(renderer, x, y, width, height, {0, 100, 200, 55});
            draw::outlineRect(renderer, x, y, width, height, CYAN, 2);
            draw::triangle(renderer, x + 18, y + height / 2, x + 36, y + 16, x + 36, y + height - 16, CYAN);
        }
        else draw::outlineRect(renderer, x, y, width, height, {80, 115, 155, 90}, 1);
        draw::text(renderer, options[i], SCREEN_W / 2, y + 17, scale, selected ? WHITE : MUTED, true);
    }
}

void Game::Impl::renderMenu()
{
    renderBackdrop();
    const std::string credits = "Criado por David Alerrandro - MXxtomxXy.";
    draw::text(renderer, credits, SCREEN_W - 28 - draw::textWidth(credits, 2), 28, 2, SILVER);
    draw::panel(renderer, 430, 105, 1060, 870, CYAN, PANEL);
    draw::text(renderer, "GEOMETRIC WARS", SCREEN_W / 2, 180, 8, CYAN, true);
    draw::text(renderer, "PS4 EDITION - NATIVO", SCREEN_W / 2, 265, 3, SILVER, true);
    renderMenuOptions({"JOGAR SOLO", "JOGAR LOCAL (CO-OP)", "LOJA DE COSMETICOS", "CONTROLES", "SAIR"}, 380, 4);
    if (controllerCount == 0)
        draw::text(renderer, "CONECTE UM CONTROLE", SCREEN_W / 2, 790, 3, RED, true);
    draw::text(renderer, "D-PAD NAVEGAR   X CONFIRMAR   O VOLTAR", SCREEN_W / 2, 910, 2, MUTED, true);
}

void Game::Impl::renderLobby()
{
    renderBackdrop();
    draw::panel(renderer, 330, 205, 1260, 670, CYAN, PANEL);
    draw::text(renderer, "SALA DE ESPERA", SCREEN_W / 2, 275, 6, CYAN, true);
    draw::text(renderer, "CO-OP LOCAL - ATE 4 JOGADORES", SCREEN_W / 2, 345, 3, SILVER, true);
    for (int i = 0; i < std::max(1, controllerCount); ++i)
    {
        const int total = std::max(1, controllerCount);
        const int cardW = 220;
        const int gap = 26;
        const int x = SCREEN_W / 2 - (total * cardW + (total - 1) * gap) / 2 + i * (cardW + gap);
        const int y = 470;
        draw::panel(renderer, x, y, cardW, 150, i < controllerCount ? PLAYER_COLORS[i] : RED, {0, 0, 0, 190});
        draw::text(renderer, "P" + number(i + 1), x + cardW / 2, y + 30, 5, i < controllerCount ? PLAYER_COLORS[i] : RED, true);
        draw::text(renderer, i < controllerCount ? "PRONTO" : "AUSENTE", x + cardW / 2, y + 95, 2, i < controllerCount ? GREEN : RED, true);
    }
    if (controllerCount >= 2)
    {
        draw::text(renderer, "INICIANDO EM " + number(std::max(1, static_cast<int>(std::ceil(lobbyTimer)))) + "...", SCREEN_W / 2, 690, 4, GREEN, true);
        draw::text(renderer, "X INICIAR AGORA", SCREEN_W / 2, 750, 2, MUTED, true);
    }
    else draw::text(renderer, "CONECTE NO MINIMO 2 CONTROLES", SCREEN_W / 2, 700, 3, RED, true);
    draw::text(renderer, "O VOLTAR", SCREEN_W / 2, 815, 2, MUTED, true);
}

void Game::Impl::renderControls()
{
    renderBackdrop();
    draw::panel(renderer, 380, 120, 1160, 840, GOLD, PANEL);
    draw::text(renderer, "CONTROLES", SCREEN_W / 2, 180, 6, GOLD, true);
    const int left = 560;
    const int right = 960;
    const int y = 310;
    draw::text(renderer, "ANALOGICO ESQUERDO", left, y, 3, CYAN, true);
    draw::text(renderer, "MOVIMENTO", right + 170, y, 3, WHITE, true);
    draw::text(renderer, "TIRO", left, y + 90, 3, CYAN, true);
    draw::text(renderer, "AUTOMATICO NO ALVO", right + 170, y + 90, 3, WHITE, true);
    draw::text(renderer, "R2", left, y + 180, 3, CYAN, true);
    draw::text(renderer, "PARAR O TEMPO", right + 170, y + 180, 3, WHITE, true);
    draw::text(renderer, "L1 / R1", left, y + 270, 3, CYAN, true);
    draw::text(renderer, "UPGRADE TIRO / DANO", right + 170, y + 270, 3, WHITE, true);
    draw::text(renderer, "TRIANGULO", left, y + 360, 3, CYAN, true);
    draw::text(renderer, "UPGRADE HABILIDADE", right + 170, y + 360, 3, WHITE, true);
    draw::text(renderer, "OPTIONS", left, y + 450, 3, CYAN, true);
    draw::text(renderer, "PAUSAR", right + 170, y + 450, 3, WHITE, true);
    draw::text(renderer, "X OU O PARA VOLTAR", SCREEN_W / 2, 865, 2, MUTED, true);
}

void Game::Impl::renderShop()
{
    renderBackdrop();
    draw::panel(renderer, 350, 160, 1220, 760, PURPLE, PANEL);
    draw::text(renderer, "LOJA DE COSMETICOS", SCREEN_W / 2, 225, 6, PURPLE, true);
    draw::text(renderer, "PRATA DO PERFIL: " + number(profileSilver), SCREEN_W / 2, 320, 3, SILVER, true);
    draw::glowCircle(renderer, SCREEN_W / 2, 525, 86, PLAYER_COLORS[0], 22);
    draw::circle(renderer, SCREEN_W / 2, 525, 55, WHITE);
    draw::text(renderer, "NUCLEO PADRAO EQUIPADO", SCREEN_W / 2, 655, 3, WHITE, true);
    draw::text(renderer, "CATALOGO COMPLETO NA PROXIMA ETAPA", SCREEN_W / 2, 735, 2, GOLD, true);
    draw::text(renderer, "X OU O PARA VOLTAR", SCREEN_W / 2, 850, 2, MUTED, true);
}

void Game::Impl::renderPause()
{
    renderPlaying();
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, {0, 0, 8, 185});
    draw::panel(renderer, 520, 225, 880, 630, CYAN, PANEL);
    draw::text(renderer, "PAUSADO", SCREEN_W / 2, 300, 7, CYAN, true);
    renderMenuOptions({"RETOMAR", "CONTROLES", "SAIR AO MENU"}, 470, 4);
    draw::text(renderer, "JOGADOR " + number(pausePad + 1), SCREEN_W / 2, 780, 2, GOLD, true);
}

void Game::Impl::renderGameOver()
{
    renderBackdrop();
    draw::panel(renderer, 470, 205, 980, 670, RED, PANEL);
    draw::text(renderer, "SISTEMA FALHOU", SCREEN_W / 2, 290, 7, RED, true);
    int finalScore = 0;
    for (int i = 0; i < playerCount; ++i) finalScore += players[i].score;
    draw::text(renderer, "ONDA MAXIMA: " + number(wave), SCREEN_W / 2, 440, 4, CYAN, true);
    draw::text(renderer, "PONTUACAO: " + number(finalScore), SCREEN_W / 2, 520, 4, GOLD, true);
    renderMenuOptions({"VOLTAR AO MENU"}, 675, 4);
}

void Game::Impl::renderEnemy(const Viewport& viewport, const Player& cameraPlayer, const Enemy& enemy)
{
    const Vec2 point = toScreen(viewport, cameraPlayer, enemy.x, enemy.y);
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    const int radius = static_cast<int>(enemy.radius);
    if (enemy.edges == 3)
    {
        draw::triangle(renderer, x, y - radius, x - radius, y + radius, x + radius, y + radius, enemy.color);
    }
    else if (enemy.edges == 4 || enemy.edges == 0)
    {
        draw::fillRect(renderer, x - radius, y - radius, radius * 2, radius * 2, withAlpha(enemy.color, 45));
        draw::outlineRect(renderer, x - radius, y - radius, radius * 2, radius * 2, enemy.color, 3);
    }
    else renderWorldEntityCircle(viewport, cameraPlayer, enemy.x, enemy.y, enemy.radius, enemy.color, 9);

    if (enemy.hp < enemy.maxHp)
    {
        const int width = radius * 2;
        draw::fillRect(renderer, x - radius, y - radius - 11, width, 4, {0, 0, 0, 210});
        draw::fillRect(renderer, x - radius, y - radius - 11, static_cast<int>(width * std::max(0.0f, enemy.hp / enemy.maxHp)), 4, GREEN);
    }
}

void Game::Impl::renderBoss(const Viewport& viewport, const Player& cameraPlayer)
{
    if (!boss.active) return;
    const Vec2 point = toScreen(viewport, cameraPlayer, boss.x, boss.y);
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    const int radius = static_cast<int>(boss.radius);
    const Color body = boss.color;
    if (boss.phase == 1 || boss.shape == 2) draw::glowCircle(renderer, x, y, radius, body, 20);
    else if (boss.shape == 0) draw::triangle(renderer, x, y - radius, x - radius, y + radius, x + radius, y + radius, body);
    else if (boss.shape == 1)
    {
        draw::fillRect(renderer, x - radius, y - radius, radius * 2, radius * 2, withAlpha(body, 65));
        draw::outlineRect(renderer, x - radius, y - radius, radius * 2, radius * 2, body, 5);
    }
    else
    {
        draw::fillRect(renderer, x - radius / 3, y - radius, radius * 2 / 3, radius * 2, body);
        draw::fillRect(renderer, x - radius, y - radius / 3, radius * 2, radius * 2 / 3, body);
    }
    draw::fillCircle(renderer, x - 12, y - 15, 13, {255, 255, 255, 40});
    if (boss.phase == 1)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!boss.weapons[i].active) continue;
            float worldX, worldY;
            bossWeaponPosition(i, worldX, worldY);
            const Vec2 weaponPoint = toScreen(viewport, cameraPlayer, worldX, worldY);
            draw::glowCircle(renderer, static_cast<int>(weaponPoint.x), static_cast<int>(weaponPoint.y), 22, ORANGE, 12);
            const int barWidth = 44;
            draw::fillRect(renderer, static_cast<int>(weaponPoint.x) - 22, static_cast<int>(weaponPoint.y) - 34, barWidth, 5, {0, 0, 0, 220});
            draw::fillRect(renderer, static_cast<int>(weaponPoint.x) - 22, static_cast<int>(weaponPoint.y) - 34, static_cast<int>(barWidth * boss.weapons[i].hp / boss.weapons[i].maxHp), 5, RED);
        }
    }
}

void Game::Impl::renderPlayer(const Viewport& viewport, const Player& cameraPlayer, const Player& player, int index)
{
    const Vec2 point = toScreen(viewport, cameraPlayer, player.x, player.y);
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    if (player.hp <= 0.0f)
    {
        draw::circle(renderer, x, y, 25, withAlpha(player.color, 130));
        draw::line(renderer, x - 12, y - 12, x + 12, y + 12, player.color, 3);
        draw::line(renderer, x + 12, y - 12, x - 12, y + 12, player.color, 3);
        return;
    }
    if (player.invincible > 0.0f && (static_cast<int>(lastTick / 60) % 2 == 0)) return;
    draw::glowCircle(renderer, x, y, static_cast<int>(player.radius), player.color, 16);
    draw::circle(renderer, x, y, static_cast<int>(player.radius - 6), withAlpha(WHITE, 150));
    draw::fillCircle(renderer, x, y - static_cast<int>(player.radius) + 4, 3, WHITE);
    if (player.timeStop > 0.0f) draw::circle(renderer, x, y, static_cast<int>(player.radius + 9), CYAN);
    if (playerCount > 1) draw::text(renderer, "P" + number(index + 1), x, y + 27, 1, player.color, true);
}

void Game::Impl::renderHud(const Viewport& viewport, const Player& player, int index)
{
    const int padding = 18;
    const int x = viewport.x + padding;
    const int y = viewport.y + padding;
    const int width = std::min(350, viewport.w - 36);
    draw::fillRect(renderer, x, y, width, 96, {0, 4, 16, 185});
    draw::outlineRect(renderer, x, y, width, 96, withAlpha(player.color, 110), 1);
    draw::text(renderer, "JOGADOR " + number(index + 1), x + 12, y + 10, 2, player.color);
    draw::text(renderer, "K " + number(player.kills) + "  G " + number(player.coins) + "  S " + number(profileSilver), x + 12, y + 39, 2, WHITE);
    draw::fillRect(renderer, x + 12, y + 70, width - 24, 10, {30, 35, 50, 220});
    draw::fillRect(renderer, x + 12, y + 70, static_cast<int>((width - 24) * std::max(0.0f, player.hp / player.maxHp)), 10, PINK);

    if (viewport.w >= 700)
    {
        const int shopX = viewport.x + viewport.w - 360;
        draw::fillRect(renderer, shopX, y, 340, 96, {0, 4, 16, 185});
        draw::text(renderer, "UPGRADES", shopX + 12, y + 10, 2, GOLD);
        draw::text(renderer, "L1 TIRO " + number(player.fireCost), shopX + 12, y + 39, 2, WHITE);
        draw::text(renderer, "R1 DANO " + number(player.damageCost) + "  TRI HAB " + number(player.skillCost), shopX + 12, y + 66, 1, WHITE);
    }
}

void Game::Impl::renderViewport(const Viewport& viewport, const Player& cameraPlayer, int playerIndex)
{
    draw::setClipRect(viewport.x, viewport.y, viewport.w, viewport.h);
    draw::fillRect(renderer, viewport.x, viewport.y, viewport.w, viewport.h, BG_BLUE);

    const int grid = 80;
    const float leftWorld = cameraPlayer.cameraX - viewport.w * 0.5f;
    const float topWorld = cameraPlayer.cameraY - viewport.h * 0.5f;
    const int firstX = static_cast<int>(std::floor(leftWorld / grid)) * grid;
    const int firstY = static_cast<int>(std::floor(topWorld / grid)) * grid;
    for (int worldX = firstX; worldX <= leftWorld + viewport.w; worldX += grid)
    {
        const Vec2 a = toScreen(viewport, cameraPlayer, static_cast<float>(worldX), topWorld);
        draw::line(renderer, static_cast<int>(a.x), viewport.y, static_cast<int>(a.x), viewport.y + viewport.h, {0, 180, 255, 18});
    }
    for (int worldY = firstY; worldY <= topWorld + viewport.h; worldY += grid)
    {
        const Vec2 a = toScreen(viewport, cameraPlayer, leftWorld, static_cast<float>(worldY));
        draw::line(renderer, viewport.x, static_cast<int>(a.y), viewport.x + viewport.w, static_cast<int>(a.y), {0, 180, 255, 18});
    }

    const Vec2 mapTopLeft = toScreen(viewport, cameraPlayer, mapMinX, mapMinY);
    draw::outlineRect(renderer, static_cast<int>(mapTopLeft.x), static_cast<int>(mapTopLeft.y), static_cast<int>(mapMaxX - mapMinX), static_cast<int>(mapMaxY - mapMinY), withAlpha(CYAN, 90), 3);

    for (unsigned i = 0; i < walls.size(); ++i)
    {
        const Vec2 point = toScreen(viewport, cameraPlayer, walls[i].x, walls[i].y);
        draw::fillRect(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(walls[i].w), static_cast<int>(walls[i].h), {20, 60, 120, 150});
        draw::outlineRect(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(walls[i].w), static_cast<int>(walls[i].h), CYAN_DIM, 2);
    }

    for (unsigned i = 0; i < drops.size(); ++i)
    {
        const float bob = std::sin(drops[i].phase) * 3.0f;
        const Color dropColor = drops[i].type == DropType::Heart ? PINK : (drops[i].type == DropType::Silver ? SILVER : GOLD);
        renderWorldEntityCircle(viewport, cameraPlayer, drops[i].x, drops[i].y + bob, drops[i].type == DropType::Heart ? 12.0f : 8.0f, dropColor, 10);
    }
    for (unsigned i = 0; i < particles.size(); ++i)
    {
        const Particle& particle = particles[i];
        const Vec2 point = toScreen(viewport, cameraPlayer, particle.x, particle.y);
        Color particleColor = particle.color;
        particleColor.a = static_cast<Uint8>(255.0f * clampf(particle.life / particle.maxLife, 0.0f, 1.0f));
        draw::fillRect(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(particle.size), static_cast<int>(particle.size), particleColor);
    }
    for (unsigned i = 0; i < projectiles.size(); ++i)
        renderWorldEntityCircle(viewport, cameraPlayer, projectiles[i].x, projectiles[i].y, projectiles[i].radius, projectiles[i].color, 8);
    for (unsigned i = 0; i < enemyProjectiles.size(); ++i)
        renderWorldEntityCircle(viewport, cameraPlayer, enemyProjectiles[i].x, enemyProjectiles[i].y, enemyProjectiles[i].radius, enemyProjectiles[i].color, 8);
    for (unsigned i = 0; i < enemies.size(); ++i) renderEnemy(viewport, cameraPlayer, enemies[i]);
    renderBoss(viewport, cameraPlayer);
    for (int i = 0; i < playerCount; ++i) renderPlayer(viewport, cameraPlayer, players[i], i);

    if (anyTimeStop()) draw::fillRect(renderer, viewport.x, viewport.y, viewport.w, viewport.h, {0, 100, 220, 22});
    renderHud(viewport, players[playerIndex], playerIndex);
    draw::clearClipRect();
}

void Game::Impl::renderPlaying()
{
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, BG);
    if (playerCount == 1)
        renderViewport({0, 0, SCREEN_W, SCREEN_H}, players[0], 0);
    else if (playerCount == 2)
    {
        renderViewport({0, 0, SCREEN_W / 2, SCREEN_H}, players[0], 0);
        renderViewport({SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H}, players[1], 1);
        draw::line(renderer, SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H, withAlpha(WHITE, 70), 3);
    }
    else
    {
        const int halfW = SCREEN_W / 2;
        const int halfH = SCREEN_H / 2;
        renderViewport({0, 0, halfW, halfH}, players[0], 0);
        renderViewport({halfW, 0, halfW, halfH}, players[1], 1);
        renderViewport({0, halfH, halfW, halfH}, players[2], 2);
        if (playerCount >= 4) renderViewport({halfW, halfH, halfW, halfH}, players[3], 3);
        else draw::fillRect(renderer, halfW, halfH, halfW, halfH, BG);
        draw::line(renderer, halfW, 0, halfW, SCREEN_H, withAlpha(WHITE, 70), 3);
        draw::line(renderer, 0, halfH, SCREEN_W, halfH, withAlpha(WHITE, 70), 3);
    }

    draw::panel(renderer, SCREEN_W / 2 - 95, 12, 190, 42, CYAN, {0, 5, 20, 190});
    draw::text(renderer, "ONDA " + number(wave), SCREEN_W / 2, 25, 2, CYAN, true);

    if (boss.active)
    {
        float ratio = 0.0f;
        if (boss.phase == 1)
        {
            float hp = 0.0f;
            float maxHp = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                hp += boss.weapons[i].hp;
                maxHp += boss.weapons[i].maxHp;
            }
            ratio = maxHp > 0.0f ? hp / maxHp : 0.0f;
        }
        else if (boss.phase == 2) ratio = boss.hp / boss.maxHp;
        else ratio = boss.rageHp / boss.maxRageHp;
        draw::fillRect(renderer, 600, 70, 720, 42, {0, 0, 0, 205});
        draw::outlineRect(renderer, 600, 70, 720, 42, RED, 2);
        draw::fillRect(renderer, 610, 82, static_cast<int>(700 * clampf(ratio, 0.0f, 1.0f)), 18, boss.phase == 3 ? RED : boss.trueColor);
        draw::text(renderer, "BOSS FASE " + number(boss.phase), SCREEN_W / 2, 119, 2, RED, true);
    }

    if (announcementTimer > 0.0f)
    {
        const bool bossWave = wave % 5 == 0;
        const std::string title = bossWave ? "BOSS APROXIMANDO" : "ONDA " + number(wave);
        draw::fillRect(renderer, 0, SCREEN_H / 2 - 90, SCREEN_W, 180, {0, 0, 10, 150});
        draw::text(renderer, title, SCREEN_W / 2, SCREEN_H / 2 - 32, 7, bossWave ? RED : CYAN, true);
    }
}

void Game::Impl::render()
{
    switch (screen)
    {
        case Screen::Menu: renderMenu(); break;
        case Screen::Lobby: renderLobby(); break;
        case Screen::Playing: renderPlaying(); break;
        case Screen::Paused: renderPause(); break;
        case Screen::Controls: renderControls(); break;
        case Screen::Shop: renderShop(); break;
        case Screen::GameOver: renderGameOver(); break;
    }
    draw::text(renderer, "FPS " + number(fpsValue), 16, SCREEN_H - 30, 2, fpsValue >= 55 ? GREEN : RED);
}

Game::Game(SDL_Renderer* renderer) : impl_(new Impl(renderer))
{
}

Game::~Game()
{
    delete impl_;
}

bool Game::initialize()
{
    return impl_->initialize();
}

void Game::tick(uint32_t now)
{
    impl_->tick(now);
}

bool Game::running() const
{
    return impl_->active;
}
}
