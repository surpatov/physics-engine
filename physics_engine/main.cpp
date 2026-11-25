#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include "ball.h"

enum class SimulationMode
{
    DCD,
    CCD
};

const float DT_TOP = 1.0f / 60.0f; // шаг обновления верхней симуляции
const float DT_NORMAL = 1.0f / 60.0f; // шаг обновления нижней медленной симуляции
const float DT_FAST = 1.0f / 240.0f; // шаг обновления нижней быстрой симуляции     <- ЗДЕСЬ МЕНЯТЬ ГЕРЦОВКУ

const float WALL_X = 610.0f;
const float WALL_Y = 90.0f;
const float WALL_WIDTH = 10.0f;
const float WALL_HEIGHT = 120.0f;

const float PROJECTILE_GRAVITY = 900.0f;
const float PROJECTILE_RESTITUTION = 0.65f;

void resetProjectile(sf::CircleShape& projectile, sf::Vector2f& velocity, bool& collided, bool& tunneled)
{
    projectile.setPosition(100.0f, 145.0f);
    velocity = sf::Vector2f(2000.0f, 0.0f);
    collided = false;
    tunneled = false;
}

void updateDCD(sf::CircleShape& projectile, sf::RectangleShape& wall, sf::Vector2f& velocity, float dt, bool& collided, bool& tunneled)
{
    velocity.y += PROJECTILE_GRAVITY * dt;

    sf::Vector2f position = projectile.getPosition();
    position += velocity * dt;
    projectile.setPosition(position);

    if (tunneled) return;

    if (projectile.getGlobalBounds().intersects(wall.getGlobalBounds()))
    {
        if (!collided)
        {
            std::cout << "DCD: Collision detected!\n";
            collided = true;
        }

        float diameter = projectile.getRadius() * 2.0f;
        position.x = wall.getPosition().x - diameter;
        projectile.setPosition(position);
        velocity.x = -velocity.x * PROJECTILE_RESTITUTION;
    }
    else
    {
        float wallRight = wall.getPosition().x + wall.getSize().x;
        sf::FloatRect bounds = projectile.getGlobalBounds();

        bool passedWall = bounds.left > wallRight;
        bool verticalOverlap = bounds.top < wall.getPosition().y + wall.getSize().y &&
            bounds.top + bounds.height > wall.getPosition().y;

        if (passedWall && verticalOverlap)
        {
            tunneled = true;
            std::cout << "DCD: TUNNELING!\n";
        }
    }
}


void updateCCD(sf::CircleShape& projectile, sf::RectangleShape& wall, sf::Vector2f& velocity, float dt, bool& collided)
{
    float remainingTime = dt;
    float diameter = projectile.getRadius() * 2.0f;

    sf::Vector2f position = projectile.getPosition();
    sf::FloatRect wallBounds = wall.getGlobalBounds();

    if (velocity.x > 0.0f)
    {
        float gap = wallBounds.left - (position.x + diameter);

        if (gap >= 0.0f)
        {
            float timeToImpact = gap / velocity.x;

            if (timeToImpact <= remainingTime)
            {
                float impactY = position.y + velocity.y*timeToImpact + 0.5f*PROJECTILE_GRAVITY*timeToImpact*timeToImpact;

                bool verticalOverlap = impactY + projectile.getRadius() > wallBounds.top &&
                    impactY - projectile.getRadius() < wallBounds.top + wallBounds.height;

                if (verticalOverlap)
                {
                    position.x = wallBounds.left - diameter;
                    position.y = impactY;

                    velocity.y += PROJECTILE_GRAVITY * timeToImpact;
                    velocity.x = -velocity.x;

                    if (!collided)
                    {
                        std::cout << "CCD: Collision detected!\n";
                        collided = true;
                    }

                    remainingTime -= timeToImpact;
                }
            }
        }
    }

    position.x += velocity.x * remainingTime;
    position.y += velocity.y * remainingTime + 0.5f * PROJECTILE_GRAVITY * remainingTime * remainingTime;
    velocity.y += PROJECTILE_GRAVITY * remainingTime;

    projectile.setPosition(position);
}


int main()
{
    sf::RenderWindow window(sf::VideoMode(1000, 700), "DCD CCD Multi-rate Simulation");
    window.setFramerateLimit(60);

    sf::CircleShape projectile(5.0f);
    projectile.setFillColor(sf::Color::Red);

    sf::RectangleShape wall(sf::Vector2f(WALL_WIDTH, WALL_HEIGHT));
    wall.setFillColor(sf::Color::White);
    wall.setPosition(WALL_X, WALL_Y);

    sf::Vector2f velocity(2000.0f, 0.0f);
    bool collided = false;
    bool tunneled = false;

    SimulationMode mode = SimulationMode::DCD;

    resetProjectile(projectile, velocity, collided, tunneled);

    Cup normalCup = createCup(30.0f);
    Cup fastCup = createCup(530.0f);

    std::vector<Ball> normalBalls;
    std::vector<Ball> fastBalls;

    resetPiles(normalBalls, fastBalls);

    double accumulatorTop = 0.0;
    double accumulatorSlow = 0.0;
    double accumulatorFast = 0.0;

    sf::Clock clock;

    while (window.isOpen())
    {
        double frameTime = clock.restart().asSeconds();
        if (frameTime > 0.05) frameTime = 0.05;

        accumulatorTop += frameTime;
        accumulatorSlow += frameTime;
        accumulatorFast += frameTime;

        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) window.close();

            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Escape) window.close();

                if (event.key.code == sf::Keyboard::Num1)
                {
                    mode = SimulationMode::DCD;
                    resetProjectile(projectile, velocity, collided, tunneled);
                    accumulatorTop = 0.0;
                    std::cout << "Mode: DCD\n";
                }

                if (event.key.code == sf::Keyboard::Num2)
                {
                    mode = SimulationMode::CCD;
                    resetProjectile(projectile, velocity, collided, tunneled);
                    accumulatorTop = 0.0;
                    std::cout << "Mode: CCD\n";
                }

                if (event.key.code == sf::Keyboard::B)
                {
                    resetPiles(normalBalls, fastBalls);
                    accumulatorSlow = 0.0;
                    accumulatorFast = 0.0;
                }
            }
        }

        while (accumulatorTop >= DT_TOP)
        {
            if (mode == SimulationMode::DCD)
                updateDCD(projectile, wall, velocity, DT_TOP, collided, tunneled);
            else
                updateCCD(projectile, wall, velocity, DT_TOP, collided);

            accumulatorTop -= DT_TOP;
        }

        while (accumulatorSlow >= DT_NORMAL)
        {
            updatePile(normalBalls, normalCup, DT_NORMAL);
            accumulatorSlow -= DT_NORMAL;
        }

        while (accumulatorFast >= DT_FAST)
        {
            updatePile(fastBalls, fastCup, DT_FAST);
            accumulatorFast -= DT_FAST;
        }

        window.clear(sf::Color(30, 30, 30));

        window.draw(wall);
        window.draw(projectile);

        sf::RectangleShape separator(sf::Vector2f(1000.0f, 2.0f));
        separator.setPosition(0.0f, 340.0f);
        separator.setFillColor(sf::Color(90, 90, 90));
        window.draw(separator);

        drawCup(window, normalCup);
        drawCup(window, fastCup);

        drawBalls(window, normalBalls);
        drawBalls(window, fastBalls);

        window.display();
    }

    return 0;
}
