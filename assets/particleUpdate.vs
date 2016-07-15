#version 150 core

uniform float uMouseForce;
uniform vec3 uMousePositions[10];
uniform sampler2D uTex;
uniform sampler2D uTex2;
uniform float attenuation;


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
const vec2 sample_offset = vec2(1.0, 0.0);

#include "./PhotoshopMathFP.glsl"

vec3 selectPoleByIndex() {
  float m = iPixel[0] * 10.0;
  int ind = int(m);
  return uMousePositions[ind];
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

vec4 blurPixel(vec3 sum)
{
  sum += texture( uTex, iPixel + -1.0 * sample_offset ).rgb * 0.009167927656011385;
  sum += texture( uTex, iPixel +  -0.9 * sample_offset ).rgb * 0.014053461291849008;
  sum += texture( uTex, iPixel +  -0.8 * sample_offset ).rgb * 0.020595286319257878;
  sum += texture( uTex, iPixel +  -0.7 * sample_offset ).rgb * 0.028855245532226279;
  sum += texture( uTex, iPixel +  -0.6 * sample_offset ).rgb * 0.038650411513543079;
  sum += texture( uTex, iPixel +  -0.5 * sample_offset ).rgb * 0.049494378859311142;
  sum += texture( uTex, iPixel +  -0.4 * sample_offset ).rgb * 0.060594058578763078;
  sum += texture( uTex, iPixel +  -0.3 * sample_offset ).rgb * 0.070921288047096992;
  sum += texture( uTex, iPixel +  -0.2 * sample_offset ).rgb * 0.079358891804948081;
  sum += texture( uTex, iPixel +  -0.1 * sample_offset ).rgb * 0.084895951965930902;
  sum += texture( uTex, iPixel +   0.0 * sample_offset ).rgb * 0.086826196862124602;
  sum += texture( uTex, iPixel +  +0.1 * sample_offset ).rgb * 0.084895951965930902;
  sum += texture( uTex, iPixel +  +0.2 * sample_offset ).rgb * 0.079358891804948081;
  sum += texture( uTex, iPixel +  +0.3 * sample_offset ).rgb * 0.070921288047096992;
  sum += texture( uTex, iPixel +  +0.4 * sample_offset ).rgb * 0.060594058578763078;
  sum += texture( uTex, iPixel +  +0.5 * sample_offset ).rgb * 0.049494378859311142;
  sum += texture( uTex, iPixel +  +0.6 * sample_offset ).rgb * 0.038650411513543079;
  sum += texture( uTex, iPixel +  +0.7 * sample_offset ).rgb * 0.028855245532226279;
  sum += texture( uTex, iPixel +  +0.8 * sample_offset ).rgb * 0.020595286319257878;
  sum += texture( uTex, iPixel +  +0.9 * sample_offset ).rgb * 0.014053461291849008;
  sum += texture( uTex, iPixel + + 1.0 * sample_offset ).rgb * 0.009167927656011385;
  return vec4(attenuation * sum, 1.0);

}
void main()
{
  position =  iPosition;
  pposition = iPPostion;
  damping =   iDamping;
  home =      iHome;
  pixel =     iPixel;
  color =     vec4(getPixelColor().rgb, 1.0);

  vec3 pole = selectPoleByIndex();
  moveToPole(pole);
  
  vec3 vel = (position - pposition) * 0;
  pposition = position;
  vec3 acc = (home - position) * 500.0;
  position += vel + acc * dt2;
  

}
