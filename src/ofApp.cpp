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

static const int ITUR_BT_601_CY = 1220542;
static const int ITUR_BT_601_CUB = 2116026;
static const int ITUR_BT_601_CUG = -409993;
static const int ITUR_BT_601_CVG = -852492;
static const int ITUR_BT_601_CVR = 1673527;
static const int ITUR_BT_601_SHIFT = 20;


static void yuv422_to_rgba(const uint8_t *yuv_src, const int stride, uint8_t *dst, const int width, const int height)
{
    const int bIdx = 2;
    const int uIdx = 0;
    const int yIdx = 0;
    
    const int uidx = 1 - yIdx + uIdx * 2;
    const int vidx = (2 + uidx) % 4;
    int j, i;
    
#define _max(a, b) (((a) > (b)) ? (a) : (b))
#define _saturate(v) static_cast<uint8_t>(static_cast<uint32_t>(v) <= 0xff ? v : v > 0 ? 0xff : 0)
    
    for (j = 0; j < height; j++, yuv_src += stride)
    {
        uint8_t* row = dst + (width * 4) * j; // 4 channels
        
        for (i = 0; i < 2 * width; i += 4, row += 8)
        {
            int u = static_cast<int>(yuv_src[i + uidx]) - 128;
            int v = static_cast<int>(yuv_src[i + vidx]) - 128;
            
            int ruv = (1 << (ITUR_BT_601_SHIFT - 1)) + ITUR_BT_601_CVR * v;
            int guv = (1 << (ITUR_BT_601_SHIFT - 1)) + ITUR_BT_601_CVG * v + ITUR_BT_601_CUG * u;
            int buv = (1 << (ITUR_BT_601_SHIFT - 1)) + ITUR_BT_601_CUB * u;
            
            int y00 = _max(0, static_cast<int>(yuv_src[i + yIdx]) - 16) * ITUR_BT_601_CY;
            row[2 - bIdx] = _saturate((y00 + ruv) >> ITUR_BT_601_SHIFT);
            row[1] = _saturate((y00 + guv) >> ITUR_BT_601_SHIFT);
            row[bIdx] = _saturate((y00 + buv) >> ITUR_BT_601_SHIFT);
            row[3] = (0xff);
            
            int y01 = _max(0, static_cast<int>(yuv_src[i + yIdx + 2]) - 16) * ITUR_BT_601_CY;
            row[6 - bIdx] = _saturate((y01 + ruv) >> ITUR_BT_601_SHIFT);
            row[5] = _saturate((y01 + guv) >> ITUR_BT_601_SHIFT);
            row[4 + bIdx] = _saturate((y01 + buv) >> ITUR_BT_601_SHIFT);
            row[7] = (0xff);
        }
    }
}


//--------------------------------------------------------------
void ofApp::setup(){
    ofSetLogLevel(OF_LOG_VERBOSE);
    
    layerIndex = 0;
    
    // generate 3d texture: openframeworks is 2d-texture-only
    GLuint texture3d;
    glEnable(GL_TEXTURE_3D);
    GL_CHECK(glGenTextures(1, &texture3d));
    
    GL_CHECK(glBindTexture(GL_TEXTURE_3D, texture3d));
    
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
    
    try {
        using namespace ps3eye;
        std::vector<PS3EYECam::PS3EYERef> devices(PS3EYECam::getDevices());
        if (devices.size())
        {
            // Only stop eye if eye is working and more then one camera is connected
            if (eye && devices.size() > 1) {
                eye->stop();
                //eye = NULL;
            }
            
            // Init a new eye only if eye is not set or if devices is bigger then 1
            if (!eye || devices.size() > 1) {
                eye = devices.at(0);
                bool res = eye->init(WIDTH, HEIGHT, 60);
                if (res) {
                    eye->start();
                    eye->setExposure(125); //TODO: was 255
                    eye->setAutogain(true);
                    
                    videoFrame = new unsigned char[eye->getWidth()*eye->getHeight() * 4];
                    videoTexture.allocate(eye->getWidth(), eye->getHeight(), GL_RGB);
                }
                else {
                    eye = NULL;
                }
            }
        }
        else {
            ofLogError() << "Failed to open PS eye!";
        }
    }
    catch (...) {
        ofLogError() << "Failed to open PS eye. Exception.";
    }

}

//--------------------------------------------------------------
void ofApp::update(){
    try {
        uint8_t* new_pixels = eye->getFrame();
        yuv422_to_rgba(new_pixels, eye->getRowBytes(), videoFrame, eye->getWidth(), eye->getHeight());
        videoTexture.loadData(videoFrame, eye->getWidth(), eye->getHeight(), GL_RGBA);
        free(new_pixels);
    }
    catch (...) {
        ofLogWarning("Can't open ps eye. exception. moving to kinect");
    }

    /*
    cameraIn.update();
    if (!cameraIn.isFrameNew()) {
        return;
    }*/
    
    layerIndex = (layerIndex + 1) % FRAMES;
    // NOTE: I modified openframeworks for this to work (gl/ofFbo.h, gl/ofFbo.cpp)
    // changed ofFbo::attachTexture signature to be:
    // void attachTexture(ofTexture & texture, GLenum internalFormat, GLenum attachmentPoint, GLuint layer = 0);
    // (added layer = 0)
    // and then in the implementation, called glFramebufferTexture3D if tex.texData.target == GL_TEXTURE_3D
    // instead of the usual glFramebufferTexture2D call
    cameraWriter.attachTexture(cameraOutput, GL_RGB, 0, layerIndex);
    cameraWriter.begin();
    //cameraIn.draw(0,0,WIDTH,HEIGHT);
    videoTexture.draw(0,0,WIDTH, HEIGHT);
    cameraWriter.end();
}

static float triangle(float t) {
    float s = fmod(t,2);
    return min(s, 2-s);
}

static void drawRect(int x, int y, int w, int h, int vertexCount, float offset, float rotateAngle) {
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
/*            // rotate (t,u,t) around Z axis through (0.5, 0.5, 0)
            pt.x = t;
            pt.y = u;
            //pt = rotation * pt;
            float s = ofMap(triangle(pt.y), 0, 1, 0 + endOffset, 1 + startOffset);
            pt.x = t;
            pt.y = u2;
            //pt = rotation * pt;
            float s2 = ofMap(triangle(pt.y), 0, 1, 0 + endOffset, 1 + startOffset);
 */
            float s = u + offset, s2 = u2 + offset;
            glTexCoord3d(t, u, s);
            glVertex2f(px, py);
            glTexCoord3d(t, u2, s2);
            glVertex2f(px, py2);
        }
        glEnd();
    }
}
//--------------------------------------------------------------
void ofApp::draw(){
    // we just wrote to layerIndex, so (layerIndex+1) % FRAMES is the oldest layer
    // we want to do a full cycle from oldest to newest (off-by-one is important here)
    float newestOffset = layerIndex / (float)FRAMES; // z-coordinate of last drawn frame
    float oldestOffset = (layerIndex + 1) / (float)FRAMES;
    
    ofMatrix4x4 originalTextureMatrix = cameraOutput.getTextureMatrix();
    ofMatrix4x4 newMatrix = originalTextureMatrix;
    newMatrix.translate(-0.5, -0.5, 0);
    newMatrix.rotate(90, 1, 0, 0);
    
    
    cameraOutput.bind();
    // draw using raw OpenGL since ofx doesn't let us use 3d texture coordinates
    drawRect(0, 0, ofGetWidth(), ofGetHeight(), 100, oldestOffset, 0);// fmod(ofGetElapsedTimef(), 360));
    cameraOutput.unbind();
    cameraOutput.setTextureMatrix(originalTextureMatrix);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == 'f') {
        ofToggleFullscreen();
    }
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
