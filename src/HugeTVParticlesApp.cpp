//
//  Copyright (c) 2014 David Wicks, sansumbrella.com
//  All rights reserved.
//
//  Particle Sphere sample application, GPU integration.
//
//  Author: David Wicks
//  License: BSD Simplified
//

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Rand.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

/**
 Particle type holds information for rendering and simulation.
 Used to buffer initial simulation values.
 */
struct Particle
{
    vec3  pos;
    vec3  ppos;
    vec3  home;
    ColorA  color;
    float damping;
};
typedef struct {
    double h;       // angle in degrees
    double s;       // percent
    double v;       // percent
} hsv;
typedef struct {
    double r;       // percent
    double g;       // percent
    double b;       // percent
} rgb;

static hsv   rgb2hsv(rgb in);
hsv rgb2hsv(rgb in)
{
    hsv         out;
    double      min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;

    out.v = max;                                // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0
        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
        if( in.g >= max )
            out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
        else
            out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan

    out.h *= 60.0;                              // degrees

    if( out.h < 0.0 )
        out.h += 360.0;

    return out;
}


/**
 Simple particle simulation with Verlet integration and mouse interaction.
 A sphere of particles is deformed by mouse interaction.
 Simulation is run using transform feedback on the GPU.
 particleUpdate.vs defines the simulation update step.
 Designed to have the same behavior as ParticleSphereCPU.
 */
class HugeTVParticlesApp : public App {
public:
  void setup() override;
  void update() override;
  void draw() override;
private:
  gl::GlslProgRef mRenderProg;
  gl::GlslProgRef mUpdateProg;

  // Descriptions of particle data layout.
  gl::VaoRef    mAttributes[2];
  // Buffers holding raw particle data on GPU.
  gl::VboRef    mParticleBuffer[2];

  // Current source and destination buffers for transform feedback.
  // Source and destination are swapped each frame after update.
  std::uint32_t mSourceIndex    = 0;
  std::uint32_t mDestinationIndex = 1;

  // Mouse state suitable for passing as uniforms to update program
  bool      mMouseDown = false;
  float     mMouseForce = 0.0f;
  vec3      mMousePos = vec3( 0, 0, 0 );
  int       GRID_SIZE = 10;
  int       NUM_PARTICLES = 0;
  gl::Texture2dRef		mTex;
};


void HugeTVParticlesApp::setup()
{
  // Create initial particle layout.
// How many particles to create. (600k default)

  GRID_SIZE = 734;

  NUM_PARTICLES = GRID_SIZE * GRID_SIZE;

  auto img = loadImage( loadAsset( "tumblr_mfcow4BlEI1qd3feno1_500-11.jpg" ) );
  mTex = gl::Texture2d::create( img );
  mTex->bind( 0 );

  
  vector<Particle> particles;
  particles.assign( NUM_PARTICLES, Particle() );

  for( int i = 0; i < GRID_SIZE; ++i )
  {
    for( int j = 0; j < GRID_SIZE; ++j ) {
      auto &p = particles.at( i*GRID_SIZE+j );
      // assign starting values to particles.
      // float x = radius * sin( inclination * i*j ) * cos( azimuth * i*j );
      // float y = radius * cos( inclination * i*j );
      // float z = radius * sin( inclination * i*j ) * sin( azimuth * i*j );

      p.pos = vec3( i+1, j+1, 0 );
      p.home = p.pos;
      p.ppos = p.home + Rand::randVec3() * 10.0f; // random initial velocity
      p.damping = 0.0f; //Rand::randFloat( 0.965f, 0.985f );
      p.color = Color( CM_RGB, 239.0f/255.0f, 3.0f/255.0f, 137.0f/255.0f);
    }
  }

  // Create particle buffers on GPU and copy data into the first buffer.
  // Mark as static since we only write from the CPU once.
  mParticleBuffer[mSourceIndex] = gl::Vbo::create( GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_STATIC_DRAW );
  mParticleBuffer[mDestinationIndex] = gl::Vbo::create( GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), nullptr, GL_STATIC_DRAW );

  for( int i = 0; i < 2; ++i )
  {
    // Describe the particle layout for OpenGL.
    mAttributes[i] = gl::Vao::create();
    gl::ScopedVao vao( mAttributes[i] );

    // Define attributes as offsets into the bound particle buffer
    gl::ScopedBuffer buffer( mParticleBuffer[i] );
    gl::enableVertexAttribArray( 0 );
    gl::enableVertexAttribArray( 1 );
    gl::enableVertexAttribArray( 2 );
    gl::enableVertexAttribArray( 3 );
    gl::enableVertexAttribArray( 4 );
    gl::vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, pos) );
    gl::vertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, color) );
    gl::vertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, ppos) );
    gl::vertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, home) );
    gl::vertexAttribPointer( 4, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, damping) );
  }

  // Load our update program.
  // Match up our attribute locations with the description we gave.

//    mRenderProg = gl::getStockShader( gl::ShaderDef().color() );
    mRenderProg = gl::GlslProg::create( gl::GlslProg::Format()
                  .vertex( loadAsset( "draw.vert" ) )
                  .fragment( loadAsset( "draw.frag" ) ) );
  
  
    mUpdateProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "particleUpdate.vs" ) )
      .feedbackFormat( GL_INTERLEAVED_ATTRIBS )
      .feedbackVaryings( { "position", "pposition", "home", "color", "damping" } )
      .attribLocation( "iPosition", 0 )
      .attribLocation( "iColor", 1 )
      .attribLocation( "iPPosition", 2 )
      .attribLocation( "iHome", 3 )
      .attribLocation( "iDamping", 4 )
  );

  // Listen to mouse events so we can send data as uniforms.
  getWindow()->getSignalMouseDown().connect( [this]( MouseEvent event )
  {
    mMouseDown = true;
    mMouseForce = 500.0f;
    mMousePos = vec3( event.getX(), event.getY(), 0.0f );
  });

  getWindow()->getSignalMouseDrag().connect( [this]( MouseEvent event )
  {
    mMousePos = vec3( event.getX(), event.getY(), 0.0f );
  });

  getWindow()->getSignalMouseUp().connect( [this]( MouseEvent event )
  {
     mMouseForce = 0.0f;
     mMouseDown = false;
  });

}


void HugeTVParticlesApp::update()
{
  
  // Update particles on the GPU
  gl::ScopedGlslProg prog( mUpdateProg );
  gl::ScopedState rasterizer( GL_RASTERIZER_DISCARD, true );  // turn off fragment stage
  mUpdateProg->uniform( "uMouseForce", mMouseForce );
  mUpdateProg->uniform( "uMousePos", mMousePos );

  // Bind the source data (Attributes refer to specific buffers).
  gl::ScopedVao source( mAttributes[mSourceIndex] );
  // Bind destination as buffer base.
  gl::bindBufferBase( GL_TRANSFORM_FEEDBACK_BUFFER, 0, mParticleBuffer[mDestinationIndex] );
  gl::beginTransformFeedback( GL_POINTS );

  // Draw source into destination, performing our vertex transformations.
  gl::drawArrays( GL_POINTS, 0, NUM_PARTICLES );

  gl::endTransformFeedback();

  // Swap source and destination for next loop
  std::swap( mSourceIndex, mDestinationIndex );

  // Update mouse force.
  if( mMouseDown ) {
      mMouseForce = 150.0f;
  }
}

void HugeTVParticlesApp::draw()
{
  gl::clear( Color( 0, 0, 0 ) );
  gl::setMatricesWindowPersp( getWindowSize() );
  gl::enableDepthRead();
  gl::enableDepthWrite();
  
  mRenderProg->uniform( "uTex", 0 );
  gl::ScopedGlslProg render( mRenderProg );
  gl::ScopedVao vao( mAttributes[mSourceIndex] );
  gl::context()->setDefaultShaderVars();
  gl::drawArrays( GL_POINTS, 0, NUM_PARTICLES );
}


CINDER_APP( HugeTVParticlesApp, RendererGl, [] ( App::Settings *settings ) {
  settings->setMultiTouchEnabled( false );
})
