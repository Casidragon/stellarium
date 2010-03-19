/*
 * Stellarium
 * Copyright (C) 2010 Fabien Chereau
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

#include "TrailGroup.hpp"
#include "StelNavigator.hpp"
#include "StelApp.hpp"
#include "StelPainter.hpp"
#include "StelObject.hpp"
#include "Planet.hpp"
#include <QtOpenGL>

TrailGroup::TrailGroup(float te) : timeExtent(te), opacity(1.f)
{
	j2000ToTrailNative=Mat4d::identity();
	j2000ToTrailNativeInverted=Mat4d::identity();
}

static QVector<Vec3d> vertexArray;
static QVector<Vec4f> colorArray;
void TrailGroup::draw(StelCore* core, StelPainter* sPainter)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	float currentTime = core->getNavigator()->getJDay();
	sPainter->setProjector(core->getProjection(core->getNavigator()->getJ2000ModelViewMat()*j2000ToTrailNativeInverted));
	for (QMap<StelObjectP, Trail>::Iterator iter = allTrails.begin();iter!=allTrails.end();++iter)
	{
		Planet* hpl = dynamic_cast<Planet*>(iter.key().data());
		if (hpl!=NULL)
		{
			// Avoid drawing the trails if the object is the home planet
			QString homePlanetName = hpl==NULL ? "" : hpl->getEnglishName();
			if (homePlanetName==StelApp::getInstance().getCore()->getNavigator()->getCurrentLocation().planetName)
				continue;
		}
		const QList<Vec3d>& posHistory = iter.value().posHistory;
		vertexArray.resize(posHistory.size());
		colorArray.resize(posHistory.size());
		const Vec3f& color = iter.key()->getInfoColor();
		for (int i=0;i<posHistory.size();++i)
		{
			float colorRatio = 1.f-(currentTime-times.at(i))/timeExtent;
			colorArray[i].set(color[0], color[1], color[2], colorRatio*opacity);
			vertexArray[i]=posHistory.at(i);
		}
		sPainter->setVertexPointer(3, GL_DOUBLE, vertexArray.constData());
		sPainter->setColorPointer(4, GL_FLOAT, colorArray.constData());
		sPainter->enableClientStates(true, false, true);
		sPainter->drawFromArray(StelPainter::LineStrip, vertexArray.size(), 0, true);
		sPainter->enableClientStates(false);
	}
}

// Add 1 point to all the curves at current time and suppress too old points
void TrailGroup::update()
{
	StelNavigator* nav = StelApp::getInstance().getCore()->getNavigator();
	times.append(nav->getJDay());
	for (QMap<StelObjectP, Trail>::Iterator iter = allTrails.begin();iter!=allTrails.end();++iter)
	{
		iter.value().posHistory.append(j2000ToTrailNative*iter.key()->getJ2000EquatorialPos(nav));
	}
	if (nav->getJDay()-times.at(0)>timeExtent)
	{
		times.pop_front();
		for (QMap<StelObjectP, Trail>::Iterator iter = allTrails.begin();iter!=allTrails.end();++iter)
		{
			iter.value().posHistory.pop_front();
		}
	}
}

// Set the matrix to use to post process J2000 positions before storing in the trail
void TrailGroup::setJ2000ToTrailNative(const Mat4d& m)
{
	j2000ToTrailNative=m;
	j2000ToTrailNativeInverted=m.inverse();
}

void TrailGroup::addObject(const StelObjectP& obj)
{
	allTrails.insert(obj,TrailGroup::Trail(obj));
}

void TrailGroup::removeObject(const StelObjectP& obj)
{
	allTrails.remove(obj);
}

void TrailGroup::reset()
{
	times.clear();
	for (QMap<StelObjectP, Trail>::Iterator iter = allTrails.begin();iter!=allTrails.end();++iter)
	{
		iter.value().posHistory.clear();
	}
}
