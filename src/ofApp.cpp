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
    
//    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
//    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

static float triangle(float t) {
    float s = fmod(t,2);
    return min(s, 2-s);
}

static void drawRect(int x, int y, int w, int h, int vertexCount, float startOffset, float endOffset, float rotateAngle) {
    // draw using raw OpenGL since ofx doesn't let us use 3d texture coordinates

    ofMatrix4x4 rotation;
    rotation.translate(-0.5,-0.5,0);
    rotation.rotate(rotateAngle, 0, 0, 1);
    rotation.translate(0.5, 0.5, 0);
    ofVec3f pt(0);
    for (int row = 0; row < vertexCount - 1; row++) {
        float py = ofMap(row, 0, vertexCount - 1, y, y + h);
        float py2 = ofMap(row + 1, 0, vertexCount - 1, y, y + h);
        float u = ofMap(row, 0, vertexCount - 1, 0, 1);
        float u2 = ofMap(row + 1, 0, vertexCount - 1, 0, 1);
        glBegin(GL_TRIANGLE_STRIP);
        
        for(int col = 0; col < vertexCount; col++) {
            float px = ofMap(col, 0, vertexCount - 1, x, x + w);
            float t = ofMap(col, 0, vertexCount - 1, 0, 1);
            // rotate (t,u,t) around Z axis through (0.5, 0.5, 0)
            pt.x = t;
            pt.y = u;
            pt = rotation * pt;
            float s = ofMap(triangle(pt.x), 0, 1, 0 + endOffset, 1 + startOffset);
            pt.x = t;
            pt.y = u2;
            pt = rotation * pt;
            float s2 = ofMap(triangle(pt.x), 0, 1, 0 + endOffset, 1 + startOffset);
            glTexCoord3d(t, u, s);
            glVertex2f(px, py);
            glTexCoord3d(t, u2, s2);
            glVertex2f(px, py2);
        }
        glEnd();
    }
    /*
    glTexCoord3d(0, 0, 0 + endOffset);
    glVertex2f(x, y);
    glTexCoord3d(0, 1, 0 + startOffset);
    glVertex2f(x, y + h);
    glTexCoord3d(1, 0, 1 + endOffset);
    glVertex2f(x + w, y);
    glTexCoord3d(1, 1, 1 + startOffset);
    glVertex2f(x + w, y + h);*/
}
//--------------------------------------------------------------
void ofApp::draw(){
    // we just wrote to layerIndex, so (layerIndex+1) % FRAMES is the oldest layer
    // we want to do a full cycle from oldest to newest (off-by-one is important here)
    float newestOffset = layerIndex / (float)FRAMES; // z-coordinate of last drawn frame
    float oldestOffset = (layerIndex + 1) / (float)FRAMES;
    
    ofMatrix4x4 originalTextureMatrix = cameraOutput.getTextureMatrix();
    
    cameraOutput.bind();
    // draw using raw OpenGL since ofx doesn't let us use 3d texture coordinates
    drawRect(0, 0, ofGetWidth(), ofGetHeight(), 100, newestOffset, oldestOffset, fmod(ofGetElapsedTimef(), 360));
    cameraOutput.unbind();
    cameraOutput.setTextureMatrix(originalTextureMatrix);
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
