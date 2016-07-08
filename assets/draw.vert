uniform sampler2D uTex;

uniform mat4	ciModelViewProjection;

in vec4			ciPosition;
out vec2		Position;

void main( void )
{
	gl_Position	= ciModelViewProjection * ciPosition;
	gl_PointSize = 1.0;
    Position    = vec2(ciPosition[0], ciPosition[1]);
}
