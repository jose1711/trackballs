/* mainMode.cc
   The main operation mode of the game.

   Copyright (C) 2000  Mathias Broxvall
                       Yannick Perret

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "mainMode.h"

#include "enterHighScoreMode.h"
#include "font.h"
#include "game.h"
#include "gamer.h"
#include "map.h"
#include "player.h"
#include "settings.h"
#include "sign.h"
#include "sound.h"

#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_timer.h>
#include <stdlib.h>
#include <string.h>

#define ENVIRONMENT_TEXTURE_SIZE 128

const int MainMode::statusBeforeGame = 0, MainMode::statusGameOver = 1,
          MainMode::statusInGame = 2;
const int MainMode::statusRestartPlayer = 3, MainMode::statusNextLevel = 4,
          MainMode::statusVictory = 5;
const int MainMode::statusLevelComplete = 6, MainMode::statusBonusLevelComplete = 7;
const int MainMode::statusPaused = 8;
// SDL_Surface *MainMode::panel,*MainMode::life,*MainMode::nolife;

MainMode *MainMode::mainMode = NULL;
uchar *MainMode::viewportData, *MainMode::environmentTextureData;

void MainMode::init() {
  mainMode = new MainMode();

  /* Reflection maps */
  viewportData = NULL;
  environmentTextureData =
      (uchar *)malloc(sizeof(uchar) * 4 * ENVIRONMENT_TEXTURE_SIZE * ENVIRONMENT_TEXTURE_SIZE);
}
MainMode::MainMode() {
  flash = 0;
  zAngle = 0.;
  wantedZAngle = 0.;
  xyAngle = 0.;
  wantedXYAngle = 0.;
  memset(camFocus, 0, sizeof(camFocus));
  memset(camDelta, 0, sizeof(camDelta));
  gameStatus = 0;
  statusCount = 0.;
  time = 0.;
  this->go_to_pause = 0;
  this->pause_time = 0.;
  memset(&this->cameraModelView[0], 0, 16 * sizeof(double));
  memset(&this->cameraProjection[0], 0, 16 * sizeof(double));
  mapname = NULL;
}
MainMode::~MainMode() {}

void MainMode::display() {
  Map *map = Game::current ? Game::current->map : NULL;
  Player *player1 = Game::current ? Game::current->player1 : NULL;

  glViewport(0, 0, screenWidth, screenHeight);

  if (Game::current->fogThickness)
    glClearColor(Game::current->fogColor[0], Game::current->fogColor[1],
                 Game::current->fogColor[2], Game::current->fogColor[3]);
  else
    glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (Game::current->fogThickness && Settings::settings->gfx_details != GFX_DETAILS_NONE) {
    activeView.fog_enabled = 1;
    assign(Game::current->fogColor, activeView.fog_color);
    activeView.fog_start = std::max(0.0, 14.0 - 7.0 * Game::current->fogThickness);
    activeView.fog_end = 26.0 - 4.0 * Game::current->fogThickness;
  } else
    activeView.fog_enabled = 0;

  perspectiveMatrix(40, (GLdouble)screenWidth / (GLdouble)std::max(screenHeight, 1), 0.1, 200,
                    activeView.projection);

  /* Setup matrixes for the camera perspective */
  if (gameStatus == statusBeforeGame) {
    lookAtMatrix(map->startPosition[0] - 12 * sin(time),
                 map->startPosition[1] - 12 * cos(time), 10.0 + map->startPosition[2],
                 map->startPosition[0], map->startPosition[1], map->startPosition[2], 0.0, 0.0,
                 1.0, activeView.modelview);

  } else {
    double angle = xyAngle * M_PI / 2. + M_PI / 4.;
    Coord3d up;
    up[0] = sin(angle) * zAngle;
    up[1] = cos(angle) * zAngle;
    up[2] = 1.0 - zAngle;
    normalize(up);
    lookAtMatrix(camFocus[0] - 10. * sin(angle) * cos(zAngle * M_PI / 2.),
                 camFocus[1] - 10. * cos(angle) * cos(zAngle * M_PI / 2.),
                 10.0 + camFocus[2] * 0.5 + (10.0 + camFocus[2]) * sin(zAngle * M_PI / 2.),
                 camFocus[0], camFocus[1], camFocus[2], up[0], up[1], up[2],
                 activeView.modelview);
  }

  /* record modelview/projection matrices for item visibility testing */
  assign(activeView.modelview, this->cameraModelView);
  assign(activeView.projection, this->cameraProjection);

  /* Some standard GL settings needed */
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  /* Debugging for problems with some cards */
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLfloat black[] = {0.0, 0.0, 0.0, 1.0};
  if (Game::current && Game::current->isNight) {
    GLfloat lightDiffuse2[] = {0.9, 0.9, 0.9, 1.0};
    Coord3d lightPosition2 = {Game::current->player1->position[0],
                              Game::current->player1->position[1],
                              Game::current->player1->position[2] + 2.0};
    assign(lightDiffuse2, activeView.light_diffuse);
    assign(lightDiffuse2, activeView.light_specular);
    assign(lightPosition2, activeView.light_position);
    assign(black, activeView.global_ambient);
    assign(black, activeView.light_ambient);
    activeView.quadratic_attenuation = 0.25;
  } else {
    GLfloat sunLight[3] = {0.8, 0.8, 0.8};
    Coord3d sunPosition = {-100.0, -50.0, 450.0};
    GLfloat ambient[3] = {0.2, 0.2, 0.2};
    assign(sunLight, activeView.light_diffuse);
    assign(sunLight, activeView.light_specular);
    assign(sunPosition, activeView.light_position);
    assign(black, activeView.global_ambient);
    assign(ambient, activeView.light_ambient);
    activeView.quadratic_attenuation = 0.;
  }

  /* Setup how we handle textures based on gfx_details */
  if (Settings::settings->gfx_details == 5) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }

  switch (gameStatus) {
  case statusBeforeGame:
    if (map) map->draw(0, (int)map->startPosition[0], (int)map->startPosition[1]);
    Game::current->draw();
    if (map) map->draw(1, (int)map->startPosition[0], (int)map->startPosition[1]);
    {
      const char *lp[3], *rp[3];
      lp[0] = _("Track:");
      rp[0] = Game::current->map->mapname;
      lp[1] = _("Author:");
      rp[1] = Game::current->map->author;
      lp[2] = _("Press spacebar to begin");
      rp[2] = " ";
      multiMessage(3, lp, rp);
    }
    break;
  case statusGameOver:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    {
      char str[256];
      snprintf(str, sizeof(str), _("Press spacebar to continue"));
      message(_("Game over"), str);
    }
    break;
  case statusRestartPlayer:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    message(_("Oops"), _("Press spacebar to continue"));
    break;
  case statusInGame:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    break;
  case statusPaused:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();

    Enter2DMode();
    // darken the display
    draw2DRectangle(0, 0, screenWidth, screenHeight, 0., 0., 1., 1., 0., 0., 0., pause_time);
    Font::drawCenterSimpleText(_("Paused"), screenWidth / 2, screenHeight / 2 - 16, 16, 1.0,
                               1.0, 1.0, 0.75);
    Leave2DMode();
    break;
  case statusBonusLevelComplete:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    message(_("Bonus level complete"), _("Press spacebar to continue"));
    break;
  case statusLevelComplete:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    showBonus();
    break;
  case statusNextLevel:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    message(_("Level complete"), _("Press spacebar to continue"));
    break;
  case statusVictory:
    map->draw(0, (int)camFocus[0], (int)camFocus[1]);
    Game::current->draw();
    map->draw(1, (int)camFocus[0], (int)camFocus[1]);
    showInfo();
    message(_("Congratulations"), _("You have completed the game!"));
    break;
  }

  Enter2DMode();
  if (player1->modTimeLeft[MOD_FROZEN]) {
    draw2DRectangle(0, 0, screenWidth, screenHeight, 0., 0., 1., 1., 0.5, 0.5, 1.0,
                    0.5 * std::min(1.0, (double)player1->modTimeLeft[MOD_FROZEN]));
  }
  displayFrameRate();
  /* Print joystick debugging information */
  if (debug_joystick && Settings::settings->hasJoystick()) {
    char str[256];
    snprintf(str, 255, "Joy: %d, %d -> %.1f, %.1f", Settings::settings->joystickRawX(),
             Settings::settings->joystickRawY(), Settings::settings->joystickX(),
             Settings::settings->joystickY());
    Font::drawCenterSimpleText(str, screenWidth / 2, screenHeight - 16, 8, 0.6, 0.6, 0.6, 0.6);
  }

  Leave2DMode();
}
void MainMode::doExpensiveComputations() { Game::current->doExpensiveComputations(); }

void MainMode::key(int key) {
  Player *player1 = Game::current ? Game::current->player1 : NULL;

  switch (gameStatus) {
  case statusBeforeGame:
    if (key == ' ') startGame();
    break;
  case statusRestartPlayer:
    if (key == ' ') {
      restartPlayer();
      gameStatus = statusInGame;
    }
    break;
  case statusPaused:
    if (key != 'p' && key != 'P') go_to_pause = 0;
    // gameStatus = statusInGame;
    break;
  case statusInGame:
    if ((key == 'p') || (key == 'P')) {
      go_to_pause = 1;
      pause_time = 0.;
      gameStatus = statusPaused;
      break;
    }
    if (key == SDLK_PRINTSCREEN) {
      createSnapshot();
      break;
    }
    player1->key(key);
    break;
  case statusBonusLevelComplete:
  case statusNextLevel:
    if (key == ' ') {
      if (!Game::current->nextLevel[0]) {
        gameStatus = statusVictory;
        break;
      }
      xyAngle = wantedXYAngle = zAngle = wantedZAngle = 0.0;
      Game::current->fogThickness = Game::current->wantedFogThickness;
      Game::current->clearLevel();
      Game::current->loadLevel(Game::current->nextLevel);
      Game::current->player1->newLevel();
      restartPlayer();
      gameStatus = statusBeforeGame;
    }
    break;
  case statusGameOver: {
    /*	  int buyCost;
    buyCost=(Game::current->player1->score/200)*100+500;
    if(Game::current->player1->score >= buyCost && key == 'y') {
          restartPlayer();
          player1->lives++;
          player1->timeLeft += 30;
          gameStatus=statusInGame;
          new ScoreSign(-buyCost,player1->position);
          } else  */
    if (key == ' ' || key == 'n') GameMode::activate(EnterHighScoreMode::enterHighScoreMode);
  } break;
  case statusVictory:
    if (key == ' ') {
      GameMode::activate(EnterHighScoreMode::enterHighScoreMode);
      Game::current->gamer->playerLose();
    }
    break;
  }
}
void MainMode::special(int /*key*/, int /*x*/, int /*y*/) {}
void MainMode::idle(Real td) {
  Player *player1 = Game::current ? Game::current->player1 : NULL;
  //   Map *map = Game::current ? Game::current->map : NULL;
  double t;

  time += td;
  flash -= td;
  if (flash < 0.0) flash = 0.0;

  switch (gameStatus) {
  case statusBeforeGame:
    SDL_SetRelativeMouseMode(SDL_TRUE);
    break;
  case statusGameOver: /* rotate view? */
    SDL_SetRelativeMouseMode(SDL_TRUE);
    Game::current->tick(td);
    break;
  case statusLevelComplete: {
    SDL_SetRelativeMouseMode(SDL_TRUE);
    int old_status = (int)statusCount;
    statusCount += 0.8 * td;
    Player *player = Game::current->player1;
    if ((int)statusCount != old_status) {
      if ((int)statusCount <= 3) playEffect(SFX_GOT_FLAG);
      switch ((int)statusCount) {
      case 1:
        player->score += player->lives * 100;
        break;
      case 2:
        player->score += (player->timeLeft / 60) * 250;
        break;
      case 3:
        player->score += Settings::settings->difficulty * 500;
        break;
      }
    }
    if (statusCount > 5.0) {
      playEffect(SFX_LV_COMPLETE);
      if (Game::current->nextLevel[0])
        gameStatus = statusNextLevel;
      else
        gameStatus = statusVictory;
    }
  }
  // note. No break here!
  case statusNextLevel:
  case statusInGame:
    SDL_SetRelativeMouseMode(SDL_TRUE);
    if (wantedZAngle > zAngle)
      zAngle += std::min(0.4 * td, wantedZAngle - zAngle);
    else if (wantedZAngle < zAngle)
      zAngle -= std::min(0.4 * td, zAngle - wantedZAngle);
    /* take shortest path under modulo */
    if (abs(xyAngle + 4.f - wantedXYAngle) < abs(xyAngle - wantedXYAngle)) { xyAngle += 4.f; }
    if (abs(xyAngle - 4.f - wantedXYAngle) < abs(xyAngle - wantedXYAngle)) { xyAngle -= 4.f; }
    if (wantedXYAngle > xyAngle)
      xyAngle += std::min(0.4 * td, wantedXYAngle - xyAngle);
    else if (wantedXYAngle < xyAngle)
      xyAngle -= std::min(0.4 * td, xyAngle - wantedXYAngle);

    Game::current->tick(td);
    for (t = td; t >= 0.0; t -= 0.01)
      for (int i = 0; i < 3; i++) {
        camDelta[i] = camDelta[i] * 0.9 + 0.002 * (player1->position[i] - camFocus[i]);
        camFocus[i] += camDelta[i];
      }
    break;
  case statusPaused:
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if (go_to_pause) {
      if (pause_time > 0.5) {
        pause_time = 0.5;
        break;
      }
      pause_time += td;
    } else {
      if (pause_time <= 0.) {
        pause_time = 0.;
        gameStatus = statusInGame;
        break;
      }
      pause_time -= td;
    }
    break;
  case statusRestartPlayer:
    SDL_SetRelativeMouseMode(SDL_TRUE);
    Game::current->tick(td);
    //    assign(map->startPosition,camFocus); zero(camDelta);
    break;
  }
}
void MainMode::activated() {
  zAngle = wantedZAngle = 0.0;
  xyAngle = wantedXYAngle = 0.0;
  zero(camFocus);
  zero(camDelta);
  gameStatus = statusBeforeGame;
  SDL_SetRelativeMouseMode(SDL_TRUE);

  camFocus[0] = Game::current->map->startPosition[0] - 5;
  camFocus[1] = Game::current->map->startPosition[1] - 5;
  time = 0.0;
  flash = 0.0;
}
void MainMode::deactivated() {
  SDL_SetRelativeMouseMode(SDL_FALSE);

  free(viewportData);
  viewportData = NULL;
}
void MainMode::playerLose() {
  Game::current->gamer->playerLose();
  gameStatus = statusGameOver;
}
void MainMode::playerDie() { gameStatus = statusRestartPlayer; }

/* Pass along, nothing to see here... */
void MainMode::mouse(int state, int x, int y) {
  if (Game::current) Game::current->player1->mouse(state, x, y);
}
void MainMode::mouseDown(int button, int /*x*/, int /*y*/) {
  if (button == SDL_BUTTON_LMASK) key(' ');
}
/* Starts the current game */
void MainMode::startGame() {
  gameStatus = statusInGame;
  Game::current->player1->hasWon = 0;
  playEffect(SFX_START);
  Coord3d pos;
  assign(Game::current->map->startPosition, pos);
  Game::current->gamer->levelStarted();

  pos[2] += 2.0;
  new Sign(_("Good luck!"), 6, 1.0, 60.0, pos);
  Game::current->player1->position[2] += 1.0;
  Game::current->player1->triggerHook(GameHookEvent_Spawn, NULL);
}

/* (Re)starts the current player */
void MainMode::restartPlayer() {
  Player *player1 = Game::current ? Game::current->player1 : NULL;
  player1->restart(Game::current->map->startPosition);
  camFocus[0] = Game::current->map->startPosition[0];
  camFocus[1] = Game::current->map->startPosition[1];
  Game::current->gamer->levelStarted();

  Coord3d pos;
  assign(Game::current->map->startPosition, pos);
  pos[2] += 2.0;
  new Sign(_("Good luck!"), 7, 1.0, 60.0, pos);
  player1->triggerHook(GameHookEvent_Spawn, NULL);
}

/* Shows various information on screen */
void MainMode::showInfo() {
  if (!Game::current) return;
  Player *player = Game::current->player1;

  /* Don't draw the panel if we have released the cursor etc. This is
     usefull for screenshots. */
  if (SDL_GetModState() & KMOD_CAPS) return;

  Enter2DMode();

  // the panel
  draw2DRectangle(0, 0, 256, 128, 0., 0., 1., 1., 1., 1., 1., 1.,
                  textures[loadTexture("left_panel.png")]);

  // lives
  for (int i = 0; i < 4; i++) {
    const char *name = (i < player->lives) ? "life.png" : "nolife.png";
    draw2DRectangle(63 + i * 20, 7, 32, 32, 0., 0., 1., 1., 1., 1., 1., 1.,
                    textures[loadTexture(name)]);
  }

  // Score
  char str[256];
  snprintf(str, sizeof(str), "%d", player->score);
  Font::drawRightSimpleText(str, 152, 52, 8, 0.0, 0.0, 0.0, 1.0);

  // Time left
  snprintf(str, sizeof(str), "%2.2d:%2.2d", player->timeLeft / 60, player->timeLeft % 60);
  if (player->timeLeft < 15 && player->timeLeft % 2)
    Font::drawRightSimpleText(str, 152, 81, 8, 1.0, 1.0, 1.0, 1.0);
  else
    Font::drawRightSimpleText(str, 152, 81, 8, 0.0, 0.0, 0.0, 1.0);

  // Health
  draw2DRectangle(8, 3, 144, 5, 0., 0., 1., 1., 0.5, 0.1, 0.1, 1.0);

  GLfloat vertices[4][2] = {{8.f, 3.f},
                            {8.f + (144.f * (GLfloat)player->health), 3.f},
                            {8.f, 8.f},
                            {8.f + (144.f * (GLfloat)player->health), 8.f}};
  GLfloat texco[4][2] = {
      0., 0., 0., 0., 0., 0., 0., 0.,
  };
  GLfloat cols[4][4] = {
      {0.8, 0.8, 0.1, 1.0}, {0.8, 0.1, 0.1, 1.0}, {0.8, 0.8, 0.1, 1.0}, {0.8, 0.1, 0.1, 1.0}};
  draw2DQuad(vertices, texco, cols);

  // Oxygen
  draw2DRectangle(8, 3, (int)(144.0 * (1. - player->oxygen)), 5, 0., 0., 1., 1., 0.2, 0.2, 0.7,
                  0.8);

  Leave2DMode();
  return;
}
void MainMode::levelComplete() {
  Game::current->player1->hasWon = 1;
  gameStatus = statusLevelComplete;
  statusCount = 0.0;
}
void MainMode::showBonus() {
  char left[16][256];
  char right[16][256];
  const char *left_pointers[16];
  const char *right_pointers[16];
  int difficulty = Settings::settings->difficulty;
  Player *player = Game::current->player1;

  for (int i = 0; i < 16; i++) {
    left_pointers[i] = &left[i][0];
    right_pointers[i] = &right[i][0];
  }

  snprintf(left[0], 255, _("Level bonuses"));
  snprintf(right[0], 255, " ");
  snprintf(left[1], 255, _("Lives left:"));
  snprintf(right[1], 255, _("%d x 100 = %d points"), player->lives, player->lives * 100);
  snprintf(left[2], 255, _("Time left:"));
  snprintf(right[2], 255, _("%d minutes = %d points"), player->timeLeft / 60,
           (player->timeLeft / 60) * 250);
  snprintf(left[3], 255, _("Difficulty:"));
  snprintf(right[3], 255, "%s = %d points",
           difficulty == 0 ? _("easy") : (difficulty == 1 ? _("normal") : _("hard")),
           difficulty * 500);

  multiMessage(std::min((int)statusCount + 1, 4), left_pointers, right_pointers);
}
void MainMode::bonusLevelComplete() {
  gameStatus = statusBonusLevelComplete;
  strcpy(Game::current->nextLevel, Game::current->returnLevel);
}

void MainMode::renderEnvironmentTexture(GLuint texture, Coord3d focus) {
  // Save gfx_details and change before drawing environment */
  int gfx_details = Settings::settings->gfx_details;
  Settings::settings->gfx_details = GFX_DETAILS_MINIMALISTIC;

  static int currentViewportSize = 512;
  static double currentTime = 0.0;
  if (currentTime > 0.05)
    currentViewportSize = std::max(128, currentViewportSize / 2);
  else if (currentTime < 0.05 / 4.0)
    currentViewportSize = std::min(512, currentViewportSize * 2);

  int viewportSize = currentViewportSize;
  if (screenHeight < 512) viewportSize = screenHeight;
  // viewportSize=256;

  double t0 = ((double)SDL_GetTicks()) / 1000.0;

  glViewport(0, 0, viewportSize, viewportSize);
  if (Game::current->fogThickness)
    glClearColor(Game::current->fogColor[0], Game::current->fogColor[1],
                 Game::current->fogColor[2], Game::current->fogColor[3]);
  else
    glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (Game::current->fogThickness && Settings::settings->gfx_details != GFX_DETAILS_NONE) {
    activeView.fog_enabled = 1;
    activeView.fog_start = std::max(0.0, 14.0 - 7.0 * Game::current->fogThickness);
    activeView.fog_end = std::max(0.0, 14.0 - 7.0 * Game::current->fogThickness);
    assign(Game::current->fogColor, activeView.fog_color);
  } else
    activeView.fog_enabled = 0;
  perspectiveMatrix(140, (GLdouble)screenWidth / (GLdouble)std::max(screenHeight, 1), 0.01,
                    200, activeView.projection);

  /* Setup openGL matrixes for the camera perspective */
  double angle = xyAngle * M_PI / 2. + M_PI / 4.;
  // TODO. Fixme. This computation is wrong when zAngle > 0.0 !!
  Coord3d up;
  up[0] = sin(angle) * zAngle;
  up[1] = cos(angle) * zAngle;
  up[2] = 1.0 - zAngle;
  normalize(up);
  lookAtMatrix(focus[0], focus[1], focus[2] + 0.0,
               focus[0] - 10. * sin(angle) * cos(zAngle * M_PI / 2.),
               focus[1] - 10. * cos(angle) * cos(zAngle * M_PI / 2.),
               zAngle > 0.0
                   ? 10.0 + focus[2] * 0.5 + (10.0 + focus[2]) * sin(zAngle * M_PI / 2.)
                   : focus[2] + 2.0,
               up[0], up[1], up[2], activeView.modelview);

  /* Some standard GL settings needed */
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  GLfloat lightDiffuse[3] = {0.9, 0.9, 0.9};
  GLfloat black[3] = {0.0, 0.0, 0.0};
  Coord3d lightPosition = {-100.0, -50.0, 450.0};
  GLfloat ambient[3] = {0.2, 0.2, 0.2};
  assign(lightDiffuse, activeView.light_diffuse);
  assign(ambient, activeView.light_ambient);
  assign(lightDiffuse, activeView.light_specular);
  assign(lightPosition, activeView.light_position);
  assign(black, activeView.global_ambient);
  activeView.quadratic_attenuation = 0.0;

  if (Game::current && Game::current->isNight) {
    GLfloat lightDiffuse2[3] = {0.9, 0.9, 0.9};
    Coord3d lightPosition2 = {Game::current->player1->position[0],
                              Game::current->player1->position[1],
                              Game::current->player1->position[2] + 2.0};
    assign(black, activeView.global_ambient);
    assign(black, activeView.light_ambient);
    assign(lightDiffuse2, activeView.light_diffuse);
    assign(lightDiffuse2, activeView.light_specular);
    assign(lightPosition2, activeView.light_position);
    activeView.quadratic_attenuation = 0.25;
  }

  /* Setup how we handle textures based on gfx_details */
  if (Settings::settings->gfx_details == 5) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }

  Map *map = Game::current ? Game::current->map : NULL;

  map->draw(0, (int)focus[0] + 10, (int)focus[1] + 10);
  Game::current->drawReflection(focus);
  // map->draw(birdsEye,1, (int) focus[0]+10,(int) focus[1]+10);

  /* Copy rendered image into viewportData */
  if (!viewportData) {
    viewportData = (uchar *)malloc(sizeof(uchar) * screenWidth * screenHeight * 3);
  }
  // approx 1-2 ms */
  glReadPixels(0, 0, viewportSize, viewportSize, GL_RGB, GL_UNSIGNED_BYTE,
               (GLvoid *)viewportData);

  // Converts viewport data to fisheye environment perspective
  // approx 10 ms */
  // double t0_5 = ((double)SDL_GetTicks()) / 1000.0;
  convertToFisheye(environmentTextureData, viewportData, viewportSize);

  /* Takes approx 1-2 ms */
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0,
               GL_RGBA,  // GL_RGBA
               ENVIRONMENT_TEXTURE_SIZE, ENVIRONMENT_TEXTURE_SIZE, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, environmentTextureData);
  double t1 = ((double)SDL_GetTicks()) / 1000.0;
  currentTime = t1 - t0;
  // printf("SIZE: %d, TD: %f + %f = %f\n",viewportSize,t0_5-t0,t1-t0_5,t1-t0);

  // Restore graphics details
  Settings::settings->gfx_details = gfx_details;
}

/* Creates a 128x128 fisheye texture from viewport data */
void MainMode::convertToFisheye(uchar *data, uchar *viewport, int viewportSize) {
  /* TODO. Let the environment map sample multiple viewport pixels.
         Encode an alpha value directly in the translationmap and let the alpha value increase
         as the angle away from the observer increases.
  */
  static short *translationMap = NULL;
  static short translationViewportSize = 0;
  if (!translationMap)
    translationMap = (short *)malloc(sizeof(short) * ENVIRONMENT_TEXTURE_SIZE *
                                     ENVIRONMENT_TEXTURE_SIZE * 2);
  if (translationViewportSize != viewportSize) {
    translationViewportSize = viewportSize;
    /* Create mapping between fisheye texture and viewport */
    int x, y, x2, y2;
    for (x = 0; x < ENVIRONMENT_TEXTURE_SIZE; x++) {
      double nx = (x - ENVIRONMENT_TEXTURE_SIZE / 2) * 2.0 / ENVIRONMENT_TEXTURE_SIZE;
      for (y = 0; y < ENVIRONMENT_TEXTURE_SIZE; y++) {
        double ny = (y - ENVIRONMENT_TEXTURE_SIZE / 2) * 2.0 / ENVIRONMENT_TEXTURE_SIZE;
        double nz = 1.0 - nx * nx - ny * ny;
        if (nz > 0.0) {
          nz = sqrt(nz);
          double dotprod = 2.0 * nz;
          double vx = -2.0 * nx;
          double vy = -2.0 * ny;
          double vz = 1 - 2.0 * nz;
          double h = 0.05 / vz;
          double fx2 = -h * vx;
          double fy2 = +h * vy;
          if (dotprod < 1.0) {
            fy2 = -fy2;
            fx2 = -fx2;
          }
          if (fx2 > 0.5) {
            fx2 = 0.5;
            fy2 = (fy2 / fx2) * 0.5;
          } else if (fx2 < -0.5) {
            fx2 = -0.5;
            fy2 = (fy2 / -fx2) * 0.5;
          }
          if (fy2 > 0.5) {
            fy2 = 0.5;
            fx2 = (fx2 / fy2) * 0.5;
          } else if (fy2 < -0.5) {
            fy2 = -0.5;
            fx2 = (fx2 / -fy2) * 0.5;
          }
          x2 = (int)(viewportSize * (fx2 * 0.99 + 0.5));
          y2 = (int)(viewportSize * (fy2 * 0.99 + 0.5));

          translationMap[(x + y * ENVIRONMENT_TEXTURE_SIZE) * 2] = x2;
          translationMap[(x + y * ENVIRONMENT_TEXTURE_SIZE) * 2 + 1] = y2;
        } else {
          translationMap[(x + y * ENVIRONMENT_TEXTURE_SIZE) * 2 + 0] = -1;
          translationMap[(x + y * ENVIRONMENT_TEXTURE_SIZE) * 2 + 1] = 0;
        }
      }
    }
  }

  int size = ENVIRONMENT_TEXTURE_SIZE * ENVIRONMENT_TEXTURE_SIZE;
  for (int i = 0; i < size; i++) {
    int x2 = translationMap[i * 2 + 0];
    int y2 = translationMap[i * 2 + 1];
    if (x2 == -1)
      data[i * 4 + 3] = 0;
    else {
      uchar *viewportPntr = &viewport[(x2 + y2 * viewportSize) * 3];
      for (int j = 0; j < 3; j++) data[i * 4 + j] = viewportPntr[j];
      data[i * 4 + 3] = 255;
    }
  }
}
