#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

class Ball
{
public:
    sf::Vector2f position;
    sf::Vector2f velocity;
    float radius;
    sf::Color color;

    Ball(sf::Vector2f pos, sf::Vector2f vel, float r, sf::Color c)
        : position(pos), velocity(vel), radius(r), color(c)
    {
    }
};

struct Cup
{
    sf::RectangleShape leftWall;
    sf::RectangleShape rightWall;
    sf::RectangleShape floor;
};

float dot(const sf::Vector2f& a, const sf::Vector2f& b);
float vectorLength(const sf::Vector2f& v);
sf::Vector2f normalize(const sf::Vector2f& v);
float clampValue(float value, float minValue, float maxValue);

Cup createCup(float x);

void createBalls(std::vector<Ball>& balls, float cupX, int count, float radius, float speed, sf::Color color, bool fast);

void resetPiles(std::vector<Ball>& normalBalls, std::vector<Ball>& fastBalls);

void resolveCircleAABB(Ball& ball, const sf::RectangleShape& box);
void resolveBallCollision(Ball& a, Ball& b);

void updatePile(std::vector<Ball>& balls, const Cup& cup, float dt);

void drawBalls(sf::RenderWindow& window, const std::vector<Ball>& balls);
void drawCup(sf::RenderWindow& window, const Cup& cup);
