#include "Frustum.hpp"
#include "Util.hpp"
#include <QtOpenGL>
#include <limits>

Frustum::Frustum()
{
    for(unsigned int i=0; i<CORNERCOUNT; i++)
    {
        corners.push_back(Vec3f(0.0f, 0.0f, 0.0f));
        drawCorners.push_back(Vec3f(0.0f, 0.0f, 0.0f));
    }

    for(unsigned int i=0; i<PLANECOUNT; i++)
    {
        planes.push_back(Plane());
    }

    fov = 0.0f;
    aspect = 0.0f;
    zNear = 0.0f;
    zFar = 0.0f;
    bbox = new AABB(Vec3f(0.0f), Vec3f(0.0f));
    drawBbox = new AABB(Vec3f(0.0f), Vec3f(0.0f));
}

Frustum::~Frustum()
{
    delete bbox;
    delete drawBbox;
}

const Vec3f& Frustum::getCorner(Corner corner) const
{
    return corners[corner];
}

const Plane& Frustum::getPlane(FrustumPlane plane) const
{
    return planes[plane];
}

void Frustum::calcFrustum(Vec3d p, Vec3d l, Vec3d u)
{
    Vec3d Y = -l;
    Y.normalize();

    Vec3d X = u^Y;
    X.normalize();

    Vec3d Z = Y^X;
    Y.normalize();

    float tang = tanf((static_cast<float>(M_PI)/360.0f)*fov);
    float nh = zNear * tang;
    float nw = nh * aspect;
    float fh = zFar * tang;
    float fw = fh * aspect;

    Vec3d nc = p - Y*zNear;
    Vec3d fc = p - Y*zFar;

    Vec3d ntl = nc + Z * nh - X * nw;
    Vec3d ntr = nc + Z * nh + X * nw;
    Vec3d nbl = nc - Z * nh - X * nw;
    Vec3d nbr = nc - Z * nh + X * nw;

    Vec3d ftl = fc + Z * fh - X * fw;
    Vec3d ftr = fc + Z * fh + X * fw;
    Vec3d fbl = fc - Z * fh - X * fw;
    Vec3d fbr = fc - Z * fh + X * fw;

    corners[NTL] = vecdToFloat(ntl);
    corners[NTR] = vecdToFloat(ntr);
    corners[NBL] = vecdToFloat(nbl);
    corners[NBR] = vecdToFloat(nbr);
    corners[FTL] = vecdToFloat(ftl);
    corners[FTR] = vecdToFloat(ftr);
    corners[FBL] = vecdToFloat(fbl);
    corners[FBR] = vecdToFloat(fbr);

    planes[TOP].setPoints(corners[NTR], corners[NTL], corners[FTL]);
    planes[BOTTOM].setPoints(corners[NBL], corners[NBR], corners[FBR]);
    planes[LEFT].setPoints(corners[NTL], corners[NBL], corners[FBL]);
    planes[RIGHT].setPoints(corners[NBR], corners[NTR], corners[FBR]);
    planes[NEARP].setPoints(corners[NTL], corners[NTR], corners[NBR]);
    planes[FARP].setPoints(corners[FTR], corners[FTL], corners[FBL]);


    bbox = new AABB(Vec3f(std::numeric_limits<float>::max()), Vec3f(-std::numeric_limits<float>::max()));

    for(unsigned int i=0; i<CORNERCOUNT; i++)
    {
        Vec3f curVert = corners[i];
        bbox->min = Vec3f(std::min(static_cast<float>(curVert[0]), bbox->min[0]),
                          std::min(static_cast<float>(curVert[1]), bbox->min[1]),
                          std::min(static_cast<float>(curVert[2]), bbox->min[2]));

        bbox->max = Vec3f(std::max(static_cast<float>(curVert[0]), bbox->max[0]),
                          std::max(static_cast<float>(curVert[1]), bbox->max[1]),
                          std::max(static_cast<float>(curVert[2]), bbox->max[2]));
    }
}

int Frustum::pointInFrustum(Vec3f p)
{
    int result = INSIDE;
    for(int i=0; i<PLANECOUNT; i++)
    {
        if(planes[i].isBehind(p))
        {
            return OUTSIDE;
        }
    }

    return result;
}

int Frustum::boxInFrustum(const AABB* bbox)
{
    int result = INSIDE;
    for(unsigned int i=0; i<PLANECOUNT; i++)
    {
        if(planes[i].isBehind(bbox->positiveVertex(planes[i].normal)))
        {
            return OUTSIDE;
        }
        else if(planes[i].isBehind(bbox->negativeVertex(planes[i].normal)))
        {
            return INTERSECT;
        }
    }

    return result;
}

void Frustum::saveCorners()
{
    for(unsigned int i=0; i<CORNERCOUNT; i++)
        drawCorners[i] = corners[i];

    for(unsigned int i=0; i<PLANECOUNT; i++)
        planes[i].saveValues();

    drawBbox = new AABB(Vec3f(std::numeric_limits<float>::max()), Vec3f(-std::numeric_limits<float>::max()));

    for(unsigned int i=0; i<CORNERCOUNT; i++)
    {
        Vec3f curVert = drawCorners[i];
        drawBbox->min = Vec3f(std::min(static_cast<float>(curVert[0]), drawBbox->min[0]),
                              std::min(static_cast<float>(curVert[1]), drawBbox->min[1]),
                              std::min(static_cast<float>(curVert[2]), drawBbox->min[2]));

        drawBbox->max = Vec3f(std::max(static_cast<float>(curVert[0]), drawBbox->max[0]),
                              std::max(static_cast<float>(curVert[1]), drawBbox->max[1]),
                              std::max(static_cast<float>(curVert[2]), drawBbox->max[2]));
    }
}

void Frustum::drawFrustum()
{
    Vec3f ntl = drawCorners[NTL];
    Vec3f ntr = drawCorners[NTR];
    Vec3f nbr = drawCorners[NBR];
    Vec3f nbl = drawCorners[NBL];
    Vec3f ftr = drawCorners[FTR];
    Vec3f ftl = drawCorners[FTL];
    Vec3f fbl = drawCorners[FBL];
    Vec3f fbr = drawCorners[FBR];

    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(5);

    glBegin(GL_LINE_LOOP);
        //near plane
        glVertex3f(ntl.v[0],ntl.v[1],ntl.v[2]);
        glVertex3f(ntr.v[0],ntr.v[1],ntr.v[2]);
        glVertex3f(nbr.v[0],nbr.v[1],nbr.v[2]);
        glVertex3f(nbl.v[0],nbl.v[1],nbl.v[2]);
    glEnd();

    glBegin(GL_LINE_LOOP);
        //far plane
        glVertex3f(ftr.v[0],ftr.v[1],ftr.v[2]);
        glVertex3f(ftl.v[0],ftl.v[1],ftl.v[2]);
        glVertex3f(fbl.v[0],fbl.v[1],fbl.v[2]);
        glVertex3f(fbr.v[0],fbr.v[1],fbr.v[2]);
    glEnd();

    glBegin(GL_LINE_LOOP);
        //bottom plane
        glVertex3f(nbl.v[0],nbl.v[1],nbl.v[2]);
        glVertex3f(nbr.v[0],nbr.v[1],nbr.v[2]);
        glVertex3f(fbr.v[0],fbr.v[1],fbr.v[2]);
        glVertex3f(fbl.v[0],fbl.v[1],fbl.v[2]);
    glEnd();

    glBegin(GL_LINE_LOOP);
        //top plane
        glVertex3f(ntr.v[0],ntr.v[1],ntr.v[2]);
        glVertex3f(ntl.v[0],ntl.v[1],ntl.v[2]);
        glVertex3f(ftl.v[0],ftl.v[1],ftl.v[2]);
        glVertex3f(ftr.v[0],ftr.v[1],ftr.v[2]);
    glEnd();

    glBegin(GL_LINE_LOOP);
        //left plane
        glVertex3f(ntl.v[0],ntl.v[1],ntl.v[2]);
        glVertex3f(nbl.v[0],nbl.v[1],nbl.v[2]);
        glVertex3f(fbl.v[0],fbl.v[1],fbl.v[2]);
        glVertex3f(ftl.v[0],ftl.v[1],ftl.v[2]);
    glEnd();

    glBegin(GL_LINE_LOOP);
        // right plane
        glVertex3f(nbr.v[0],nbr.v[1],nbr.v[2]);
        glVertex3f(ntr.v[0],ntr.v[1],ntr.v[2]);
        glVertex3f(ftr.v[0],ftr.v[1],ftr.v[2]);
        glVertex3f(fbr.v[0],fbr.v[1],fbr.v[2]);
    glEnd();

    Vec3f a,b;
    glBegin(GL_LINES);
        // near
        a = (ntr + ntl + nbr + nbl) * 0.25;
        b = a + planes[NEARP].sNormal;
        glVertex3f(a.v[0],a.v[1],a.v[2]);
        glVertex3f(b.v[0],b.v[1],b.v[2]);

        // far
        a = (ftr + ftl + fbr + fbl) * 0.25;
        b = a + planes[FARP].sNormal;
        glVertex3f(a.v[0],a.v[1],a.v[2]);
        glVertex3f(b.v[0],b.v[1],b.v[2]);

        // left
        a = (ftl + fbl + nbl + ntl) * 0.25;
        b = a + planes[LEFT].sNormal;
        glVertex3f(a.v[0],a.v[1],a.v[2]);
        glVertex3f(b.v[0],b.v[1],b.v[2]);

        // right
        a = (ftr + nbr + fbr + ntr) * 0.25;
        b = a + planes[RIGHT].sNormal;
        glVertex3f(a.v[0],a.v[1],a.v[2]);
        glVertex3f(b.v[0],b.v[1],b.v[2]);

        // top
        a = (ftr + ftl + ntr + ntl) * 0.25;
        b = a + planes[TOP].sNormal;
        glVertex3f(a.v[0],a.v[1],a.v[2]);
        glVertex3f(b.v[0],b.v[1],b.v[2]);

        // bottom
        a = (fbr + fbl + nbr + nbl) * 0.25;
        b = a + planes[BOTTOM].sNormal;
        glVertex3f(a.v[0],a.v[1],a.v[2]);
        glVertex3f(b.v[0],b.v[1],b.v[2]);
    glEnd();

    //drawBbox->render();
}
