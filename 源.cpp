#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <cmath>
#include <Windows.h>  // Windows API用于控制台操作

using namespace std::chrono_literals; // 允许使用时间字面量，例如100ms

// ========================
// 组件定义（ECS架构中的组件）
// ========================

// 位置组件：存储实体的坐标
struct Position { int x, y; };

// 可渲染组件：存储显示符号和颜色
struct Renderable { char symbol; WORD color; };

// 属性组件：存储角色的生命值、攻击力等属性
struct Stats { int hp, max_hp, attack, defense; };

// 出口组件：标记出口实体
struct Exit { bool reached = false; };

// ========================
// 实体类（ECS架构中的实体）
// ========================
class Entity {
public:
    // 添加组件到实体
    template<typename T>
    void addComponent(T component) {
        components[typeid(T)] = std::make_shared<T>(component);
    }

    // 从实体获取组件
    template<typename T>
    std::shared_ptr<T> getComponent() {
        auto it = components.find(typeid(T));
        return (it != components.end()) ?
            std::static_pointer_cast<T>(it->second) : nullptr;
    }

private:
    // 使用类型索引存储各种组件（类型擦除技术）
    std::unordered_map<std::type_index, std::shared_ptr<void>> components;
};

// ========================
// 游戏系统基类（ECS架构中的系统）
// ========================
class GameSystem {
public:
    virtual void update(std::vector<std::shared_ptr<Entity>>& entities) = 0;
};

// ========================
// 地图生成器（使用DFS生成有效迷宫）
// ========================
class MapGenerator {
public:
    // 生成指定尺寸的地牢地图，并返回出口位置
    static std::vector<std::vector<int>> generateDungeon(int width, int height, Position& exitPos) {
        std::vector<std::vector<int>> map(height, std::vector<int>(width, 1)); // 初始化为全墙
        std::mt19937 rng(std::random_device{}());

        // 生成中心主房间（玩家起始区域）
        const int mainRoomSize = 8;
        createRoom(map, width / 2 - mainRoomSize / 2, height / 2 - mainRoomSize / 2, mainRoomSize, mainRoomSize);

        // 生成出口房间（位于地图边缘）
        exitPos = generateExitRoom(map, width, height, rng);

        // 生成其他随机房间
        for (int i = 0; i < 4; ++i) {
            int roomW = std::uniform_int_distribution<int>(4, 8)(rng);
            int roomH = std::uniform_int_distribution<int>(4, 8)(rng);
            int x = std::uniform_int_distribution<int>(1, width - roomW - 1)(rng);
            int y = std::uniform_int_distribution<int>(1, height - roomH - 1)(rng);
            createRoom(map, x, y, roomW, roomH);
        }

        // 验证路径有效性（确保玩家可以到达出口）
        Position start{ width / 2, height / 2 };
        if (!isPathValid(map, start, exitPos)) {
            return generateDungeon(width, height, exitPos); // 无效则重新生成
        }

        return map;
    }

private:
    // 在指定位置创建房间（将墙变为地板）
    static void createRoom(std::vector<std::vector<int>>& map, int x, int y, int w, int h) {
        for (int dy = y; dy < y + h; ++dy)
            for (int dx = x; dx < x + w; ++dx)
                if (dx < map[0].size() && dy < map.size())
                    map[dy][dx] = 0;
    }

    // 生成出口房间（位于地图四边之一）
    static Position generateExitRoom(std::vector<std::vector<int>>& map,
        int width, int height, std::mt19937& rng) {
        const int exitSize = 3;
        std::uniform_int_distribution<int> edgeDist(0, 3);
        switch (edgeDist(rng)) {
        case 0: return createEdgeRoom(map, width / 2 - exitSize / 2, 1, exitSize, exitSize);
        case 1: return createEdgeRoom(map, width / 2 - exitSize / 2, height - exitSize - 1, exitSize, exitSize);
        case 2: return createEdgeRoom(map, 1, height / 2 - exitSize / 2, exitSize, exitSize);
        case 3: return createEdgeRoom(map, width - exitSize - 1, height / 2 - exitSize / 2, exitSize, exitSize);
        }
        return { 0,0 };
    }

    // 创建边缘房间并返回中心位置
    static Position createEdgeRoom(std::vector<std::vector<int>>& map,
        int x, int y, int w, int h) {
        createRoom(map, x, y, w, h);
        return { x + w / 2, y + h / 2 };
    }

    // 深度优先搜索验证路径有效性
    static bool isPathValid(const std::vector<std::vector<int>>& map,
        Position start, Position end) {
        std::vector<std::vector<bool>> visited(map.size(),
            std::vector<bool>(map[0].size(), false));
        return dfs(map, visited, start.x, start.y, end.x, end.y);
    }

    // DFS递归实现
    static bool dfs(const std::vector<std::vector<int>>& map,
        std::vector<std::vector<bool>>& visited,
        int x, int y, int targetX, int targetY) {
        if (x == targetX && y == targetY) return true;
        if (x < 0 || x >= map[0].size() || y < 0 || y >= map.size()) return false;
        if (map[y][x] != 0 || visited[y][x]) return false;

        visited[y][x] = true;
        return dfs(map, visited, x + 1, y, targetX, targetY) ||
            dfs(map, visited, x - 1, y, targetX, targetY) ||
            dfs(map, visited, x, y + 1, targetX, targetY) ||
            dfs(map, visited, x, y - 1, targetX, targetY);
    }
};

// ========================
// 战斗系统（处理近战攻击）
// ========================
class CombatSystem : public GameSystem {
public:
    void update(std::vector<std::shared_ptr<Entity>>& entities) override {
        auto player = entities[0];
        auto playerPos = player->getComponent<Position>();
        auto playerStats = player->getComponent<Stats>();

        if (!playerPos || !playerStats) return;

        // 遍历所有实体寻找敌人
        for (auto& e : entities) {
            if (e == player) continue;

            auto enemyPos = e->getComponent<Position>();
            auto enemyStats = e->getComponent<Stats>();

            if (!enemyPos || !enemyStats) continue;

            // 四方向相邻检测（严格水平/垂直相邻）
            int dx = std::abs(playerPos->x - enemyPos->x);
            int dy = std::abs(playerPos->y - enemyPos->y);
            if ((dx <= 1 && dy == 0) || (dx == 0 && dy <= 1)) {
                // 计算伤害：攻击力 - 防御力（至少造成1点伤害）
                int damage = std::max<int>(1, enemyStats->attack - playerStats->defense);
                playerStats->hp = std::max<int>(0, playerStats->hp - damage);

                // 实时显示伤害数值（使用Windows API）
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                COORD pos = { static_cast<SHORT>(playerPos->x),
                             static_cast<SHORT>(playerPos->y) };
                SetConsoleCursorPosition(hConsole, pos);
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
                std::cout << damage;
                std::this_thread::sleep_for(50ms); // 短暂显示伤害数字
            }
        }
    }
};

// ========================
// 游戏引擎（主控制类）
// ========================
class GameEngine {
public:
    GameEngine(int w, int h) : width(w), height(h),
        map(MapGenerator::generateDungeon(w, h, exitPos)),  // 生成地图
        startTime(std::chrono::steady_clock::now())         // 记录开始时间
    {
        initEntities();  // 初始化游戏实体
        systems.emplace_back(std::make_unique<CombatSystem>()); // 注册战斗系统
    }

    void run() {
        while (running) {
            processInput();  // 处理玩家输入
            update();        // 更新游戏状态
            render();        // 渲染游戏画面
            std::this_thread::sleep_for(100ms); // 控制游戏速度
        }
    }

private:
    // 游戏配置参数
    int width, height;          // 地图尺寸
    Position exitPos;           // 出口位置
    std::vector<std::vector<int>> map; // 二维地图数据（0=地板，1=墙）
    std::vector<std::shared_ptr<Entity>> entities; // 所有实体
    std::vector<std::unique_ptr<GameSystem>> systems; // 游戏系统
    bool running = true;        // 游戏运行标志
    std::chrono::steady_clock::time_point startTime; // 游戏开始时间

    // 初始化游戏实体（玩家、敌人、出口）
    void initEntities() {
        // 玩家初始位置（地图中心）
        Position playerStart{ width / 2, height / 2 };

        // 创建玩家实体
        auto player = std::make_shared<Entity>();
        player->addComponent(playerStart);
        player->addComponent(Renderable{ '@', FOREGROUND_RED | FOREGROUND_INTENSITY });
        player->addComponent(Stats{ 20, 20, 5, 1 }); // HP，攻击力，防御力
        entities.push_back(player);

        // 创建出口实体
        auto exitEntity = std::make_shared<Entity>();
        exitEntity->addComponent(exitPos);
        exitEntity->addComponent(Renderable{ 'E', FOREGROUND_BLUE | FOREGROUND_INTENSITY });
        exitEntity->addComponent(Exit{});
        entities.push_back(exitEntity);

        // 战略性地生成敌人（确保在关键路径附近）
        generateStrategicEnemies(playerStart);
    }

    // 生成策略性放置的敌人（在关键路径周围扩展区域）
    void generateStrategicEnemies(const Position& playerStart) {
        // 获取扩展后的关键路径区域
        std::vector<Position> extendedPath = getExtendedCriticalPath(3); // 扩展3格范围

        std::mt19937 rng(std::random_device{}());
        std::vector<Position> validPositions;

        // 收集有效生成位置（排除出口和玩家附近）
        for (const auto& pos : extendedPath) {
            if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) continue;

            bool valid = true;
            valid &= !(pos.x == exitPos.x && pos.y == exitPos.y); // 不在出口
            valid &= !(std::abs(pos.x - playerStart.x) <= 2 &&    // 离玩家至少3格
                std::abs(pos.y - playerStart.y) <= 2);
            valid &= (map[pos.y][pos.x] == 0);                    // 可通行区域

            if (valid) validPositions.push_back(pos);
        }

        // 去重并随机排序有效位置
        std::sort(validPositions.begin(), validPositions.end(),
            [](const Position& a, const Position& b) {
                return a.x == b.x ? a.y < b.y : a.x < b.x;
            });
        auto last = std::unique(validPositions.begin(), validPositions.end(),
            [](const Position& a, const Position& b) {
                return a.x == b.x && a.y == b.y;
            });
        validPositions.erase(last, validPositions.end());
        std::shuffle(validPositions.begin(), validPositions.end(), rng);

        // 生成敌人实体（至少4个）
        int enemiesToSpawn = 4;
        for (const auto& pos : validPositions) {
            if (enemiesToSpawn <= 0) break;

            auto enemy = std::make_shared<Entity>();
            enemy->addComponent(pos);
            enemy->addComponent(Renderable{ 'G', FOREGROUND_GREEN });
            enemy->addComponent(Stats{ 10, 10, 3, 1 });
            entities.push_back(enemy);
            enemiesToSpawn--;
        }

        // 保底生成（如果有效位置不足）
        if (enemiesToSpawn > 0) {
            generateFallbackEnemies(enemiesToSpawn);
        }
    }

    // 获取扩展后的关键路径区域（用于敌人生成）
    std::vector<Position> getExtendedCriticalPath(int radius) {
        std::vector<Position> basePath = findCriticalPath();
        std::vector<Position> extendedPath;

        // 在路径点的周围生成扩展区域
        for (const auto& center : basePath) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (dx == 0 && dy == 0) continue; // 跳过中心点
                    extendedPath.push_back({ center.x + dx, center.y + dy });
                }
            }
        }

        // 合并原始路径
        extendedPath.insert(extendedPath.end(), basePath.begin(), basePath.end());
        return extendedPath;
    }

    // 保底敌人生成（扫描全图寻找有效位置）
    void generateFallbackEnemies(int required) {
        std::vector<Position> allValid;

        // 扫描全图寻找有效位置
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (map[y][x] == 0 &&
                    !(x == exitPos.x && y == exitPos.y)) {
                    allValid.emplace_back(Position{ x, y });
                }
            }
        }

        std::mt19937 rng(std::random_device{}());
        std::shuffle(allValid.begin(), allValid.end(), rng);

        // 生成敌人实体
        for (int i = 0; i < required && i < allValid.size(); ++i) {
            auto enemy = std::make_shared<Entity>();
            enemy->addComponent(allValid[i]);
            enemy->addComponent(Renderable{ 'G', FOREGROUND_GREEN });
            enemy->addComponent(Stats{ 10, 10, 3, 1 });
            entities.push_back(enemy);
        }
    }

    // 寻找主路径（从中心到出口的直线路径）
    std::vector<Position> findCriticalPath() {
        std::vector<Position> path;
        Position start{ width / 2, height / 2 };

        // 计算移动方向
        int dx = exitPos.x > start.x ? 1 : -1;
        int dy = exitPos.y > start.y ? 1 : -1;

        // 逐步移动到出口位置
        Position current = start;
        while (current.x != exitPos.x || current.y != exitPos.y) {
            if (current.x != exitPos.x) current.x += dx;
            if (current.y != exitPos.y) current.y += dy;
            path.push_back(current);
        }
        return path;
    }

    // 处理玩家输入（使用Windows键盘检测）
    void processInput() {
        auto checkKey = [](int key) {
            return (GetAsyncKeyState(key) & 0x8000) != 0;
            };

        // 方向键检测
        if (checkKey(VK_UP))    movePlayer(0, -1);
        if (checkKey(VK_DOWN))  movePlayer(0, 1);
        if (checkKey(VK_LEFT))  movePlayer(-1, 0);
        if (checkKey(VK_RIGHT)) movePlayer(1, 0);
        if (checkKey(VK_ESCAPE)) running = false; // ESC退出
    }

    // 移动玩家（碰撞检测）
    void movePlayer(int dx, int dy) {
        auto pos = entities[0]->getComponent<Position>();
        int newX = pos->x + dx;
        int newY = pos->y + dy;

        // 边界检查和墙壁碰撞检测
        if (newX >= 0 && newX < width && newY >= 0 && newY < height)
            if (map[newY][newX] == 0) { // 0表示可通行的地板
                pos->x = newX;
                pos->y = newY;
            }
    }

    // 更新游戏状态
    void update() {
        // 检查时间限制（10秒）
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (elapsed >= 10) {
            endGame("Time's up");
            return;
        }

        // 检查是否到达出口
        auto playerPos = entities[0]->getComponent<Position>();
        for (auto& e : entities) {
            if (auto exitComp = e->getComponent<Exit>()) {
                if (auto exitPosComp = e->getComponent<Position>()) {
                    if (playerPos->x == exitPosComp->x && playerPos->y == exitPosComp->y) {
                        endGame("Congratulations You win");
                        return;
                    }
                }
            }
        }

        // 更新所有游戏系统
        for (auto& system : systems)
            system->update(entities);

        // 检查玩家死亡
        if (auto stats = entities[0]->getComponent<Stats>()) {
            if (stats->hp <= 0) {
                endGame("Game Over");
            }
        }
    }

    // 结束游戏并显示信息
    void endGame(const std::string& message) {
        system("cls"); // 清屏
        SetConsoleOutputCP(65001); // 设置控制台编码支持中文

        // 居中显示信息
        const int consoleWidth = 80;
        std::string border(consoleWidth, '=');
        int padding = (consoleWidth - message.length() * 2) / 2;

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD pos = { 0, 10 };
        SetConsoleCursorPosition(hConsole, pos);

        // 输出边框和信息
        std::cout << "\t" << border << "\n";
        std::cout << "\t" << std::string(padding, ' ') << message << "\n";
        std::cout << "\t" << border << "\n\n";

        // 保持窗口打开
        while (true) {
            std::this_thread::sleep_for(1000ms);
        }
    }

    // 渲染游戏画面
    // 在GameEngine类的render函数中修改地图和实体渲染逻辑
    void render() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);

        // 获取玩家位置
        auto playerPos = entities[0]->getComponent<Position>();
        if (!playerPos) return;
        int px = playerPos->x;
        int py = playerPos->y;

        // 绘制地图（带视野限制）
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
                SetConsoleCursorPosition(hConsole, coord);

                // 计算与玩家的距离平方
                int dx = x - px;
                int dy = y - py;
                int distanceSq = dx * dx + dy * dy;

                // 只显示半径4格内的区域（4^2=16）
                if (distanceSq <= 16) {
                    std::cout << (map[y][x] ? '#' : ' ');
                }
                else {
                    std::cout << ' ';  // 不可见区域显示空白
                }
            }
        }

        // 绘制实体（带视野限制）
        for (auto& e : entities) {
            auto pos = e->getComponent<Position>();
            auto ren = e->getComponent<Renderable>();
            if (!pos || !ren) continue;

            // 计算与玩家的距离
            int dx = pos->x - px;
            int dy = pos->y - py;
            if (dx * dx + dy * dy > 16) continue;  // 超出视野不显示

            COORD coord = { static_cast<SHORT>(pos->x), static_cast<SHORT>(pos->y) };
            SetConsoleCursorPosition(hConsole, coord);
            SetConsoleTextAttribute(hConsole, ren->color);
            std::cout << ren->symbol;
        }

        // 状态显示保持不变...


        // 显示状态信息
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        COORD statusPos = { 0, static_cast<SHORT>(height) };
        SetConsoleCursorPosition(hConsole, statusPos);

        auto stats = entities[0]->getComponent<Stats>();
        int remaining = 10 - std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        remaining = std::max<int>(0, remaining);

        // 格式化状态信息
        printf("HP: %2d/%2d | time: %2ds | exit: [%2d,%2d] ",
            stats->hp, stats->max_hp, remaining, exitPos.x, exitPos.y);
    }
};

// ========================
// 主函数（程序入口）
// ========================
int main() {
    SetConsoleOutputCP(65001); // 设置控制台支持UTF-8编码
    system("mode con cols=80 lines=40"); // 设置控制台窗口大小

    GameEngine game(40, 20); // 创建游戏引擎（地图尺寸40x20）
    game.run();              // 启动游戏主循环
    system("pause");
    return 0;
}