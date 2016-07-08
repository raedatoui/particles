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
  vec2  pixel;
};


/**
 Simple particle simulation with Verlet integration and mouse interaction.
 A sphere of particles is deformed by mouse interaction.
 Simulation is run using transform feedback on the GPU.
 particleUpdate.vs defines the simulation update step.
 Designed to have the same behavior as ParticleSphereCPU.
 */
class ParticlesApp : public App {
public:
  void setup() override;
  void update() override;
  void draw() override;
  void keyDown( KeyEvent event ) override;
  
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
  int       NUM_PARTICLES = 0;
  Surface32f		mImage;
  vector<Particle> particles;
  float pointSize = 1.0f;
  gl::Texture2dRef		mTex;
};


void ParticlesApp::setup()
{

  mImage = loadImage( loadAsset( "textures/small.jpg" ) );
  mTex = gl::Texture2d::create( mImage );
  mTex->bind(0);
  
  //Surface32f::Iter pixelIter = mImage.getIter();
  uint32_t mWidth = mImage.getWidth();
  uint32_t mHeight = mImage.getHeight();
  
  NUM_PARTICLES = mWidth * mHeight;

  // Create initial particle layout.
  // How many particles to create. (600k default)
  particles.assign( NUM_PARTICLES, Particle() );
  
  for( int i = 0; i < mWidth; ++i )
  {
    for( int j = 0; j < mHeight; ++j ) {
      auto &p = particles.at( i*mWidth +j );
      p.pos = vec3( i, j, 0 );
      p.pixel = vec2(float(i/(mWidth-1.0f)),float(j/(mHeight-1.0f)));
      printf("pixel is %f %f\n",  p.pixel[0], p.pixel[1]);
      p.home = p.pos;
      p.ppos = p.home + Rand::randVec3() * 10.0f; // random initial velocity
      p.damping = 0.0f; //Rand::randFloat( 0.965f, 0.985f );
      p.color = Color( CM_RGB, 239.0f/255.0f, 3.0f/255.0f, 137.0f/255.0f);
    }
    printf("index is %d out of %d\n", (i*mWidth), NUM_PARTICLES);
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
    gl::enableVertexAttribArray( 5 );
    
    gl::vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, pos) );
    gl::vertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, color) );
    gl::vertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, ppos) );
    gl::vertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, home) );
    gl::vertexAttribPointer( 4, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, damping) );
    gl::vertexAttribPointer( 5, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (const GLvoid*)offsetof(Particle, pixel) );
    
  }

  // Load our update program.
  // Match up our attribute locations with the description we gave.

//  mRenderProg = gl::getStockShader( gl::ShaderDef().color() );
    mRenderProg = gl::GlslProg::create( gl::GlslProg::Format()
                  .vertex( loadAsset( "draw.vert" ) )
                  .fragment( loadAsset( "draw.frag" ) ) );
  
  
    mUpdateProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "particleUpdate.vs" ) )
                                       .feedbackFormat( GL_INTERLEAVED_ATTRIBS )
                                       .feedbackVaryings( { "position", "pposition", "home", "color", "damping", "pixel" } )
                                       .attribLocation( "iPosition", 0 )
                                       .attribLocation( "iColor", 1 )
                                       .attribLocation( "iPPosition", 2 )
                                       .attribLocation( "iHome", 3 )
                                       .attribLocation( "iDamping", 4 )
                                       .attribLocation( "iPixel", 5 )
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

void ParticlesApp::keyDown( KeyEvent event )
{
  if( event.getChar() == 'f' ) {
    pointSize += 0.1f;
    gl::pointSize( pointSize );
    mRenderProg->uniform("pointSize", pointSize);
  }

  if( event.getChar() == 'c' ) {
    pointSize = 1.0f;
    gl::pointSize( pointSize );
    mRenderProg->uniform("pointSize", pointSize);
  }
}

void ParticlesApp::update()
{
  
  // Update particles on the GPU
  gl::ScopedGlslProg prog( mUpdateProg );
  gl::ScopedState rasterizer( GL_RASTERIZER_DISCARD, true );  // turn off fragment stage
  mUpdateProg->uniform( "uMouseForce", mMouseForce );
  mUpdateProg->uniform( "uMousePos", mMousePos );
  mUpdateProg->uniform( "uTex", 0);
    
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
      mMouseForce += 10.0f;
  }
//  mMouseDown = true;
//  mMousePos[1] += 50.0f;
//  if (mMousePos[1] >= 880) {
//    mMousePos[0] += 40.0f;
//    mMousePos[1] = 0.0f;
//  }
//  if (mMousePos[0] >= 1440) {
//    mMousePos[0] = 0.0f;
//    mMousePos[1] = 0.0f;
//  }
}

void ParticlesApp::draw()
{
  gl::clear( Color( 0, 0, 0 ) );
  gl::setMatricesWindowPersp( getWindowSize() );
  gl::enableDepthRead();
  gl::enableDepthWrite();

  gl::ScopedGlslProg render( mRenderProg );
  gl::ScopedVao vao( mAttributes[mSourceIndex] );
  gl::context()->setDefaultShaderVars();
  gl::drawArrays( GL_POINTS, 0, NUM_PARTICLES );
}


CINDER_APP( ParticlesApp, RendererGl, [] ( App::Settings *settings ) {
  settings->setWindowSize( 	1440, 880 );
  settings->setMultiTouchEnabled( false );
})
