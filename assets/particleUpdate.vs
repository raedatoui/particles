#version 150 core

uniform float uMouseForce;
uniform vec3 uMousePositions[2];

in vec3   iPosition;
in vec3   iPPostion;
in vec3   iHome;
in float  iDamping;
in vec4   iColor;
in float    iIndex;

out vec3  position;
out vec3  pposition;
out vec3  home;
out float damping;
out vec4  color;
out float index;

const float dt2 = 1.0 / (60.0 * 60.0);



void main()
{
  position =  iPosition;
  pposition = iPPostion;
  damping =   iDamping;
  home =      iHome;
  color =     iColor;
  index =     iIndex;
  
  vec3 rgb = color.rgb;
  int offset = 0;
  if (rgb.r + rgb.b + rgb.g > 2.5) {
    offset = 1;
  }
  //offset = int(mod(index, 2.0));
  vec3 uMousePos = uMousePositions[offset];

  //mouse interaction
  
  if( uMouseForce > 0.0 ) {
      vec3 dir = position - uMousePos;
      float d2 = length( dir );
      d2 *= d2;
      position -= uMouseForce * dir / d2;
  }

  vec3 vel = (position - pposition) * damping;
  pposition = position;
  vec3 acc = (home - position) * 500.0f;
  position += vel + acc * dt2;
  

}
