#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <raymath.h>
#include <math.h>
#include "include/config.h"

#define VEC_EPS 0.0001f
#define GLIDE_DEBUG

typedef struct Enemy {
    int id;
    Vector2 pos;
    Vector2 target_pos;
    bool isAlive;
    float radius;

    int gridX;
    int gridY;

    float move_speed;
    float health;
    float max_health;
} Enemy;

typedef struct Chunk {
    Enemy *enemies[MAX_ENEMY_PER_CHUNK];
    int enemyCount;
} Chunk;

typedef enum {
    STATE_IDLE,
    STATE_MOVE,
    STATE_CHASE,
    STATE_ATTACK

} PlayerState;
typedef struct Player {
    Vector2 pos;
    Color color;
    Vector2 target_pos;
    Vector2 look_dir;
    float move_speed;
    float turn_speed;
    float attack_range;

    Enemy *target_enemy;
    PlayerState current_state;

    double last_aa;
    bool is_aa;
    float attack_speed;
    float ad;
} Player;

typedef struct State {
    Player *player;
    Camera2D *camera;
    Vector2 screen;
} State;

void process_input(State *state);
void physics_update(State *state, double dt);
void late_update(State *state, double dt);

void summon_enemies(int count);
void draw_move_rec(int type, Vector2 pos);

void update_enemy_chunk(Enemy *enemy);

inline void stop_player(Player *player) {
    player->target_pos = player->pos;
}
inline bool IsValidChunk(int x, int y) {
    return (x >= 0 && x < TOTAL_CHUNKS_X && y >= 0 && y < TOTAL_CHUNKS_Y);
}
void start_attack(Player *p, Enemy *e);
Enemy *find_nearest_enemy(Vector2 point_pos, float search_radius);

Chunk world_grid[TOTAL_CHUNKS_X][TOTAL_CHUNKS_Y];
Enemy all_enemy[MAX_ENEMY];
int active_enemy_count = 0;
int alive_enemy_count = 0;

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(S_WIDTH, S_HEIGHT, TITLE);

    Player player = {
        (Vector2){S_WIDTH/2.0f,S_HEIGHT/2.0f},
        BLACK,
        (Vector2){S_WIDTH/2.0f,S_HEIGHT/2.0f},
        (Vector2){0.0f,0.0f},
        300.0f,
        30.0f,
        300.f,
        NULL,
        0,
        0.0,
        false,
        2.2f,
        20.0f
    };

    Camera2D camera = { 0 };
    camera.offset = (Vector2){ S_WIDTH/2.0f, S_HEIGHT/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    State state = {&player, &camera, GetWorldToScreen2D(player.pos, camera)};

    SetTargetFPS(FPS);

    for(int dx = 0; dx < TOTAL_CHUNKS_X; dx++) {
        for(int dy = 0; dy < TOTAL_CHUNKS_Y; dy++) {
            world_grid[dx][dy].enemyCount = 0;
            for(int i = 0; i < MAX_ENEMY_PER_CHUNK; i++) {
                world_grid[dx][dy].enemies[i] = NULL;
            }
        }
    }

    summon_enemies(1000);
    double dt;
    while(!WindowShouldClose())
    {
        dt = GetFrameTime();
        process_input(&state);
        physics_update(&state, dt);
        late_update(&state, dt);

        Vector2 topLeft = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
        Vector2 bottomRight = GetScreenToWorld2D((Vector2){ GetScreenWidth(), GetScreenHeight() }, camera);

        int startChunkX = (int)(topLeft.x / CHUNK_SIZE);
        int startChunkY = (int)(topLeft.y / CHUNK_SIZE);
        int endChunkX = (int)(bottomRight.x / CHUNK_SIZE);
        int endChunkY = (int)(bottomRight.y / CHUNK_SIZE);

        startChunkX = Clamp(startChunkX - 1, 0, TOTAL_CHUNKS_X - 1);
        startChunkY = Clamp(startChunkY - 1, 0, TOTAL_CHUNKS_Y - 1);

        endChunkX = Clamp(endChunkX + 1, 0, TOTAL_CHUNKS_X - 1);
        endChunkY = Clamp(endChunkY + 1, 0, TOTAL_CHUNKS_Y - 1);


        BeginDrawing();
        ClearBackground(RAYWHITE);
            BeginMode2D(camera);
            // Draw chunks
            for (int y = startChunkY; y <= endChunkY; y++) {
                    for (int x = startChunkX; x <= endChunkX; x++) {
                        Vector2 chunk_pos = { (float)x * CHUNK_SIZE, (float)y * CHUNK_SIZE };
                        DrawRectangleLines(chunk_pos.x, chunk_pos.y, CHUNK_SIZE, CHUNK_SIZE, DARKGRAY);
                        DrawText(TextFormat("%d,%d", x, y), chunk_pos.x + 5, chunk_pos.y + 5, 10, LIGHTGRAY);
                        for (int i = 0; i < CHUNK_SIZE; i += 32) {
                            for (int j = 0; j < CHUNK_SIZE; j += 32) {
                                DrawRectangleLines(chunk_pos.x + i, chunk_pos.y + j, 32, 32, Fade(GRAY, 0.3f));
                            }
                        }
                        // Draw Enemy
                        for (int i = 0; i < world_grid[x][y].enemyCount; i++) {
                            Enemy* e = world_grid[x][y].enemies[i];
                            if (e->isAlive) {
                                DrawCircleV(e->pos, e->radius, RED);
                                if(e->health < 100.0f && e->health > EPSILON) {
                                    float start_pos = e->pos.x - 16.0f;
                                    float end_pos = e->pos.x + 16.0f;
                                    DrawLineEx(
                                            (Vector2) {start_pos, e->pos.y - 20.0f},
                                            (Vector2) {end_pos, e->pos.y - 20.0f},
                                            3.0f,
                                            GREEN);
                                    // Missing health (overlay the green one) 32 because of padding (16+16)
                                    DrawLineEx(
                                            (Vector2) {end_pos, e->pos.y - 20.0f},
                                            (Vector2) {end_pos - ((32 * (e->max_health - e->health)) / e->max_health), e->pos.y - 20.0f},

                                            3.0f,
                                            RED);
                                }
                            }
                        }
                    }
                }

            // Draw Player
            float rotation_angle = atan2f(player.look_dir.y, player.look_dir.x) * RAD2DEG;
            DrawPoly(player.pos, 3, 20.0f, rotation_angle, player.color);
            DrawCircleLinesV(player.pos, player.attack_range, Fade(BLACK,0.6f));
            
            if(player.is_aa)
            DrawLineV(player.pos, player.target_enemy->pos, GREEN);

#ifdef GLIDE_DEBUG
            // DEBUG ONLY
                DrawCircleV(player.target_pos, 5, GREEN);
                DrawLineV(player.pos, player.target_pos, Fade(GRAY, 0.5f));
                DrawCircleLines(player.pos.x, player.pos.y, 20.0f, RED);
#endif // GLIDE_DEBUG
            EndMode2D(); 
        DrawFPS(10, 10);
        char text[64];
        sprintf(text, "Enemies left: %d/%d", alive_enemy_count,active_enemy_count);
        DrawText(text, 30, 30, 32, BLACK);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

void process_input(State *state) {
    if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !IsKeyDown(KEY_A)) {
        state->player->target_enemy = NULL;
        state->player->target_pos = GetScreenToWorld2D(GetMousePosition(),*state->camera);
        state->player->target_pos.x = Clamp(state->player->target_pos.x, 0, CHUNK_SIZE*TOTAL_CHUNKS_X-1);
        state->player->target_pos.y = Clamp(state->player->target_pos.y, 0, CHUNK_SIZE*TOTAL_CHUNKS_Y-1);
        state->player->is_aa = false;
    }

    if(IsKeyPressed(KEY_A)) {
        state->player->is_aa = false;
        Enemy *nearest = find_nearest_enemy(GetScreenToWorld2D(GetMousePosition(), *state->camera), 1000.0f);
        if(nearest != NULL) {
            printf("Found enemy on: (%.3f, %.3f)\n", nearest->pos.x, nearest->pos.y);
            state->player->target_enemy = nearest;
        }
    }
}

void physics_update(State *state, double dt) {
    Player *p = state->player;
    if(p == NULL) {
        return;
    }
    float stop_distance = 5.0f;

    static Vector2 old_turn_dir = (Vector2){0.0f,0.0f};

    if (p->target_enemy != NULL && p->target_enemy->isAlive) {

        p->target_pos = p->target_enemy->pos;

        float dist_to_enemy = Vector2Distance(p->pos, p->target_enemy->pos);

        float attack_range = p->attack_range + p->target_enemy->radius;

        if (dist_to_enemy <= attack_range) {
            old_turn_dir = Vector2Normalize(Vector2Subtract(p->target_pos, p->pos));
            start_attack(p,p->target_enemy);
            // Skip movement
            goto A;
        }
    }
    float vec_dist = Vector2DistanceSqr(p->pos, p->target_pos);

    if (vec_dist > stop_distance * stop_distance) {
        old_turn_dir = p->look_dir;
        Vector2 dir = Vector2Subtract(p->target_pos, p->pos);
        dir = Vector2Normalize(dir);

        p->pos.x += dir.x * p->move_speed * dt;
        p->pos.y += dir.y * p->move_speed * dt;

        p->look_dir = Vector2Lerp(p->look_dir, dir, p->turn_speed * dt);
        p->look_dir = Vector2Normalize(p->look_dir);
    } else {
        if (p->target_enemy == NULL) {
            stop_player(p);
        }
    }
A:
    if(Vector2DistanceSqr(old_turn_dir, p->look_dir) > VEC_EPS) {
        printf("yo");
        p->look_dir = Vector2Lerp(p->look_dir, old_turn_dir, p->turn_speed * dt);
        p->look_dir = Vector2Normalize(p->look_dir);
    }
    else {
        p->look_dir = old_turn_dir;
    }
    alive_enemy_count = 0;
    for(int i = 0; i < active_enemy_count; i++) {
        if(all_enemy[i].isAlive) {
            alive_enemy_count++;
            Enemy *e = &all_enemy[i];
            e->target_pos = p->pos;
            Vector2 screen = GetWorldToScreen2D(e->pos, *state->camera);
            // Random movement

            e->pos.x += rand() % 2;
            e->pos.y += rand() % 2;
            e->pos.x -= rand() % 2;
            e->pos.y -= rand() % 2;

            if(screen.x >= 0 &&
                    screen.x <= GetScreenWidth() &&
                    screen.y >=0 &&
                    screen.y <= GetScreenHeight()) {
                float e_dist = Vector2Distance(e->pos, e->target_pos);

                if(e_dist > stop_distance) {
                    Vector2 dir = Vector2Subtract(e->target_pos, e->pos);
                    dir = Vector2Normalize(dir);
                    e->pos.x += dir.x * e->move_speed * dt;
                    e->pos.y += dir.y * e->move_speed * dt;
                } else {
                    //e->pos = e->target_pos;
                }
            }
            update_enemy_chunk(e);
        }
    }


}

void late_update(State *state, double dt) {
    state->camera->target = state->player->pos;
    state->camera->offset = (Vector2){GetScreenWidth()/2.0f, GetScreenHeight()/2.0f};
    state->screen = GetWorldToScreen2D(state->player->pos, *state->camera);
}

Enemy *find_nearest_enemy(Vector2 point_pos, float search_radius) {
    Enemy *closest_enemy = NULL;

    // square to avoid sqrt later (Vector2Distance())
    float distance_sq = search_radius * search_radius;

    int center_chunk_x = (int)(point_pos.x/CHUNK_SIZE);
    int center_chunk_y = (int)(point_pos.y/CHUNK_SIZE);

    int chunk_radius = (int)(search_radius / CHUNK_SIZE) + 1;

    int startX = fmax(0, center_chunk_x - chunk_radius);
    int endX = fmin(TOTAL_CHUNKS_X - 1, center_chunk_x + chunk_radius);
    int startY = fmax(0, center_chunk_y - chunk_radius);
    int endY = fmin(TOTAL_CHUNKS_Y - 1, center_chunk_y + chunk_radius);

    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            Chunk* currentChunk = &world_grid[x][y];
            for (int i = 0; i < currentChunk->enemyCount; i++) {
                Enemy* enemy = currentChunk->enemies[i];
                if (!enemy->isAlive) continue;
                float dx = enemy->pos.x - point_pos.x;
                float dy = enemy->pos.y - point_pos.y;
                float distSq = (dx * dx) + (dy * dy);

                if (distSq < distance_sq) {
                    distance_sq = distSq;
                    closest_enemy = enemy;
                }
            }
        }
    }

    return closest_enemy;
}

void summon_enemies(int count) {
    for(int i = 0; i < count; i++) {
        if(active_enemy_count >= MAX_ENEMY) break;

        float rx = (float)GetRandomValue(0, (int)CHUNK_SIZE*TOTAL_CHUNKS_X);
        float ry = (float)GetRandomValue(0, (int)CHUNK_SIZE*TOTAL_CHUNKS_Y);

        all_enemy[active_enemy_count].id = active_enemy_count;
        all_enemy[active_enemy_count].pos = (Vector2){rx,ry};
        all_enemy[active_enemy_count].isAlive = true;
        all_enemy[active_enemy_count].radius = 10.0f;
        all_enemy[active_enemy_count].move_speed = 120.0f;
        all_enemy[active_enemy_count].target_pos = all_enemy[active_enemy_count].pos;
        all_enemy[active_enemy_count].max_health = 100.0f;
        all_enemy[active_enemy_count].health = all_enemy[active_enemy_count].max_health;
        int chX = (int)(rx/CHUNK_SIZE);
        int chY = (int)(ry/CHUNK_SIZE);

        if(IsValidChunk(chX, chY)) {
            Chunk *chunk = &world_grid[chX][chY];
            if(chunk->enemyCount < MAX_ENEMY_PER_CHUNK) {
                chunk->enemies[chunk->enemyCount] = &all_enemy[active_enemy_count];
                chunk->enemyCount++;
                active_enemy_count++;
                printf("Spawned enemy on (%.3f, %.3f)\n",rx,ry);
            }
        }
    }
}

void start_attack(Player *p, Enemy *e) {
    printf("last aa: %.3f\n", p->last_aa);
    if (e != NULL && e->isAlive) {
        p->is_aa = true;
        stop_player(p);
        if(p->last_aa < GetTime() - 1/p->attack_speed) {
            printf("time delta: %.3f\n",p->last_aa + (1/p->attack_speed) - GetTime());
            if(p->is_aa) {
                p->is_aa = false;
                e->health -= p->ad;
                if(e->health <= EPSILON) {
                    e->isAlive = false;
                }
            }
            p->last_aa = GetTime();
        }

    }
}

void update_enemy_chunk(Enemy *e) {
    int new_x = (int)(e->pos.x / CHUNK_SIZE);
    int new_y = (int)(e->pos.y / CHUNK_SIZE);

    if(!IsValidChunk(new_x, new_y)) 
    {
        e->isAlive = false;
        return;
    }

    if (new_x != e->gridX || new_y != e->gridY) 
    {
        Chunk *old_c = &world_grid[e->gridX][e->gridY];
        for (int i = 0; i < old_c->enemyCount; i++) {
            if(old_c->enemies[i]->id == e->id) {
                // swap and pop O(1)
                old_c->enemies[i] = old_c->enemies[old_c->enemyCount - 1];
                old_c->enemyCount--;
                break;
            }
        }

        Chunk *new_c = &world_grid[new_x][new_y];
        if(new_c->enemyCount < MAX_ENEMY_PER_CHUNK) {
            new_c->enemies[new_c->enemyCount] = e;
            new_c->enemyCount++;

            e->gridX = new_x;
            e->gridY = new_y;
        }

    }

}
