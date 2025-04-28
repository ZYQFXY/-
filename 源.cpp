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
#include <Windows.h>  // Windows API���ڿ���̨����

using namespace std::chrono_literals; // ����ʹ��ʱ��������������100ms

// ========================
// ������壨ECS�ܹ��е������
// ========================

// λ��������洢ʵ�������
struct Position { int x, y; };

// ����Ⱦ������洢��ʾ���ź���ɫ
struct Renderable { char symbol; WORD color; };

// ����������洢��ɫ������ֵ��������������
struct Stats { int hp, max_hp, attack, defense; };

// �����������ǳ���ʵ��
struct Exit { bool reached = false; };

// ========================
// ʵ���ࣨECS�ܹ��е�ʵ�壩
// ========================
class Entity {
public:
    // ��������ʵ��
    template<typename T>
    void addComponent(T component) {
        components[typeid(T)] = std::make_shared<T>(component);
    }

    // ��ʵ���ȡ���
    template<typename T>
    std::shared_ptr<T> getComponent() {
        auto it = components.find(typeid(T));
        return (it != components.end()) ?
            std::static_pointer_cast<T>(it->second) : nullptr;
    }

private:
    // ʹ�����������洢������������Ͳ���������
    std::unordered_map<std::type_index, std::shared_ptr<void>> components;
};

// ========================
// ��Ϸϵͳ���ࣨECS�ܹ��е�ϵͳ��
// ========================
class GameSystem {
public:
    virtual void update(std::vector<std::shared_ptr<Entity>>& entities) = 0;
};

// ========================
// ��ͼ��������ʹ��DFS������Ч�Թ���
// ========================
class MapGenerator {
public:
    // ����ָ���ߴ�ĵ��ε�ͼ�������س���λ��
    static std::vector<std::vector<int>> generateDungeon(int width, int height, Position& exitPos) {
        std::vector<std::vector<int>> map(height, std::vector<int>(width, 1)); // ��ʼ��Ϊȫǽ
        std::mt19937 rng(std::random_device{}());

        // �������������䣨�����ʼ����
        const int mainRoomSize = 8;
        createRoom(map, width / 2 - mainRoomSize / 2, height / 2 - mainRoomSize / 2, mainRoomSize, mainRoomSize);

        // ���ɳ��ڷ��䣨λ�ڵ�ͼ��Ե��
        exitPos = generateExitRoom(map, width, height, rng);

        // ���������������
        for (int i = 0; i < 4; ++i) {
            int roomW = std::uniform_int_distribution<int>(4, 8)(rng);
            int roomH = std::uniform_int_distribution<int>(4, 8)(rng);
            int x = std::uniform_int_distribution<int>(1, width - roomW - 1)(rng);
            int y = std::uniform_int_distribution<int>(1, height - roomH - 1)(rng);
            createRoom(map, x, y, roomW, roomH);
        }

        // ��֤·����Ч�ԣ�ȷ����ҿ��Ե�����ڣ�
        Position start{ width / 2, height / 2 };
        if (!isPathValid(map, start, exitPos)) {
            return generateDungeon(width, height, exitPos); // ��Ч����������
        }

        return map;
    }

private:
    // ��ָ��λ�ô������䣨��ǽ��Ϊ�ذ壩
    static void createRoom(std::vector<std::vector<int>>& map, int x, int y, int w, int h) {
        for (int dy = y; dy < y + h; ++dy)
            for (int dx = x; dx < x + w; ++dx)
                if (dx < map[0].size() && dy < map.size())
                    map[dy][dx] = 0;
    }

    // ���ɳ��ڷ��䣨λ�ڵ�ͼ�ı�֮һ��
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

    // ������Ե���䲢��������λ��
    static Position createEdgeRoom(std::vector<std::vector<int>>& map,
        int x, int y, int w, int h) {
        createRoom(map, x, y, w, h);
        return { x + w / 2, y + h / 2 };
    }

    // �������������֤·����Ч��
    static bool isPathValid(const std::vector<std::vector<int>>& map,
        Position start, Position end) {
        std::vector<std::vector<bool>> visited(map.size(),
            std::vector<bool>(map[0].size(), false));
        return dfs(map, visited, start.x, start.y, end.x, end.y);
    }

    // DFS�ݹ�ʵ��
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
// ս��ϵͳ�������ս������
// ========================
class CombatSystem : public GameSystem {
public:
    void update(std::vector<std::shared_ptr<Entity>>& entities) override {
        auto player = entities[0];
        auto playerPos = player->getComponent<Position>();
        auto playerStats = player->getComponent<Stats>();

        if (!playerPos || !playerStats) return;

        // ��������ʵ��Ѱ�ҵ���
        for (auto& e : entities) {
            if (e == player) continue;

            auto enemyPos = e->getComponent<Position>();
            auto enemyStats = e->getComponent<Stats>();

            if (!enemyPos || !enemyStats) continue;

            // �ķ������ڼ�⣨�ϸ�ˮƽ/��ֱ���ڣ�
            int dx = std::abs(playerPos->x - enemyPos->x);
            int dy = std::abs(playerPos->y - enemyPos->y);
            if ((dx <= 1 && dy == 0) || (dx == 0 && dy <= 1)) {
                // �����˺��������� - ���������������1���˺���
                int damage = std::max<int>(1, enemyStats->attack - playerStats->defense);
                playerStats->hp = std::max<int>(0, playerStats->hp - damage);

                // ʵʱ��ʾ�˺���ֵ��ʹ��Windows API��
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                COORD pos = { static_cast<SHORT>(playerPos->x),
                             static_cast<SHORT>(playerPos->y) };
                SetConsoleCursorPosition(hConsole, pos);
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
                std::cout << damage;
                std::this_thread::sleep_for(50ms); // ������ʾ�˺�����
            }
        }
    }
};

// ========================
// ��Ϸ���棨�������ࣩ
// ========================
class GameEngine {
public:
    GameEngine(int w, int h) : width(w), height(h),
        map(MapGenerator::generateDungeon(w, h, exitPos)),  // ���ɵ�ͼ
        startTime(std::chrono::steady_clock::now())         // ��¼��ʼʱ��
    {
        initEntities();  // ��ʼ����Ϸʵ��
        systems.emplace_back(std::make_unique<CombatSystem>()); // ע��ս��ϵͳ
    }

    void run() {
        while (running) {
            processInput();  // �����������
            update();        // ������Ϸ״̬
            render();        // ��Ⱦ��Ϸ����
            std::this_thread::sleep_for(100ms); // ������Ϸ�ٶ�
        }
    }

private:
    // ��Ϸ���ò���
    int width, height;          // ��ͼ�ߴ�
    Position exitPos;           // ����λ��
    std::vector<std::vector<int>> map; // ��ά��ͼ���ݣ�0=�ذ壬1=ǽ��
    std::vector<std::shared_ptr<Entity>> entities; // ����ʵ��
    std::vector<std::unique_ptr<GameSystem>> systems; // ��Ϸϵͳ
    bool running = true;        // ��Ϸ���б�־
    std::chrono::steady_clock::time_point startTime; // ��Ϸ��ʼʱ��

    // ��ʼ����Ϸʵ�壨��ҡ����ˡ����ڣ�
    void initEntities() {
        // ��ҳ�ʼλ�ã���ͼ���ģ�
        Position playerStart{ width / 2, height / 2 };

        // �������ʵ��
        auto player = std::make_shared<Entity>();
        player->addComponent(playerStart);
        player->addComponent(Renderable{ '@', FOREGROUND_RED | FOREGROUND_INTENSITY });
        player->addComponent(Stats{ 20, 20, 5, 1 }); // HP����������������
        entities.push_back(player);

        // ��������ʵ��
        auto exitEntity = std::make_shared<Entity>();
        exitEntity->addComponent(exitPos);
        exitEntity->addComponent(Renderable{ 'E', FOREGROUND_BLUE | FOREGROUND_INTENSITY });
        exitEntity->addComponent(Exit{});
        entities.push_back(exitEntity);

        // ս���Ե����ɵ��ˣ�ȷ���ڹؼ�·��������
        generateStrategicEnemies(playerStart);
    }

    // ���ɲ����Է��õĵ��ˣ��ڹؼ�·����Χ��չ����
    void generateStrategicEnemies(const Position& playerStart) {
        // ��ȡ��չ��Ĺؼ�·������
        std::vector<Position> extendedPath = getExtendedCriticalPath(3); // ��չ3��Χ

        std::mt19937 rng(std::random_device{}());
        std::vector<Position> validPositions;

        // �ռ���Ч����λ�ã��ų����ں���Ҹ�����
        for (const auto& pos : extendedPath) {
            if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) continue;

            bool valid = true;
            valid &= !(pos.x == exitPos.x && pos.y == exitPos.y); // ���ڳ���
            valid &= !(std::abs(pos.x - playerStart.x) <= 2 &&    // ���������3��
                std::abs(pos.y - playerStart.y) <= 2);
            valid &= (map[pos.y][pos.x] == 0);                    // ��ͨ������

            if (valid) validPositions.push_back(pos);
        }

        // ȥ�ز����������Чλ��
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

        // ���ɵ���ʵ�壨����4����
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

        // �������ɣ������Чλ�ò��㣩
        if (enemiesToSpawn > 0) {
            generateFallbackEnemies(enemiesToSpawn);
        }
    }

    // ��ȡ��չ��Ĺؼ�·���������ڵ������ɣ�
    std::vector<Position> getExtendedCriticalPath(int radius) {
        std::vector<Position> basePath = findCriticalPath();
        std::vector<Position> extendedPath;

        // ��·�������Χ������չ����
        for (const auto& center : basePath) {
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    if (dx == 0 && dy == 0) continue; // �������ĵ�
                    extendedPath.push_back({ center.x + dx, center.y + dy });
                }
            }
        }

        // �ϲ�ԭʼ·��
        extendedPath.insert(extendedPath.end(), basePath.begin(), basePath.end());
        return extendedPath;
    }

    // ���׵������ɣ�ɨ��ȫͼѰ����Чλ�ã�
    void generateFallbackEnemies(int required) {
        std::vector<Position> allValid;

        // ɨ��ȫͼѰ����Чλ��
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

        // ���ɵ���ʵ��
        for (int i = 0; i < required && i < allValid.size(); ++i) {
            auto enemy = std::make_shared<Entity>();
            enemy->addComponent(allValid[i]);
            enemy->addComponent(Renderable{ 'G', FOREGROUND_GREEN });
            enemy->addComponent(Stats{ 10, 10, 3, 1 });
            entities.push_back(enemy);
        }
    }

    // Ѱ����·���������ĵ����ڵ�ֱ��·����
    std::vector<Position> findCriticalPath() {
        std::vector<Position> path;
        Position start{ width / 2, height / 2 };

        // �����ƶ�����
        int dx = exitPos.x > start.x ? 1 : -1;
        int dy = exitPos.y > start.y ? 1 : -1;

        // ���ƶ�������λ��
        Position current = start;
        while (current.x != exitPos.x || current.y != exitPos.y) {
            if (current.x != exitPos.x) current.x += dx;
            if (current.y != exitPos.y) current.y += dy;
            path.push_back(current);
        }
        return path;
    }

    // ����������루ʹ��Windows���̼�⣩
    void processInput() {
        auto checkKey = [](int key) {
            return (GetAsyncKeyState(key) & 0x8000) != 0;
            };

        // ��������
        if (checkKey(VK_UP))    movePlayer(0, -1);
        if (checkKey(VK_DOWN))  movePlayer(0, 1);
        if (checkKey(VK_LEFT))  movePlayer(-1, 0);
        if (checkKey(VK_RIGHT)) movePlayer(1, 0);
        if (checkKey(VK_ESCAPE)) running = false; // ESC�˳�
    }

    // �ƶ���ң���ײ��⣩
    void movePlayer(int dx, int dy) {
        auto pos = entities[0]->getComponent<Position>();
        int newX = pos->x + dx;
        int newY = pos->y + dy;

        // �߽����ǽ����ײ���
        if (newX >= 0 && newX < width && newY >= 0 && newY < height)
            if (map[newY][newX] == 0) { // 0��ʾ��ͨ�еĵذ�
                pos->x = newX;
                pos->y = newY;
            }
    }

    // ������Ϸ״̬
    void update() {
        // ���ʱ�����ƣ�10�룩
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (elapsed >= 10) {
            endGame("Time's up");
            return;
        }

        // ����Ƿ񵽴����
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

        // ����������Ϸϵͳ
        for (auto& system : systems)
            system->update(entities);

        // ����������
        if (auto stats = entities[0]->getComponent<Stats>()) {
            if (stats->hp <= 0) {
                endGame("Game Over");
            }
        }
    }

    // ������Ϸ����ʾ��Ϣ
    void endGame(const std::string& message) {
        system("cls"); // ����
        SetConsoleOutputCP(65001); // ���ÿ���̨����֧������

        // ������ʾ��Ϣ
        const int consoleWidth = 80;
        std::string border(consoleWidth, '=');
        int padding = (consoleWidth - message.length() * 2) / 2;

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD pos = { 0, 10 };
        SetConsoleCursorPosition(hConsole, pos);

        // ����߿����Ϣ
        std::cout << "\t" << border << "\n";
        std::cout << "\t" << std::string(padding, ' ') << message << "\n";
        std::cout << "\t" << border << "\n\n";

        // ���ִ��ڴ�
        while (true) {
            std::this_thread::sleep_for(1000ms);
        }
    }

    // ��Ⱦ��Ϸ����
    // ��GameEngine���render�������޸ĵ�ͼ��ʵ����Ⱦ�߼�
    void render() {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hConsole, &csbi);

        // ��ȡ���λ��
        auto playerPos = entities[0]->getComponent<Position>();
        if (!playerPos) return;
        int px = playerPos->x;
        int py = playerPos->y;

        // ���Ƶ�ͼ������Ұ���ƣ�
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
                SetConsoleCursorPosition(hConsole, coord);

                // ��������ҵľ���ƽ��
                int dx = x - px;
                int dy = y - py;
                int distanceSq = dx * dx + dy * dy;

                // ֻ��ʾ�뾶4���ڵ�����4^2=16��
                if (distanceSq <= 16) {
                    std::cout << (map[y][x] ? '#' : ' ');
                }
                else {
                    std::cout << ' ';  // ���ɼ�������ʾ�հ�
                }
            }
        }

        // ����ʵ�壨����Ұ���ƣ�
        for (auto& e : entities) {
            auto pos = e->getComponent<Position>();
            auto ren = e->getComponent<Renderable>();
            if (!pos || !ren) continue;

            // ��������ҵľ���
            int dx = pos->x - px;
            int dy = pos->y - py;
            if (dx * dx + dy * dy > 16) continue;  // ������Ұ����ʾ

            COORD coord = { static_cast<SHORT>(pos->x), static_cast<SHORT>(pos->y) };
            SetConsoleCursorPosition(hConsole, coord);
            SetConsoleTextAttribute(hConsole, ren->color);
            std::cout << ren->symbol;
        }

        // ״̬��ʾ���ֲ���...


        // ��ʾ״̬��Ϣ
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        COORD statusPos = { 0, static_cast<SHORT>(height) };
        SetConsoleCursorPosition(hConsole, statusPos);

        auto stats = entities[0]->getComponent<Stats>();
        int remaining = 10 - std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        remaining = std::max<int>(0, remaining);

        // ��ʽ��״̬��Ϣ
        printf("HP: %2d/%2d | time: %2ds | exit: [%2d,%2d] ",
            stats->hp, stats->max_hp, remaining, exitPos.x, exitPos.y);
    }
};

// ========================
// ��������������ڣ�
// ========================
int main() {
    SetConsoleOutputCP(65001); // ���ÿ���̨֧��UTF-8����
    system("mode con cols=80 lines=40"); // ���ÿ���̨���ڴ�С

    GameEngine game(40, 20); // ������Ϸ���棨��ͼ�ߴ�40x20��
    game.run();              // ������Ϸ��ѭ��
    system("pause");
    return 0;
}