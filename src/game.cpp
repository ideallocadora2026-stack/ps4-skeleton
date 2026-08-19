#include "game.hpp"

#include "audio.hpp"
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
const Color GRENADE_GREEN = {126, 255, 154, 255};
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
    Stats,
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
    ActionGrenade,
    ActionBuyGrenade,
    ActionStats,
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
    Hats,
    Characters
};

enum CharacterType
{
    CharacterTimeStop = 0,
    CharacterDamageArea,
    CharacterShield,
    CharacterDrone,
    CharacterVampire,
    CharacterManipulator,
    CharacterCount
};

enum BossType
{
    BossX = 0,
    BossSquare,
    BossTriangle,
    BossCircle,
    BossDpad,
    BossTouchpad,
    BossCount
};

enum ProjectileKind
{
    ProjectileOrb = 0,
    ProjectileX,
    ProjectileSquare,
    ProjectileTriangle,
    ProjectileArrow,
    ProjectileDebris,
    ProjectileGrenade,
    ProjectileFireShard
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

struct CharacterItem
{
    const char* name;
    const char* description;
    int cost;
    Color color;
};

const CharacterItem CHARACTERS[CharacterCount] = {
    {"TIME STOP", "PARA O TEMPO", 0, CYAN},
    {"DAMAGE AREA", "AREA DE VENENO", 1000, GREEN},
    {"SHIELD", "ESCUDO POR 10S", 1000, WHITE},
    {"THE DRONE", "DRONE DE COMBATE", 1000, GOLD},
    {"VAMPIRE", "ROUBA 90% DE VIDA", 1000, PINK},
    {"THE MANIPULATOR", "CONVERTE 3 INIMIGOS", 1000, PURPLE}
};

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
    int8_t keys[4][5];
    float sensitivity[4];
    float rumble[4];
    uint8_t reserved[64];
};

const uint32_t PROFILE_MAGIC = 0x47575034u;
const uint32_t PROFILE_VERSION = 2u;
const int PROFILE_STORED_ACTIONS = 5;
const uint8_t PROFILE_GRENADE_MARKER = 0x47u;
const uint8_t PROFILE_BUY_GRENADE_MARKER = 0x42u;
const uint8_t PROFILE_VOLUME_MARKER = 0x56u;
const uint8_t PROFILE_CHARACTER_MARKER = 0x43u;
const uint8_t PROFILE_STATS_MARKER = 0x53u;
const int GRENADE_COST = 10;
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
    bool touchActive;
    bool previousTouchActive;
    float touchX;
    float touchY;

    Pad() : handle(-1), userId(-1), ownsHandle(false), connected(false),
            touchActive(false), previousTouchActive(false), touchX(0.0f), touchY(0.0f)
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
    int fireLevel;
    int damageLevel;
    int skillLevel;
    int grenades;
    int character;
    int souls;
    float poisonAreaTimer;
    float shieldTimer;
    float droneTimer;
    float droneFireTimer;
    float droneAngle;
    float droneBeamTimer;
    float droneTargetX;
    float droneTargetY;
    bool pendingRevive;
    int keys[ActionCount];
    float sensitivity;
    float rumbleStrength;
    int skin;
    int hat;
    float cosmeticAnimation;
    bool touchTracking;
    float touchAnchorX;
    float touchAnchorY;
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
    int kind;
    int bounces;
    float angle;
    float rotationSpeed;
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
    int kind;
    int targetPlayer;
    float poisonTimer;
    float poisonVisualTimer;
    int poisonOwner;
    float burnTimer;
    float burnVisualTimer;
    int burnOwner;
};

struct FriendlyBot
{
    bool active;
    int owner;
    int slot;
    float x;
    float y;
    float hp;
    float maxHp;
    float speed;
    float radius;
    float angle;
    float attackTimer;
    Color color;
};

struct FireArea
{
    float x;
    float y;
    float radius;
    float life;
    int owner;
    float phase;
};

struct Soul
{
    float x;
    float y;
    int owner;
    float life;
};

struct LifeStream
{
    float x;
    float y;
    int owner;
    float heal;
    float life;
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

struct BossPart
{
    float x;
    float y;
    float hp;
    float maxHp;
    bool active;
    bool collectible;
    bool collected;
    int collector;
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
    float velocityX;
    float velocityY;
    float rotationDirection;
    int state;
    float stateTimer;
    float sweepRemaining;
    bool laserActive;
    float droneTimer;
    int droneCount;
    int bounceCount;
    int telegraphDirection;
    int memoryPlayer;
    int memoryRound;
    int memoryLives;
    int memoryState;
    int memorySequence[8];
    int memoryShowIndex;
    int memoryInputIndex;
    float memoryTimer;
    uint32_t memoryRemovedMask;
    int memoryFlashDirection;
    float memoryFlashTimer;
    bool shootSuppressed;
    bool freezeActive;
    float abilityTimer;
    Color color;
    Color trueColor;
    BossWeapon weapons[3];
    BossPart parts[4];
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

Color rainbowColor(float phase)
{
    const float red = 127.5f + std::sin(phase) * 127.5f;
    const float green = 127.5f + std::sin(phase + PI * 2.0f / 3.0f) * 127.5f;
    const float blue = 127.5f + std::sin(phase + PI * 4.0f / 3.0f) * 127.5f;
    return {static_cast<Uint8>(red), static_cast<Uint8>(green), static_cast<Uint8>(blue), 255};
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

bool isBossWaveNumber(int value)
{
    return value >= 5 && value % 5 == 0;
}

float pointSegmentDistanceSquared(float px, float py, float ax, float ay, float bx, float by)
{
    const float abX = bx - ax;
    const float abY = by - ay;
    const float lengthSquared = abX * abX + abY * abY;
    if (lengthSquared <= 0.0001f) return distanceSquared(px, py, ax, ay);
    const float t = clampf(((px - ax) * abX + (py - ay) * abY) / lengthSquared, 0.0f, 1.0f);
    return distanceSquared(px, py, ax + abX * t, ay + abY * t);
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
          totalKills(0), profileSilver(0), lobbyTimer(3.0f), localRequested(false), enemyEdges(0), lastBossShape(-1),
          enemyColor(RED), graphicsQuality(GraphicsQuality::High), ownedSkins(1u), ownedHats(1u), ownedCharacters(1u),
          shopTab(ShopTab::Skins), controlIndex(0), bindingAction(-1), noticeTimer(0),
          resetConfirmTimer(0), profileDirty(false), disconnectedPlayers(0), controllerRefreshTick(0),
          menuTravelEffect(0), panelEntryEffect(0.28f), renderedScreen(Screen::Menu),
          musicVolume(1.0f), soundVolume(1.0f), coinSoundAlternate(false),
          intermissionActive(false), combatCleanupPending(false), portalCharge(0.0f), statsBotIndex(0),
          backdropCacheQuality(-1), playingCacheQuality(-1), playingCacheLayout(-1),
          playingCacheDriftX(1000000), playingCacheDriftY(1000000)
    {
        std::memset(&boss, 0, sizeof(boss));
        std::memset(nativePadHandles, -1, sizeof(nativePadHandles));
        std::memset(rumbleTimers, 0, sizeof(rumbleTimers));
        std::memset(activeSkins, 0, sizeof(activeSkins));
        std::memset(activeHats, 0, sizeof(activeHats));
        std::memset(activeCharacters, 0, sizeof(activeCharacters));
        std::memset(friendlyBots, 0, sizeof(friendlyBots));
        resetSettings();
    }

    ~Impl()
    {
        audio.shutdown();
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
    AudioEngine audio;
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
    int lastBossShape;
    Color enemyColor;
    GraphicsQuality graphicsQuality;
    uint32_t ownedSkins;
    uint32_t ownedHats;
    uint32_t ownedCharacters;
    uint8_t activeSkins[4];
    uint8_t activeHats[4];
    uint8_t activeCharacters[4];
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
    float menuTravelEffect;
    float panelEntryEffect;
    Screen renderedScreen;
    float musicVolume;
    float soundVolume;
    bool coinSoundAlternate;
    bool intermissionActive;
    bool combatCleanupPending;
    float portalCharge;
    int statsBotIndex;
    std::vector<uint32_t> backdropCache;
    int backdropCacheQuality;
    std::vector<uint32_t> playingBackgroundCache;
    int playingCacheQuality;
    int playingCacheLayout;
    int playingCacheDriftX;
    int playingCacheDriftY;

    std::vector<Projectile> projectiles;
    std::vector<Projectile> enemyProjectiles;
    std::vector<Enemy> enemies;
    std::vector<Drop> drops;
    std::vector<Particle> particles;
    std::vector<FloatingText> floatingTexts;
    std::vector<Soul> souls;
    std::vector<LifeStream> lifeStreams;
    std::vector<FireArea> fireAreas;
    std::vector<Wall> walls;
    FriendlyBot friendlyBots[4][3];
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
    void updateStats(float dt);
    void startGame(int count);
    void resetWorld();
    void nextWave();
    void spawnEnemy();
    void spawnBoss();
    void updateBoss(float dt);
    void setBossPhase(int phase);
    void bossWeaponPosition(int index, float& x, float& y) const;
    void damageBossPart(int index, float damage);
    void spawnBossDrone();
    void spawnArrowWall(int direction);
    void startMemoryRound();
    void failMemoryRound();
    void damageBoss(float damage, int weaponIndex);
    void defeatBoss();
    void addEnemyProjectile(float x, float y, float angle, float speed, float radius, Color color,
                            int kind = ProjectileOrb, int bounces = 0, float damage = 10.0f);
    void shoot(Player& player, int playerIndex);
    void throwGrenade(Player& player, int playerIndex);
    void explodeGrenade(float x, float y, int owner);
    void createFireArea(float x, float y, int owner);
    void updateFireAreas(float dt);
    bool anyTimeStop() const;
    bool skillReady(const Player& player) const;
    void configureCharacter(Player& player);
    void activateSkill(Player& player, int playerIndex);
    void updateCharacterAbilities(float dt);
    void updateSouls(float dt);
    void fireCompanionDrone(Player& player, int playerIndex);
    void manipulateEnemies(Player& player, int playerIndex);
    void updateFriendlyBots(float dt);
    void repairFriendlyBot(Player& player, int playerIndex, int slot);
    void addParticles(float x, float y, Color color, int count, float speed, float life = 0.7f);
    void addFloatingText(float x, float y, const std::string& text, Color color);
    void addDrop(float x, float y, DropType type);
    void rewardDefeatedEnemy(const Enemy& enemy, int owner);
    void damagePlayer(Player& player, float damage);
    void buyUpgrade(Player& player, int type);
    void buyMechanicUpgrade(Player& player, int type);
    bool nearMechanic(const Player& player) const;
    void updateIntermission(float dt);
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
    void renderStats();
    void renderGameOver();
    void renderPause();
    void renderPlaying();
    void paintViewportBase(const Viewport& viewport, int driftX, int driftY);
    void preparePlayingBackground();
    void renderViewport(const Viewport& viewport, const Player& cameraPlayer, int playerIndex);
    void renderWorldEntityCircle(const Viewport& viewport, const Player& cameraPlayer, float x, float y, float radius, Color color, int glow);
    Vec2 toScreen(const Viewport& viewport, const Player& cameraPlayer, float x, float y) const;
    void renderEnemy(const Viewport& viewport, const Player& cameraPlayer, const Enemy& enemy);
    void renderBoss(const Viewport& viewport, const Player& cameraPlayer);
    void renderPlayer(const Viewport& viewport, const Player& cameraPlayer, const Player& player, int index);
    void renderCharacterEffects(const Viewport& viewport, const Player& cameraPlayer, const Player& player, int index);
    void renderIntermission(const Viewport& viewport, const Player& cameraPlayer);
    void renderAvatar(int x, int y, int radius, Color color, int skin, int hat, float animation, bool dead);
    void renderFloatingTexts(const Viewport& viewport, const Player& cameraPlayer);
    void renderHud(const Viewport& viewport, const Player& player, int index);
    void renderMenuOptions(const std::vector<std::string>& options, int startY, int scale);
    int qualityGlow(int value) const;
    int qualityStars() const;
    int qualityParticleStep() const;
    int qualityParticleLimit() const;
    int targetFps() const;
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
        settings[i].keys[ActionGrenade] = PAD_L2;
        settings[i].keys[ActionBuyGrenade] = PAD_SQUARE;
        settings[i].keys[ActionStats] = PAD_L3;
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
    ownedCharacters = 1u;
    std::memset(activeSkins, 0, sizeof(activeSkins));
    std::memset(activeHats, 0, sizeof(activeHats));
    std::memset(activeCharacters, 0, sizeof(activeCharacters));
    graphicsQuality = GraphicsQuality::High;
    musicVolume = 1.0f;
    soundVolume = 1.0f;
    audio.setVolumes(musicVolume, soundVolume);
    resetSettings();
    resetConfirmTimer = 0.0f;
    for (int i = 0; i < playerCount; ++i)
    {
        const int slot = std::max(0, std::min(3, players[i].pad));
        players[i].skin = 0;
        players[i].hat = 0;
        players[i].character = CharacterTimeStop;
        players[i].skillLevel = 1;
        players[i].souls = 0;
        configureCharacter(players[i]);
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
    ownedCharacters = 1u;
    const uint32_t validMask = (1u << COSMETIC_COUNT) - 1u;
    ownedSkins &= validMask;
    ownedHats &= validMask;
    for (int i = 0; i < 4; ++i)
    {
        activeSkins[i] = profile.activeSkin[i] < COSMETIC_COUNT && (ownedSkins & (1u << profile.activeSkin[i])) ? profile.activeSkin[i] : 0;
        activeHats[i] = profile.activeHat[i] < COSMETIC_COUNT && (ownedHats & (1u << profile.activeHat[i])) ? profile.activeHat[i] : 0;
        for (int action = 0; action < PROFILE_STORED_ACTIONS; ++action)
        {
            const int key = profile.keys[i][action];
            if (key >= 0 && key < PAD_BUTTON_COUNT) settings[i].keys[action] = key;
        }
        if (profile.reserved[0] == PROFILE_GRENADE_MARKER)
        {
            const int grenadeKey = profile.reserved[1 + i];
            if (grenadeKey >= 0 && grenadeKey < PAD_BUTTON_COUNT) settings[i].keys[ActionGrenade] = grenadeKey;
        }
        if (profile.reserved[8] == PROFILE_BUY_GRENADE_MARKER)
        {
            const int buyGrenadeKey = profile.reserved[9 + i];
            if (buyGrenadeKey >= 0 && buyGrenadeKey < PAD_BUTTON_COUNT) settings[i].keys[ActionBuyGrenade] = buyGrenadeKey;
        }
        if (profile.reserved[20] == PROFILE_CHARACTER_MARKER)
        {
            ownedCharacters = static_cast<uint32_t>(profile.reserved[21]) | 1u;
            const int character = profile.reserved[22 + i];
            activeCharacters[i] = character >= 0 && character < CharacterCount && (ownedCharacters & (1u << character)) ? static_cast<uint8_t>(character) : 0;
        }
        if (profile.reserved[27] == PROFILE_STATS_MARKER)
        {
            const int statsKey = profile.reserved[28 + i];
            if (statsKey >= 0 && statsKey < PAD_BUTTON_COUNT) settings[i].keys[ActionStats] = statsKey;
        }
        settings[i].sensitivity = clampf(profile.sensitivity[i], 0.5f, 3.0f);
        settings[i].rumble = clampf(profile.rumble[i], 0.0f, 2.0f);
    }
    if (profile.reserved[16] == PROFILE_VOLUME_MARKER)
    {
        musicVolume = clampf(profile.reserved[17] / 100.0f, 0.0f, 1.0f);
        soundVolume = clampf(profile.reserved[18] / 100.0f, 0.0f, 1.0f);
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
        for (int action = 0; action < PROFILE_STORED_ACTIONS; ++action) profile.keys[i][action] = static_cast<int8_t>(settings[i].keys[action]);
        profile.reserved[1 + i] = static_cast<uint8_t>(settings[i].keys[ActionGrenade]);
        profile.reserved[9 + i] = static_cast<uint8_t>(settings[i].keys[ActionBuyGrenade]);
        profile.reserved[22 + i] = activeCharacters[i];
        profile.reserved[28 + i] = static_cast<uint8_t>(settings[i].keys[ActionStats]);
        profile.sensitivity[i] = settings[i].sensitivity;
        profile.rumble[i] = settings[i].rumble;
    }
    profile.reserved[0] = PROFILE_GRENADE_MARKER;
    profile.reserved[8] = PROFILE_BUY_GRENADE_MARKER;
    profile.reserved[16] = PROFILE_VOLUME_MARKER;
    profile.reserved[17] = static_cast<uint8_t>(musicVolume * 100.0f + 0.5f);
    profile.reserved[18] = static_cast<uint8_t>(soundVolume * 100.0f + 0.5f);
    profile.reserved[20] = PROFILE_CHARACTER_MARKER;
    profile.reserved[21] = static_cast<uint8_t>(ownedCharacters & 0xffu);
    profile.reserved[27] = PROFILE_STATS_MARKER;
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

int Game::Impl::targetFps() const
{
    return 60;
}

bool Game::Impl::initialize()
{
    loadProfile();
    sceUserServiceInitialize(nullptr);
    scePadInit();
    audio.setVolumes(musicVolume, soundVolume);
    audio.initialize();
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
        audio.setControllerUser(i, desired);
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
        pads[p].previousTouchActive = pads[p].touchActive;
        pads[p].touchActive = false;
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
        if (data.touch.fingers > 0)
        {
            pads[p].touchActive = true;
            pads[p].touchX = static_cast<float>(data.touch.touch[0].x);
            pads[p].touchY = static_cast<float>(data.touch.touch[0].y);
        }
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
    const bool activeSession = screen == Screen::Playing || screen == Screen::Paused || screen == Screen::Stats ||
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
    menuTravelEffect = std::max(0.0f, menuTravelEffect - dt);
    panelEntryEffect = std::max(0.0f, panelEntryEffect - dt);
    const int previousMenuIndex = menuIndex;
    const Screen previousScreen = screen;

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
        case Screen::Stats:
            updateStats(dt);
            break;
        case Screen::Lobby:
            updateLobby(dt);
            break;
        case Screen::Playing:
            updatePlaying(dt);
            break;
    }
    if (menuIndex != previousMenuIndex) menuTravelEffect = 0.42f;
    if (screen != previousScreen || screen != renderedScreen)
    {
        renderedScreen = screen;
        panelEntryEffect = 0.28f;
        menuTravelEffect = 0.55f;
    }
    audio.setMusicMode(screen == Screen::Shop ? MusicMode::Shop : (boss.active ? MusicMode::Boss : MusicMode::General));
    draw::setThinText(screen != Screen::Menu);
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

    const int optionCount = ActionCount + 6;
    if (pressed(pad, PAD_UP)) controlIndex = (controlIndex + optionCount - 1) % optionCount;
    if (pressed(pad, PAD_DOWN)) controlIndex = (controlIndex + 1) % optionCount;

    int direction = 0;
    if (pressed(pad, PAD_LEFT)) direction = -1;
    if (pressed(pad, PAD_RIGHT)) direction = 1;
    if (direction != 0 && controlIndex >= ActionCount && controlIndex <= ActionCount + 4)
    {
        if (controlIndex == ActionCount)
        {
            int quality = static_cast<int>(graphicsQuality);
            quality = (quality + direction + 3) % 3;
            graphicsQuality = static_cast<GraphicsQuality>(quality);
        }
        else if (controlIndex == ActionCount + 1) config.sensitivity = clampf(config.sensitivity + direction * 0.1f, 0.5f, 3.0f);
        else if (controlIndex == ActionCount + 2) config.rumble = clampf(config.rumble + direction * 0.1f, 0.0f, 2.0f);
        else if (controlIndex == ActionCount + 3) musicVolume = clampf(musicVolume + direction * 0.05f, 0.0f, 1.0f);
        else soundVolume = clampf(soundVolume + direction * 0.05f, 0.0f, 1.0f);
        audio.setVolumes(musicVolume, soundVolume);
        profileDirty = true;
        saveProfile(false);
    }

    if (pressed(pad, PAD_CIRCLE) || (controlIndex == ActionCount + 5 && pressed(pad, PAD_CROSS)))
    {
        saveProfile(false);
        screen = returnScreen;
        menuIndex = 0;
        return;
    }
    if (pressed(pad, PAD_CROSS))
    {
        if (controlIndex < ActionCount) bindingAction = controlIndex;
        else if (controlIndex >= ActionCount && controlIndex <= ActionCount + 4)
        {
            if (controlIndex == ActionCount)
                graphicsQuality = static_cast<GraphicsQuality>((static_cast<int>(graphicsQuality) + 1) % 3);
            else if (controlIndex == ActionCount + 1)
                config.sensitivity = config.sensitivity >= 3.0f ? 0.5f : clampf(config.sensitivity + 0.1f, 0.5f, 3.0f);
            else if (controlIndex == ActionCount + 2)
                config.rumble = config.rumble >= 2.0f ? 0.0f : clampf(config.rumble + 0.1f, 0.0f, 2.0f);
            else if (controlIndex == ActionCount + 3)
                musicVolume = musicVolume >= 1.0f ? 0.0f : clampf(musicVolume + 0.05f, 0.0f, 1.0f);
            else
                soundVolume = soundVolume >= 1.0f ? 0.0f : clampf(soundVolume + 0.05f, 0.0f, 1.0f);
            audio.setVolumes(musicVolume, soundVolume);
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
    const int tabCount = 3;
    const int itemStart = 3;
    const int backIndex = 15;
    const int itemCount = shopTab == ShopTab::Characters ? CharacterCount : COSMETIC_COUNT;
    if (pressed(pad, PAD_CIRCLE))
    {
        saveProfile(false);
        screen = returnScreen;
        menuIndex = 0;
        return;
    }

    if (menuIndex < tabCount)
    {
        if (pressed(pad, PAD_LEFT)) menuIndex = (menuIndex + tabCount - 1) % tabCount;
        if (pressed(pad, PAD_RIGHT)) menuIndex = (menuIndex + 1) % tabCount;
        shopTab = static_cast<ShopTab>(menuIndex);
        if (pressed(pad, PAD_DOWN)) menuIndex = itemStart;
    }
    else if (menuIndex < backIndex)
    {
        const int lastItem = itemStart + itemCount - 1;
        menuIndex = std::min(menuIndex, lastItem);
        if (pressed(pad, PAD_LEFT)) menuIndex = std::max(itemStart, menuIndex - 1);
        if (pressed(pad, PAD_RIGHT)) menuIndex = std::min(lastItem, menuIndex + 1);
        if (pressed(pad, PAD_UP)) menuIndex = menuIndex - 3 >= itemStart ? menuIndex - 3 : static_cast<int>(shopTab);
        if (pressed(pad, PAD_DOWN)) menuIndex = menuIndex + 3 <= lastItem ? menuIndex + 3 : backIndex;
    }
    else if (pressed(pad, PAD_UP) || pressed(pad, PAD_LEFT) || pressed(pad, PAD_RIGHT)) menuIndex = itemStart + itemCount - 1;

    if (!pressed(pad, PAD_CROSS)) return;
    if (menuIndex < tabCount)
    {
        shopTab = static_cast<ShopTab>(menuIndex);
        return;
    }
    if (menuIndex == backIndex)
    {
        saveProfile(false);
        screen = returnScreen;
        menuIndex = 0;
        return;
    }

    const int item = menuIndex - itemStart;
    if (shopTab == ShopTab::Characters)
    {
        if (item < 0 || item >= CharacterCount) return;
        const uint32_t mask = 1u << item;
        if ((ownedCharacters & mask) == 0)
        {
            if (profileSilver < CHARACTERS[item].cost)
            {
                showNotice("PRATA INSUFICIENTE");
                return;
            }
            profileSilver -= CHARACTERS[item].cost;
            ownedCharacters |= mask;
            showNotice("PERSONAGEM COMPRADO");
        }
        activeCharacters[pad] = static_cast<uint8_t>(item);
        for (int i = 0; i < playerCount; ++i)
        {
            if (players[i].pad != pad) continue;
            players[i].character = item;
            players[i].timeStop = 0.0f;
            players[i].poisonAreaTimer = 0.0f;
            players[i].shieldTimer = 0.0f;
            players[i].droneTimer = 0.0f;
            players[i].skillCooldown = 0.0f;
            configureCharacter(players[i]);
        }
        profileDirty = true;
        saveProfile(false);
        return;
    }
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

void Game::Impl::updateStats(float)
{
    int playerIndex = 0;
    for (int i = 0; i < playerCount; ++i) if (players[i].pad == pausePad) { playerIndex = i; break; }
    Player& player = players[playerIndex];
    if (pressed(pausePad, PAD_CIRCLE) || pressed(pausePad, player.keys[ActionStats])) screen = Screen::Playing;
    if (player.character == CharacterManipulator)
    {
        if (pressed(pausePad, PAD_UP)) statsBotIndex = (statsBotIndex + 2) % 3;
        if (pressed(pausePad, PAD_DOWN)) statsBotIndex = (statsBotIndex + 1) % 3;
        if (pressed(pausePad, PAD_CROSS)) repairFriendlyBot(player, playerIndex, statsBotIndex);
    }
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
    souls.clear();
    lifeStreams.clear();
    fireAreas.clear();
    walls.clear();
    std::memset(friendlyBots, 0, sizeof(friendlyBots));
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
    intermissionActive = false;
    combatCleanupPending = false;
    portalCharge = 0.0f;
    statsBotIndex = 0;
    cameraShake = 0.0f;
    totalKills = 0;
    enemyEdges = 0;
    lastBossShape = -1;
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
        player.damageCost = 30;
        player.skillCost = 25;
        player.fireLevel = 1;
        player.damageLevel = 1;
        player.skillLevel = 1;
        player.grenades = 0;
        player.pendingRevive = false;
        const int slot = std::max(0, std::min(3, player.pad));
        for (int action = 0; action < ActionCount; ++action) player.keys[action] = settings[slot].keys[action];
        player.sensitivity = settings[slot].sensitivity;
        player.rumbleStrength = settings[slot].rumble;
        player.skin = activeSkins[slot];
        player.hat = activeHats[slot];
        player.character = activeCharacters[slot];
        player.souls = 0;
        player.poisonAreaTimer = 0.0f;
        player.shieldTimer = 0.0f;
        player.droneTimer = 0.0f;
        player.droneFireTimer = 0.0f;
        player.droneAngle = 0.0f;
        player.droneBeamTimer = 0.0f;
        player.droneTargetX = player.x;
        player.droneTargetY = player.y;
        player.cosmeticAnimation = 0.0f;
        player.touchTracking = false;
        player.touchAnchorX = 0.0f;
        player.touchAnchorY = 0.0f;
        configureCharacter(player);
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

bool Game::Impl::skillReady(const Player& player) const
{
    if (player.character == CharacterVampire) return player.souls >= 50;
    if (player.skillCooldown > 0.0f) return false;
    if (player.character == CharacterTimeStop) return player.timeStop <= 0.0f;
    if (player.character == CharacterDamageArea) return player.poisonAreaTimer <= 0.0f;
    if (player.character == CharacterShield) return player.shieldTimer <= 0.0f;
    if (player.character == CharacterDrone) return player.droneTimer <= 0.0f;
    return true;
}

void Game::Impl::configureCharacter(Player& player)
{
    const float levelBonus = std::max(0, player.skillLevel - 1) * 0.5f;
    if (player.character == CharacterDamageArea)
    {
        player.skillDuration = 3.0f + levelBonus;
        player.skillCooldownMax = std::max(3.0f, 5.0f - (player.skillLevel - 1) * 0.2f);
    }
    else if (player.character == CharacterShield)
    {
        player.skillDuration = 10.0f + levelBonus;
        player.skillCooldownMax = std::max(10.0f, 20.0f - (player.skillLevel - 1) * 0.8f);
    }
    else if (player.character == CharacterDrone)
    {
        player.skillDuration = 8.0f + levelBonus;
        player.skillCooldownMax = std::max(8.0f, 16.0f - (player.skillLevel - 1) * 0.6f);
    }
    else if (player.character == CharacterVampire)
    {
        player.skillDuration = 0.0f;
        player.skillCooldownMax = 1.0f;
    }
    else if (player.character == CharacterManipulator)
    {
        player.skillDuration = 0.0f;
        player.skillCooldownMax = std::max(8.0f, 18.0f - (player.skillLevel - 1) * 0.8f);
    }
    else
    {
        player.skillDuration = 3.0f + levelBonus;
        player.skillCooldownMax = std::max(5.0f, 12.0f - (player.skillLevel - 1));
    }
}

void Game::Impl::activateSkill(Player& player, int playerIndex)
{
    if (!skillReady(player)) return;
    if (player.character == CharacterDamageArea)
    {
        player.poisonAreaTimer = player.skillDuration;
        player.skillCooldown = player.skillCooldownMax;
        addFloatingText(player.x, player.y - 34.0f, "DAMAGE AREA!", GREEN);
    }
    else if (player.character == CharacterShield)
    {
        player.shieldTimer = player.skillDuration;
        player.skillCooldown = player.skillCooldownMax;
        addFloatingText(player.x, player.y - 34.0f, "SHIELD!", WHITE);
    }
    else if (player.character == CharacterDrone)
    {
        player.droneTimer = player.skillDuration;
        player.droneFireTimer = 0.0f;
        player.skillCooldown = player.skillCooldownMax;
        addFloatingText(player.x, player.y - 34.0f, "THE DRONE!", GOLD);
    }
    else if (player.character == CharacterVampire)
    {
        int nearestIndex = -1;
        float nearest = 1e30f;
        for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
        {
            if (enemies[i].kind == 1) continue;
            const float d = distanceSquared(player.x, player.y, enemies[i].x, enemies[i].y);
            if (d < nearest) { nearest = d; nearestIndex = i; }
        }
        if (nearestIndex < 0)
        {
            addFloatingText(player.x, player.y - 34.0f, "SEM ALVO", MUTED);
            return;
        }
        Enemy& target = enemies[nearestIndex];
        const float stolen = std::max(1.0f, target.hp * 0.90f);
        target.hp = std::max(1.0f, target.hp - stolen);
        LifeStream stream = {target.x, target.y, playerIndex, stolen, 2.0f};
        lifeStreams.push_back(stream);
        player.souls = 0;
        addFloatingText(target.x, target.y - 30.0f, "VIDA CONVERTIDA", PINK);
    }
    else if (player.character == CharacterManipulator)
    {
        manipulateEnemies(player, playerIndex);
        return;
    }
    else
    {
        player.timeStop = player.skillDuration;
        player.skillCooldown = player.skillCooldownMax;
        addFloatingText(player.x, player.y - 30.0f, "TEMPO PARADO!", player.color);
    }
    addParticles(player.x, player.y, CHARACTERS[player.character].color, 38, 380.0f, 0.9f);
    rumble(player.pad, 0.6f, 500);
}

void Game::Impl::fireCompanionDrone(Player& player, int playerIndex)
{
    player.droneTargetX = player.x;
    player.droneTargetY = player.y;
    int nearestIndex = -1;
    float nearest = 1e30f;
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
    {
        const float d = distanceSquared(player.x, player.y, enemies[i].x, enemies[i].y);
        if (d < nearest) { nearest = d; nearestIndex = i; }
    }
    if (nearestIndex >= 0)
    {
        Enemy& target = enemies[nearestIndex];
        player.droneTargetX = target.x;
        player.droneTargetY = target.y;
        target.hp -= target.maxHp * 0.50f;
        if (target.hp <= 0.0f)
        {
            const Enemy defeated = target;
            rewardDefeatedEnemy(defeated, playerIndex);
            addParticles(defeated.x, defeated.y, defeated.color, 18, 330.0f, 0.8f);
            enemies.erase(enemies.begin() + nearestIndex);
        }
    }
    else if (boss.active)
    {
        player.droneTargetX = boss.x;
        player.droneTargetY = boss.y;
        if (boss.phase == 1)
        {
            for (int i = 0; i < 3; ++i)
                if (boss.weapons[i].active) { damageBoss(boss.weapons[i].maxHp * 0.10f, i); break; }
        }
        else if (boss.phase == 2 && boss.shape == BossSquare)
        {
            for (int i = 0; i < 4; ++i)
                if (boss.parts[i].active) { player.droneTargetX = boss.parts[i].x; player.droneTargetY = boss.parts[i].y; damageBossPart(i, boss.parts[i].maxHp * 0.10f); break; }
        }
        else damageBoss((boss.phase == 2 ? boss.maxHp : boss.maxRageHp) * 0.10f, -1);
    }
    player.droneBeamTimer = 0.16f;
}

void Game::Impl::manipulateEnemies(Player& player, int playerIndex)
{
    int converted = 0;
    for (int slot = 0; slot < 3; ++slot)
    {
        FriendlyBot& bot = friendlyBots[playerIndex][slot];
        if (bot.active) continue;
        int nearestIndex = -1;
        float nearest = 1e30f;
        for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
        {
            if (enemies[i].kind == 1) continue;
            const float d = distanceSquared(player.x, player.y, enemies[i].x, enemies[i].y);
            if (d < nearest) { nearest = d; nearestIndex = i; }
        }
        if (nearestIndex < 0) break;
        const Enemy source = enemies[nearestIndex];
        bot.active = true;
        bot.owner = playerIndex;
        bot.slot = slot;
        bot.x = source.x;
        bot.y = source.y;
        bot.hp = source.maxHp;
        bot.maxHp = source.maxHp;
        bot.speed = std::max(180.0f, source.speed * 0.9f);
        bot.radius = std::max(13.0f, source.radius);
        bot.angle = source.angle;
        bot.attackTimer = 0.15f * slot;
        bot.color = player.color;
        enemies.erase(enemies.begin() + nearestIndex);
        addParticles(bot.x, bot.y, PURPLE, 26, 300.0f, 0.9f);
        addFloatingText(bot.x, bot.y - 26.0f, "BOT " + number(slot + 1) + " CONVERTIDO", PURPLE);
        ++converted;
    }
    if (converted == 0)
    {
        addFloatingText(player.x, player.y - 34.0f, "SEM ESPACO OU ALVO", MUTED);
        return;
    }
    player.skillCooldown = player.skillCooldownMax;
    rumble(player.pad, 0.65f, 420);
}

void Game::Impl::repairFriendlyBot(Player& player, int playerIndex, int slot)
{
    if (slot < 0 || slot >= 3) return;
    FriendlyBot& bot = friendlyBots[playerIndex][slot];
    if (!bot.active) { showNotice("BOT " + number(slot + 1) + " INATIVO"); return; }
    if (bot.hp >= bot.maxHp) { showNotice("BOT COM VIDA CHEIA"); return; }
    if (player.coins < 5) { showNotice("G INSUFICIENTE"); return; }
    player.coins -= 5;
    const float restored = bot.maxHp * 0.5f;
    bot.hp = std::min(bot.maxHp, bot.hp + restored);
    addParticles(bot.x, bot.y, GREEN, 18, 220.0f, 0.7f);
    showNotice("BOT " + number(slot + 1) + " REPARADO");
}

void Game::Impl::updateFriendlyBots(float dt)
{
    for (int owner = 0; owner < playerCount; ++owner)
    {
        for (int slot = 0; slot < 3; ++slot)
        {
            FriendlyBot& bot = friendlyBots[owner][slot];
            if (!bot.active) continue;
            bot.attackTimer = std::max(0.0f, bot.attackTimer - dt);
            bot.angle += dt * 2.8f;
            int enemyIndex = -1;
            float nearest = 1e30f;
            float targetX = players[owner].x + std::cos(slot * PI * 2.0f / 3.0f) * 64.0f;
            float targetY = players[owner].y + std::sin(slot * PI * 2.0f / 3.0f) * 64.0f;
            bool bossTarget = false;
            int bossSubTarget = -1;
            for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
            {
                const float d = distanceSquared(bot.x, bot.y, enemies[i].x, enemies[i].y);
                if (d < nearest) { nearest = d; enemyIndex = i; targetX = enemies[i].x; targetY = enemies[i].y; }
            }
            if (enemyIndex < 0 && boss.active)
            {
                bossTarget = true;
                if (boss.phase == 1)
                {
                    for (int weapon = 0; weapon < 3; ++weapon)
                    {
                        if (!boss.weapons[weapon].active) continue;
                        float wx, wy;
                        bossWeaponPosition(weapon, wx, wy);
                        const float d = distanceSquared(bot.x, bot.y, wx, wy);
                        if (d < nearest) { nearest = d; bossSubTarget = weapon; targetX = wx; targetY = wy; }
                    }
                }
                else if (boss.phase == 2 && boss.shape == BossSquare)
                {
                    for (int part = 0; part < 4; ++part)
                    {
                        if (!boss.parts[part].active) continue;
                        const float d = distanceSquared(bot.x, bot.y, boss.parts[part].x, boss.parts[part].y);
                        if (d < nearest) { nearest = d; bossSubTarget = part; targetX = boss.parts[part].x; targetY = boss.parts[part].y; }
                    }
                }
                else { targetX = boss.x; targetY = boss.y; nearest = distanceSquared(bot.x, bot.y, targetX, targetY); }
            }
            const float targetDistance = std::sqrt(std::max(0.0f, distanceSquared(bot.x, bot.y, targetX, targetY)));
            if (targetDistance > 45.0f)
            {
                const float direction = angleTo(bot.x, bot.y, targetX, targetY);
                bot.x += std::cos(direction) * bot.speed * dt;
                bot.y += std::sin(direction) * bot.speed * dt;
                resolveWalls(bot.x, bot.y, bot.radius);
                bot.x = clampf(bot.x, mapMinX + bot.radius, mapMaxX - bot.radius);
                bot.y = clampf(bot.y, mapMinY + bot.radius, mapMaxY - bot.radius);
            }
            else if (bot.attackTimer <= 0.0f && (enemyIndex >= 0 || bossTarget))
            {
                bot.attackTimer = 0.45f;
                if (enemyIndex >= 0 && enemyIndex < static_cast<int>(enemies.size()))
                {
                    Enemy& target = enemies[enemyIndex];
                    target.hp -= 6.0f;
                    addParticles(target.x, target.y, PURPLE, 3, 120.0f, 0.35f);
                    if (target.hp <= 0.0f)
                    {
                        const Enemy defeated = target;
                        rewardDefeatedEnemy(defeated, owner);
                        enemies.erase(enemies.begin() + enemyIndex);
                    }
                }
                else if (boss.phase == 1 && bossSubTarget >= 0) damageBoss(6.0f, bossSubTarget);
                else if (boss.phase == 2 && boss.shape == BossSquare && bossSubTarget >= 0) damageBossPart(bossSubTarget, 6.0f);
                else damageBoss(6.0f, -1);
            }
        }
    }
}

void Game::Impl::updateCharacterAbilities(float dt)
{
    for (int p = 0; p < playerCount; ++p)
    {
        Player& player = players[p];
        player.poisonAreaTimer = std::max(0.0f, player.poisonAreaTimer - dt);
        player.shieldTimer = std::max(0.0f, player.shieldTimer - dt);
        player.droneTimer = std::max(0.0f, player.droneTimer - dt);
        player.droneBeamTimer = std::max(0.0f, player.droneBeamTimer - dt);
        if (player.poisonAreaTimer > 0.0f)
        {
            const float radiusSquared = 235.0f * 235.0f;
            for (unsigned e = 0; e < enemies.size(); ++e)
                if (distanceSquared(player.x, player.y, enemies[e].x, enemies[e].y) <= radiusSquared)
                {
                    enemies[e].poisonTimer = 6.0f;
                    enemies[e].poisonOwner = p;
                }
        }
        if (player.droneTimer > 0.0f)
        {
            player.droneAngle += dt * 3.4f;
            player.droneFireTimer -= dt;
            if (player.droneFireTimer <= 0.0f)
            {
                player.droneFireTimer = 1.0f;
                fireCompanionDrone(player, p);
            }
        }
    }
}

void Game::Impl::updateSouls(float dt)
{
    for (int i = static_cast<int>(souls.size()) - 1; i >= 0; --i)
    {
        Soul& soul = souls[i];
        if (soul.owner < 0 || soul.owner >= playerCount || players[soul.owner].hp <= 0.0f) { souls.erase(souls.begin() + i); continue; }
        Player& owner = players[soul.owner];
        const float angle = angleTo(soul.x, soul.y, owner.x, owner.y);
        soul.x += std::cos(angle) * 520.0f * dt;
        soul.y += std::sin(angle) * 520.0f * dt;
        soul.life -= dt;
        if (distanceSquared(soul.x, soul.y, owner.x, owner.y) < 24.0f * 24.0f)
        {
            owner.souls = std::min(50, owner.souls + 1);
            souls.erase(souls.begin() + i);
        }
        else if (soul.life <= 0.0f) souls.erase(souls.begin() + i);
    }
    for (int i = static_cast<int>(lifeStreams.size()) - 1; i >= 0; --i)
    {
        LifeStream& stream = lifeStreams[i];
        if (stream.owner < 0 || stream.owner >= playerCount || players[stream.owner].hp <= 0.0f) { lifeStreams.erase(lifeStreams.begin() + i); continue; }
        Player& owner = players[stream.owner];
        const float angle = angleTo(stream.x, stream.y, owner.x, owner.y);
        stream.x += std::cos(angle) * 660.0f * dt;
        stream.y += std::sin(angle) * 660.0f * dt;
        stream.life -= dt;
        if (distanceSquared(stream.x, stream.y, owner.x, owner.y) < 28.0f * 28.0f)
        {
            owner.hp = std::min(owner.maxHp, owner.hp + stream.heal);
            addFloatingText(owner.x, owner.y - 30.0f, "+VIDA VAMPIRE", PINK);
            lifeStreams.erase(lifeStreams.begin() + i);
        }
        else if (stream.life <= 0.0f) lifeStreams.erase(lifeStreams.begin() + i);
    }
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

void Game::Impl::rewardDefeatedEnemy(const Enemy& enemy, int owner)
{
    if (enemy.kind == 1)
    {
        addDrop(enemy.x, enemy.y, DropType::Heart);
        addFloatingText(enemy.x, enemy.y - 20.0f, "CORACAO!", PINK);
    }
    else
    {
        if (random01() < 0.40f) addDrop(enemy.x, enemy.y, DropType::Gold);
        if (random01() < 0.18f) addDrop(enemy.x, enemy.y, DropType::Silver);
    }
    if (owner >= 0 && owner < playerCount)
    {
        players[owner].score += 10;
        players[owner].kills++;
        if (enemy.kind == 0 && players[owner].character == CharacterVampire)
        {
            Soul soul = {enemy.x, enemy.y, owner, 5.0f};
            souls.push_back(soul);
        }
    }
    ++totalKills;
    if (enemy.kind != 1 && totalKills > 0 && totalKills % 50 == 0)
    {
        addDrop(enemy.x, enemy.y, DropType::Heart);
        addFloatingText(enemy.x, enemy.y - 20.0f, "CORACAO!", PINK);
    }
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
    enemy.kind = 0;
    enemy.targetPlayer = -1;
    enemy.poisonTimer = 0.0f;
    enemy.poisonVisualTimer = 0.0f;
    enemy.poisonOwner = -1;
    enemy.burnTimer = 0.0f;
    enemy.burnVisualTimer = 0.0f;
    enemy.burnOwner = -1;
    enemies.push_back(enemy);
}

void Game::Impl::nextWave()
{
    intermissionActive = false;
    portalCharge = 0.0f;
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
    if (isBossWaveNumber(wave)) spawnBoss();
}

void Game::Impl::spawnBoss()
{
    std::memset(&boss, 0, sizeof(boss));
    boss.active = true;
    boss.phase = 1;
    boss.shape = randomInt(0, BossCount - 1);
    if (BossCount > 1 && boss.shape == lastBossShape)
        boss.shape = (boss.shape + randomInt(1, BossCount - 1)) % BossCount;
    lastBossShape = boss.shape;
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
    boss.rotationDirection = 1.0f;
    boss.telegraphDirection = -1;
    boss.memoryFlashDirection = -1;
    boss.memoryPlayer = -1;
    const Color shapeColors[BossCount] = {PLAYER_COLORS[0], PURPLE, GREEN, RED, CYAN, GOLD};
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
    enemyProjectiles.clear();
    const char* bossNames[BossCount] = {"NEXO X", "QUADRADO", "TRIANGULO", "CIRCULO", "DIRECIONAL", "TOUCHPAD"};
    showNotice(std::string("CHEFE ALEATORIO: ") + bossNames[boss.shape]);
}

void Game::Impl::bossWeaponPosition(int index, float& x, float& y) const
{
    const BossWeapon& weapon = boss.weapons[index];
    const float c = std::cos(boss.angle);
    const float s = std::sin(boss.angle);
    x = boss.x + c * weapon.relX - s * weapon.relY;
    y = boss.y + s * weapon.relX + c * weapon.relY;
}

void Game::Impl::addEnemyProjectile(float x, float y, float angle, float speed, float radius, Color colorValue,
                                    int kind, int bounces, float damage)
{
    Projectile projectile;
    projectile.x = x;
    projectile.y = y;
    projectile.vx = std::cos(angle) * speed;
    projectile.vy = std::sin(angle) * speed;
    projectile.radius = radius;
    projectile.damage = damage;
    projectile.owner = -1;
    projectile.color = colorValue;
    projectile.trailCount = 0;
    projectile.kind = kind;
    projectile.bounces = bounces;
    projectile.angle = angle;
    projectile.rotationSpeed = kind == ProjectileSquare ? 4.2f : (kind == ProjectileX ? -5.0f : 0.0f);
    enemyProjectiles.push_back(projectile);
}

void Game::Impl::spawnBossDrone()
{
    if (boss.droneCount >= 10) return;
    int activeDrones = 0;
    for (unsigned i = 0; i < enemies.size(); ++i) if (enemies[i].kind == 1) ++activeDrones;
    if (activeDrones >= 10) return;
    Enemy drone;
    const float angle = random01() * PI * 2.0f;
    drone.x = boss.x + std::cos(angle) * (boss.radius + 35.0f);
    drone.y = boss.y + std::sin(angle) * (boss.radius + 35.0f);
    drone.hp = 28.0f + playerCount * 5.0f;
    drone.maxHp = drone.hp;
    drone.speed = 360.0f + random01() * 90.0f;
    drone.radius = 13.0f;
    drone.angle = random01() * PI * 2.0f;
    drone.rotationSpeed = (random01() < 0.5f ? -1.0f : 1.0f) * (4.0f + random01() * 3.0f);
    drone.edges = 0;
    drone.color = PLAYER_COLORS[0];
    drone.kind = 1;
    drone.targetPlayer = randomInt(0, std::max(0, playerCount - 1));
    drone.poisonTimer = 0.0f;
    drone.poisonVisualTimer = 0.0f;
    drone.poisonOwner = -1;
    drone.burnTimer = 0.0f;
    drone.burnVisualTimer = 0.0f;
    drone.burnOwner = -1;
    enemies.push_back(drone);
    ++boss.droneCount;
}

void Game::Impl::spawnArrowWall(int direction)
{
    const float speed = 610.0f;
    const int lanes = 16;
    const int gap = randomInt(2, lanes - 4);
    for (int lane = 0; lane < lanes; ++lane)
    {
        if (lane == gap || lane == gap + 1 || (random01() < 0.05f && lane > 0 && lane < lanes - 1)) continue;
        for (int volley = 0; volley < 3; ++volley)
        {
            const float spacing = volley * 76.0f;
            if (direction == 0 || direction == 2)
            {
                const float x = mapMinX + (lane + 0.5f) * (mapMaxX - mapMinX) / lanes;
                const bool downwards = direction == 2;
                addEnemyProjectile(x, downwards ? mapMinY + 15.0f - spacing : mapMaxY - 15.0f + spacing,
                                   downwards ? PI * 0.5f : -PI * 0.5f, speed, 13.0f, RED, ProjectileArrow, 0, 18.0f);
            }
            else
            {
                const float y = mapMinY + (lane + 0.5f) * (mapMaxY - mapMinY) / lanes;
                const bool rightwards = direction == 1;
                addEnemyProjectile(rightwards ? mapMinX + 15.0f - spacing : mapMaxX - 15.0f + spacing, y,
                                   rightwards ? 0.0f : PI, speed, 13.0f, RED, ProjectileArrow, 0, 18.0f);
            }
        }
    }
    cameraShake = 13.0f;
}

void Game::Impl::startMemoryRound()
{
    int available[4];
    int count = 0;
    for (int direction = 0; direction < 4; ++direction)
        if ((boss.memoryRemovedMask & (1u << direction)) == 0) available[count++] = direction;
    if (count == 0) available[count++] = randomInt(0, 3);
    for (int i = 0; i < 8; ++i) boss.memorySequence[i] = available[randomInt(0, count - 1)];
    boss.memoryShowIndex = 0;
    boss.memoryInputIndex = 0;
    boss.memoryState = 1;
    boss.stateTimer = 0.68f;
    boss.memoryTimer = 60.0f;
    boss.memoryFlashDirection = boss.memorySequence[0];
    boss.memoryFlashTimer = 0.45f;
    enemyProjectiles.clear();
}

void Game::Impl::failMemoryRound()
{
    if (boss.memoryPlayer < 0 || boss.memoryPlayer >= playerCount) return;
    Player& player = players[boss.memoryPlayer];
    --boss.memoryLives;
    boss.memoryFlashDirection = boss.memoryInputIndex < 8 ? boss.memorySequence[boss.memoryInputIndex] : -1;
    boss.memoryFlashTimer = 0.7f;
    player.hp = std::max(0.0f, player.hp - player.maxHp / 3.0f);
    player.invincible = 0.8f;
    audio.playController(SoundEffect::PlayerDamage, player.pad);
    cameraShake = 34.0f;
    rumble(player.pad, 1.0f, 900);
    addParticles(player.x, player.y, RED, 45, 480.0f, 1.0f);
    addFloatingText(player.x, player.y - 40.0f, "ERRO! CHANCES " + number(std::max(0, boss.memoryLives)) + "/3", RED);
    if (boss.memoryLives <= 0)
    {
        const float left = player.x - mapMinX;
        const float right = mapMaxX - player.x;
        const float top = player.y - mapMinY;
        const float bottom = mapMaxY - player.y;
        const float nearest = std::min(std::min(left, right), std::min(top, bottom));
        if (nearest == left) player.x = mapMinX + player.radius;
        else if (nearest == right) player.x = mapMaxX - player.radius;
        else if (nearest == top) player.y = mapMinY + player.radius;
        else player.y = mapMaxY - player.radius;
        player.hp = 0.0f;
        player.pendingRevive = true;
        addParticles(player.x, player.y, RED, 90, 720.0f, 1.5f);
        addFloatingText(player.x, player.y - 55.0f, "MEMORIA CORROMPIDA", RED);
        int replacement = -1;
        for (int i = 0; i < playerCount; ++i) if (players[i].hp > 0.0f) { replacement = i; break; }
        if (replacement < 0) { checkGameOver(); return; }
        boss.memoryPlayer = replacement;
        boss.memoryLives = 3;
        boss.memoryRound = 1;
        boss.memoryRemovedMask = 0;
    }
    boss.memoryState = 0;
    boss.stateTimer = 1.1f;
}

void Game::Impl::setBossPhase(int phase)
{
    boss.phase = phase;
    boss.attackTimer = 0.0f;
    boss.state = 0;
    boss.stateTimer = 0.0f;
    boss.telegraphDirection = -1;
    boss.laserActive = false;
    boss.freezeActive = false;
    boss.shootSuppressed = false;
    boss.velocityX = 0.0f;
    boss.velocityY = 0.0f;
    boss.rotationDirection = 1.0f;
    cameraShake = 30.0f;
    if (phase == 2)
    {
        boss.color = boss.trueColor;
        addFloatingText(boss.x, boss.y - 70.0f, "SISTEMA COMPROMETIDO!", boss.color);
        if (boss.shape == BossX)
        {
            const float direction = random01() * PI * 2.0f;
            boss.velocityX = std::cos(direction) * 230.0f;
            boss.velocityY = std::sin(direction) * 230.0f;
            boss.teleportTimer = 2.4f;
        }
        else if (boss.shape == BossSquare)
        {
            const float cx = (mapMinX + mapMaxX) * 0.5f;
            const float cy = (mapMinY + mapMaxY) * 0.5f;
            const float positions[4][2] = {
                {cx, mapMinY + 55.0f}, {mapMaxX - 55.0f, cy},
                {cx, mapMaxY - 55.0f}, {mapMinX + 55.0f, cy}
            };
            for (int i = 0; i < 4; ++i)
            {
                boss.parts[i].x = positions[i][0];
                boss.parts[i].y = positions[i][1];
                boss.parts[i].hp = boss.maxHp * 0.25f;
                boss.parts[i].maxHp = boss.parts[i].hp;
                boss.parts[i].active = true;
                boss.parts[i].collectible = false;
                boss.parts[i].collected = false;
                boss.parts[i].collector = -1;
            }
        }
        else if (boss.shape == BossCircle || boss.shape == BossDpad || boss.shape == BossTouchpad)
        {
            boss.x = (mapMinX + mapMaxX) * 0.5f;
            boss.y = (mapMinY + mapMaxY) * 0.5f;
        }
        if (boss.shape == BossCircle) boss.attackTimer = -0.4f;
        if (boss.shape == BossDpad) boss.attackTimer = 0.0f;
        if (boss.shape == BossTouchpad) boss.abilityTimer = 25.0f;
        if (boss.shape == BossDpad || boss.shape == BossTouchpad) boss.color = {2, 3, 7, 255};
    }
    else
    {
        boss.color = RED;
        addFloatingText(boss.x, boss.y - 70.0f, "PROTOCOLO DE FURIA!", RED);
        enemyProjectiles.clear();
        if (boss.shape == BossX)
        {
            boss.x = (mapMinX + mapMaxX) * 0.5f;
            boss.y = (mapMinY + mapMaxY) * 0.5f;
            boss.sweepRemaining = 340.0f * PI / 180.0f;
            boss.rotationDirection = 1.0f;
            boss.laserActive = true;
            boss.droneCount = 0;
            boss.droneTimer = 0.0f;
        }
        else if (boss.shape == BossSquare)
        {
            boss.x = (mapMinX + mapMaxX) * 0.5f;
            boss.y = (mapMinY + mapMaxY) * 0.5f;
            boss.teleportTimer = 1.0f;
        }
        else if (boss.shape == BossTriangle) boss.teleportTimer = 4.0f;
        else if (boss.shape == BossCircle)
        {
            boss.state = 0;
            boss.stateTimer = 1.0f;
            boss.bounceCount = 0;
        }
        else if (boss.shape == BossDpad)
        {
            boss.x = (mapMinX + mapMaxX) * 0.5f;
            boss.y = (mapMinY + mapMaxY) * 0.5f;
            boss.memoryRound = 1;
            boss.memoryLives = 3;
            boss.memoryState = 0;
            boss.memoryRemovedMask = 0;
            boss.memoryPlayer = randomInt(0, std::max(0, playerCount - 1));
            for (int i = 0; i < playerCount; ++i)
                if (players[boss.memoryPlayer].hp <= 0.0f && players[i].hp > 0.0f) boss.memoryPlayer = i;
            boss.stateTimer = 1.5f;
            boss.memoryFlashDirection = -1;
            enemies.clear();
        }
        else if (boss.shape == BossTouchpad)
        {
            boss.abilityTimer = playerCount > 0 ? players[0].skillCooldownMax : 12.0f;
            boss.stateTimer = 0.0f;
        }
        if (boss.shape == BossDpad || boss.shape == BossTouchpad) boss.color = {2, 3, 7, 255};
    }
    addParticles(boss.x, boss.y, boss.color, 80, 430.0f, 1.3f);
    for (int i = 0; i < playerCount; ++i) rumble(players[i].pad, 0.8f, 600);
}

void Game::Impl::damageBossPart(int index, float damage)
{
    if (index < 0 || index >= 4 || !boss.parts[index].active) return;
    BossPart& part = boss.parts[index];
    part.hp -= damage;
    if (part.hp > 0.0f) return;
    part.hp = 0.0f;
    part.active = false;
    part.collectible = true;
    cameraShake = 18.0f;
    addParticles(part.x, part.y, PURPLE, 34, 420.0f, 1.0f);
    addFloatingText(part.x, part.y - 30.0f, "RECOLHA O TRACO", GOLD);
}

void Game::Impl::updateBoss(float dt)
{
    if (!boss.active) return;
    if (boss.y < mapMinY + 160.0f) boss.y += 110.0f * dt;
    if (anyTimeStop()) return;

    if (boss.phase == 1) boss.angle += 1.6f * dt;
    boss.attackTimer += dt;
    boss.memoryFlashTimer = std::max(0.0f, boss.memoryFlashTimer - dt);
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
            setBossPhase(2);
    }
    else if (boss.phase == 2)
    {
        if (boss.shape == BossX)
        {
            boss.angle += boss.rotationDirection * 2.6f * dt;
            boss.teleportTimer -= dt;
            if (boss.teleportTimer <= 0.0f)
            {
                const float direction = random01() * PI * 2.0f;
                boss.velocityX = std::cos(direction) * 230.0f;
                boss.velocityY = std::sin(direction) * 230.0f;
                boss.teleportTimer = 1.8f + random01() * 1.8f;
            }
            boss.x += boss.velocityX * dt;
            boss.y += boss.velocityY * dt;
            bool bounced = false;
            if (boss.x <= mapMinX + boss.radius || boss.x >= mapMaxX - boss.radius) { boss.velocityX = -boss.velocityX; bounced = true; }
            if (boss.y <= mapMinY + boss.radius || boss.y >= mapMaxY - boss.radius) { boss.velocityY = -boss.velocityY; bounced = true; }
            boss.x = clampf(boss.x, mapMinX + boss.radius, mapMaxX - boss.radius);
            boss.y = clampf(boss.y, mapMinY + boss.radius, mapMaxY - boss.radius);
            if (bounced) { boss.rotationDirection = -boss.rotationDirection; cameraShake = 9.0f; }
            if (boss.attackTimer >= 0.45f)
            {
                boss.attackTimer = 0.0f;
                for (int tip = 0; tip < 4; ++tip)
                {
                    const float angle = boss.angle + tip * PI * 0.5f;
                    addEnemyProjectile(boss.x + std::cos(angle) * boss.radius, boss.y + std::sin(angle) * boss.radius,
                                       angle, 430.0f, 9.0f, boss.trueColor, ProjectileX);
                }
            }
        }
        else if (boss.shape == BossSquare)
        {
            int collected = 0;
            for (int i = 0; i < 4; ++i)
            {
                BossPart& part = boss.parts[i];
                if (part.active && boss.attackTimer >= 0.95f)
                {
                    Player* partTarget = target;
                    float partNearest = distanceSquared(part.x, part.y, target->x, target->y);
                    for (int p = 0; p < playerCount; ++p)
                    {
                        if (players[p].hp <= 0.0f) continue;
                        const float d = distanceSquared(part.x, part.y, players[p].x, players[p].y);
                        if (d < partNearest) { partNearest = d; partTarget = &players[p]; }
                    }
                    addEnemyProjectile(part.x, part.y, angleTo(part.x, part.y, partTarget->x, partTarget->y),
                                       400.0f, 9.0f, PURPLE, ProjectileSquare);
                }
                if (part.collectible)
                {
                    for (int p = 0; p < playerCount; ++p)
                    {
                        if (players[p].hp <= 0.0f || distanceSquared(part.x, part.y, players[p].x, players[p].y) > 58.0f * 58.0f) continue;
                        part.collectible = false;
                        part.collected = true;
                        part.collector = p;
                        addFloatingText(players[p].x, players[p].y - 35.0f, "TRACO RECUPERADO", GOLD);
                        rumble(players[p].pad, 0.55f, 280);
                        break;
                    }
                }
                if (part.collected)
                {
                    ++collected;
                    const Player& carrier = players[std::max(0, std::min(playerCount - 1, part.collector))];
                    const float orbit = lastTick / 500.0f + i * PI * 0.5f;
                    part.x = carrier.x + std::cos(orbit) * 45.0f;
                    part.y = carrier.y + std::sin(orbit) * 45.0f;
                }
            }
            if (boss.attackTimer >= 0.95f) boss.attackTimer = 0.0f;
            if (collected == 4) setBossPhase(3);
        }
        else if (boss.shape == BossTriangle && boss.attackTimer >= 0.9f)
        {
            boss.attackTimer = 0.0f;
            const float base = angleTo(boss.x, boss.y, target->x, target->y);
            for (int i = -1; i <= 1; ++i) addEnemyProjectile(boss.x, boss.y, base + i * 0.35f, 340.0f, 8.0f, boss.trueColor);
        }
        else if (boss.shape == BossCircle && boss.attackTimer >= 3.0f)
        {
            boss.attackTimer = 0.0f;
            const float base = random01() * PI * 0.5f;
            for (int i = 0; i < 4; ++i)
                addEnemyProjectile(boss.x, boss.y, base + i * PI * 0.5f, 330.0f, boss.radius * 0.5f,
                                   boss.trueColor, ProjectileDebris, 4, 24.0f);
        }
        else if (boss.shape == BossDpad)
        {
            boss.x = (mapMinX + mapMaxX) * 0.5f;
            boss.y = (mapMinY + mapMaxY) * 0.5f;
            if (boss.state == 0 && boss.attackTimer >= 2.7f)
            {
                boss.state = 1;
                boss.stateTimer = 1.0f;
                boss.telegraphDirection = randomInt(0, 3);
                boss.attackTimer = 0.0f;
            }
            else if (boss.state == 1)
            {
                boss.stateTimer -= dt;
                if (boss.stateTimer <= 0.0f)
                {
                    spawnArrowWall(boss.telegraphDirection);
                    boss.telegraphDirection = -1;
                    boss.state = 0;
                }
            }
        }
        else if (boss.shape == BossTouchpad)
        {
            boss.abilityTimer -= dt;
            if (!boss.shootSuppressed && boss.abilityTimer <= 0.0f)
            {
                boss.shootSuppressed = true;
                boss.stateTimer = 15.0f;
                boss.attackTimer = 0.0f;
                projectiles.clear();
                showNotice("TIROS DESINTEGRADOS POR 15S");
            }
            if (boss.shootSuppressed)
            {
                boss.stateTimer -= dt;
                projectiles.clear();
                if (boss.attackTimer >= 0.48f)
                {
                    boss.attackTimer = 0.0f;
                    const int touchKinds[4] = {ProjectileTriangle, ProjectileX, ProjectileSquare, ProjectileOrb};
                    const int kind = touchKinds[randomInt(0, 3)];
                    const float base = angleTo(boss.x, boss.y, target->x, target->y);
                    for (int i = -1; i <= 1; ++i)
                        addEnemyProjectile(boss.x, boss.y, base + i * 0.22f, 410.0f, 10.0f,
                                           i == 0 ? RED : boss.trueColor, kind, 0, 14.0f);
                }
                if (boss.stateTimer <= 0.0f)
                {
                    boss.shootSuppressed = false;
                    boss.abilityTimer = 25.0f;
                    showNotice("TIROS RESTAURADOS");
                }
            }
        }
    }
    else
    {
        const float targetAngle = angleTo(boss.x, boss.y, target->x, target->y);
        if (boss.shape == BossX)
        {
            const float rotationSpeed = 0.92f;
            boss.angle += boss.rotationDirection * rotationSpeed * dt;
            boss.sweepRemaining -= rotationSpeed * dt;
            boss.laserActive = true;
            if (boss.rotationDirection < 0.0f && boss.droneCount < 10)
            {
                boss.droneTimer -= dt;
                while (boss.droneTimer <= 0.0f && boss.droneCount < 10)
                {
                    spawnBossDrone();
                    boss.droneTimer += 0.30f;
                }
            }
            if (boss.sweepRemaining <= 0.0f)
            {
                boss.rotationDirection = -boss.rotationDirection;
                boss.sweepRemaining = 340.0f * PI / 180.0f;
                if (boss.rotationDirection < 0.0f) boss.droneTimer = 0.0f;
            }
            for (int tip = 0; tip < 4; ++tip)
            {
                const float laserAngle = boss.angle + PI * 0.25f + tip * PI * 0.5f;
                const float startX = boss.x + std::cos(laserAngle) * boss.radius;
                const float startY = boss.y + std::sin(laserAngle) * boss.radius;
                const float endX = startX + std::cos(laserAngle) * 2800.0f;
                const float endY = startY + std::sin(laserAngle) * 2800.0f;
                for (int p = 0; p < playerCount; ++p)
                    if (players[p].hp > 0.0f && pointSegmentDistanceSquared(players[p].x, players[p].y, startX, startY, endX, endY) < 25.0f * 25.0f)
                        damagePlayer(players[p], 14.0f);
            }
        }
        else if (boss.shape == BossSquare)
        {
            boss.angle += 2.8f * dt;
            boss.teleportTimer -= dt;
            if (boss.teleportTimer <= 0.0f)
            {
                const float margin = 95.0f;
                const int corner = boss.state++ % 4;
                const float oldX = boss.x;
                const float oldY = boss.y;
                boss.x = (corner == 0 || corner == 3) ? mapMinX + margin : mapMaxX - margin;
                boss.y = (corner < 2) ? mapMinY + margin : mapMaxY - margin;
                addParticles(oldX, oldY, PURPLE, 30, 420.0f, 0.8f);
                addParticles(boss.x, boss.y, PURPLE, 40, 480.0f, 1.0f);
                for (int i = 0; i < 14; ++i)
                    addEnemyProjectile(boss.x, boss.y, i * PI * 2.0f / 14.0f, 300.0f + (i % 2) * 90.0f,
                                       11.0f, PURPLE, ProjectileSquare, 0, 16.0f);
                boss.teleportTimer = 5.0f;
                boss.attackTimer = 0.0f;
            }
        }
        else if (boss.shape == BossTriangle)
        {
            boss.x += std::cos(targetAngle) * 100.0f * dt;
            boss.y += std::sin(targetAngle) * 100.0f * dt;
            boss.teleportTimer -= dt;
            if (boss.teleportTimer <= 0.0f)
            {
                boss.teleportTimer = 4.0f;
                const float angle = random01() * PI * 2.0f;
                boss.x = clampf(target->x + std::cos(angle) * 120.0f, mapMinX + boss.radius, mapMaxX - boss.radius);
                boss.y = clampf(target->y + std::sin(angle) * 120.0f, mapMinY + boss.radius, mapMaxY - boss.radius);
                addParticles(boss.x, boss.y, boss.trueColor, 26, 360.0f, 0.8f);
            }
            if (boss.attackTimer >= 0.7f)
            {
                boss.attackTimer = 0.0f;
                for (int i = -1; i <= 1; ++i) addEnemyProjectile(boss.x, boss.y, targetAngle + i * 0.28f, 490.0f, 11.0f, RED);
            }
        }
        else if (boss.shape == BossCircle)
        {
            if (boss.state == 0)
            {
                boss.stateTimer -= dt;
                boss.velocityX = boss.velocityY = 0.0f;
                if (boss.stateTimer <= 0.0f)
                {
                    boss.velocityX = std::cos(targetAngle) * 1120.0f;
                    boss.velocityY = std::sin(targetAngle) * 1120.0f;
                    boss.bounceCount = 0;
                    boss.state = 1;
                    cameraShake = 25.0f;
                }
            }
            else if (boss.state == 1)
            {
                boss.x += boss.velocityX * dt;
                boss.y += boss.velocityY * dt;
                bool bounced = false;
                if (boss.x <= mapMinX + boss.radius || boss.x >= mapMaxX - boss.radius) { boss.velocityX = -boss.velocityX; bounced = true; }
                if (boss.y <= mapMinY + boss.radius || boss.y >= mapMaxY - boss.radius) { boss.velocityY = -boss.velocityY; bounced = true; }
                boss.x = clampf(boss.x, mapMinX + boss.radius, mapMaxX - boss.radius);
                boss.y = clampf(boss.y, mapMinY + boss.radius, mapMaxY - boss.radius);
                if (bounced)
                {
                    ++boss.bounceCount;
                    boss.velocityX *= 0.72f;
                    boss.velocityY *= 0.72f;
                    cameraShake = 28.0f;
                    for (int i = 0; i < 6; ++i)
                        addEnemyProjectile(boss.x, boss.y, random01() * PI * 2.0f, 260.0f + random01() * 180.0f,
                                           9.0f, RED, ProjectileDebris, 0, 13.0f);
                    if (boss.bounceCount >= 3)
                    {
                        boss.state = 2;
                        boss.stateTimer = 1.4f;
                    }
                }
                for (int p = 0; p < playerCount; ++p)
                {
                    const float radius = boss.radius + players[p].radius;
                    if (players[p].hp > 0.0f && distanceSquared(boss.x, boss.y, players[p].x, players[p].y) <= radius * radius)
                        damagePlayer(players[p], 65.0f);
                }
            }
            else
            {
                boss.stateTimer -= dt;
                if (boss.stateTimer <= 0.0f) { boss.state = 0; boss.stateTimer = 1.0f; }
            }
        }
        else if (boss.shape == BossDpad)
        {
            if (boss.memoryPlayer < 0 || boss.memoryPlayer >= playerCount) return;
            Player& memoryPlayer = players[boss.memoryPlayer];
            if (boss.memoryState == 0)
            {
                const float holdX = boss.x + 190.0f;
                const float holdY = boss.y;
                memoryPlayer.x += (holdX - memoryPlayer.x) * std::min(1.0f, dt * 3.2f);
                memoryPlayer.y += (holdY - memoryPlayer.y) * std::min(1.0f, dt * 3.2f);
                boss.stateTimer -= dt;
                if (boss.stateTimer <= 0.0f) startMemoryRound();
            }
            else if (boss.memoryState == 1)
            {
                boss.stateTimer -= dt;
                if (boss.stateTimer <= 0.0f)
                {
                    ++boss.memoryShowIndex;
                    if (boss.memoryShowIndex >= 8)
                    {
                        boss.memoryState = 2;
                        boss.memoryFlashDirection = -1;
                        boss.memoryTimer = 60.0f;
                    }
                    else
                    {
                        boss.memoryFlashDirection = boss.memorySequence[boss.memoryShowIndex];
                        boss.memoryFlashTimer = 0.44f;
                        boss.stateTimer = 0.68f;
                    }
                }
            }
            else if (boss.memoryState == 2)
            {
                boss.memoryTimer -= dt;
                int input = -1;
                if (pressed(memoryPlayer.pad, PAD_UP)) input = 0;
                else if (pressed(memoryPlayer.pad, PAD_RIGHT)) input = 1;
                else if (pressed(memoryPlayer.pad, PAD_DOWN)) input = 2;
                else if (pressed(memoryPlayer.pad, PAD_LEFT)) input = 3;
                if (input >= 0)
                {
                    if (input != boss.memorySequence[boss.memoryInputIndex]) { failMemoryRound(); return; }
                    boss.memoryFlashDirection = input;
                    boss.memoryFlashTimer = 0.28f;
                    ++boss.memoryInputIndex;
                    if (boss.memoryInputIndex >= 8)
                    {
                        boss.memoryState = 3;
                        boss.memoryFlashDirection = -2;
                        boss.memoryFlashTimer = 999.0f;
                        addFloatingText(memoryPlayer.x, memoryPlayer.y - 40.0f, "SEQUENCIA CORRETA!", GOLD);
                    }
                }
                if (boss.memoryTimer <= 0.0f) { failMemoryRound(); return; }
            }
            else if (boss.memoryState == 3)
            {
                int input = -1;
                if (pressed(memoryPlayer.pad, PAD_UP)) input = 0;
                else if (pressed(memoryPlayer.pad, PAD_RIGHT)) input = 1;
                else if (pressed(memoryPlayer.pad, PAD_DOWN)) input = 2;
                else if (pressed(memoryPlayer.pad, PAD_LEFT)) input = 3;
                if (input >= 0 && (boss.memoryRemovedMask & (1u << input)) == 0)
                {
                    if (boss.memoryRound >= 4)
                    {
                        boss.memoryState = 4;
                        boss.memoryFlashDirection = -1;
                        boss.rageHp = boss.maxRageHp * 0.5f;
                        showNotice("MEMORIA QUEBRADA - ATAQUE!");
                    }
                    else
                    {
                        boss.memoryRemovedMask |= 1u << input;
                        ++boss.memoryRound;
                        boss.memoryState = 0;
                        boss.stateTimer = 0.8f;
                        boss.memoryFlashDirection = -1;
                        addParticles(boss.x, boss.y, GOLD, 28, 350.0f, 0.8f);
                    }
                }
            }
            else if (boss.attackTimer >= 0.55f)
            {
                boss.attackTimer = 0.0f;
                const float base = angleTo(boss.x, boss.y, target->x, target->y);
                for (int i = -2; i <= 2; ++i)
                    addEnemyProjectile(boss.x, boss.y, base + i * 0.18f, 520.0f, 12.0f, RED, ProjectileArrow, 0, 17.0f);
            }
        }
        else if (boss.shape == BossTouchpad)
        {
            if (boss.freezeActive)
            {
                boss.stateTimer -= dt;
                if (boss.stateTimer <= 0.0f)
                {
                    boss.stateTimer = 0.32f;
                    const float step = 31.0f;
                    boss.x += std::cos(targetAngle) * step;
                    boss.y += std::sin(targetAngle) * step;
                    const float d = std::sqrt(std::max(1.0f, nearest));
                    rumble(target->pad, clampf(1.0f - d / 900.0f, 0.18f, 1.0f), 430);
                    cameraShake = std::max(cameraShake, clampf(18.0f - d / 70.0f, 2.0f, 18.0f));
                    if (d < boss.radius + target->radius + 20.0f)
                    {
                        damagePlayer(*target, 50.0f);
                        boss.freezeActive = false;
                        boss.abilityTimer = target->skillCooldownMax;
                        showNotice("COLAPSO TEMPORAL");
                    }
                }
            }
            else
            {
                boss.abilityTimer -= dt;
                if (boss.attackTimer >= 1.25f)
                {
                    boss.attackTimer = 0.0f;
                    const int touchKinds[4] = {ProjectileTriangle, ProjectileX, ProjectileSquare, ProjectileOrb};
                    addEnemyProjectile(boss.x, boss.y, targetAngle, 430.0f, 11.0f, GOLD,
                                       touchKinds[randomInt(0, 3)], 0, 15.0f);
                }
                if (boss.abilityTimer <= 0.0f)
                {
                    boss.freezeActive = true;
                    boss.stateTimer = 0.0f;
                    enemyProjectiles.clear();
                    showNotice("TEMPO BLOQUEADO - USE SUA HABILIDADE!");
                }
            }
        }
        boss.x = clampf(boss.x, mapMinX + boss.radius, mapMaxX - boss.radius);
        boss.y = clampf(boss.y, mapMinY + boss.radius, mapMaxY - boss.radius);
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
            setBossPhase(3);
        }
    }
    else if (boss.phase == 3)
    {
        if (boss.shape == BossDpad && boss.memoryState != 4) return;
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
        players[i].coins += 15;
        if (players[i].hp <= 0.0f)
        {
            players[i].hp = players[i].maxHp * 0.5f;
            players[i].pendingRevive = false;
            players[i].invincible = 2.0f;
        }
        addFloatingText(players[i].x, players[i].y - 50.0f, "BOSS! +50 S +15 G", GOLD);
        rumble(players[i].pad, 1.0f, 1200);
    }
    profileSilver += 50;
    profileDirty = true;
    saveProfile(false);
    boss.active = false;
    // A morte pode acontecer dentro de handleProjectileCollisions(). Limpar os
    // vetores aqui invalidaria a referencia do projetil que causou o golpe final.
    // A limpeza e feita no fim do processamento de colisoes do mesmo quadro.
    combatCleanupPending = true;
    bossDelay = 0.0f;
    intermissionActive = true;
    portalCharge = 0.0f;
    const int shapeEdges[BossCount] = {8, 4, 3, 0, 8, 4};
    enemyEdges = shapeEdges[shape];
    enemyColor = theme;
    audio.playTv(SoundEffect::BossDestroyed);
    mapMinX -= 300.0f;
    mapMinY -= 300.0f;
    mapMaxX += 300.0f;
    mapMaxY += 300.0f;
    const float cx = (mapMinX + mapMaxX) * 0.5f;
    const float cy = (mapMinY + mapMaxY) * 0.5f;
    walls.push_back({mapMinX + 100.0f, cy - 40.0f, 80.0f, 80.0f, 40.0f});
    walls.push_back({mapMaxX - 180.0f, cy - 40.0f, 80.0f, 80.0f, 40.0f});
    walls.push_back({cx - 340.0f, mapMinY + 180.0f, 80.0f, 80.0f, 40.0f});
    walls.push_back({cx + 260.0f, mapMaxY - 260.0f, 80.0f, 80.0f, 40.0f});
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
        else if (boss.shape == BossSquare && boss.phase == 2)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (!boss.parts[i].active) continue;
                const float d = distanceSquared(player.x, player.y, boss.parts[i].x, boss.parts[i].y);
                if (d < nearest)
                {
                    nearest = d;
                    targetX = boss.parts[i].x;
                    targetY = boss.parts[i].y;
                    found = true;
                }
            }
        }
        else
        {
            const float d = distanceSquared(player.x, player.y, boss.x, boss.y);
            if (d < nearest)
            {
                nearest = d;
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
    projectile.kind = ProjectileOrb;
    projectile.bounces = 0;
    projectile.angle = angle;
    projectile.rotationSpeed = 0.0f;
    projectiles.push_back(projectile);
    player.fireTimer = player.fireRate;
}

void Game::Impl::throwGrenade(Player& player, int playerIndex)
{
    if (player.grenades <= 0)
    {
        addFloatingText(player.x, player.y - 34.0f, "SEM GRANADAS", GRENADE_GREEN);
        return;
    }

    float directionX = axis(player.pad, 2);
    float directionY = axis(player.pad, 3);
    const float aimMagnitude = std::sqrt(directionX * directionX + directionY * directionY);
    float angle = -PI * 0.5f;
    if (aimMagnitude > 0.24f)
    {
        angle = std::atan2(directionY, directionX);
    }
    else
    {
        float nearest = 1e30f;
        float targetX = player.x;
        float targetY = player.y - 100.0f;
        for (unsigned i = 0; i < enemies.size(); ++i)
        {
            const float d = distanceSquared(player.x, player.y, enemies[i].x, enemies[i].y);
            if (d < nearest)
            {
                nearest = d;
                targetX = enemies[i].x;
                targetY = enemies[i].y;
            }
        }
        if (boss.active)
        {
            const float d = distanceSquared(player.x, player.y, boss.x, boss.y);
            if (d < nearest)
            {
                targetX = boss.x;
                targetY = boss.y;
            }
        }
        angle = angleTo(player.x, player.y, targetX, targetY);
    }

    Projectile grenade;
    grenade.x = player.x + std::cos(angle) * (player.radius + 14.0f);
    grenade.y = player.y + std::sin(angle) * (player.radius + 14.0f);
    grenade.vx = std::cos(angle) * 590.0f;
    grenade.vy = std::sin(angle) * 590.0f;
    grenade.radius = 12.0f;
    grenade.damage = 160.0f;
    grenade.owner = playerIndex;
    grenade.color = GRENADE_GREEN;
    grenade.trailCount = 0;
    grenade.kind = ProjectileGrenade;
    grenade.bounces = 0;
    grenade.angle = angle;
    grenade.rotationSpeed = 7.0f;
    projectiles.push_back(grenade);
    --player.grenades;
    addFloatingText(player.x, player.y - 34.0f, "GRANADA!", GRENADE_GREEN);
    rumble(player.pad, 0.35f, 110);
}

void Game::Impl::explodeGrenade(float x, float y, int owner)
{
    audio.playTv(SoundEffect::Grenade);
    const float blastRadius = 285.0f;
    const float blastRadiusSquared = blastRadius * blastRadius;
    int destroyed = 0;
    for (int kill = 0; kill < 5; ++kill)
    {
        int nearestIndex = -1;
        float nearest = blastRadiusSquared;
        for (int e = 0; e < static_cast<int>(enemies.size()); ++e)
        {
            const float d = distanceSquared(x, y, enemies[e].x, enemies[e].y);
            if (d < nearest)
            {
                nearest = d;
                nearestIndex = e;
            }
        }
        if (nearestIndex < 0) break;

        const Enemy defeated = enemies[nearestIndex];
        rewardDefeatedEnemy(defeated, owner);
        addParticles(defeated.x, defeated.y, defeated.color, 18, 390.0f, 0.85f);
        enemies.erase(enemies.begin() + nearestIndex);
        ++destroyed;
    }

    addParticles(x, y, GRENADE_GREEN, 70, 610.0f, 1.05f);
    addParticles(x, y, WHITE, 28, 430.0f, 0.65f);
    const float baseAngle = random01() * PI * 0.5f;
    for (int shard = 0; shard < 4; ++shard)
    {
        const float angle = baseAngle + shard * PI * 0.5f;
        Projectile fire;
        fire.x = x;
        fire.y = y;
        fire.vx = std::cos(angle) * 560.0f;
        fire.vy = std::sin(angle) * 560.0f;
        fire.radius = 7.0f;
        fire.damage = 0.0f;
        fire.owner = owner;
        fire.color = ORANGE;
        fire.trailCount = 0;
        fire.kind = ProjectileFireShard;
        fire.bounces = 0;
        fire.angle = angle;
        fire.rotationSpeed = 8.0f;
        projectiles.push_back(fire);
    }
    addFloatingText(x, y - 45.0f, destroyed > 0 ? "BOOM! x" + number(destroyed) : "BOOM!", GRENADE_GREEN);
    cameraShake = std::max(cameraShake, 32.0f);
    if (owner >= 0 && owner < playerCount) rumble(players[owner].pad, 0.85f, 390);
}

void Game::Impl::createFireArea(float x, float y, int owner)
{
    FireArea area;
    area.x = clampf(x, mapMinX + 22.0f, mapMaxX - 22.0f);
    area.y = clampf(y, mapMinY + 22.0f, mapMaxY - 22.0f);
    area.radius = 24.0f;
    area.life = 4.0f;
    area.owner = owner;
    area.phase = random01() * PI * 2.0f;
    fireAreas.push_back(area);
    addParticles(area.x, area.y, ORANGE, 18, 170.0f, 0.65f);
}

void Game::Impl::updateFireAreas(float dt)
{
    for (int i = static_cast<int>(fireAreas.size()) - 1; i >= 0; --i)
    {
        FireArea& area = fireAreas[i];
        area.life -= dt;
        area.phase += dt * 5.0f;
        const float radiusSquared = area.radius * area.radius;
        for (unsigned e = 0; e < enemies.size(); ++e)
        {
            const float combined = area.radius + enemies[e].radius;
            if (distanceSquared(area.x, area.y, enemies[e].x, enemies[e].y) <= std::max(radiusSquared, combined * combined))
            {
                enemies[e].burnTimer = 3.0f;
                enemies[e].burnOwner = area.owner;
            }
        }
        if (area.life <= 0.0f) fireAreas.erase(fireAreas.begin() + i);
    }
}

void Game::Impl::buyUpgrade(Player& player, int type)
{
    if (type == 1 && player.coins >= player.fireCost && player.fireLevel < 10)
    {
        player.coins -= player.fireCost;
        ++player.fireLevel;
        player.fireRate = std::max(0.065f, 0.16f - (player.fireLevel - 1) * 0.012f);
        player.fireCost = 15 + (player.fireLevel - 1) * 10;
        addFloatingText(player.x, player.y - 28.0f, "TIRO LV." + number(player.fireLevel), GOLD);
        rumble(player.pad, 0.35f, 120);
    }
    else if (type == 2 && player.coins >= player.damageCost && player.damageLevel < 10)
    {
        player.coins -= player.damageCost;
        ++player.damageLevel;
        player.damageMultiplier = 1.0f + (player.damageLevel - 1) * 0.20f;
        player.damageCost = 30 + (player.damageLevel - 1) * 15;
        addFloatingText(player.x, player.y - 28.0f, "DANO LV." + number(player.damageLevel), GOLD);
        rumble(player.pad, 0.45f, 120);
    }
    else if (type == 3 && player.coins >= player.skillCost)
    {
        player.coins -= player.skillCost;
        ++player.skillLevel;
        configureCharacter(player);
        player.skillCost = static_cast<int>(player.skillCost * 1.8f);
        addFloatingText(player.x, player.y - 28.0f, "HABILIDADE LV." + number(player.skillLevel), GOLD);
        rumble(player.pad, 0.4f, 120);
    }
    else if (type == 4 && player.coins >= GRENADE_COST)
    {
        player.coins -= GRENADE_COST;
        ++player.grenades;
        addFloatingText(player.x, player.y - 28.0f, "+1 GRANADA", GRENADE_GREEN);
        rumble(player.pad, 0.35f, 120);
    }
}

bool Game::Impl::nearMechanic(const Player& player) const
{
    if (!intermissionActive) return false;
    const float mechanicX = (mapMinX + mapMaxX) * 0.5f;
    const float mechanicY = (mapMinY + mapMaxY) * 0.5f;
    return distanceSquared(player.x, player.y, mechanicX, mechanicY) <= 175.0f * 175.0f;
}

void Game::Impl::buyMechanicUpgrade(Player& player, int type)
{
    const int original = type == 1 ? player.fireCost : (type == 2 ? player.damageCost : (type == 3 ? player.skillCost : 8));
    const int price = type == 4 ? 8 : std::max(1, static_cast<int>(std::ceil(original * 0.70f)));
    if (type == 1 && player.fireLevel >= 10) { showNotice("TIRO NO MAXIMO"); return; }
    if (type == 2 && player.damageLevel >= 10) { showNotice("DANO NO MAXIMO"); return; }
    if (player.coins < price) { showNotice("G INSUFICIENTE"); return; }
    player.coins -= price;
    if (type == 1)
    {
        ++player.fireLevel;
        player.fireRate = std::max(0.065f, 0.16f - (player.fireLevel - 1) * 0.012f);
        player.fireCost = 15 + (player.fireLevel - 1) * 10;
        addFloatingText(player.x, player.y - 28.0f, "TIRO LV." + number(player.fireLevel) + " -30%", GOLD);
    }
    else if (type == 2)
    {
        ++player.damageLevel;
        player.damageMultiplier = 1.0f + (player.damageLevel - 1) * 0.20f;
        player.damageCost = 30 + (player.damageLevel - 1) * 15;
        addFloatingText(player.x, player.y - 28.0f, "DANO LV." + number(player.damageLevel) + " -30%", GOLD);
    }
    else if (type == 3)
    {
        ++player.skillLevel;
        configureCharacter(player);
        player.skillCost = static_cast<int>(player.skillCost * 1.8f);
        addFloatingText(player.x, player.y - 28.0f, "HABILIDADE LV." + number(player.skillLevel) + " -30%", GOLD);
    }
    else
    {
        ++player.grenades;
        addFloatingText(player.x, player.y - 28.0f, "+1 GRANADA 8G", GRENADE_GREEN);
    }
    rumble(player.pad, 0.4f, 130);
}

void Game::Impl::updateIntermission(float dt)
{
    if (!intermissionActive) return;
    const float portalX = (mapMinX + mapMaxX) * 0.5f;
    const float portalY = mapMinY + 105.0f;
    bool everyoneReady = playerCount > 0;
    for (int i = 0; i < playerCount; ++i)
    {
        const Player& player = players[i];
        const bool inside = player.hp > 0.0f && std::abs(player.x - portalX) <= 145.0f && std::abs(player.y - portalY) <= 62.0f;
        if (!inside) { everyoneReady = false; break; }
    }
    portalCharge = everyoneReady ? portalCharge + dt : 0.0f;
    if (portalCharge >= 1.25f)
    {
        intermissionActive = false;
        portalCharge = 0.0f;
        addParticles(portalX, portalY, CYAN, 70, 430.0f, 1.0f);
        nextWave();
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
    if (player.shieldTimer > 0.0f)
    {
        addParticles(player.x, player.y, WHITE, 7, 180.0f, 0.45f);
        rumble(player.pad, 0.22f, 70);
        return;
    }
    player.hp -= damage;
    player.invincible = 0.4f;
    cameraShake = 14.0f;
    addParticles(player.x, player.y, RED, 16, 300.0f, 0.8f);
    rumble(player.pad, 0.9f, 250);
    audio.playController(SoundEffect::PlayerDamage, player.pad);
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
        projectile.angle += projectile.rotationSpeed * dt;
        bool hit = false;

        if (projectile.kind == ProjectileGrenade)
        {
            bool impact = projectile.x <= mapMinX + projectile.radius || projectile.x >= mapMaxX - projectile.radius ||
                          projectile.y <= mapMinY + projectile.radius || projectile.y >= mapMaxY - projectile.radius;

            if (!impact && boss.active)
            {
                if (boss.phase == 1)
                {
                    for (int weapon = 0; weapon < 3; ++weapon)
                    {
                        if (!boss.weapons[weapon].active) continue;
                        float weaponX, weaponY;
                        bossWeaponPosition(weapon, weaponX, weaponY);
                        const float radius = projectile.radius + 22.0f;
                        if (distanceSquared(projectile.x, projectile.y, weaponX, weaponY) <= radius * radius)
                        {
                            damageBoss(projectile.damage * 0.5f, weapon);
                            impact = true;
                            break;
                        }
                    }
                }
                else if (boss.shape == BossSquare && boss.phase == 2)
                {
                    for (int part = 0; part < 4; ++part)
                    {
                        if (!boss.parts[part].active) continue;
                        const float radius = projectile.radius + 34.0f;
                        if (distanceSquared(projectile.x, projectile.y, boss.parts[part].x, boss.parts[part].y) <= radius * radius)
                        {
                            damageBossPart(part, projectile.damage * 0.5f);
                            impact = true;
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
                        impact = true;
                    }
                }
            }

            if (!impact)
            {
                for (unsigned e = 0; e < enemies.size(); ++e)
                {
                    const float radius = projectile.radius + enemies[e].radius;
                    if (distanceSquared(projectile.x, projectile.y, enemies[e].x, enemies[e].y) <= radius * radius)
                    {
                        impact = true;
                        break;
                    }
                }
            }

            if (!impact)
            {
                for (int wallIndex = static_cast<int>(walls.size()) - 1; wallIndex >= 0; --wallIndex)
                {
                    Wall& wall = walls[wallIndex];
                    const float closestX = clampf(projectile.x, wall.x, wall.x + wall.w);
                    const float closestY = clampf(projectile.y, wall.y, wall.y + wall.h);
                    if (distanceSquared(projectile.x, projectile.y, closestX, closestY) > projectile.radius * projectile.radius) continue;
                    wall.hp -= 8.0f;
                    if (wall.hp <= 0.0f) walls.erase(walls.begin() + wallIndex);
                    impact = true;
                    break;
                }
            }

            if (impact)
            {
                explodeGrenade(projectile.x, projectile.y, projectile.owner);
                projectiles.erase(projectiles.begin() + i);
            }
            continue;
        }

        if (projectile.kind == ProjectileFireShard)
        {
            bool impact = projectile.x <= mapMinX + projectile.radius || projectile.x >= mapMaxX - projectile.radius ||
                          projectile.y <= mapMinY + projectile.radius || projectile.y >= mapMaxY - projectile.radius;
            if (!impact)
            {
                for (unsigned e = 0; e < enemies.size(); ++e)
                {
                    const float radius = projectile.radius + enemies[e].radius;
                    if (distanceSquared(projectile.x, projectile.y, enemies[e].x, enemies[e].y) <= radius * radius) { impact = true; break; }
                }
            }
            if (!impact && boss.active)
            {
                if (boss.phase == 1)
                {
                    for (int weapon = 0; weapon < 3; ++weapon)
                    {
                        if (!boss.weapons[weapon].active) continue;
                        float wx, wy;
                        bossWeaponPosition(weapon, wx, wy);
                        if (distanceSquared(projectile.x, projectile.y, wx, wy) <= 30.0f * 30.0f) { impact = true; break; }
                    }
                }
                else if (boss.phase == 2 && boss.shape == BossSquare)
                {
                    for (int part = 0; part < 4; ++part)
                    {
                        if (!boss.parts[part].active) continue;
                        if (distanceSquared(projectile.x, projectile.y, boss.parts[part].x, boss.parts[part].y) <= 42.0f * 42.0f) { impact = true; break; }
                    }
                }
                else
                {
                    const float radius = projectile.radius + boss.radius;
                    impact = distanceSquared(projectile.x, projectile.y, boss.x, boss.y) <= radius * radius;
                }
            }
            if (!impact)
            {
                for (unsigned wallIndex = 0; wallIndex < walls.size(); ++wallIndex)
                {
                    const Wall& wall = walls[wallIndex];
                    const float closestX = clampf(projectile.x, wall.x, wall.x + wall.w);
                    const float closestY = clampf(projectile.y, wall.y, wall.y + wall.h);
                    if (distanceSquared(projectile.x, projectile.y, closestX, closestY) <= projectile.radius * projectile.radius) { impact = true; break; }
                }
            }
            if (impact)
            {
                createFireArea(projectile.x, projectile.y, projectile.owner);
                projectiles.erase(projectiles.begin() + i);
            }
            continue;
        }

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
                        damageBoss(projectile.damage, weapon);
                        if (projectile.owner >= 0) players[projectile.owner].score += 5;
                        hit = true;
                        break;
                    }
                }
            }
            else if (boss.shape == BossSquare && boss.phase == 2)
            {
                for (int part = 0; part < 4; ++part)
                {
                    if (!boss.parts[part].active) continue;
                    const float radius = projectile.radius + 34.0f;
                    if (distanceSquared(projectile.x, projectile.y, boss.parts[part].x, boss.parts[part].y) <= radius * radius)
                    {
                        damageBossPart(part, projectile.damage);
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
                    damageBoss(projectile.damage, -1);
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
                    const Enemy defeated = enemy;
                    rewardDefeatedEnemy(defeated, projectile.owner);
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
    const bool stopped = anyTimeStop() ||
        (boss.active && boss.shape == BossTouchpad && boss.phase == 3 && boss.freezeActive);
    for (int i = static_cast<int>(enemies.size()) - 1; i >= 0; --i)
    {
        Enemy& enemy = enemies[i];
        if (!stopped)
        {
            if (enemy.poisonTimer > 0.0f)
            {
                enemy.poisonTimer = std::max(0.0f, enemy.poisonTimer - dt);
                enemy.poisonVisualTimer -= dt;
                enemy.hp -= std::max(5.0f, enemy.maxHp * 0.075f) * dt;
                if (enemy.poisonVisualTimer <= 0.0f)
                {
                    enemy.poisonVisualTimer = 0.22f;
                    addParticles(enemy.x, enemy.y, GREEN, 2, 75.0f, 0.45f);
                }
                if (enemy.hp <= 0.0f)
                {
                    const Enemy defeated = enemy;
                    rewardDefeatedEnemy(defeated, enemy.poisonOwner);
                    addParticles(enemy.x, enemy.y, GREEN, 20, 310.0f, 0.9f);
                    enemies.erase(enemies.begin() + i);
                    continue;
                }
            }
            if (enemy.burnTimer > 0.0f)
            {
                enemy.burnTimer = std::max(0.0f, enemy.burnTimer - dt);
                enemy.burnVisualTimer -= dt;
                enemy.hp -= 10.0f * dt;
                if (enemy.burnVisualTimer <= 0.0f)
                {
                    enemy.burnVisualTimer = 0.16f;
                    addParticles(enemy.x, enemy.y, ORANGE, 2, 95.0f, 0.45f);
                }
                if (enemy.hp <= 0.0f)
                {
                    const Enemy defeated = enemy;
                    rewardDefeatedEnemy(defeated, enemy.burnOwner);
                    addParticles(enemy.x, enemy.y, ORANGE, 22, 340.0f, 0.9f);
                    enemies.erase(enemies.begin() + i);
                    continue;
                }
            }
            Player* target = nullptr;
            float nearest = 1e30f;
            if (enemy.kind == 1 && enemy.targetPlayer >= 0 && enemy.targetPlayer < playerCount && players[enemy.targetPlayer].hp > 0.0f)
                target = &players[enemy.targetPlayer];
            if (!target)
            {
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
            }
            FriendlyBot* botTarget = nullptr;
            if (enemy.kind == 0)
            {
                for (int owner = 0; owner < playerCount; ++owner)
                    for (int slot = 0; slot < 3; ++slot)
                    {
                        FriendlyBot& candidate = friendlyBots[owner][slot];
                        if (!candidate.active) continue;
                        const float d = distanceSquared(enemy.x, enemy.y, candidate.x, candidate.y);
                        if (d < nearest) { nearest = d; target = nullptr; botTarget = &candidate; }
                    }
            }
            if (target)
            {
                const float angle = angleTo(enemy.x, enemy.y, target->x, target->y);
                enemy.x += std::cos(angle) * enemy.speed * dt;
                enemy.y += std::sin(angle) * enemy.speed * dt;
            }
            else if (botTarget)
            {
                const float angle = angleTo(enemy.x, enemy.y, botTarget->x, botTarget->y);
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
                    damagePlayer(players[p], enemy.kind == 1 ? 12.0f : 15.0f);
                    addParticles(enemy.x, enemy.y, enemy.color, 16, 330.0f, 0.8f);
                    enemies.erase(enemies.begin() + i);
                    consumed = true;
                    break;
                }
            }
            if (!consumed)
            {
                for (int owner = 0; owner < playerCount && !consumed; ++owner)
                {
                    for (int slot = 0; slot < 3; ++slot)
                    {
                        FriendlyBot& bot = friendlyBots[owner][slot];
                        if (!bot.active) continue;
                        const float radius = enemy.radius + bot.radius;
                        if (distanceSquared(enemy.x, enemy.y, bot.x, bot.y) > radius * radius) continue;
                        bot.hp -= enemy.kind == 1 ? 12.0f : 15.0f;
                        addParticles(enemy.x, enemy.y, PURPLE, 12, 260.0f, 0.7f);
                        enemies.erase(enemies.begin() + i);
                        consumed = true;
                        if (bot.hp <= 0.0f)
                        {
                            bot.hp = 0.0f;
                            bot.active = false;
                            addParticles(bot.x, bot.y, RED, 28, 360.0f, 1.0f);
                            addFloatingText(bot.x, bot.y - 26.0f, "BOT " + number(slot + 1) + " DESTRUIDO", RED);
                        }
                        break;
                    }
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
            projectile.angle += projectile.rotationSpeed * dt;
            if (projectile.bounces > 0)
            {
                bool bounced = false;
                if (projectile.x <= mapMinX + projectile.radius || projectile.x >= mapMaxX - projectile.radius)
                {
                    projectile.vx = -projectile.vx;
                    bounced = true;
                }
                if (projectile.y <= mapMinY + projectile.radius || projectile.y >= mapMaxY - projectile.radius)
                {
                    projectile.vy = -projectile.vy;
                    bounced = true;
                }
                if (bounced)
                {
                    --projectile.bounces;
                    projectile.x = clampf(projectile.x, mapMinX + projectile.radius, mapMaxX - projectile.radius);
                    projectile.y = clampf(projectile.y, mapMinY + projectile.radius, mapMaxY - projectile.radius);
                    cameraShake = std::max(cameraShake, 5.0f);
                }
            }
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
            if (!hit)
            {
                for (int owner = 0; owner < playerCount && !hit; ++owner)
                {
                    for (int slot = 0; slot < 3; ++slot)
                    {
                        FriendlyBot& bot = friendlyBots[owner][slot];
                        if (!bot.active) continue;
                        const float radius = projectile.radius + bot.radius;
                        if (distanceSquared(projectile.x, projectile.y, bot.x, bot.y) > radius * radius) continue;
                        bot.hp -= projectile.damage;
                        hit = true;
                        if (bot.hp <= 0.0f)
                        {
                            bot.hp = 0.0f;
                            bot.active = false;
                            addParticles(bot.x, bot.y, RED, 28, 360.0f, 1.0f);
                            addFloatingText(bot.x, bot.y - 26.0f, "BOT " + number(slot + 1) + " DESTRUIDO", RED);
                        }
                        break;
                    }
                }
            }
        }
        if (!hit)
        {
            for (int wallIndex = static_cast<int>(walls.size()) - 1; wallIndex >= 0; --wallIndex)
            {
                Wall& wall = walls[wallIndex];
                if (projectile.x < wall.x || projectile.x > wall.x + wall.w || projectile.y < wall.y || projectile.y > wall.y + wall.h) continue;
                if (projectile.bounces > 0)
                {
                    const float leftDistance = std::abs(projectile.x - wall.x);
                    const float rightDistance = std::abs(projectile.x - (wall.x + wall.w));
                    const float topDistance = std::abs(projectile.y - wall.y);
                    const float bottomDistance = std::abs(projectile.y - (wall.y + wall.h));
                    if (std::min(leftDistance, rightDistance) < std::min(topDistance, bottomDistance)) projectile.vx = -projectile.vx;
                    else projectile.vy = -projectile.vy;
                    --projectile.bounces;
                    projectile.x += projectile.vx * dt * 2.0f;
                    projectile.y += projectile.vy * dt * 2.0f;
                    cameraShake = std::max(cameraShake, 5.0f);
                    break;
                }
                wall.hp -= 1.0f;
                if (wall.hp <= 0.0f) walls.erase(walls.begin() + wallIndex);
                hit = true;
                break;
            }
        }
        if (hit || projectile.x < mapMinX - projectile.radius || projectile.x > mapMaxX + projectile.radius || projectile.y < mapMinY - projectile.radius || projectile.y > mapMaxY + projectile.radius)
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
            if (drop.type == DropType::Heart)
                audio.playController(SoundEffect::Heart, target->pad);
            else
            {
                audio.playController(coinSoundAlternate ? SoundEffect::Coin2 : SoundEffect::Coin1, target->pad);
                coinSoundAlternate = !coinSoundAlternate;
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
        if (pressed(players[i].pad, players[i].keys[ActionStats]))
        {
            pausePad = players[i].pad;
            statsBotIndex = 0;
            screen = Screen::Stats;
            return;
        }
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
        if (player.character != CharacterTimeStop || player.timeStop <= 0.0f)
            player.skillCooldown = std::max(0.0f, player.skillCooldown - dt);
        player.fireTimer = std::max(0.0f, player.fireTimer - dt);
        player.cosmeticAnimation += dt;

        const bool skillPressed = pressed(player.pad, player.keys[ActionSkill]);
        if (skillPressed && boss.active && boss.shape == BossTouchpad && boss.phase == 3 && boss.freezeActive && skillReady(player))
        {
            boss.freezeActive = false;
            boss.abilityTimer = player.skillCooldownMax;
            player.timeStop = 0.0f;
            if (player.character == CharacterVampire) player.souls = 0;
            else player.skillCooldown = player.skillCooldownMax;
            addParticles(player.x, player.y, player.color, 70, 520.0f, 1.2f);
            addParticles(boss.x, boss.y, GOLD, 70, 520.0f, 1.2f);
            addFloatingText(player.x, player.y - 38.0f, "TEMPO ANULADO!", GOLD);
            showNotice("HABILIDADES TEMPORAIS ANULADAS");
            rumble(player.pad, 0.9f, 700);
            cameraShake = 36.0f;
        }
        else if (skillPressed) activateSkill(player, i);

        float moveX = 0.0f;
        float moveY = 0.0f;
        const bool touchpadMovement = boss.active && boss.shape == BossTouchpad;
        if (touchpadMovement)
        {
            Pad& input = pads[player.pad];
            if (input.touchActive)
            {
                if (!player.touchTracking || !input.previousTouchActive)
                {
                    player.touchTracking = true;
                    player.touchAnchorX = input.touchX;
                    player.touchAnchorY = input.touchY;
                }
                moveX = clampf((input.touchX - player.touchAnchorX) / 180.0f, -1.0f, 1.0f);
                moveY = clampf((input.touchY - player.touchAnchorY) / 180.0f, -1.0f, 1.0f);
            }
            else player.touchTracking = false;
        }
        else
        {
            player.touchTracking = false;
            moveX = axis(player.pad, 0);
            moveY = axis(player.pad, 1);
        }
        const bool bossTimeFrozen = boss.active && boss.shape == BossTouchpad && boss.phase == 3 && boss.freezeActive;
        const bool memoryLocked = boss.active && boss.shape == BossDpad && boss.phase == 3 && boss.memoryState < 4 && boss.memoryPlayer == i;
        if (bossTimeFrozen || memoryLocked) { moveX = 0.0f; moveY = 0.0f; }
        const float magnitude = std::sqrt(moveX * moveX + moveY * moveY);
        if (magnitude <= 0.18f) { moveX = 0.0f; moveY = 0.0f; }
        player.x += moveX * 300.0f * player.sensitivity * dt;
        player.y += moveY * 300.0f * player.sensitivity * dt;
        resolveWalls(player.x, player.y, player.radius);
        player.x = clampf(player.x, mapMinX + player.radius, mapMaxX - player.radius);
        player.y = clampf(player.y, mapMinY + player.radius, mapMaxY - player.radius);
        player.cameraX += (player.x - player.cameraX) * std::min(1.0f, dt * 5.0f);
        player.cameraY += (player.y - player.cameraY) * std::min(1.0f, dt * 5.0f);

        const bool shotsBlocked = bossTimeFrozen ||
            (boss.active && boss.shape == BossTouchpad && boss.phase == 2 && boss.shootSuppressed) ||
            (boss.active && boss.shape == BossDpad && boss.phase == 3 && boss.memoryState < 4);
        if (!shotsBlocked && player.fireTimer <= 0.0f && (!enemies.empty() || boss.active)) shoot(player, i);
        if (!shotsBlocked && pressed(player.pad, player.keys[ActionGrenade])) throwGrenade(player, i);
        const bool mechanicDiscount = nearMechanic(player);
        if (pressed(player.pad, player.keys[ActionUpgradeFire])) { if (mechanicDiscount) buyMechanicUpgrade(player, 1); else buyUpgrade(player, 1); }
        if (pressed(player.pad, player.keys[ActionUpgradeDamage])) { if (mechanicDiscount) buyMechanicUpgrade(player, 2); else buyUpgrade(player, 2); }
        if (pressed(player.pad, player.keys[ActionUpgradeSkill])) { if (mechanicDiscount) buyMechanicUpgrade(player, 3); else buyUpgrade(player, 3); }
        if (pressed(player.pad, player.keys[ActionBuyGrenade])) { if (mechanicDiscount) buyMechanicUpgrade(player, 4); else buyUpgrade(player, 4); }
    }

    updateCharacterAbilities(dt);
    updateSouls(dt);
    updateFriendlyBots(dt);
    updateFireAreas(dt);

    if (announcementTimer > 0.0f) announcementTimer -= dt;
    if (intermissionActive)
    {
        updateIntermission(dt);
    }
    else if (bossDelay > 0.0f)
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
    if (combatCleanupPending)
    {
        projectiles.clear();
        enemyProjectiles.clear();
        enemies.clear();
        fireAreas.clear();
        combatCleanupPending = false;
    }
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
    const int quality = static_cast<int>(graphicsQuality);
    bool restored = false;
    if (backdropCacheQuality == quality)
        restored = draw::restoreFrame(backdropCache);

    if (!restored)
    {
        draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, BG);
        const int bandCount = graphicsQuality == GraphicsQuality::High ? 8 : (graphicsQuality == GraphicsQuality::Medium ? 5 : 3);
        for (int band = 0; band < bandCount; ++band)
        {
            const int margin = band * 90;
            draw::fillRect(renderer, margin, margin / 2, SCREEN_W - margin * 2, SCREEN_H - margin, {4, static_cast<Uint8>(8 + band), static_cast<Uint8>(20 + band * 3), 30});
        }
        for (int x = 0; x < SCREEN_W; x += 80) draw::line(renderer, x, 0, x, SCREEN_H, {0, 120, 210, 9});
        for (int y = 0; y < SCREEN_H; y += 80) draw::line(renderer, 0, y, SCREEN_W, y, {0, 120, 210, 9});
        if (draw::captureFrame(backdropCache)) backdropCacheQuality = quality;
    }

    const float time = lastTick / 1000.0f;
    const int stars = qualityStars();
    for (int i = 0; i < stars; ++i)
    {
        const int x = (i * 83 + 17) % 1919;
        const int y = (i * 173 + 71) % 1079;
        const int alpha = 50 + static_cast<int>((std::sin(time * (1.2f + (i % 5) * 0.4f) + i) + 1.0f) * 45.0f);
        const Color star = i % 7 == 0 ? withAlpha(PURPLE, alpha) : (i % 5 == 0 ? withAlpha(GOLD, alpha) : withAlpha(CYAN, alpha));
        draw::fillRect(renderer, x, y, i % 9 == 0 ? 3 : 2, i % 9 == 0 ? 3 : 2, star);
    }
    if (menuTravelEffect > 0.0f)
    {
        const float strength = clampf(menuTravelEffect / 0.55f, 0.0f, 1.0f);
        for (int i = 0; i < 54; ++i)
        {
            const int x = (i * 137 + 31) % SCREEN_W;
            const int y = (i * 211 + 53) % SCREEN_H;
            const float dx = x - SCREEN_W * 0.5f;
            const float dy = y - SCREEN_H * 0.5f;
            const float length = (35.0f + (i % 8) * 16.0f) * strength;
            const float magnitude = std::sqrt(dx * dx + dy * dy) + 1.0f;
            draw::line(renderer, x, y, x + static_cast<int>(dx / magnitude * length), y + static_cast<int>(dy / magnitude * length),
                       i % 5 == 0 ? withAlpha(PURPLE, static_cast<int>(150 * strength)) : withAlpha(CYAN, static_cast<int>(130 * strength)), i % 7 == 0 ? 3 : 2);
        }
        draw::ellipse(renderer, SCREEN_W / 2, SCREEN_H / 2, static_cast<int>(520 + strength * 260), static_cast<int>(270 + strength * 150), withAlpha(CYAN, static_cast<int>(75 * strength)), 3);
    }
    for (int y = 1; y < SCREEN_H; y += 5) draw::fillRect(renderer, 0, y, SCREEN_W, 1, {80, 150, 220, 4});
    const int corner = 46;
    draw::line(renderer, 24, 24, 24 + corner, 24, withAlpha(CYAN, 85), 2);
    draw::line(renderer, 24, 24, 24, 24 + corner, withAlpha(CYAN, 85), 2);
    draw::line(renderer, SCREEN_W - 24, 24, SCREEN_W - 24 - corner, 24, withAlpha(CYAN, 85), 2);
    draw::line(renderer, SCREEN_W - 24, 24, SCREEN_W - 24, 24 + corner, withAlpha(CYAN, 85), 2);
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
            const int shimmer = x + static_cast<int>(std::fmod(lastTick / 1000.0f, 1.25f) / 1.25f * (width + 180)) - 90;
            draw::fillRect(renderer, shimmer, y + 2, 58, height - 4, {0, 212, 255, 28});
            draw::fillRect(renderer, shimmer + 58, y + 2, 26, height - 4, {187, 68, 255, 18});
            draw::outlineRect(renderer, x, y, width, height, CYAN, 2);
            draw::outlineRect(renderer, x - 6, y - 6, width + 12, height + 12, withAlpha(CYAN, 30 + static_cast<int>((std::sin(lastTick / 180.0f) + 1.0f) * 18.0f)), 2);
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
    draw::panel(renderer, 370, 28, 1180, 1025, GOLD, PANEL);
    draw::text(renderer, "CONTROLES - JOGADOR " + number(pausePad + 1), SCREEN_W / 2, 60, 5, GOLD, true);
    const char* labels[14] = {
        "PAUSAR", "HABILIDADE DO PERSONAGEM", "UPGRADE TIRO", "UPGRADE DANO",
        "UPGRADE HABILIDADE", "LANCAR GRANADA", "COMPRAR GRANADA", "ESTATISTICAS",
        "GRAFICOS",
        "SENSIBILIDADE", "VIBRACAO", "VOLUME MUSICA", "VOLUME SONS", "VOLTAR"
    };
    const int yStart = 112;
    const PlayerSettings& config = settings[std::max(0, std::min(3, pausePad))];
    for (int i = 0; i < 14; ++i)
    {
        const int y = yStart + i * 63;
        if (i == controlIndex)
        {
            draw::fillRect(renderer, 470, y - 12, 980, 50, {110, 80, 0, 65});
            draw::outlineRect(renderer, 470, y - 12, 980, 50, GOLD, 2);
            draw::triangle(renderer, 490, y + 15, 510, y + 2, 510, y + 28, GOLD);
        }
        draw::text(renderer, labels[i], 535, y, i == 1 || i == 4 || i == 5 || i == 6 || i == 7 ? 2 : 3, i == controlIndex ? WHITE : MUTED);
        std::string value;
        if (i < ActionCount) value = bindingAction == i ? "PRESSIONE UM BOTAO..." : buttonName(config.keys[i]);
        else if (i == ActionCount) value = graphicsQuality == GraphicsQuality::High ? "ALTO 60 FPS" : (graphicsQuality == GraphicsQuality::Medium ? "MEDIO 60 FPS" : "BAIXO 60 FPS");
        else if (i == ActionCount + 1) value = oneDecimal(config.sensitivity) + "X";
        else if (i == ActionCount + 2) value = number(static_cast<int>(config.rumble * 100.0f + 0.5f)) + "%";
        else if (i == ActionCount + 3) value = number(static_cast<int>(musicVolume * 100.0f + 0.5f)) + "%";
        else if (i == ActionCount + 4) value = number(static_cast<int>(soundVolume * 100.0f + 0.5f)) + "%";
        if (!value.empty()) draw::text(renderer, value, 1375 - draw::textWidth(value, 2), y + 3, 2, i == controlIndex ? CYAN : WHITE);
    }
    draw::text(renderer, "D-PAD NAVEGAR / AJUSTAR   X ALTERAR   O VOLTAR", SCREEN_W / 2, 1010, 2, MUTED, true);
}

void Game::Impl::renderShop()
{
    renderBackdrop();
    draw::panel(renderer, 120, 45, 1680, 990, PURPLE, PANEL);
    draw::text(renderer, "LOJA DE COSMETICOS", SCREEN_W / 2, 82, 5, PURPLE, true);
    draw::text(renderer, "JOGADOR " + number(pausePad + 1), SCREEN_W / 2, 135, 2, GOLD, true);
    draw::text(renderer, "PRATA DO PERFIL: " + number(profileSilver), SCREEN_W / 2, 168, 2, SILVER, true);

    const int tabX[3] = {205, 480, 755};
    const char* tabNames[3] = {"TRAJES", "ACESSORIOS", "PERSONAGENS"};
    for (int tab = 0; tab < 3; ++tab)
    {
        const bool selected = menuIndex == tab;
        draw::fillRect(renderer, tabX[tab], 220, 250, 52, selected ? Color{90, 30, 130, 120} : Color{0, 4, 16, 180});
        draw::outlineRect(renderer, tabX[tab], 220, 250, 52, selected ? PURPLE : withAlpha(MUTED, 90), selected ? 2 : 1);
        draw::text(renderer, tabNames[tab], tabX[tab] + 125, 237, tab == 2 ? 1 : 2, selected ? WHITE : MUTED, true);
    }

    const bool skins = shopTab == ShopTab::Skins;
    const bool characters = shopTab == ShopTab::Characters;
    const CosmeticItem* catalog = skins ? SKINS : HATS;
    const uint32_t owned = characters ? ownedCharacters : (skins ? ownedSkins : ownedHats);
    const int activeItem = characters ? activeCharacters[pausePad] : (skins ? activeSkins[pausePad] : activeHats[pausePad]);
    const int itemCount = characters ? CharacterCount : COSMETIC_COUNT;
    for (int item = 0; item < itemCount; ++item)
    {
        const int column = item % 3;
        const int row = item / 3;
        const int x = 220 + column * 285;
        const int y = 305 + row * 125;
        const bool selected = menuIndex == item + 3;
        const bool itemOwned = (owned & (1u << item)) != 0;
        const bool equipped = activeItem == item;
        const Color border = equipped ? GREEN : (itemOwned ? GOLD : PURPLE);
        draw::fillRect(renderer, x, y, 265, 108, selected ? Color{60, 25, 90, 190} : Color{0, 4, 16, 180});
        draw::outlineRect(renderer, x, y, 265, 108, selected ? WHITE : withAlpha(border, 150), selected ? 3 : 1);
        const Color previewColor = characters ? CHARACTERS[item].color : PLAYER_COLORS[pausePad];
        renderAvatar(x + 42, y + 55, 20, previewColor,
                     characters ? activeSkins[pausePad] : (skins ? item : 0),
                     characters ? activeHats[pausePad] : (skins ? 0 : item), lastTick / 900.0f, false);
        const std::string name = characters ? CHARACTERS[item].name : catalog[item].name;
        draw::text(renderer, name, x + 78, y + 20, name.size() > 17 ? 1 : 2, WHITE);
        const int itemCost = characters ? CHARACTERS[item].cost : catalog[item].cost;
        const std::string status = equipped ? "EQUIPADO" : (itemOwned ? "X EQUIPAR" : (itemCost == 0 ? "GRATIS" : number(itemCost) + " SILVER"));
        draw::text(renderer, status, x + 78, y + 67, 1, equipped ? GREEN : (itemOwned ? GOLD : SILVER));
    }

    draw::panel(renderer, 1130, 250, 500, 560, PURPLE, {1, 4, 15, 210});
    draw::text(renderer, characters ? "PERSONAGEM" : "MONSTRUARIO", 1380, 292, 3, PURPLE, true);
    renderAvatar(1380, 500, 72, characters ? CHARACTERS[activeCharacters[pausePad]].color : PLAYER_COLORS[pausePad], activeSkins[pausePad], activeHats[pausePad], lastTick / 900.0f, false);
    if (characters)
    {
        draw::text(renderer, std::string("ATIVO: ") + CHARACTERS[activeCharacters[pausePad]].name, 1380, 635, 2, WHITE, true);
        draw::text(renderer, CHARACTERS[activeCharacters[pausePad]].description, 1380, 685, 1, GREEN, true);
        draw::text(renderer, activeCharacters[pausePad] == CharacterVampire ? "50 ALMAS PARA ATIVAR" : "R2 / BOTAO MAPEADO", 1380, 735, 1, MUTED, true);
    }
    else
    {
        draw::text(renderer, std::string("TRAJE: ") + SKINS[activeSkins[pausePad]].name, 1380, 635, 2, WHITE, true);
        draw::text(renderer, std::string("ACESSORIO: ") + HATS[activeHats[pausePad]].name, 1380, 685, 1, WHITE, true);
        draw::text(renderer, "EQUIPAMENTO ATUAL", 1380, 735, 1, MUTED, true);
    }

    const bool backSelected = menuIndex == 15;
    if (backSelected) draw::outlineRect(renderer, 770, 875, 380, 54, WHITE, 2);
    draw::text(renderer, "VOLTAR", SCREEN_W / 2, 893, 3, backSelected ? WHITE : MUTED, true);
    draw::text(renderer, "D-PAD NAVEGA   X COMPRA / EQUIPA   O VOLTA", SCREEN_W / 2, 970, 2, MUTED, true);
}

void Game::Impl::renderStats()
{
    renderPlaying();
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, {0, 0, 8, 145});
    int playerIndex = 0;
    for (int i = 0; i < playerCount; ++i) if (players[i].pad == pausePad) { playerIndex = i; break; }
    const Player& player = players[playerIndex];
    draw::panel(renderer, 410, 125, 1100, 850, player.color, {2, 8, 24, 244});
    draw::text(renderer, "ESTATISTICAS - JOGADOR " + number(playerIndex + 1), SCREEN_W / 2, 215, 4, player.color, true);
    draw::text(renderer, CHARACTERS[player.character].name, SCREEN_W / 2, 270, 2, CHARACTERS[player.character].color, true);
    const int x = 470;
    const int valueX = player.character == CharacterManipulator ? 990 : 1390;
    const int y = 350;
    const int spacing = 72;
    const std::string values[5] = {
        oneDecimal(1.0f / std::max(0.01f, player.fireRate)) + "/S   LV." + number(player.fireLevel),
        oneDecimal(20.0f * player.damageMultiplier) + "   LV." + number(player.damageLevel),
        (player.character == CharacterVampire ? number(player.souls) + "/50 ALMAS" :
         (player.character == CharacterManipulator ? "3 BOTS" : oneDecimal(player.skillDuration) + "S")) + "   LV." + number(player.skillLevel),
        number(player.grenades), number(player.kills)
    };
    const char* labels[5] = {"VELOCIDADE DO TIRO", "DANO DO JOGADOR", "HABILIDADE", "GRANADAS", "KILLS"};
    for (int i = 0; i < 5; ++i)
    {
        const int rowWidth = player.character == CharacterManipulator ? 540 : 860;
        draw::fillRect(renderer, x, y + i * spacing - 10, rowWidth, 54, i % 2 == 0 ? Color{0, 35, 65, 95} : Color{0, 10, 25, 120});
        draw::text(renderer, labels[i], x + 22, y + i * spacing + 5, 2, MUTED);
        draw::text(renderer, values[i], valueX - draw::textWidth(values[i], 2), y + i * spacing + 5, 2, i == 2 ? CHARACTERS[player.character].color : WHITE);
    }
    if (player.character == CharacterManipulator)
    {
        draw::text(renderer, "NPCS ALIADOS", 1245, 330, 2, PURPLE, true);
        for (int slot = 0; slot < 3; ++slot)
        {
            const FriendlyBot& bot = friendlyBots[playerIndex][slot];
            const int botY = 375 + slot * 100;
            const bool selected = statsBotIndex == slot;
            draw::fillRect(renderer, 1050, botY, 390, 82, selected ? Color{70, 15, 100, 190} : Color{0, 8, 20, 200});
            draw::outlineRect(renderer, 1050, botY, 390, 82, selected ? GREEN : withAlpha(PURPLE, 120), selected ? 3 : 1);
            draw::text(renderer, "BOT " + number(slot + 1), 1070, botY + 14, 2, bot.active ? WHITE : MUTED);
            const std::string botLife = bot.active ? number(static_cast<int>(std::ceil(bot.hp))) + "/" + number(static_cast<int>(std::ceil(bot.maxHp))) : "INATIVO";
            draw::text(renderer, botLife, 1070, botY + 49, 1, bot.active ? GREEN : RED);
            draw::fillRect(renderer, 1318, botY + 19, 102, 44, GREEN);
            draw::text(renderer, "X 5G", 1369, botY + 32, 1, BG, true);
        }
        draw::text(renderer, "D-PAD ESCOLHE   X REPARA METADE DA VIDA", 1245, 700, 1, MUTED, true);
    }
    draw::text(renderer, std::string("[") + buttonName(player.keys[ActionStats]) + "] OU O PARA VOLTAR", SCREEN_W / 2, 910, 2, MUTED, true);
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
    if (enemy.kind == 1 || enemy.edges == 8)
    {
        const float c = std::cos(enemy.angle);
        const float s = std::sin(enemy.angle);
        const int ax = static_cast<int>(c * radius);
        const int ay = static_cast<int>(s * radius);
        const int bx = static_cast<int>(-s * radius);
        const int by = static_cast<int>(c * radius);
        draw::line(renderer, x - ax - bx, y - ay - by, x + ax + bx, y + ay + by, withAlpha(enemy.color, 70), 10);
        draw::line(renderer, x - ax + bx, y - ay + by, x + ax - bx, y + ay - by, withAlpha(enemy.color, 70), 10);
        draw::line(renderer, x - ax - bx, y - ay - by, x + ax + bx, y + ay + by, enemy.color, 4);
        draw::line(renderer, x - ax + bx, y - ay + by, x + ax - bx, y + ay - by, enemy.color, 4);
        draw::fillCircle(renderer, x, y, 4, WHITE);
    }
    else if (enemy.edges == 3)
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
    else if (enemy.edges == 4)
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

    if (enemy.poisonTimer > 0.0f)
    {
        draw::circle(renderer, x, y, radius + 7, GREEN);
        draw::fillCircle(renderer, x + radius / 2, y - radius / 2, 3, withAlpha(GREEN, 190));
    }
    if (enemy.burnTimer > 0.0f)
    {
        draw::circle(renderer, x, y, radius + 10, ORANGE);
        draw::triangle(renderer, x - 6, y - radius - 4, x, y - radius - 17, x + 6, y - radius - 4, ORANGE);
    }

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

    if (boss.phase == 3 && boss.shape == BossX && boss.laserActive)
    {
        for (int tip = 0; tip < 4; ++tip)
        {
            const float laserAngle = boss.angle + PI * 0.25f + tip * PI * 0.5f;
            const float startX = boss.x + std::cos(laserAngle) * boss.radius;
            const float startY = boss.y + std::sin(laserAngle) * boss.radius;
            const Vec2 laserStart = toScreen(viewport, cameraPlayer, startX, startY);
            const Vec2 laserEnd = toScreen(viewport, cameraPlayer, startX + std::cos(laserAngle) * 2800.0f, startY + std::sin(laserAngle) * 2800.0f);
            draw::line(renderer, static_cast<int>(laserStart.x), static_cast<int>(laserStart.y), static_cast<int>(laserEnd.x), static_cast<int>(laserEnd.y), {255, 20, 60, 34}, 34);
            draw::line(renderer, static_cast<int>(laserStart.x), static_cast<int>(laserStart.y), static_cast<int>(laserEnd.x), static_cast<int>(laserEnd.y), {255, 35, 85, 115}, 18);
            draw::line(renderer, static_cast<int>(laserStart.x), static_cast<int>(laserStart.y), static_cast<int>(laserEnd.x), static_cast<int>(laserEnd.y), WHITE, 4);
        }
    }

    if (boss.shape == BossSquare && boss.phase == 2)
    {
        draw::glowCircle(renderer, x, y, 18, withAlpha(PURPLE, 70), 18);
        for (int i = 0; i < 4; ++i)
        {
            const BossPart& part = boss.parts[i];
            const Vec2 partPoint = toScreen(viewport, cameraPlayer, part.x, part.y);
            const int px = static_cast<int>(partPoint.x);
            const int py = static_cast<int>(partPoint.y);
            const Color partColor = part.collected ? CYAN : (part.collectible ? GOLD : PURPLE);
            const bool horizontal = i == 0 || i == 2;
            draw::line(renderer, px - (horizontal ? 42 : 0), py - (horizontal ? 0 : 42),
                       px + (horizontal ? 42 : 0), py + (horizontal ? 0 : 42), withAlpha(partColor, 50), 18);
            draw::line(renderer, px - (horizontal ? 42 : 0), py - (horizontal ? 0 : 42),
                       px + (horizontal ? 42 : 0), py + (horizontal ? 0 : 42), partColor, 7);
            if (part.active)
            {
                draw::fillRect(renderer, px - 36, py - 54, 72, 6, {0, 0, 0, 220});
                draw::fillRect(renderer, px - 36, py - 54, static_cast<int>(72.0f * std::max(0.0f, part.hp / part.maxHp)), 6, PURPLE);
            }
            else if (part.collectible) draw::ellipse(renderer, px, py, 58, 28, withAlpha(GOLD, 180), 3);
        }
        return;
    }

    if (point.x < viewport.x - 220 || point.x > viewport.x + viewport.w + 220 || point.y < viewport.y - 220 || point.y > viewport.y + viewport.h + 220) return;

    if (boss.phase == 1)
        draw::glowCircle(renderer, x, y, radius, body, 24);
    else if (boss.shape == BossX)
    {
        const float c = std::cos(boss.angle);
        const float s = std::sin(boss.angle);
        const int ax = static_cast<int>(c * radius * 0.72f);
        const int ay = static_cast<int>(s * radius * 0.72f);
        const int bx = static_cast<int>(-s * radius * 0.72f);
        const int by = static_cast<int>(c * radius * 0.72f);
        draw::line(renderer, x - ax - bx, y - ay - by, x + ax + bx, y + ay + by, withAlpha(body, 45), 28);
        draw::line(renderer, x - ax + bx, y - ay + by, x + ax - bx, y + ay - by, withAlpha(body, 45), 28);
        draw::line(renderer, x - ax - bx, y - ay - by, x + ax + bx, y + ay + by, body, 12);
        draw::line(renderer, x - ax + bx, y - ay + by, x + ax - bx, y + ay - by, body, 12);
    }
    else if (boss.shape == BossSquare)
    {
        draw::fillRect(renderer, x - radius, y - radius, radius * 2, radius * 2, withAlpha(body, 80));
        draw::outlineRect(renderer, x - radius, y - radius, radius * 2, radius * 2, withAlpha(body, 45), 18);
        draw::outlineRect(renderer, x - radius, y - radius, radius * 2, radius * 2, body, 6);
        draw::outlineRect(renderer, x - radius + 12, y - radius + 12, radius * 2 - 24, radius * 2 - 24, withAlpha(WHITE, 55), 2);
    }
    else if (boss.shape == BossTriangle)
    {
        int px[3], py[3];
        for (int i = 0; i < 3; ++i)
        {
            const float angle = boss.angle + i * PI * 2.0f / 3.0f - PI * 0.5f;
            px[i] = x + static_cast<int>(std::cos(angle) * radius);
            py[i] = y + static_cast<int>(std::sin(angle) * radius);
        }
        draw::triangle(renderer, px[0], py[0], px[1], py[1], px[2], py[2], body);
        for (int i = 0; i < 3; ++i) draw::line(renderer, px[i], py[i], px[(i + 1) % 3], py[(i + 1) % 3], withAlpha(WHITE, 90), 3);
    }
    else if (boss.shape == BossCircle)
    {
        Color circleColor = body;
        if (boss.phase == 3 && boss.state == 0)
        {
            const float pulse = (std::sin(lastTick / 45.0f) + 1.0f) * 0.5f;
            circleColor = pulse > 0.45f ? WHITE : RED;
            draw::glowCircle(renderer, x, y, radius + static_cast<int>(pulse * 18.0f), withAlpha(RED, 120), 35);
        }
        draw::glowCircle(renderer, x, y, radius, circleColor, 28);
        draw::circle(renderer, x, y, radius - 13, withAlpha(WHITE, 80));
    }
    else if (boss.shape == BossDpad)
    {
        const auto directionColor = [&](int direction) -> Color
        {
            if ((boss.memoryRemovedMask & (1u << direction)) != 0) return {35, 40, 55, 100};
            if (boss.phase == 2 && boss.telegraphDirection == direction) return RED;
            if (boss.phase == 3 && boss.memoryFlashTimer > 0.0f)
            {
                if (boss.memoryFlashDirection == -2) return GOLD;
                if (boss.memoryFlashDirection == direction)
                    return boss.memoryState == 1 ? GREEN : (boss.memoryState == 2 ? CYAN : RED);
            }
            return body;
        };
        draw::fillCircle(renderer, x, y, 24, withAlpha(body, 170));
        for (int direction = 0; direction < 4; ++direction)
        {
            const Color directionBody = directionColor(direction);
            if (direction == 0)
            {
                draw::fillRect(renderer, x - 17, y - radius + 24, 34, radius - 35, directionBody);
                draw::triangle(renderer, x, y - radius - 8, x - 34, y - radius + 30, x + 34, y - radius + 30, directionBody);
            }
            else if (direction == 1)
            {
                draw::fillRect(renderer, x + 12, y - 17, radius - 24, 34, directionBody);
                draw::triangle(renderer, x + radius + 8, y, x + radius - 30, y - 34, x + radius - 30, y + 34, directionBody);
            }
            else if (direction == 2)
            {
                draw::fillRect(renderer, x - 17, y + 12, 34, radius - 24, directionBody);
                draw::triangle(renderer, x, y + radius + 8, x - 34, y + radius - 30, x + 34, y + radius - 30, directionBody);
            }
            else
            {
                draw::fillRect(renderer, x - radius + 24, y - 17, radius - 35, 34, directionBody);
                draw::triangle(renderer, x - radius - 8, y, x - radius + 30, y - 34, x - radius + 30, y + 34, directionBody);
            }
        }
    }
    else
    {
        const int width = radius * 3;
        const int height = radius * 2;
        draw::panel(renderer, x - width / 2, y - height / 2, width, height, body,
                    boss.phase >= 2 ? Color{1, 1, 3, 245} : Color{5, 12, 25, 230});
        draw::outlineRect(renderer, x - width / 2 + 13, y - height / 2 + 13, width - 26, height - 26, withAlpha(WHITE, 55), 2);
        const int scan = x - width / 2 + 16 + static_cast<int>(std::fmod(lastTick / 1000.0f, 1.0f) * (width - 32));
        draw::line(renderer, scan, y - height / 2 + 16, scan, y + height / 2 - 16, withAlpha(CYAN, 90), 3);
        draw::fillCircle(renderer, x - 32, y, 9, withAlpha(CYAN, 170));
        draw::fillCircle(renderer, x + 32, y, 9, withAlpha(PURPLE, 170));
        if (boss.shootSuppressed || boss.freezeActive)
            draw::ellipse(renderer, x, y, width / 2 + 24, height / 2 + 24, boss.freezeActive ? GOLD : RED, 5);
    }

    draw::fillCircle(renderer, x - 12, y - 15, 13, {255, 255, 255, 38});
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
    const Color rgbPrimary = rainbowColor(animation * 3.2f);
    const Color rgbSecondary = rainbowColor(animation * 3.2f + PI * 0.66f);
    const Color rgbTertiary = rainbowColor(animation * 3.2f + PI * 1.33f);
    Color body = colorValue;
    if (skin == 5) body = withAlpha(body, 135);
    if (skin == 10) body = {8, 5, 17, 255};
    if (skin == 11) body = rgbPrimary;

    if (skin == 2) draw::glowCircle(renderer, x, y, radius + static_cast<int>(radius * 1.25f), withAlpha(colorValue, 34), qualityGlow(12));
    if (skin == 11)
    {
        draw::glowCircle(renderer, x, y, radius + static_cast<int>(13.0f * scale), withAlpha(rgbPrimary, 70), qualityGlow(static_cast<int>(28.0f * scale)));
        draw::glowCircle(renderer, x, y, radius + static_cast<int>(6.0f * scale), withAlpha(rgbSecondary, 105), qualityGlow(static_cast<int>(18.0f * scale)));
    }
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
        draw::circle(renderer, x, y, radius + ring, rgbSecondary);
        draw::circle(renderer, x, y, radius + ring + std::max(2, static_cast<int>(4.0f * scale)), rgbTertiary);
        for (int segment = 0; segment < 8; ++segment)
        {
            const float start = animation * 1.7f + segment * PI * 0.25f;
            const float end = start + PI * 0.18f;
            const Color segmentColor = rainbowColor(animation * 3.2f + segment * PI * 0.25f);
            draw::line(renderer,
                       x + static_cast<int>(std::cos(start) * (radius + ring)),
                       y + static_cast<int>(std::sin(start) * (radius + ring)),
                       x + static_cast<int>(std::cos(end) * (radius + ring)),
                       y + static_cast<int>(std::sin(end) * (radius + ring)),
                       segmentColor, std::max(2, static_cast<int>(4.0f * scale)));
        }
        for (int spark = 0; spark < 6; ++spark)
        {
            const float angle = -animation * 2.4f + spark * PI / 3.0f;
            const int orbit = radius + ring + static_cast<int>(10.0f * scale);
            const Color sparkColor = rainbowColor(animation * 4.0f + spark * PI / 3.0f);
            draw::glowCircle(renderer,
                             x + static_cast<int>(std::cos(angle) * orbit),
                             y + static_cast<int>(std::sin(angle) * orbit),
                             std::max(2, static_cast<int>(3.0f * scale)), sparkColor, qualityGlow(8));
        }
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

void Game::Impl::renderCharacterEffects(const Viewport& viewport, const Player& cameraPlayer, const Player& player, int)
{
    if (player.hp <= 0.0f) return;
    const Vec2 point = toScreen(viewport, cameraPlayer, player.x, player.y);
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    if (player.poisonAreaTimer > 0.0f)
    {
        draw::ellipse(renderer, x, y, 235, 150, withAlpha(GREEN, 135), 4);
        draw::ellipse(renderer, x, y, 205, 130, withAlpha(GREEN, 55), 2);
    }
    if (player.shieldTimer > 0.0f)
    {
        const int pulse = 35 + static_cast<int>(std::sin(lastTick / 90.0f) * 3.0f);
        draw::glowCircle(renderer, x, y, pulse, withAlpha(WHITE, 90), qualityGlow(14));
        draw::circle(renderer, x, y, pulse, WHITE);
    }
    if (player.droneTimer > 0.0f)
    {
        const float droneWorldX = player.x + std::cos(player.droneAngle) * 58.0f;
        const float droneWorldY = player.y + std::sin(player.droneAngle) * 42.0f - 22.0f;
        const Vec2 drone = toScreen(viewport, cameraPlayer, droneWorldX, droneWorldY);
        draw::glowCircle(renderer, static_cast<int>(drone.x), static_cast<int>(drone.y), 10, GOLD, qualityGlow(8));
        draw::circle(renderer, static_cast<int>(drone.x), static_cast<int>(drone.y), 13, WHITE);
        if (player.droneBeamTimer > 0.0f)
        {
            const Vec2 target = toScreen(viewport, cameraPlayer, player.droneTargetX, player.droneTargetY);
            draw::line(renderer, static_cast<int>(drone.x), static_cast<int>(drone.y), static_cast<int>(target.x), static_cast<int>(target.y), GOLD, 4);
        }
    }
}

void Game::Impl::renderIntermission(const Viewport& viewport, const Player& cameraPlayer)
{
    if (!intermissionActive) return;
    const float centerX = (mapMinX + mapMaxX) * 0.5f;
    const float centerY = (mapMinY + mapMaxY) * 0.5f;
    const float portalY = mapMinY + 105.0f;
    const Vec2 portal = toScreen(viewport, cameraPlayer, centerX, portalY);
    const int px = static_cast<int>(portal.x);
    const int py = static_cast<int>(portal.y);
    const int pulse = 8 + static_cast<int>((std::sin(lastTick / 90.0f) + 1.0f) * 5.0f);
    draw::fillRect(renderer, px - 145, py - 62, 290, 124, {0, 35, 75, 120});
    draw::outlineRect(renderer, px - 145, py - 62, 290, 124, CYAN, 4);
    draw::outlineRect(renderer, px - 132 - pulse / 2, py - 50 - pulse / 2, 264 + pulse, 100 + pulse, withAlpha(PURPLE, 135), 3);
    for (int light = 0; light < 8; ++light)
    {
        const int lx = px - 126 + light * 36;
        const Color colorValue = light % 2 == 0 ? CYAN : PURPLE;
        draw::glowCircle(renderer, lx, py - 49, 5, colorValue, qualityGlow(8));
        draw::glowCircle(renderer, lx, py + 49, 5, colorValue, qualityGlow(8));
    }
    draw::text(renderer, "PORTAL DA PROXIMA ONDA", px, py - 9, 1, WHITE, true);
    draw::fillRect(renderer, px - 100, py + 17, 200, 8, {15, 25, 45, 220});
    draw::fillRect(renderer, px - 100, py + 17, static_cast<int>(200.0f * clampf(portalCharge / 1.25f, 0.0f, 1.0f)), 8, GREEN);

    const Vec2 mechanic = toScreen(viewport, cameraPlayer, centerX, centerY);
    const int mx = static_cast<int>(mechanic.x);
    const int my = static_cast<int>(mechanic.y);
    draw::fillRect(renderer, mx - 33, my - 18, 66, 76, {30, 38, 55, 255});
    draw::outlineRect(renderer, mx - 33, my - 18, 66, 76, GOLD, 3);
    draw::fillCircle(renderer, mx, my - 43, 25, {205, 154, 105, 255});
    draw::fillRect(renderer, mx - 31, my - 66, 62, 15, {35, 44, 62, 255});
    draw::fillRect(renderer, mx - 21, my - 79, 42, 17, {35, 44, 62, 255});
    draw::circle(renderer, mx - 8, my - 45, 3, BG);
    draw::circle(renderer, mx + 8, my - 45, 3, BG);
    const int crateOffsets[6][2] = {{-135, -5}, {92, 16}, {-105, 70}, {118, 82}, {-175, 72}, {165, -2}};
    for (int crate = 0; crate < 6; ++crate)
    {
        const int cx = mx + crateOffsets[crate][0];
        const int cy = my + crateOffsets[crate][1];
        draw::fillRect(renderer, cx - 24, cy - 20, 48, 40, crate % 2 == 0 ? Color{92, 56, 24, 255} : Color{45, 62, 82, 255});
        draw::outlineRect(renderer, cx - 24, cy - 20, 48, 40, GOLD, 2);
        draw::line(renderer, cx - 20, cy - 16, cx + 20, cy + 16, withAlpha(GOLD, 120), 2);
    }
    draw::text(renderer, "MAQUINISTA - 30% OFF", mx, my - 116, 2, GOLD, true);
    draw::text(renderer, "APROXIME-SE E USE OS BOTOES DE UPGRADE", mx, my + 112, 1, MUTED, true);
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
    const int leftWidth = std::min(310, viewport.w / 2 - 24);
    const int statsHeight = 134;
    const int barsHeight = 90;
    const int gap = 7;
    const int statsY = bottom - barsHeight - gap - statsHeight;
    const int barsY = bottom - barsHeight;

    draw::fillRect(renderer, leftX, statsY, leftWidth, statsHeight, {2, 6, 20, 230});
    draw::outlineRect(renderer, leftX, statsY, leftWidth, statsHeight, withAlpha(CYAN, 45), 1);
    draw::text(renderer, "JOGADOR " + number(index + 1), leftX + 15, statsY + 13, 1, player.color);
    draw::text(renderer, "K " + number(player.kills) + " KILLS", leftX + 15, statsY + 43, 2, PINK);
    draw::text(renderer, "G", leftX + 15, statsY + 72, 2, GOLD);
    draw::text(renderer, number(player.coins), leftX + 43, statsY + 72, 2, WHITE);
    draw::text(renderer, "S", leftX + 128, statsY + 72, 2, SILVER);
    draw::text(renderer, number(profileSilver), leftX + 156, statsY + 72, 2, WHITE);
    draw::text(renderer, "GRANADA: " + number(player.grenades) + " [" + buttonName(player.keys[ActionGrenade]) + "]", leftX + 15, statsY + 102, 2, GRENADE_GREEN);

    draw::fillRect(renderer, leftX, barsY, leftWidth, barsHeight, {2, 6, 20, 230});
    draw::outlineRect(renderer, leftX, barsY, leftWidth, barsHeight, withAlpha(CYAN, 45), 1);
    draw::text(renderer, "INTEGRIDADE", leftX + 15, barsY + 10, 1, MUTED);
    draw::fillRect(renderer, leftX + 15, barsY + 29, leftWidth - 30, 8, {30, 35, 50, 220});
    draw::fillRect(renderer, leftX + 15, barsY + 29, static_cast<int>((leftWidth - 30) * std::max(0.0f, player.hp / player.maxHp)), 8, {255, 88, 55, 255});
    draw::text(renderer, std::string("HABILIDADE LV.") + number(player.skillLevel) + " [" + buttonName(player.keys[ActionSkill]) + "]", leftX + 15, barsY + 48, 1, MUTED);
    draw::fillRect(renderer, leftX + 15, barsY + 69, leftWidth - 30, 8, {30, 35, 50, 220});
    float activeDuration = player.timeStop;
    if (player.character == CharacterDamageArea) activeDuration = player.poisonAreaTimer;
    else if (player.character == CharacterShield) activeDuration = player.shieldTimer;
    else if (player.character == CharacterDrone) activeDuration = player.droneTimer;
    float skillRatio = player.character == CharacterVampire ? player.souls / 50.0f :
        (activeDuration > 0.0f ? activeDuration / std::max(0.01f, player.skillDuration) : 1.0f - player.skillCooldown / std::max(0.01f, player.skillCooldownMax));
    draw::fillRect(renderer, leftX + 15, barsY + 69, static_cast<int>((leftWidth - 30) * clampf(skillRatio, 0.0f, 1.0f)), 8,
                   activeDuration > 0.0f ? RED : CHARACTERS[player.character].color);

    if (viewport.w >= 620)
    {
        const bool mechanicDiscount = nearMechanic(player);
        const auto displayedPrice = [&](int value) -> int { return mechanicDiscount ? std::max(1, static_cast<int>(std::ceil(value * 0.70f))) : value; };
        const int shopWidth = 400;
        const int shopHeight = 164;
        const int shopX = viewport.x + viewport.w - padding - shopWidth;
        const int shopY = bottom - shopHeight;
        draw::fillRect(renderer, shopX, shopY, shopWidth, shopHeight, {2, 6, 20, 230});
        draw::outlineRect(renderer, shopX, shopY, shopWidth, shopHeight, withAlpha(CYAN, 45), 1);
        draw::text(renderer, mechanicDiscount ? "MAQUINISTA -30%" : "UPGRADES", shopX + 15, shopY + 12, 1, mechanicDiscount ? GOLD : MUTED);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionUpgradeFire]) + "]", shopX + 15, shopY + 40, 1, MUTED);
        draw::text(renderer, "TIRO LV." + number(player.fireLevel), shopX + 170, shopY + 39, 1, WHITE);
        const std::string firePrice = player.fireLevel >= 10 ? "MAX" : number(displayedPrice(player.fireCost)) + " G";
        draw::text(renderer, firePrice, shopX + 385 - draw::textWidth(firePrice, 1), shopY + 39, 1, GOLD);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionUpgradeDamage]) + "]", shopX + 15, shopY + 68, 1, MUTED);
        draw::text(renderer, "DANO LV." + number(player.damageLevel), shopX + 170, shopY + 67, 1, WHITE);
        const std::string damagePrice = player.damageLevel >= 10 ? "MAX" : number(displayedPrice(player.damageCost)) + " G";
        draw::text(renderer, damagePrice, shopX + 385 - draw::textWidth(damagePrice, 1), shopY + 67, 1, GOLD);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionUpgradeSkill]) + "]", shopX + 15, shopY + 96, 1, MUTED);
        draw::text(renderer, "HABILIDADE LV." + number(player.skillLevel), shopX + 170, shopY + 95, 1, WHITE);
        const std::string skillPrice = number(displayedPrice(player.skillCost)) + " G";
        draw::text(renderer, skillPrice, shopX + 385 - draw::textWidth(skillPrice, 1), shopY + 95, 1, GOLD);
        draw::text(renderer, std::string("[") + buttonName(player.keys[ActionBuyGrenade]) + "]", shopX + 15, shopY + 124, 1, GRENADE_GREEN);
        draw::text(renderer, "GRANADA", shopX + 170, shopY + 123, 1, GRENADE_GREEN);
        const std::string grenadePrice = number(mechanicDiscount ? 8 : GRENADE_COST) + " G";
        draw::text(renderer, grenadePrice, shopX + 385 - draw::textWidth(grenadePrice, 1), shopY + 123, 1, GOLD);
    }
}

void Game::Impl::paintViewportBase(const Viewport& viewport, int driftX, int driftY)
{
    draw::setClipRect(viewport.x, viewport.y, viewport.w, viewport.h);
    draw::fillRect(renderer, viewport.x, viewport.y, viewport.w, viewport.h, BG_BLUE);
    if (graphicsQuality == GraphicsQuality::High)
    {
        draw::fillRect(renderer, viewport.x + viewport.w / 12 + driftX, viewport.y + viewport.h * 2 / 3 + driftY, viewport.w * 2 / 3, viewport.h / 4, {70, 20, 170, 18});
        draw::fillRect(renderer, viewport.x + viewport.w / 2 - driftX, viewport.y + viewport.h / 10 - driftY, viewport.w / 2, viewport.h / 4, {0, 120, 200, 13});
        draw::fillRect(renderer, viewport.x + viewport.w / 3, viewport.y + viewport.h / 3, viewport.w / 3, viewport.h / 3, {180, 0, 100, 9});
    }
    draw::clearClipRect();
}

void Game::Impl::preparePlayingBackground()
{
    const int quality = static_cast<int>(graphicsQuality);
    const int layout = std::max(1, std::min(4, playerCount));
    const float backgroundTime = lastTick / 1000.0f;
    const int driftX = graphicsQuality == GraphicsQuality::High ? static_cast<int>(std::sin(backgroundTime * 0.12f) * 35.0f) : 0;
    const int driftY = graphicsQuality == GraphicsQuality::High ? static_cast<int>(std::cos(backgroundTime * 0.09f) * 25.0f) : 0;

    if (playingCacheQuality == quality && playingCacheLayout == layout &&
        playingCacheDriftX == driftX && playingCacheDriftY == driftY &&
        draw::restoreFrame(playingBackgroundCache))
        return;

    draw::clearClipRect();
    draw::fillRect(renderer, 0, 0, SCREEN_W, SCREEN_H, BG);
    if (layout == 1)
        paintViewportBase({0, 0, SCREEN_W, SCREEN_H}, driftX, driftY);
    else if (layout == 2)
    {
        paintViewportBase({0, 0, SCREEN_W / 2, SCREEN_H}, driftX, driftY);
        paintViewportBase({SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H}, driftX, driftY);
    }
    else
    {
        const int halfW = SCREEN_W / 2;
        const int halfH = SCREEN_H / 2;
        paintViewportBase({0, 0, halfW, halfH}, driftX, driftY);
        paintViewportBase({halfW, 0, halfW, halfH}, driftX, driftY);
        paintViewportBase({0, halfH, halfW, halfH}, driftX, driftY);
        if (layout == 4) paintViewportBase({halfW, halfH, halfW, halfH}, driftX, driftY);
    }
    draw::clearClipRect();
    if (draw::captureFrame(playingBackgroundCache))
    {
        playingCacheQuality = quality;
        playingCacheLayout = layout;
        playingCacheDriftX = driftX;
        playingCacheDriftY = driftY;
    }
}

void Game::Impl::renderViewport(const Viewport& viewport, const Player& cameraPlayer, int playerIndex)
{
    draw::setClipRect(viewport.x, viewport.y, viewport.w, viewport.h);

    const float backgroundTime = lastTick / 1000.0f;
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
    if (anyTimeStop())
    {
        draw::outlineRect(renderer, viewport.x + 3, viewport.y + 3, viewport.w - 6, viewport.h - 6, withAlpha(CYAN, 125), 4);
        for (int y = viewport.y + 8; y < viewport.y + viewport.h; y += 18)
            draw::fillRect(renderer, viewport.x, y, viewport.w, 1, {0, 130, 220, 18});
    }

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
    renderIntermission(viewport, cameraPlayer);

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
    for (unsigned i = 0; i < fireAreas.size(); ++i)
    {
        const FireArea& area = fireAreas[i];
        const Vec2 point = toScreen(viewport, cameraPlayer, area.x, area.y);
        const int radius = static_cast<int>(area.radius + std::sin(area.phase) * 2.0f);
        draw::glowCircle(renderer, static_cast<int>(point.x), static_cast<int>(point.y), radius, withAlpha(ORANGE, 105), qualityGlow(10));
        draw::circle(renderer, static_cast<int>(point.x), static_cast<int>(point.y), radius, ORANGE);
        draw::fillCircle(renderer, static_cast<int>(point.x), static_cast<int>(point.y), std::max(3, radius / 3), {255, 75, 0, 105});
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
        if (projectiles[i].kind == ProjectileGrenade)
        {
            const Vec2 grenadePoint = toScreen(viewport, cameraPlayer, projectiles[i].x, projectiles[i].y);
            const int gx = static_cast<int>(grenadePoint.x);
            const int gy = static_cast<int>(grenadePoint.y);
            const int radius = static_cast<int>(projectiles[i].radius);
            draw::glowCircle(renderer, gx, gy, radius + 5, GRENADE_GREEN, qualityGlow(18));
            draw::circle(renderer, gx, gy, radius + 5, WHITE);
            for (int spoke = 0; spoke < 4; ++spoke)
            {
                const float angle = projectiles[i].angle + spoke * PI * 0.5f;
                draw::line(renderer, gx, gy,
                           gx + static_cast<int>(std::cos(angle) * (radius + 8)),
                           gy + static_cast<int>(std::sin(angle) * (radius + 8)),
                           GRENADE_GREEN, 3);
            }
        }
        else if (projectiles[i].kind == ProjectileFireShard)
        {
            const Vec2 fire = toScreen(viewport, cameraPlayer, projectiles[i].x, projectiles[i].y);
            const int fx = static_cast<int>(fire.x);
            const int fy = static_cast<int>(fire.y);
            draw::glowCircle(renderer, fx, fy, 8, ORANGE, qualityGlow(12));
            draw::triangle(renderer, fx + static_cast<int>(std::cos(projectiles[i].angle) * 13.0f), fy + static_cast<int>(std::sin(projectiles[i].angle) * 13.0f),
                           fx + static_cast<int>(std::cos(projectiles[i].angle + 2.5f) * 7.0f), fy + static_cast<int>(std::sin(projectiles[i].angle + 2.5f) * 7.0f),
                           fx + static_cast<int>(std::cos(projectiles[i].angle - 2.5f) * 7.0f), fy + static_cast<int>(std::sin(projectiles[i].angle - 2.5f) * 7.0f), ORANGE);
        }
        else renderWorldEntityCircle(viewport, cameraPlayer, projectiles[i].x, projectiles[i].y, projectiles[i].radius, projectiles[i].color, 8);
    }
    for (unsigned i = 0; i < enemyProjectiles.size(); ++i)
    {
        const Projectile& projectile = enemyProjectiles[i];
        const Vec2 projectilePoint = toScreen(viewport, cameraPlayer, projectile.x, projectile.y);
        const int px = static_cast<int>(projectilePoint.x);
        const int py = static_cast<int>(projectilePoint.y);
        const int pr = static_cast<int>(projectile.radius);
        if (px < viewport.x - pr - 30 || px > viewport.x + viewport.w + pr + 30 || py < viewport.y - pr - 30 || py > viewport.y + viewport.h + pr + 30) continue;
        if (projectile.kind == ProjectileX)
        {
            const int dx = static_cast<int>(std::cos(projectile.angle) * pr);
            const int dy = static_cast<int>(std::sin(projectile.angle) * pr);
            draw::line(renderer, px - dx - dy, py - dy + dx, px + dx + dy, py + dy - dx, withAlpha(projectile.color, 65), 8);
            draw::line(renderer, px - dx + dy, py - dy - dx, px + dx - dy, py + dy + dx, withAlpha(projectile.color, 65), 8);
            draw::line(renderer, px - dx - dy, py - dy + dx, px + dx + dy, py + dy - dx, projectile.color, 4);
            draw::line(renderer, px - dx + dy, py - dy - dx, px + dx - dy, py + dy + dx, projectile.color, 4);
        }
        else if (projectile.kind == ProjectileSquare)
        {
            int sx[4], sy[4];
            for (int corner = 0; corner < 4; ++corner)
            {
                const float angle = projectile.angle + PI * 0.25f + corner * PI * 0.5f;
                sx[corner] = px + static_cast<int>(std::cos(angle) * pr * 1.35f);
                sy[corner] = py + static_cast<int>(std::sin(angle) * pr * 1.35f);
            }
            draw::triangle(renderer, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], withAlpha(projectile.color, 190));
            draw::triangle(renderer, sx[0], sy[0], sx[2], sy[2], sx[3], sy[3], withAlpha(projectile.color, 190));
            for (int corner = 0; corner < 4; ++corner) draw::line(renderer, sx[corner], sy[corner], sx[(corner + 1) % 4], sy[(corner + 1) % 4], WHITE, 2);
        }
        else if (projectile.kind == ProjectileTriangle || projectile.kind == ProjectileArrow)
        {
            const float angle = projectile.angle;
            const int tipX = px + static_cast<int>(std::cos(angle) * pr * 1.8f);
            const int tipY = py + static_cast<int>(std::sin(angle) * pr * 1.8f);
            const int leftX = px + static_cast<int>(std::cos(angle + 2.45f) * pr);
            const int leftY = py + static_cast<int>(std::sin(angle + 2.45f) * pr);
            const int rightX = px + static_cast<int>(std::cos(angle - 2.45f) * pr);
            const int rightY = py + static_cast<int>(std::sin(angle - 2.45f) * pr);
            draw::triangle(renderer, tipX, tipY, leftX, leftY, rightX, rightY, projectile.color);
            if (projectile.kind == ProjectileArrow)
                draw::line(renderer, px - static_cast<int>(std::cos(angle) * pr * 1.8f), py - static_cast<int>(std::sin(angle) * pr * 1.8f), px, py, projectile.color, 5);
        }
        else renderWorldEntityCircle(viewport, cameraPlayer, projectile.x, projectile.y, projectile.radius, projectile.color, projectile.kind == ProjectileDebris ? 14 : 8);
    }
    for (unsigned i = 0; i < enemies.size(); ++i) renderEnemy(viewport, cameraPlayer, enemies[i]);
    for (int owner = 0; owner < playerCount; ++owner)
    {
        for (int slot = 0; slot < 3; ++slot)
        {
            const FriendlyBot& bot = friendlyBots[owner][slot];
            if (!bot.active) continue;
            const Vec2 point = toScreen(viewport, cameraPlayer, bot.x, bot.y);
            const int bx = static_cast<int>(point.x);
            const int by = static_cast<int>(point.y);
            const int radius = static_cast<int>(bot.radius);
            draw::glowCircle(renderer, bx, by, radius, PURPLE, qualityGlow(8));
            draw::circle(renderer, bx, by, radius + 5, players[owner].color);
            draw::text(renderer, "B" + number(slot + 1), bx, by - 5, 1, WHITE, true);
            draw::fillRect(renderer, bx - 20, by - radius - 14, 40, 5, {0, 0, 0, 220});
            draw::fillRect(renderer, bx - 20, by - radius - 14, static_cast<int>(40.0f * clampf(bot.hp / std::max(1.0f, bot.maxHp), 0.0f, 1.0f)), 5, GREEN);
        }
    }
    for (unsigned i = 0; i < souls.size(); ++i)
    {
        const Vec2 soul = toScreen(viewport, cameraPlayer, souls[i].x, souls[i].y);
        const int sx = static_cast<int>(soul.x);
        const int sy = static_cast<int>(soul.y);
        draw::fillRect(renderer, sx - 8, sy - 8, 16, 16, WHITE);
        draw::fillRect(renderer, sx - 4, sy - 2, 3, 4, BG);
        draw::fillRect(renderer, sx + 2, sy - 2, 3, 4, BG);
    }
    for (unsigned i = 0; i < lifeStreams.size(); ++i)
    {
        const Vec2 stream = toScreen(viewport, cameraPlayer, lifeStreams[i].x, lifeStreams[i].y);
        draw::glowCircle(renderer, static_cast<int>(stream.x), static_cast<int>(stream.y), 7, PINK, qualityGlow(8));
    }
    renderBoss(viewport, cameraPlayer);
    for (int i = 0; i < playerCount; ++i) renderCharacterEffects(viewport, cameraPlayer, players[i], i);
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
    preparePlayingBackground();
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
        draw::line(renderer, halfW, 0, halfW, SCREEN_H, withAlpha(WHITE, 70), 3);
        draw::line(renderer, 0, halfH, SCREEN_W, halfH, withAlpha(WHITE, 70), 3);
    }

    draw::panel(renderer, SCREEN_W / 2 - 95, 12, 190, 42, CYAN, {0, 5, 20, 190});
    draw::text(renderer, "ONDA " + number(wave), SCREEN_W / 2, 25, 2, CYAN, true);

    if (boss.active)
    {
        const char* bossNames[BossCount] = {"NEXO X", "QUADRADO FRAGMENTADO", "TRIANGULO", "CIRCULO CINETICO", "DIRECIONAL", "TOUCHPAD TEMPORAL"};
        float ratio = 0.0f;
        float currentHp = 0.0f;
        float maximumHp = 1.0f;
        if (boss.phase == 1)
        {
            for (int i = 0; i < 3; ++i)
            {
                currentHp += boss.weapons[i].hp;
                maximumHp += boss.weapons[i].maxHp;
            }
            maximumHp -= 1.0f;
            ratio = maximumHp > 0.0f ? currentHp / maximumHp : 0.0f;
        }
        else if (boss.phase == 2 && boss.shape == BossSquare)
        {
            currentHp = 0.0f;
            maximumHp = 0.0f;
            for (int i = 0; i < 4; ++i) { currentHp += boss.parts[i].hp; maximumHp += boss.parts[i].maxHp; }
            ratio = maximumHp > 0.0f ? currentHp / maximumHp : 0.0f;
        }
        else if (boss.phase == 2) { currentHp = boss.hp; maximumHp = boss.maxHp; ratio = boss.hp / boss.maxHp; }
        else if (boss.shape == BossDpad && boss.memoryState < 4)
        {
            ratio = 1.0f - (boss.memoryRound - 1) / 4.0f;
            maximumHp = boss.maxRageHp;
            currentHp = maximumHp * ratio;
        }
        else { currentHp = boss.rageHp; maximumHp = boss.maxRageHp; ratio = boss.rageHp / boss.maxRageHp; }
        draw::fillRect(renderer, 600, 70, 720, 42, {0, 0, 0, 205});
        draw::outlineRect(renderer, 600, 70, 720, 42, RED, 2);
        draw::fillRect(renderer, 610, 82, static_cast<int>(700 * clampf(ratio, 0.0f, 1.0f)), 18, boss.phase == 3 ? RED : boss.trueColor);
        const std::string hpText = number(static_cast<int>(std::ceil(std::max(0.0f, currentHp)))) + "/" + number(static_cast<int>(std::ceil(maximumHp)));
        draw::text(renderer, hpText, SCREEN_W / 2, 84, 1, WHITE, true);
        draw::text(renderer, std::string(bossNames[boss.shape]) + "  -  FASE " + number(boss.phase), SCREEN_W / 2, 119, 2, RED, true);

        if (boss.shape == BossDpad && boss.phase == 3 && boss.memoryState < 4)
        {
            draw::panel(renderer, 575, 150, 770, 72, boss.memoryState == 3 ? GOLD : CYAN, {0, 4, 18, 225});
            const std::string memoryStatus = "MEMORIA " + number(boss.memoryRound) + "/4   CHANCES " + number(boss.memoryLives) + "/3   TEMPO " + number(static_cast<int>(std::ceil(boss.memoryTimer))) + "S";
            draw::text(renderer, memoryStatus, SCREEN_W / 2, 171, 2, boss.memoryState == 3 ? GOLD : WHITE, true);
            draw::text(renderer, boss.memoryState == 1 ? "OBSERVE AS 8 DIRECOES" : (boss.memoryState == 2 ? "REPITA NO D-PAD" : (boss.memoryState == 3 ? "ESCOLHA UMA DIRECAO PARA ARRANCAR" : "SINCRONIZANDO...")), SCREEN_W / 2, 198, 1, MUTED, true);
        }
        if (boss.shape == BossTouchpad)
        {
            draw::panel(renderer, 650, 150, 620, 54, GOLD, {0, 4, 18, 215});
            std::string touchStatus = "TOUCHPAD: TOQUE E ARRASTE PARA MOVER";
            if (boss.shootSuppressed) touchStatus = "TIROS BLOQUEADOS: " + number(static_cast<int>(std::ceil(boss.stateTimer))) + "S";
            else if (boss.freezeActive) touchStatus = "TEMPO BLOQUEADO: USE SUA HABILIDADE";
            draw::text(renderer, touchStatus, SCREEN_W / 2, 169, 2, boss.freezeActive ? RED : GOLD, true);
        }
        if (boss.shape == BossCircle && boss.phase == 3 && boss.state == 0)
            draw::text(renderer, "IMPACTO IMINENTE - PARE O TEMPO!", SCREEN_W / 2, 162, 3, RED, true);
    }

    if (intermissionActive)
    {
        int ready = 0;
        const float portalX = (mapMinX + mapMaxX) * 0.5f;
        const float portalY = mapMinY + 105.0f;
        for (int i = 0; i < playerCount; ++i)
            if (players[i].hp > 0.0f && std::abs(players[i].x - portalX) <= 145.0f && std::abs(players[i].y - portalY) <= 62.0f) ++ready;
        draw::panel(renderer, 575, 68, 770, 96, GOLD, {0, 5, 18, 225});
        draw::text(renderer, "INTERVALO DO MAQUINISTA", SCREEN_W / 2, 87, 2, GOLD, true);
        draw::text(renderer, "UPGRADES -30%   GRANADA 8G", SCREEN_W / 2, 118, 1, WHITE, true);
        draw::text(renderer, "PORTAL: " + number(ready) + "/" + number(playerCount) + " JOGADORES", SCREEN_W / 2, 140, 1, ready == playerCount ? GREEN : CYAN, true);
    }

    if (announcementTimer > 0.0f)
    {
        const bool bossWave = isBossWaveNumber(wave);
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
        case Screen::Stats: renderStats(); break;
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

int Game::targetFps() const
{
    return impl_->targetFps();
}
}
