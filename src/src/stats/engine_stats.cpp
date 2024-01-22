#include<stats/engine_stats.h>

void EngineStats::add_frametime(float frametime)
{
    this->frametime += frametime;
}

void EngineStats::add_triangle_count(int triangle_count){
    this->triangle_count += triangle_count;
}

void EngineStats::add_drawcall_count(int drawcall_count){
    this->drawcall_count += drawcall_count;
}

void EngineStats::add_scene_update_time(float scene_update_time){
    this->scene_update_time += scene_update_time;
}

void EngineStats::add_mesh_draw_time(float mesh_draw_time){
    this->mesh_draw_time += mesh_draw_time;
}