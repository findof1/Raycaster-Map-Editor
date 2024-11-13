#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
int mapWidth;
int mapHeight;
int cellType = 1;
int main()
{
    std::vector<int> map;
    std::cout << "Enter the width of the map: ";
    std::cin >> mapWidth;
    std::cout << std::endl;
    std::cout << "Enter the height of the map: ";
    std::cin >> mapHeight;
    std::cout << std::endl;

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

    bool running = true;
    float width = 700 / mapWidth;
    float height = 700 / mapHeight;
    while (running)
    {

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
             if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int x = event.button.x
                int y = event.button.y
                int cellX = floor(x / width);
                int cellY = floor(y / height);
                map[cellX + cellY*mapWidth] = cellType;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

    
        for (int x = 0; x < mapWidth; x++)
        {
            for (int y = 0; y < mapHeight; y++)
            {
                SDL_FRect square;
                square.w = width - 1;
                square.h = height - 1;
                square.x = x * width + 1;
                square.y = y * height + 1;
                switch(cellType){
                    case 0:
                    SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
                    break;
                    case 1:
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                    break;
                    case 2:
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                    case 3:
                    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
                    case 4:
                    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                    case 5:
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                    case 6:
                    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                    case 7:
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    break;
                }
                SDL_RenderDrawRectF(renderer, &square);
            }
        }

        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    for(int i = 0; i < mapWidth; i++){
        for(int j = 0; j < mapHeight; j++){
            std::cout << map[i + j * mapWidth] << ",";
        }
        std::endl;
    }

    std::cin.get();
    std::cin.get();
    std::cin.get();

    return 0;
}