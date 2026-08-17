#include "game.hpp"

#include "draw.hpp"

#include <orbis/Pad.h>
#include <orbis/UserService.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
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
    PAD_SHARE = 8,
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

enum Action
{
    ActionPause = 0,
    ActionSkill,
    ActionUpgradeFire,
    ActionUpgradeDamage,
    ActionUpgradeSkill,
    ActionCount
};

enum class GraphicsQuality
{
    High = 0,
    Medium,
    Low
};

enum class ShopTab
{
    Skins = 0,
    Hats
};

struct CosmeticItem
{
    const char* name;
    int cost;
};

const CosmeticItem SKINS[] = {
    {"NUCLEO PADRAO", 0}, {"ANEL NEON", 12}, {"AURA VIVA", 20},
    {"ESCUDO ION", 30}, {"PUAS ROTATIVAS", 38}, {"FANTASMA", 48},
    {"TRAJE PLASMA", 58}, {"MALHA MATRIX", 68}, {"BRASA VIVA", 80},
    {"CRISTAL AZUL", 95}, {"VEU DO VAZIO", 110}, {"ESPECTRO RGB", 130}
};

const CosmeticItem HATS[] = {
    {"SEM ACESSORIO", 0}, {"VISOR CYBER", 10}, {"BONE NEON", 16},
    {"ANTENA", 22}, {"COROA", 30}, {"CARTOLA", 36},
    {"HEADSET", 44}, {"HALO", 54}, {"CHIFRES", 62},
    {"CAPACETE", 72}, {"FLOR COSMICA", 84}, {"ANEL CELESTE", 100}
};

const int COSMETIC_COUNT = 12;

struct PlayerSettings
{
    int keys[ActionCount];
    float sensitivity;
    float rumble;
};

struct ProfileDisk
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    int32_t silver;
    int32_t graphics;
    uint32_t ownedSkins;
    uint32_t ownedHats;
    uint8_t activeSkin[4];
    uint8_t activeHat[4];
    int8_t keys[4][ActionCount];
    float sensitivity[4];
    float rumble[4];
    uint8_t reserved[64];
};

const uint32_t PROFILE_MAGIC = 0x47575034u;
const uint32_t PROFILE_VERSION = 2u;
const char* PROFILE_DIRECTORY = "/data/GEOM00001";
const char* PROFILE_PATH = "/data/GEOM00001/profile.bin";
const char* PROFILE_TEMP_PATH = "/data/GEOM00001/profile.tmp";

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
    int handle;
    int userId;
    bool ownsHandle;
    bool connected;
    bool current[PAD_BUTTON_COUNT];
    bool previous[PAD_BUTTON_COUNT];
    float axes[4];

    Pad() : handle(-1), userId(-1), ownsHandle(false), connected(false)
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
    int keys[ActionCount];
    float sensitivity;
    float rumbleStrength;
    int skin;
    int hat;
    float cosmeticAnimation;
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
    Vec2 trail[5];
    int trailCount;
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

struct FloatingText
{
    float x;
    float y;
    float life;
    float velocityY;
    std::string text;
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

std::string oneDecimal(float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", value);
    return std::string(buffer);
}

Color withAlpha(Color value, int alpha)
{
    value.a = static_cast<Uint8>(clampf(static_cast<float>(alpha), 0.0f, 255.0f));
    return value;
}

const char* buttonName(int button)
{
    switch (button)
    {
        case PAD_CROSS: return "CRUZ";
        case PAD_CIRCLE: return "CIRCULO";
        case PAD_SQUARE: return "QUADRADO";
        case PAD_TRIANGLE: return "TRIANGULO";
        case PAD_L1: return "L1";
        case PAD_R1: return "R1";
        case PAD_SHARE: return "SHARE";
        case PAD_OPTIONS: return "OPTIONS";
        case PAD_L3: return "L3";
        case PAD_R3: return "R3";
        case PAD_UP: return "CIMA";
        case PAD_DOWN: return "BAIXO";
        case PAD_LEFT: return "ESQUERDA";
        case PAD_RIGHT: return "DIREITA";
        case PAD_TOUCH: return "TOUCH";
        case PAD_L2: return "L2";
        case PAD_R2: return "R2";
        default: return "BOTAO";
    }
}

bool nativeButton(uint32_t buttons, int button)
{
    switch (button)
    {
        case PAD_CROSS: return (buttons & ORBIS_PAD_BUTTON_CROSS) != 0;
        case PAD_CIRCLE: return (buttons & ORBIS_PAD_BUTTON_CIRCLE) != 0;
        case PAD_SQUARE: return (buttons & ORBIS_PAD_BUTTON_SQUARE) != 0;
        case PAD_TRIANGLE: return (buttons & ORBIS_PAD_BUTTON_TRIANGLE) != 0;
        case PAD_L1: return (buttons & ORBIS_PAD_BUTTON_L1) != 0;
        case PAD_R1: return (buttons & ORBIS_PAD_BUTTON_R1) != 0;
        case PAD_SHARE: return (buttons & 0x0001u) != 0;
        case PAD_OPTIONS: return (buttons & ORBIS_PAD_BUTTON_OPTIONS) != 0;
        case PAD_L3: return (buttons & ORBIS_PAD_BUTTON_L3) != 0;
        case PAD_R3: return (buttons & ORBIS_PAD_BUTTON_R3) != 0;
        case PAD_UP: return (buttons & ORBIS_PAD_BUTTON_UP) != 0;
        case PAD_DOWN: return (buttons & ORBIS_PAD_BUTTON_DOWN) != 0;
        case PAD_LEFT: return (buttons & ORBIS_PAD_BUTTON_LEFT) != 0;
        case PAD_RIGHT: return (buttons & ORBIS_PAD_BUTTON_RIGHT) != 0;
        case PAD_TOUCH: return (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) != 0;
        case PAD_L2: return (buttons & ORBIS_PAD_BUTTON_L2) != 0;
        case PAD_R2: return (buttons & ORBIS_PAD_BUTTON_R2) != 0;
        default: return false;
    }
}

float nativeAxis(uint8_t value)
{
    return clampf((static_cast<int>(value) - 128) / 127.0f, -1.0f, 1.0f);
}

uint32_t profileChecksum(const ProfileDisk& profile)
{
    ProfileDisk copy = profile;
    copy.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&copy);
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < sizeof(copy); ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
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
          enemyColor(RED), graphicsQuality(GraphicsQuality::High), ownedSkins(1u), ownedHats(1u),
          shopTab(ShopTab::Skins), controlIndex(0), bindingAction(-1), noticeTimer(0),
          resetConfirmTimer(0), profileDirty(false), disconnectedPlayers(0), controllerRefreshTick(0)
    {
        std::memset(&boss, 0, sizeof(boss));
        std::memset(nativePadHandles, -1, sizeof(nativePadHandles));
        std::memset(rumbleTimers, 0, sizeof(rumbleTimers));
        std::memset(activeSkins, 0, sizeof(activeSkins));
        std::memset(activeHats, 0, sizeof(activeHats));
        resetSettings();
    }

    ~Impl()
    {
        for (int i = 0; i < 4; ++i)
        {
            if (nativePadHandles[i] >= 0)
            {
                OrbisPadVibeParam stop = {0, 0};
                scePadSetVibration(nativePadHandles[i], &stop);
            }
            if (pads[i].handle >= 0 && pads[i].ownsHandle) scePadClose(pads[i].handle);
        }
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
    GraphicsQuality graphicsQuality;
    uint32_t ownedSkins;
    uint32_t ownedHats;
    uint8_t activeSkins[4];
    uint8_t activeHats[4];
    PlayerSettings settings[4];
    int nativePadHandles[4];
    float rumbleTimers[4];
    ShopTab shopTab;
    int controlIndex;
    int bindingAction;
    float noticeTimer;
    std::string noticeText;
    float resetConfirmTimer;
    bool profileDirty;
    uint32_t disconnectedPlayers;
    uint32_t controllerRefreshTick;

    std::vector<Projectile> projectiles;
    std::vector<Projectile> enemyProjectiles;
    std::vector<Enemy> enemies;
    std::vector<Drop> drops;
    std::vector<Particle> particles;
    std::vector<FloatingText> floatingTexts;
    std::vector<Wall> walls;
    Boss boss;

    bool initialize();
    void resetSettings();
    void resetProfile(bool persist);
    bool loadProfile();
    bool saveProfile(bool showNotice);
    void showNotice(const std::string& text);
    void updateNativePad(float dt);
    void refreshControllers();
    void sampleInput();
    int firstConnectedPad() const;
    void updateDisconnectedPlayers();
    bool down(int pad, int button) const;
    bool pressed(int pad, int button) const;
    float axis(int pad, int index) const;
    void rumble(int pad, float strength, uint32_t milliseconds);
    uint32_t randomNext();
    float random01();
    int randomInt(int minimum, int maximum);
    void tick(uint32_t now);
    void updateMenu(float dt);
    void updateControls(float dt);
    void updateShop(float dt);
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
    void addFloatingText(float x, float y, const std::string& text, Color color);
    void addDrop(float x, float y, DropType type);
    void damagePlayer(Player& player, float damage);
    void buyUpgrade(Player& player, int type);
    void resolveWalls(float& x, float& y, float radius);
    void handleProjectileCollisions(float dt);
    void handleEnemies(float dt);
    void handleDrops(float dt);
    void updateParticles(float dt);
    void updateFloatingTexts(float dt);
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
    void renderAvatar(int x, int y, int radius, Color color, int skin, int hat, float animation, bool dead);
    void renderFloatingTexts(const Viewport& viewport, const Player& cameraPlayer);
    void renderHud(const Viewport& viewport, const Player& player, int index);
    void renderMenuOptions(const std::vector<std::string>& options, int startY, int scale);
    int qualityGlow(int value) const;
    int qualityStars() const;
    int qualityParticleStep() const;
    int qualityParticleLimit() const;
};

void Game::Impl::resetSettings()
{
    for (int i = 0; i < 4; ++i)
    {
        settings[i].keys[ActionPause] = PAD_OPTIONS;
        settings[i].keys[ActionSkill] = PAD_R2;
        settings[i].keys[ActionUpgradeFire] = PAD_L1;
        settings[i].keys[ActionUpgradeDamage] = PAD_R1;
        settings[i].keys[ActionUpgradeSkill] = PAD_TRIANGLE;
        settings[i].sensitivity = 1.0f;
        settings[i].rumble = 1.0f;
    }
}

void Game::Impl::showNotice(const std::string& textValue)
{
    noticeText = textValue;
    noticeTimer = 1.8f;
}

void Game::Impl::resetProfile(bool persist)
{
    profileSilver = 0;
    ownedSkins = 1u;
    ownedHats = 1u;
    std::memset(activeSkins, 0, sizeof(activeSkins));
    std::memset(activeHats, 0, sizeof(activeHats));
    graphicsQuality = GraphicsQuality::High;
    resetSettings();
    resetConfirmTimer = 0.0f;
    for (int i = 0; i < playerCount; ++i)
    {
        const int slot = std::max(0, std::min(3, players[i].pad));
        players[i].skin = 0;
        players[i].hat = 0;
        players[i].sensitivity = settings[slot].sensitivity;
        players[i].rumbleStrength = settings[slot].rumble;
        for (int action = 0; action < ActionCount; ++action) players[i].keys[action] = settings[slot].keys[action];
    }
    if (persist) saveProfile(false);
    showNotice("PERFIL RESETADO");
}

bool Game::Impl::loadProfile()
{
    FILE* file = std::fopen(PROFILE_PATH, "rb");
    if (!file) return false;
    ProfileDisk profile;
    const size_t read = std::fread(&profile, 1, sizeof(profile), file);
    std::fclose(file);
    if (read != sizeof(profile) || profile.magic != PROFILE_MAGIC || profile.version != PROFILE_VERSION || profile.checksum != profileChecksum(profile))
        return false;

    profileSilver = std::max(0, profile.silver);
    graphicsQuality = profile.graphics >= 0 && profile.graphics <= 2 ? static_cast<GraphicsQuality>(profile.graphics) : GraphicsQuality::High;
    ownedSkins = profile.ownedSkins | 1u;
    ownedHats = profile.ownedHats | 1u;
    const uint32_t validMask = (1u << COSMETIC_COUNT) - 1u;
    ownedSkins &= validMask;
    ownedHats &= validMask;
    for (int i = 0; i < 4; ++i)
    {
        activeSkins[i] = profile.activeSkin[i] < COSMETIC_COUNT && (ownedSkins & (1u << profile.activeSkin[i])) ? profile.activeSkin[i] : 0;
        activeHats[i] = profile.activeHat[i] < COSMETIC_COUNT && (ownedHats & (1u << profile.activeHat[i])) ? profile.activeHat[i] : 0;
        for (int action = 0; action < ActionCount; ++action)
        {
            const int key = profile.keys[i][action];
            if (key >= 0 && key < PAD_BUTTON_COUNT) settings[i].keys[action] = key;
        }
        settings[i].sensitivity = clampf(profile.sensitivity[i], 0.5f, 3.0f);
        settings[i].rumble = clampf(profile.rumble[i], 0.0f, 2.0f);
    }
    return true;
}

bool Game::Impl::saveProfile(bool show)
{
    ProfileDisk profile;
    std::memset(&profile, 0, sizeof(profile));
    profile.magic = PROFILE_MAGIC;
    profile.version = PROFILE_VERSION;
    profile.silver = profileSilver;
    profile.graphics = static_cast<int32_t>(graphicsQuality);
    profile.ownedSkins = ownedSkins;
    profile.ownedHats = ownedHats;
    for (int i = 0; i < 4; ++i)
    {
        profile.activeSkin[i] = activeSkins[i];
        profile.activeHat[i] = activeHats[i];
        for (int action = 0; action < ActionCount; ++action) profile.keys[i][action] = static_cast<int8_t>(settings[i].keys[action]);
        profile.sensitivity[i] = settings[i].sensitivity;
        profile.rumble[i] = settings[i].rumble;
    }
    profile.checksum = profileChecksum(profile);

    mkdir(PROFILE_DIRECTORY, 0777);
    FILE* file = std::fopen(PROFILE_TEMP_PATH, "wb");
    if (!file)
    {
        if (show) showNotice("ERRO AO SALVAR");
        return false;
    }
    const bool written = std::fwrite(&profile, 1, sizeof(profile), file) == sizeof(profile);
    const bool closed = std::fclose(file) == 0;
    if (!written || !closed)
    {
        std::remove(PROFILE_TEMP_PATH);
        if (show) showNotice("ERRO AO SALVAR");
        return false;
    }
    std::remove(PROFILE_PATH);
    if (std::rename(PROFILE_TEMP_PATH, PROFILE_PATH) != 0)
    {
        std::remove(PROFILE_TEMP_PATH);
        if (show) showNotice("ERRO AO SALVAR");
        return false;
    }
    profileDirty = false;
    if (show) showNotice("PERFIL SALVO");
    return true;
}

int Game::Impl::qualityGlow(int value) const
{
    if (graphicsQuality == GraphicsQuality::Low) return std::max(1, value / 5);
    if (graphicsQuality == GraphicsQuality::Medium) return std::max(1, value * 3 / 5);
    return value;
}

int Game::Impl::qualityStars() const
{
    return graphicsQuality == GraphicsQuality::High ? 90 : (graphicsQuality == GraphicsQuality::Medium ? 28 : 0);
}

int Game::Impl::qualityParticleStep() const
{
    return graphicsQuality == GraphicsQuality::High ? 1 : (graphicsQuality == GraphicsQuality::Medium ? 2 : 3);
}

int Game::Impl::qualityParticleLimit() const
{
    return graphicsQuality == GraphicsQuality::High ? 600 : (graphicsQuality == GraphicsQuality::Medium ? 260 : 130);
}

bool Game::Impl::initialize()
{
    loadProfile();
    sceUserServiceInitialize(nullptr);
    scePadInit();
    const uint32_t inputNow = SDL_GetTicks();
    if (controllerRefreshTick == 0 || inputNow - controllerRefreshTick >= 100)
    {
        refreshControllers();
        controllerRefreshTick = inputNow;
    }
    return true;
}

void Game::Impl::refreshControllers()
{
    OrbisUserServiceLoginUserIdList users;
    std::memset(&users, 0xff, sizeof(users));
    if (sceUserServiceGetLoginUserIdList(&users) < 0) return;

    int desiredUsers[4] = {-1, -1, -1, -1};
    int desiredCount = 0;
    for (int i = 0; i < 4 && desiredCount < 4; ++i)
        if (users.userId[i] >= 0) desiredUsers[desiredCount++] = users.userId[i];

    for (int i = 0; i < 4; ++i)
    {
        const int desired = desiredUsers[i];
        if (pads[i].userId != desired)
        {
            if (pads[i].handle >= 0 && pads[i].ownsHandle) scePadClose(pads[i].handle);
            pads[i] = Pad();
            nativePadHandles[i] = -1;
            pads[i].userId = desired;
        }
        if (desired >= 0 && pads[i].handle < 0)
        {
            pads[i].handle = scePadOpen(desired, ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
            pads[i].ownsHandle = pads[i].handle >= 0;
            if (pads[i].handle < 0)
            {
                pads[i].handle = scePadGetHandle(desired, ORBIS_PAD_PORT_TYPE_STANDARD, 0);
                pads[i].ownsHandle = false;
            }
            nativePadHandles[i] = pads[i].handle;
        }
    }
}

void Game::Impl::sampleInput()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT) active = false;

    const uint32_t inputNow = SDL_GetTicks();
    if (controllerRefreshTick == 0 || inputNow - controllerRefreshTick >= 100)
    {
        refreshControllers();
        controllerRefreshTick = inputNow;
    }
    controllerCount = 0;
    for (int p = 0; p < 4; ++p)
    {
        for (int button = 0; button < PAD_BUTTON_COUNT; ++button)
        {
            pads[p].previous[button] = pads[p].current[button];
            pads[p].current[button] = false;
        }
        std::memset(pads[p].axes, 0, sizeof(pads[p].axes));
        pads[p].connected = false;
        if (pads[p].handle < 0) continue;

        OrbisPadData data;
        std::memset(&data, 0, sizeof(data));
        if (scePadReadState(pads[p].handle, &data) < 0 || !data.connected) continue;
        pads[p].connected = true;
        ++controllerCount;
        for (int button = 0; button < PAD_BUTTON_COUNT; ++button) pads[p].current[button] = nativeButton(data.buttons, button);
        pads[p].axes[0] = nativeAxis(data.leftStick.x);
        pads[p].axes[1] = nativeAxis(data.leftStick.y);
        pads[p].axes[2] = nativeAxis(data.rightStick.x);
        pads[p].axes[3] = nativeAxis(data.rightStick.y);
    }
    updateDisconnectedPlayers();
}

int Game::Impl::firstConnectedPad() const
{
    for (int i = 0; i < 4; ++i)
        if (pads[i].connected) return i;
    return 0;
}

void Game::Impl::updateDisconnectedPlayers()
{
    const bool activeSession = screen == Screen::Playing || screen == Screen::Paused ||
        ((screen == Screen::Controls || screen == Screen::Shop) && returnScreen == Screen::Paused);
    if (!activeSession) return;

    uint32_t missing = 0;
    for (int i = 0; i < playerCount; ++i)
        if (players[i].pad < 0 || players[i].pad >= 4 || !pads[players[i].pad].connected) missing |= 1u << i;

    const uint32_t previousMissing = disconnectedPlayers;
    disconnectedPlayers = missing;
    if (missing != 0)
    {
        if (screen != Screen::Paused)
        {
            screen = Screen::Paused;
            menuIndex = 0;
        }
        if (!pads[pausePad].connected) pausePad = firstConnectedPad();
        if (missing != previousMissing)
        {
            int player = 0;
            while (player < playerCount && (missing & (1u << player)) == 0) ++player;
            showNotice("CONTROLE " + number(player + 1) + " DESCONECTADO");
        }
    }
    else if (previousMissing != 0)
    {
        pausePad = firstConnectedPad();
        showNotice("CONTROLES RECONECTADOS");
    }
}

bool Game::Impl::down(int pad, int button) const
{
    return pad >= 0 && pad < 4 && pads[pad].connected && button >= 0 && button < PAD_BUTTON_COUNT && pads[pad].current[button];
}

bool Game::Impl::pressed(int pad, int button) const
{
    return down(pad, button) && !pads[pad].previous[button];
}

float Game::Impl::axis(int pad, int index) const
{
    if (pad < 0 || pad >= 4 || !pads[pad].connected || index < 0 || index >= 4) return 0.0f;
    return pads[pad].axes[index];
}

void Game::Impl::rumble(int pad, float strength, uint32_t milliseconds)
{
    if (pad < 0 || pad >= 4 || nativePadHandles[pad] < 0) return;
    const float configured = settings[pad].rumble;
    if (configured <= 0.0f) return;
    const float value = clampf(strength * configured, 0.0f, 1.0f);
    OrbisPadVibeParam vibration;
    vibration.lgMotor = static_cast<uint8_t>(value * 255.0f);
    vibration.smMotor = static_cast<uint8_t>(value * 190.0f);
    if (scePadSetVibration(nativePadHandles[pad], &vibration) >= 0)
        rumbleTimers[pad] = std::max(rumbleTimers[pad], milliseconds / 1000.0f);
}

void Game::Impl::updateNativePad(float dt)
{
    for (int i = 0; i < 4; ++i)
    {
        if (rumbleTimers[i] <= 0.0f) continue;
        rumbleTimers[i] -= dt;
        if (rumbleTimers[i] <= 0.0f && nativePadHandles[i] >= 0)
        {
            OrbisPadVibeParam stop = {0, 0};
            scePadSetVibration(nativePadHandles[i], &stop);
        }
    }
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
    updateNativePad(dt);
    noticeTimer = std::max(0.0f, noticeTimer - dt);
    resetConfirmTimer = std::max(0.0f, resetConfirmTimer - dt);

    switch (screen)
    {
        case Screen::Menu:
        case Screen::Paused:
        case Screen::GameOver:
            updateMenu(dt);
            break;
        case Screen::Controls:
            updateControls(dt);
            break;
        case Screen::Shop:
            updateShop(dt);
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
    const int pad = screen == Screen::Paused ? pausePad : firstConnectedPad();
    int optionCount = 0;
    if (screen == Screen::Menu) optionCount = 7;
    else if (screen == Screen::Paused) optionCount = 5;
    else if (screen == Screen::GameOver) optionCount = 1;

    if (pressed(pad, PAD_UP)) menuIndex = (menuIndex + optionCount - 1) % optionCount;
    if (pressed(pad, PAD_DOWN)) menuIndex = (menuIndex + 1) % optionCount;
    if (screen == Screen::Paused && pressed(pad, PAD_CIRCLE) && disconnectedPlayers == 0)
    {
        screen = Screen::Playing;
        return;
    }
    if (!pressed(pad, PAD_CROSS)) return;

    if (screen == Screen::Menu)
    {
        if (menuIndex == 0 || menuIndex == 1)
        {
            localRequested = menuIndex == 1;
            lobbyTimer = 3.0f;
            screen = Screen::Lobby;
        }
        else if (menuIndex == 2)
        {
            returnScreen = Screen::Menu;
            pausePad = firstConnectedPad();
            shopTab = ShopTab::Skins;
            menuIndex = 0;
            screen = Screen::Shop;
        }
        else if (menuIndex == 3)
        {
            returnScreen = Screen::Menu;
            pausePad = firstConnectedPad();
            controlIndex = 0;
            screen = Screen::Controls;
        }
        else if (menuIndex == 4) saveProfile(true);
        else if (menuIndex == 5)
        {
            if (resetConfirmTimer > 0.0f) resetProfile(true);
            else
            {
                resetConfirmTimer = 3.0f;
                showNotice("CONFIRME RESETAR SAVE");
            }
        }
        else active = false;
        menuIndex = 0;
    }
    else if (screen == Screen::Paused)
    {
        if (menuIndex == 0)
        {
            if (disconnectedPlayers == 0) screen = Screen::Playing;
            else showNotice("RECONECTE OS CONTROLES");
        }
        else if (menuIndex == 1)
        {
            returnScreen = Screen::Paused;
            controlIndex = 0;
            screen = Screen::Controls;
        }
        else if (menuIndex == 2)
        {
            returnScreen = Screen::Paused;
            shopTab = ShopTab::Skins;
            menuIndex = 0;
            screen = Screen::Shop;
        }
        else if (menuIndex == 3) saveProfile(true);
        else
        {
            saveProfile(false);
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

void Game::Impl::updateControls(float)
{
    const int pad = std::max(0, std::min(3, pausePad));
    PlayerSettings& config = settings[pad];
    if (bindingAction >= 0)
    {
        if (pressed(pad, PAD_CIRCLE))
        {
            bindingAction = -1;
            showNotice("ALTERACAO CANCELADA");
            return;
        }
        for (int button = 0; button < PAD_BUTTON_COUNT; ++button)
        {
            if (!pressed(pad, button)) continue;
            config.keys[bindingAction] = button;
            bindingAction = -1;
            profileDirty = true;
            saveProfile(false);
            showNotice("CONTROLE ALTERADO");
            for (int i = 0; i < playerCount; ++i)
                if (players[i].pad == pad)
                    for (int action = 0; action < ActionCount; ++action) players[i].keys[action] = config.keys[action];
            return;
        }
        return;
    }

    const int optionCount = 9;
    if (pressed(pad, PAD_UP)) controlIndex = (controlIndex + optionCount - 1) % optionCount;
    if (pressed(pad, PAD_DOWN)) controlIndex = (controlIndex + 1) % optionCount;

    int direction = 0;
    if (pressed(pad, PAD_LEFT)) direction = -1;
    if (pressed(pad, PAD_RIGHT)) direction = 1;
    if (direction != 0 && controlIndex >= 5 && controlIndex <= 7)
    {
        if (controlIndex == 5)
        {
            int quality = static_cast<int>(graphicsQuality);
            quality = (quality + direction + 3) % 3;
            graphicsQuality = static_cast<GraphicsQuality>(quality);
        }
        else if (controlIndex == 6) config.sensitivity = clampf(config.sensitivity + direction * 0.1f, 0.5f, 3.0f);
        else config.rumble = clampf(config.rumble + direction * 0.1f, 0.0f, 2.0f);
        profileDirty = true;
        saveProfile(false);
    }

    if (pressed(pad, PAD_CIRCLE) || (controlIndex == 8 && pressed(pad, PAD_CROSS)))
    {
        saveProfile(false);
        screen = returnScreen;
        menuIndex = 0;
        return;
    }
    if (pressed(pad, PAD_CROSS))
    {
        if (controlIndex < ActionCount) bindingAction = controlIndex;
        else if (controlIndex >= 5 && controlIndex <= 7)
        {
            if (controlIndex == 5)
                graphicsQuality = static_cast<GraphicsQuality>((static_cast<int>(graphicsQuality) + 1) % 3);
            else if (controlIndex == 6)
                config.sensitivity = config.sensitivity >= 3.0f ? 0.5f : clampf(config.sensitivity + 0.1f, 0.5f, 3.0f);
            else
                config.rumble = config.rumble >= 2.0f ? 0.0f : clampf(config.rumble + 0.1f, 0.0f, 2.0f);
            profileDirty = true;
            saveProfile(false);
        }
    }

    for (int i = 0; i < playerCount; ++i)
    {
        if (players[i].pad != pad) continue;
        players[i].sensitivity = config.sensitivity;
        players[i].rumbleStrength = config.rumble;
    }
}

void Game::Impl::updateShop(float)
{
    const int pad = std::max(0, std::min(3, pausePad));
    if (pressed(pad, PAD_CIRCLE))
    {
        saveProfile(false);
        screen = returnScreen;
        menuIndex = 0;
        return;
    }

    if (menuIndex < 2)
    {
        if (pressed(pad, PAD_LEFT) || pressed(pad, PAD_RIGHT)) menuIndex = menuIndex == 0 ? 1 : 0;
        if (pressed(pad, PAD_DOWN)) menuIndex = 2;
    }
    else if (menuIndex < 14)
    {
        if (pressed(pad, PAD_LEFT)) menuIndex = std::max(2, menuIndex - 1);
        if (pressed(pad, PAD_RIGHT)) menuIndex = std::min(13, menuIndex + 1);
        if (pressed(pad, PAD_UP)) menuIndex = menuIndex - 3 >= 2 ? menuIndex - 3 : ((menuIndex - 2) % 3 == 0 ? 0 : 1);
        if (pressed(pad, PAD_DOWN)) menuIndex = menuIndex + 3 <= 13 ? menuIndex + 3 : 14;
    }
    else if (pressed(pad, PAD_UP) || pressed(pad, PAD_LEFT) || pressed(pad, PAD_RIGHT)) menuIndex = 13;

    if (!pressed(pad, PAD_CROSS)) return;
    if (menuIndex == 0 || menuIndex == 1)
    {
        shopTab = menuIndex == 0 ? ShopTab::Skins : ShopTab::Hats;
        return;
    }
    if (menuIndex == 14)
    {
        saveProfile(false);
        screen = returnScreen;
        menuIndex = 0;
        return;
    }

    const int item = menuIndex - 2;
    const bool skinTab = shopTab == ShopTab::Skins;
    uint32_t& owned = skinTab ? ownedSkins : ownedHats;
    const CosmeticItem* catalog = skinTab ? SKINS : HATS;
    const uint32_t mask = 1u << item;
    if ((owned & mask) == 0)
    {
        if (profileSilver < catalog[item].cost)
        {
            showNotice("PRATA INSUFICIENTE");
            return;
        }
        profileSilver -= catalog[item].cost;
        owned |= mask;
        showNotice("COSMETICO COMPRADO");
    }
    if (skinTab) activeSkins[pad] = static_cast<uint8_t>(item);
    else activeHats[pad] = static_cast<uint8_t>(item);
    for (int i = 0; i < playerCount; ++i)
    {
        if (players[i].pad != pad) continue;
        players[i].skin = activeSkins[pad];
        players[i].hat = activeHats[pad];
    }
    profileDirty = true;
    saveProfile(false);
}

void Game::Impl::updateLobby(float dt)
{
    const int pad = firstConnectedPad();
    if (pressed(pad, PAD_CIRCLE))
    {
        screen = Screen::Menu;
        menuIndex = 0;
        return;
    }
    const int required = localRequested ? 2 : 1;
    if (controllerCount >= required)
    {
        lobbyTimer -= dt;
        if (pressed(pad, PAD_CROSS) || lobbyTimer <= 0.0f) startGame(localRequested ? controllerCount : 1);
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
    floatingTexts.clear();
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
    disconnectedPlayers = 0;
    int nextPad = 0;
    for (int i = 0; i < playerCount; ++i)
    {
        Player& player = players[i];
        while (nextPad < 4 && !pads[nextPad].connected) ++nextPad;
        player.pad = nextPad < 4 ? nextPad++ : firstConnectedPad();
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
        const int slot = std::max(0, std::min(3, player.pad));
        for (int action = 0; action < ActionCount; ++action) player.keys[action] = settings[slot].keys[action];
        player.sensitivity = settings[slot].sensitivity;
        player.rumbleStrength = settings[slot].rumble;
        player.skin = activeSkins[slot];
        player.hat = activeHats[slot];
        player.cosmeticAnimation = 0.0f;
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
    const int limit = qualityParticleLimit();
    for (int i = 0; i < count && static_cast<int>(particles.size()) < limit; ++i)
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

void Game::Impl::addFloatingText(float x, float y, const std::string& textValue, Color colorValue)
{
    FloatingText message;
    message.x = x;
    message.y = y;
    message.life = 1.5f;
    message.velocityY = -40.0f;
    message.text = textValue;
    message.color = colorValue;
    floatingTexts.push_back(message);
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
            addFloatingText(player.x, player.y - 30.0f, "SISTEMA RELIGADO!", player.color);
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
    projectile.trailCount = 0;
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
            addFloatingText(boss.x, boss.y - 70.0f, "SISTEMA COMPROMETIDO!", boss.color);
        }
    }
    else if (boss.phase == 2)
    {
        if (boss.shape == 1) boss.x += std::cos(boss.angle) * 84.0f * dt;
        else if (boss.shape == 2) boss.x += std::cos(boss.angle) * 108.0f * dt;
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
        const float chaseSpeed = boss.shape == 2 ? 130.0f : (boss.shape == 0 ? 100.0f : (boss.shape == 1 ? 80.0f : 90.0f));
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
                boss.x = clampf(target->x + std::cos(angle) * 120.0f, mapMinX + boss.radius, mapMaxX - boss.radius);
                boss.y = clampf(target->y + std::sin(angle) * 120.0f, mapMinY + boss.radius, mapMaxY - boss.radius);
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
            addFloatingText(boss.x, boss.y - 70.0f, "MODO FURIA!", RED);
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
        addFloatingText(players[i].x, players[i].y - 50.0f, "BOSS DESTRUIDO! +15 PRATA", GOLD);
        rumble(players[i].pad, 1.0f, 1200);
    }
    profileSilver += 15;
    profileDirty = true;
    saveProfile(false);
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
    projectile.trailCount = 0;
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
        addFloatingText(player.x, player.y - 28.0f, "TIRO RAPIDO!", GOLD);
        rumble(player.pad, 0.35f, 120);
    }
    else if (type == 2 && player.coins >= player.damageCost)
    {
        player.coins -= player.damageCost;
        player.damageMultiplier += 0.5f;
        player.damageCost = static_cast<int>(player.damageCost * 2.2f);
        addFloatingText(player.x, player.y - 28.0f, "+50% DANO!", GOLD);
        rumble(player.pad, 0.45f, 120);
    }
    else if (type == 3 && player.coins >= player.skillCost)
    {
        player.coins -= player.skillCost;
        player.skillDuration += 0.5f;
        player.skillCooldownMax = std::max(5.0f, player.skillCooldownMax - 1.0f);
        player.skillCost = static_cast<int>(player.skillCost * 1.8f);
        addFloatingText(player.x, player.y - 28.0f, "HAB MELHORADA!", GOLD);
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
        addFloatingText(player.x, player.y, "SISTEMA DESLIGADO", player.color);
        addParticles(player.x, player.y, player.color, 34, 420.0f, 1.2f);
        checkGameOver();
    }
}

void Game::Impl::handleProjectileCollisions(float dt)
{
    for (int i = static_cast<int>(projectiles.size()) - 1; i >= 0; --i)
    {
        Projectile& projectile = projectiles[i];
        if (graphicsQuality == GraphicsQuality::High)
        {
            for (int trail = std::min(4, projectile.trailCount); trail > 0; --trail) projectile.trail[trail] = projectile.trail[trail - 1];
            projectile.trail[0] = {projectile.x, projectile.y};
            projectile.trailCount = std::min(5, projectile.trailCount + 1);
        }
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
                    if (totalKills > 0 && totalKills % 50 == 0)
                    {
                        addDrop(x, y, DropType::Heart);
                        addFloatingText(x, y - 20.0f, "CORACAO!", PINK);
                    }
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
            else if (drop.type == DropType::Silver)
            {
                profileSilver++;
                profileDirty = true;
                saveProfile(false);
            }
            else
            {
                const int heal = static_cast<int>(target->maxHp * 0.30f);
                target->hp = std::min(target->maxHp, target->hp + heal);
                addFloatingText(target->x, target->y - 30.0f, "+" + number(heal) + " INTEGRIDADE", PINK);
            }
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
    const int limit = qualityParticleLimit();
    if (static_cast<int>(particles.size()) > limit)
        particles.erase(particles.begin(), particles.begin() + (particles.size() - limit));
}

void Game::Impl::updateFloatingTexts(float dt)
{
    for (int i = static_cast<int>(floatingTexts.size()) - 1; i >= 0; --i)
    {
        FloatingText& textValue = floatingTexts[i];
        textValue.y += textValue.velocityY * dt;
        textValue.velocityY *= std::pow(0.93f, dt * 60.0f);
        textValue.life -= dt;
        if (textValue.life <= 0.0f) floatingTexts.erase(floatingTexts.begin() + i);
    }
}

void Game::Impl::checkGameOver()
{
    for (int i = 0; i < playerCount; ++i)
        if (players[i].hp > 0.0f) return;
    screen = Screen::GameOver;
    menuIndex = 0;
    saveProfile(false);
}

void Game::Impl::updatePlaying(float dt)
{
    for (int i = 0; i < playerCount; ++i)
    {
        if (pressed(players[i].pad, players[i].keys[ActionPause]))
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
        player.cosmeticAnimation += dt;

        float moveX = axis(player.pad, 0);
        float moveY = axis(player.pad, 1);
        const float magnitude = std::sqrt(moveX * moveX + moveY * moveY);
        if (magnitude <= 0.18f) { moveX = 0.0f; moveY = 0.0f; }
        player.x += moveX * 300.0f * player.sensitivity * dt;
        player.y += moveY * 300.0f * player.sensitivity * dt;
        resolveWalls(player.x, player.y, player.radius);
        player.x = clampf(player.x, mapMinX + player.radius, mapMaxX - player.radius);
        player.y = clampf(player.y, mapMinY + player.radius, mapMaxY - player.radius);
        player.cameraX += (player.x - player.cameraX) * std::min(1.0f, dt * 5.0f);
        player.cameraY += (player.y - player.cameraY) * std::min(1.0f, dt * 5.0f);

        if (player.fireTimer <= 0.0f && (!enemies.empty() || boss.active)) shoot(player, i);
        if (pressed(player.pad, player.keys[ActionSkill]) && player.skillCooldown <= 0.0f && player.timeStop <= 0.0f)
        {
            player.timeStop = player.skillDuration;
            player.skillCooldown = player.skillCooldownMax;
            addParticles(player.x, player.y, player.color, 44, 390.0f, 1.0f);
            addFloatingText(player.x, player.y - 30.0f, "TEMPO PARADO!", player.color);
            rumble(player.pad, 0.6f, 500);
        }
        if (pressed(player.pad, player.keys[ActionUpgradeFire])) buyUpgrade(player, 1);
        if (pressed(player.pad, player.keys[ActionUpgradeDamage])) buyUpgrade(player, 2);
        if (pressed(player.pad, player.keys[ActionUpgradeSkill])) buyUpgrade(player, 3);
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
            const int cap = static_cast<int>(14 + wave * 1.5f + (playerCount - 1) * 6);
            const float playerFactor = 1.0f + (playerCount - 1) * 0.30f;
            const float chancePerFrame = clampf((0.02f + wave * 0.005f) * playerFactor, 0.0f, 0.95f);
            const float chanceThisTick = 1.0f - std::pow(1.0f - chancePerFrame, dt * 60.0f);
            if (random01() < chanceThisTick && static_cast<int>(enemies.size()) < cap)
            {
                spawnEnemy();
            }
        }
    }

    updateBoss(dt);
    handleProjectileCollisions(dt);
    handleEnemies(dt);
    handleDrops(dt);
    updateParticles(dt);
    updateFloatingTexts(dt);
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
    draw::glowCircle(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(radius), colorValue, qualityGlow(glow));
}

void Game::Impl::renderBackdrop()
{
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, BG);
    const float time = lastTick / 1000.0f;
    const int bandCount = graphicsQuality == GraphicsQuality::High ? 8 : (graphicsQuality == GraphicsQuality::Medium ? 5 : 3);
    for (int band = 0; band < bandCount; ++band)
    {
        const int margin = band * 90;
        draw::fillRect(renderer, margin, margin / 2, SCREEN_W - margin * 2, SCREEN_H - margin, {4, static_cast<Uint8>(8 + band), static_cast<Uint8>(20 + band * 3), 30});
    }
    const int stars = qualityStars();
    for (int i = 0; i < stars; ++i)
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
    draw::panel(renderer, 430, 72, 1060, 930, CYAN, PANEL);
    draw::text(renderer, "GEOMETRIC WARS", SCREEN_W / 2, 130, 8, CYAN, true);
    draw::text(renderer, "PS4 EDITION - NATIVO", SCREEN_W / 2, 215, 3, SILVER, true);
    std::vector<std::string> options;
    options.push_back("JOGAR SOLO");
    options.push_back("JOGAR LOCAL (CO-OP)");
    options.push_back("LOJA DE COSMETICOS");
    options.push_back("CONTROLES GLOBAIS");
    options.push_back("SALVAR PERFIL");
    options.push_back(resetConfirmTimer > 0.0f ? "CONFIRMAR RESET?" : "RESETAR SAVE");
    options.push_back("SAIR");
    renderMenuOptions(options, 300, 3);
    if (controllerCount == 0)
        draw::text(renderer, "CONECTE UM CONTROLE", SCREEN_W / 2, 825, 2, RED, true);
    draw::text(renderer, "D-PAD NAVEGAR   X CONFIRMAR   O VOLTAR", SCREEN_W / 2, 940, 2, MUTED, true);
}

void Game::Impl::renderLobby()
{
    renderBackdrop();
    draw::panel(renderer, 330, 205, 1260, 670, CYAN, PANEL);
    draw::text(renderer, "SALA DE ESPERA", SCREEN_W / 2, 275, 6, CYAN, true);
    draw::text(renderer, localRequested ? "CO-OP LOCAL - ATE 4 JOGADORES" : "MODO SOLO", SCREEN_W / 2, 345, 3, SILVER, true);
    const int total = std::max(1, controllerCount);
    int card = 0;
    for (int pad = 0; pad < 4; ++pad)
    {
        if (!pads[pad].connected) continue;
        const int cardW = 220;
        const int gap = 26;
        const int x = SCREEN_W / 2 - (total * cardW + (total - 1) * gap) / 2 + card * (cardW + gap);
        const int y = 470;
        draw::panel(renderer, x, y, cardW, 150, PLAYER_COLORS[card], {0, 0, 0, 190});
        draw::text(renderer, "P" + number(card + 1), x + cardW / 2, y + 30, 5, PLAYER_COLORS[card], true);
        draw::text(renderer, "PRONTO", x + cardW / 2, y + 95, 2, GREEN, true);
        ++card;
    }
    if (controllerCount == 0)
        draw::text(renderer, "NENHUM CONTROLE DETECTADO", SCREEN_W / 2, 530, 2, MUTED, true);
    const int required = localRequested ? 2 : 1;
    if (controllerCount >= required)
    {
        draw::text(renderer, "INICIANDO EM " + number(std::max(1, static_cast<int>(std::ceil(lobbyTimer)))) + "...", SCREEN_W / 2, 690, 4, GREEN, true);
        draw::text(renderer, "X INICIAR AGORA", SCREEN_W / 2, 750, 2, MUTED, true);
    }
    else draw::text(renderer, localRequested ? "CONECTE NO MINIMO 2 CONTROLES" : "CONECTE 1 CONTROLE", SCREEN_W / 2, 700, 3, RED, true);
    draw::text(renderer, "O VOLTAR", SCREEN_W / 2, 815, 2, MUTED, true);
}

void Game::Impl::renderControls()
{
    renderBackdrop();
    draw::panel(renderer, 370, 65, 1180, 950, GOLD, PANEL);
    draw::text(renderer, "CONTROLES - JOGADOR " + number(pausePad + 1), SCREEN_W / 2, 105, 5, GOLD, true);
    const char* labels[9] = {"PAUSAR", "HABILIDADE (TIME STOP)", "UPGRADE TIRO", "UPGRADE DANO", "UPGRADE HABILIDADE", "GRAFICOS", "SENSIBILIDADE", "VIBRACAO", "VOLTAR"};
    const int yStart = 205;
    const PlayerSettings& config = settings[std::max(0, std::min(3, pausePad))];
    for (int i = 0; i < 9; ++i)
    {
        const int y = yStart + i * 78;
        if (i == controlIndex)
        {
            draw::fillRect(renderer, 470, y - 14, 980, 58, {110, 80, 0, 65});
            draw::outlineRect(renderer, 470, y - 14, 980, 58, GOLD, 2);
            draw::triangle(renderer, 490, y + 15, 510, y + 2, 510, y + 28, GOLD);
        }
        draw::text(renderer, labels[i], 535, y, i == 1 || i == 4 ? 2 : 3, i == controlIndex ? WHITE : MUTED);
        std::string value;
        if (i < ActionCount) value = bindingAction == i ? "PRESSIONE UM BOTAO..." : buttonName(config.keys[i]);
        else if (i == 5) value = graphicsQuality == GraphicsQuality::High ? "ALTO" : (graphicsQuality == GraphicsQuality::Medium ? "MEDIO" : "BAIXO");
        else if (i == 6) value = oneDecimal(config.sensitivity) + "X";
        else if (i == 7) value = number(static_cast<int>(config.rumble * 100.0f + 0.5f)) + "%";
        if (!value.empty()) draw::text(renderer, value, 1375 - draw::textWidth(value, 2), y + 3, 2, i == controlIndex ? CYAN : WHITE);
    }
    draw::text(renderer, "D-PAD NAVEGAR / AJUSTAR   X ALTERAR   O VOLTAR", SCREEN_W / 2, 940, 2, MUTED, true);
}

void Game::Impl::renderShop()
{
    renderBackdrop();
    draw::panel(renderer, 120, 45, 1680, 990, PURPLE, PANEL);
    draw::text(renderer, "LOJA DE COSMETICOS", SCREEN_W / 2, 82, 5, PURPLE, true);
    draw::text(renderer, "JOGADOR " + number(pausePad + 1), SCREEN_W / 2, 135, 2, GOLD, true);
    draw::text(renderer, "PRATA DO PERFIL: " + number(profileSilver), SCREEN_W / 2, 168, 2, SILVER, true);

    const int tabX[2] = {255, 535};
    for (int tab = 0; tab < 2; ++tab)
    {
        const bool selected = menuIndex == tab;
        draw::fillRect(renderer, tabX[tab], 220, 250, 52, selected ? Color{90, 30, 130, 120} : Color{0, 4, 16, 180});
        draw::outlineRect(renderer, tabX[tab], 220, 250, 52, selected ? PURPLE : withAlpha(MUTED, 90), selected ? 2 : 1);
        draw::text(renderer, tab == 0 ? "TRAJES" : "ACESSORIOS", tabX[tab] + 125, 237, 2, selected ? WHITE : MUTED, true);
    }

    const bool skins = shopTab == ShopTab::Skins;
    const CosmeticItem* catalog = skins ? SKINS : HATS;
    const uint32_t owned = skins ? ownedSkins : ownedHats;
    const int activeItem = skins ? activeSkins[pausePad] : activeHats[pausePad];
    for (int item = 0; item < COSMETIC_COUNT; ++item)
    {
        const int column = item % 3;
        const int row = item / 3;
        const int x = 220 + column * 285;
        const int y = 305 + row * 125;
        const bool selected = menuIndex == item + 2;
        const bool itemOwned = (owned & (1u << item)) != 0;
        const bool equipped = activeItem == item;
        const Color border = equipped ? GREEN : (itemOwned ? GOLD : PURPLE);
        draw::fillRect(renderer, x, y, 265, 108, selected ? Color{60, 25, 90, 190} : Color{0, 4, 16, 180});
        draw::outlineRect(renderer, x, y, 265, 108, selected ? WHITE : withAlpha(border, 150), selected ? 3 : 1);
        renderAvatar(x + 42, y + 55, 20, PLAYER_COLORS[pausePad], skins ? item : 0, skins ? 0 : item, lastTick / 900.0f, false);
        const std::string name = catalog[item].name;
        draw::text(renderer, name, x + 78, y + 20, name.size() > 17 ? 1 : 2, WHITE);
        const std::string status = equipped ? "EQUIPADO" : (itemOwned ? "X EQUIPAR" : (catalog[item].cost == 0 ? "GRATIS" : number(catalog[item].cost) + " PRATA"));
        draw::text(renderer, status, x + 78, y + 67, 1, equipped ? GREEN : (itemOwned ? GOLD : SILVER));
    }

    draw::panel(renderer, 1130, 250, 500, 560, PURPLE, {1, 4, 15, 210});
    draw::text(renderer, "MONSTRUARIO", 1380, 292, 3, PURPLE, true);
    renderAvatar(1380, 500, 72, PLAYER_COLORS[pausePad], activeSkins[pausePad], activeHats[pausePad], lastTick / 900.0f, false);
    draw::text(renderer, std::string("TRAJE: ") + SKINS[activeSkins[pausePad]].name, 1380, 635, 2, WHITE, true);
    draw::text(renderer, std::string("ACESSORIO: ") + HATS[activeHats[pausePad]].name, 1380, 685, 1, WHITE, true);
    draw::text(renderer, "EQUIPAMENTO ATUAL", 1380, 735, 1, MUTED, true);

    const bool backSelected = menuIndex == 14;
    if (backSelected) draw::outlineRect(renderer, 770, 875, 380, 54, WHITE, 2);
    draw::text(renderer, "VOLTAR", SCREEN_W / 2, 893, 3, backSelected ? WHITE : MUTED, true);
    draw::text(renderer, "D-PAD NAVEGA   X COMPRA / EQUIPA   O VOLTA", SCREEN_W / 2, 970, 2, MUTED, true);
}

void Game::Impl::renderPause()
{
    renderPlaying();
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, {0, 0, 8, 120});
    const bool disconnected = disconnectedPlayers != 0;
    draw::panel(renderer, 620, 240, 680, 600, disconnected ? RED : CYAN, {2, 8, 24, 238});

    if (disconnected)
    {
        int player = 0;
        while (player < playerCount && (disconnectedPlayers & (1u << player)) == 0) ++player;
        draw::text(renderer, "CONTROLE " + number(player + 1) + " DESCONECTADO", SCREEN_W / 2, 282, 4, RED, true);
        draw::text(renderer, "RECONECTE PARA CONTINUAR", SCREEN_W / 2, 335, 2, GOLD, true);
    }
    else
    {
        draw::text(renderer, "PAUSADO", SCREEN_W / 2, 282, 6, CYAN, true);
        draw::text(renderer, "JOGADOR " + number(pausePad + 1) + " PAUSOU", SCREEN_W / 2, 345, 2, GOLD, true);
    }

    const char* options[5] = {"RETOMAR", "CONTROLES / AJUSTES", "LOJA DE COSMETICOS", "SALVAR PERFIL", "SAIR AO MENU"};
    for (int i = 0; i < 5; ++i)
    {
        const int x = 690;
        const int y = 390 + i * 82;
        const bool selected = menuIndex == i;
        Color label = MUTED;
        if (i == 3) label = GOLD;
        else if (i == 4) label = RED;
        else if (selected) label = WHITE;
        if (i == 0 && disconnected) label = {70, 80, 100, 255};
        draw::fillRect(renderer, x, y, 540, 68, selected ? Color{0, 75, 145, 105} : Color{0, 4, 16, 190});
        draw::outlineRect(renderer, x, y, 540, 68, selected ? CYAN : withAlpha(CYAN, 80), selected ? 2 : 1);
        if (selected) draw::triangle(renderer, x + 18, y + 34, x + 34, y + 23, x + 34, y + 45, CYAN);
        draw::text(renderer, options[i], x + 300, y + 23, 3, label, true);
    }
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
    if (point.x < viewport.x - 80 || point.x > viewport.x + viewport.w + 80 || point.y < viewport.y - 80 || point.y > viewport.y + viewport.h + 80) return;
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    const int radius = static_cast<int>(enemy.radius);
    if (enemy.edges == 3)
    {
        int px[3], py[3];
        for (int i = 0; i < 3; ++i)
        {
            const float angle = enemy.angle + i * PI * 2.0f / 3.0f - PI * 0.5f;
            px[i] = x + static_cast<int>(std::cos(angle) * radius);
            py[i] = y + static_cast<int>(std::sin(angle) * radius);
        }
        draw::triangle(renderer, px[0], py[0], px[1], py[1], px[2], py[2], enemy.color);
    }
    else if (enemy.edges == 4 || enemy.edges == 0)
    {
        int px[4], py[4];
        for (int i = 0; i < 4; ++i)
        {
            const float angle = enemy.angle + PI * 0.25f + i * PI * 0.5f;
            px[i] = x + static_cast<int>(std::cos(angle) * radius * 1.35f);
            py[i] = y + static_cast<int>(std::sin(angle) * radius * 1.35f);
        }
        draw::triangle(renderer, px[0], py[0], px[1], py[1], px[2], py[2], withAlpha(enemy.color, 220));
        draw::triangle(renderer, px[0], py[0], px[2], py[2], px[3], py[3], withAlpha(enemy.color, 220));
        for (int i = 0; i < 4; ++i) draw::line(renderer, px[i], py[i], px[(i + 1) % 4], py[(i + 1) % 4], enemy.color, 2);
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
    if (point.x < viewport.x - 180 || point.x > viewport.x + viewport.w + 180 || point.y < viewport.y - 180 || point.y > viewport.y + viewport.h + 180) return;
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

void Game::Impl::renderAvatar(int x, int y, int radius, Color colorValue, int skin, int hat, float animation, bool dead)
{
    if (dead)
    {
        draw::circle(renderer, x, y, radius + 7, withAlpha(colorValue, 130));
        draw::line(renderer, x - radius / 2, y - radius / 2, x + radius / 2, y + radius / 2, colorValue, 3);
        draw::line(renderer, x + radius / 2, y - radius / 2, x - radius / 2, y + radius / 2, colorValue, 3);
        return;
    }

    const float scale = radius / 18.0f;
    const int ring = std::max(2, static_cast<int>(5.0f * scale));
    Color body = colorValue;
    if (skin == 5) body = withAlpha(body, 135);
    if (skin == 10) body = {8, 5, 17, 255};
    if (skin == 11) body = PURPLE;

    if (skin == 2) draw::glowCircle(renderer, x, y, radius + static_cast<int>(radius * 1.25f), withAlpha(colorValue, 34), qualityGlow(12));
    draw::glowCircle(renderer, x, y, radius, body, qualityGlow(static_cast<int>(20.0f * scale)));
    draw::circle(renderer, x, y, std::max(2, radius - static_cast<int>(6.0f * scale)), withAlpha(WHITE, 150));
    draw::fillCircle(renderer, x, y - radius + std::max(2, static_cast<int>(4.0f * scale)), std::max(2, static_cast<int>(3.0f * scale)), WHITE);

    if (skin == 1) draw::circle(renderer, x, y, radius + ring, colorValue);
    else if (skin == 3)
    {
        draw::circle(renderer, x, y, radius + static_cast<int>(8.0f * scale), withAlpha(colorValue, 210));
        draw::circle(renderer, x, y, radius + static_cast<int>(11.0f * scale), withAlpha(WHITE, 70));
    }
    else if (skin == 4 || skin == 8)
    {
        const int count = skin == 4 ? 6 : 7;
        const int outer = radius + static_cast<int>((skin == 4 ? 8.0f : 11.0f) * scale);
        for (int i = 0; i < count; ++i)
        {
            const float angle = animation * (skin == 4 ? 1.0f : 0.7f) + i * PI * 2.0f / count;
            const int tipX = x + static_cast<int>(std::cos(angle) * outer);
            const int tipY = y + static_cast<int>(std::sin(angle) * outer);
            const int aX = x + static_cast<int>(std::cos(angle + 0.22f) * (radius - 2));
            const int aY = y + static_cast<int>(std::sin(angle + 0.22f) * (radius - 2));
            const int bX = x + static_cast<int>(std::cos(angle - 0.22f) * (radius - 2));
            const int bY = y + static_cast<int>(std::sin(angle - 0.22f) * (radius - 2));
            draw::triangle(renderer, tipX, tipY, aX, aY, bX, bY, skin == 8 ? ORANGE : colorValue);
        }
    }
    else if (skin == 6)
    {
        draw::fillCircle(renderer, x - radius / 3, y - radius / 3, std::max(2, radius / 3), withAlpha(WHITE, 185));
        for (int i = 0; i < 4; ++i)
        {
            const float angle = animation * 1.8f + i * PI * 0.5f;
            draw::glowCircle(renderer, x + static_cast<int>(std::cos(angle) * (radius + ring)), y + static_cast<int>(std::sin(angle) * (radius + ring)), std::max(2, static_cast<int>(3.0f * scale)), CYAN, qualityGlow(7));
        }
    }
    else if (skin == 7)
    {
        const int inner = radius - 3;
        for (int offset = -inner; offset <= inner; offset += std::max(4, static_cast<int>(6.0f * scale)))
        {
            const int half = static_cast<int>(std::sqrt(static_cast<float>(std::max(0, inner * inner - offset * offset))));
            draw::line(renderer, x + offset, y - half, x + offset, y + half, withAlpha(GREEN, 170));
            draw::line(renderer, x - half, y + offset, x + half, y + offset, withAlpha(GREEN, 170));
        }
    }
    else if (skin == 9)
    {
        const int crystalRadius = radius + static_cast<int>(7.0f * scale);
        int firstX = 0, firstY = 0, previousX = 0, previousY = 0;
        for (int i = 0; i < 6; ++i)
        {
            const float angle = animation * 0.22f + i * PI / 3.0f;
            const int pointX = x + static_cast<int>(std::cos(angle) * crystalRadius);
            const int pointY = y + static_cast<int>(std::sin(angle) * crystalRadius);
            if (i == 0) { firstX = pointX; firstY = pointY; }
            else draw::line(renderer, previousX, previousY, pointX, pointY, {174, 234, 255, 255}, std::max(2, static_cast<int>(3.0f * scale)));
            previousX = pointX;
            previousY = pointY;
        }
        draw::line(renderer, previousX, previousY, firstX, firstY, {174, 234, 255, 255}, std::max(2, static_cast<int>(3.0f * scale)));
    }
    else if (skin == 10)
    {
        draw::fillCircle(renderer, x, y, std::max(2, radius - static_cast<int>(7.0f * scale)), {32, 4, 56, 255});
        draw::circle(renderer, x, y, radius + ring, PURPLE);
    }
    else if (skin == 11)
    {
        draw::circle(renderer, x, y, radius + ring, WHITE);
        draw::line(renderer, x - radius, y, x, y - radius, CYAN, std::max(2, static_cast<int>(3.0f * scale)));
        draw::line(renderer, x, y - radius, x + radius, y, PURPLE, std::max(2, static_cast<int>(3.0f * scale)));
        draw::line(renderer, x + radius, y, x, y + radius, PINK, std::max(2, static_cast<int>(3.0f * scale)));
        draw::line(renderer, x, y + radius, x - radius, y, GOLD, std::max(2, static_cast<int>(3.0f * scale)));
    }

    const int top = y - radius;
    if (hat == 1)
    {
        draw::fillRect(renderer, x - radius + 3, y - static_cast<int>(5.0f * scale), radius * 2 - 6, std::max(4, static_cast<int>(8.0f * scale)), {7, 27, 50, 255});
        draw::outlineRect(renderer, x - radius + 3, y - static_cast<int>(5.0f * scale), radius * 2 - 6, std::max(4, static_cast<int>(8.0f * scale)), CYAN, 2);
    }
    else if (hat == 2)
    {
        draw::fillCircle(renderer, x - static_cast<int>(2.0f * scale), top + static_cast<int>(2.0f * scale), std::max(5, static_cast<int>(13.0f * scale)), {22, 59, 155, 255});
        draw::fillRect(renderer, x - static_cast<int>(14.0f * scale), top, static_cast<int>(23.0f * scale), std::max(3, static_cast<int>(4.0f * scale)), {22, 59, 155, 255});
    }
    else if (hat == 3)
    {
        draw::line(renderer, x, top, x, top - static_cast<int>(17.0f * scale), WHITE, 2);
        draw::glowCircle(renderer, x, top - static_cast<int>(19.0f * scale), std::max(2, static_cast<int>(4.0f * scale)), PINK, qualityGlow(6));
    }
    else if (hat == 4)
    {
        const int crownTop = top - static_cast<int>(14.0f * scale);
        draw::triangle(renderer, x - static_cast<int>(12.0f * scale), top - 2, x - static_cast<int>(12.0f * scale), crownTop, x - static_cast<int>(5.0f * scale), top - static_cast<int>(8.0f * scale), GOLD);
        draw::triangle(renderer, x - static_cast<int>(6.0f * scale), top - 2, x, crownTop, x + static_cast<int>(6.0f * scale), top - 2, GOLD);
        draw::triangle(renderer, x + static_cast<int>(5.0f * scale), top - static_cast<int>(8.0f * scale), x + static_cast<int>(12.0f * scale), crownTop, x + static_cast<int>(12.0f * scale), top - 2, GOLD);
        draw::fillRect(renderer, x - static_cast<int>(12.0f * scale), top - 4, static_cast<int>(24.0f * scale), std::max(3, static_cast<int>(5.0f * scale)), GOLD);
    }
    else if (hat == 5)
    {
        draw::fillRect(renderer, x - static_cast<int>(12.0f * scale), top - static_cast<int>(20.0f * scale), static_cast<int>(24.0f * scale), static_cast<int>(16.0f * scale), {17, 17, 21, 255});
        draw::outlineRect(renderer, x - static_cast<int>(12.0f * scale), top - static_cast<int>(20.0f * scale), static_cast<int>(24.0f * scale), static_cast<int>(16.0f * scale), MUTED, 1);
        draw::fillRect(renderer, x - static_cast<int>(15.0f * scale), top - static_cast<int>(4.0f * scale), static_cast<int>(30.0f * scale), std::max(3, static_cast<int>(4.0f * scale)), {17, 17, 21, 255});
    }
    else if (hat == 6)
    {
        draw::circle(renderer, x, y, radius + ring, PURPLE);
        draw::fillRect(renderer, x - radius - ring, y - static_cast<int>(4.0f * scale), ring, static_cast<int>(11.0f * scale), {35, 16, 67, 255});
        draw::fillRect(renderer, x + radius, y - static_cast<int>(4.0f * scale), ring, static_cast<int>(11.0f * scale), {35, 16, 67, 255});
    }
    else if (hat == 7 || hat == 11)
    {
        draw::ellipse(renderer, x, top - static_cast<int>(8.0f * scale), static_cast<int>((hat == 11 ? 11.0f : 10.0f) * scale), std::max(3, static_cast<int>(4.0f * scale)), hat == 11 ? WHITE : GOLD, std::max(2, static_cast<int>(3.0f * scale)));
    }
    else if (hat == 8)
    {
        draw::triangle(renderer, x - static_cast<int>(9.0f * scale), top, x - static_cast<int>(11.0f * scale), top - static_cast<int>(14.0f * scale), x - static_cast<int>(3.0f * scale), top - 2, RED);
        draw::triangle(renderer, x + static_cast<int>(9.0f * scale), top, x + static_cast<int>(11.0f * scale), top - static_cast<int>(14.0f * scale), x + static_cast<int>(3.0f * scale), top - 2, RED);
    }
    else if (hat == 9)
    {
        draw::circle(renderer, x, y, radius + ring, {183, 235, 255, 255});
        draw::fillRect(renderer, x - radius, top - 2, radius * 2, std::max(3, static_cast<int>(4.0f * scale)), withAlpha(CYAN, 150));
    }
    else if (hat == 10)
    {
        const int flowerY = top - static_cast<int>(9.0f * scale);
        for (int i = 0; i < 5; ++i)
        {
            const float angle = i * PI * 2.0f / 5.0f;
            draw::fillCircle(renderer, x + static_cast<int>(std::cos(angle) * 7.0f * scale), flowerY + static_cast<int>(std::sin(angle) * 7.0f * scale), std::max(2, static_cast<int>(5.0f * scale)), {255, 111, 216, 255});
        }
        draw::fillCircle(renderer, x, flowerY, std::max(2, static_cast<int>(4.0f * scale)), GOLD);
    }
}

void Game::Impl::renderPlayer(const Viewport& viewport, const Player& cameraPlayer, const Player& player, int index)
{
    const Vec2 point = toScreen(viewport, cameraPlayer, player.x, player.y);
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    if (x < viewport.x - 80 || x > viewport.x + viewport.w + 80 || y < viewport.y - 80 || y > viewport.y + viewport.h + 80) return;
    if (player.hp <= 0.0f) { renderAvatar(x, y, static_cast<int>(player.radius), player.color, player.skin, player.hat, player.cosmeticAnimation, true); return; }
    if (player.invincible > 0.0f && (static_cast<int>(lastTick / 60) % 2 == 0)) return;
    renderAvatar(x, y, static_cast<int>(player.radius), player.color, player.skin, player.hat, player.cosmeticAnimation, false);
    if (player.timeStop > 0.0f) draw::circle(renderer, x, y, static_cast<int>(player.radius + 9), CYAN);
    if (playerCount > 1) draw::text(renderer, "P" + number(index + 1), x, y + 27, 1, player.color, true);
}

void Game::Impl::renderFloatingTexts(const Viewport& viewport, const Player& cameraPlayer)
{
    for (unsigned i = 0; i < floatingTexts.size(); ++i)
    {
        const FloatingText& message = floatingTexts[i];
        const Vec2 point = toScreen(viewport, cameraPlayer, message.x, message.y);
        if (point.x < viewport.x - 300 || point.x > viewport.x + viewport.w + 300 || point.y < viewport.y - 40 || point.y > viewport.y + viewport.h + 40) continue;
        draw::text(renderer, message.text, static_cast<int>(point.x), static_cast<int>(point.y), 2, withAlpha(message.color, static_cast<int>(255.0f * clampf(message.life / 1.5f, 0.0f, 1.0f))), true);
    }
}

void Game::Impl::renderHud(const Viewport& viewport, const Player& player, int index)
{
    const int padding = 14;
    const int leftX = viewport.x + padding;
    const int bottom = viewport.y + viewport.h - padding;
    const int leftWidth = std::min(270, viewport.w / 2 - 24);
    const int statsHeight = 106;
    const int barsHeight = 90;
    const int gap = 7;
    const int statsY = bottom - barsHeight - gap - statsHeight;
    const int barsY = bottom - barsHeight;

    draw::fillRect(renderer, leftX, statsY, leftWidth, statsHeight, {2, 6, 20, 230});
    draw::outlineRect(renderer, leftX, statsY, leftWidth, statsHeight, withAlpha(CYAN, 45), 1);
    draw::text(renderer, "JOGADOR " + number(index + 1), leftX + 15, statsY + 13, 1, player.color);
    draw::text(renderer, "K " + number(player.kills) + " KILLS", leftX + 15, statsY + 43, 2, PINK);
    draw::text(renderer, "G " + number(player.coins) + "   S " + number(profileSilver), leftX + 15, statsY + 72, 2, WHITE);

    draw::fillRect(renderer, leftX, barsY, leftWidth, barsHeight, {2, 6, 20, 230});
    draw::outlineRect(renderer, leftX, barsY, leftWidth, barsHeight, withAlpha(CYAN, 45), 1);
    draw::text(renderer, "INTEGRIDADE", leftX + 15, barsY + 10, 1, MUTED);
    draw::fillRect(renderer, leftX + 15, barsY + 29, leftWidth - 30, 8, {30, 35, 50, 220});
    draw::fillRect(renderer, leftX + 15, barsY + 29, static_cast<int>((leftWidth - 30) * std::max(0.0f, player.hp / player.maxHp)), 8, {255, 88, 55, 255});
    draw::text(renderer, std::string("HABILIDADE [") + buttonName(player.keys[ActionSkill]) + "]", leftX + 15, barsY + 48, 1, MUTED);
    draw::fillRect(renderer, leftX + 15, barsY + 69, leftWidth - 30, 8, {30, 35, 50, 220});
    float skillRatio = player.timeStop > 0.0f ? player.timeStop / player.skillDuration : 1.0f - player.skillCooldown / player.skillCooldownMax;
    draw::fillRect(renderer, leftX + 15, barsY + 69, static_cast<int>((leftWidth - 30) * clampf(skillRatio, 0.0f, 1.0f)), 8, player.timeStop > 0.0f ? RED : player.color);

    if (viewport.w >= 620)
    {
        const int shopWidth = 260;
        const int shopHeight = 130;
        const int shopX = viewport.x + viewport.w - padding - shopWidth;
        const int shopY = bottom - shopHeight;
        draw::fillRect(renderer, shopX, shopY, shopWidth, shopHeight, {2, 6, 20, 230});
        draw::outlineRect(renderer, shopX, shopY, shopWidth, shopHeight, withAlpha(CYAN, 45), 1);
        draw::text(renderer, "UPGRADES", shopX + 15, shopY + 12, 1, MUTED);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionUpgradeFire]) + "]", shopX + 15, shopY + 40, 1, MUTED);
        draw::text(renderer, "TIRO", shopX + 88, shopY + 39, 1, WHITE);
        draw::text(renderer, number(player.fireCost) + " G", shopX + 215, shopY + 39, 1, GOLD);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionUpgradeDamage]) + "]", shopX + 15, shopY + 68, 1, MUTED);
        draw::text(renderer, "DANO", shopX + 88, shopY + 67, 1, WHITE);
        draw::text(renderer, number(player.damageCost) + " G", shopX + 215, shopY + 67, 1, GOLD);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionUpgradeSkill]) + "]", shopX + 15, shopY + 96, 1, MUTED);
        draw::text(renderer, "HAB.", shopX + 88, shopY + 95, 1, WHITE);
        draw::text(renderer, number(player.skillCost) + " G", shopX + 215, shopY + 95, 1, GOLD);
    }
}

void Game::Impl::renderViewport(const Viewport& viewport, const Player& cameraPlayer, int playerIndex)
{
    draw::setClipRect(viewport.x, viewport.y, viewport.w, viewport.h);
    draw::fillRect(renderer, viewport.x, viewport.y, viewport.w, viewport.h, BG_BLUE);

    const float backgroundTime = lastTick / 1000.0f;
    if (graphicsQuality == GraphicsQuality::High)
    {
        const int driftX = static_cast<int>(std::sin(backgroundTime * 0.12f) * 35.0f);
        const int driftY = static_cast<int>(std::cos(backgroundTime * 0.09f) * 25.0f);
        draw::fillRect(renderer, viewport.x + viewport.w / 12 + driftX, viewport.y + viewport.h * 2 / 3 + driftY, viewport.w * 2 / 3, viewport.h / 4, {70, 20, 170, 18});
        draw::fillRect(renderer, viewport.x + viewport.w / 2 - driftX, viewport.y + viewport.h / 10 - driftY, viewport.w / 2, viewport.h / 4, {0, 120, 200, 13});
        draw::fillRect(renderer, viewport.x + viewport.w / 3, viewport.y + viewport.h / 3, viewport.w / 3, viewport.h / 3, {180, 0, 100, 9});
    }
    const int stars = qualityStars();
    for (int i = 0; i < stars; ++i)
    {
        const int x = viewport.x + ((i * 83 + 17) % 997) * viewport.w / 997;
        const int y = viewport.y + ((i * 173 + 71) % 991) * viewport.h / 991;
        const int alpha = 45 + static_cast<int>((std::sin(backgroundTime * (1.2f + (i % 5) * 0.4f) + i * 2.7f) + 1.0f) * 45.0f);
        const Color star = i % 7 == 0 ? withAlpha(PURPLE, alpha) : (i % 5 == 0 ? withAlpha(GOLD, alpha) : withAlpha(CYAN, alpha));
        const int size = graphicsQuality == GraphicsQuality::High && i % 9 == 0 ? 3 : 2;
        draw::fillRect(renderer, x, y, size, size, star);
    }
    if (anyTimeStop()) draw::fillRect(renderer, viewport.x, viewport.y, viewport.w, viewport.h, {0, 100, 220, 18});

    const int grid = 80;
    const float leftWorld = cameraPlayer.cameraX - viewport.w * 0.5f;
    const float topWorld = cameraPlayer.cameraY - viewport.h * 0.5f;
    const int firstX = static_cast<int>(std::floor(leftWorld / grid)) * grid;
    const int firstY = static_cast<int>(std::floor(topWorld / grid)) * grid;
    for (int worldX = firstX; worldX <= leftWorld + viewport.w; worldX += grid)
    {
        const Vec2 a = toScreen(viewport, cameraPlayer, static_cast<float>(worldX), topWorld);
        draw::line(renderer, static_cast<int>(a.x), viewport.y, static_cast<int>(a.x), viewport.y + viewport.h, {0, 180, 255, static_cast<Uint8>(graphicsQuality == GraphicsQuality::High ? 18 : (graphicsQuality == GraphicsQuality::Medium ? 12 : 6))});
    }
    for (int worldY = firstY; worldY <= topWorld + viewport.h; worldY += grid)
    {
        const Vec2 a = toScreen(viewport, cameraPlayer, leftWorld, static_cast<float>(worldY));
        draw::line(renderer, viewport.x, static_cast<int>(a.y), viewport.x + viewport.w, static_cast<int>(a.y), {0, 180, 255, static_cast<Uint8>(graphicsQuality == GraphicsQuality::High ? 18 : (graphicsQuality == GraphicsQuality::Medium ? 12 : 6))});
    }

    const Vec2 mapTopLeft = toScreen(viewport, cameraPlayer, mapMinX, mapMinY);
    draw::outlineRect(renderer, static_cast<int>(mapTopLeft.x), static_cast<int>(mapTopLeft.y), static_cast<int>(mapMaxX - mapMinX), static_cast<int>(mapMaxY - mapMinY), withAlpha(CYAN, 90), 3);

    for (unsigned i = 0; i < walls.size(); ++i)
    {
        const Vec2 point = toScreen(viewport, cameraPlayer, walls[i].x, walls[i].y);
        if (point.x + walls[i].w < viewport.x || point.x > viewport.x + viewport.w || point.y + walls[i].h < viewport.y || point.y > viewport.y + viewport.h) continue;
        draw::fillRect(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(walls[i].w), static_cast<int>(walls[i].h), {20, 60, 120, 150});
        draw::outlineRect(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(walls[i].w), static_cast<int>(walls[i].h), CYAN_DIM, 2);
    }

    for (unsigned i = 0; i < drops.size(); ++i)
    {
        const float bob = std::sin(drops[i].phase) * 3.0f;
        const Color dropColor = drops[i].type == DropType::Heart ? PINK : (drops[i].type == DropType::Silver ? SILVER : GOLD);
        const Vec2 point = toScreen(viewport, cameraPlayer, drops[i].x, drops[i].y + bob);
        if (drops[i].type == DropType::Heart)
        {
            const int pulse = 12 + static_cast<int>(std::sin(drops[i].phase * 2.0f) * 2.0f);
            draw::glowCircle(renderer, static_cast<int>(point.x) - pulse / 3, static_cast<int>(point.y) - pulse / 4, pulse * 2 / 3, PINK, qualityGlow(10));
            draw::glowCircle(renderer, static_cast<int>(point.x) + pulse / 3, static_cast<int>(point.y) - pulse / 4, pulse * 2 / 3, PINK, qualityGlow(10));
            draw::triangle(renderer, static_cast<int>(point.x) - pulse, static_cast<int>(point.y), static_cast<int>(point.x) + pulse, static_cast<int>(point.y), static_cast<int>(point.x), static_cast<int>(point.y) + pulse, PINK);
        }
        else
        {
            draw::glowCircle(renderer, static_cast<int>(point.x), static_cast<int>(point.y), 8, dropColor, qualityGlow(10));
            draw::fillCircle(renderer, static_cast<int>(point.x) - 2, static_cast<int>(point.y) - 2, 3, withAlpha(WHITE, 120));
        }
    }
    const int particleStep = qualityParticleStep();
    for (unsigned i = 0; i < particles.size(); i += particleStep)
    {
        const Particle& particle = particles[i];
        const Vec2 point = toScreen(viewport, cameraPlayer, particle.x, particle.y);
        if (point.x < viewport.x - 8 || point.x > viewport.x + viewport.w + 8 || point.y < viewport.y - 8 || point.y > viewport.y + viewport.h + 8) continue;
        Color particleColor = particle.color;
        particleColor.a = static_cast<Uint8>(255.0f * clampf(particle.life / particle.maxLife, 0.0f, 1.0f));
        draw::fillRect(renderer, static_cast<int>(point.x), static_cast<int>(point.y), static_cast<int>(particle.size), static_cast<int>(particle.size), particleColor);
    }
    for (unsigned i = 0; i < projectiles.size(); ++i)
    {
        if (graphicsQuality == GraphicsQuality::High)
        {
            for (int trail = projectiles[i].trailCount - 1; trail >= 0; --trail)
            {
                const float ratio = (projectiles[i].trailCount - trail) / 5.0f;
                renderWorldEntityCircle(viewport, cameraPlayer, projectiles[i].trail[trail].x, projectiles[i].trail[trail].y, std::max(1.0f, projectiles[i].radius * ratio), withAlpha(projectiles[i].color, static_cast<int>(80.0f * ratio)), 0);
            }
        }
        renderWorldEntityCircle(viewport, cameraPlayer, projectiles[i].x, projectiles[i].y, projectiles[i].radius, projectiles[i].color, 8);
    }
    for (unsigned i = 0; i < enemyProjectiles.size(); ++i)
        renderWorldEntityCircle(viewport, cameraPlayer, enemyProjectiles[i].x, enemyProjectiles[i].y, enemyProjectiles[i].radius, enemyProjectiles[i].color, 8);
    for (unsigned i = 0; i < enemies.size(); ++i) renderEnemy(viewport, cameraPlayer, enemies[i]);
    renderBoss(viewport, cameraPlayer);
    for (int i = 0; i < playerCount; ++i) renderPlayer(viewport, cameraPlayer, players[i], i);
    renderFloatingTexts(viewport, cameraPlayer);

    if (graphicsQuality == GraphicsQuality::High)
    {
        for (int band = 0; band < 8; ++band)
        {
            const int inset = band * 10;
            const int alpha = 8 + band * 3;
            draw::outlineRect(renderer, viewport.x + inset, viewport.y + inset, viewport.w - inset * 2, viewport.h - inset * 2, {0, 0, 10, static_cast<Uint8>(alpha)}, 10);
        }
        for (int y = viewport.y; y < viewport.y + viewport.h; y += 4)
            draw::fillRect(renderer, viewport.x, y, viewport.w, 1, {110, 180, 255, 6});
    }
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
    if (noticeTimer > 0.0f && !noticeText.empty())
    {
        const int width = std::max(360, draw::textWidth(noticeText, 2) + 70);
        draw::panel(renderer, SCREEN_W / 2 - width / 2, 30, width, 60, GOLD, {0, 4, 16, 235});
        draw::text(renderer, noticeText, SCREEN_W / 2, 51, 2, GOLD, true);
    }
    draw::text(renderer, "FPS " + number(fpsValue), 16, 16, 1, fpsValue >= 55 ? GREEN : RED);
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
