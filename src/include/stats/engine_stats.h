#pragma once

class EngineStats {
public:
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;

    void add_frametime(float frametime);

    void add_triangle_count(int triangle_count);

    void add_drawcall_count(int drawcall_count);

    void add_scene_update_time(float scene_update_time);

    void add_mesh_draw_time(float mesh_draw_time);

    void reset() {
        frametime = 0;
        triangle_count = 0;
        drawcall_count = 0;
        scene_update_time = 0;
        mesh_draw_time = 0;
    }

};