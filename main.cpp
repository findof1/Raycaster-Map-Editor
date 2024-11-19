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

struct Command
{
    std::string cmd;

    enum DataType
    {
        None,
        LoadData,
        UnloadData
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
        Data() {}
        ~Data() {}
    };

    void setLoadData(const std::string &fileName)
    {
        clearData();
        new (&data->loadData) Data::LoadData(fileName);
        dataType = DataType::LoadData;
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

int mapWidth;
int mapHeight;
int cellType = 1;
std::atomic<bool> running(true);
std::mutex commandMutex;
Command command;

std::vector<SDL_Texture *> textures;
std::vector<std::string> texturePaths = {"./textures/texture-1.png", "./textures/texture-2.png", "./textures/texture-3.png", "./textures/texture-4.png", "./textures/texture-5.png", "./textures/texture-6.png"};

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

void consoleCommands()
{
    while (running)
    {
        std::string input;
        std::cin >> input;
        command.clearData();
        command.dataType = Command::DataType::None;
        if (input == "load")
        {
            command.dataType = Command::DataType::LoadData;
            std::string fileName;
            std::cout << "Enter a filename: ";
            std::cin >> fileName;

            command.setLoadData(fileName);
        }

        if (input == "unload")
        {
            int w, h;
            command.dataType = Command::DataType::UnloadData;
            std::cout << "Enter the width of the map: ";
            std::cin >> w;
            std::cout << std::endl;
            std::cout << "Enter the height of the map: ";
            command.clearData();
            std::cin >> h;

            command.setUnloadData(w, h);
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
    for (int i = 0; i < 6; i++)
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
            if ((event.type == SDL_MOUSEMOTION && SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) || event.type == SDL_MOUSEBUTTONDOWN)
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
        }

        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
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
                    serialize(mapWidth, mapHeight, map, mapFloors, mapCeiling, "map.dat");
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