#version 450
layout(location=0) in vec2 pos;
layout(location=1) in vec2 inUV;
layout(location=2) in vec4 inColor;
layout(push_constant) uniform Params {vec4 transform;vec4 options;} p;
layout(location=0) out vec2 uv;
layout(location=1) out vec4 vertexColor;
void main(){uv=inUV;vertexColor=inColor;gl_Position=vec4(pos*p.transform.xy+p.transform.zw,0,1);}
