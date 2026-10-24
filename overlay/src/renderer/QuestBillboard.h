#pragma once

#include <math.h>

namespace QuestBillboard {

// Im2D world sprites are projected by the game, then unprojected by rw_im2d.
// Rotate their camera-space offsets towards the centre's viewing ray before
// projecting them again. A shared view-parallel plane looks edge-on when the
// player looks past a sprite. This keeps its centre, size and texture rotation.
template <class Vertex>
void FaceViewer(Vertex *vertices, int count, float centreX, float centreY,
    float width, float height, float viewX, float viewY)
{
    if(count <= 0 || width <= 0.0f || height <= 0.0f ||
       viewX <= 0.0f || viewY <= 0.0f)
        return;
    const float depth = vertices[0].w;
    if(!(depth > 0.0f))
        return;
    const float pixelX = 2.0f*viewX/width;
    const float pixelY = 2.0f*viewY/height;
    const float cx = (centreX-width*0.5f)*pixelX*depth;
    const float cy = (height*0.5f-centreY)*pixelY*depth;
    const float inverseLength = 1.0f/sqrtf(cx*cx+cy*cy+depth*depth);
    const float nx = cx*inverseLength, ny = cy*inverseLength;
    const float nz = depth*inverseLength;
    // Shortest-arc rotation from camera forward to the centre ray. No
    // world-up cross product: that would become singular looking overhead.
    const float k = 1.0f/(1.0f+nz);
    const float rx = 1.0f-nx*nx*k, ry = -nx*ny*k;
    const float uy = 1.0f-ny*ny*k;
    // Avoid an undefined projection exactly on the camera plane. Vertices
    // behind it are valid: stereo GPU clipping handles those after unprojection.
    for(int i = 0; i < count; ++i){
        const float dx = (vertices[i].x-centreX)*pixelX*depth;
        const float dy = (centreY-vertices[i].y)*pixelY*depth;
        if(fabsf(depth-nx*dx-ny*dy) < 0.000001f)
            return;
    }
    for(int i = 0; i < count; ++i){
        const float dx = (vertices[i].x-centreX)*pixelX*depth;
        const float dy = (centreY-vertices[i].y)*pixelY*depth;
        const float z = depth-nx*dx-ny*dy;
        vertices[i].x = width*0.5f+(cx+rx*dx+ry*dy)/(pixelX*z);
        vertices[i].y = height*0.5f-(cy+ry*dx+uy*dy)/(pixelY*z);
        vertices[i].w = z;
        // Vulkan Im2D otherwise clamps shallow vertices to the HUD plane.
        // Screen Z is unused for world reconstruction; reserve negative Z
        // for these world vertices so close effects keep their actual depth.
        vertices[i].z = -1.0f;
    }
}

}
