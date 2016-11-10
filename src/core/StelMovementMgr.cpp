/*
 * Stellarium
 * Copyright (C) 2007 Fabien Chereau
 * Copyright (C) 2015 Georg Zotti (offset view adaptations, Up vector fixes)
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

#include "StelMovementMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelUtils.hpp"
#include "StelTranslator.hpp"

#include <cmath>
#include <QString>
#include <QTextStream>
#include <QSettings>
#include <QKeyEvent>
#include <QDebug>

StelMovementMgr::StelMovementMgr(StelCore* acore)
	: currentFov(60.)
	, initFov(60.)
	, minFov(0.001389)
	, maxFov(100.)
	, core(acore)
	, objectMgr(NULL)
	, flagLockEquPos(false)
	, flagTracking(false)
	, flagInhibitAllAutomoves(false)
	, isMouseMovingHoriz(false)
	, isMouseMovingVert(false)
	, flagEnableMoveAtScreenEdge(false)
	, flagEnableMouseNavigation(true)
	, mouseZoomSpeed(30)
	, flagEnableZoomKeys(true)
	, flagEnableMoveKeys(true)
	, keyMoveSpeed(0.00025)
	, keyZoomSpeed(0.00025)
	, flagMoveSlow(false)
	, movementsSpeedFactor(1.0)
	, flagAutoMove(false)
	, zoomingMode(ZoomNone)
	, deltaFov(0.)
	, deltaAlt(0.)
	, deltaAz(0.)
	, flagManualZoom(false)
	, autoMoveDuration(1.5)
	, isDragging(false)
	, hasDragged(false)
	, previousX(0)
	, previousY(0)
	, beforeTimeDragTimeRate(0.)
	, dragTimeMode(false)
	, zoomMove()
	, flagAutoZoom(0)
	, flagAutoZoomOutResetsDirection(0)
	, mountMode(MountAltAzimuthal)
	, dragTriggerDistance(4.f)
	, viewportOffsetTimeline(NULL)
{
	setObjectName("StelMovementMgr");
	upVectorMountFrame.set(0.,0.,1.);
}

StelMovementMgr::~StelMovementMgr()
{
	if (viewportOffsetTimeline) delete viewportOffsetTimeline; viewportOffsetTimeline=NULL;
}

void StelMovementMgr::init()
{
	QSettings* conf = StelApp::getInstance().getSettings();
	objectMgr = GETSTELMODULE(StelObjectMgr);
	Q_ASSERT(conf);
	Q_ASSERT(objectMgr);
	connect(objectMgr, SIGNAL(selectedObjectChanged(StelModule::StelModuleSelectAction)),
			this, SLOT(selectedObjectChange(StelModule::StelModuleSelectAction)));

	flagEnableMoveAtScreenEdge = conf->value("navigation/flag_enable_move_at_screen_edge",false).toBool();
	mouseZoomSpeed = conf->value("navigation/mouse_zoom",30).toInt();
	flagEnableZoomKeys = conf->value("navigation/flag_enable_zoom_keys").toBool();
	flagEnableMoveKeys = conf->value("navigation/flag_enable_move_keys").toBool();
	keyMoveSpeed = conf->value("navigation/move_speed",0.0004f).toFloat();
	keyZoomSpeed = conf->value("navigation/zoom_speed", 0.0004f).toFloat();
	autoMoveDuration = conf->value ("navigation/auto_move_duration",1.5f).toFloat();
	flagManualZoom = conf->value("navigation/flag_manual_zoom").toBool();
	flagAutoZoomOutResetsDirection = conf->value("navigation/auto_zoom_out_resets_direction", true).toBool();
	flagEnableMouseNavigation = conf->value("navigation/flag_enable_mouse_navigation",true).toBool();

	minFov = conf->value("navigation/min_fov",0.001389).toDouble(); // default: minimal FOV = 5"
	maxFov = 100.;
	initFov = conf->value("navigation/init_fov",60.f).toFloat();
	currentFov = initFov;


	// With a special code of init_view_position=x/y/1 (or actually, anything equal or larger to 1) you can set zenith into the center and atan2(x/y) to bottom of screen.
	// examples: 1/0->0         NORTH is bottom
	//           -1/0 ->180     SOUTH is bottom
	//            0/-1 --> 90   EAST is bottom
	//            0/1  ->270    WEST is bottom
	Vec3f tmp = StelUtils::strToVec3f(conf->value("navigation/init_view_pos").toString());
	if (tmp[2]>=1)
	{
		//qDebug() << "Special zenith setup:";
		setViewDirectionJ2000(mountFrameToJ2000(Vec3d(0., 0., 1.)));
		initViewPos.set(0., 0., 1.);

		// It is not good to code 0/0/1 as view vector: bottom azimuth is undefined. Use default-south:
		if ((tmp[0]==0.) && (tmp[1]==0.))
			tmp[0]=-1.;

		upVectorMountFrame.set(tmp[0], tmp[1], 0.);
		upVectorMountFrame.normalize();
		initViewUp=upVectorMountFrame;
	}
	else
	{
		initViewPos.set(tmp[0], tmp[1], tmp[2]);
		initViewUp.set(0., 0., 1.);
		viewDirectionJ2000 = core->altAzToJ2000(initViewPos, StelCore::RefractionOff);
	}

	QString tmpstr = conf->value("navigation/viewing_mode", "horizon").toString();
	if (tmpstr=="equator")
		setMountMode(StelMovementMgr::MountEquinoxEquatorial);
	else
	{
		if (tmpstr=="horizon")
			setMountMode(StelMovementMgr::MountAltAzimuthal);
		else
		{
			qWarning() << "ERROR: Unknown viewing mode type: " << tmpstr;
			setMountMode(StelMovementMgr::MountEquinoxEquatorial);
		}
	}

	QString movementGroup = N_("Movement and Selection");
	addAction("actionSwitch_Equatorial_Mount", N_("Miscellaneous"), N_("Switch between equatorial and azimuthal mount"), "equatorialMount", "Ctrl+M");
	addAction("actionGoto_Selected_Object", movementGroup, N_("Center on selected object"), "setFlagTracking()", "Space");
	addAction("actionZoom_In_Auto", movementGroup, N_("Zoom in on selected object"), "autoZoomIn()", "/");
	addAction("actionZoom_Out_Auto", movementGroup, N_("Zoom out"), "autoZoomOut()", "\\");
	addAction("actionSet_Tracking", movementGroup, N_("Track object"), "tracking", "T");
	// Implementation of quick turning to different directions (examples: CdC, HNSKY)
	addAction("actionLook_Towards_East", movementGroup, N_("Look towards East"), "lookEast()", "Shift+E");
	addAction("actionLook_Towards_West", movementGroup, N_("Look towards West"), "lookWest()", "Shift+W");
	addAction("actionLook_Towards_North", movementGroup, N_("Look towards North"), "lookNorth()", "Shift+N");
	addAction("actionLook_Towards_South", movementGroup, N_("Look towards South"), "lookSouth()", "Shift+S");
	addAction("actionLook_Towards_Zenith", movementGroup, N_("Look towards Zenith"), "lookZenith()", "Shift+Z");
	// Additional hooks
	addAction("actionLook_Towards_NCP", movementGroup, N_("Look towards North Celestial pole"), "lookTowardsNCP()", "Alt+Shift+N");
	addAction("actionLook_Towards_SCP", movementGroup, N_("Look towards South Celestial pole"), "lookTowardsSCP()", "Alt+Shift+S");

	viewportOffsetTimeline=new QTimeLine(1000, this);
	viewportOffsetTimeline->setFrameRange(0, 100);
	connect(viewportOffsetTimeline, SIGNAL(valueChanged(qreal)), this, SLOT(handleViewportOffsetMovement(qreal)));
	targetViewportOffset.set(core->getViewportHorizontalOffset(), core->getViewportVerticalOffset());
}

void StelMovementMgr::setMountMode(MountMode m)
{
	mountMode = m;
	setViewDirectionJ2000(viewDirectionJ2000);
	// TODO: Decide whether re-setting Up-vector is required here.
	//setViewUpVector(Vec3d(0., 0., 1.));
	//setViewUpVectorJ2000(Vec3d(0., 0., 1.)); // Looks wrong on start.
	emit equatorialMountChanged(m==MountEquinoxEquatorial);
}

void StelMovementMgr::setFlagLockEquPos(bool b)
{
	flagLockEquPos=b;
}

void StelMovementMgr::setViewUpVectorJ2000(const Vec3d& up)
{
	upVectorMountFrame = j2000ToMountFrame(up);
}

// For simplicity you can set this directly. Take care when looking into poles like zenith in altaz mode:
// We have a problem if alt=+/-90degrees: view and up angles are ill-defined (actually, angle between them=0 and therefore we saw shaky rounding effects), therefore Bug LP:1068529
void StelMovementMgr::setViewUpVector(const Vec3d& up)
{
	upVectorMountFrame = up;
}

Vec3d StelMovementMgr::getViewUpVectorJ2000() const
{
	return mountFrameToJ2000(upVectorMountFrame);
}

bool StelMovementMgr::handleMouseMoves(int x, int y, Qt::MouseButtons)
{
	// Turn if the mouse is at the edge of the screen unless config asks otherwise
	if (flagEnableMoveAtScreenEdge)
	{
		if (x <= 1)
		{
			turnLeft(true);
			isMouseMovingHoriz = true;
		}
		else if (x >= core->getProjection2d()->getViewportWidth() - 2)
		{
			turnRight(true);
			isMouseMovingHoriz = true;
		}
		else if (isMouseMovingHoriz)
		{
			turnLeft(false);
			isMouseMovingHoriz = false;
		}

		if (y <= 1)
		{
			turnUp(true);
			isMouseMovingVert = true;
		}
		else if (y >= core->getProjection2d()->getViewportHeight() - 2)
		{
			turnDown(true);
			isMouseMovingVert = true;
		}
		else if (isMouseMovingVert)
		{
			turnUp(false);
			isMouseMovingVert = false;
		}
	}

	// We can hardly use the mouse exactly enough to go to the zenith/pole. Any mouse motion can safely reset the simplified up vector.
	setViewUpVector(Vec3d(0., 0., 1.));

	if (isDragging && flagEnableMouseNavigation)
	{
		if (hasDragged || (std::sqrt((float)((x-previousX)*(x-previousX) +(y-previousY)*(y-previousY)))>dragTriggerDistance))
		{
			hasDragged = true;
			setFlagTracking(false);
			dragView(previousX, previousY, x, y);
			previousX = x;
			previousY = y;
			return true;
		}
	}
	return false;
}

double StelMovementMgr::getCallOrder(StelModuleActionName actionName) const
{
	// allow plugins to intercept keys by using a lower number than this!
	if (actionName == StelModule::ActionHandleKeys)
		return 5;
	return 0;
}



void StelMovementMgr::handleKeys(QKeyEvent* event)
{
        if (event->type() == QEvent::KeyPress)
	{
		// Direction and zoom deplacements
		switch (event->key())
		{
			case Qt::Key_Left:
				turnLeft(true); break;
			case Qt::Key_Right:
				turnRight(true); break;
			case Qt::Key_Up:
				if (event->modifiers().testFlag(Qt::ControlModifier)){
					zoomIn(true);
				} else {
					turnUp(true);
				}
				break;
			case Qt::Key_Down:
				if (event->modifiers().testFlag(Qt::ControlModifier)) {
					zoomOut(true);
				} else {
					turnDown(true);
				}
				break;
			case Qt::Key_PageUp:
				zoomIn(true); break;
			case Qt::Key_PageDown:
				zoomOut(true); break;
			case Qt::Key_Shift:
				moveSlow(true); break;
			case Qt::Key_Space:
				if (event->modifiers().testFlag(Qt::ControlModifier))
					setDragTimeMode(true);
				break;
			default:
				return;
		}
	}
	else
	{
		// When a deplacement key is released stop moving
		switch (event->key())
		{
			case Qt::Key_Left:
				turnLeft(false); break;
			case Qt::Key_Right:
				turnRight(false); break;
			case Qt::Key_Up:
				zoomIn(false);
				turnUp(false);
				break;
			case Qt::Key_Down:
				zoomOut(false);
				turnDown(false);
				break;
			case Qt::Key_PageUp:
				zoomIn(false); break;
			case Qt::Key_PageDown:
				zoomOut(false); break;
			case Qt::Key_Shift:
				moveSlow(false); break;
			case Qt::Key_Control:
				// This can be all that is seen for anything with control, so stop them all.
				// This is true for 4.8.1
				turnRight(false);
				turnLeft(false);
				zoomIn(false);
				zoomOut(false);
				turnDown(false);
				turnUp(false);
				setDragTimeMode(false);
				break;
			default:
				return;
		}
	}
	event->accept();
}

//! Handle mouse wheel events.
void StelMovementMgr::handleMouseWheel(QWheelEvent* event)
{
	if (flagEnableMouseNavigation==false)
		return;

	// Manage only vertical wheel event
	if (event->orientation() != Qt::Vertical)
		return;
  
	const float numSteps = event->angleDelta().y() / 120.f;
	if (dragTimeMode)
	{
		if (event->modifiers() & Qt::ShiftModifier)
		{
			// move time by hours
			core->setJD(core->getJD()+numSteps/(24.f));
		}
		else
		{
			// move time by minutes
			core->setJD(core->getJD()+numSteps/(24.f*60.f));
		}
	}
	else
	{
		const float zoomFactor = std::exp(-mouseZoomSpeed * numSteps / 60.f);
		const float zoomDuration = 0.2f * qAbs(numSteps);
		zoomTo(getAimFov() * zoomFactor, zoomDuration);
	}
	event->accept();
}

void StelMovementMgr::addTimeDragPoint(int x, int y)
{
	DragHistoryEntry e;
	e.runTime=StelApp::getInstance().getTotalRunTime();
	e.jd=core->getJD();
	e.x=x;
	e.y=y;
	timeDragHistory.append(e);
	if (timeDragHistory.size()>3)
		timeDragHistory.removeFirst();
}

bool StelMovementMgr::handlePinch(qreal scale, bool started)
{
#ifdef Q_OS_WIN
	if (flagEnableMouseNavigation == false)
		return true;
#endif

	static double previousFov = 0;
	if (started)
		previousFov = getAimFov();
	if (scale>0)
		zoomTo(previousFov/scale, 0);
	return true;
}

void StelMovementMgr::handleMouseClicks(QMouseEvent* event)
{
	switch (event->button())
	{
		case Qt::RightButton:
		{
			if (event->type()==QEvent::MouseButtonRelease)
			{
				// Deselect the selected object
				StelApp::getInstance().getStelObjectMgr().unSelect();
				setFlagLockEquPos(false);
				event->accept();
				return;
			}
			break;
		}
		case Qt::LeftButton :
			if (event->type()==QEvent::MouseButtonPress)
			{
				isDragging = true;
				hasDragged = false;
				previousX = event->x();
				previousY = event->y();
				if (dragTimeMode)
				{
					beforeTimeDragTimeRate=core->getTimeRate();
					timeDragHistory.clear();
					addTimeDragPoint(event->x(), event->y());
				}
				event->accept();
				return;
			}
			else if (event->type()==QEvent::MouseButtonRelease)
			{
				isDragging = false;
				if (hasDragged)
				{
					event->accept();
					if (dragTimeMode)
					{
						if (timeDragHistory.size()>=3)
						{
							const double deltaT = timeDragHistory.last().runTime-timeDragHistory.first().runTime;
							Vec2f d(timeDragHistory.last().x-timeDragHistory.first().x, timeDragHistory.last().y-timeDragHistory.first().y);
							if (d.length()/deltaT<dragTriggerDistance)
							{
								core->setTimeRate(StelCore::JD_SECOND);
							}
							else
							{
								const double deltaJd = timeDragHistory.last().jd-timeDragHistory.first().jd;
								const double newTimeRate = deltaJd/deltaT;
								if (deltaT>0.00000001)
								{
									if (newTimeRate>=0)
										core->setTimeRate(qMax(newTimeRate, StelCore::JD_SECOND));
									else
										core->setTimeRate(qMin(newTimeRate, -StelCore::JD_SECOND));
								}
								else
									core->setTimeRate(beforeTimeDragTimeRate);
							}
						}
						else
							core->setTimeRate(beforeTimeDragTimeRate);
					}
					return;
				}
				else // has not dragged...
				{
					// It's a normal click release
					// TODO: Leave time dragging in Natural speed or zero speed (config option?) if mouse was resting
			#ifdef Q_OS_MAC
					// CTRL + left clic = right clic for 1 button mouse
					if (event->modifiers().testFlag(Qt::ControlModifier))
					{
						StelApp::getInstance().getStelObjectMgr().unSelect();
						event->accept();
						return;
					}

					// Try to select object at that position
					StelApp::getInstance().getStelObjectMgr().findAndSelect(StelApp::getInstance().getCore(), event->x(), event->y(),
						event->modifiers().testFlag(Qt::MetaModifier) ? StelModule::AddToSelection : StelModule::ReplaceSelection);
			#else
					StelApp::getInstance().getStelObjectMgr().findAndSelect(StelApp::getInstance().getCore(), event->x(), event->y(),
						event->modifiers().testFlag(Qt::ControlModifier) ? StelModule::AddToSelection : StelModule::ReplaceSelection);
			#endif
					if (StelApp::getInstance().getStelObjectMgr().getWasSelected())
					{
						setFlagTracking(false);
					}
					//GZ: You must comment out this line for testing Landscape transparency debug prints.
					//event->accept();
					return;
				}
			}
			else
			{
				qDebug() << "StelMovementMgr::handleMouseClicks: unknown mouse event type, skipping: " << event->type();
			}
			break;
		case Qt::MidButton :
			if (event->type()==QEvent::MouseButtonRelease)
			{
				if (objectMgr->getWasSelected())
				{
					moveToObject(objectMgr->getSelectedObject()[0],autoMoveDuration);
					setFlagTracking(true);
				}
			}
			break;
		default: break;
	}
	return;
}

void StelMovementMgr::setInitViewDirectionToCurrent()
{
	initViewPos = core->j2000ToAltAz(viewDirectionJ2000, StelCore::RefractionOff);
	QString dirStr = QString("%1,%2,%3").arg(initViewPos[0]).arg(initViewPos[1]).arg(initViewPos[2]);
	StelApp::getInstance().getSettings()->setValue("navigation/init_view_pos", dirStr);
}

/*************************************************************************
 The selected objects changed, follow it if we were already following another one
*************************************************************************/
void StelMovementMgr::selectedObjectChange(StelModule::StelModuleSelectAction)
{
	// If an object was selected keep the earth following
	if (objectMgr->getWasSelected())
	{
		if (getFlagTracking())
			setFlagLockEquPos(true);
		setFlagTracking(false);
	}
}

void StelMovementMgr::turnRight(bool s)
{
	if (s && flagEnableMoveKeys)
	{
		deltaAz = 1;
		setFlagTracking(false);
		setFlagLockEquPos(false);
	}
	else
		deltaAz = 0;
}

void StelMovementMgr::turnLeft(bool s)
{
	if (s && flagEnableMoveKeys)
	{
		deltaAz = -1;
		setFlagTracking(false);
		setFlagLockEquPos(false);
	}
	else
		deltaAz = 0;
}

void StelMovementMgr::turnUp(bool s)
{
	if (s && flagEnableMoveKeys)
	{
		deltaAlt = 1;
		setFlagTracking(false);
		setFlagLockEquPos(false);
	}
	else
		deltaAlt = 0;
}

void StelMovementMgr::turnDown(bool s)
{
	if (s && flagEnableMoveKeys)
	{
		deltaAlt = -1;
		setFlagTracking(false);
		setFlagLockEquPos(false);
	}
	else
		deltaAlt = 0;
}


void StelMovementMgr::zoomIn(bool s)
{
	if (flagEnableZoomKeys)
		deltaFov = -1*(s!=0);
}

void StelMovementMgr::zoomOut(bool s)
{
	if (flagEnableZoomKeys)
		deltaFov = (s!=0);
}

void StelMovementMgr::lookEast(void)
{
	float cx, cy;
	Vec3f dir;
	StelUtils::rectToSphe(&cy,&cx,core->j2000ToAltAz(getViewDirectionJ2000(), StelCore::RefractionOff));
	cy = M_PI/2.;
	StelUtils::spheToRect(cy, cx, dir);
	setViewDirectionJ2000(core->altAzToJ2000(Vec3d(dir[0], dir[1], dir[2]), StelCore::RefractionOff));
}

void StelMovementMgr::lookWest(void)
{
	float cx, cy;
	Vec3f dir;
	StelUtils::rectToSphe(&cy,&cx,core->j2000ToAltAz(getViewDirectionJ2000(), StelCore::RefractionOff));
	cy = 3.*M_PI/2.;
	StelUtils::spheToRect(cy, cx, dir);
	setViewDirectionJ2000(core->altAzToJ2000(Vec3d(dir[0], dir[1], dir[2]), StelCore::RefractionOff));
}

void StelMovementMgr::lookNorth(void)
{
	float cx, cy;
	Vec3f dir;
	StelUtils::rectToSphe(&cy,&cx,core->j2000ToAltAz(getViewDirectionJ2000(), StelCore::RefractionOff));
	cy = M_PI;
	StelUtils::spheToRect(cy, cx, dir);
	setViewDirectionJ2000(core->altAzToJ2000(Vec3d(dir[0], dir[1], dir[2]), StelCore::RefractionOff));
}

void StelMovementMgr::lookSouth(void)
{
	float cx, cy;
	Vec3f dir;
	StelUtils::rectToSphe(&cy,&cx,core->j2000ToAltAz(getViewDirectionJ2000(), StelCore::RefractionOff));
	cy = 0.;
	StelUtils::spheToRect(cy, cx, dir);
	setViewDirectionJ2000(core->altAzToJ2000(Vec3d(dir[0], dir[1], dir[2]), StelCore::RefractionOff));
}

void StelMovementMgr::lookZenith(void)
{
	Vec3f dir;
	StelUtils::spheToRect(M_PI, M_PI/2., dir);
	setViewDirectionJ2000(core->altAzToJ2000(Vec3d(dir[0], dir[1], dir[2]), StelCore::RefractionOff));
}

void StelMovementMgr::lookTowardsNCP(void)
{
	setViewDirectionJ2000(core->equinoxEquToJ2000(Vec3d(0,0,1)));
}

void StelMovementMgr::lookTowardsSCP(void)
{
	setViewDirectionJ2000(core->equinoxEquToJ2000(Vec3d(0,0,-1)));
}

// Increment/decrement smoothly the vision field and position
void StelMovementMgr::updateMotion(double deltaTime)
{
	updateVisionVector(deltaTime);

	const StelProjectorP proj = core->getProjection(StelCore::FrameJ2000);
	// the more it is zoomed, the lower the moving speed is (in angle)
	double depl=keyMoveSpeed*deltaTime*1000*currentFov;
	double deplzoom=keyZoomSpeed*deltaTime*1000*proj->deltaZoom(currentFov*(M_PI/360.0))*(360.0/M_PI);

	if (flagMoveSlow)
	{
		depl *= 0.2;
		deplzoom *= 0.2;
	}

	if (deltaAz<0)
	{
		deltaAz = -depl/30;
		if (deltaAz<-0.2)
			deltaAz = -0.2;
	}
	else if (deltaAz>0)
	{
		deltaAz = (depl/30);
		if (deltaAz>0.2)
			deltaAz = 0.2;
	}

	if (deltaAlt<0)
	{
		deltaAlt = -depl/30;
		if (deltaAlt<-0.2)
			deltaAlt = -0.2;
	}
	else if (deltaAlt>0)
	{
		deltaAlt = depl/30;
		if (deltaAlt>0.2)
			deltaAlt = 0.2;
	}

	if (deltaFov<0)
	{
		deltaFov = -deplzoom*5;
		if (deltaFov<-0.15*currentFov)
			deltaFov = -0.15*currentFov;
	}
	else if (deltaFov>0)
	{
		deltaFov = deplzoom*5;
		if (deltaFov>20)
			deltaFov = 20;
	}

	if (deltaFov != 0 )
	{
		changeFov(deltaFov);
	}
	panView(deltaAz, deltaAlt);
	updateAutoZoom(deltaTime);
}


void StelMovementMgr::updateVisionVector(double deltaTime)
{
	// Specialized setups cannot use this functionality!
	if (flagInhibitAllAutomoves)
		return;

	if (flagAutoMove)
	{
		if (!move.targetObject.isNull())
		{
			// if zooming in, object may be moving so be sure to zoom to latest position
			// In case we have offset center, we want object still visible in center.
			// Note that if we do not center on an object, we set view direction of the potentially offset screen center!
			// This is by design, to allow accurate setting of display coordinates.
			Vec3d v;
			switch (mountMode)
			{
				case MountAltAzimuthal:
					v = move.targetObject->getAltAzPosAuto(core);
					break;
				case MountEquinoxEquatorial:
					v = move.targetObject->getEquinoxEquatorialPos(core);
					break;
				case MountGalactic:
					v = move.targetObject->getGalacticPos(core);
					break;
				case MountSupergalactic:
					v = move.targetObject->getSupergalacticPos(core);
					break;
				default:
					qWarning() << "StelMovementMgr: unexpected mountMode" << mountMode;
					Q_ASSERT(0);
					v = move.targetObject->getAltAzPosAuto(core); // still do something useful
			}

			double lat, lon;
			StelUtils::rectToSphe(&lon, &lat, v);
			float altOffset=core->getCurrentStelProjectorParams().viewportCenterOffset[1]*currentFov*M_PI/180.0f;
			lat+=altOffset;
			StelUtils::spheToRect(lon, lat, v);
			move.aim=mountFrameToJ2000(v);
			move.aim.normalize();
			move.aim*=2.;
			// For aiming at objects, we can assume simple up vector.
			move.startUp=getViewUpVectorJ2000();
			move.aimUp=mountFrameToJ2000(Vec3d(0., 0., 1.));
		}
		else
		{
//			if (move.mountMode == MountAltAzimuthal)
//			{
//				move.startUp=Vec3d(0., 0., 1.);
//				move.aimUp=Vec3d(0., 0., 1.);
//			}
//			else
//			{ // March 2016
				move.startUp=getViewUpVectorJ2000();
				move.aimUp=mountFrameToJ2000(Vec3d(0., 0., 1.));
//			}
		}
		move.coef+=move.speed*deltaTime*1000;
		if (move.coef>=1.)
		{
			setViewUpVectorJ2000(move.aimUp);
			//qDebug() << "AutoMove finished. Setting Up vector (in mount frame) to " << upVectorMountFrame.v[0] << "/" << upVectorMountFrame.v[1] << "/" << upVectorMountFrame.v[2];
			flagAutoMove=false;
			move.coef=1.;
		}
		else
		{	// 2016-03 Maybe this is the culprit?
			//setViewUpVectorJ2000(move.startUp*(1.-move.coef) + move.aimUp*move.coef);
			setViewUpVectorJ2000(move.aimUp);
		}
		// Use a smooth function
		float smooth = 4.f;
		double c;
		if (zoomingMode==ZoomIn)
		{
			if (move.coef>.9)
			{
				c = 1.;
			}
			else
			{
				c = 1. - pow(1.-1.11*move.coef,3.);
			}
		}
		else if (zoomingMode==ZoomOut)
		{
			if (move.coef<0.1)
			{
				// keep in view at first as zoom out
				c = 0;
			}
			else
			{
				c =  pow(1.11*(move.coef-.1),3.);
			}
		}
		else
			c = std::atan(smooth * 2.*move.coef-smooth)/std::atan(smooth)/2+0.5;

		// 2016-03: In case of azimuthal moves, it is not useful to compute anything from J2000 coordinates.
		// Imagine a slow AltAz move during speedy timelapse: Aim will move!
		// TODO: all variants...
		Vec3d tmpStart;
		Vec3d tmpAim;
		if (move.mountMode==MountAltAzimuthal)
		{
			tmpStart=move.start;
			tmpAim=move.aim;
		}
		else
		{
			tmpStart = j2000ToMountFrame(move.start);
			tmpAim   = j2000ToMountFrame(move.aim);
		}
		double ra_aim, de_aim, ra_start, de_start;
		StelUtils::rectToSphe(&ra_start, &de_start, tmpStart);
		StelUtils::rectToSphe(&ra_aim, &de_aim, tmpAim);

		// Make sure the position of the object to be aimed at is defined...
		Q_ASSERT(move.aim[0]==move.aim[0] && move.aim[1]==move.aim[1] && move.aim[2]==move.aim[2]);
		// Trick to choose the good moving direction and never travel on a distance > PI
		if (ra_aim-ra_start > M_PI)
		{
			ra_aim -= 2.*M_PI;
		}
		else if (ra_aim-ra_start < -M_PI)
		{
			ra_aim += 2.*M_PI;
		}
		const double de_now = de_aim*c + de_start*(1.-c);
		const double ra_now = ra_aim*c + ra_start*(1.-c);
		Vec3d tmp;
		StelUtils::spheToRect(ra_now, de_now, tmp);
		// now tmp is either Mountframe or AltAz interpolated vector.
//		if (move.mountMode==MountAltAzimuthal)
//		{ // Actually, Altaz moves work in Altaz coords only. Maybe we are here?
//		}
//		else
			setViewDirectionJ2000(mountFrameToJ2000(tmp));
//			if (move.mountMode==MountAltAzimuthal)
//			{
//				setViewUpVector(Vec3d(0., 0., 1.));
//				qDebug() << "We do indeed set this";
//			}
		// qDebug() << "setting view direction to " << tmp.v[0] << "/" << tmp.v[1] << "/" << tmp.v[2];
	}
	else // no autoMove
	{
		if (flagTracking && objectMgr->getWasSelected()) // Equatorial vision vector locked on selected object
		{
			Vec3d v;
			switch (mountMode)
			{
				case MountAltAzimuthal:
					v = objectMgr->getSelectedObject()[0]->getAltAzPosAuto(core);
					break;
				case MountEquinoxEquatorial:
					v = objectMgr->getSelectedObject()[0]->getEquinoxEquatorialPos(core);
					break;
				case MountGalactic:
					v = objectMgr->getSelectedObject()[0]->getGalacticPos(core);
					break;
				case MountSupergalactic:
					v = objectMgr->getSelectedObject()[0]->getSupergalacticPos(core);
					break;
				default:
					qWarning() << "StelMovementMgr: unexpected mountMode" << mountMode;
					Q_ASSERT(0);
					v = move.targetObject->getAltAzPosAuto(core); // still do something useful in release build
			}

			double lat, lon; // general: longitudinal, latitudinal
			StelUtils::rectToSphe(&lon, &lat, v);
			float latOffset=core->getCurrentStelProjectorParams().viewportCenterOffset[1]*currentFov*M_PI/180.0f;
			lat+=latOffset;
			StelUtils::spheToRect(lon, lat, v);

			setViewDirectionJ2000(mountFrameToJ2000(v));
			setViewUpVectorJ2000(mountFrameToJ2000(Vec3d(0., 0., 1.))); // Does not disturb to reassure this former default.
		}
		else
		{
			if (flagLockEquPos) // Equatorial vision vector locked
			{
				// Recalc local vision vector
				setViewDirectionJ2000(viewDirectionJ2000);
			}
			else
			{
				// Vision vector locked to its position in the mountFrame
				setViewDirectionJ2000(mountFrameToJ2000(viewDirectionMountFrame));
				// After setting time, moveToAltAz broke the up vector without this:
				// Make sure this does not now break zenith views!
				// Or make sure to call moveToAltAz twice.
				setViewUpVectorJ2000(mountFrameToJ2000(Vec3d(0.,0.,1.)));
			}
		}
	}
}

// Go and zoom to the selected object. (Action linked to key, default "/")
void StelMovementMgr::autoZoomIn(float moveDuration, bool allowManualZoom)
{
	if (!objectMgr->getWasSelected())
		return;

	moveDuration /= movementsSpeedFactor;

	float manualMoveDuration;
	if (!getFlagTracking())
	{
		setFlagTracking(true); // includes a call to moveToObject(), but without zooming=1!
		moveToObject(objectMgr->getSelectedObject()[0], moveDuration, ZoomIn);
		manualMoveDuration = moveDuration;
	}
	else
	{
		// faster zoom in manual zoom mode once object is centered
		manualMoveDuration = moveDuration*.66f;
	}

	if( allowManualZoom && flagManualZoom )
	{
		// if manual zoom mode, user can zoom in incrementally
		float newfov = currentFov*0.5f;
		zoomTo(newfov, manualMoveDuration);
	}
	else
	{
		float satfov = objectMgr->getSelectedObject()[0]->getSatellitesFov(core);

		if (satfov>0.0 && currentFov*0.9>satfov)
			zoomTo(satfov, moveDuration);
		else
		{
			float closefov = objectMgr->getSelectedObject()[0]->getCloseViewFov(core);
			if (currentFov>closefov)
				zoomTo(closefov, moveDuration);
		}
	}
}


// Unzoom and go to the init position
void StelMovementMgr::autoZoomOut(float moveDuration, bool full)
{
	moveDuration /= movementsSpeedFactor;

	if (objectMgr->getWasSelected() && !full)
	{
		// If the selected object has satellites, unzoom to satellites view
		// unless specified otherwise
		float satfov = objectMgr->getSelectedObject()[0]->getSatellitesFov(core);

		if (satfov>0.0 && currentFov<=satfov*0.9)
		{
			zoomTo(satfov, moveDuration);
			return;
		}

		// If the selected object is part of a Planet subsystem (other than sun),
		// unzoom to subsystem view
		satfov = objectMgr->getSelectedObject()[0]->getParentSatellitesFov((core));
		if (satfov>0.0 && currentFov<=satfov*0.9)
		{
			zoomTo(satfov, moveDuration);
			return;
		}
	}

	zoomTo(initFov, moveDuration);
	if (flagAutoZoomOutResetsDirection)
	{
		moveToJ2000(core->altAzToJ2000(getInitViewingDirection(), StelCore::RefractionOff), mountFrameToJ2000(initViewUp), moveDuration, ZoomOut);
		setFlagTracking(false);
		setFlagLockEquPos(false);
	}
}

// This is called when you press SPACEBAR: slowly centering&tracking object
void StelMovementMgr::setFlagTracking(bool b)
{
	if (!b || !objectMgr->getWasSelected())
	{
		if(b!=flagTracking)
		{
			flagTracking=false;
			emit flagTrackingChanged(b);
		}
	}
	else
	{
		moveToObject(objectMgr->getSelectedObject()[0], getAutoMoveDuration());
		if(b!=flagTracking)
		{
			flagTracking=true;
			emit flagTrackingChanged(b);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// Move to the given J2000 equatorial position

// aim and aimUp must be in J2000 frame!
void StelMovementMgr::moveToJ2000(const Vec3d& aim, const Vec3d& aimUp, float moveDuration, ZoomingMode zooming)
{
	moveDuration /= movementsSpeedFactor;

	zoomingMode = zooming;
	move.aim=aim;
	move.aim.normalize();
	move.aim*=2.;
	move.aimUp=aimUp; // the new up vector. We cannot simply keep vertical axis, there may be the intention to look into the zenith or so.
	move.aimUp.normalize();
	move.start=viewDirectionJ2000;	
	move.start.normalize();
	move.startUp=getViewUpVectorJ2000();
	move.startUp.normalize();
	move.speed=1.f/(moveDuration*1000);
	move.coef=0.;
	move.targetObject.clear();
	//move.mountMode=mountMode; // Maybe better to have MountEquinoxEquatorial here? ==> YES, fixed orientation problem.
	move.mountMode=MountEquinoxEquatorial;
	flagAutoMove = true;
}

void StelMovementMgr::moveToObject(const StelObjectP& target, float moveDuration, ZoomingMode zooming)
{
	moveDuration /= movementsSpeedFactor;

	zoomingMode = zooming;
	move.aim=Vec3d(0);
	move.aimUp=mountFrameToJ2000(Vec3d(0., 0., 1.)); // the new up vector. We try simply vertical axis here. (Should be same as pre-0.15)
	move.aimUp.normalize();
	move.start=viewDirectionJ2000;
	move.start.normalize();
	move.startUp=getViewUpVectorJ2000();
	move.startUp.normalize();
	move.speed=1.f/(moveDuration*1000);
	move.coef=0.;
	move.targetObject = target;
	//move.mountMode=mountMode;  // Maybe better to have MountEquinoxEquatorial here? ==> YES, fixed orientation problem.
	move.mountMode=MountEquinoxEquatorial;
	flagAutoMove = true;
}

// March 2016: This call does nothing when mount frame is not AltAzi! (TODO later: rethink&fix.)
void StelMovementMgr::moveToAltAzi(const Vec3d& aim, const Vec3d &aimUp, float moveDuration, ZoomingMode zooming)
{
	if (mountMode!=StelMovementMgr::MountAltAzimuthal)
	{
		qDebug() << "StelMovementMgr: called moveToAltAzi, but not in AltAz mount frame. Ignoring.";
		return;
	}

	moveDuration /= movementsSpeedFactor;

	// TODO: Specify start and aim vectors in AltAz system! Then the auto functions should be able to work it out properly with only minor edits.
	zoomingMode = zooming;
	move.aim=aim;
	move.aim.normalize();
	move.aim*=2.;
	move.aimUp=aimUp; // the new up vector. We cannot simply keep vertical axis, there may be the intention to look into the zenith or so.
	move.aimUp.normalize();
	// TODO: Replace by fixed current orientation!
	move.start=core->j2000ToAltAz(viewDirectionJ2000, StelCore::RefractionOff);
	move.start.normalize();
	move.startUp.set(0., 0., 1.);
	move.speed=1.f/(moveDuration*1000);
	move.coef=0.;
	move.targetObject.clear();
	move.mountMode=MountAltAzimuthal; // This signals: start and aim are given in AltAz coordinates.
	flagAutoMove = true;
	//	// debug output if required
	//	double currAlt, currAzi, newAlt, newAzi;
	//	StelUtils::rectToSphe(&currAzi, &currAlt, move.start);
	//	StelUtils::rectToSphe(&newAzi, &newAlt, move.aim);
	//	qDebug() << "StelMovementMgr::moveToAltAzi() from alt:" << currAlt*(180./M_PI) << "/azi" << currAzi*(180./M_PI)  << "to alt:" << newAlt*(180./M_PI)  << "azi" << newAzi*(180./M_PI) ;
}


Vec3d StelMovementMgr::j2000ToMountFrame(const Vec3d& v) const
{
	switch (mountMode)
	{
		case MountAltAzimuthal:
			return core->j2000ToAltAz(v, StelCore::RefractionOff);
		case MountEquinoxEquatorial:
			return core->j2000ToEquinoxEqu(v);
		case MountGalactic:
			return core->j2000ToGalactic(v);
		case MountSupergalactic:
			return core->j2000ToSupergalactic(v);
	}
	Q_ASSERT(0);
	return Vec3d(0);
}

Vec3d StelMovementMgr::mountFrameToJ2000(const Vec3d& v) const
{
	switch (mountMode)
	{
		case MountAltAzimuthal:
			return core->altAzToJ2000(v, StelCore::RefractionOff);
		case MountEquinoxEquatorial:
			return core->equinoxEquToJ2000(v);
		case MountGalactic:
			return core->galacticToJ2000(v);
		case MountSupergalactic:
			return core->supergalacticToJ2000(v);
	}
	Q_ASSERT(0);
	return Vec3d(0);
}

void StelMovementMgr::setViewDirectionJ2000(const Vec3d& v)
{
	core->lookAtJ2000(v, getViewUpVectorJ2000());
	viewDirectionJ2000 = v;
	viewDirectionMountFrame = j2000ToMountFrame(v);
}

void StelMovementMgr::panView(const double deltaAz, const double deltaAlt)
{
	// The function is called in update loops, so make a quick check for exit.
	if ((deltaAz==0.) && (deltaAlt==0.))
		return;

	double azVision, altVision;
	StelUtils::rectToSphe(&azVision,&altVision,j2000ToMountFrame(viewDirectionJ2000));
	// Az is counted from South, eastward.

	// qDebug() << "Azimuth:" << azVision * 180./M_PI << "Altitude:" << altVision * 180./M_PI << "Up.X=" << upVectorMountFrame.v[0] << "Up.Y=" << upVectorMountFrame.v[1] << "Up.Z=" << upVectorMountFrame.v[2];

	// if we are just looking into the pole, azimuth can hopefully be recovered from the customized up vector!
	// When programmatically centering on a pole, we should have set a better up vector for |alt|>0.9*M_PI/2.
	if (fabs(altVision)> 0.95* M_PI/2.)
	{
		if (upVectorMountFrame.v[2] < 0.9)
		{
			// qDebug() << "Recovering azimuth...";
			azVision=atan2(-upVectorMountFrame.v[1], -upVectorMountFrame.v[0]);
			if (altVision < 0.)
				azVision+=M_PI;
		}
//		else
//		{
//			// qDebug() << "UpVector:" << upVectorMountFrame.v[0] << "/" << upVectorMountFrame.v[1] << "/" << upVectorMountFrame.v[2] << "Cannot recover azimuth. Hope it's OK";
//		}
	}

	// if we are moving in the Azimuthal angle (left/right)
	if (deltaAz)
		azVision-=deltaAz;
	if (deltaAlt)
	{
		if (altVision+deltaAlt <= M_PI_2 && altVision+deltaAlt >= -M_PI_2) altVision+=deltaAlt;
		if (altVision+deltaAlt > M_PI_2) altVision = M_PI_2 - 0.000001;		// Prevent bug
		if (altVision+deltaAlt < -M_PI_2) altVision = -M_PI_2 + 0.000001;	// Prevent bug
	}

	// recalc all the position variables
	if (deltaAz || deltaAlt)
	{
		setFlagTracking(false);
		Vec3d tmp;
		StelUtils::spheToRect(azVision, altVision, tmp);
		setViewDirectionJ2000(mountFrameToJ2000(tmp));
		setViewUpVector(Vec3d(0., 0., 1.)); // We ensured above that view vector is never parallel to this simple up vector.
	}
}


//! Make the first screen position correspond to the second (useful for mouse dragging)
void StelMovementMgr::dragView(int x1, int y1, int x2, int y2)
{
	if (dragTimeMode)
	{
		core->setTimeRate(0);
		Vec3d v1, v2;
		const StelProjectorP prj = core->getProjection(StelCore::FrameEquinoxEqu);
		prj->unProject(x2,y2, v2);
		prj->unProject(x1,y1, v1);
		v1[2]=0; v1.normalize();
		v2[2]=0; v2.normalize();
		double angle = (v2^v1)[2];
		double deltaDay = angle/(2.*M_PI)*core->getLocalSiderealDayLength();
		core->setJD(core->getJD()+deltaDay);
		addTimeDragPoint(x2, y2);
	}
	else
	{
		Vec3d tempvec1, tempvec2;
		const StelProjectorP prj = core->getProjection(StelCore::FrameJ2000);
		prj->unProject(x2,y2, tempvec2);
		prj->unProject(x1,y1, tempvec1);
		double az1, alt1, az2, alt2;
		StelUtils::rectToSphe(&az1, &alt1, j2000ToMountFrame(tempvec1));
		StelUtils::rectToSphe(&az2, &alt2, j2000ToMountFrame(tempvec2));
		panView(az2-az1, alt1-alt2);
	}
	setFlagTracking(false);
	setFlagLockEquPos(false);
}


// Update autoZoom if activated
void StelMovementMgr::updateAutoZoom(double deltaTime)
{
	if (flagAutoZoom)
	{
		// Use a smooth function
		double c;

		if( zoomMove.startFov > zoomMove.aimFov )
		{
			// slow down as we approach final view
			c = 1 - (1-zoomMove.coef)*(1-zoomMove.coef)*(1-zoomMove.coef);
		}
		else
		{
			// speed up as we leave zoom target
			c = (zoomMove.coef)*(zoomMove.coef)*(zoomMove.coef);
		}

		double newFov=zoomMove.startFov + (zoomMove.aimFov - zoomMove.startFov) * c;

		zoomMove.coef+=zoomMove.speed*deltaTime*1000;
		if (zoomMove.coef>=1.)
		{
			flagAutoZoom = 0;
			newFov=zoomMove.aimFov;
		}

		setFov(newFov); // updates currentFov->don't use newFov later!

		// In case we have offset center, we want object still visible in center.
		if (flagTracking && objectMgr->getWasSelected()) // vision vector locked on selected object
		{
			Vec3d v, vUp;
			switch (mountMode)
			{
				case MountAltAzimuthal:
					v = objectMgr->getSelectedObject()[0]->getAltAzPosAuto(core);
					break;
				case MountEquinoxEquatorial:
					v = objectMgr->getSelectedObject()[0]->getEquinoxEquatorialPos(core);
					break;
				case MountGalactic:
					v = objectMgr->getSelectedObject()[0]->getGalacticPos(core);
					break;
				case MountSupergalactic:
					v = objectMgr->getSelectedObject()[0]->getSupergalacticPos(core);
					break;
				default:
					qWarning() << "StelMovementMgr: unexpected mountMode" << mountMode;
					Q_ASSERT(0);
			}

			double lat, lon; // general: longitudinal, latitudinal
			StelUtils::rectToSphe(&lon, &lat, v); // guaranteed to be normalized.
			// vUp could usually be (0/0/1) in most cases, unless |lat|==pi/2. We MUST build an adequate Up vector!
			if (fabs(lat)>0.9*M_PI/2.0)
			{
				vUp = Vec3d(-cos(lon), -sin(lon), 0.) * (lat>0. ? 1. : -1. );
			}
			else
				vUp.set(0.,0.,1.);
			float latOffset=core->getCurrentStelProjectorParams().viewportCenterOffset[1]*currentFov*M_PI/180.0f;
			lat+=latOffset;
			StelUtils::spheToRect(lon, lat, v);

			if (flagAutoMove)
			{
				move.aim=mountFrameToJ2000(v);
				move.aim.normalize();
				move.aim*=2.;
				move.aimUp=mountFrameToJ2000(vUp);
				move.aimUp.normalize();
			}
			else
			{
				setViewDirectionJ2000(mountFrameToJ2000(v));
				setViewUpVectorJ2000(mountFrameToJ2000(vUp));
			}
		}
	}
}

// Zoom to the given field of view
void StelMovementMgr::zoomTo(double aim_fov, float moveDuration)
{
	moveDuration /= movementsSpeedFactor;

	zoomMove.aimFov=aim_fov;
	zoomMove.startFov=currentFov;
	zoomMove.speed=1.f/(moveDuration*1000);
	zoomMove.coef=0.;
	flagAutoZoom = true;
}

void StelMovementMgr::changeFov(double deltaFov)
{
	// if we are zooming in or out
	if (deltaFov)
		setFov(currentFov + deltaFov);
}

double StelMovementMgr::getAimFov(void) const
{
	return (flagAutoZoom ? zoomMove.aimFov : currentFov);
}

void StelMovementMgr::setMaxFov(double max)
{
	maxFov = max;
	if (currentFov > max)
	{
		setFov(max);
	}
}


void StelMovementMgr::moveViewport(float offsetX, float offsetY, const float duration)
{
	//clamp to valid range
	offsetX = qMax(-50.f, qMin(50.f, offsetX));
	offsetY = qMax(-50.f, qMin(50.f, offsetY));

	Vec2f oldTargetViewportOffset = targetViewportOffset;
	targetViewportOffset.set(offsetX, offsetY);

	if(offsetX != oldTargetViewportOffset[0])
		emit viewportHorizontalOffsetTargetChanged(offsetX);
	if(offsetY != oldTargetViewportOffset[1])
		emit viewportVerticalOffsetTargetChanged(offsetY);

	if (duration<=0.0f)
	{
		//avoid using the timeline to minimize overhead
		//core->setViewportHorizontalOffset(targetViewportOffset[0]);
		//core->setViewportVerticalOffset(targetViewportOffset[1]);
		core->setViewportOffset(offsetX, offsetY);
		return;
	}

	// Frame will now be 0..100, and we must interpolate in handleViewportOffsetMovement(frame) between old and new offsets.
	oldViewportOffset.set(core->getViewportHorizontalOffset(), core->getViewportVerticalOffset());

	viewportOffsetTimeline->stop();
	viewportOffsetTimeline->setDuration(1000.*duration);

	//qDebug() << "moveViewport() started, from " << oldViewportOffset.v[0] << "/" << oldViewportOffset.v[1] << " towards " << offsetX << "/" << offsetY;
	viewportOffsetTimeline->start();
}

// slot which is connected to the viewportOffsetTimeline and does the actual updates.
void StelMovementMgr::handleViewportOffsetMovement(qreal value)
{
	// value is always 0...1
	float offsetX=oldViewportOffset.v[0] + (targetViewportOffset.v[0]-oldViewportOffset.v[0])*value;
	float offsetY=oldViewportOffset.v[1] + (targetViewportOffset.v[1]-oldViewportOffset.v[1])*value;
	//qDebug() << "handleViewportOffsetMovement(" << value << "): Setting viewport offset to " << offsetX << "/" << offsetY;
	//core->setViewportHorizontalOffset(offsetX);
	//core->setViewportVerticalOffset(offsetY);
	core->setViewportOffset(offsetX, offsetY);
}
