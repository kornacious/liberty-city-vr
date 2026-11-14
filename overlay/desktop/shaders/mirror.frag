#version 450
layout(set=0,binding=0) uniform sampler2DArray eyeImage;
layout(push_constant) uniform Params {vec4 transform;vec4 options;} p;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec3 linear(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),greaterThan(c,vec3(0.04045)));}
void main(){color=texture(eyeImage,vec3(p.transform.xy+uv*p.transform.zw,p.options.x));if(p.options.y>0.5)color.rgb=linear(color.rgb);color.a=1;}
