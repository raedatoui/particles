uniform mat4	ciModelViewProjection;
uniform float	pointSize;

in vec4			ciPosition;
in vec4			ciColor;

out lowp vec4	Color;

void main( void )
{
  gl_Position	= ciModelViewProjection * ciPosition;
  gl_PointSize  = pointSize;
  Color 		= ciColor;
}

