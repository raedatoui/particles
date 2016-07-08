uniform sampler2D uTex;

in vec2			Position;
out highp vec4 	oColor;

void main( void )
{
  oColor = texture( uTex, Position );
  //oColor = vec4(0.4, 1.0, 0.5, 1.0);
}
