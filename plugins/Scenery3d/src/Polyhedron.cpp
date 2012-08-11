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

/**
  * This implementation is based on Stingl's Robust Hard Shadows. */

#include "Polyhedron.hpp"
#include "Util.hpp"
#include <limits>

Polyhedron::Polyhedron()
{
    clear();
}

Polyhedron::~Polyhedron()
{
    clear();
}

void Polyhedron::clear()
{
    for(unsigned int i=0; i<static_cast<unsigned int>(polygons.size()); i++)
    {
        delete polygons[i];
    }

    polygons.clear();
    uniqueVerts.clear();
}

void Polyhedron::add(Frustum &f)
{
    //Front
    add(new SPolygon(f.getCorner(Frustum::NBL), f.getCorner(Frustum::NBR), f.getCorner(Frustum::NTR), f.getCorner(Frustum::NTL)));
    //Back
    add(new SPolygon(f.getCorner(Frustum::FTL), f.getCorner(Frustum::FTR), f.getCorner(Frustum::FBR), f.getCorner(Frustum::FBL)));
    //Left
    add(new SPolygon(f.getCorner(Frustum::NBL), f.getCorner(Frustum::NTL), f.getCorner(Frustum::FTL), f.getCorner(Frustum::FBL)));
    //Right
    add(new SPolygon(f.getCorner(Frustum::NBR), f.getCorner(Frustum::FBR), f.getCorner(Frustum::FTR), f.getCorner(Frustum::NTR)));
    //Bottom
    add(new SPolygon(f.getCorner(Frustum::FBL), f.getCorner(Frustum::FBR), f.getCorner(Frustum::NBR), f.getCorner(Frustum::NBL)));
    //Top
    add(new SPolygon(f.getCorner(Frustum::FTR), f.getCorner(Frustum::FTL), f.getCorner(Frustum::NTL), f.getCorner(Frustum::NTR)));
}

void Polyhedron::add(SPolygon *p)
{
    if(p) polygons.push_back(p);
}

void Polyhedron::add(const std::vector<Vec3f> &verts, const Vec3f &normal)
{
    if(verts.size() < 3) return;

    SPolygon *p = new SPolygon();

    for(unsigned int i=0; i<verts.size(); i++)
    {
        p->addUniqueVert(verts[i]);
    }

    if(p->vertices.size() < 3)
    {
        delete p;
        return;
    }

    //Check normal direction
    Plane polyPlane(p->vertices[0], p->vertices[1], p->vertices[2], SPolygon::CCW);

    //Might need to reverse the vertex order
    if(polyPlane.normal.dot(normal) < 0.0f) p->reverseOrder();

    polygons.push_back(p);
}

void Polyhedron::intersect(const AABB &bb)
{
    for(unsigned int i=0; i<AABB::PLANECOUNT; i++)
    {
        intersect(Plane(bb.getEquation(static_cast<AABB::Plane>(i))));
    }
}

void Polyhedron::intersect(const Plane &p)
{
    //Save intersection points
    std::vector<Vec3f> intersectionPoints;

    //Iterate over this polyhedron's polygons
    for(std::vector<SPolygon*>::iterator it = polygons.begin(); it != polygons.end();)
    {
        //Retrive the polygon and intersect it with the specified plane, save the intersection points
        (*it)->intersect(p, intersectionPoints);

        //If all vertices were clipped, remove the polygon
        if(!(*it)->vertices.size())
        {
            delete (*it);
            it = polygons.erase(it);
        }
        else
        {
            it++;
        }
    }

    //We need to add a closing polygon
    if(intersectionPoints.size()) add(intersectionPoints, p.normal);
}

void Polyhedron::intersect(const Line &l, const Vec3f &min, const Vec3f &max, std::vector<Vec3f> &vertices)
{
    const Vec3f &dir = l.direction;
    const Vec3f &p = l.startPoint;

    float t1 = 0.0f;
    float t2 = std::numeric_limits<float>::infinity();

    bool intersect = clip(-dir.v[0], p.v[0]-min.v[0], t1, t2) && clip(dir.v[0], max.v[0]-p.v[0], t1, t2) &&
                     clip(-dir.v[1], p.v[1]-min.v[1], t1, t2) && clip(dir.v[1], max.v[1]-p.v[1], t1, t2) &&
                     clip(-dir.v[2], p.v[2]-min.v[2], t1, t2) && clip(dir.v[2], max.v[2]-p.v[2], t1, t2);


    if(!intersect) return;

    Vec3f newPoint;
    intersect = false;

    if(t1 >= 0.0f)
    {
        newPoint = p + t1*dir;
        intersect = true;
    }

    if(t2 >= 0.0f)
    {
        newPoint = p + t2*dir;
        intersect = true;
    }

    if(intersect) vertices.push_back(newPoint);
}

bool Polyhedron::clip(float p, float q, float &u1, float &u2) const
{
    if(p < 0.0f)
    {
        float r = q/p;

        if(r > u2)
        {
            return false;
        }
        else
        {
            if(r > u1)
            {
                u1 = r;
            }

            return true;
        }
    }
    else
    {
        if(p > 0.0f)
        {
            float r = q/p;

            if(r < u1)
            {
                return false;
            }
            else
            {
                if(r < u2)
                {
                    u2 = r;
                }

                return true;
            }
        }
        else
        {
            return q >= 0.0f;
        }
    }
}

void Polyhedron::extrude(const Vec3f &dir, const AABB &bb)
{
    makeUniqueVerts();

    const Vec3f &min = bb.min;
    const Vec3f &max = bb.max;

    unsigned int size = static_cast<unsigned int>(uniqueVerts.size());

    for(unsigned int i=0; i<size; i++)
    {
        intersect(Line(uniqueVerts[i], dir), min, max, uniqueVerts);
    }
}

void Polyhedron::makeUniqueVerts()
{
    uniqueVerts.clear();

    for(unsigned int i=0; i<polygons.size(); i++)
    {
        std::vector<Vec3f> &verts = polygons[i]->vertices;

        for(unsigned int j=0; j<verts.size(); j++)
        {
            addUniqueVert(verts[j]);
        }
    }
}

void Polyhedron::addUniqueVert(const Vec3f &v)
{
    bool flag = true;

    for(unsigned int i=0; i<uniqueVerts.size() && flag; i++)
    {
        flag = !(CompareVerts(v, uniqueVerts[i]));
    }

    if(flag) uniqueVerts.push_back(v);
}

unsigned int Polyhedron::getVertCount()
{
    return static_cast<unsigned int>(uniqueVerts.size());
}

const std::vector<Vec3f> &Polyhedron::getVerts() const
{
    return uniqueVerts;
}
