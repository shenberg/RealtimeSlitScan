#include "ofApp.h"


static const int WIDTH = 640;
static const int HEIGHT = 480;
static const int FRAMES = 256;

static void checkOpenGLError(const char* stmt, const char* fname, int line)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        ofLogError() << "OpenGL error " << err << ", at " << fname << ":" << line << " - for " << stmt;
    }
}

#ifdef _DEBUG
#define GL_CHECK(stmt) do { \
stmt; \
CheckOpenGLError(#stmt, __FILE__, __LINE__); \
} while (0)
#else
#define GL_CHECK(stmt) stmt
#endif


//--------------------------------------------------------------
void ofApp::setup(){
    ofSetLogLevel(OF_LOG_VERBOSE);
    
    layerIndex = 0;
    
    // generate 3d texture: openframeworks is 2d-texture-only
    GLuint texture3d;
    glEnable(GL_TEXTURE_3D);
    GL_CHECK(glGenTextures(1, &texture3d));
    
    GL_CHECK(glBindTexture(GL_TEXTURE_3D, texture3d));
    
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // load data to the texture, set its resolution etc.
    unsigned char * texData = new unsigned char[WIDTH*HEIGHT*FRAMES*3];
    memset(texData, 0, WIDTH*HEIGHT*FRAMES*3);
    
    // NOTE: won't work without npot support
    GL_CHECK(glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, WIDTH, HEIGHT, FRAMES, 0, GL_RGB, GL_UNSIGNED_BYTE, texData));
    glDisable(GL_TEXTURE_3D);
    delete[] texData;
    
    // create an openframeworks texture object to wrap the texture id we created
    ofTextureData settings;
    settings.width = WIDTH;
    settings.height = HEIGHT;
    settings.tex_w = settings.tex_t = WIDTH;
    settings.tex_h = settings.tex_u = HEIGHT;
    settings.textureTarget = GL_TEXTURE_3D;
    cameraOutput.allocate(settings);
    cameraOutput.setUseExternalTextureID(texture3d);
    
    // set up camera & FBO to write to 3d texture
    cameraIn.setup(WIDTH, HEIGHT);
    cameraWriter.allocate(WIDTH, HEIGHT);
    cameraWriter.attachTexture(cameraOutput, GL_RGB, 0, layerIndex);
}

//--------------------------------------------------------------
void ofApp::update(){
    cameraIn.update();
    if (!cameraIn.isFrameNew()) {
        return;
    }
    
    layerIndex = (layerIndex + 1) % FRAMES;
    // NOTE: I modified openframeworks for this to work (gl/ofFbo.h, gl/ofFbo.cpp)
    // changed ofFbo::attachTexture signature to be:
    // void attachTexture(ofTexture & texture, GLenum internalFormat, GLenum attachmentPoint, GLuint layer = 0);
    // (added layer = 0)
    // and then in the implementation, called glFramebufferTexture3D if tex.texData.target == GL_TEXTURE_3D
    // instead of the usual glFramebufferTexture2D call
    cameraWriter.attachTexture(cameraOutput, GL_RGB, 0, layerIndex);
    cameraWriter.begin();
    cameraIn.draw(0,0,WIDTH,HEIGHT);
    cameraWriter.end();
}

//--------------------------------------------------------------
void ofApp::draw(){
    // we just wrote to layerIndex, so (layerIndex+1) % FRAMES is the oldest layer
    // we want to do a full cycle from oldest to newest (off-by-one is important here)
    float newestOffset = layerIndex / (float)FRAMES; // z-coordinate of last drawn frame
    float oldestOffset = (layerIndex + 1) / (float)FRAMES;
    
    cameraOutput.bind();
    // draw using raw OpenGL since ofx doesn't let us use 3d texture coordinates
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord3d(1, 0, 1 + newestOffset);
    glVertex2f(ofGetWidth(), 0);
    glTexCoord3d(1, 1, 1 + newestOffset);
    glVertex2f(ofGetWidth(), ofGetHeight());
    glTexCoord3d(0, 0, 0 + oldestOffset);
    glVertex2f(0, 0);
    glTexCoord3d(0, 1, 0 + oldestOffset);
    glVertex2f(0, ofGetHeight());
    glEnd();
    cameraOutput.unbind();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
