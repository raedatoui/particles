#version 150 core

uniform float uMouseForce;
uniform vec3 uMousePositions[10];
uniform sampler2D uTex;
uniform sampler2D uTex2;

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

const float dt2 = 1.0 / (60.0 * 60.0);

#include "./PhotoshopMathFP.glsl"

vec3 selectPoleByIndex() {
  float m = iPixel[0] * 10.0;
  int offset = int(m);
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
  vec3 color1 = texture(uTex, iPixel).rgb;
  vec3 color2 = texture(uTex2, iPixel).rgb;
  vec4 colorOut = vec4(1.0);

  color1 = Desaturate(color1, 1.0).rgb;
  colorOut.rgb = color2;
  
  colorOut.rgb = mix(colorOut.rgb, BlendDarken(colorOut.rgb, color1), color1[0]);
  colorOut.rgb = mix(colorOut.rgb, BlendLighten(colorOut.rgb, color1), color1[1]);
  
  colorOut.rgb = mix(colorOut.rgb, BlendSoftLight(colorOut.rgb, color2), color2[0]);
  colorOut.rgb = mix(colorOut.rgb, BlendColor(colorOut.rgb, color2), color2[1]);
  return colorOut;
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
  
  vec3 vel = (position - pposition) * 0;
  pposition = position;
  vec3 acc = (home - position) * 500.0;
  position += vel + acc * dt2;
  

}
