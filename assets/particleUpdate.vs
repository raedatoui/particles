#version 150 core

uniform float uMouseForce;
uniform vec3 uMousePosition;
uniform sampler2D uTex;
uniform sampler2D uTex2;
uniform float uTexBlend;
uniform float delta;


in vec3  iPosition;
in vec3  iPPostion;
in vec3  iHome;
in float iDamping;
in vec4  iColor;
in vec2  iPixel;

out vec3  position;
out vec3  pposition;
out vec3  home;
out float damping;
out vec4  color;
out vec2  pixel;

const float dt2 = 1.0 / (60.0 * 60.0);


#include "./PhotoshopMathFP.glsl"



void moveToPole() {
  //mouse interaction
  float dist = distance(uMousePosition, position);
  if( uMouseForce > 0.0 && ( (delta < 2.0 && dist > 50.0f*delta) || delta > 5.0) )  {
    vec3 dir = position - uMousePosition;
    float d2 = length( dir );
    d2 *= d2;
    position -= uMouseForce * dir / d2;
  }
}

vec4 getPixelColor() {
  vec4 color1 = texture(uTex, iPixel);
  vec4 color2 = texture(uTex2, iPixel);
  color1.rgb = vec3( delta / 2.0 ) - color1.rgb;
  color2.a = 0.5;
  vec4 r = vec4(BlendPhoenix(color1.rgb, color2.rgb), 1.0);
  vec4 colorOut = mix(color1, r, color1.a * uTexBlend);
  return colorOut;
}

void main()
{
  position =  iPosition;
  pposition = iPPostion;
  damping =   iDamping;
  home =      iHome;
  pixel =     iPixel;
  color =  getPixelColor();


  moveToPole();
  float dist2 = distance(uMousePosition, position);  
//  if( dist2 > 500.0f * (1.0 - delta)) {
    vec3 vel = (position - pposition) * damping;
    pposition = position;
    vec3 acc = (home - position) * 128.0f;
    position += acc * dt2;
//}

}
