#include "ball.h"
#include <cmath>
#include <algorithm>

const float GRAVITY = 900.0f;
const float RESTITUTION = 0.65f;

float dot(const sf::Vector2f& a, const sf::Vector2f& b)
{
    return a.x * b.x + a.y * b.y;
}

float vectorLength(const sf::Vector2f& v)
{
    return std::sqrt(dot(v, v));
}

sf::Vector2f normalize(const sf::Vector2f& v)
{
    float len = vectorLength(v);
    if (len < 0.0001f) return sf::Vector2f(0.0f, 0.0f);
    return v / len;
}

float clampValue(float value, float minValue, float maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

Cup createCup(float x)
{
    Cup cup;

    cup.leftWall.setSize(sf::Vector2f(12.0f, 270.0f));
    cup.leftWall.setPosition(x, 400.0f);
    cup.leftWall.setFillColor(sf::Color(130, 135, 145));

    cup.rightWall.setSize(sf::Vector2f(12.0f, 270.0f));
    cup.rightWall.setPosition(x + 408.0f, 400.0f);
    cup.rightWall.setFillColor(sf::Color(130, 135, 145));

    cup.floor.setSize(sf::Vector2f(420.0f, 12.0f));
    cup.floor.setPosition(x, 658.0f);
    cup.floor.setFillColor(sf::Color(130, 135, 145));

    return cup;
}

void createBalls(std::vector<Ball>& balls, float cupX, int count, float radius, float speed, sf::Color color, bool fast)
{
    balls.clear();

    for (int i = 0; i < count; i++)
    {
        float x;
        float y;
        float vx;
        float vy;

        if (fast)
        {
            x = cupX + 85.0f + (i % 3) * 65.0f;
            y = 440.0f + (i / 3) * 35.0f;
            vx = speed;
            vy = 0.0f;
        }
        else
        {
            int column = i % 5;
            int row = i / 5;

            x = cupX + 75.0f + column * 70.0f;
            y = 430.0f + row * 23.0f;
            vx = (column % 2 == 0) ? speed : -speed;
            vy = 0.0f;
        }

        balls.push_back({ sf::Vector2f(x, y), sf::Vector2f(vx, vy), radius, color });
    }
}

void resetPiles(std::vector<Ball>& normalBalls, std::vector<Ball>& fastBalls)
{
    createBalls(normalBalls, 30.0f, 20, 9.0f, 150.0f, sf::Color(60, 220, 110), false);
    createBalls(fastBalls, 530.0f, 5, 9.0f, 2000.0f, sf::Color(255, 155, 45), true); // здесь менять скорость
}

void resolveCircleAABB(Ball& ball, const sf::RectangleShape& box)
{
    sf::FloatRect bounds = box.getGlobalBounds();

    float closestX = clampValue(ball.position.x, bounds.left, bounds.left + bounds.width);
    float closestY = clampValue(ball.position.y, bounds.top, bounds.top + bounds.height);

    sf::Vector2f difference(ball.position.x - closestX, ball.position.y - closestY);
    float distance = vectorLength(difference);

    if (distance >= ball.radius) return;

    sf::Vector2f normal;
    float penetration;

    if (distance > 0.0001f)
    {
        normal = difference / distance;
        penetration = ball.radius - distance;
    }
    else // если нормаль не свысчитать правильно, смотрим, ближе к какой части стенки и отталкиваемся от нее
    {
        float leftDistance = ball.position.x - bounds.left;
        float rightDistance = bounds.left + bounds.width - ball.position.x;
        float topDistance = ball.position.y - bounds.top;
        float bottomDistance = bounds.top + bounds.height - ball.position.y;

        float minDistance = std::min({ leftDistance, rightDistance, topDistance, bottomDistance });

        if (minDistance == leftDistance) normal = sf::Vector2f(-1.0f, 0.0f);
        else if (minDistance == rightDistance) normal = sf::Vector2f(1.0f, 0.0f);
        else if (minDistance == topDistance) normal = sf::Vector2f(0.0f, -1.0f);
        else normal = sf::Vector2f(0.0f, 1.0f);

        penetration = ball.radius + minDistance;
    }

    ball.position += normal * penetration;

    float velocityAlongNormal = dot(ball.velocity, normal);

    if (velocityAlongNormal < 0.0f)
    {
        ball.velocity -= normal * ((1.0f + RESTITUTION) * velocityAlongNormal);
    }
}

void resolveBallCollision(Ball& a, Ball& b)
{
    sf::Vector2f difference = b.position - a.position;
    float distance = vectorLength(difference);
    float minDistance = a.radius + b.radius;

    if (distance >= minDistance) return;

    sf::Vector2f normal;

    if (distance > 0.0001f)
        normal = difference / distance;
    else
        normal = sf::Vector2f(1.0f, 0.0f);

    float penetration = minDistance - distance;

    a.position -= normal * (penetration * 0.5f);
    b.position += normal * (penetration * 0.5f);

    sf::Vector2f relativeVelocity = b.velocity - a.velocity;
    float velocityAlongNormal = dot(relativeVelocity, normal);

    if (velocityAlongNormal > 0.0f) return;

    float impulseMagnitude = -(1.0f + RESTITUTION) * velocityAlongNormal / 2.0f;
    sf::Vector2f impulse = normal * impulseMagnitude;

    a.velocity -= impulse;
    b.velocity += impulse;
}

void updatePile(std::vector<Ball>& balls, const Cup& cup, float dt)
{
    for (Ball& ball : balls)
    {
        ball.velocity.y += GRAVITY * dt;
        ball.position += ball.velocity * dt;
    }

    for (int iteration = 0; iteration < 5; iteration++)
    {
        for (Ball& ball : balls)
        {
            resolveCircleAABB(ball, cup.leftWall);
            resolveCircleAABB(ball, cup.rightWall);
            resolveCircleAABB(ball, cup.floor);
        }

        for (std::size_t i = 0; i < balls.size(); i++)
        {
            for (std::size_t j = i + 1; j < balls.size(); j++)
            {
                resolveBallCollision(balls[i], balls[j]);
            }
        }
    }
}

void drawBalls(sf::RenderWindow& window, const std::vector<Ball>& balls)
{
    for (const Ball& ball : balls)
    {
        sf::CircleShape shape(ball.radius);
        shape.setOrigin(ball.radius, ball.radius);
        shape.setPosition(ball.position);
        shape.setFillColor(ball.color);
        window.draw(shape);
    }
}

void drawCup(sf::RenderWindow& window, const Cup& cup)
{
    window.draw(cup.leftWall);
    window.draw(cup.rightWall);
    window.draw(cup.floor);
}
