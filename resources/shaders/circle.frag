#version 330 core
out vec4 frag_color;

in VS_OUT {
    vec2 uv;
    vec3 coord;
} fs_in;

uniform vec3 center;
uniform vec3 color;
uniform float radius2;
uniform sampler2D tex_smp;
uniform bool textured; // Whenever use texture or not

void main() {
    vec3 rv = fs_in.coord - center;
    float cur_r2 = dot(rv, rv);
    if (cur_r2 > radius2)
        discard;
        
    vec4 solid = vec4(mix(color, color * 0.6, cur_r2 / radius2), 1.0);
    
    if (textured) {
        vec4 tex = texture(tex_smp, fs_in.uv);
        frag_color = mix(solid, tex, tex.a);
    } else {
        frag_color = solid;
    }
}