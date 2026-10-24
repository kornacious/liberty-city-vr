#pragma once

#include <math.h>

namespace QuestBillboard {

// Im2D world sprites are projected by the game, then unprojected by rw_im2d.
// Rotate their camera-space offsets towards the centre's viewing ray before
// projecting them again. A shared view-parallel plane looks edge-on when the
// player looks past a sprite. This keeps its centre, size and texture rotation.
template <class Vertex>
void FaceViewer(Vertex *vertices, int count, float centreX, float centreY,
    float width, float height, float viewX, float viewY,
    const float *worldUp = nullptr, const float *worldEast = nullptr)
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
    // Build the artwork axes from world-up in CAMERA coordinates, rather
    // than from headset right/up. Facing alone does not remove head roll.
    const float ux0 = worldUp ? worldUp[0] : 0.0f;
    const float uy0 = worldUp ? worldUp[1] : 1.0f;
    const float uz0 = worldUp ? worldUp[2] : 0.0f;
    float rx = uy0*nz-uz0*ny;
    float ry = uz0*nx-ux0*nz;
    float rz = ux0*ny-uy0*nx;
    float rightLength2 = rx*rx+ry*ry+rz*rz;
    if(rightLength2 < 0.000001f){
        // At the zenith/nadir, world-up has no projection on the card.
        // Use a fixed world-east reference, so the fallback is roll-invariant.
        const float ex = worldEast ? worldEast[0] : 1.0f;
        const float ey = worldEast ? worldEast[1] : 0.0f;
        const float ez = worldEast ? worldEast[2] : 0.0f;
        const float along = ex*nx+ey*ny+ez*nz;
        rx = ex-along*nx; ry = ey-along*ny; rz = ez-along*nz;
        rightLength2 = rx*rx+ry*ry+rz*rz;
    }
    if(rightLength2 < 0.000001f)
        return;
    const float inverseRight = 1.0f/sqrtf(rightLength2);
    rx *= inverseRight; ry *= inverseRight; rz *= inverseRight;
    const float ux = ny*rz-nz*ry;
    const float uy = nz*rx-nx*rz;
    const float uz = nx*ry-ny*rx;
    // Avoid an undefined projection exactly on the camera plane. Vertices
    // behind it are valid: stereo GPU clipping handles those after unprojection.
    for(int i = 0; i < count; ++i){
        const float dx = (vertices[i].x-centreX)*pixelX*depth;
        const float dy = (centreY-vertices[i].y)*pixelY*depth;
        if(fabsf(depth+rz*dx+uz*dy) < 0.000001f)
            return;
    }
    for(int i = 0; i < count; ++i){
        const float dx = (vertices[i].x-centreX)*pixelX*depth;
        const float dy = (centreY-vertices[i].y)*pixelY*depth;
        const float z = depth+rz*dx+uz*dy;
        vertices[i].x = width*0.5f+(cx+rx*dx+ux*dy)/(pixelX*z);
        vertices[i].y = height*0.5f-(cy+ry*dx+uy*dy)/(pixelY*z);
        vertices[i].w = z;
        // Vulkan Im2D otherwise clamps shallow vertices to the HUD plane.
        // Screen Z is unused for world reconstruction; reserve negative Z
        // for these world vertices so close effects keep their actual depth.
        vertices[i].z = -1.0f;
    }
}

}
