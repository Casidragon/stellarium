/*
 * Copyright (C) 2009 Matthew Gates
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

#include "TextUserInterface.hpp"
#include "TuiNode.hpp"
#include "TuiNodeActivate.hpp"
#include "TuiNodeBool.hpp"
#include "TuiNodeInt.hpp"
#include "TuiNodeDouble.hpp"
#include "TuiNodeFloat.hpp"
#include "TuiNodeDateTime.hpp"
#include "TuiNodeColor.hpp"
#include "TuiNodeEnum.hpp"

#include "StelProjector.hpp"
#include "StelPainter.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocaleMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StarMgr.hpp"
#include "StelMovementMgr.hpp"
#include "StelSkyDrawer.hpp"
#include "ConstellationMgr.hpp"
#include "NebulaMgr.hpp"
#include "SolarSystem.hpp"
#include "LandscapeMgr.hpp"
#include "GridLinesMgr.hpp"
#include "MilkyWay.hpp"
#include "StelLocation.hpp"
#include "StelMainGraphicsView.hpp"
#include "StelSkyCultureMgr.hpp"
#include "StelFileMgr.hpp"
#include "StelUtils.hpp"
#include "StelScriptMgr.hpp"

#include <QtOpenGL>
#include <QKeyEvent>
#include <QDebug>
#include <QLabel>

/*************************************************************************
 Utility functions
*************************************************************************/
QString colToConf(const Vec3f& c)
{
	return QString("%1,%2,%3").arg(c[0],2,'f',2).arg(c[1],2,'f',2).arg(c[2],2,'f',2);
}

/*************************************************************************
 This method is the one called automatically by the StelModuleMgr just 
 after loading the dynamic library
*************************************************************************/
StelModule* TextUserInterfaceStelPluginInterface::getStelModule() const
{
	return new TextUserInterface();
}

StelPluginInfo TextUserInterfaceStelPluginInterface::getPluginInfo() const
{
	StelPluginInfo info;
	info.id = "TextUserInterface";
	info.displayedName = N_("Text User Interface");
	info.authors = "Matthew Gates";
	info.contact = "http://porpoisehead.net/";
	info.description = N_("Plugin implementation of 0.9.x series Text User Interface (TUI), used in planetarium systems");
	return info;
}

Q_EXPORT_PLUGIN2(TextUserInterface, TextUserInterfaceStelPluginInterface)


/*************************************************************************
 Constructor
*************************************************************************/
TextUserInterface::TextUserInterface()
	: dummyDialog(this), tuiActive(false), currentNode(NULL)
{
	setObjectName("TextUserInterface");
	font.setPixelSize(15);
}

/*************************************************************************
 Destructor
*************************************************************************/
TextUserInterface::~TextUserInterface()
{
}

/*************************************************************************
 Reimplementation of the getCallOrder method
*************************************************************************/
double TextUserInterface::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName==StelModule::ActionDraw)
		return StelApp::getInstance().getModuleMgr().getModule("LandscapeMgr")->getCallOrder(actionName)+10.;
	if (actionName==StelModule::ActionHandleKeys)
		return -1;
	return 0;
}


/*************************************************************************
 Init our module
*************************************************************************/
void TextUserInterface::init()
{
	qDebug() << "init called for TextUserInterface";
	TuiNode* m1 = new TuiNode(QString("1. %1").arg(q_("Set Location")));
	TuiNode* m1_1 = new TuiNodeDouble(QString("1.1 %1").arg(q_("Latitude")),
	                                  this, SLOT(setLatitude(double)),
	                                  getLatitude(), -180, 180, 0.5, m1);
	TuiNode* m1_2 = new TuiNodeDouble(QString("1.2 %1").arg(q_("Longitude")),
	                                  this, SLOT(setLongitude(double)),
	                                  getLongitude(), -180, 180, 0.5, m1, m1_1);
	TuiNode* m1_3 = new TuiNodeInt(QString("1.3 %1").arg(q_("Altitude")),
					this, SLOT(setAltitude(int)),
					StelApp::getInstance().getCore()->getCurrentLocation().altitude,
					-200, 200000, 100, m1, m1_2);
	TuiNode* m1_4 = new TuiNodeEnum(QString("1.4 %1").arg(q_("Solar System Body")),
	                                this, SLOT(setHomePlanet(QString)),
					GETSTELMODULE(SolarSystem)->getAllPlanetEnglishNames(),
					StelApp::getInstance().getCore()->getCurrentLocation().planetName,
	                                m1, m1_3);
	m1_1->setPrevNode(m1_4);
	m1_1->setNextNode(m1_2);
	m1_2->setNextNode(m1_3);
	m1_3->setNextNode(m1_4);
	m1_4->setNextNode(m1_1);
	m1->setChildNode(m1_1);

	TuiNode* m2 = new TuiNode(QString("2. %1").arg(q_("Date & Time")), NULL, m1);
	m1->setNextNode(m2);
	TuiNode* m2_1 = new TuiNodeDateTime(QString("2.1 %1").arg(q_("Sky Time")),
										StelApp::getInstance().getCore(),
	                                    SLOT(setJDay(double)),  
										StelApp::getInstance().getCore()->getJDay(),
	                                    m2);
	TuiNode* m2_2 = new TuiNode(QString("2.2 %1").arg(q_("Set Time Zone")), m2, m2_1);
	TuiNode* m2_3 = new TuiNode(QString("2.3 %1").arg(q_("Day Keys")), m2, m2_2);
	TuiNode* m2_4 = new TuiNodeDateTime(QString("2.4 %1").arg(q_("Preset Sky Time")),
										StelApp::getInstance().getCore(),
	                                    SLOT(setPresetSkyTime(double)), 
										StelApp::getInstance().getCore()->getPresetSkyTime(),
	                                    m2, m2_3);
	QStringList startupModes;
	startupModes << "system" << "preset";
	TuiNode* m2_5 = new TuiNodeEnum(QString("2.5 %1").arg(q_("Sky Time at Startup")),
	                                this, SLOT(setStartupDateMode(QString)), startupModes,
									StelApp::getInstance().getCore()->getStartupTimeMode(),
	                                m2, m2_4);
	QStringList dateFormats;
	dateFormats << "system_default" << "mmddyyyy" << "ddmmyyyy" << "yyyymmdd";
	TuiNode* m2_6 = new TuiNodeEnum(QString("2.6 %1").arg(q_("Date Display Format")),
	                                this, SLOT(setDateFormat(QString)), dateFormats,
	                                StelApp::getInstance().getLocaleMgr().getDateFormatStr(),
	                                m2, m2_5);
	QStringList timeFormats;
	timeFormats << "system_default" << "12h" << "24h";
	TuiNode* m2_7 = new TuiNodeEnum(QString("2.7 %1").arg(q_("Time Display Format")),
	                                this, SLOT(setTimeFormat(QString)), timeFormats,
	                                StelApp::getInstance().getLocaleMgr().getTimeFormatStr(),
	                                m2, m2_6);
	m2_1->setPrevNode(m2_7);
	m2_1->setNextNode(m2_2);
	m2_2->setNextNode(m2_3);
	m2_3->setNextNode(m2_4);
	m2_4->setNextNode(m2_5);
	m2_5->setNextNode(m2_6);
	m2_6->setNextNode(m2_7);
	m2_7->setNextNode(m2_1);
	m2->setChildNode(m2_1);

	TuiNode* m3 = new TuiNode(QString("3. %1").arg(q_("General")), NULL, m2);
	m2->setNextNode(m3);
	TuiNode* m3_1 = new TuiNodeEnum(QString("3.1 %1").arg(q_("Sky Culture")),
	                                this, 
	                                SLOT(setSkyCulture(QString)), 
	                                StelApp::getInstance().getSkyCultureMgr().getSkyCultureListI18(),
	                                StelApp::getInstance().getSkyCultureMgr().getCurrentSkyCultureNameI18(),
	                                m3);
	TuiNode* m3_2 = new TuiNodeEnum(QString("3.2 %1").arg(q_("Language")),
	                                this, 
	                                SLOT(setAppLanguage(QString)), 
									StelTranslator::globalTranslator.getAvailableLanguagesNamesNative(StelFileMgr::getLocaleDir()),
					StelTranslator::iso639_1CodeToNativeName(StelApp::getInstance().getLocaleMgr().getAppLanguage()),
	                                m3, m3_1);
	m3_1->setPrevNode(m3_2);
	m3_1->setNextNode(m3_2);
	m3_2->setNextNode(m3_1);
	m3->setChildNode(m3_1);

	TuiNode* m4 = new TuiNode(QString("4. %1").arg(q_("Stars")), NULL, m3);
	m3->setNextNode(m4);
	TuiNode* m4_1 = new TuiNodeBool(QString("4.1 %1").arg(q_("Show Stars")),
	                                GETSTELMODULE(StarMgr), SLOT(setFlagStars(bool)), 
	                                GETSTELMODULE(StarMgr)->getFlagStars(), m4); 
	TuiNode* m4_2 = new TuiNodeDouble(QString("4.2 %1").arg(q_("Star Relative Scale")),
	                                  StelApp::getInstance().getCore()->getSkyDrawer(), SLOT(setRelativeStarScale(double)),
	                                  StelApp::getInstance().getCore()->getSkyDrawer()->getRelativeStarScale(), 0.0, 5., 0.15,
	                                  m4, m4_1);
	TuiNode* m4_3 = new TuiNodeDouble(QString("4.3 %1").arg(q_("Absolute Star Scale")),
	                                  StelApp::getInstance().getCore()->getSkyDrawer(), SLOT(setAbsoluteStarScale(double)),
	                                  StelApp::getInstance().getCore()->getSkyDrawer()->getAbsoluteStarScale(), 0.0, 9., 0.15,
	                                  m4, m4_2);
	TuiNode* m4_4 = new TuiNodeDouble(QString("4.4 %1").arg(q_("Twinkling")),
	                                  StelApp::getInstance().getCore()->getSkyDrawer(), SLOT(setTwinkleAmount(double)),
	                                  StelApp::getInstance().getCore()->getSkyDrawer()->getTwinkleAmount(), 0.0, 1.5, 0.1,
	                                  m4, m4_3);
	m4_1->setPrevNode(m4_4);
	m4_1->setNextNode(m4_2);
	m4_2->setNextNode(m4_3);
	m4_3->setNextNode(m4_4);
	m4_4->setNextNode(m4_1);
	m4->setChildNode(m4_1);

	TuiNode* m5 = new TuiNode(QString("5. %1").arg(q_("Colors")), NULL, m4);
	m4->setNextNode(m5);
	TuiNode* m5_1 = new TuiNodeColor(QString("5.1 %1").arg(q_("Constellation Lines")),
	                                 GETSTELMODULE(ConstellationMgr), SLOT(setLinesColor(Vec3f)),
	                                 GETSTELMODULE(ConstellationMgr)->getLinesColor(), 
	                                 m5);
	TuiNode* m5_2 = new TuiNodeColor(QString("5.2 %1").arg(q_("Constellation Names")),
	                                 GETSTELMODULE(ConstellationMgr), SLOT(setLabelsColor(Vec3f)),
	                                 GETSTELMODULE(ConstellationMgr)->getLabelsColor(), 
	                                 m5, m5_1);
	TuiNode* m5_3 = new TuiNode(QString("5.3 %1").arg(q_("Constellation Art")), m5, m5_2);
	TuiNode* m5_4 = new TuiNodeColor(QString("5.4 %1").arg(q_("Constellation Boundaries")),
	                                 GETSTELMODULE(ConstellationMgr), SLOT(setBoundariesColor(Vec3f)),
	                                 GETSTELMODULE(ConstellationMgr)->getBoundariesColor(), 
                                         m5, m5_3);
	TuiNode* m5_5 = new TuiNodeDouble(QString("5.5 %1").arg(q_("Constellation Art Intensity")),
	                                  GETSTELMODULE(ConstellationMgr), SLOT(setArtIntensity(double)),
	                                  GETSTELMODULE(ConstellationMgr)->getArtIntensity(), 0.0, 1.0, 0.05,
	                                  m5, m5_4);
	TuiNode* m5_6 = new TuiNodeColor(QString("5.6 %1").arg(q_("Cardinal Points")),
	                                 GETSTELMODULE(LandscapeMgr), SLOT(setColorCardinalPoints(Vec3f)),
	                                 GETSTELMODULE(LandscapeMgr)->getColorCardinalPoints(), 
	                                 m5, m5_5);
	TuiNode* m5_7 = new TuiNodeColor(QString("5.7 %1").arg(q_("Planet Names")),
	                                 GETSTELMODULE(SolarSystem), SLOT(setLabelsColor(Vec3f)),
	                                 GETSTELMODULE(SolarSystem)->getLabelsColor(), 
	                                 m5, m5_6);
	TuiNode* m5_8 = new TuiNodeColor(QString("5.8 %1").arg(q_("Planet Orbits")),
	                                 GETSTELMODULE(SolarSystem), SLOT(setOrbitsColor(Vec3f)),
	                                 GETSTELMODULE(SolarSystem)->getOrbitsColor(), 
	                                 m5, m5_7);
	TuiNode* m5_9 = new TuiNodeColor(QString("5.9 %1").arg(q_("Planet Trails")),
	                                 GETSTELMODULE(SolarSystem), SLOT(setTrailsColor(Vec3f)),
	                                 GETSTELMODULE(SolarSystem)->getTrailsColor(), 
	                                 m5, m5_8);
	TuiNode* m5_10 = new TuiNodeColor(QString("5.10 %1").arg(q_("Meridian Line")),
	                                 GETSTELMODULE(GridLinesMgr), SLOT(setColorMeridianLine(Vec3f)),
	                                 GETSTELMODULE(GridLinesMgr)->getColorMeridianLine(), 
	                                 m5, m5_9);
	TuiNode* m5_11 = new TuiNodeColor(QString("5.11 %1").arg(q_("Azimuthal Grid")),
	                                 GETSTELMODULE(GridLinesMgr), SLOT(setColorAzimuthalGrid(Vec3f)),
	                                 GETSTELMODULE(GridLinesMgr)->getColorAzimuthalGrid(), 
	                                 m5, m5_10);
	TuiNode* m5_12 = new TuiNodeColor(QString("5.12 %1").arg(q_("Equatorial Grid")),
	                                 GETSTELMODULE(GridLinesMgr), SLOT(setColorEquatorGrid(Vec3f)),
	                                 GETSTELMODULE(GridLinesMgr)->getColorEquatorGrid(), 
	                                 m5, m5_11);
	TuiNode* m5_13 = new TuiNodeColor(QString("5.13 %1").arg(q_("Equatorial J2000 Grid")),
	                                 GETSTELMODULE(GridLinesMgr), SLOT(setColorEquatorJ2000Grid(Vec3f)),
	                                 GETSTELMODULE(GridLinesMgr)->getColorEquatorJ2000Grid(), 
	                                 m5, m5_12);
	TuiNode* m5_14 = new TuiNodeColor(QString("5.14 %1").arg(q_("Equator Line")),
	                                 GETSTELMODULE(GridLinesMgr), SLOT(setColorEquatorLine(Vec3f)),
	                                 GETSTELMODULE(GridLinesMgr)->getColorEquatorLine(), 
	                                 m5, m5_13);
	TuiNode* m5_15 = new TuiNodeColor(QString("5.15 %1").arg(q_("Ecliptic Line")),
	                                 GETSTELMODULE(GridLinesMgr), SLOT(setColorEclipticLine(Vec3f)),
	                                 GETSTELMODULE(GridLinesMgr)->getColorEclipticLine(), 
	                                 m5, m5_14);
	TuiNode* m5_16 = new TuiNodeColor(QString("5.16 %1").arg(q_("Nebula Names")),
	                                 GETSTELMODULE(NebulaMgr), SLOT(setLabelsColor(Vec3f)),
	                                 GETSTELMODULE(NebulaMgr)->getLabelsColor(), 
	                                 m5, m5_15);
	TuiNode* m5_17 = new TuiNodeColor(QString("5.17 %1").arg(q_("Nebubla Hints")),
	                                 GETSTELMODULE(NebulaMgr), SLOT(setCirclesColor(Vec3f)),
	                                 GETSTELMODULE(NebulaMgr)->getCirclesColor(), 
	                                 m5, m5_16);
	m5_1->setPrevNode(m5_17);
	m5_1->setNextNode(m5_2);
	m5_2->setNextNode(m5_3);
	m5_3->setNextNode(m5_4);
	m5_4->setNextNode(m5_5);
	m5_5->setNextNode(m5_6);
	m5_6->setNextNode(m5_7);
	m5_7->setNextNode(m5_8);
	m5_8->setNextNode(m5_9);
	m5_9->setNextNode(m5_10);
	m5_10->setNextNode(m5_11);
	m5_11->setNextNode(m5_12);
	m5_12->setNextNode(m5_13);
	m5_13->setNextNode(m5_14);
	m5_14->setNextNode(m5_15);
	m5_15->setNextNode(m5_16);
	m5_16->setNextNode(m5_17);
	m5_17->setNextNode(m5_1);
	m5->setChildNode(m5_1);

	TuiNode* m6 = new TuiNode(QString("6. %1").arg(q_("Effects")), NULL, m5);
	m5->setNextNode(m6);
	TuiNode* m6_1 = new TuiNodeInt(QString("6.1 %1").arg(q_("Light Pollution Level")),
	                               GETSTELMODULE(LandscapeMgr), SLOT(setAtmosphereBortleLightPollution(int)),
	                               3, 1, 9, 1,
	                               m6);
	TuiNode* m6_2 = new TuiNodeEnum(QString("6.2 %1").arg(q_("Landscape")),
	                                GETSTELMODULE(LandscapeMgr),
	                                SLOT(setCurrentLandscapeName(QString)),
	                                GETSTELMODULE(LandscapeMgr)->getAllLandscapeNames(),
	                                GETSTELMODULE(LandscapeMgr)->getCurrentLandscapeName(),
	                                m6, m6_1);
	TuiNode* m6_3 = new TuiNodeBool(QString("6.3 %1").arg(q_("Manual Zoom")),
	                                GETSTELMODULE(StelMovementMgr), SLOT(setFlagAutoZoomOutResetsDirection(bool)), 
	                                GETSTELMODULE(StelMovementMgr)->getFlagAutoZoomOutResetsDirection(), 
	                                m6, m6_2);
	TuiNode* m6_4 = new TuiNode(QString("6.4 %1").arg(q_("Magnitude Scaling Multiplier")), m6, m6_3);
	TuiNode* m6_5 = new TuiNodeFloat(QString("6.5 %1").arg(q_("Milky Way Intensity")),
	                                 GETSTELMODULE(MilkyWay), SLOT(setIntensity(float)),
	                                 GETSTELMODULE(MilkyWay)->getIntensity(), 0, 10.0, 0.1, 
	                                 m6, m6_4);
	TuiNode* m6_6 = new TuiNode(QString("6.6 %1").arg(q_("Nebula Label Frequency")), m6, m6_5);
	TuiNode* m6_7 = new TuiNodeFloat(QString("6.7 %1").arg(q_("Zoom Duration")),
	                                 GETSTELMODULE(StelMovementMgr), SLOT(setAutoMoveDuration(float)), 
	                                 GETSTELMODULE(StelMovementMgr)->getAutoMoveDuration(), 0, 20.0, 0.1,
	                                 m6, m6_6);
	TuiNode* m6_8 = new TuiNode(QString("6.8 %1").arg(q_("Cursor Timeout")), m6, m6_7);
	TuiNode* m6_9 = new TuiNodeBool(QString("6.9 %1").arg(q_("Setting Landscape Sets Location")),
	                                GETSTELMODULE(LandscapeMgr), SLOT(setFlagLandscapeSetsLocation(bool)), 
	                                GETSTELMODULE(LandscapeMgr)->getFlagLandscapeSetsLocation(), 
	                                m6, m6_8);
	m6_1->setPrevNode(m6_9);
	m6_1->setNextNode(m6_2);
	m6_2->setNextNode(m6_3);
	m6_3->setNextNode(m6_4);
	m6_4->setNextNode(m6_5);
	m6_5->setNextNode(m6_6);
	m6_6->setNextNode(m6_7);
	m6_7->setNextNode(m6_8);
	m6_8->setNextNode(m6_9);
	m6_9->setNextNode(m6_1);
	m6->setChildNode(m6_1);

	TuiNode* m7 = new TuiNode(QString("7. %1").arg(q_("Scripts")), NULL, m6);
	m6->setNextNode(m7);
	TuiNode* m7_1 = new TuiNodeEnum(QString("7.1 %1").arg(q_("Run Local Script")),
									&StelMainGraphicsView::getInstance().getScriptMgr(),
	                                SLOT(runScript(QString)),
									StelMainGraphicsView::getInstance().getScriptMgr().getScriptList(),
	                                "",
	                                m7);
	TuiNode* m7_2 = new TuiNodeActivate(QString("7.2 %1").arg(q_("Stop Running Script")), &StelMainGraphicsView::getInstance().getScriptMgr(), SLOT(stopScript()), m7, m7_1);
	TuiNode* m7_3 = new TuiNode(QString("7.3 %1").arg(q_("CD/DVD Script")), m7, m7_2);
	m7_1->setPrevNode(m7_2);
	m7_1->setNextNode(m7_2);
	m7_2->setNextNode(m7_3);
	m7_3->setNextNode(m7_1);
	m7->setChildNode(m7_1);

	TuiNode* m8 = new TuiNode(QString("8. %1").arg(q_("Administration")), NULL, m7);
	m7->setNextNode(m8);
	m8->setNextNode(m1);
	m1->setPrevNode(m8);
	TuiNode* m8_1 = new TuiNode(QString("8.1 %1").arg(q_("Load Default Configuration")), m8);
	TuiNode* m8_2 = new TuiNodeActivate(QString("8.2 %1").arg(q_("Save Current Configuration")), this, SLOT(saveDefaultSettings()), m8, m8_1);
	TuiNode* m8_3 = new TuiNode(QString("8.3 %1").arg(q_("Shut Down")), m8, m8_2);
	m8_1->setPrevNode(m8_3);
	m8_1->setNextNode(m8_2);
	m8_2->setNextNode(m8_3);
	m8_3->setNextNode(m8_1);
	m8->setChildNode(m8_1);


	currentNode = m1;
}

/*************************************************************************
 Draw our module.
*************************************************************************/
void TextUserInterface::draw(StelCore* core)
{
	if (tuiActive)
	{
		QString tuiText = q_("[no TUI node]");
		if (currentNode!=NULL)
			tuiText = currentNode->getDisplayText();

		StelPainter painter(core->getProjection(StelCore::FrameAltAz));
		painter.setFont(font);
		painter.setColor(0.3,1,0.3);
		painter.drawText(StelMainGraphicsView::getInstance().size().width()*0.6,
				 50, tuiText, 0, 0, 0, false);
	}
}

void TextUserInterface::handleKeys(QKeyEvent* event)
{
	if (currentNode == NULL)
	{
		qWarning() << "WARNING: no current node in TextUserInterface plugin";
		event->setAccepted(false);
		return;
	}

	if (event->type()==QEvent::KeyPress && event->key()==Qt::Key_M)
	{
		tuiActive = ! tuiActive;
		dummyDialog.close();
		event->setAccepted(true);
		return;
	}

	if (!tuiActive)
	{
		event->setAccepted(false);
		return;
	}

	if (event->type()==QEvent::KeyPress)
	{
		TuiNodeResponse response = currentNode->handleKey(event->key());
		if (response.accepted)
		{
			currentNode = response.newNode;
			event->setAccepted(true);
		}
		else
		{
			event->setAccepted(false);
		}
		return;
	}
}

void TextUserInterface::setHomePlanet(QString planetName)
{
	StelCore* core = StelApp::getInstance().getCore();
	if (core->getCurrentLocation().planetName != planetName)
	{
		StelLocation newLocation = core->getCurrentLocation();
		newLocation.planetName = planetName;
		core->moveObserverTo(newLocation);
	}
}

void TextUserInterface::setAltitude(int altitude)
{
	StelCore* core = StelApp::getInstance().getCore();
	if (core->getCurrentLocation().altitude != altitude)
	{
		StelLocation newLocation = core->getCurrentLocation();
		newLocation.altitude = altitude;
		core->moveObserverTo(newLocation, 0.0, 0.0);
	}
}

void TextUserInterface::setLatitude(double latitude)
{
	StelCore* core = StelApp::getInstance().getCore();
	if (core->getCurrentLocation().latitude != latitude)
	{
		StelLocation newLocation = core->getCurrentLocation();
		newLocation.latitude = latitude;
		core->moveObserverTo(newLocation, 0.0, 0.0);
	}
}

void TextUserInterface::setLongitude(double longitude)
{
	StelCore* core = StelApp::getInstance().getCore();
	if (core->getCurrentLocation().longitude != longitude)
	{
		StelLocation newLocation = core->getCurrentLocation();
		newLocation.longitude = longitude;
		core->moveObserverTo(newLocation, 0.0, 0.0);
	}
}

double TextUserInterface::getLatitude(void)
{
	return StelApp::getInstance().getCore()->getCurrentLocation().latitude;
}

double TextUserInterface::getLongitude(void)
{
	return StelApp::getInstance().getCore()->getCurrentLocation().longitude;
}

void TextUserInterface::setStartupDateMode(QString mode)
{
	StelApp::getInstance().getCore()->setStartupTimeMode(mode);
}

void TextUserInterface::setDateFormat(QString format)
{
	StelApp::getInstance().getLocaleMgr().setDateFormatStr(format);
}

void TextUserInterface::setTimeFormat(QString format)
{
	StelApp::getInstance().getLocaleMgr().setTimeFormatStr(format);
}

void TextUserInterface::setSkyCulture(QString i18)
{
	StelApp::getInstance().getSkyCultureMgr().setCurrentSkyCultureNameI18(i18);
}

void TextUserInterface::setAppLanguage(QString lang)
{
	QString code = StelTranslator::nativeNameToIso639_1Code(lang);
	StelApp::getInstance().getLocaleMgr().setAppLanguage(code);
	StelApp::getInstance().getLocaleMgr().setSkyLanguage(code);
}

void TextUserInterface::saveDefaultSettings(void)
{
	QSettings* conf = StelApp::getInstance().getSettings();
	Q_ASSERT(conf);

	LandscapeMgr* lmgr = GETSTELMODULE(LandscapeMgr);
	Q_ASSERT(lmgr);
	SolarSystem* ssmgr = GETSTELMODULE(SolarSystem);
	Q_ASSERT(ssmgr);
	StelSkyDrawer* skyd = StelApp::getInstance().getCore()->getSkyDrawer();
	Q_ASSERT(skyd);
	ConstellationMgr* cmgr = GETSTELMODULE(ConstellationMgr);
	Q_ASSERT(cmgr);
	StarMgr* smgr = GETSTELMODULE(StarMgr);
	Q_ASSERT(smgr);
	NebulaMgr* nmgr = GETSTELMODULE(NebulaMgr);
	Q_ASSERT(nmgr);
	GridLinesMgr* glmgr = GETSTELMODULE(GridLinesMgr);
	Q_ASSERT(glmgr);
	StelMovementMgr* mvmgr = GETSTELMODULE(StelMovementMgr);
	Q_ASSERT(mvmgr);
	StelCore* core = StelApp::getInstance().getCore();
	Q_ASSERT(core);
	MilkyWay* milk = GETSTELMODULE(MilkyWay);
	Q_ASSERT(milk);
	const StelProjectorP proj = StelApp::getInstance().getCore()->getProjection(StelCore::FrameJ2000);
	Q_ASSERT(proj);
	StelLocaleMgr& lomgr = StelApp::getInstance().getLocaleMgr();

	// MENU ITEMS
	// sub-menu 1: location
	// TODO
	

	// sub-menu 2: date and time
	conf->setValue("navigation/preset_sky_time", core->getPresetSkyTime());
	conf->setValue("navigation/startup_time_mode", core->getStartupTimeMode());
	conf->setValue("navigation/startup_time_mode", core->getStartupTimeMode());
	conf->setValue("localization/time_display_format", lomgr.getTimeFormatStr());
	conf->setValue("localization/date_display_format", lomgr.getDateFormatStr());


	// sub-menu 3: general
	StelApp::getInstance().getSkyCultureMgr().setDefaultSkyCultureID(StelApp::getInstance().getSkyCultureMgr().getCurrentSkyCultureID());
	QString langName = lomgr.getAppLanguage();
	conf->setValue("localization/app_locale", StelTranslator::nativeNameToIso639_1Code(langName));
	langName = StelApp::getInstance().getLocaleMgr().getSkyLanguage();
	conf->setValue("localization/sky_locale", StelTranslator::nativeNameToIso639_1Code(langName));

	// sub-menu 4: stars
	conf->setValue("astro/flag_stars", smgr->getFlagStars());
	conf->setValue("stars/absolute_scale", skyd->getAbsoluteStarScale());
	conf->setValue("stars/relative_scale", skyd->getRelativeStarScale());
	conf->setValue("stars/flag_star_twinkle", skyd->getFlagTwinkle());

	// sub-menu 5: colors
	conf->setValue("color/const_lines_color", colToConf(cmgr->getLinesColor()));
 	conf->setValue("color/const_names_color", colToConf(cmgr->getLabelsColor()));
	conf->setValue("color/const_boundary_color", colToConf(cmgr->getBoundariesColor()));
	conf->setValue("viewing/constellation_art_intensity", cmgr->getArtIntensity());
	conf->setValue("color/cardinal_color", colToConf(lmgr->getColorCardinalPoints()) );
	conf->setValue("color/planet_names_color", colToConf(ssmgr->getLabelsColor()));
	conf->setValue("color/planet_orbits_color", colToConf(ssmgr->getOrbitsColor()));
	conf->setValue("color/object_trails_color", colToConf(ssmgr->getTrailsColor()));
	conf->setValue("color/meridian_color", colToConf(glmgr->getColorMeridianLine()));
	conf->setValue("color/azimuthal_color", colToConf(glmgr->getColorAzimuthalGrid()));
	conf->setValue("color/equator_color", colToConf(glmgr->getColorEquatorGrid()));
	conf->setValue("color/equatorial_J2000_color", colToConf(glmgr->getColorEquatorJ2000Grid()));
	conf->setValue("color/equator_color", colToConf(glmgr->getColorEquatorLine()));
	conf->setValue("color/ecliptic_color", colToConf(glmgr->getColorEclipticLine()));
	conf->setValue("color/nebula_label_color", colToConf(nmgr->getLabelsColor()));
	conf->setValue("color/nebula_circle_color", colToConf(nmgr->getCirclesColor()));

	// sub-menu 6: effects
	// TODO enable this when we do the release after 0.10.2 - this plugin will crash for 0.10.2 because
	// getAtmosphereBortleLightPollution() was not defined in the 0.10.2 release.
	// conf->setValue("stars/init_bortle_scale", lmgr->getAtmosphereBortleLightPollution());
	lmgr->setDefaultLandscapeID(lmgr->getCurrentLandscapeID());
	conf->setValue("navigation/auto_zoom_out_resets_direction", mvmgr->getFlagAutoZoomOutResetsDirection());
	conf->setValue("astro/milky_way_intensity", milk->getIntensity());
	conf->setValue("navigation/auto_move_duration", mvmgr->getAutoMoveDuration());
	conf->setValue("landscape/flag_landscape_sets_location", lmgr->getFlagLandscapeSetsLocation());

	// GLOBAL DISPLAY SETTINGS
	// TODO 
	
	qDebug() << "TextUserInterface::saveDefaultSettings done";
}




