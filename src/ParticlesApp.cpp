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
#include "cinder/Timeline.h"

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
  vec2 pixel;
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
  Surface32f		mImage2;
  
  vector<Particle> particles;
  float pointSize = 1.0f;
  Anim<vec3> mMousePositions[10];
  int32_t mWidth;
  int32_t mHeight;
  gl::Texture2dRef		mTex;
  gl::Texture2dRef		mTex2;

};


void ParticlesApp::setup()
{

  mImage = loadImage( loadAsset( "textures/h1.jpg" ) );
  mWidth = mImage.getWidth();
  mHeight = mImage.getHeight();
  
  
  float midH = float(mHeight)/2.0;
  float sliceHalfWidth = float(mWidth)/20.0;
  
  for (int slice = 0; slice < 10; ++slice) {
    mMousePositions[slice] = vec3(mWidth/10.0 * float(slice)+sliceHalfWidth, midH, 0.0f );
  }

  
  mTex = gl::Texture2d::create( mImage );
  mTex->bind(0);
  

  mImage2 = loadImage( loadAsset( "textures/h2.jpg" ) );
  mTex2 = gl::Texture2d::create( mImage2 );
  mTex2->bind(1);
  
  NUM_PARTICLES = mWidth * mHeight;

  // Create initial particle layout.
  // How many particles to create. (600k default)
  particles.assign( NUM_PARTICLES, Particle() );
  
  
  int i = 0;
  int j = 0;

  try {
    for (i=1; i <= mHeight; ++i) {
      for (j=1; j <= mWidth; ++j) {
        int index = j + (i-1)*mWidth - 1;
          auto &p = particles.at(index );
          p.pos = vec3(j,i,0);
          p.home = p.pos;
          p.ppos = p.home + Rand::randVec3() * 10.0f; // random initial velocity
          p.damping = 0; //Rand::randFloat( 0.0965f, 0.0985f );
          p.pixel = vec2(float(j)/float(mWidth),1.0-float(i)/float(mHeight));
      }

    }
  }
  catch( ci::Exception &exc ) {
    console() << i << j << " failed to load doc, what: " << exc.what() << endl;
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
  
  mUpdateProg = gl::GlslProg::create( gl::GlslProg::Format().vertex( loadAsset( "particleUpdate.vs" )  )
      .feedbackFormat( GL_INTERLEAVED_ATTRIBS )
      .feedbackVaryings( { "position", "pposition", "home", "color", "damping", "pixel" } )
      .attribLocation( "iPosition", 0 )
      .attribLocation( "iColor", 1 )
      .attribLocation( "iPPosition", 2 )
      .attribLocation( "iHome", 3 )
      .attribLocation( "iDamping", 4 )
      .attribLocation( "iPixel", 5 )
  );
  mUpdateProg->uniform( "uTex", 0);
  mUpdateProg->uniform( "uTex2", 1);
  
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
  if (event.getChar() == 'a') {
    vec3 ball1 = vec3(randFloat()*mWidth, randFloat()*mHeight, 0.0);
    vec3 ball2 = vec3(randFloat()*mWidth, randFloat()*mHeight, 0.0);
    
    // the call to apply() replaces any existing tweens on mBlackPos with this new one
    timeline().apply( &mMousePositions[0], ball1, 3.5f, EaseInCubic() );
    // the call to appendTo causes the white circle to start when the black one finishes
    timeline().apply( &mMousePositions[1] , ball2, 3.5f, EaseOutQuint() ); //.appendTo( &mBlackPos );
  }
}


void ParticlesApp::update()
{
  
  // Update particles on the GPU
  gl::ScopedGlslProg prog( mUpdateProg );
  gl::ScopedState rasterizer( GL_RASTERIZER_DISCARD, true );  // turn off fragment stage
  
  vec3 wt[10];
  for(int k = 0; k < 10; ++k) {
    wt[k] =  mMousePositions[k].value();
  }
  
  mUpdateProg->uniform( "uMouseForce", mMouseForce );
  mUpdateProg->uniform( "uMousePositions", wt, 10);
  
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
      mMouseForce += 100.0f;
  }
 
}

void ParticlesApp::draw()
{
  gl::clear( Color( 0, 0, 0 ) );
//  gl::setMatricesWindowPersp( getWindowSize() );
  gl::enableDepthRead();
  gl::enableDepthWrite();
  
  gl::ScopedGlslProg render( mRenderProg );
  gl::ScopedVao vao( mAttributes[mSourceIndex] );
  gl::context()->setDefaultShaderVars();
  gl::drawArrays( GL_POINTS, 0, NUM_PARTICLES );
}


CINDER_APP( ParticlesApp, RendererGl, [] ( App::Settings *settings ) {
//  settings->setWindowSize( 	1440, 880 );
  settings->setMultiTouchEnabled( false );
})
