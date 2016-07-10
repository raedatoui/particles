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



vec3 getPole(int offset) {
  
//  float x = uMousePositions[0];
//  offset++;
//  float y = uMousePositions[1];
  return vec3(uMousePositions[0],uMousePositions[1],0.0);
}

void main()
{
  position =  iPosition;
  pposition = iPPostion;
  damping =   iDamping;
  home =      iHome;
//  color =     iColor;
  index =     iIndex;
  
//  int offset = int(mod(index, 2.0)) * 3;
  int offset = 3;

  vec3 uMousePos = getPole(offset);
  color = vec4(uMousePos, 1.0);
  //mouse interaction
  if( uMouseForce > 0.0 ) {
      vec3 dir = position - uMousePos;
      float d2 = length( dir );
      d2 *= d2;
      position -= uMouseForce * dir / d2;
  }

  vec3 vel = (position - pposition) * damping;
  pposition = position;
  vec3 acc = (home - position) * 100.0f;
  position += vel + acc * dt2;
  
  //color = iColor + vec4(acc, 1.0) * vec4(vel, uMouseForce);

}
