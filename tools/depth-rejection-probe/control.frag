#version 450
layout(location=0) in vec3 colour;
layout(location=0) out vec4 outputColour;
void main() {
    // Physically no discard, depth output, storage writes, atomics or sample
    // masking. Nonconstant arithmetic makes shading rejection measurable.
    float v=gl_FragCoord.x*.001+gl_FragCoord.y*.003;
    for(int i=0;i<8;++i)v=sin(v*1.17+float(i))*.5+.5;
    outputColour=vec4(colour*.9+vec3(v*.05),1);
}
