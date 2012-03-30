/*
 * Stellarium Scenery3d Plug-in
 * 
 * Copyright (C) 2011 Simon Parzer, Peter Neubauer, Georg Zotti
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _SCENERY3D_HPP_
#define _SCENERY3D_HPP_

#include "fixx11h.h"
#include "StelGui.hpp"
#include "StelModule.hpp"
#include "StelPainter.hpp"
#include "Landscape.hpp"
#include "OBJ.hpp"
#include "Heightmap.hpp"
#include "Frustum.hpp"

#include <QString>
//#include <vector>
#include <QGLFramebufferObject>
// This is a tentative preparation for some changes to come with Qt4.8+.
// Currently this does not work after compilation!
#if QT_VERSION >= 0x040800
#include <QGLFunctions>
#endif
#include "StelShader.hpp"

using std::vector;

//! Representation of a complete 3D scenery
#if QT_VERSION >= 0x040800
class Scenery3d: protected QGLFunctions
#else
class Scenery3d
#endif
{
public:
    //! Initializes an empty Scenery3d object.
    //! @param cbmSize Size of the cubemap to use for indirect rendering.
    Scenery3d(int cubemapSize=1024, int shadowmapSize=1024);
    virtual ~Scenery3d();

    //! Sets the shaders for the plugin
    void setShaders(StelShader* shadowShader = 0, StelShader* bumpShader = 0, StelShader* univShader = 0)
    {
        this->shadowShader = shadowShader;
        this->bumpShader = bumpShader;
        this->univShader = univShader;
    }

    //! Loads configuration values from a scenery3d.ini file.
    void loadConfig(const QSettings& scenery3dIni, const QString& scenery3dID);
    //! Loads the model given in the scenery3d.ini file.
    //! Make sure to call .loadConfig() before this.
    void loadModel();
	
    //! Walk/Fly Navigation with Ctrl+Cursor and Ctrl+PgUp/Dn keys.
    //! Pressing Ctrl-Alt: 5x, Ctrl-Shift: 10x speedup; Ctrl-Shift-Alt: 50x!
    //! To allow fine control, zoom in.
    //! If you release Ctrl key while pressing cursor key, movement will continue.
    void handleKeys(QKeyEvent* e);

    //! Update method, called by Scenery3dMgr.
    //! Shifts observer position due to movement through the landscape.
    void update(double deltaTime);
    //! Draw observer grid coordinates as text.
    void drawCoordinatesText(StelCore* core);
    //! Draw some text output. This can be filled as needed by development.
    void drawDebugText(StelCore* core);

    //! Draw scenery, called by Scenery3dMgr.
    void draw(StelCore* core);
    //! Initializes shadow mapping
    void initShadowMapping();

    bool getShadowsEnabled(void) { return shadowsEnabled; }
    void setShadowsEnabled(bool shadowsEnabled) { this->shadowsEnabled = shadowsEnabled; }
    bool getBumpsEnabled(void) { return bumpsEnabled; }
    void setBumpsEnabled(bool bumpsEnabled) { this->bumpsEnabled = bumpsEnabled; }
    bool getTorchEnabled(void) { return torchEnabled;}
    void setTorchEnabled(bool torchEnabled) { this->torchEnabled = torchEnabled; }

    //! @return Name of the scenery.
    QString getName() const { return name; }
    //! @return Author of the scenery.
    QString getAuthorName() const { return authorName; }
    //! @return Description of the scenery.
    QString getDescription() const { return description; }
    //! @return Name of the landscape associated with the scenery.
    QString getLandscapeName() const { return landscapeName; }
    //! @return Flag, whether scenery provides location data or only inherits from its linked Landscape.
    bool hasLocation() const { return (location != NULL); }
    //! @return Location data. These are valid only if written in the @file scenery3d.ini, section location.
    //! Else, returns NULL. You can also check with .hasLocation() before calling.
    const StelLocation& getLocation() const {return *location; }
    //! @return Flag, whether scenery provides default/startup looking direction.
    bool hasLookat() const { return (lookAt_fov[2]!=-1000.0f); }
    //! @return default looking direction and field-of-view (altazimuthal, 0=North).
    //! Valid only if written in the @file scenery3d.ini, value: coord/start_az_alt_fov.
    //! Else, returns NULL. You can also check with .hasLookup() before calling.
    const Vec3f& getLookat() const {return lookAt_fov; }


    enum ShadowCaster { None, Sun, Moon, Venus };
    enum Effect { No, BumpMapping, ShadowMapping, All };
    Mat4d mv;
    Mat4d mp;
    Vec3d viewUp;
    Vec3d viewDir;
    Vec3d viewPos;
    int drawn;

private:
    static const float TORCH_BRIGHTNESS=0.5f;
    static const float AMBIENT_BRIGHTNESS_FACTOR=0.05;
    static const float LUNAR_BRIGHTNESS_FACTOR=0.2;
    static const float VENUS_BRIGHTNESS_FACTOR=0.005;

    double eyeLevel;

    void drawObjModel(StelCore* core);
    void generateShadowMap(StelCore* core);
    void generateCubeMap(StelCore* core);
    void generateCubeMap_drawScene(StelPainter& painter, float ambientBrightness, float directionalBrightness);
    void generateCubeMap_drawSceneWithShadows(StelPainter& painter, float ambientBrightness, float directionalBrightness);
    void drawArrays(StelPainter& painter, bool textures=true);
    void drawFromCubeMap(StelCore* core);

    //! @return height at -absolutePosition, which is the current eye point.
    float groundHeight();

    bool hasModels;             // flag to see if there's anything to draw
    bool shadowsEnabled;        // switchable value (^SPACE): Use shadow mapping
    bool bumpsEnabled;          // switchable value (^B): Use bump mapping
    bool textEnabled;           // switchable value (^K): display coordinates on screen. THIS IS NOT FOR DEBUGGING, BUT A PROGRAM FEATURE!
    bool torchEnabled;          // switchable value (^L): adds artificial ambient light
    bool debugEnabled;          // switchable value (^D): display debug graphics and debug texts on screen
    bool lightCamEnabled;       // switchable value: switches camera to light camera
    bool sceneryGenNormals;     // Config flag, true generates normals for the given OBJ
    bool groundGenNormals;      // Config flag, true generates normals for the given ground Model

    int cubemapSize;            // configurable values, typically 512/1024/2048/4096
    int shadowmapSize;

    Vec3d absolutePosition;     // current eyepoint in model
    float movement_x;           // speed values for moving around the scenery
    float movement_y;
    float movement_z;

    float fTransparencyThresh;  // Threshold for discarding transparent texels

    StelCore* core;
    OBJ* objModel;
    OBJ* groundModel;
    Heightmap* heightmap;
    OBJ::vertexOrder objVertexOrder; // some OBJ files have left-handed coordinate counting or swapped axes. Allows accounting for those.

    Mat4f projectionMatrix;
    Mat4f lightViewMatrix;
    Mat4f lightProjectionMatrix;
    QGLFramebufferObject* shadowMapFbo;
    QGLFramebufferObject* cubeMap[6]; // front, right, left, back, top, bottom
    StelVertexArray cubePlaneFront, cubePlaneBack,
                cubePlaneLeft, cubePlaneRight,
                cubePlaneTop, cubePlaneBottom;

    vector<OBJ::StelModel> objModelArrays;

    QString id;
    QString name;
    QString authorName;
    QString description;
    QString landscapeName;
    QString modelSceneryFile;
    QString modelGroundFile;
    StelLocation* location;
    Vec3f lookAt_fov; // (az_deg, alt_deg, fov_deg)
    Vec3d modelWorldOffset; // required for coordinate display
    QString gridName;
    double gridCentralMeridian;
    double groundNullHeight; // Used as height value outside the model ground, or if ground=NULL
    QString lightMessage; // DEBUG/TEST ONLY. contains on-screen info on ambient/directional light strength and source.
    QString lightMessage2; // DEBUG/TEST ONLY. contains on-screen info on ambient/directional light strength and source.
    QString lightMessage3; // DEBUG/TEST ONLY. contains on-screen info on ambient/directional light strength and source.

    // used to apply the rotation from model/grid coordinates to Stellarium coordinates.
    // In the OBJ files, X=Grid-East, Y=Grid-North, Z=height.
    Mat4d zRotateMatrix;
    // if only a non-georeferenced OBJ can be provided, you can specify a matrix via .ini/[model]/obj_world_trafo.
    // This will be applied to make sure that X=Grid-East, Y=Grid-North, Z=height.
    Mat4d obj2gridMatrix;

    //Currently selected Shader
    StelShader* curShader;
    //Shadow mapping shader + per pixel lighting
    StelShader* shadowShader;
    //Bump mapping shader
    StelShader* bumpShader;
    //Universal shader: shadow + bump mapping
    StelShader* univShader;
    //Depth texture id
    GLuint shadowMapTexture;
    //Shadow Map FBO handle
    GLuint shadowFBO;
    //Currently selected effect
    Effect curEffect;
    //Sends texture data to the shader based on which effect is selected;
    void sendToShader(const OBJ::StelModel* pStelModel, Effect cur, bool& tangEnabled, int& tangLocation);
    //Binds the shader for the selected effect
    void bindShader();
    //Prepare ambient and directional light components from Sun, Moon, Venus.
    Scenery3d::ShadowCaster setupLights(float &ambientBrightness, float &diffuseBrightness, Vec3f &lightsourcePosition);
    //Set independent brightness factors (allow e.g. solar twilight ambient&lunar specular). Call setupLights first!
    void setLights(float ambientBrightness, float diffuseBrightness);
    //Current Model View Matrix
    Mat4f modelView;
    //Current sun position
    Vec3d sunPosition;
    //Switches the camera to sun position camera for debug purposes
    void switchToLightCam();
    //Sets the scenes' min/max vertices for AABB construction
    void setSceneAABB(Vec3f vecMin, Vec3f vecMax);
    //Renders the Scene's AABB as wireframe
    void renderSceneAABB();
    //Scene AABB
    Vec3f aabb[8];
    //Camera Frustum
    Frustum cFrust;
    Vec3f nbl;
};

#endif
