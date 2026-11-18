// art.h - procedural pixel art renderer
#pragma once
#include "gfx.h"
#include "player.h"
#include "world.h"

namespace rpg {
namespace art {

void tile(Framebuffer& fb, Tile t, int px, int py, int tx, int ty, float time);
void shadow(Framebuffer& fb, int cx, int cy, int rx, int ry, int alpha);
void player(Framebuffer& fb, int px, int py, int dir, float phase, bool moving, int weapon,
            int armor);
void playerScaled(Framebuffer& fb, int x, int y, int scale, int dir, float phase, bool moving,
                  int weapon, int armor);
void heroBack(Framebuffer& fb, int x, int y, int scale, float time, int weapon, int armor);
void npc(Framebuffer& fb, int px, int py, NpcStyle style, int dir, float time);
void chest(Framebuffer& fb, int px, int py, bool opened, float time);
void enemy(Framebuffer& fb, EnemyType type, int cx, int cy, float time, bool hurt);
void enemyWorld(Framebuffer& fb, EnemyType type, int px, int py, float time);
void wardenWorld(Framebuffer& fb, int px, int py, float time);

}  // namespace art
}  // namespace rpg
