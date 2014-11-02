/*
 * Stellarium
 * Copyright (C) 2002 Fabien Chereau (MilkyWay class)
 * Copyright (C) 2014 Georg Zotti (followed pattern for ZodiacalLight)
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "ZodiacalLight.hpp"
#include "StelFader.hpp"
#include "StelTexture.hpp"
#include "StelUtils.hpp"
#include "StelFileMgr.hpp"

#include "StelProjector.hpp"
#include "StelToneReproducer.hpp"
#include "StelApp.hpp"
#include "StelTextureMgr.hpp"
#include "StelCore.hpp"
#include "StelSkyDrawer.hpp"
#include "StelPainter.hpp"

#include <QDebug>
#include <QSettings>

// Class which manages the displaying of the Zodiacal Light
ZodiacalLight::ZodiacalLight()
	: color(1.f, 1.f, 1.f)
	, intensity(1.)
	, lastJD(-1.0E6)
	, vertexArray()
{
	setObjectName("ZodiacalLight");
	fader = new LinearFader();
}

ZodiacalLight::~ZodiacalLight()
{
	delete fader;
	fader = NULL;
	
	delete vertexArray;
	vertexArray = NULL;
}

void ZodiacalLight::init()
{
	QSettings* conf = StelApp::getInstance().getSettings();
	Q_ASSERT(conf);

	// The Paper describes brightness values over the complete sky, so also the texture covers the full sky. 

	// The value range from the paper is 57..1670, but show a hole around the sun where it is much brighter.
	// pixel arithmetics used to prepare 8bit gray map: (val-50)/6.36 to keep brightness from overflowing.
	// TODO (maybe later): split this object into a spherical component with hole 60x80degrees around the sun, and a brighter mesh 60x80.
	// By separating these meshes there should be no texture overlap problems. However, I have no good data for this hole.
	//tex = StelApp::getInstance().getTextureManager().createTexture(StelFileMgr::getInstallationDir()+"/textures/zodiacal_2004_pwr0p2_M2_mul18.png");
	tex = StelApp::getInstance().getTextureManager().createTexture(StelFileMgr::getInstallationDir()+"/textures/zodiacallight_2004.png");
//	tex = StelApp::getInstance().getTextureManager().createTexture(StelFileMgr::getInstallationDir()+"/textures/zodiacal_2004_m55by8.png");
	setFlagShow(conf->value("astro/flag_zodiacal_light", true).toBool());
	setIntensity(conf->value("astro/zodiacal_light_intensity",1.f).toFloat());

	vertexArray = new StelVertexArray(StelPainter::computeSphereNoLight(1.f,1.f,60,30,1, true)); // 6x6 degree quads
	vertexArray->colors.resize(vertexArray->vertex.length());
	vertexArray->colors.fill(Vec3f(1.0, 0.3, 0.9));

	eclipticalVertices=vertexArray->vertex;
	// This vector is used to keep original vertices, these will be modified in update().
}


void ZodiacalLight::update(double deltaTime)
{
	fader->update((int)(deltaTime*1000));

	if (!getFlagShow() || (getIntensity()<0.01) )
		return;

	StelCore* core=StelApp::getInstance().getCore();
	// Test if we are not on Earth. Texture would not fit, so don't draw then.
	if (core->getCurrentLocation().planetName != "Earth") return;

	double currentJD=core->getJDay();
	if (abs(currentJD - lastJD) > 0.25) // should be enough to update position every 6 hours.
	{
		// update vertices
		Vec3d obsPos=core->getObserverHeliocentricEclipticPos();
		double solarLongitude=atan2(obsPos[1], obsPos[0]) + 0.5*M_PI;
		Mat4d transMat=core->matVsop87ToJ2000 * Mat4d::zrotation(solarLongitude);
		for (int i=0; i<eclipticalVertices.size(); ++i)
		{
			Vec3d tmp=eclipticalVertices.at(i);
			vertexArray->vertex.replace(i, transMat * tmp);
		}
		lastJD=currentJD;
	}
}

void ZodiacalLight::setFlagShow(bool b){*fader = b;}
bool ZodiacalLight::getFlagShow() const {return *fader;}

void ZodiacalLight::draw(StelCore* core)
{
	if (!getFlagShow() || (getIntensity()<0.01) )
		return;

	// Test if we are not on Earth. Texture would not fit, so don't draw then.
	if (core->getCurrentLocation().planetName != "Earth") return;

	int bortle=core->getSkyDrawer()->getBortleScaleIndex();
	// Test for light pollution, return if too bad. (So fps stays high for really bad places.)
	if ( (core->getSkyDrawer()->getFlagHasAtmosphere()) && (bortle > 5) ) return;

	StelProjector::ModelViewTranformP transfo = core->getJ2000ModelViewTransform();

	const StelProjectorP prj = core->getProjection(transfo); // GZ: Maybe this can be simplified?
	StelToneReproducer* eye = core->getToneReproducer();

	Q_ASSERT(tex);	// A texture must be loaded before calling this

	// This RGB color corresponds to the night blue scotopic color = 0.25, 0.25 in xyY mode.
	// since Zodiacal Light is always seen white RGB value in the texture (1.0,1.0,1.0)
	// GZ: This taken from MilkyWay. Z.L. is redder, maybe adapt?
	//Vec3f c = Vec3f(0.34165f, 0.429666f, 0.63586f);
	Vec3f c = Vec3f(1.0f, 1.0f, 1.0f);

	// ZL is quite sensitive to light pollution. I scale to make it less visible.
	float lum = core->getSkyDrawer()->surfacebrightnessToLuminance(5.0f*+0.5*bortle);

	// Get the luminance scaled between 0 and 1
	float aLum =eye->adaptLuminanceScaled(lum*fader->getInterstate());

	// Bound a maximum luminance (minimum?)
	aLum = qMin(0.38f, aLum*2.f);
	//aLum = qMin(0.1f, aLum*2.f); // value 0.1 pure guessing!

	// intensity of 1.0 is "proper", but allow boost for dim screens
	c*=aLum*intensity;

	if (c[0]<0) c[0]=0;
	if (c[1]<0) c[1]=0;
	if (c[2]<0) c[2]=0;

	const bool withExtinction=(core->getSkyDrawer()->getFlagHasAtmosphere() && core->getSkyDrawer()->getExtinction().getExtinctionCoefficient()>=0.01f);

	if (withExtinction)
	{
		// We must process the vertices to find geometric altitudes in order to compute vertex colors.
		Extinction extinction=core->getSkyDrawer()->getExtinction();
		vertexArray->colors.clear();

		for (int i=0; i<vertexArray->vertex.size(); ++i)
		{
			Vec3d vertAltAz=core->j2000ToAltAz(vertexArray->vertex.at(i), StelCore::RefractionOn);
			Q_ASSERT(vertAltAz.lengthSquared()-1.0 < 0.001f);

			float oneMag=0.0f;
			extinction.forward(vertAltAz, &oneMag);
			float extinctionFactor=std::pow(0.4f , oneMag)/bortle; // drop of one magnitude: factor 2.5 or 40%, and further reduced by light pollution
			Vec3f thisColor=Vec3f(c[0]*extinctionFactor, c[1]*extinctionFactor, c[2]*extinctionFactor);
			vertexArray->colors.append(thisColor);
		}
	}
	else
		vertexArray->colors.fill(Vec3f(c[0], c[1], c[2]));

	StelPainter sPainter(prj);
	glEnable(GL_CULL_FACE);
	sPainter.enableTexture2d(true);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	tex->bind();
	sPainter.drawStelVertexArray(*vertexArray);
	glDisable(GL_CULL_FACE);
}
