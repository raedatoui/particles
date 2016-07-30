#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Rand.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"
#include "cinder/CameraUi.h"

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
  void resize() override;
  void keyDown( KeyEvent event ) override;
  void render();

private:
  gl::GlslProgRef mRenderProg;
  gl::GlslProgRef mUpdateProg;
  gl::GlslProgRef mShaderBlur;

  // Descriptions of particle data layout.
  gl::VaoRef    mAttributes[2];
  // Buffers holding raw particle data on GPU.
  gl::VboRef    mParticleBuffer[2];

  gl::FboRef mFbo;
  gl::FboRef mFboBlur1;
  gl::FboRef mFboBlur2;

  // Current source and destination buffers for transform feedback.
  // Source and destination are swapped each frame after update.
  std::uint32_t mSourceIndex    = 0;
  std::uint32_t mDestinationIndex = 1;

  // Mouse state suitable for passing as uniforms to update program
  bool      mMouseDown = false;
  Anim<float>     mMouseForce;
  vec3      mMousePos = vec3( 0, 0, 0 );
  
  int       NUM_PARTICLES = 0;
  Surface32f    mImage;
  Surface32f    mImage2;
  int32_t mWidth;
  int32_t mHeight;
  gl::Texture2dRef mTex;
  gl::Texture2dRef mTex2;
  
  vector<Particle> particles;
  float pointSize = 1.0f;
  Anim<vec3> mMousePosition;
  float attenuation;
  Anim<float> mTexBlend;
  static const int FBO_WIDTH = 1440, FBO_HEIGHT = 880;
  static const int BLUR_SIZE = 512;
  int lastFrame;
  int currentFrame;
  float delta;
  CameraPersp mCamera;
  CameraUi mCamUi;
};


void ParticlesApp::setup()
{
  mCamera = CameraPersp( FBO_WIDTH, FBO_HEIGHT, 45.0f, 5.0f, 2000.0f );
  mCamera.setPerspective(  75.0f, getWindowAspectRatio(), 5.0f, 2000.0f );
//  mCamera.lookAt( vec3( 0.0f, 0.0f, 10.0f), vec3( FBO_WIDTH/2, FBO_HEIGHT/2, 0.0f));
  mCamUi = CameraUi( &mCamera, getWindow(), -1 );

  mImage = loadImage( loadAsset( "textures/h1.jpg" ) );
  mWidth = mImage.getWidth();
  mHeight = mImage.getHeight();
  
  
  float midH = float(mHeight)/2.0;
  float sliceHalfWidth = float(mWidth)/20.0;
  mMousePosition = vec3(mWidth/10.0 * 5.0 + sliceHalfWidth, midH, 0.0f );
  mMouseForce = 0;
  
  
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
          p.damping = Rand::randFloat( 0.000965f, 0.000985f );
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

  mFbo = gl::Fbo::create( FBO_WIDTH, FBO_HEIGHT);
  mFboBlur1 = gl::Fbo::create( FBO_WIDTH, FBO_HEIGHT );
  mFboBlur2 = gl::Fbo::create( FBO_WIDTH, FBO_HEIGHT );

  try {
    mShaderBlur = gl::GlslProg::create( loadAsset( "blur.vert" ), loadAsset( "blur.frag" ) );
  }
  catch( const std::exception &e ) {
    console() << e.what() << endl;
    quit();
  }
  
  
  // Listen to mouse events so we can send data as uniforms.
  getWindow()->getSignalMouseDown().connect( [this]( MouseEvent event )
  {
    mMouseDown = true;
    mMousePosition = vec3( event.getX(), event.getY(), 0.0f );
  });

  getWindow()->getSignalMouseDrag().connect( [this]( MouseEvent event )
  {
    mMousePosition = vec3( event.getX(), event.getY(), 0.0f );
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
  }

  if (event.getChar() == 'a') {
//    vec3 ball = vec3(randFloat()*mWidth, randFloat()*mHeight, 0.0);
//    timeline().apply( &mMousePosition, ball, 1.0f, EaseInCubic() );
//    
//    float force = randFloat()*500.0f;
//    timeline().apply( &mMouseForce, force, 1.0f, EaseInCubic() );
    timeline().apply( &mTexBlend, 1.0f, 1.0f, EaseInCubic() );
  }

  if (event.getChar() == 'b') {
    timeline().apply( &mTexBlend, 0.5f, 1.0f, EaseInCubic() );
  }
  
}


void ParticlesApp::update()
{
  
  // Update particles on the GPU
  gl::ScopedGlslProg prog( mUpdateProg );
  gl::ScopedState rasterizer( GL_RASTERIZER_DISCARD, true );  // turn off fragment stage

  
  mUpdateProg->uniform( "uMouseForce", mMouseForce.value());
  mUpdateProg->uniform( "uMousePosition", mMousePosition.value());
  mUpdateProg->uniform( "uTex", 0);
  mUpdateProg->uniform( "uTex2", 1);
  mUpdateProg->uniform( "uTexBlend", mTexBlend.value());
  
  
  if (mMouseDown) {
    mMouseForce = mMouseForce + 10.0f;
    currentFrame = ci::app::getElapsedFrames();
    delta = float(currentFrame - lastFrame)/100.0f;
    mUpdateProg->uniform( "delta", delta);
    if(attenuation < 3.0f)  {
      attenuation += 0.05f;

    }
  }
  else {
    if(attenuation > 0) {
      attenuation -= 0.1f;
    } else {
      attenuation = 0.0f;
    }
    if(mMouseForce > 0)
      mMouseForce = mMouseForce - 10.0f;

     lastFrame = ci::app::getElapsedFrames();
     delta -= 0.01f;
     mUpdateProg->uniform("delta", delta);
    
  }

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
  

//  else {
//    if(mMouseForce >= 0)
//      mMouseForce = mMouseForce - 10.0f;
//  }
  
}

void ParticlesApp::render()
{

  gl::pushMatrices();
  

  gl::ScopedViewport viewportScope( ivec2( 0 ), mFbo->getSize() );
  //  gl::setMatricesWindowPersp( mFbo->getSize() );
//  gl::setMatricesWindow(FBO_WIDTH, FBO_HEIGHT);
  gl::setMatrices(mCamera);
  gl::ScopedDepth enableDepth( true );
  
  gl::enableDepthRead();
  gl::enableDepthWrite();

  gl::ScopedGlslProg render( mRenderProg );
  gl::ScopedVao vao( mAttributes[mSourceIndex] );
  gl::context()->setDefaultShaderVars();
  gl::drawArrays( GL_POINTS, 0, NUM_PARTICLES );
  
  gl::disableDepthWrite();
  gl::disableDepthRead();
  
  gl::popMatrices();
}

void ParticlesApp::draw()
{
  gl::pushMatrices();
  
  // render scene into mFboScene using illumination texture
  {
    gl::ScopedFramebuffer fbo( mFbo );
    gl::ScopedViewport    viewport( 0, 0, mFbo->getWidth(), mFbo->getHeight() );
    
    gl::setMatricesWindow( FBO_WIDTH, FBO_WIDTH );
    gl::clear( Color::black() );
    
    render();
  }
  
  
  // bind the blur shader
  {
    gl::ScopedGlslProg shader( mShaderBlur );
    mShaderBlur->uniform( "tex0", 0 ); // use texture unit 0
    
    // tell the shader to blur horizontally and the size of 1 pixel
    mShaderBlur->uniform( "sample_offset", vec2( 1.0f / mFboBlur1->getWidth(), 0.0f ) );
    mShaderBlur->uniform( "attenuation", attenuation );
    
    // copy a horizontally blurred version of our scene into the first blur Fbo
    {
      gl::ScopedFramebuffer fbo( mFboBlur1 );
      gl::ScopedViewport    viewport( 0, 0, mFboBlur1->getWidth(), mFboBlur1->getHeight() );
      
      gl::ScopedTextureBind tex0( mFbo->getColorTexture(), (uint8_t)0 );
      
      gl::setMatricesWindow( FBO_WIDTH, FBO_HEIGHT );
      gl::clear( Color::black() );
      
      gl::drawSolidRect( mFboBlur1->getBounds() );
    }
    
    // tell the shader to blur vertically and the size of 1 pixel
    mShaderBlur->uniform( "sample_offset", vec2( 0.0f, 1.0f / mFboBlur2->getHeight() ) );
    mShaderBlur->uniform( "attenuation", attenuation );
    
    // copy a vertically blurred version of our blurred scene into the second blur Fbo
    {
      gl::ScopedFramebuffer fbo( mFboBlur2 );
      gl::ScopedViewport    viewport( 0, 0, mFboBlur2->getWidth(), mFboBlur2->getHeight() );
      
      gl::ScopedTextureBind tex0( mFboBlur1->getColorTexture(), (uint8_t)0 );
      
      gl::setMatricesWindow( FBO_WIDTH, FBO_HEIGHT );
      gl::clear( Color::black() );
      
      gl::drawSolidRect( mFboBlur2->getBounds() );
    }
  }
  
  gl::popMatrices();
//  gl::setMatricesWindow( FBO_WIDTH, FBO_HEIGHT );
  auto tex0 = mFbo->getColorTexture();
  gl::color( Color::white() );
  gl::draw( tex0, tex0->getBounds() );

  gl::enableAdditiveBlending();
  tex0 = mFboBlur2->getColorTexture();

  gl::draw( tex0, tex0->getBounds() );
  gl::disableAlphaBlending();
  
  // clear the window to gray
//  gl::clear( Color( 0.35f, 0.35f, 0.35f ) );
//  gl::setMatricesWindow( FBO_WIDTH, FBO_HEIGHT );
//  auto tex0 = mFbo->getColorTexture();
//  gl::draw( tex0, tex0->getBounds());

  
}


void ParticlesApp::resize()
{
  mCamera.setAspectRatio( getWindowAspectRatio() );
  
}

CINDER_APP( ParticlesApp, RendererGl, [] ( App::Settings *settings ) {
  settings->setWindowSize( 	1440, 880 );
  settings->setMultiTouchEnabled( false );
})
