#pragma once

#include "ofMain.h"
#include "ps3eye.h"

class ofApp : public ofBaseApp{

public:
    void setup();
    void update();
    void draw();

    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseEntered(int x, int y);
    void mouseExited(int x, int y);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
    
private:
    ofVideoGrabber cameraIn;
    ofFbo          cameraWriter;
    ofTexture      cameraOutput;
    int            layerIndex;
    ps3eye::PS3EYECam::PS3EYERef eye = NULL;
    unsigned char *		videoFrame;
    ofTexture			videoTexture;

};
