#version 150 core

uniform float uMouseForce;
uniform vec3 uMousePositions[2];
uniform sampler2D uTex;

in vec3   iPosition;
in vec3   iPPostion;
in vec3   iHome;
in float  iDamping;
in vec4   iColor;
in vec2  iPixel;

out vec3  position;
out vec3  pposition;
out vec3  home;
out float damping;
out vec4  color;
out vec2 pixel;

const float dt2 = 1.0 / (120.0 * 120.0);


vec3 selectPoleByIndex() {
  int offset = int(mod(iPixel[0], 2.0));
  return uMousePositions[offset];
}

vec3 selectPoleByColor() {
  vec3 rgb = color.rgb;
  int offset = 0;
  if (rgb.r + rgb.b + rgb.g > 2.5) {
    offset = 1;
  }
  return uMousePositions[offset];
}

void moveToPole(vec3 pole) {
  //mouse interaction
  if( uMouseForce > 0.0 ) {
    vec3 dir = position - pole;
    float d2 = length( dir );
    d2 *= d2;
    position -= uMouseForce * dir / d2;
  }
}

vec4 getPixelColor() {
  float x = iPixel[0]/1440.0;
  float y = 1.0 - iPixel[1]/880.0;
  return texture(uTex, vec2(x,y));
}

void main()
{
  position =  iPosition;
  pposition = iPPostion;
  damping =   iDamping;
  home =      iHome;
  pixel =     iPixel;
  color =     getPixelColor();

  vec3 pole = selectPoleByIndex();
  moveToPole(pole);
  
  vec3 vel = (position - pposition) * damping;
  pposition = position;
  vec3 acc = (home - position) * 150.0;
  position += vel + acc * dt2;
  

}
