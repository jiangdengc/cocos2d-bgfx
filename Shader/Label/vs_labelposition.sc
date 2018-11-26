$input a_position, a_texcoord0, a_color0
$output v_color0, v_texcoord0, v_position

#include "../bgfx_shader.sh"

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position.xyz, 1.0));
	v_color0 = a_color0;
	v_texcoord0 = a_texcoord0;
    v_position = a_position;
}