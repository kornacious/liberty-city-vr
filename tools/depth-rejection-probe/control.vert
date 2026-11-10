#version 450
#extension GL_EXT_multiview : require
layout(push_constant) uniform Params { vec4 shape; vec4 colour; } p;
layout(location=0) out vec3 colour;
void main() {
    const vec2 corner[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
    int grid=int(p.shape.y), cell=gl_VertexIndex/6;
    vec2 xy=(vec2(cell%grid,cell/grid)+corner[gl_VertexIndex%6])/float(grid)*2.0-1.0;
    // Both eyes deliberately have identical clip-space geometry. No camera,
    // tracking, textures, vertex buffers or game culling affect this control.
    gl_Position=vec4(xy+vec2(p.shape.z,0),p.shape.x,1);
    colour=p.colour.rgb;
}
