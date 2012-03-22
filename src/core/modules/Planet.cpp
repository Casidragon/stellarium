/*
 * Stellarium
 * Copyright (C) 2002 Fabien Chereau
 * Copyright (C) 2011 Eleni Maria Stea (Planet rendering using normal mapping
 * and clouds)
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

#include <iomanip>
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QTextStream>
#include <QString>
#include <QDebug>
#include <QObject>
#include <QVariant>
#include <QVarLengthArray>
#include <QDateTime>

#include <GLee.h>
#include "StelShader.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelTexture.hpp"
#include "StelSkyDrawer.hpp"
#include "SolarSystem.hpp"
#include "Planet.hpp"

#include "StelProjector.hpp"
#include "sideral_time.h"
#include "StelTextureMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StarMgr.hpp"
#include "StelMovementMgr.hpp"
#include "StelPainter.hpp"
#include "StelTranslator.hpp"


static unsigned int permMap;
static unsigned int createPermTexture();

Vec3f Planet::labelColor = Vec3f(0.4,0.4,0.8);
Vec3f Planet::orbitColor = Vec3f(1,0.6,1);
StelTextureSP Planet::hintCircleTex;
StelTextureSP Planet::texEarthShadow;
Planet::Planet(const QString& englishName,
			   int flagLighting,
			   double radius,
			   double oblateness,
			   Vec3f color,
			   float albedo,
			   const QString& atexMapName,
			   posFuncType coordFunc,
			   void* auserDataPtr,
			   OsculatingFunctType *osculatingFunc,
			   bool acloseOrbit,
			   bool hidden,
			   bool hasAtmosphere)
	: englishName(englishName),
	  flagLighting(flagLighting),
	  radius(radius), oneMinusOblateness(1.0-oblateness),
	  color(color), albedo(albedo), axisRotation(0.), rings(NULL),
	  sphereScale(1.f),
	  lastJD(J2000),
	  coordFunc(coordFunc),
	  userDataPtr(auserDataPtr),
	  osculatingFunc(osculatingFunc),
	  parent(NULL),
	  hidden(hidden),
	  atmosphere(hasAtmosphere)
{
	texMapName = atexMapName;
	lastOrbitJD =0;
	deltaJD = StelCore::JD_SECOND;
	orbitCached = 0;
	closeOrbit = acloseOrbit;

	eclipticPos=Vec3d(0.,0.,0.);
	rotLocalToParent = Mat4d::identity();
	texMap = StelApp::getInstance().getTextureManager().createTextureThread("textures/"+texMapName, StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));

	nameI18 = englishName;
	if (englishName!="Pluto")
	{
		deltaJD = 0.001*StelCore::JD_SECOND;
	}
	flagLabels = true;
}

Planet::Planet(const QString& englishName,
			   int flagLighting,
			   double radius,
			   double oblateness,
			   Vec3f color,
			   float albedo,
			   const QString& atexMapName,
			   const QString& anormalMapName,
			   posFuncType coordFunc,
			   void* auserDataPtr,
			   OsculatingFunctType *osculatingFunc,
			   bool acloseOrbit,
			   bool hidden,
			   bool hasAtmosphere)
	: englishName(englishName),
	  flagLighting(flagLighting),
	  radius(radius), oneMinusOblateness(1.0-oblateness),
	  color(color), albedo(albedo), axisRotation(0.), rings(NULL),
	  sphereScale(1.f),
	  lastJD(J2000),
	  coordFunc(coordFunc),
	  userDataPtr(auserDataPtr),
	  osculatingFunc(osculatingFunc),
	  parent(NULL),
	  hidden(hidden),
	  atmosphere(hasAtmosphere)
{
	texMapName = atexMapName;
	normalMapName = anormalMapName;
	lastOrbitJD =0;
	deltaJD = StelCore::JD_SECOND;
	orbitCached = 0;
	closeOrbit = acloseOrbit;

	eclipticPos=Vec3d(0.,0.,0.);
	rotLocalToParent = Mat4d::identity();
	texMap = StelApp::getInstance().getTextureManager().createTextureThread("textures/"+texMapName, StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));
	normalMap = StelApp::getInstance().getTextureManager().createTexture("textures/"+normalMapName, StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));

	nameI18 = englishName;
	if (englishName!="Pluto")
	{
		deltaJD = 0.001*StelCore::JD_SECOND;
	}
	flagLabels = true;
}

Planet::Planet(const QString& englishName,
			   int flagLighting,
			   double radius,
			   double oblateness,
			   Vec3f color,
			   Vec3f cloudColor,
			   float cloudDensity,
			   float cloudScale,
			   float cloudSharpness,
			   Vec3f cloudVel,
			   float albedo,
			   const QString& atexMapName,
			   const QString& anormalMapName,
			   posFuncType coordFunc,
			   void* auserDataPtr,
			   OsculatingFunctType *osculatingFunc,
			   bool acloseOrbit,
			   bool hidden,
			   bool hasAtmosphere)
	: englishName(englishName),
	  flagLighting(flagLighting),
	  radius(radius), oneMinusOblateness(1.0-oblateness),
	  color(color), cloudColor(cloudColor), cloudDensity(cloudDensity), cloudScale(cloudScale), cloudSharpness(cloudSharpness), cloudVel(cloudVel), albedo(albedo), axisRotation(0.), rings(NULL),
	  sphereScale(1.f),
	  lastJD(J2000),
	  coordFunc(coordFunc),
	  userDataPtr(auserDataPtr),
	  osculatingFunc(osculatingFunc),
	  parent(NULL),
	  hidden(hidden),
	  atmosphere(hasAtmosphere)
{
	texMapName = atexMapName;
	normalMapName = anormalMapName;

	lastOrbitJD =0;
	deltaJD = StelCore::JD_SECOND;
	orbitCached = 0;
	closeOrbit = acloseOrbit;

	eclipticPos=Vec3d(0.,0.,0.);
	rotLocalToParent = Mat4d::identity();
	texMap = StelApp::getInstance().getTextureManager().createTextureThread("textures/"+texMapName, StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));
	normalMap = StelApp::getInstance().getTextureManager().createTexture("textures/"+normalMapName, StelTexture::StelTextureParams(true, GL_LINEAR, GL_REPEAT));

	nameI18 = englishName;
	if (englishName!="Pluto")
	{
		deltaJD = 0.001*StelCore::JD_SECOND;
	}
	flagLabels = true;
}

Planet::~Planet()
{
	if (rings)
		delete rings;
}

void Planet::translateName(StelTranslator& trans)
{
	nameI18 = trans.qtranslate(englishName);
}

// Return the information string "ready to print" :)
QString Planet::getInfoString(const StelCore* core, const InfoStringGroup& flags) const
{
	QString str;
	QTextStream oss(&str);

	if (flags&Name)
	{
		oss << "<h2>" << q_(englishName);  // UI translation can differ from sky translation
		oss.setRealNumberNotation(QTextStream::FixedNotation);
		oss.setRealNumberPrecision(1);
		if (sphereScale != 1.f)
			oss << QString::fromUtf8(" (\xC3\x97") << sphereScale << ")";
		oss << "</h2>";
	}

	if (flags&Magnitude)
		oss << q_("Magnitude: <b>%1</b>").arg(getVMagnitude(core), 0, 'f', 2) << "<br>";

	if (flags&AbsoluteMagnitude)
		oss << q_("Absolute Magnitude: %1").arg(getVMagnitude(core)-5.*(std::log10(getJ2000EquatorialPos(core).length()*AU/PARSEC)-1.), 0, 'f', 2) << "<br>";

	oss << getPositionInfoString(core, flags);

	if ((flags&Extra2) && (core->getCurrentLocation().planetName=="Earth"))
	{
		//static SolarSystem *ssystem=GETSTELMODULE(SolarSystem);
		//double ecl= -(ssystem->getEarth()->getRotObliquity()); // BUG DETECTED! Earth's obliquity is apparently reported constant.
		double ra_equ, dec_equ, lambda, beta;
		double ecl= get_mean_ecliptical_obliquity(core->getJDay()) *M_PI/180.0;
		StelUtils::rectToSphe(&ra_equ,&dec_equ,getEquinoxEquatorialPos(core));
		StelUtils::ctRadec2Ecl(ra_equ, dec_equ, ecl, &lambda, &beta);
		if (lambda<0) lambda+=2.0*M_PI;
		oss << q_("Ecliptic Geocentric (of date): %1/%2").arg(StelUtils::radToDmsStr(lambda, true), StelUtils::radToDmsStr(beta, true)) << "<br>";
		oss << q_("Obliquity (of date): %1").arg(StelUtils::radToDmsStr(ecl, true)) << "<br>";
	}

	if (flags&Distance)
	{
		// xgettext:no-c-format
		oss << q_("Distance: %1AU").arg(getJ2000EquatorialPos(core).length(), 0, 'f', 8) << "<br>";
	}

	if (flags&Size)
		oss << q_("Apparent diameter: %1").arg(StelUtils::radToDmsStr(2.*getAngularSize(core)*M_PI/180., true));

	postProcessInfoString(str, flags);

	return str;
}

//! Get sky label (sky translation)
QString Planet::getSkyLabel(const StelCore*) const
{
	QString str;
	QTextStream oss(&str);
	oss.setRealNumberPrecision(2);
	oss << nameI18;

	if (sphereScale != 1.f)
	{
		oss << QString::fromUtf8(" (\xC3\x97") << sphereScale << ")";
	}
	return str;
}

float Planet::getSelectPriority(const StelCore* core) const
{
	if( ((SolarSystem*)StelApp::getInstance().getModuleMgr().getModule("SolarSystem"))->getFlagHints() )
	{
	// easy to select, especially pluto
		return getVMagnitude(core)-15.f;
	}
	else
	{
		return getVMagnitude(core) - 8.f;
	}
}

Vec3f Planet::getInfoColor(void) const
{
	return StelApp::getInstance().getVisionModeNight() ? Vec3f(0.8, 0.2, 0.4) : ((SolarSystem*)StelApp::getInstance().getModuleMgr().getModule("SolarSystem"))->getLabelsColor();
}


double Planet::getCloseViewFov(const StelCore* core) const
{
	return std::atan(radius*sphereScale*2.f/getEquinoxEquatorialPos(core).length())*180./M_PI * 4;
}

double Planet::getSatellitesFov(const StelCore* core) const
{
	// TODO: calculate from satellite orbits rather than hard code
	if (englishName=="Jupiter") return std::atan(0.005f/getEquinoxEquatorialPos(core).length())*180./M_PI * 4;
	if (englishName=="Saturn") return std::atan(0.005f/getEquinoxEquatorialPos(core).length())*180./M_PI * 4;
	if (englishName=="Mars") return std::atan(0.0001f/getEquinoxEquatorialPos(core).length())*180./M_PI * 4;
	if (englishName=="Uranus") return std::atan(0.002f/getEquinoxEquatorialPos(core).length())*180./M_PI * 4;
	return -1.;
}

double Planet::getParentSatellitesFov(const StelCore* core) const
{
	if (parent && parent->parent) return parent->getSatellitesFov(core);
	return -1.0;
}

// Set the orbital elements
void Planet::setRotationElements(float _period, float _offset, double _epoch, float _obliquity, float _ascendingNode, float _precessionRate, double _siderealPeriod )
{
	re.period = _period;
	re.offset = _offset;
	re.epoch = _epoch;
	re.obliquity = _obliquity;
	re.ascendingNode = _ascendingNode;
	re.precessionRate = _precessionRate;
	re.siderealPeriod = _siderealPeriod;  // used for drawing orbit lines

	deltaOrbitJD = re.siderealPeriod/ORBIT_SEGMENTS;
}

Vec3d Planet::getJ2000EquatorialPos(const StelCore *core) const
{
	return StelCore::matVsop87ToJ2000.multiplyWithoutTranslation(getHeliocentricEclipticPos() - core->getObserverHeliocentricEclipticPos());
}

// Compute the position in the parent Planet coordinate system
// Actually call the provided function to compute the ecliptical position
void Planet::computePositionWithoutOrbits(const double date)
{
	if (fabs(lastJD-date)>deltaJD)
	{
		coordFunc(date, eclipticPos, userDataPtr);
		lastJD = date;
	}
}

void Planet::computePosition(const double date)
{

	if (orbitFader.getInterstate()>0.000001 && deltaOrbitJD > 0 && (fabs(lastOrbitJD-date)>deltaOrbitJD || !orbitCached))
	{

		// calculate orbit first (for line drawing)
		double date_increment = re.siderealPeriod/ORBIT_SEGMENTS;
		double calc_date;
		// int delta_points = (int)(0.5 + (date - lastOrbitJD)/date_increment);
		int delta_points;

		if( date > lastOrbitJD )
		{
			delta_points = (int)(0.5 + (date - lastOrbitJD)/date_increment);
		}
		else
		{
			delta_points = (int)(-0.5 + (date - lastOrbitJD)/date_increment);
		}
		double new_date = lastOrbitJD + delta_points*date_increment;

		// qDebug( "Updating orbit coordinates for %s (delta %f) (%d points)\n", name.c_str(), deltaOrbitJD, delta_points);

		if( delta_points > 0 && delta_points < ORBIT_SEGMENTS && orbitCached)
		{

			for( int d=0; d<ORBIT_SEGMENTS; d++ )
			{
				if(d + delta_points >= ORBIT_SEGMENTS )
				{
					// calculate new points
					calc_date = new_date + (d-ORBIT_SEGMENTS/2)*date_increment;

					// date increments between points will not be completely constant though
					computeTransMatrix(calc_date);
					if (osculatingFunc)
					{
						(*osculatingFunc)(date,calc_date,eclipticPos);
					}
					else
					{
						coordFunc(calc_date, eclipticPos, userDataPtr);
					}
					orbit[d] = getHeliocentricEclipticPos();
				}
				else
				{
					orbit[d] = orbit[d+delta_points];
				}
			}

			lastOrbitJD = new_date;
		}
		else if( delta_points < 0 && abs(delta_points) < ORBIT_SEGMENTS  && orbitCached)
		{

			for( int d=ORBIT_SEGMENTS-1; d>=0; d-- )
			{
				if(d + delta_points < 0 )
				{
					// calculate new points
					calc_date = new_date + (d-ORBIT_SEGMENTS/2)*date_increment;

					computeTransMatrix(calc_date);
					if (osculatingFunc) {
						(*osculatingFunc)(date,calc_date,eclipticPos);
					}
					else
					{
						coordFunc(calc_date, eclipticPos, userDataPtr);
					}
					orbit[d] = getHeliocentricEclipticPos();
				}
				else
				{
					orbit[d] = orbit[d+delta_points];
				}
			}

			lastOrbitJD = new_date;

		}
		else if( delta_points || !orbitCached)
		{

			// update all points (less efficient)
			for( int d=0; d<ORBIT_SEGMENTS; d++ )
			{
				calc_date = date + (d-ORBIT_SEGMENTS/2)*date_increment;
				computeTransMatrix(calc_date);
				if (osculatingFunc)
				{
					(*osculatingFunc)(date,calc_date,eclipticPos);
				}
				else
				{
					coordFunc(calc_date, eclipticPos, userDataPtr);
				}
				orbit[d] = getHeliocentricEclipticPos();
			}

			lastOrbitJD = date;
			if (!osculatingFunc) orbitCached = 1;
		}


		// calculate actual Planet position
		coordFunc(date, eclipticPos, userDataPtr);

		lastJD = date;

	}
	else if (fabs(lastJD-date)>deltaJD)
	{
		// calculate actual Planet position
		coordFunc(date, eclipticPos, userDataPtr);
		lastJD = date;
	}

}

// Compute the transformation matrix from the local Planet coordinate to the parent Planet coordinate
void Planet::computeTransMatrix(double jd)
{
	axisRotation = getSiderealTime(jd);

	// Special case - heliocentric coordinates are on ecliptic,
	// not solar equator...
	if (parent)
	{
		rotLocalToParent = Mat4d::zrotation(re.ascendingNode - re.precessionRate*(jd-re.epoch)) * Mat4d::xrotation(re.obliquity);
	}
}

Mat4d Planet::getRotEquatorialToVsop87(void) const
{
	Mat4d rval = rotLocalToParent;
	if (parent)
	{
		for (PlanetP p=parent;p->parent;p=p->parent)
			rval = p->rotLocalToParent * rval;
	}
	return rval;
}

void Planet::setRotEquatorialToVsop87(const Mat4d &m)
{
	Mat4d a = Mat4d::identity();
	if (parent)
	{
		for (PlanetP p=parent;p->parent;p=p->parent)
			a = p->rotLocalToParent * a;
	}
	rotLocalToParent = a.transpose() * m;
}


// Compute the z rotation to use from equatorial to geographic coordinates
double Planet::getSiderealTime(double jd) const
{
	if (englishName=="Earth")
	{
		return get_apparent_sidereal_time(jd);
	}

	double t = jd - re.epoch;
	double rotations = t / (double) re.period;
	double wholeRotations = floor(rotations);
	double remainder = rotations - wholeRotations;

	return remainder * 360. + re.offset;
}

// Get the Planet position in the parent Planet ecliptic coordinate in AU
Vec3d Planet::getEclipticPos() const
{
	return eclipticPos;
}

// Return the heliocentric ecliptical position (Vsop87)
Vec3d Planet::getHeliocentricEclipticPos() const
{
	Vec3d pos = eclipticPos;
	PlanetP pp = parent;
	if (pp)
	{
		while (pp->parent)
		{
			pos += pp->eclipticPos;
			pp = pp->parent;
		}
	}
	return pos;
}

void Planet::setHeliocentricEclipticPos(const Vec3d &pos)
{
	eclipticPos = pos;
	PlanetP p = parent;
	if (p)
	{
		while (p->parent)
		{
			eclipticPos -= p->eclipticPos;
			p = p->parent;
		}
	}
}

// Compute the distance to the given position in heliocentric coordinate (in AU)
double Planet::computeDistance(const Vec3d& obsHelioPos)
{
	distance = (obsHelioPos-getHeliocentricEclipticPos()).length();
	return distance;
}

// Get the phase angle for an observer at pos obsPos in the heliocentric coordinate (dist in AU)
double Planet::getPhase(const Vec3d& obsPos) const
{
	const double observerRq = obsPos.lengthSquared();
	const Vec3d& planetHelioPos = getHeliocentricEclipticPos();
	const double planetRq = planetHelioPos.lengthSquared();
	const double observerPlanetRq = (obsPos - planetHelioPos).lengthSquared();
	return std::acos(observerPlanetRq + planetRq - observerRq)/(2.0*sqrt(observerPlanetRq*planetRq));
}

// Computation of the visual magnitude (V band) of the planet.
float Planet::getVMagnitude(const StelCore* core) const
{
	if (parent == 0)
	{
		// sun, compute the apparent magnitude for the absolute mag (4.83) and observer's distance
		const double distParsec = std::sqrt(core->getObserverHeliocentricEclipticPos().lengthSquared())*AU/PARSEC;
		return 4.83 + 5.*(std::log10(distParsec)-1.);
	}

	// Compute the angular phase
	const Vec3d& observerHelioPos = core->getObserverHeliocentricEclipticPos();
	const double observerRq = observerHelioPos.lengthSquared();
	const Vec3d& planetHelioPos = getHeliocentricEclipticPos();
	const double planetRq = planetHelioPos.lengthSquared();
	const double observerPlanetRq = (observerHelioPos - planetHelioPos).lengthSquared();
	const double cos_chi = (observerPlanetRq + planetRq - observerRq)/(2.0*sqrt(observerPlanetRq*planetRq));
	double phase = std::acos(cos_chi);

	double shadowFactor = 1.;
	// Check if the satellite is inside the inner shadow of the parent planet:
	if (parent->parent != 0)
	{
		const Vec3d& parentHeliopos = parent->getHeliocentricEclipticPos();
		const double parent_Rq = parentHeliopos.lengthSquared();
		const double pos_times_parent_pos = planetHelioPos * parentHeliopos;
		if (pos_times_parent_pos > parent_Rq)
		{
			// The satellite is farther away from the sun than the parent planet.
			const double sun_radius = parent->parent->radius;
			const double sun_minus_parent_radius = sun_radius - parent->radius;
			const double quot = pos_times_parent_pos/parent_Rq;

			// Compute d = distance from satellite center to border of inner shadow.
			// d>0 means inside the shadow cone.
			double d = sun_radius - sun_minus_parent_radius*quot - std::sqrt((1.-sun_minus_parent_radius/sqrt(parent_Rq)) * (planetRq-pos_times_parent_pos*quot));
			if (d>=radius)
			{
				// The satellite is totally inside the inner shadow.
				shadowFactor = 1e-9;
			}
			else if (d>-radius)
			{
				// The satellite is partly inside the inner shadow,
				// compute a fantasy value for the magnitude:
				d /= radius;
				shadowFactor = (0.5 - (std::asin(d)+d*std::sqrt(1.0-d*d))/M_PI);
			}
		}
	}

	// Use empirical formulae for main planets when seen from earth
	// Algorithm provided by Pere Planesas (Observatorio Astronomico Nacional)
	if (core->getCurrentLocation().planetName=="Earth")
	{
		phase*=180./M_PI;
		const double d = 5. * log10(sqrt(observerPlanetRq*planetRq));
		double f1 = phase/100.;

		if (englishName=="Mercury")
		{
			if ( phase > 150. ) f1 = 1.5;
			return -0.36 + d + 3.8*f1 - 2.73*f1*f1 + 2*f1*f1*f1;
		}
		if (englishName=="Venus")
			return -4.29 + d + 0.09*f1 + 2.39*f1*f1 - 0.65*f1*f1*f1;
		if (englishName=="Mars")
			return -1.52 + d + 0.016*phase;
		if (englishName=="Jupiter")
			return -9.25 + d + 0.005*phase;
		if (englishName=="Saturn")
		{
			// TODO re-add rings computation
			// double rings = -2.6*sinx + 1.25*sinx*sinx;
			return -8.88 + d + 0.044*phase;// + rings;
		}

		if (englishName=="Uranus")
			return -7.19 + d + 0.0028*phase;
		if (englishName=="Neptune")
			return -6.87 + d;
		if (englishName=="Pluto")
			return -1.01 + d + 0.041*phase;

		phase/=180./M_PI;
	}

	// This formula seems to give wrong results
	const double p = (1.0 - phase/M_PI) * cos_chi + std::sqrt(1.0 - cos_chi*cos_chi) / M_PI;
	double F = 2.0 * albedo * radius * radius * p / (3.0*observerPlanetRq*planetRq) * shadowFactor;
	return -26.73 - 2.5 * std::log10(F);
}

double Planet::getAngularSize(const StelCore* core) const
{
	double rad = radius;
	if (rings)
		rad = rings->getSize();
	return std::atan2(rad*sphereScale,getJ2000EquatorialPos(core).length()) * 180./M_PI;
}


double Planet::getSpheroidAngularSize(const StelCore* core) const
{
	return std::atan2(radius*sphereScale,getJ2000EquatorialPos(core).length()) * 180./M_PI;
}

// Draw the Planet and all the related infos : name, circle etc..
void Planet::draw(StelCore* core, float maxMagLabels, const QFont& planetNameFont)
{
	if (hidden)
		return;

	Mat4d mat = Mat4d::translation(eclipticPos) * rotLocalToParent;
	PlanetP p = parent;
	while (p && p->parent)
	{
		mat = Mat4d::translation(p->eclipticPos) * mat * p->rotLocalToParent;
		p = p->parent;
	}

	// This removed totally the Planet shaking bug!!!
	StelProjector::ModelViewTranformP transfo = core->getHeliocentricEclipticModelViewTransform();
	transfo->combine(mat);
	if (getEnglishName() == core->getCurrentLocation().planetName)
	{
		// Draw the rings if we are located on a planet with rings, but not the planet itself.
		if (rings)
		{
			StelPainter sPainter(core->getProjection(transfo));
			rings->draw(&sPainter,transfo,1000.0);
		}
		return;
	}

	// Compute the 2D position and check if in the screen
	const StelProjectorP prj = core->getProjection(transfo);
	float screenSz = getAngularSize(core)*M_PI/180.*prj->getPixelPerRadAtCenter();
	float viewport_left = prj->getViewportPosX();
	float viewport_bottom = prj->getViewportPosY();
	if (prj->project(Vec3d(0), screenPos)
	    && screenPos[1]>viewport_bottom - screenSz && screenPos[1] < viewport_bottom + prj->getViewportHeight()+screenSz
	    && screenPos[0]>viewport_left - screenSz && screenPos[0] < viewport_left + prj->getViewportWidth() + screenSz)
	{
		// Draw the name, and the circle if it's not too close from the body it's turning around
		// this prevents name overlaping (ie for jupiter satellites)
		float ang_dist = 300.f*atan(getEclipticPos().length()/getEquinoxEquatorialPos(core).length())/core->getMovementMgr()->getCurrentFov();
		if (ang_dist==0.f)
			ang_dist = 1.f; // if ang_dist == 0, the Planet is sun..

		// by putting here, only draw orbit if Planet is visible for clarity
		drawOrbit(core);  // TODO - fade in here also...

		if (flagLabels && ang_dist>0.25 && maxMagLabels>getVMagnitude(core))
		{
			labelsFader=true;
		}
		else
		{
			labelsFader=false;
		}
		drawHints(core, planetNameFont);

		draw3dModel(core,transfo,screenSz);
	}
	return;
}

void Planet::draw3dModel(StelCore* core, StelProjector::ModelViewTranformP transfo, float screenSz)
{
	// This is the main method drawing a planet 3d model
	// Some work has to be done on this method to make the rendering nicer

	if (screenSz>1.)
	{
		StelProjector::ModelViewTranformP transfo2 = transfo->clone();
		transfo2->combine(Mat4d::zrotation(M_PI/180*(axisRotation + 90.)));
		StelPainter* sPainter = new StelPainter(core->getProjection(transfo2));

		if (flagLighting)
		{
			sPainter->getLight().enable();

			// Set the main source of light to be the sun
			Vec3d sunPos(0);
			core->getHeliocentricEclipticModelViewTransform()->forward(sunPos);
			sPainter->getLight().setPosition(Vec4f(sunPos[0],sunPos[1],sunPos[2],1.f));

			// Set the light parameters taking sun as the light source
			static const Vec4f diffuse = Vec4f(2.f,2.f,2.f,1.f);
			static const Vec4f zero = Vec4f(0.f,0.f,0.f,0.f);
			static const Vec4f ambient = Vec4f(0.02f,0.02f,0.02f,0.02f);
			sPainter->getLight().setAmbient(ambient);
			sPainter->getLight().setDiffuse(diffuse);
			sPainter->getLight().setSpecular(zero);

			sPainter->getMaterial().setAmbient(ambient);
			sPainter->getMaterial().setEmission(zero);
			sPainter->getMaterial().setShininess(0.f);
			sPainter->getMaterial().setSpecular(zero);
		}
		else
		{
			sPainter->getLight().disable();
			sPainter->setColor(1.f,1.f,1.f);
		}

		if (rings)
		{
			const double dist = getEquinoxEquatorialPos(core).length();
			double z_near = 0.9*(dist - rings->getSize());
			double z_far  = 1.1*(dist + rings->getSize());
			if (z_near < 0.0) z_near = 0.0;
			double n,f;
			core->getClippingPlanes(&n,&f); // Save clipping planes
			core->setClippingPlanes(z_near,z_far);
			glDepthMask(GL_TRUE);
			glClear(GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			drawSphere(sPainter, screenSz);
			glDepthMask(GL_FALSE);
			sPainter->getLight().disable();
			rings->draw(sPainter,transfo,screenSz);
			sPainter->getLight().enable();
			glDisable(GL_DEPTH_TEST);
			core->setClippingPlanes(n,f);  // Restore old clipping planes
		}
		else
		{
			SolarSystem* ssm = GETSTELMODULE(SolarSystem);
			if (this==ssm->getMoon() && core->getCurrentLocation().planetName=="Earth" && ssm->nearLunarEclipse())
			{
				// Draw earth shadow over moon using stencil buffer if appropriate
				// This effect curently only looks right from earth viewpoint
				// TODO: moon magnitude label during eclipse isn't accurate...
				glClearStencil(0x0);
				glClear(GL_STENCIL_BUFFER_BIT);
				glStencilFunc(GL_ALWAYS, 0x1, 0x1);
				glStencilOp(GL_ZERO, GL_REPLACE, GL_REPLACE);
				glEnable(GL_STENCIL_TEST);
				drawSphere(sPainter, screenSz);
				glDisable(GL_STENCIL_TEST);

				sPainter->getLight().disable();
				drawEarthShadow(core, sPainter);
			}
			else
			{
				// Normal planet
				drawNMapSphere(sPainter, screenSz);
			}
		}
		if (sPainter)
			delete sPainter;
		sPainter=NULL;
	}

	// Draw the halo

	// Prepare openGL lighting parameters according to luminance
	float surfArcMin2 = getSpheroidAngularSize(core)*60;
	surfArcMin2 = surfArcMin2*surfArcMin2*M_PI; // the total illuminated area in arcmin^2

	StelPainter sPainter(core->getProjection(StelCore::FrameJ2000));
	Vec3d tmp = getJ2000EquatorialPos(core);
	core->getSkyDrawer()->postDrawSky3dModel(&sPainter, Vec3f(tmp[0], tmp[1], tmp[2]), surfArcMin2, getVMagnitude(core), color);
}


void Planet::drawSphere(StelPainter* painter, float screenSz)
{
	if (texMap)
	{
		// For lazy loading, return if texture not yet loaded
		if (!texMap->bind())
		{
			return;
		}
	}
	painter->enableTexture2d(true);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);

	// Draw the spheroid itself
	// Adapt the number of facets according with the size of the sphere for optimization
	int nb_facet = (int)(screenSz * 40/50);	// 40 facets for 1024 pixels diameter on screen
	if (nb_facet<10) nb_facet = 10;
	if (nb_facet>40) nb_facet = 40;
	painter->setShadeModel(StelPainter::ShadeModelSmooth);
	// Rotate and add an extra quarter rotation so that the planet texture map
	// fits to the observers position. No idea why this is necessary,
	// perhaps some openGl strangeness, or confusing sin/cos.

	painter->sSphere(radius*sphereScale, oneMinusOblateness, nb_facet, nb_facet);
	painter->setShadeModel(StelPainter::ShadeModelFlat);
	glDisable(GL_CULL_FACE);
}


static int perm[256] = {
	151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static int grad3[16][3] = {
	{0,1,1},{0,1,-1},{0,-1,1},{0,-1,-1},
	{1,0,1},{1,0,-1},{-1,0,1},{-1,0,-1},
	{1,1,0},{1,-1,0},{-1,1,0},{-1,-1,0},
	{1,0,-1},{-1,0,-1},{0,-1,1},{0,1,1}
};

// Procedural texture
static unsigned int createPermTexture()
{
	unsigned int tex;

	unsigned char *pixels = new unsigned char[256 * 256 * 4];

	for(int i=0; i<256; i++) {
		for(int j=0; j<256; j++) {
			int offset = (i * 256 + j) * 4;
			char value = perm[(j+perm[i]) & 0xff];
			pixels[offset+0] = grad3[value & 0xf][0] * 64 + 64;	/* Gradient x */
			pixels[offset+1] = grad3[value & 0xf][1] * 64 + 64;	/* Gradient y */
			pixels[offset+2] = grad3[value & 0xf][2] * 64 + 64;	/* Gradient z */
			pixels[offset+3] = value;							/* Permuted index */
		}
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	delete [] pixels;

	return tex;
}


//! draws the planet when normal map is used
//! @param StelPainter* (pointer to the painter used)
//! @param float (screen size)

void Planet::drawNMapSphere(StelPainter* painter, float screenSz)
{
    if (texMap)
    {
        if (!texMap->bind())
        {
            return;
        }
        painter->enableTexture2d(true);
    }
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);

    // Draw the spheroid itself
    // Adapt the number of facets according with the size of the sphere for optimization
    int nb_facet = (int)(screenSz * 40/50);	// 40 facets for 1024 pixels diameter on screen
    if (nb_facet<10) nb_facet = 10;
    if (nb_facet>40) nb_facet = 40;
    painter->setShadeModel(StelPainter::ShadeModelSmooth);
    // Rotate and add an extra quarter rotation so that the planet texture map
    // fits to the observers position. No idea why this is necessary,
    // perhaps some openGl strangeness, or confusing sin/cos.

    //the following lines are compatible to opengl es but have not been tested in an opengl es machine
#ifndef USE_OPENGL_ES2
	SolarSystem* ssm = GETSTELMODULE(SolarSystem);
    if (ssm->nMapShader != 0)
    {
			if (normalMap)
			{
				if (!normalMap->bind(1))
				{
					painter->sSphere(radius*sphereScale, oneMinusOblateness, nb_facet, nb_facet);
				}
				else
				{
					painter->enableTexture2d(true, 1);

					if (!permMap) {
				        permMap = createPermTexture();
					}

                    glActiveTexture(GL_TEXTURE2);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, permMap);

					if (!(cloudColor && cloudDensity && cloudSharpness && cloudScale && cloudVel)) {
					        cloudColor = Vec3f(0.0, 0.0, 0.0);
					        cloudVel = Vec3f(0.0, 0.5, 0.5);
					        cloudDensity = 0;
					        cloudSharpness = 0;
					        cloudScale = 1;
					}

					int location = -1;

			        location = ssm->nMapShader->uniformLocation("ccolor");
			        ssm->nMapShader->setUniform(location, cloudColor[0], cloudColor[1], cloudColor[2]);

					location = ssm->nMapShader->uniformLocation("cdensity");
			        ssm->nMapShader->setUniform(location, cloudDensity);

					location = ssm->nMapShader->uniformLocation("cscale");
			        ssm->nMapShader->setUniform(location, cloudScale);

					location = ssm->nMapShader->uniformLocation("csharp");
			        ssm->nMapShader->setUniform(location, cloudSharpness);

					ssm->nMapShader->use();

					location = ssm->nMapShader->uniformLocation("tex");
					ssm->nMapShader->setUniform(location, 0);

					location = ssm->nMapShader->uniformLocation("nmap");
					ssm->nMapShader->setUniform(location, 1);

			        location = ssm->nMapShader->uniformLocation("permap");
			        ssm->nMapShader->setUniform(location, 2);

			        float pixw = 1.0 / 256.0;
			        location = ssm->nMapShader->uniformLocation("pixw");
			        ssm->nMapShader->setUniform(location, pixw);


			        float halfpixw = 0.5 / 256.0;
					location = ssm->nMapShader->uniformLocation("halfpixw");
					ssm->nMapShader->setUniform(location, halfpixw);

					location = ssm->nMapShader->uniformLocation("cvel");
					ssm->nMapShader->setUniform(location, cloudVel[0], cloudVel[1], cloudVel[2]);

					QTime dat;
					float t = (float) dat.msecsTo(QTime::currentTime()) -
						QTime::currentTime().hour() * 3600000 -
						QTime::currentTime().minute() * 60000;

					t = t / 3000.0;

					location = ssm->nMapShader->uniformLocation("t");
					ssm->nMapShader->setUniform(location, t);

					painter->nmSphere(radius*sphereScale, oneMinusOblateness, nb_facet, nb_facet, ssm);

//					useShader(0);
					glActiveTexture(GL_TEXTURE2);
					glDisable(GL_TEXTURE_2D);
					painter->enableTexture2d(false, 1);
				}
			}
			else
			{
				painter->sSphere(radius*sphereScale, oneMinusOblateness, nb_facet, nb_facet);
			}
	}
	else
	{
		painter->sSphere(radius*sphereScale, oneMinusOblateness, nb_facet, nb_facet);
	}
#else
	painter->sSphere(radius*sphereScale, oneMinusOblateness, nb_facet, nb_facet);
#endif

    painter->setShadeModel(StelPainter::ShadeModelFlat);
    glDisable(GL_CULL_FACE);

    if (texMap) {
		painter->enableTexture2d(false, 0);
	}
}


// draws earth shadow overlapping the moon using stencil buffer
// umbra and penumbra are sized separately for accuracy
void Planet::drawEarthShadow(StelCore* core, StelPainter* sPainter)
{
	SolarSystem* ssm = GETSTELMODULE(SolarSystem);
	Vec3d e = ssm->getEarth()->getEclipticPos();
	Vec3d m = ssm->getMoon()->getEclipticPos();  // relative to earth
	Vec3d mh = ssm->getMoon()->getHeliocentricEclipticPos();  // relative to sun
	float mscale = ssm->getMoon()->getSphereScale();

	// shadow location at earth + moon distance along earth vector from sun
	Vec3d en = e;
	en.normalize();
	Vec3d shadow = en * (e.length() + m.length());

	// find shadow radii in AU
	double r_penumbra = shadow.length()*702378.1/AU/e.length() - 696000./AU;
	double r_umbra = 6378.1/AU - m.length()*(689621.9/AU/e.length());

	// find vector orthogonal to sun-earth vector using cross product with
	// a non-parallel vector
	Vec3d rpt = shadow^Vec3d(0,0,1);
	rpt.normalize();
	Vec3d upt = rpt*r_umbra*mscale*1.02;  // point on umbra edge
	rpt *= r_penumbra*mscale;  // point on penumbra edge

	// modify shadow location for scaled moon
	Vec3d mdist = shadow - mh;
	if (mdist.length() > r_penumbra + 2000./AU)
		return;   // not visible so don't bother drawing

	shadow = mh + mdist*mscale;
	r_penumbra *= mscale;

	StelProjectorP saveProj = sPainter->getProjector();
	sPainter->setProjector(core->getProjection(StelCore::FrameHeliocentricEcliptic));

	sPainter->enableTexture2d(true);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	sPainter->setColor(1,1,1);

	glEnable(GL_STENCIL_TEST);
	// We draw only where the stencil buffer is at 1, i.e. where the moon was drawn
	glStencilFunc(GL_EQUAL, 0x1, 0x1);
	// Don't change stencil buffer value
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// shadow radial texture
	texEarthShadow->bind();

	Vec3d r;

	// Draw umbra first
	QVector<Vec2f> texCoordArray;
	QVector<Vec3d> vertexArray;
	texCoordArray.reserve(210);
	vertexArray.reserve(210);
	texCoordArray << Vec2f(0.f, 0.5f);
	// johannes: work-around for nasty ATI rendering bug: use y-texture coordinate of 0.5 instead of 0.0
	vertexArray << shadow;

	const Mat4d& rotMat = Mat4d::rotation(shadow, 2.*M_PI/100.);
	r = upt;
	for (int i=1; i<=101; ++i)
	{
		// position in texture of umbra edge
		texCoordArray << Vec2f(0.6f, 0.5f);
		r.transfo4d(rotMat);
		vertexArray << shadow + r;
	}
	sPainter->setArrays(vertexArray.constData(), texCoordArray.constData());
	sPainter->drawFromArray(StelPainter::TriangleFan, 102);

	// now penumbra
	vertexArray.resize(0);
	texCoordArray.resize(0);
	Vec3d u;
	r = rpt;
	u = upt;
	for (int i=0; i<=200; i+=2)
	{
		r.transfo4d(rotMat);
		u.transfo4d(rotMat);
		texCoordArray << Vec2f(0.6f, 0.5f) << Vec2f(1.f, 0.5f); // position in texture of umbra edge
		vertexArray <<  shadow + u << shadow + r;
	}
	sPainter->setArrays(vertexArray.constData(), texCoordArray.constData());
	sPainter->drawFromArray(StelPainter::TriangleStrip, 202);
	glDisable(GL_STENCIL_TEST);
	glClearStencil(0x0);
	glClear(GL_STENCIL_BUFFER_BIT);	// Clean again to let a clean buffer for later Qt display
	sPainter->setProjector(saveProj);
}

void Planet::drawHints(const StelCore* core, const QFont& planetNameFont)
{
	if (labelsFader.getInterstate()<=0.f)
		return;

	const StelProjectorP prj = core->getProjection(StelCore::FrameJ2000);
	StelPainter sPainter(prj);
	sPainter.setFont(planetNameFont);
	// Draw nameI18 + scaling if it's not == 1.
	float tmp = (hintFader.getInterstate()<=0 ? 7.f : 10.f) + getAngularSize(core)*M_PI/180.f*prj->getPixelPerRadAtCenter()/1.44f; // Shift for nameI18 printing
	sPainter.setColor(labelColor[0], labelColor[1], labelColor[2],labelsFader.getInterstate());
	sPainter.drawText(screenPos[0],screenPos[1], getSkyLabel(core), 0, tmp, tmp, false);

	// hint disapears smoothly on close view
	if (hintFader.getInterstate()<=0)
		return;
	tmp -= 10.f;
	if (tmp<1) tmp=1;
	sPainter.setColor(labelColor[0], labelColor[1], labelColor[2],labelsFader.getInterstate()*hintFader.getInterstate()/tmp*0.7f);

	// Draw the 2D small circle
	glEnable(GL_BLEND);
	sPainter.enableTexture2d(true);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Planet::hintCircleTex->bind();
	sPainter.drawSprite2dMode(screenPos[0], screenPos[1], 11);
}

Ring::Ring(double radiusMin,double radiusMax,const QString &texname)
	 :radiusMin(radiusMin),radiusMax(radiusMax)
{
	tex = StelApp::getInstance().getTextureManager().createTexture("textures/"+texname);
}

Ring::~Ring(void)
{
}

void Ring::draw(StelPainter* sPainter,StelProjector::ModelViewTranformP transfo,double screenSz)
{
	screenSz -= 50;
	screenSz /= 250.0;
	if (screenSz < 0.0) screenSz = 0.0;
	else if (screenSz > 1.0) screenSz = 1.0;
	const int slices = 128+(int)((256-128)*screenSz);
	const int stacks = 8+(int)((32-8)*screenSz);

	// Normal transparency mode
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	sPainter->setColor(1.f, 1.f, 1.f);
	sPainter->enableTexture2d(true);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	if (tex) tex->bind();

	Mat4d mat = transfo->getApproximateLinearTransfo();
	  // solve the ring wraparound by culling:
	  // decide if we are above or below the ring plane
	const double h = mat.r[ 8]*mat.r[12]
				   + mat.r[ 9]*mat.r[13]
				   + mat.r[10]*mat.r[14];
	sPainter->sRing(radiusMin,radiusMax,(h<0.0)?slices:-slices,stacks, 0);
	glDisable(GL_CULL_FACE);
}


// draw orbital path of Planet
void Planet::drawOrbit(const StelCore* core)
{
	if (!orbitFader.getInterstate())
		return;
	if (!re.siderealPeriod)
		return;

	const StelProjectorP prj = core->getProjection(StelCore::FrameHeliocentricEcliptic);

	StelPainter sPainter(prj);

	// Normal transparency mode
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	sPainter.setColor(orbitColor[0], orbitColor[1], orbitColor[2], orbitFader.getInterstate());
	Vec3d onscreen;
	// special case - use current Planet position as center vertex so that draws
	// on it's orbit all the time (since segmented rather than smooth curve)
	Vec3d savePos = orbit[ORBIT_SEGMENTS/2];
	orbit[ORBIT_SEGMENTS/2]=getHeliocentricEclipticPos();
	orbit[ORBIT_SEGMENTS]=orbit[0];
	int nbIter = closeOrbit ? ORBIT_SEGMENTS : ORBIT_SEGMENTS-1;
	QVarLengthArray<float, 1024> vertexArray;

	sPainter.enableClientStates(true, false, false);

	for (int n=0; n<=nbIter; ++n)
	{
		if (prj->project(orbit[n],onscreen) && (vertexArray.size()==0 || !prj->intersectViewportDiscontinuity(orbit[n-1], orbit[n])))
		{
			vertexArray.append(onscreen[0]);
			vertexArray.append(onscreen[1]);
		}
		else if (!vertexArray.isEmpty())
		{
			sPainter.setVertexPointer(2, GL_FLOAT, vertexArray.constData());
			sPainter.drawFromArray(StelPainter::LineStrip, vertexArray.size()/2, 0, false);
			vertexArray.clear();
		}
	}
	orbit[ORBIT_SEGMENTS/2]=savePos;
	if (!vertexArray.isEmpty())
	{
		sPainter.setVertexPointer(2, GL_FLOAT, vertexArray.constData());
		sPainter.drawFromArray(StelPainter::LineStrip, vertexArray.size()/2, 0, false);
	}
	sPainter.enableClientStates(false);
}

void Planet::update(int deltaTime)
{
	hintFader.update(deltaTime);
	labelsFader.update(deltaTime);
	orbitFader.update(deltaTime);
}
