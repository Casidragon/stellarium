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

#include "OBJ.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    bool StelModelCompFunc(const OBJ::StelModel& lhs, const OBJ::StelModel& rhs)
    {
        return lhs.pMaterial->alpha > rhs.pMaterial->alpha;
    }
}

OBJ::OBJ()
{
    //Iinitialize this OBJ
    m_hasPositions = false;
    m_hasNormals = false;
    m_hasTextureCoords = false;
    m_hasTangents = false;

    m_numberOfVertexCoords = 0;
    m_numberOfTextureCoords = 0;
    m_numberOfNormals = 0;
    m_numberOfTriangles = 0;
    m_numberOfMaterials = 0;
    m_numberOfStelModels = 0;

    pBoundingBox = new AABB(Vec3f(0.0f), Vec3f(0.0f));
}

OBJ::~OBJ()
{
    clean();
}

void OBJ::clean()
{
    m_hasPositions = false;
    m_hasNormals = false;
    m_hasTextureCoords = false;
    m_hasTangents = false;

    m_numberOfVertexCoords = 0;
    m_numberOfTextureCoords = 0;
    m_numberOfNormals = 0;
    m_numberOfTriangles = 0;
    m_numberOfMaterials = 0;
    m_numberOfStelModels = 0;

    delete pBoundingBox;

    m_stelModels.clear();
    m_materials.clear();
    m_vertexArray.clear();
    m_indexArray.clear();
    m_attributeArray.clear();

    m_vertexCoords.clear();
    m_textureCoords.clear();
    m_normals.clear();

    m_materialCache.clear();
    m_vertexCache.clear();
}

bool OBJ::load(const char* filename, const enum vertexOrder order, bool rebuildNormals)
{
    FILE* pFile = fopen(filename, "r");

    if(!pFile)
        return false;

    //Extract the base path, will be used to load the MTL file later on
    m_basePath.clear();
    m_basePath = std::string(StelFileMgr::dirName(filename).toAscii() + "/");

    //Parse the file
    importFirstPass(pFile);
    rewind(pFile);
    importSecondPass(pFile, order);

    //Done parsing, close file
    fclose(pFile);

    //Build the StelModels
    buildStelModels();
    m_hasStelModels = m_numberOfStelModels > 0;
    //Find bounding extrema
    bounds();

    //Create vertex normals if specified or required
    if(rebuildNormals)
    {
        generateNormals();
    }
    else
    {
        if(!hasNormals())
            generateNormals();
    }

    //Create tangents
    generateTangents();

    //Upload the textures to GL and set the StelTextureSP
    uploadTexturesGL();

    //Loaded
    qDebug() << getTime() << "[Scenery3d] Loaded OBJ successfully: " << filename;
    qDebug() << getTime() << "[Scenery3d] Triangles#: " << m_numberOfTriangles;
    qDebug() << getTime() << "[Scenery3d] Vertices#: " << m_numberOfVertexCoords;
    qDebug() << getTime() << "[Scenery3d] Normals#: " << m_numberOfNormals;
    qDebug() << getTime() << "[Scenery3d] StelModels#: " << m_numberOfStelModels;
    qDebug() << getTime() << "[Scenery3d] Bounding Box";
    qDebug() << getTime() << "[Scenery3d] X: [" << pBoundingBox->min[0] << ", " << pBoundingBox->max[0] << "] ";
    qDebug() << getTime() << "[Scenery3d] Y: [" << pBoundingBox->min[1] << ", " << pBoundingBox->max[1] << "] ";
    qDebug() << getTime() << "[Scenery3d] Z: [" << pBoundingBox->min[2] << ", " << pBoundingBox->max[2] << "] ";

    return true;
}

void OBJ::addTrianglePos(unsigned int index, int material, int v0, int v1, int v2)
{
    Vertex vertex;

    //Add the material index to the index for grouping models
    m_attributeArray[index] = material;

    //Add v0
    vertex.position[0] = m_vertexCoords[v0*3];
    vertex.position[1] = m_vertexCoords[v0*3+1];
    vertex.position[2] = m_vertexCoords[v0*3+2];
    m_indexArray[index*3] = addVertex(v0, &vertex);

    //Add v1
    vertex.position[0] = m_vertexCoords[v1*3];
    vertex.position[1] = m_vertexCoords[v1*3+1];
    vertex.position[2] = m_vertexCoords[v1*3+2];
    m_indexArray[index*3+1] = addVertex(v1, &vertex);

    //Add v2
    vertex.position[0] = m_vertexCoords[v2*3];
    vertex.position[1] = m_vertexCoords[v2*3+1];
    vertex.position[2] = m_vertexCoords[v2*3+2];
    m_indexArray[index*3+2] = addVertex(v2, &vertex);
}

void OBJ::addTrianglePosNormal(unsigned int index, int material, int v0, int v1, int v2, int vn0, int vn1, int vn2)
{
    Vertex vertex;

    //Add the material index to the index for grouping models
    m_attributeArray[index] = material;

    //Add v0//vn0
    vertex.position[0] = m_vertexCoords[v0*3];
    vertex.position[1] = m_vertexCoords[v0*3+1];
    vertex.position[2] = m_vertexCoords[v0*3+2];
    vertex.normal[0] = m_normals[vn0*3];
    vertex.normal[1] = m_normals[vn0*3+1];
    vertex.normal[2] = m_normals[vn0*3+2];
    m_indexArray[index*3] = addVertex(v0, &vertex);

    //Add v1//n1
    vertex.position[0] = m_vertexCoords[v1*3];
    vertex.position[1] = m_vertexCoords[v1*3+1];
    vertex.position[2] = m_vertexCoords[v1*3+2];
    vertex.normal[0] = m_normals[vn1*3];
    vertex.normal[1] = m_normals[vn1*3+1];
    vertex.normal[2] = m_normals[vn1*3+2];
    m_indexArray[index*3+1] = addVertex(v1, &vertex);

    //Add v2//n2
    vertex.position[0] = m_vertexCoords[v2*3];
    vertex.position[1] = m_vertexCoords[v2*3+1];
    vertex.position[2] = m_vertexCoords[v2*3+2];
    vertex.normal[0] = m_normals[vn2*3];
    vertex.normal[1] = m_normals[vn2*3+1];
    vertex.normal[2] = m_normals[vn2*3+2];
    m_indexArray[index*3+2] = addVertex(v2, &vertex);
}

void OBJ::addTrianglePosTexCoord(unsigned int index, int material, int v0, int v1, int v2, int vt0, int vt1, int vt2)
{
    Vertex vertex;

    //Add the material index to the index for grouping models
    m_attributeArray[index] = material;

    //Add v0/vt0
    vertex.position[0] = m_vertexCoords[v0*3];
    vertex.position[1] = m_vertexCoords[v0*3+1];
    vertex.position[2] = m_vertexCoords[v0*3+2];
    vertex.texCoord[0] = m_textureCoords[vt0*2];
    vertex.texCoord[1] = m_textureCoords[vt0*2+1];
    m_indexArray[index*3] = addVertex(v0, &vertex);

    //Add v1/vt1
    vertex.position[0] = m_vertexCoords[v1*3];
    vertex.position[1] = m_vertexCoords[v1*3+1];
    vertex.position[2] = m_vertexCoords[v1*3+2];
    vertex.texCoord[0] = m_textureCoords[vt1*2];
    vertex.texCoord[1] = m_textureCoords[vt1*2+1];
    m_indexArray[index*3+1] = addVertex(v1, &vertex);

    //Add v2/vt2
    vertex.position[0] = m_vertexCoords[v2*3];
    vertex.position[1] = m_vertexCoords[v2*3+1];
    vertex.position[2] = m_vertexCoords[v2*3+2];
    vertex.texCoord[0] = m_textureCoords[vt2*2];
    vertex.texCoord[1] = m_textureCoords[vt2*2+1];
    m_indexArray[index*3+2] = addVertex(v2, &vertex);
}

void OBJ::addTrianglePosTexCoordNormal(unsigned int index, int material, int v0, int v1, int v2, int vt0, int vt1, int vt2, int vn0, int vn1, int vn2)
{
    Vertex vertex;

    //Add the material index to the index for grouping models
    m_attributeArray[index] = material;

    //Add v0/vt0/vn0
    vertex.position[0] = m_vertexCoords[v0*3];
    vertex.position[1] = m_vertexCoords[v0*3+1];
    vertex.position[2] = m_vertexCoords[v0*3+2];
    vertex.texCoord[0] = m_textureCoords[vt0*2];
    vertex.texCoord[1] = m_textureCoords[vt0*2+1];
    vertex.normal[0] = m_normals[vn0*3];
    vertex.normal[1] = m_normals[vn0*3+1];
    vertex.normal[2] = m_normals[vn0*3+2];
    m_indexArray[index*3] = addVertex(v0, &vertex);

    //Add v1/vt1/vn1
    vertex.position[0] = m_vertexCoords[v1*3];
    vertex.position[1] = m_vertexCoords[v1*3+1];
    vertex.position[2] = m_vertexCoords[v1*3+2];
    vertex.texCoord[0] = m_textureCoords[vt1*2];
    vertex.texCoord[1] = m_textureCoords[vt1*2+1];
    vertex.normal[0] = m_normals[vn1*3];
    vertex.normal[1] = m_normals[vn1*3+1];
    vertex.normal[2] = m_normals[vn1*3+2];
    m_indexArray[index*3+1] = addVertex(v1, &vertex);

    //Add v2/vt2/vn2
    vertex.position[0] = m_vertexCoords[v2*3];
    vertex.position[1] = m_vertexCoords[v2*3+1];
    vertex.position[2] = m_vertexCoords[v2*3+2];
    vertex.texCoord[0] = m_textureCoords[vt2*2];
    vertex.texCoord[1] = m_textureCoords[vt2*2+1];
    vertex.normal[0] = m_normals[vn2*3];
    vertex.normal[1] = m_normals[vn2*3+1];
    vertex.normal[2] = m_normals[vn2*3+2];
    m_indexArray[index*3+2] = addVertex(v2, &vertex);
}

int OBJ::addVertex(int hash, const Vertex *pVertex)
{
    unsigned int index = -1;
    std::map<int, std::vector<int> >::const_iterator iter = m_vertexCache.find(hash);

    if (iter == m_vertexCache.end())
    {
        // Vertex hash doesn't exist in the cache.
        index = static_cast<int>(m_vertexArray.size());
        m_vertexArray.push_back(*pVertex);
        m_vertexCache.insert(std::make_pair(hash, std::vector<int>(1, index)));
    }
    else
    {
        // One or more vertices have been hashed to this entry in the cache.
        const std::vector<int> &vertices = iter->second;
        const Vertex *pCachedVertex = 0;
        bool found = false;

        for (std::vector<int>::const_iterator i = vertices.begin(); i != vertices.end(); ++i)
        {
            index = *i;
            pCachedVertex = &m_vertexArray[index];

            if (memcmp(pCachedVertex, pVertex, sizeof(Vertex)) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            index = static_cast<int>(m_vertexArray.size());
            m_vertexArray.push_back(*pVertex);
            m_vertexCache[hash].push_back(index);
        }
    }

    return index;
}

void OBJ::buildStelModels()
{
    //Group model's triangles based on material
    StelModel* pStelModel = 0;
    int materialId = -1;
    int numStelModels = 0;

    // Count the number of meshes.
    for (int i=0; i<static_cast<int>(m_attributeArray.size()); ++i)
    {
        if (m_attributeArray[i] != materialId)
        {
            materialId = m_attributeArray[i];
            ++numStelModels;
        }
    }

    // Allocate memory for the StelModel and reset counters.
    m_numberOfStelModels = numStelModels;
    m_stelModels.resize(m_numberOfStelModels);
    numStelModels = 0;
    materialId = -1;

    // Build the StelModel. One StelModel for each unique material.
    for (int i=0; i<static_cast<int>(m_attributeArray.size()); ++i)
    {
        if (m_attributeArray[i] != materialId)
        {
            materialId = m_attributeArray[i];
            pStelModel = &m_stelModels[numStelModels++];
            pStelModel->pMaterial = &m_materials[materialId];
            pStelModel->startIndex = i*3;
            ++pStelModel->triangleCount;
        }
        else
        {
            ++pStelModel->triangleCount;
        }
    }

    // Sort the meshes based on its material alpha. Fully opaque meshes
    // towards the front and fully transparent towards the back.
    std::sort(m_stelModels.begin(), m_stelModels.end(), StelModelCompFunc);
}

void OBJ::generateNormals()
{
    const unsigned int *pTriangle = 0;
    Vertex *pVertex0 = 0;
    Vertex *pVertex1 = 0;
    Vertex *pVertex2 = 0;
    float edge1[3] = {0.0f, 0.0f, 0.0f};
    float edge2[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float length = 0.0f;
    int totalVertices = getNumberOfVertices();
    int totalTriangles = getNumberOfTriangles();

    // Initialize all the vertex normals.
    for (int i=0; i<totalVertices; ++i)
    {
        pVertex0 = &m_vertexArray[i];
        pVertex0->normal[0] = 0.0f;
        pVertex0->normal[1] = 0.0f;
        pVertex0->normal[2] = 0.0f;
    }

    // Calculate the vertex normals.
    for (int i=0; i<totalTriangles; ++i)
    {
        pTriangle = &m_indexArray[i*3];

        pVertex0 = &m_vertexArray[pTriangle[0]];
        pVertex1 = &m_vertexArray[pTriangle[1]];
        pVertex2 = &m_vertexArray[pTriangle[2]];

        // Calculate triangle face normal.
        edge1[0] = static_cast<float>(pVertex1->position[0] - pVertex0->position[0]);
        edge1[1] = static_cast<float>(pVertex1->position[1] - pVertex0->position[1]);
        edge1[2] = static_cast<float>(pVertex1->position[2] - pVertex0->position[2]);

        edge2[0] = static_cast<float>(pVertex2->position[0] - pVertex0->position[0]);
        edge2[1] = static_cast<float>(pVertex2->position[1] - pVertex0->position[1]);
        edge2[2] = static_cast<float>(pVertex2->position[2] - pVertex0->position[2]);

        normal[0] = (edge1[1]*edge2[2]) - (edge1[2]*edge2[1]);
        normal[1] = (edge1[2]*edge2[0]) - (edge1[0]*edge2[2]);
        normal[2] = (edge1[0]*edge2[1]) - (edge1[1]*edge2[0]);

        // Accumulate the normals.

        pVertex0->normal[0] += normal[0];
        pVertex0->normal[1] += normal[1];
        pVertex0->normal[2] += normal[2];

        pVertex1->normal[0] += normal[0];
        pVertex1->normal[1] += normal[1];
        pVertex1->normal[2] += normal[2];

        pVertex2->normal[0] += normal[0];
        pVertex2->normal[1] += normal[1];
        pVertex2->normal[2] += normal[2];
    }

    // Normalize the vertex normals.
    for (int i=0; i<totalVertices; ++i)
    {
        pVertex0 = &m_vertexArray[i];

        length = 1.0f / sqrtf(pVertex0->normal[0]*pVertex0->normal[0] +
                              pVertex0->normal[1]*pVertex0->normal[1] +
                              pVertex0->normal[2]*pVertex0->normal[2]);

        pVertex0->normal[0] *= length;
        pVertex0->normal[1] *= length;
        pVertex0->normal[2] *= length;
    }

    m_hasNormals = true;
}

void OBJ::generateTangents()
{
    const unsigned int *pTriangle = 0;
    Vertex *pVertex0 = 0;
    Vertex *pVertex1 = 0;
    Vertex *pVertex2 = 0;
    float edge1[3] = {0.0f, 0.0f, 0.0f};
    float edge2[3] = {0.0f, 0.0f, 0.0f};
    float texEdge1[2] = {0.0f, 0.0f};
    float texEdge2[2] = {0.0f, 0.0f};
    float tangent[3] = {0.0f, 0.0f, 0.0f};
    float bitangent[3] = {0.0f, 0.0f, 0.0f};
    float det = 0.0f;
    float nDotT = 0.0f;
    float bDotB = 0.0f;
    float length = 0.0f;
    int totalVertices = getNumberOfVertices();
    int totalTriangles = getNumberOfTriangles();

    // Initialize all the vertex tangents and bitangents.
    for (int i=0; i<totalVertices; ++i)
    {
        pVertex0 = &m_vertexArray[i];

        pVertex0->tangent[0] = 0.0f;
        pVertex0->tangent[1] = 0.0f;
        pVertex0->tangent[2] = 0.0f;
        pVertex0->tangent[3] = 0.0f;

        pVertex0->bitangent[0] = 0.0f;
        pVertex0->bitangent[1] = 0.0f;
        pVertex0->bitangent[2] = 0.0f;
    }

    // Calculate the vertex tangents and bitangents.
    for (int i=0; i<totalTriangles; ++i)
    {
        pTriangle = &m_indexArray[i*3];

        pVertex0 = &m_vertexArray[pTriangle[0]];
        pVertex1 = &m_vertexArray[pTriangle[1]];
        pVertex2 = &m_vertexArray[pTriangle[2]];

        // Calculate the triangle face tangent and bitangent.

        edge1[0] = static_cast<float>(pVertex1->position[0] - pVertex0->position[0]);
        edge1[1] = static_cast<float>(pVertex1->position[1] - pVertex0->position[1]);
        edge1[2] = static_cast<float>(pVertex1->position[2] - pVertex0->position[2]);

        edge2[0] = static_cast<float>(pVertex2->position[0] - pVertex0->position[0]);
        edge2[1] = static_cast<float>(pVertex2->position[1] - pVertex0->position[1]);
        edge2[2] = static_cast<float>(pVertex2->position[2] - pVertex0->position[2]);

        texEdge1[0] = pVertex1->texCoord[0] - pVertex0->texCoord[0];
        texEdge1[1] = pVertex1->texCoord[1] - pVertex0->texCoord[1];

        texEdge2[0] = pVertex2->texCoord[0] - pVertex0->texCoord[0];
        texEdge2[1] = pVertex2->texCoord[1] - pVertex0->texCoord[1];

        det = texEdge1[0]*texEdge2[1] - texEdge2[0]*texEdge1[1];

        if (fabs(det) < 1e-6f)
        {
            tangent[0] = 1.0f;
            tangent[1] = 0.0f;
            tangent[2] = 0.0f;

            bitangent[0] = 0.0f;
            bitangent[1] = 1.0f;
            bitangent[2] = 0.0f;
        }
        else
        {
            det = 1.0f / det;

            tangent[0] = (texEdge2[1]*edge1[0] - texEdge1[1]*edge2[0])*det;
            tangent[1] = (texEdge2[1]*edge1[1] - texEdge1[1]*edge2[1])*det;
            tangent[2] = (texEdge2[1]*edge1[2] - texEdge1[1]*edge2[2])*det;

            bitangent[0] = (-texEdge2[0]*edge1[0] + texEdge1[0]*edge2[0])*det;
            bitangent[1] = (-texEdge2[0]*edge1[1] + texEdge1[0]*edge2[1])*det;
            bitangent[2] = (-texEdge2[0]*edge1[2] + texEdge1[0]*edge2[2])*det;
        }

        // Accumulate the tangents and bitangents.

        pVertex0->tangent[0] += tangent[0];
        pVertex0->tangent[1] += tangent[1];
        pVertex0->tangent[2] += tangent[2];
        pVertex0->bitangent[0] += bitangent[0];
        pVertex0->bitangent[1] += bitangent[1];
        pVertex0->bitangent[2] += bitangent[2];

        pVertex1->tangent[0] += tangent[0];
        pVertex1->tangent[1] += tangent[1];
        pVertex1->tangent[2] += tangent[2];
        pVertex1->bitangent[0] += bitangent[0];
        pVertex1->bitangent[1] += bitangent[1];
        pVertex1->bitangent[2] += bitangent[2];

        pVertex2->tangent[0] += tangent[0];
        pVertex2->tangent[1] += tangent[1];
        pVertex2->tangent[2] += tangent[2];
        pVertex2->bitangent[0] += bitangent[0];
        pVertex2->bitangent[1] += bitangent[1];
        pVertex2->bitangent[2] += bitangent[2];
    }

    // Orthogonalize and normalize the vertex tangents.
    for (int i=0; i<totalVertices; ++i)
    {
        pVertex0 = &m_vertexArray[i];

        // Gram-Schmidt orthogonalize tangent with normal.

        nDotT = pVertex0->normal[0]*pVertex0->tangent[0] +
                pVertex0->normal[1]*pVertex0->tangent[1] +
                pVertex0->normal[2]*pVertex0->tangent[2];

        pVertex0->tangent[0] -= pVertex0->normal[0]*nDotT;
        pVertex0->tangent[1] -= pVertex0->normal[1]*nDotT;
        pVertex0->tangent[2] -= pVertex0->normal[2]*nDotT;

        // Normalize the tangent.

        length = 1.0f / sqrtf(pVertex0->tangent[0]*pVertex0->tangent[0] +
                              pVertex0->tangent[1]*pVertex0->tangent[1] +
                              pVertex0->tangent[2]*pVertex0->tangent[2]);

        pVertex0->tangent[0] *= length;
        pVertex0->tangent[1] *= length;
        pVertex0->tangent[2] *= length;

        // Calculate the handedness of the local tangent space.
        // The bitangent vector is the cross product between the triangle face
        // normal vector and the calculated tangent vector. The resulting
        // bitangent vector should be the same as the bitangent vector
        // calculated from the set of linear equations above. If they point in
        // different directions then we need to invert the cross product
        // calculated bitangent vector. We store this scalar multiplier in the
        // tangent vector's 'w' component so that the correct bitangent vector
        // can be generated in the normal mapping shader's vertex shader.
        //
        // Normal maps have a left handed coordinate system with the origin
        // located at the top left of the normal map texture. The x coordinates
        // run horizontally from left to right. The y coordinates run
        // vertically from top to bottom. The z coordinates run out of the
        // normal map texture towards the viewer. Our handedness calculations
        // must take this fact into account as well so that the normal mapping
        // shader's vertex shader will generate the correct bitangent vectors.
        // Some normal map authoring tools such as Crazybump
        // (http://www.crazybump.com/) includes options to allow you to control
        // the orientation of the normal map normal's y-axis.

        bitangent[0] = (pVertex0->normal[1]*pVertex0->tangent[2]) -
                       (pVertex0->normal[2]*pVertex0->tangent[1]);
        bitangent[1] = (pVertex0->normal[2]*pVertex0->tangent[0]) -
                       (pVertex0->normal[0]*pVertex0->tangent[2]);
        bitangent[2] = (pVertex0->normal[0]*pVertex0->tangent[1]) -
                       (pVertex0->normal[1]*pVertex0->tangent[0]);

        bDotB = bitangent[0]*pVertex0->bitangent[0] +
                bitangent[1]*pVertex0->bitangent[1] +
                bitangent[2]*pVertex0->bitangent[2];

        pVertex0->tangent[3] = (bDotB < 0.0f) ? 1.0f : -1.0f;

        pVertex0->bitangent[0] = bitangent[0];
        pVertex0->bitangent[1] = bitangent[1];
        pVertex0->bitangent[2] = bitangent[2];
    }

    m_hasTangents = true;
}

void OBJ::importFirstPass(FILE *pFile)
{
    m_hasTextureCoords = false;
    m_hasNormals = false;

    m_numberOfVertexCoords = 0;
    m_numberOfTextureCoords = 0;
    m_numberOfNormals = 0;
    m_numberOfTriangles = 0;

    unsigned int v = 0;
    unsigned int vt = 0;
    unsigned int vn = 0;
    char buffer[256] = {0};
    std::string name;

    while (fscanf(pFile, "%s", buffer) != EOF)
    {
        switch (buffer[0])
        {
        case 'f':   //! v, v//vn, v/vt or v/vt/vn.
            fscanf(pFile, "%s", buffer);

            if (strstr(buffer, "//"))   //! v//vn
            {
                sscanf(buffer, "%d//%d", &v, &vn);
                fscanf(pFile, "%d//%d", &v, &vn);
                fscanf(pFile, "%d//%d", &v, &vn);
                ++m_numberOfTriangles;

                while (fscanf(pFile, "%d//%d", &v, &vn) > 0)
                    ++m_numberOfTriangles;
            }
            else if (sscanf(buffer, "%d/%d/%d", &v, &vt, &vn) == 3) //! v/vt/vn
            {
                fscanf(pFile, "%d/%d/%d", &v, &vt, &vn);
                fscanf(pFile, "%d/%d/%d", &v, &vt, &vn);
                ++m_numberOfTriangles;

                while (fscanf(pFile, "%d/%d/%d", &v, &vt, &vn) > 0)
                    ++m_numberOfTriangles;
            }
            else if (sscanf(buffer, "%d/%d", &v, &vt) == 2) //! v/vt
            {
                fscanf(pFile, "%d/%d", &v, &vt);
                fscanf(pFile, "%d/%d", &v, &vt);
                ++m_numberOfTriangles;

                while (fscanf(pFile, "%d/%d", &v, &vt) > 0)
                    ++m_numberOfTriangles;
            }
            else // v
            {
                fscanf(pFile, "%d", &v);
                fscanf(pFile, "%d", &v);
                ++m_numberOfTriangles;

                while (fscanf(pFile, "%d", &v) > 0)
                    ++m_numberOfTriangles;
            }
            break;

        case 'm':   //! mtllib
            fgets(buffer, sizeof(buffer), pFile);
            sscanf(buffer, "%s %s", buffer, buffer);
            name = m_basePath;
            name += buffer;
            importMaterials(name.c_str());
            break;

        case 'v':   //! v, vt, or vn
            switch (buffer[1])
            {
            case '\0':
                fgets(buffer, sizeof(buffer), pFile);
                ++m_numberOfVertexCoords;
                break;

            case 'n':
                fgets(buffer, sizeof(buffer), pFile);
                ++m_numberOfNormals;
                break;

            case 't':
                fgets(buffer, sizeof(buffer), pFile);
                ++m_numberOfTextureCoords;

            default:
                break;
            }
            break;

        default:
            fgets(buffer, sizeof(buffer), pFile);
            break;
        }
    }

    m_hasPositions = m_numberOfVertexCoords > 0;
    m_hasNormals = m_numberOfNormals > 0;
    m_hasTextureCoords = m_numberOfTextureCoords > 0;

    // Allocate memory for the OBJ model data.
    m_vertexCoords.resize(m_numberOfVertexCoords * 3);
    m_textureCoords.resize(m_numberOfTextureCoords * 2);
    m_normals.resize(m_numberOfNormals * 3);
    m_indexArray.resize(m_numberOfTriangles * 3);
    m_attributeArray.resize(m_numberOfTriangles);

    // Define a default material if no materials were loaded.
    if (m_numberOfMaterials == 0)
    {
        Material defaultMaterial;

        m_materials.push_back(defaultMaterial);
        m_materialCache[defaultMaterial.name] = 0;
    }
}

void OBJ::importSecondPass(FILE *pFile, const vertexOrder order)
{
    unsigned int v[3] = {0};
    unsigned int vt[3] = {0};
    unsigned int vn[3] = {0};
    int numVertices = 0;
    int numTexCoords = 0;
    int numNormals = 0;
    unsigned int numTriangles = 0;
    int activeMaterial = 0;
    char buffer[256] = {0};
    std::string name;
    std::map<std::string, int>::const_iterator iter;

    float fTmp[3] = {0.0f};
    double dTmp[3] = {0.0};

    while (fscanf(pFile, "%s", buffer) != EOF)
    {
        switch (buffer[0])
        {
        case 'f': //! v, v//vn, v/vt, or v/vt/vn.
            v[0]  = v[1]  = v[2]  = 0;
            vt[0] = vt[1] = vt[2] = 0;
            vn[0] = vn[1] = vn[2] = 0;

            fscanf(pFile, "%s", buffer);

            if (strstr(buffer, "//")) //! v//vn
            {
                sscanf(buffer, "%d//%d", &v[0], &vn[0]);
                fscanf(pFile, "%d//%d", &v[1], &vn[1]);
                fscanf(pFile, "%d//%d", &v[2], &vn[2]);                

                v[0] = (v[0] < 0) ? v[0]+numVertices-1 : v[0]-1;
                v[1] = (v[1] < 0) ? v[1]+numVertices-1 : v[1]-1;
                v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;

                vn[0] = (vn[0] < 0) ? vn[0]+numNormals-1 : vn[0]-1;
                vn[1] = (vn[1] < 0) ? vn[1]+numNormals-1 : vn[1]-1;
                vn[2] = (vn[2] < 0) ? vn[2]+numNormals-1 : vn[2]-1;

                addTrianglePosNormal(numTriangles++, activeMaterial,
                                     v[0], v[1], v[2],
                                     vn[0], vn[1], vn[2]);

                v[1] = v[2];
                vn[1] = vn[2];

                while (fscanf(pFile, "%d//%d", &v[2], &vn[2]) > 0)
                {
                    v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;
                    vn[2] = (vn[2] < 0) ? vn[2]+numNormals-1 : vn[2]-1;

                    addTrianglePosNormal(numTriangles++, activeMaterial,
                                         v[0], v[1], v[2],
                                         vn[0], vn[1], vn[2]);

                    v[1] = v[2];
                    vn[1] = vn[2];
                }
            }
            else if (sscanf(buffer, "%d/%d/%d", &v[0], &vt[0], &vn[0]) == 3) //! v/vt/vn
            {
                fscanf(pFile, "%d/%d/%d", &v[1], &vt[1], &vn[1]);
                fscanf(pFile, "%d/%d/%d", &v[2], &vt[2], &vn[2]);

                v[0] = (v[0] < 0) ? v[0]+numVertices-1 : v[0]-1;
                v[1] = (v[1] < 0) ? v[1]+numVertices-1 : v[1]-1;
                v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;

                vt[0] = (vt[0] < 0) ? vt[0]+numTexCoords-1 : vt[0]-1;
                vt[1] = (vt[1] < 0) ? vt[1]+numTexCoords-1 : vt[1]-1;
                vt[2] = (vt[2] < 0) ? vt[2]+numTexCoords-1 : vt[2]-1;

                vn[0] = (vn[0] < 0) ? vn[0]+numNormals-1 : vn[0]-1;
                vn[1] = (vn[1] < 0) ? vn[1]+numNormals-1 : vn[1]-1;
                vn[2] = (vn[2] < 0) ? vn[2]+numNormals-1 : vn[2]-1;

                addTrianglePosTexCoordNormal(numTriangles++, activeMaterial,
                                             v[0], v[1], v[2],
                                             vt[0], vt[1], vt[2],
                                             vn[0], vn[1], vn[2]);

                v[1] = v[2];
                vt[1] = vt[2];
                vn[1] = vn[2];

                while (fscanf(pFile, "%d/%d/%d", &v[2], &vt[2], &vn[2]) > 0)
                {
                    v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;
                    vt[2] = (vt[2] < 0) ? vt[2]+numTexCoords-1 : vt[2]-1;
                    vn[2] = (vn[2] < 0) ? vn[2]+numNormals-1 : vn[2]-1;

                    addTrianglePosTexCoordNormal(numTriangles++, activeMaterial,
                                                 v[0], v[1], v[2],
                                                 vt[0], vt[1], vt[2],
                                                 vn[0], vn[1], vn[2]);

                    v[1] = v[2];
                    vt[1] = vt[2];
                    vn[1] = vn[2];
                }
            }
            else if (sscanf(buffer, "%d/%d", &v[0], &vt[0]) == 2) //! v/vt
            {
                fscanf(pFile, "%d/%d", &v[1], &vt[1]);
                fscanf(pFile, "%d/%d", &v[2], &vt[2]);

                v[0] = (v[0] < 0) ? v[0]+numVertices-1 : v[0]-1;
                v[1] = (v[1] < 0) ? v[1]+numVertices-1 : v[1]-1;
                v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;

                vt[0] = (vt[0] < 0) ? vt[0]+numTexCoords-1 : vt[0]-1;
                vt[1] = (vt[1] < 0) ? vt[1]+numTexCoords-1 : vt[1]-1;
                vt[2] = (vt[2] < 0) ? vt[2]+numTexCoords-1 : vt[2]-1;

                addTrianglePosTexCoord(numTriangles++, activeMaterial,
                                       v[0], v[1], v[2],
                                       vt[0], vt[1], vt[2]);

                v[1] = v[2];
                vt[1] = vt[2];

                while (fscanf(pFile, "%d/%d", &v[2], &vt[2]) > 0)
                {
                    v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;
                    vt[2] = (vt[2] < 0) ? vt[2]+numTexCoords-1 : vt[2]-1;

                    addTrianglePosTexCoord(numTriangles++, activeMaterial,
                                           v[0], v[1], v[2],
                                           vt[0], vt[1], vt[2]);

                    v[1] = v[2];
                    vt[1] = vt[2];
                }
            }
            else //! v
            {
                sscanf(buffer, "%d", &v[0]);
                fscanf(pFile, "%d", &v[1]);
                fscanf(pFile, "%d", &v[2]);

                v[0] = (v[0] < 0) ? v[0]+numVertices-1 : v[0]-1;
                v[1] = (v[1] < 0) ? v[1]+numVertices-1 : v[1]-1;
                v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;

                addTrianglePos(numTriangles++, activeMaterial, v[0], v[1], v[2]);

                v[1] = v[2];

                while (fscanf(pFile, "%d", &v[2]) > 0)
                {
                    v[2] = (v[2] < 0) ? v[2]+numVertices-1 : v[2]-1;

                    addTrianglePos(numTriangles++, activeMaterial, v[0], v[1], v[2]);

                    v[1] = v[2];
                }
            }
            break;

        case 'u': //! usemtl
            fgets(buffer, sizeof(buffer), pFile);
            sscanf(buffer, "%s %s", buffer, buffer);
            name = buffer;
            iter = m_materialCache.find(buffer);
            activeMaterial = (iter == m_materialCache.end()) ? 0 : iter->second;
            break;

        case 'v': //! v, vn, or vt.
            switch (buffer[1])
            {
            case '\0': //! v
                fscanf(pFile, "%lf %lf %lf", &dTmp[0], &dTmp[1], &dTmp[2]);

                switch(order)
                {
                case XZY:
                    m_vertexCoords[3*numVertices] = dTmp[0];
                    m_vertexCoords[3*numVertices+1] = -dTmp[2];
                    m_vertexCoords[3*numVertices+2] = dTmp[1];
                    break;

                default: //! XYZ
                    m_vertexCoords[3*numVertices] = dTmp[0];
                    m_vertexCoords[3*numVertices+1] = dTmp[1];
                    m_vertexCoords[3*numVertices+2] = dTmp[2];
                    break;
                }

                ++numVertices;
                break;

            case 'n': //! vn
                fscanf(pFile, "%f %f %f", &fTmp[0], &fTmp[1], &fTmp[2]);

                switch(order)
                {
                case XZY:
                    m_normals[3*numNormals] = fTmp[0];
                    m_normals[3*numNormals+1] = -fTmp[2];
                    m_normals[3*numNormals+2] = fTmp[1];
                    break;

                default: //! XYZ
                    m_normals[3*numNormals] = fTmp[0];
                    m_normals[3*numNormals+1] = fTmp[1];
                    m_normals[3*numNormals+2] = fTmp[2];
                    break;
                }

                ++numNormals;
                break;

            case 't': //! vt
                fscanf(pFile, "%f %f",
                       &m_textureCoords[2*numTexCoords],
                       &m_textureCoords[2*numTexCoords+1]);
                ++numTexCoords;
                break;

            default:
                //Everything else shall not be parsed
                break;
            }
            break;

        default:
            fgets(buffer, sizeof(buffer), pFile);
            break;
        }
    }
}

bool OBJ::importMaterials(const char *filename)
{
    FILE* pFile = fopen(filename, "r");

    if (!pFile)
        return false;

    Material* pMaterial = 0;
    int illum = 0;
    int numMaterials = 0;
    char buffer[256] = {0};

    // Count the number of materials in the MTL file.
    while (fscanf(pFile, "%s", buffer) != EOF)
    {
        switch (buffer[0])
        {
        case 'n': //! newmtl
            ++numMaterials;
            fgets(buffer, sizeof(buffer), pFile);
            sscanf(buffer, "%s %s", buffer, buffer);
            break;

        default:
            fgets(buffer, sizeof(buffer), pFile);
            break;
        }
    }

    rewind(pFile);

    m_numberOfMaterials = numMaterials;
    numMaterials = 0;
    m_materials.resize(m_numberOfMaterials);

    // Load the materials in the MTL file.
    while (fscanf(pFile, "%s", buffer) != EOF)
    {
        switch (buffer[0])
        {
        case 'N':
            switch (buffer[1])
            {
            case 's': //! Ns
                fscanf(pFile, "%f", &pMaterial->shininess);
                pMaterial->shininess = qMin(128.0f, pMaterial->shininess);
                break;

            default:
                break;
            }

            break;

        case 'K': // Ka, Kd, or Ks
            switch (buffer[1])
            {
            case 'a': //! Ka
                fscanf(pFile, "%f %f %f",
                       &pMaterial->ambient[0],
                       &pMaterial->ambient[1],
                       &pMaterial->ambient[2]);
                pMaterial->ambient[3] = 1.0f;
                break;

            case 'd': //! Kd
                fscanf(pFile, "%f %f %f",
                       &pMaterial->diffuse[0],
                       &pMaterial->diffuse[1],
                       &pMaterial->diffuse[2]);
                pMaterial->diffuse[3] = 1.0f;
                break;

            case 's': //! Ks
                fscanf(pFile, "%f %f %f",
                       &pMaterial->specular[0],
                       &pMaterial->specular[1],
                       &pMaterial->specular[2]);
                pMaterial->specular[3] = 1.0f;
                break;

            case 'e': //! Ke
                fscanf(pFile, "%f %f %f",
                       &pMaterial->emission[0],
                       &pMaterial->emission[1],
                       &pMaterial->emission[2]);
                pMaterial->emission[3] = 1.0f;
                break;

            default:
                fgets(buffer, sizeof(buffer), pFile);
                break;
            }
            break;

        case 'T': //! Tr
            switch (buffer[1])
            {
            case 'r': //! Tr
                fscanf(pFile, "%f", &pMaterial->alpha);
                break;

            default:
                fgets(buffer, sizeof(buffer), pFile);
                break;
            }
            break;

        case 'd':
            fscanf(pFile, "%f", &pMaterial->alpha);
            break;

        case 'i': // illum
            fscanf(pFile, "%d", &illum);

            pMaterial->illum = (Illum) illum;
            break;

        case 'm': //! map_Kd, map_bump
            if (strstr(buffer, "map_Kd") != 0)
            {
                fgets(buffer, sizeof(buffer), pFile);
                sscanf("%[^\n]", buffer);

                std::string tex;
                parseTextureString(buffer, tex);
                pMaterial->textureName = tex;
            }
            else if (strstr(buffer, "map_bump") != 0)
            {
                fgets(buffer, sizeof(buffer), pFile);
                sscanf(buffer, "%[^\n]", buffer);

                std::string bump;
                parseTextureString(buffer, bump);
                pMaterial->bumpMapName = bump;
            }
            else
            {
                fgets(buffer, sizeof(buffer), pFile);
            }
            break;

        case 'n': //! newmtl
            fgets(buffer, sizeof(buffer), pFile);
            sscanf(buffer, "%s %s", buffer, buffer);

            pMaterial = &m_materials[numMaterials];
            pMaterial->ambient[0] = 0.2f;
            pMaterial->ambient[1] = 0.2f;
            pMaterial->ambient[2] = 0.2f;
            pMaterial->ambient[3] = 1.0f;
            pMaterial->diffuse[0] = 0.8f;
            pMaterial->diffuse[1] = 0.8f;
            pMaterial->diffuse[2] = 0.8f;
            pMaterial->diffuse[3] = 1.0f;
            pMaterial->specular[0] = 0.0f;
            pMaterial->specular[1] = 0.0f;
            pMaterial->specular[2] = 0.0f;
            pMaterial->specular[3] = 1.0f;
            pMaterial->shininess = 0.0f;
            pMaterial->alpha = 1.0f;
            pMaterial->name = buffer;
            pMaterial->textureName.clear();
            pMaterial->texture.clear();
            pMaterial->bumpMapName.clear();
            pMaterial->bump_texture.clear();


            m_materialCache[pMaterial->name] = numMaterials;
            ++numMaterials;
            break;

        default:
            fgets(buffer, sizeof(buffer), pFile);
            break;
        }
    }

    fclose(pFile);

    return true;
}

void OBJ::uploadTexturesGL()
{
    StelTextureMgr textureMgr;

    for(int i=0; i<m_numberOfMaterials; ++i)
    {
        Material* pMaterial = &getMaterial(i);

//        qDebug() << getTime() << "[Scenery3d]" << pMaterial->name.c_str();
//        qDebug() << getTime() << "[Scenery3d] Ka:" << pMaterial->ambient[0] << "," << pMaterial->ambient[1] << "," << pMaterial->ambient[2] << "," << pMaterial->ambient[3];
//        qDebug() << getTime() << "[Scenery3d] Kd:" << pMaterial->diffuse[0] << "," << pMaterial->diffuse[1] << "," << pMaterial->diffuse[2] << "," << pMaterial->diffuse[3];
//        qDebug() << getTime() << "[Scenery3d] Ks:" << pMaterial->specular[0] << "," << pMaterial->specular[1] << "," << pMaterial->specular[2] << "," << pMaterial->specular[3];
//        qDebug() << getTime() << "[Scenery3d] Shininess:" << pMaterial->shininess;
//        qDebug() << getTime() << "[Scenery3d] Alpha:" << pMaterial->alpha;
//        qDebug() << getTime() << "[Scenery3d] Illum:" << pMaterial->illum;

        qDebug() << getTime() << "[Scenery3d] Uploading textures for Material: " << pMaterial->name.c_str();
        qDebug() << getTime() << "[Scenery3d] Texture:" << pMaterial->textureName.c_str();
        if(!pMaterial->textureName.empty())
        {
            StelTextureSP tex = textureMgr.createTexture(QString(absolutePath(pMaterial->textureName).c_str()), StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));
            if(!tex.isNull())
            {
                pMaterial->texture = tex;
            }
            else
            {
                qWarning() << getTime() << "[Scenery3d] Failed to load Texture:" << pMaterial->textureName.c_str();
            }
        }

        qDebug() << getTime() << "[Scenery3d] Normal Map:" << pMaterial->bumpMapName.c_str();
        if(!pMaterial->bumpMapName.empty())
        {
            StelTextureSP bumpTex = textureMgr.createTexture(QString(absolutePath(pMaterial->bumpMapName).c_str()), StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));
            if(!bumpTex.isNull())
            {
                pMaterial->bump_texture = bumpTex;
            }
            else
            {
                qWarning() << getTime() << "[Scenery3d] Failed to load Normal Map:" << pMaterial->bumpMapName.c_str();
            }
        }
    }
}

void OBJ::transform(Mat4d mat)
{
    m = mat;
    pBoundingBox->min = Vec3f(std::numeric_limits<float>::max());
    pBoundingBox->max = Vec3f(-std::numeric_limits<float>::max());

    //Transform all vertices and normals by mat
    for(int i=0; i<m_numberOfVertexCoords; ++i)
    {
        Vertex *pVertex = &m_vertexArray[i];

        Vec3d pos = Vec3d(pVertex->position[0], pVertex->position[1], pVertex->position[2]);
        mat.transfo(pos);
        pVertex->position[0] = pos[0];
        pVertex->position[1] = pos[1];
        pVertex->position[2] = pos[2];

        Vec3d nor = Vec3d(pVertex->normal[0], pVertex->normal[1], pVertex->normal[2]);
        mat.transfo(nor);
        pVertex->normal[0] = nor[0];
        pVertex->normal[1] = nor[1];
        pVertex->normal[2] = nor[2];

        Vec3d tan = Vec3d(pVertex->tangent[0], pVertex->tangent[1], pVertex->tangent[2]);
        mat.transfo(tan);
        pVertex->tangent[0] = tan[0];
        pVertex->tangent[1] = tan[1];
        pVertex->tangent[2] = tan[2];

        Vec3d biTan = Vec3d(pVertex->bitangent[0], pVertex->bitangent[1], pVertex->bitangent[2]);
        mat.transfo(biTan);
        pVertex->bitangent[0] = biTan[0];
        pVertex->bitangent[1] = biTan[1];
        pVertex->bitangent[2] = biTan[2];

        //Update bounding box in case it changed
        pBoundingBox->min = Vec3f(std::min(static_cast<float>(pVertex->position[0]), pBoundingBox->min[0]),
                                  std::min(static_cast<float>(pVertex->position[1]), pBoundingBox->min[1]),
                                  std::min(static_cast<float>(pVertex->position[2]), pBoundingBox->min[2]));

        pBoundingBox->max = Vec3f(std::max(static_cast<float>(pVertex->position[0]), pBoundingBox->max[0]),
                                  std::max(static_cast<float>(pVertex->position[1]), pBoundingBox->max[1]),
                                  std::max(static_cast<float>(pVertex->position[2]), pBoundingBox->max[2]));
    }
}

void OBJ::bounds()
{
    //Find Bounding Box for entire Scene
    pBoundingBox->min = Vec3f(std::numeric_limits<float>::max());
    pBoundingBox->max = Vec3f(-std::numeric_limits<float>::max());

    for(int i=0; i<m_numberOfVertexCoords; ++i)
    {
        Vertex* pVertex = &m_vertexArray[i];

        pBoundingBox->min = Vec3f(std::min(static_cast<float>(pVertex->position[0]), pBoundingBox->min[0]),
                                  std::min(static_cast<float>(pVertex->position[1]), pBoundingBox->min[1]),
                                  std::min(static_cast<float>(pVertex->position[2]), pBoundingBox->min[2]));

        pBoundingBox->max = Vec3f(std::max(static_cast<float>(pVertex->position[0]), pBoundingBox->max[0]),
                                  std::max(static_cast<float>(pVertex->position[1]), pBoundingBox->max[1]),
                                  std::max(static_cast<float>(pVertex->position[2]), pBoundingBox->max[2]));
    }

    //Find AABB per Stel Model
    for(int i=0; i<m_numberOfStelModels; ++i)
    {
        StelModel* pStelModel = &m_stelModels[i];
        pStelModel->bbox = new AABB(Vec3f(std::numeric_limits<float>::max()), Vec3f(-std::numeric_limits<float>::max()));

        for(int j=pStelModel->startIndex; j<pStelModel->triangleCount*3; ++j)
        {
            unsigned int vertexIndex = m_indexArray[j];
            Vertex* pVertex = &m_vertexArray[vertexIndex];

            pStelModel->bbox->min = Vec3f(std::min(static_cast<float>(pVertex->position[0]), pStelModel->bbox->min[0]),
                                          std::min(static_cast<float>(pVertex->position[1]), pStelModel->bbox->min[1]),
                                          std::min(static_cast<float>(pVertex->position[2]), pStelModel->bbox->min[2]));

            pStelModel->bbox->max = Vec3f(std::max(static_cast<float>(pVertex->position[0]), pStelModel->bbox->max[0]),
                                          std::max(static_cast<float>(pVertex->position[1]), pStelModel->bbox->max[1]),
                                          std::max(static_cast<float>(pVertex->position[2]), pStelModel->bbox->max[2]));
        }
    }
}

void OBJ::renderAABBs()
{
    for(int i=0; i<m_numberOfStelModels; ++i)
    {
        StelModel* pStelModel = &m_stelModels[i];
        pStelModel->bbox->render(&m);
    }
}
