/*
 * Stellarium Scenery3d Plug-in
 *
 * Copyright (C) 2011 Simon Parzer, Peter Neubauer, Georg Zotti, Andrei Borza
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

/** OBJ loader based on dhpoware's glObjViewer (http://www.dhpoware.com/demos/glObjViewer.html) See license below **/

//-----------------------------------------------------------------------------
// Copyright (c) 2007 dhpoware. All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#ifndef _OBJ_HPP_
#define _OBJ_HPP_

#include "StelTexture.hpp"
#include "StelTextureMgr.hpp"
#include "StelFileMgr.hpp"
#include "StelLogger.hpp"
#include "VecMath.hpp"
#include "Util.hpp"
#include "AABB.hpp"

class Heightmap;

class OBJ
{
public:
    //! OBJ files can have vertices encoded in different order.
    //! Only XYZ and XZY may occur in real life, but we can cope with all...
    enum vertexOrder { XYZ, XZY, YXZ, YZX, ZXY, ZYX };
    //!< Supported OpenGL illumination models. Use specular sparingly!
    enum Illum { DIFFUSE, DIFFUSE_AND_AMBIENT, SPECULAR, TRANSLUCENT=9 };

    struct Material
    {
        Material() {
            ambient[0] = 0.2f;
            ambient[1] = 0.2f;
            ambient[2] = 0.2f;
            ambient[3] = 1.0f;
            diffuse[0] = 0.8f;
            diffuse[1] = 0.8f;
            diffuse[2] = 0.8f;
            diffuse[3] = 1.0f;
            specular[0] = 0.0f;
            specular[1] = 0.0f;
            specular[2] = 0.0f;
            specular[3] = 1.0f;
            emission[0] = 0.0f;
            emission[1] = 0.0f;
            emission[2] = 0.0f;
            emission[3] = 0.0f;
            shininess = 0.0f;
            alpha = 0.0f;
            illum = DIFFUSE;
            textureName.clear();
            texture.clear();
            bumpMapName.clear();
            bump_texture.clear();
        }
        //! Material name
        std::string name;
        //! Ka, Kd, Ks, Ke
        float ambient[4];
        float diffuse[4];
        float specular[4];
        float emission[4];
        //! Shininess [0..128]
        float shininess;
        //! Transparency [0..1]
        float alpha;
        //!< illumination model, copied from MTL.
        Illum illum;
        //! Texture name
        std::string textureName;
        //!< Shared pointer to texture of the model. This can be null.
        StelTextureSP texture;
        //! Bump map name
        std::string bumpMapName;
        //!< Shared pointer to bump map texture of the model. This can be null.
        StelTextureSP bump_texture;
    };

    //! A vertex struct holds the vertex itself (position), corresponding texture coordinates, normals, tangents and bitangents
    struct Vertex
    {
        Vertex() : position(0.0f), texCoord(0.0f), normal(0.0f), tangent(0.0f), bitangent(0.0f) {}
        Vec3d position;
        Vec2f texCoord;
        Vec3f normal;
        Vec4f tangent;
        Vec3f bitangent;
    };

    //! Structure for a Mesh, will be used with Stellarium to render
    //! Holds the starting index, the number of triangles and a pointer to the MTL
    struct StelModel
    {
        int startIndex, triangleCount;
        const Material* pMaterial;
        AABB* bbox;
    };

    //! Initializes values
    OBJ();
    //! Destructor
    ~OBJ();

    //! Cleanup, will be called inside the destructor
    void clean();
    //! Loads the given obj file and, if specified rebuilds normals
    bool load(const char* filename, const enum vertexOrder order, bool rebuildNormals = false);
    //! Transform all the vertices through multiplication with a 4x4 matrix.
    //! @param mat Matrix to multiply vertices with.
    void transform(Mat4d mat);
    //! Returns the index array
    const unsigned int* getIndexArray() const;
    //! Returns the size of an index
    unsigned int getIndexSize() const;
    //! Returns a Material
    Material &getMaterial(int i);
    //! Returns a StelModel
    const StelModel& getStelModel(int i) const;

    //! Getters for various datastructures
    int getNumberOfIndices() const;
    int getNumberOfStelModels() const;
    int getNumberOfTriangles() const;
    int getNumberOfVertices() const;
    int getNumberOfMaterials() const;

    //! Returns a vertex reference
    const Vertex& getVertex(int i) const;
    //! Returns the vertex array
    const Vertex* getVertexArray() const;
    //! Returns the vertex size
    int getVertexSize() const;

    //! Returns flags
    bool hasPositions() const;
    bool hasTextureCoords() const;
    bool hasNormals() const;
    bool hasTangents() const;
    bool hasStelModels() const;

    //! Returns the bounding box for this OBJ
    //const BoundingBox* getBoundingBox() const;
    AABB* getBoundingBox();

    void renderAABBs();

private:
    void addTrianglePos(unsigned int index, int material, int v0, int v1, int v2);
    void addTrianglePosNormal(unsigned int index, int material,
                              int v0, int v1, int v2,
                              int vn0, int vn1, int vn2);
    void addTrianglePosTexCoord(unsigned int index, int material,
                                int v0, int v1, int v2,
                                int vt0, int vt1, int vt2);
    void addTrianglePosTexCoordNormal(unsigned int index, int material,
                                      int v0, int v1, int v2,
                                      int vt0, int vt1, int vt2,
                                      int vn0, int vn1, int vn2);
    int addVertex(int hash, const Vertex* pVertex);
    //! Builds the StelModels based on material
    void buildStelModels();
    //! Generates normals in case they aren't specified/need rebuild
    void generateNormals();
    //! Generates tangents (and bitangents/binormals) (useful for NormalMapping, Parallax Mapping, ...)
    void generateTangents();
    //! First pass - scans the file for memory allocation
    void importFirstPass(FILE* pFile);
    //! Second pass - actual parsing step
    void importSecondPass(FILE* pFile, const enum vertexOrder order);
    //! Imports material file and fills the material datastructure
    bool importMaterials(const char* filename);
    //! Uploads the textures to GL
    void uploadTexturesGL();
    std::string absolutePath(std::string path);
    //! Determine the bounding box extrema
    void bounds();

    //! Flags
    bool m_hasPositions;
    bool m_hasTextureCoords;
    bool m_hasNormals;
    bool m_hasTangents;
    bool m_hasStelModels;

    //! Structure sizes
    int m_numberOfVertexCoords;
    int m_numberOfTextureCoords;
    int m_numberOfNormals;
    int m_numberOfTriangles;
    int m_numberOfMaterials;
    int m_numberOfStelModels;

    //! Bounding box for the entire scene
    AABB* pBoundingBox;
    Mat4d m;

    //! Base path to this file
    std::string m_basePath;

    //! Datastructures
    std::vector<StelModel> m_stelModels;
    std::vector<Material> m_materials;
    std::vector<Vertex> m_vertexArray;
    std::vector<unsigned int> m_indexArray;
    std::vector<int> m_attributeArray;
    std::vector<double> m_vertexCoords;
    std::vector<float> m_textureCoords;
    std::vector<float> m_normals;

    std::map<std::string, int> m_materialCache;
    std::map<int, std::vector<int> > m_vertexCache;

    //! Heightmap
    friend class Heightmap;
};

inline const unsigned int* OBJ::getIndexArray() const { return &m_indexArray[0]; }

inline unsigned int OBJ::getIndexSize() const { return static_cast<unsigned int>(sizeof(unsigned int)); }

inline OBJ::Material& OBJ::getMaterial(int i) { return m_materials[i]; }

inline const OBJ::StelModel& OBJ::getStelModel(int i) const { return m_stelModels[i]; }

inline int OBJ::getNumberOfIndices() const { return m_numberOfTriangles * 3; }

inline int OBJ::getNumberOfStelModels() const { return m_numberOfStelModels; }

inline int OBJ::getNumberOfTriangles() const { return m_numberOfTriangles; }

inline int OBJ::getNumberOfVertices() const { return static_cast<int>(m_vertexArray.size()); }

inline int OBJ::getNumberOfMaterials() const {return m_numberOfMaterials; }

inline const OBJ::Vertex& OBJ::getVertex(int i) const { return m_vertexArray[i]; }

inline const OBJ::Vertex* OBJ::getVertexArray() const { return &m_vertexArray[0]; }

inline int OBJ::getVertexSize() const { return static_cast<int>(sizeof(Vertex)); }

inline bool OBJ::hasNormals() const{ return m_hasNormals; }

inline bool OBJ::hasPositions() const { return m_hasPositions; }

inline bool OBJ::hasTangents() const { return m_hasTangents; }

inline bool OBJ::hasTextureCoords() const { return m_hasTextureCoords; }

inline bool OBJ::hasStelModels() const { return m_hasStelModels; }

//inline const OBJ::BoundingBox* OBJ::getBoundingBox() const { return pBoundingBox; }
inline AABB* OBJ::getBoundingBox() {return pBoundingBox; }

inline std::string OBJ::absolutePath(std::string path) { return m_basePath + path; }

#endif
