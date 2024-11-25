#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <string>
#include <mutex>
#include <optional>

int uniqueId = 1;

struct Command
{
    std::string cmd;

    enum DataType
    {
        None,
        LoadData,
        UnloadData,
        SaveData,
        SaveSpriteData,
        EditSpriteData
    } dataType = DataType::None;

    union Data
    {
        struct LoadData
        {
            std::string fileName;
            LoadData() : fileName() {}
            LoadData(const std::string &name) : fileName(name) {}
            ~LoadData() {}
        } loadData;
        struct UnloadData
        {
            int width;
            int height;
            UnloadData() : width(0), height(0) {}
            UnloadData(int w, int h) : width(w), height(h) {}
            ~UnloadData() {}
        } unloadData;
        struct SaveSpriteData
        {
            std::string fileName;
            SaveSpriteData() : fileName() {}
            SaveSpriteData(const std::string &name) : fileName(name) {}
            ~SaveSpriteData() {}
        } saveSpriteData;
        struct EditSpriteData
        {
            bool del;
            float scaleX;
            float scaleY;
            int health;
            float direction;
            int z;
            std::string identifier;
            EditSpriteData() : del(), scaleX(), scaleY(), health(), direction(), identifier(), z(z) {}
            EditSpriteData(const bool &del, const float &scaleX, const float &scaleY, const int &health, const float &direction, std::string identifier, int z) : del(del), scaleX(scaleX), scaleY(scaleY), health(health), direction(direction), identifier(identifier), z(z) {}
            ~EditSpriteData() {}
        } editSpriteData;
        struct SaveData
        {
            std::string fileName;
            SaveData() : fileName() {}
            SaveData(const std::string &name) : fileName(name) {}
            ~SaveData() {}
        } saveData;
        Data() {}
        ~Data() {}
    };

    void setLoadData(const std::string &fileName)
    {
        clearData();
        new (&data->loadData) Data::LoadData(fileName);
        dataType = DataType::LoadData;
    }

    void setSaveData(const std::string &fileName)
    {
        clearData();
        new (&data->saveData) Data::SaveData(fileName);
        dataType = DataType::SaveData;
    }

    void setSaveSpriteData(const std::string &fileName)
    {
        clearData();
        new (&data->saveSpriteData) Data::SaveSpriteData(fileName);
        dataType = DataType::SaveData;
    }

    void setEditSpriteData(const bool &del, const float &scaleX, const float &scaleY, const int &health, const float &direction, std::string identifier, int z)
    {
        clearData();
        new (&data->editSpriteData) Data::EditSpriteData(del, scaleX, scaleY, health, direction, identifier, z);
        dataType = DataType::EditSpriteData;
    }

    void setUnloadData(int width, int height)
    {
        clearData();
        new (&data->unloadData) Data::UnloadData(width, height);
        dataType = DataType::UnloadData;
    }

    void clearData()
    {
        if (data)
        {
            if (dataType == DataType::LoadData)
                data->loadData.~LoadData();
            else if (dataType == DataType::UnloadData)
                data->unloadData.~UnloadData();
            else if (dataType == DataType::SaveData)
                data->saveData.~SaveData();
            else if (dataType == DataType::SaveSpriteData)
                data->saveSpriteData.~SaveSpriteData();
            else if (dataType == DataType::EditSpriteData)
                data->editSpriteData.~EditSpriteData();

            data.reset();
        }
        dataType = DataType::None;
    }

    ~Command()
    {
        clearData();
    }

    std::optional<Data> data;
};

enum SpriteType
{
    Key,
    Bomb,
    Enemy,
    ShooterEnemy,
    Bullet,
    EnemyBullet,
    Coin
};

void printSpriteType(SpriteType type)
{
    switch (type)
    {
    case Key:
        std::cout << "Key" << std::endl;
        break;
    case Bomb:
        std::cout << "Bomb" << std::endl;
        break;
    case Enemy:
        std::cout << "Enemy" << std::endl;
        break;
    case ShooterEnemy:
        std::cout << "ShooterEnemy" << std::endl;
        break;
    case Bullet:
        std::cout << "Bullet" << std::endl;
        break;
    case EnemyBullet:
        std::cout << "EnemyBullet" << std::endl;
        break;
    case Coin:
        std::cout << "Coin" << std::endl;
        break;
    default:
        std::cout << "Unknown SpriteType" << std::endl;
        break;
    }
}

struct Sprite
{
    std::string identifier;
    SpriteType type;
    float x, y, z;
    float scaleX = 1;
    float scaleY = 1;
    bool active;
    std::optional<float> direction;
    std::optional<float> health;
    std::optional<std::chrono::_V2::system_clock::time_point> enemyLastBulletTime;
};

int mapWidth;
int mapHeight;
int cellType = 1;
int selected = 0;
std::atomic<bool> running(true);
std::mutex commandMutex;
Command command;

std::vector<Sprite> sprites;
std::vector<SDL_Texture *> textures;
std::vector<SDL_Texture *> spriteTextures;
std::vector<std::string> texturePaths = {
    "./textures/texture-1.png",
    "./textures/texture-2.png",
    "./textures/texture-3.png",
    "./textures/texture-4.png",
    "./textures/texture-5.png",
    "./textures/texture-6.png",
    "./textures/safeDoor.png",
    "./textures/brickWall.png",
    "./textures/crackedBrickWall.png",
    "./textures/tiledFloor.png",
    "./textures/blueBrickWall.png",
    "./textures/crackedBlueBrickWall.png",
    "./textures/carpet.png",
    "./textures/tiledCeiling.png",
};
std::vector<std::string> spritePaths = {
    "./textures/key.png",
    "./textures/bomb.png",
    "./textures/enemy.png",
    "./textures/enemy.png",
    "./textures/bullet.png",
    "./textures/bullet.png",
    "./textures/coin.png",
};

void serialize(int mapWidth, int mapHeight, const std::vector<int> &map, const std::vector<int> &mapFloors, const std::vector<int> &mapCeiling, const std::string &filename)
{
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    if (file)
    {
        file.write(reinterpret_cast<const char *>(&mapWidth), sizeof(mapWidth));
        file.write(reinterpret_cast<const char *>(&mapHeight), sizeof(mapHeight));

        size_t count = map.size();
        file.write(reinterpret_cast<const char *>(&count), sizeof(count));
        file.write(reinterpret_cast<const char *>(map.data()), sizeof(int) * count);

        count = mapFloors.size();
        file.write(reinterpret_cast<const char *>(&count), sizeof(count));
        file.write(reinterpret_cast<const char *>(mapFloors.data()), sizeof(int) * count);

        count = mapCeiling.size();
        file.write(reinterpret_cast<const char *>(&count), sizeof(count));
        file.write(reinterpret_cast<const char *>(mapCeiling.data()), sizeof(int) * count);

        file.close();
    }
    else
    {
        std::cerr << "Error opening file for writing.\n";
    }
}

void deserialize(int *mapWidth, int *mapHeight, std::vector<int> *map, std::vector<int> *mapFloors, std::vector<int> *mapCeiling, const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::in);
    if (file)
    {
        file.read(reinterpret_cast<char *>(mapWidth), sizeof(int));
        file.read(reinterpret_cast<char *>(mapHeight), sizeof(int));

        size_t count;
        file.read(reinterpret_cast<char *>(&count), sizeof(count));
        map->resize(count);
        file.read(reinterpret_cast<char *>(map->data()), sizeof(int) * count);

        count;
        file.read(reinterpret_cast<char *>(&count), sizeof(count));
        mapFloors->resize(count);
        file.read(reinterpret_cast<char *>(mapFloors->data()), sizeof(int) * count);

        count;
        file.read(reinterpret_cast<char *>(&count), sizeof(count));
        mapCeiling->resize(count);
        file.read(reinterpret_cast<char *>(mapCeiling->data()), sizeof(int) * count);
        file.close();
    }
    else
    {
        std::cerr << "Error opening file for reading.\n";
    }
}

void serializeSprites(const std::string &filename)
{
    std::ofstream file(filename, std::ios::binary | std::ios::out);
    if (file)
    {
        int spritesSize = sprites.size();
        file.write(reinterpret_cast<const char *>(&spritesSize), sizeof(spritesSize));
        for (auto sprite : sprites)
        {
            int type = static_cast<int>(sprite.type);
            file.write(reinterpret_cast<const char *>(&type), sizeof(int));
            file.write(reinterpret_cast<const char *>(&sprite.x), sizeof(sprite.x));
            file.write(reinterpret_cast<const char *>(&sprite.y), sizeof(sprite.y));
            file.write(reinterpret_cast<const char *>(&sprite.z), sizeof(sprite.z));
            file.write(reinterpret_cast<const char *>(&sprite.scaleX), sizeof(sprite.scaleX));
            file.write(reinterpret_cast<const char *>(&sprite.scaleY), sizeof(sprite.scaleY));
            file.write(reinterpret_cast<const char *>(&sprite.active), sizeof(sprite.active));
            bool hasHealth = sprite.health.has_value();
            file.write(reinterpret_cast<const char *>(&hasHealth), sizeof(hasHealth));
            if (hasHealth)
            {
                file.write(reinterpret_cast<const char *>(&sprite.health.value()), sizeof(sprite.health.value()));
            }
            bool hasDirection = sprite.direction.has_value();
            file.write(reinterpret_cast<const char *>(&hasDirection), sizeof(hasDirection));
            if (hasDirection)
            {
                file.write(reinterpret_cast<const char *>(&sprite.direction.value()), sizeof(sprite.direction.value()));
            }
        }

        file.close();
    }
    else
    {
        std::cerr << "Error opening file for writing.\n";
    }
}

void deserializeSprites(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::in);
    if (file)
    {
        int spritesSize;
        file.read(reinterpret_cast<char *>(&spritesSize), sizeof(int));
        for (int i = 0; i < spritesSize; i++)
        {
            Sprite sprite;
            int type;
            file.read(reinterpret_cast<char *>(&type), sizeof(int));
            sprite.type = static_cast<SpriteType>(type);
            file.read(reinterpret_cast<char *>(&sprite.x), sizeof(float));
            file.read(reinterpret_cast<char *>(&sprite.y), sizeof(float));
            file.read(reinterpret_cast<char *>(&sprite.z), sizeof(float));
            file.read(reinterpret_cast<char *>(&sprite.scaleX), sizeof(float));
            file.read(reinterpret_cast<char *>(&sprite.scaleY), sizeof(float));
            file.read(reinterpret_cast<char *>(&sprite.active), sizeof(bool));
            bool hasHealth;
            file.read(reinterpret_cast<char *>(&hasHealth), sizeof(bool));
            if (hasHealth)
            {
                file.read(reinterpret_cast<char *>(&sprite.health), sizeof(float));
            }
            bool hasDirection;
            file.read(reinterpret_cast<char *>(&hasDirection), sizeof(bool));
            if (hasDirection)
            {
                file.read(reinterpret_cast<char *>(&sprite.direction), sizeof(bool));
            }
            sprites.emplace_back(sprite);
        }
        file.close();
    }
    else
    {
        std::cerr << "Error opening file for reading.\n";
    }
}

void consoleCommands()
{
    while (running)
    {
        std::string input;
        std::cin >> input;
        command.clearData();
        command.dataType = Command::DataType::None;
        if (input == "help")
        {
            std::cout << "Commands:" << std::endl;
            std::cout << "help - shows a list of commands" << std::endl;
            std::cout << "load - loads a map file" << std::endl;
            std::cout << "unload - creates a new map" << std::endl;
            std::cout << "save - saves the current map" << std::endl;
            std::cout << "editSprite - edits a sprite" << std::endl;
            std::cout << "saveSprites - saves the sprites" << std::endl;
        }
        if (input == "load" || "loadSprites")
        {
            std::string fileName;
            std::cout << "Enter a filename: ";
            std::cin >> fileName;

            command.setLoadData(fileName);
        }
        if (input == "save")
        {
            std::string fileName;
            std::cout << "Enter a filename: ";
            std::cin >> fileName;

            command.setSaveData(fileName);
        }

        if (input == "saveSprites")
        {
            std::string fileName;
            std::cout << "Enter a filename: ";
            std::cin >> fileName;

            command.setSaveSpriteData(fileName);
        }

        if (input == "unload")
        {
            int w, h;
            std::cout << "Enter the width of the map: ";
            std::cin >> w;
            std::cout << std::endl;
            std::cout << "Enter the height of the map: ";
            command.clearData();
            std::cin >> h;

            command.setUnloadData(w, h);
        }

        if (input == "editSprite")
        {
            std::string identifier;
            std::cout << "Which sprite do you want to edit: ";
            std::cin >> identifier;

            bool del = false;

            std::string res;
            std::cout << "Delete sprite (Y/N): ";
            std::cin >> res;

            if (res == "Y" || res == "y")
                del = true;

            float scaleX = 0;
            std::cout << "scaleX: ";
            std::cin >> scaleX;

            float scaleY = 0;
            std::cout << "scaleY: ";
            std::cin >> scaleY;

            float z = -1;
            std::cout << "z: ";
            std::cin >> z;

            int health = -1;
            std::cout << "health (-1 for none): ";
            std::cin >> health;

            float direction = -1;
            std::cout << "direction (-1 for none): ";
            std::cin >> direction;

            command.setEditSpriteData(del, scaleX, scaleY, health, direction, identifier, z);
        }

        std::lock_guard<std::mutex> lock(commandMutex);
        command.cmd = input;
    }
}

int main()
{
    std::vector<int> map;

    std::vector<int> mapFloors;

    std::vector<int> mapCeiling;

    std::string loadMap;
    std::cout << "Do you want to load a map (Y/N)";
    std::cin >> loadMap;
    if (loadMap == "Y" || loadMap == "y")
    {
        deserialize(&mapWidth, &mapHeight, &map, &mapFloors, &mapCeiling, "map.dat");
    }
    else
    {
        std::cout << "Enter the width of the map: ";
        std::cin >> mapWidth;
        std::cout << std::endl;
        std::cout << "Enter the height of the map: ";
        std::cin >> mapHeight;
        std::cout << std::endl;

        map.resize(mapWidth * mapHeight, 0);
        mapFloors.resize(mapWidth * mapHeight, 0);
        mapCeiling.resize(mapWidth * mapHeight, 0);

        std::fill(map.begin(), map.end(), 0);
        std::fill(mapFloors.begin(), mapFloors.end(), 0);
        std::fill(mapCeiling.begin(), mapCeiling.end(), 0);
    }

    std::vector<int> *currentMap = &map;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Raycaster map editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 700, SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        printf("IMG_Init Error: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    for (int i = 0; i < texturePaths.size(); i++)
    {
        printf("Loading texture: %s\n", texturePaths[i].c_str());

        SDL_Surface *surface = IMG_Load(texturePaths[i].c_str());
        if (!surface)
        {
            printf("IMG_Load Error for texture %d: %s\n", i, IMG_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture)
        {
            printf("SDL_CreateTextureFromSurface Error for texture %d: %s\n", i, SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        textures.emplace_back(texture);
    }
    for (int i = 0; i < spritePaths.size(); i++)
    {
        printf("Loading texture: %s\n", spritePaths[i].c_str());

        SDL_Surface *surface = IMG_Load(spritePaths[i].c_str());
        if (!surface)
        {
            printf("IMG_Load Error for texture %d: %s\n", i, IMG_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture)
        {
            printf("SDL_CreateTextureFromSurface Error for texture %d: %s\n", i, SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        spriteTextures.emplace_back(texture);
    }
    float width = 700 / mapWidth;
    float height = 700 / mapHeight;
    std::thread consoleThread(consoleCommands);
    while (running)
    {

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            if ((event.type == SDL_MOUSEMOTION && SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) || (event.type == SDL_MOUSEBUTTONDOWN && SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
            {
                int x = event.button.x;
                int y = event.button.y;
                int cellX = floor(x / width);
                int cellY = floor(y / height);
                if (cellX < mapWidth && cellY < mapHeight && cellX >= 0 && cellY >= 0)
                {
                    currentMap->at(cellX + cellY * mapWidth) = cellType;
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {

                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_p)
                {
                    if (selected == 0)
                    {
                        selected = 1;
                    }
                    else
                    {

                        selected = 0;
                    }
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN && SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT))
            {
                if (cellType == 0)
                {
                    int x = event.button.x;
                    int y = event.button.y;

                    for (int i = sprites.size() - 1; i >= 0; --i)
                    {
                        float dx = x - ((sprites[i].x / 64) * width);
                        float dy = y - ((sprites[i].y / 64) * height);
                        float distance = sqrt(dx * dx + dy * dy);
                        if (distance < 10)
                        {
                            sprites.erase(sprites.begin() + i);
                        }
                    }
                }
                else
                {
                    int x = event.button.x;
                    int y = event.button.y;
                    Sprite sprite;
                    sprite.type = static_cast<SpriteType>(cellType);
                    sprite.active = true;
                    sprite.x = (x / width) * 64;
                    sprite.y = (y / height) * 64;
                    sprite.z = 0;
                    sprite.identifier = std::to_string(uniqueId++);
                    sprites.emplace_back(sprite);
                }
            }
        }

        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
        if (selected == 0)
        {
            if (keystate[SDL_SCANCODE_0])
            {
                cellType = 0;
            }
            if (keystate[SDL_SCANCODE_1])
            {
                cellType = 1;
            }
            if (keystate[SDL_SCANCODE_2])
            {
                cellType = 2;
            }
            if (keystate[SDL_SCANCODE_3])
            {
                cellType = 3;
            }
            if (keystate[SDL_SCANCODE_4])
            {
                cellType = 4;
            }
            if (keystate[SDL_SCANCODE_5])
            {
                cellType = 5;
            }
            if (keystate[SDL_SCANCODE_6])
            {
                cellType = 6;
            }
            if (keystate[SDL_SCANCODE_7])
            {
                cellType = 7;
            }
            if (keystate[SDL_SCANCODE_8])
            {
                cellType = 8;
            }
            if (keystate[SDL_SCANCODE_9])
            {
                cellType = 9;
            }
        }
        else
        {
            if (keystate[SDL_SCANCODE_0])
            {
                cellType = 0;
            }
            if (keystate[SDL_SCANCODE_1])
            {
                cellType = 10;
            }
            if (keystate[SDL_SCANCODE_2])
            {
                cellType = 11;
            }
            if (keystate[SDL_SCANCODE_3])
            {
                cellType = 12;
            }
            if (keystate[SDL_SCANCODE_4])
            {
                cellType = 13;
            }
            if (keystate[SDL_SCANCODE_5])
            {
                cellType = 14;
            }
        }
        if (keystate[SDL_SCANCODE_Q])
        {
            currentMap = &map;
        }
        if (keystate[SDL_SCANCODE_W])
        {
            currentMap = &mapFloors;
        }
        if (keystate[SDL_SCANCODE_E])
        {
            currentMap = &mapCeiling;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int x = 0; x < mapWidth; x++)
        {
            for (int y = 0; y < mapHeight; y++)
            {
                SDL_Rect square;
                square.w = width - 1;
                square.h = height - 1;
                square.x = x * width + 1;
                square.y = y * height + 1;
                if (currentMap->at(x + y * mapWidth) - 1 >= 0)
                {
                    SDL_RenderCopy(renderer, textures[currentMap->at(x + y * mapWidth) - 1], NULL, &square);
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
                }

                SDL_RenderDrawRect(renderer, &square);
            }
        }

        for (int i = 0; i < sprites.size(); i++)
        {
            const Sprite &sprite = sprites.at(i);
            SDL_Rect square;
            square.w = 10;
            square.h = 10;
            square.x = (sprite.x / 64) * width;
            square.y = (sprite.y / 64) * height;

            SDL_RenderCopy(renderer, spriteTextures[sprite.type - 1], NULL, &square);
        }

        SDL_RenderPresent(renderer);

        {
            std::lock_guard<std::mutex> lock(commandMutex);
            if (!command.cmd.empty())
            {
                std::cout << "Command received: " << command.cmd << std::endl;
                if (command.cmd == "quit")
                {
                    running = false;
                }
                else if (command.cmd == "save")
                {
                    serialize(mapWidth, mapHeight, map, mapFloors, mapCeiling, command.data->saveData.fileName);
                }
                else if (command.cmd == "load")
                {
                    deserialize(&mapWidth, &mapHeight, &map, &mapFloors, &mapCeiling, command.data->loadData.fileName);
                    width = 700 / mapWidth;
                    height = 700 / mapHeight;
                }
                else if (command.cmd == "unload")
                {
                    mapWidth = command.data->unloadData.width;
                    mapHeight = command.data->unloadData.height;

                    map.clear();
                    mapFloors.clear();
                    mapCeiling.clear();

                    map.resize(mapWidth * mapHeight, 0);
                    mapFloors.resize(mapWidth * mapHeight, 0);
                    mapCeiling.resize(mapWidth * mapHeight, 0);

                    std::fill(map.begin(), map.end(), 0);
                    std::fill(mapFloors.begin(), mapFloors.end(), 0);
                    std::fill(mapCeiling.begin(), mapCeiling.end(), 0);
                    width = 700 / mapWidth;
                    height = 700 / mapHeight;
                }
                else if (command.cmd == "editSprite")
                {
                    std::string ident = command.data->editSpriteData.identifier;
                    std::unique_ptr<Sprite> sprite = nullptr;
                    int at = -1;
                    for (int i = 0; i < sprites.size(); i++)
                    {
                        if (sprites.at(i).identifier == ident)
                        {
                            sprite = std::make_unique<Sprite>(sprites.at(i));
                            at = i;
                        }
                    }
                    if (at != -1)
                    {
                        if (command.data->editSpriteData.del)
                        {
                            sprites.erase(sprites.begin() + at);
                        }
                        else
                        {
                            sprites.at(at).scaleX = command.data->editSpriteData.scaleX;
                            sprites.at(at).scaleY = command.data->editSpriteData.scaleY;
                            sprites.at(at).z = command.data->editSpriteData.z;
                            float dir = command.data->editSpriteData.direction;
                            if (dir != -1)
                            {
                                sprites.at(at).direction = dir;
                            }
                            float health = command.data->editSpriteData.health;
                            if (health != -1)
                            {
                                sprites.at(at).health = health;
                            }
                        }
                    }
                }
                else if (command.cmd == "saveSprites")
                {
                    serializeSprites(command.data->saveSpriteData.fileName);
                }
                else if (command.cmd == "loadSprites")
                {
                    sprites.clear();
                    deserializeSprites(command.data->loadData.fileName);
                }
                command.cmd.clear();
            }
        }

        SDL_Delay(16);
    }

    consoleThread.join();

    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    serialize(mapWidth, mapHeight, map, mapFloors, mapCeiling, "map.dat");

    return 0;
}