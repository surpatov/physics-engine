#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include "ball.h"

enum class SimulationMode
{
    DCD,
    CCD
};

const float DT_TOP = 1.0f / 60.0f;
const float DT_NORMAL = 1.0f / 60.0f;
const float DT_FAST = 1.0f / 240.0f;

const float WALL_X = 610.0f;
const float WALL_Y = 90.0f;
const float WALL_WIDTH = 10.0f;
const float WALL_HEIGHT = 120.0f;

const float PROJECTILE_GRAVITY = 900.0f;
const float PROJECTILE_RESTITUTION = 0.65f;

// Временная структура для сбора статистики.
// Все значения времени измеряются в миллисекундах реального времени CPU.
struct Measurement
{
    double totalMs = 0.0;
    unsigned long long calls = 0;

    void add(double ms)
    {
        totalMs += ms;
        ++calls;
    }

    double averageMs() const
    {
        return calls ? totalMs / static_cast<double>(calls) : 0.0;
    }

    void reset()
    {
        totalMs = 0.0;
        calls = 0;
    }
};

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
                float impactY = position.y + velocity.y * timeToImpact
                    + 0.5f * PROJECTILE_GRAVITY * timeToImpact * timeToImpact;

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
    position.y += velocity.y * remainingTime
        + 0.5f * PROJECTILE_GRAVITY * remainingTime * remainingTime;
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

    // Отдельные измерения каждого типа физического обновления.
    Measurement dcdMeasurement;
    Measurement ccdMeasurement;
    Measurement normalMeasurement;
    Measurement fastMeasurement;
    Measurement framePhysicsMeasurement;

    // Счётчики количества вызовов физики за кадр.
    unsigned long long totalTopSteps = 0;
    unsigned long long totalSlowSteps = 0;
    unsigned long long totalFastSteps = 0;
    unsigned long long renderedFrames = 0;

    // Статистика выводится раз в 2 секунды реального времени.
    auto reportStart = std::chrono::steady_clock::now();

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

        // Суммарное реальное время всех физических расчётов в текущем кадре.
        double thisFramePhysicsMs = 0.0;

        while (accumulatorTop >= DT_TOP)
        {
            auto start = std::chrono::steady_clock::now();

            if (mode == SimulationMode::DCD)
                updateDCD(projectile, wall, velocity, DT_TOP, collided, tunneled);
            else
                updateCCD(projectile, wall, velocity, DT_TOP, collided);

            auto finish = std::chrono::steady_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(finish - start).count();

            if (mode == SimulationMode::DCD)
                dcdMeasurement.add(elapsedMs);
            else
                ccdMeasurement.add(elapsedMs);

            thisFramePhysicsMs += elapsedMs;
            ++totalTopSteps;
            accumulatorTop -= DT_TOP;
        }

        while (accumulatorSlow >= DT_NORMAL)
        {
            auto start = std::chrono::steady_clock::now();

            updatePile(normalBalls, normalCup, DT_NORMAL);

            auto finish = std::chrono::steady_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(finish - start).count();

            normalMeasurement.add(elapsedMs);
            thisFramePhysicsMs += elapsedMs;
            ++totalSlowSteps;
            accumulatorSlow -= DT_NORMAL;
        }

        while (accumulatorFast >= DT_FAST)
        {
            auto start = std::chrono::steady_clock::now();

            updatePile(fastBalls, fastCup, DT_FAST);

            auto finish = std::chrono::steady_clock::now();
            double elapsedMs = std::chrono::duration<double, std::milli>(finish - start).count();

            fastMeasurement.add(elapsedMs);
            thisFramePhysicsMs += elapsedMs;
            ++totalFastSteps;
            accumulatorFast -= DT_FAST;
        }

        framePhysicsMeasurement.add(thisFramePhysicsMs);
        ++renderedFrames;

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

        // Печать статистики раз в 2 секунды.
        auto now = std::chrono::steady_clock::now();
        double reportSeconds = std::chrono::duration<double>(now - reportStart).count();

        if (reportSeconds >= 2.0)
        {
            std::cout << "\n========== PHYSICS PERFORMANCE ==========\n";
            std::cout << std::fixed << std::setprecision(6);

            std::cout << "DCD: average update = " << dcdMeasurement.averageMs()
                << " ms; calls = " << dcdMeasurement.calls << "\n";

            std::cout << "CCD: average update = " << ccdMeasurement.averageMs()
                << " ms; calls = " << ccdMeasurement.calls << "\n";

            std::cout << "Normal balls (20, 60 Hz): average update = "
                << normalMeasurement.averageMs()
                << " ms; calls = " << normalMeasurement.calls << "\n";

            std::cout << "Fast balls (5, 240 Hz): average update = "
                << fastMeasurement.averageMs()
                << " ms; calls = " << fastMeasurement.calls << "\n";

            std::cout << "Average total physics time per rendered frame = "
                << framePhysicsMeasurement.averageMs() << " ms\n";

            if (renderedFrames > 0)
            {
                std::cout << "Average physics steps per rendered frame: "
                    << "projectile = "
                    << static_cast<double>(totalTopSteps) / renderedFrames
                    << ", normal = "
                    << static_cast<double>(totalSlowSteps) / renderedFrames
                    << ", fast = "
                    << static_cast<double>(totalFastSteps) / renderedFrames
                    << "\n";
            }

            std::cout << "=========================================\n\n";

            dcdMeasurement.reset();
            ccdMeasurement.reset();
            normalMeasurement.reset();
            fastMeasurement.reset();
            framePhysicsMeasurement.reset();

            totalTopSteps = 0;
            totalSlowSteps = 0;
            totalFastSteps = 0;
            renderedFrames = 0;
            reportStart = now;
        }
    }

    return 0;
}
