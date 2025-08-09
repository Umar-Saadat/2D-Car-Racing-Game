#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <GL/glut.h>
#include <GL/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <fstream>
#include <ctime>
#include <cctype>

using namespace std;

const int windowWidth = 2048;
const int windowHeight = 1152;

GLuint roadTex, playerTex, car1Tex, car2Tex, car3Tex;

float roadOffset = 0;
float playerSpeed = 30.0f;
float minSpeed = 30.0f;
const float maxSpeed = 250.0f;
const float acceleration = 0.5f;
const float deceleration = 0.3f;
const float brakeDecel = 1.5f;

bool keyUp = false, keyLeft = false, keyRight = false, keyBrake = false;

enum Difficulty { EASY, MEDIUM, HARD };
Difficulty currentDifficulty = MEDIUM;

enum GameState { MENU, GAME, SCORES, SELECT_LEVEL, GAME_OVER, PAUSED };
GameState currentState = MENU;

struct Car {
    float x, y;
    float speed;
    GLuint texture;
    bool isPlayer;
    int health = 5;
};

const float roadLeftBound = 300.0f;
const float roadRightBound = 1400.0f;

const float carHeight = 500.0f;
const float carWidth = 300.0f;
const float bottomMargin = 50.0f;
const float horizontalMargin = 50.0f;
const float collisionGap = 5.0f;
const float collisionBuffer = 10.0f;

Car player = { 800.0f, windowHeight - carHeight - bottomMargin, 30.0f, 0, true };
vector<Car> aiCars;

float raceDistance = 0;
int lap = 1;
int playerPosition = 1;
bool gameOver = false;
const float lapDistance = 20000.0f;
const int totalLaps = 3;
int score = 0;
float scoreMultiplier = 2.0f;

float gameOverTime = 0;
const float gameOverDelay = 3.0f;

#ifdef _WIN32
static DWORD lastCollisionSound = 0;
#endif

void drawScene();
void drawHUD();
void update(int value);
void keyDown(unsigned char key, int x, int y);
void keyUpFunc(unsigned char key, int x, int y);
void specialDown(int key, int x, int y);
void specialUp(int key, int x, int y);
void initGame();
void mouseClick(int button, int state, int x, int y);
void mouseMotion(int x, int y);
#ifdef _WIN32
void cleanup();
#endif
void resetGame();
void startGame();
void showScores();
void selectLevel();
void setEasy();
void setMedium();
void setHard();
void goBack();
void exitGame();
void continueGame();
void pauseGame();

struct Button {
    float x, y, width, height;
    string label;
    bool hovered;
    void (*action)();
};

vector<Button> menuButtons;
vector<Button> levelButtons;
vector<Button> scoreButtons;

GLuint loadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data) {
        cerr << "Failed to load texture: " << filename << endl;
        return 0;
    }
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return textureID;
}

void drawTexturedQuad(GLuint texture, float x, float y, float w = 300, float h = carHeight) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x, y);
    glTexCoord2f(1, 0); glVertex2f(x + w, y);
    glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
    glTexCoord2f(0, 1); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void drawText(float x, float y, string text, float scale = 1.0f) {
    glColor3f(1.0, 1.0, 1.0);
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
}

void drawButton(const Button& button) {
    glColor3f(button.hovered ? 0.7f : 0.5f, button.hovered ? 0.7f : 0.5f, button.hovered ? 0.7f : 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(button.x, button.y);
    glVertex2f(button.x + button.width, button.y);
    glVertex2f(button.x + button.width, button.y + button.height);
    glVertex2f(button.x, button.y + button.height);
    glEnd();
    float textX = button.x + (button.width - button.label.length() * 12) / 2;
    float textY = button.y + button.height / 2 + 6;
    drawText(textX, textY, button.label, 1.0f);
}

bool checkCollision(const Car& a, const Car& b) {
    return abs(a.x - b.x) < (carWidth - collisionBuffer) &&
           abs(a.y - b.y) < (carHeight - collisionBuffer);
}

void resolvePlayerAICollision(Car& player, Car& aiCar) {
    float overlapX = carWidth - abs(player.x - aiCar.x);
    float overlapY = carHeight - abs(player.y - aiCar.y);

    if (overlapY < overlapX) {
        if (player.y < aiCar.y) {
            aiCar.y = player.y + carHeight + collisionGap;
            aiCar.speed = max(minSpeed, playerSpeed * 0.9f);
        } else {
            player.y = aiCar.y + carHeight + collisionGap;
            playerSpeed = max(minSpeed, aiCar.speed * 0.9f);
        }
    } else {
        if (player.x < aiCar.x) {
            player.x = aiCar.x - carWidth - collisionGap;
        } else {
            player.x = aiCar.x + carWidth + collisionGap;
        }
        playerSpeed = max(minSpeed, playerSpeed * 0.95f);
        aiCar.speed = max(minSpeed, aiCar.speed * 0.95f);
    }

    if (player.x < roadLeftBound + horizontalMargin) player.x = roadLeftBound + horizontalMargin;
    if (player.x > roadRightBound - horizontalMargin) player.x = roadRightBound - horizontalMargin;
    if (aiCar.x < roadLeftBound + horizontalMargin) aiCar.x = roadLeftBound + horizontalMargin;
    if (aiCar.x > roadRightBound - horizontalMargin) aiCar.x = roadRightBound - horizontalMargin;

    cout << "Player-AI Collision: Player(" << player.x << ", " << player.y << "), AI("
         << aiCar.x << ", " << aiCar.y << "), Overlap(X: " << overlapX << ", Y: " << overlapY << ")" << endl;
}

void resolveAICarCollision(Car& car1, Car& car2) {
    float overlapX = carWidth - abs(car1.x - car2.x);
    float overlapY = carHeight - abs(car1.y - car2.y);

    if (overlapY < overlapX) {
        if (car1.y < car2.y) {
            car2.y = car1.y + carHeight + collisionGap;
            car2.speed = max(minSpeed, car1.speed * 0.9f);
        } else {
            car1.y = car2.y + carHeight + collisionGap;
            car1.speed = max(minSpeed, car2.speed * 0.9f);
        }
    } else {
        if (car1.x < car2.x) {
            car1.x = car2.x - carWidth - collisionGap;
        } else {
            car1.x = car2.x + carWidth + collisionGap;
        }
        car1.speed = max(minSpeed, car1.speed * 0.95f);
        car2.speed = max(minSpeed, car2.speed * 0.95f);
    }

    if (car1.x < roadLeftBound + horizontalMargin) car1.x = roadLeftBound + horizontalMargin;
    if (car1.x > roadRightBound - horizontalMargin) car1.x = roadRightBound - horizontalMargin;
    if (car2.x < roadLeftBound + horizontalMargin) car2.x = roadLeftBound + horizontalMargin;
    if (car2.x > roadRightBound - horizontalMargin) car2.x = roadRightBound - horizontalMargin;

    cout << "AI-AI Collision: Car1(" << car1.x << ", " << car1.y << "), Car2("
         << car2.x << ", " << car2.y << "), Overlap(X: " << overlapX << ", Y: " << overlapY << ")" << endl;
}

float lastAISpawnTime = 0;
float aiSpawnInterval = 2.0f;

void updateAI() {
    float aggression = 1.0f + (lap - 1) * 0.5f;
    float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

    if (currentTime - lastAISpawnTime > aiSpawnInterval && aiCars.size() < (currentDifficulty == EASY ? 2 : currentDifficulty == MEDIUM ? 3 : 4)) {
        float x = roadLeftBound + horizontalMargin + (rand() % static_cast<int>(roadRightBound - roadLeftBound - 2 * horizontalMargin - carWidth));
        GLuint tex = (rand() % 3 == 0) ? car1Tex : (rand() % 2 == 0) ? car2Tex : car3Tex;
        aiCars.push_back({ x, static_cast<float>(-600 - (rand() % 400)), playerSpeed * (0.85f + static_cast<float>(rand()) / RAND_MAX * 0.3f), tex, false });
        lastAISpawnTime = currentTime;
    }

    for (auto it = aiCars.begin(); it != aiCars.end();) {
        Car& car = *it;
        float relativeSpeed = playerSpeed * (0.85f + static_cast<float>(rand()) / RAND_MAX * 0.3f);
        car.speed = relativeSpeed;
        car.y += car.speed * 0.5f;

        if (car.y > -carHeight) {
            if (car.x < player.x) car.x += 2.0f * aggression;
            else if (car.x > player.x) car.x -= 2.0f * aggression;

            float zigzag = sin(glutGet(GLUT_ELAPSED_TIME) * 0.001f + car.y * 0.01f) * 1.5f;
            car.x += zigzag;

            if (car.x < roadLeftBound + horizontalMargin) car.x = roadLeftBound + horizontalMargin;
            if (car.x > roadRightBound - horizontalMargin) car.x = roadRightBound - horizontalMargin;

            for (auto& other : aiCars) {
                if (&car != &other && checkCollision(car, other)) {
                    resolveAICarCollision(car, other);
                }
            }
        }

        if (car.y > windowHeight + 200) {
            car.y = static_cast<float>(-600 - (rand() % 400));
            car.speed = playerSpeed * (0.85f + static_cast<float>(rand()) / RAND_MAX * 0.3f);
        }

        if (checkCollision(player, car)) {
            player.health--;
            resolvePlayerAICollision(player, car);
            #ifdef _WIN32
            DWORD currentTime = GetTickCount();
            if (currentTime - lastCollisionSound > 500) {
                mciSendString(TEXT("stop crash"), NULL, 0, NULL);
                mciSendString(TEXT("close crash"), NULL, 0, NULL);
                mciSendString(TEXT("open \"CRASH.mp3\" type mpegvideo alias crash"), NULL, 0, NULL);
                mciSendString(TEXT("play crash"), NULL, 0, NULL);
                lastCollisionSound = currentTime;
            }
            #endif
            it = aiCars.erase(it);
            if (player.health <= 0) {
                gameOver = true;
                gameOverTime = currentTime;
            }
        } else {
            ++it;
        }
    }

    playerPosition = 1;
    for (auto& car : aiCars)
        if (car.y > player.y)
            playerPosition++;
}

void drawHUD() {
    if (currentState == GAME || currentState == GAME_OVER || currentState == PAUSED) {
        stringstream hud;
        if (!gameOver) {
            hud << "Speed: " << (int)playerSpeed << " km/h  "
                << "Lap: " << lap << "/" << totalLaps << "  "
                << "Distance: " << (int)(raceDistance / 1000.0f) << "km/" << (int)(lapDistance * totalLaps / 1000.0f) << "km  "
                << "Health: " << player.health << "  "
                << "Position: " << playerPosition << "/4  "
                << "Score: " << score;
            drawText(20, 30, hud.str(), 1.0f);
        } else {
            hud << (playerPosition == 1 && lap > totalLaps ? "You Win!" : "Game Over!") << "\n"
                << "Final Score: " << score << "\n"
                << "Final Lap: " << lap << "/" << totalLaps << "\n"
                << "Final Position: " << playerPosition << "/4\n"
                << "Returning to menu in " << (int)(gameOverDelay - (glutGet(GLUT_ELAPSED_TIME) / 1000.0f - gameOverTime)) << " seconds";
            drawText(windowWidth / 2 - 200, windowHeight / 2 - 100, hud.str(), 2.0f);
        }
    }
}

void update(int value) {
    float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    if (!gameOver && currentState == GAME) {
        if (keyUp) playerSpeed += acceleration;
        if (keyBrake) playerSpeed -= brakeDecel;

        if (playerSpeed < minSpeed) playerSpeed = minSpeed;
        if (playerSpeed > maxSpeed) playerSpeed = maxSpeed;

        if (keyLeft) player.x -= 10;
        if (keyRight) player.x += 10;

        if (player.x < roadLeftBound + horizontalMargin) player.x = roadLeftBound + horizontalMargin;
        if (player.x > roadRightBound - horizontalMargin) player.x = roadRightBound - horizontalMargin;

        if (player.y < bottomMargin) player.y = bottomMargin;
        if (player.y > windowHeight - carHeight - bottomMargin) player.y = windowHeight - carHeight - bottomMargin;

        raceDistance += playerSpeed * 0.5f;
        score += static_cast<int>((raceDistance / 1000.0f) * scoreMultiplier);

        if (raceDistance >= lapDistance * lap && lap <= totalLaps) lap++;
        if (lap > totalLaps && playerPosition == 1) {
            gameOver = true;
            gameOverTime = currentTime;
        }

        roadOffset += playerSpeed * 0.5f;
        if (roadOffset >= windowHeight)
            roadOffset = 0;

        updateAI();
    } else if (currentState == GAME_OVER && currentTime - gameOverTime > gameOverDelay) {
        resetGame();
        currentState = MENU;
    }
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void drawMenu() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawTexturedQuad(roadTex, 0, 0, windowWidth, windowHeight);
    drawText(windowWidth / 2 - 200, 100, "2D Racing Game", 2.0f);
    for (const auto& button : menuButtons) {
        drawButton(button);
    }
    drawText(windowWidth / 2 - 100, windowHeight - 100, "N: New Game | S: Score | L: Level | E: Exit", 1.0f);
    glutSwapBuffers();
}

void drawScores() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawTexturedQuad(roadTex, 0, 0, windowWidth, windowHeight);
    drawText(windowWidth / 2 - 100, 100, "High Scores", 2.0f);

    ifstream file("scores.txt");
    string line;
    int yOffset = 200;
    while (getline(file, line) && yOffset < windowHeight - 150) {
        drawText(200, yOffset, line, 1.0f);
        yOffset += 50;
    }
    file.close();

    for (const auto& button : scoreButtons) {
        drawButton(button);
    }
    drawText(windowWidth / 2 - 50, windowHeight - 100, "B: Back", 1.0f);
    glutSwapBuffers();
}

void drawLevelSelect() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawTexturedQuad(roadTex, 0, 0, windowWidth, windowHeight);
    drawText(windowWidth / 2 - 100, 100, "Select Level", 2.0f);
    for (const auto& button : levelButtons) {
        drawButton(button);
    }
    drawText(windowWidth / 2 - 50, windowHeight - 100, "B: Back", 1.0f);
    glutSwapBuffers();
}

void drawScene() {
    if (currentState == MENU) {
        drawMenu();
    } else if (currentState == SCORES) {
        drawScores();
    } else if (currentState == SELECT_LEVEL) {
        drawLevelSelect();
    } else if (currentState == GAME || currentState == GAME_OVER || currentState == PAUSED) {
        glClear(GL_COLOR_BUFFER_BIT);
        drawTexturedQuad(roadTex, 0, -roadOffset, windowWidth, windowHeight);
        drawTexturedQuad(roadTex, 0, -roadOffset + windowHeight, windowWidth, windowHeight);

        for (auto& car : aiCars) drawTexturedQuad(car.texture, car.x, car.y);
        drawTexturedQuad(player.texture, player.x, player.y);
        drawHUD();

        glutSwapBuffers();
    }
}

void keyDown(unsigned char key, int x, int y) {
    key = tolower(key);
    if (currentState == MENU) {
        if (key == 'n') startGame();
        else if (key == 's') showScores();
        else if (key == 'l') selectLevel();
        else if (key == 'e') exitGame();
        else if (key == 'c') continueGame();
    } else if (currentState == SCORES || currentState == SELECT_LEVEL) {
        if (key == 'b') goBack();
    } else if (currentState == GAME) {
        if (key == 27) {
            pauseGame();
        }
        if (key == 32) keyBrake = true;
    } else if (currentState == GAME_OVER) {
        resetGame();
        currentState = MENU;
    }
    #ifdef _WIN32
    if (key == 'p' && currentState == GAME) {
        static bool paused = false;
        if (paused) {
            mciSendString(TEXT("play bgm repeat"), NULL, 0, NULL);
        } else {
            mciSendString(TEXT("pause bgm"), NULL, 0, NULL);
        }
        paused = !paused;
    }
    #endif
}

void keyUpFunc(unsigned char key, int x, int y) {
    if (currentState == GAME) {
        if (key == 32) keyBrake = false;
    }
}

void specialDown(int key, int x, int y) {
    if (currentState == GAME) {
        if (key == GLUT_KEY_UP) keyUp = true;
        if (key == GLUT_KEY_LEFT) keyLeft = true;
        if (key == GLUT_KEY_RIGHT) keyRight = true;
    }
}

void specialUp(int key, int x, int y) {
    if (currentState == GAME) {
        if (key == GLUT_KEY_UP) keyUp = false;
        if (key == GLUT_KEY_LEFT) keyLeft = false;
        if (key == GLUT_KEY_RIGHT) keyRight = false;
    }
}

void startGame() {
    resetGame();
    currentState = GAME;
}

void showScores() {
    currentState = SCORES;
}

void selectLevel() {
    currentState = SELECT_LEVEL;
}

void setEasy() {
    currentDifficulty = EASY;
    minSpeed = 10.0f;
    playerSpeed = minSpeed;
    scoreMultiplier = 1.0f;
    aiSpawnInterval = 4.0f;
    currentState = MENU;
}

void setMedium() {
    currentDifficulty = MEDIUM;
    minSpeed = 20.0f;
    playerSpeed = minSpeed;
    scoreMultiplier = 2.0f;
    aiSpawnInterval = 2.0f;
    currentState = MENU;
}

void setHard() {
    currentDifficulty = HARD;
    minSpeed = 40.0f;
    playerSpeed = minSpeed;
    scoreMultiplier = 3.0f;
    aiSpawnInterval = 1.0f;
    currentState = MENU;
}

void goBack() {
    currentState = MENU;
}

void exitGame() {
    #ifdef _WIN32
    cleanup();
    #endif
    exit(0);
}

void continueGame() {
    if (currentState == MENU && !gameOver) {
        currentState = GAME;
    }
}

void pauseGame() {
    if (currentState == GAME) {
        currentState = MENU;
    }
}

void resetGame() {
    player = { 800.0f, windowHeight - carHeight - bottomMargin, minSpeed, playerTex, true, 5 };
    aiCars.clear();
    raceDistance = 0;
    lap = 1;
    playerPosition = 1;
    gameOver = false;
    score = 0;
    lastAISpawnTime = 0;
    roadOffset = 0;
    playerSpeed = minSpeed;
}

void mouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        cout << "Click at (" << x << ", " << y << ")" << endl;
        vector<Button>* buttons = nullptr;
        if (currentState == MENU) buttons = &menuButtons;
        else if (currentState == SCORES) buttons = &scoreButtons;
        else if (currentState == SELECT_LEVEL) buttons = &levelButtons;

        if (buttons) {
            for (auto& btn : *buttons) {
                cout << "Checking button: " << btn.label << " at (" << btn.x << ", " << btn.y << ", " << btn.x + btn.width << ", " << btn.y + btn.height << ")" << endl;
                if (x >= btn.x && x <= btn.x + btn.width && y >= btn.y && y <= btn.y + btn.height) {
                    cout << "Button clicked: " << btn.label << endl;
                    btn.action();
                    break;
                }
            }
        }
    }
}

void mouseMotion(int x, int y) {
    vector<Button>* buttons = nullptr;
    if (currentState == MENU) buttons = &menuButtons;
    else if (currentState == SCORES) buttons = &scoreButtons;
    else if (currentState == SELECT_LEVEL) buttons = &levelButtons;

    if (buttons) {
        for (auto& btn : *buttons) {
            btn.hovered = (x >= btn.x && x <= btn.x + btn.width && y >= btn.y && y <= btn.y + btn.height);
        }
    }
    glutPostRedisplay();
}

#ifdef _WIN32
void cleanup() {
    ofstream file("scores.txt", ios::app);
    if (file.is_open()) {
        time_t now = time(0);
        string diff = currentDifficulty == EASY ? "Easy" : currentDifficulty == MEDIUM ? "Medium" : "Hard";
        file << "Score: " << score << " | Difficulty: " << diff << " | Time: " << ctime(&now) << endl;
        file.close();
    }
    mciSendString(TEXT("stop bgm"), NULL, 0, NULL);
    mciSendString(TEXT("close bgm"), NULL, 0, NULL);
    mciSendString(TEXT("stop crash"), NULL, 0, NULL);
    mciSendString(TEXT("close crash"), NULL, 0, NULL);
}
#endif

void initGame() {
    #ifdef _WIN32
    mciSendString(TEXT("open \"audio.mp3\" type mpegvideo alias bgm"), NULL, 0, NULL);
    mciSendString(TEXT("setaudio bgm volume to 300"), NULL, 0, NULL);
    mciSendString(TEXT("play bgm repeat"), NULL, 0, NULL);
    #endif

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, windowHeight, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    roadTex = loadTexture("ROAD.png");
    playerTex = loadTexture("PLAYER.png");
    car1Tex = loadTexture("CAR1.png");
    car2Tex = loadTexture("CAR2.png");
    car3Tex = loadTexture("CAR3.png");

    float btnWidth = 400, btnHeight = 100, btnSpacing = 50;
    float totalHeight = 5 * btnHeight + 4 * btnSpacing;
    float startY = (windowHeight - totalHeight) / 2;

    menuButtons.clear();
    menuButtons.push_back({ (windowWidth - btnWidth) / 2, startY, btnWidth, btnHeight, "Continue", false, continueGame });
    menuButtons.push_back({ (windowWidth - btnWidth) / 2, startY + btnHeight + btnSpacing, btnWidth, btnHeight, "Start Game", false, startGame });
    menuButtons.push_back({ (windowWidth - btnWidth) / 2, startY + 2 * (btnHeight + btnSpacing), btnWidth, btnHeight, "Score", false, showScores });
    menuButtons.push_back({ (windowWidth - btnWidth) / 2, startY + 3 * (btnHeight + btnSpacing), btnWidth, btnHeight, "Select Level", false, selectLevel });
    menuButtons.push_back({ (windowWidth - btnWidth) / 2, startY + 4 * (btnHeight + btnSpacing), btnWidth, btnHeight, "Exit", false, exitGame });

    levelButtons.clear();
    levelButtons.push_back({ (windowWidth - btnWidth) / 2, startY, btnWidth, btnHeight, "Easy", false, setEasy });
    levelButtons.push_back({ (windowWidth - btnWidth) / 2, startY + btnHeight + btnSpacing, btnWidth, btnHeight, "Medium", false, setMedium });
    levelButtons.push_back({ (windowWidth - btnWidth) / 2, startY + 2 * (btnHeight + btnSpacing), btnWidth, btnHeight, "Hard", false, setHard });
    levelButtons.push_back({ (windowWidth - btnWidth) / 2, startY + 3 * (btnHeight + btnSpacing), btnWidth, btnHeight, "Back", false, goBack });

    scoreButtons.clear();
    scoreButtons.push_back({ (windowWidth - btnWidth) / 2, windowHeight - btnHeight - 50, btnWidth, btnHeight, "Back", false, goBack });

    setMedium();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("2D Racing Game - OpenGL/GLUT");

    initGame();

    #ifdef _WIN32
    atexit(cleanup);
    #endif

    glutDisplayFunc(drawScene);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUpFunc);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseClick);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(mouseMotion);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}