#version 450
#ifndef CONTROL_KILL
#define CONTROL_KILL 0
#endif
layout(location=0) in vec3 colour;
layout(location=0) out vec4 outputColour;
void main() {
    float alpha=.75;
#if CONTROL_KILL
    // A real half-covered cutout, not a dead/unreachable discard. Surviving
    // fragments have fractional alpha so blending is nontrivial as well.
    if(((int(gl_FragCoord.x/32)+int(gl_FragCoord.y/32))&1)==0)alpha=0;
    if(alpha<.5)discard;
#endif
    float v=gl_FragCoord.x*.001+gl_FragCoord.y*.003;
    for(int i=0;i<8;++i)v=sin(v*1.17+float(i))*.5+.5;
    outputColour=vec4(colour*.9+vec3(v*.05),alpha);
}
