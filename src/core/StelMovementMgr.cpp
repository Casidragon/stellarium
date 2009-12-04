/*
 * Stellarium
 * Copyright (C) 2007 Fabien Chereau
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

#include "StelMovementMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelNavigator.hpp"
#include "StelScriptMgr.hpp"
#include "StelUtils.hpp"

#include <QString>
#include <QTextStream>
#include <QSettings>
#include <QKeyEvent>
#include <QDebug>

StelMovementMgr::StelMovementMgr(StelCore* acore) : core(acore),
	flagLockEquPos(false),
	flagTracking(false),
	isMouseMovingHoriz(false),
	isMouseMovingVert(false),
	flagEnableMouseNavigation(true),
	keyMoveSpeed(0.00025),
	flagMoveSlow(false),
	flagAutoMove(false),
	deltaFov(0.),
	deltaAlt(0.),
	deltaAz(0.),
	flagAutoZoom(0),
	flagAutoZoomOutResetsDirection(0)
{
	setObjectName("StelMovementMgr");
	isDragging = false;
	mountMode = MountAltAzimuthal;  // default
}

StelMovementMgr::~StelMovementMgr()
{
}

void StelMovementMgr::init()
{
	QSettings* conf = StelApp::getInstance().getSettings();
	objectMgr = GETSTELMODULE(StelObjectMgr);
	Q_ASSERT(conf);
	Q_ASSERT(objectMgr);

	flagEnableMoveAtScreenEdge = conf->value("navigation/flag_enable_move_at_screen_edge",false).toBool();
	mouseZoomSpeed = conf->value("navigation/mouse_zoom",30).toInt();
	flagEnableZoomKeys = conf->value("navigation/flag_enable_zoom_keys").toBool();
	flagEnableMoveKeys = conf->value("navigation/flag_enable_move_keys").toBool();
	keyMoveSpeed = conf->value("navigation/move_speed",0.0004).toDouble();
	keyZoomSpeed = conf->value("navigation/zoom_speed", 0.0004).toDouble();
	autoMoveDuration = conf->value ("navigation/auto_move_duration",1.5).toDouble();
	flagManualZoom = conf->value("navigation/flag_manual_zoom").toBool();
	flagAutoZoomOutResetsDirection = conf->value("navigation/auto_zoom_out_resets_direction", true).toBool();
	flagEnableMouseNavigation = conf->value("navigation/flag_enable_mouse_navigation",true).toBool();

	minFov = 0.0001;
	maxFov = 100.;
	initFov = conf->value("navigation/init_fov",60.).toDouble();
	currentFov = initFov;

	initViewPos = StelUtils::strToVec3f(conf->value("navigation/init_view_pos").toString());
	viewDirectionJ2000 = core->getNavigator()->altAzToJ2000(initViewPos);

	QString tmpstr = conf->value("navigation/viewing_mode", "horizon").toString();
	if (tmpstr=="equator")
		setMountMode(StelMovementMgr::MountEquinoxEquatorial);
	else
	{
		if (tmpstr=="horizon")
			setMountMode(StelMovementMgr::MountAltAzimuthal);
		else
		{
			qWarning() << "ERROR : Unknown viewing mode type : " << tmpstr;
			setMountMode(StelMovementMgr::MountEquinoxEquatorial);
		}
	}
}

void StelMovementMgr::setMountMode(MountMode m)
{
	mountMode = m;
	setViewDirectionJ2000(viewDirectionJ2000);
}

void StelMovementMgr::setFlagLockEquPos(bool b)
{
	flagLockEquPos=b;
}

Vec3d StelMovementMgr::getViewUpVectorJ2000() const
{
	return mountFrameToJ2000(Vec3d(0,0,1));
}

bool StelMovementMgr::handleMouseMoves(int x, int y, Qt::MouseButtons b)
{
	// Turn if the mouse is at the edge of the screen unless config asks otherwise
	if (flagEnableMoveAtScreenEdge)
	{
		if (x <= 1)
		{
			turnLeft(1);
			isMouseMovingHoriz = true;
		}
		else if (x >= core->getProjection2d()->getViewportWidth() - 2)
		{
			turnRight(1);
			isMouseMovingHoriz = true;
		}
		else if (isMouseMovingHoriz)
		{
			turnLeft(0);
			isMouseMovingHoriz = false;
		}

		if (y <= 1)
		{
			turnUp(1);
			isMouseMovingVert = true;
		}
		else if (y >= core->getProjection2d()->getViewportHeight() - 2)
		{
			turnDown(1);
			isMouseMovingVert = true;
		}
		else if (isMouseMovingVert)
		{
			turnUp(0);
			isMouseMovingVert = false;
		}
	}

	if (isDragging && flagEnableMouseNavigation)
	{
		if (hasDragged || (std::sqrt((double)((x-previousX)*(x-previousX) +(y-previousY)*(y-previousY)))>4.))
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
				if (event->modifiers().testFlag(Qt::ControlModifier)) zoomIn(true);
				else turnUp(true);
				break;
			case Qt::Key_Down:
				if (event->modifiers().testFlag(Qt::ControlModifier)) zoomOut(true);
				else turnDown(true);
				break;
			case Qt::Key_PageUp:
				zoomIn(true); break;
			case Qt::Key_PageDown:
				zoomOut(true); break;
			case Qt::Key_Shift:
				moveSlow(true); break;
			default:
				return;
		}
	}
	else
	{
		// When a deplacement key is released stop mooving
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
	int numDegrees = event->delta() / 8;
	int numSteps = numDegrees / 15;
	zoomTo(getAimFov()-mouseZoomSpeed*numSteps*getAimFov()/60., 0.2);
	event->accept();
}

void StelMovementMgr::handleMouseClicks(QMouseEvent* event)
{
	switch (event->button())
	{
		case Qt::RightButton : break;
		case Qt::LeftButton :
			if (event->type()==QEvent::MouseButtonPress)
			{
				isDragging = true;
				hasDragged = false;
				previousX = event->x();
				previousY = event->y();
				event->accept();
				return;
			}
			else
			{
				if (isDragging)
				{
					isDragging = false;
					if (hasDragged)
					{
						event->accept();
						return;
					}
					else
						return;
				}
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
	initViewPos = core->getNavigator()->j2000ToAltAz(viewDirectionJ2000);
	QString dirStr = QString("%1,%2,%3").arg(initViewPos[0]).arg(initViewPos[1]).arg(initViewPos[2]);
	StelApp::getInstance().getSettings()->setValue("navigation/init_view_pos", dirStr);
}

/*************************************************************************
 The selected objects changed, follow it it we were already following another one
*************************************************************************/
void StelMovementMgr::selectedObjectChangeCallBack(StelModuleSelectAction action)
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
	else
	{
		if (deltaAz>0)
		{
			deltaAz = (depl/30);
			if (deltaAz>0.2)
				deltaAz = 0.2;
		}
	}
	if (deltaAlt<0)
	{
		deltaAlt = -depl/30;
		if (deltaAlt<-0.2)
			deltaAlt = -0.2;
	}
	else
	{
		if (deltaAlt>0)
		{
			deltaAlt = depl/30;
			if (deltaAlt>0.2)
				deltaAlt = 0.2;
		}
	}

	if (deltaFov<0)
	{
		deltaFov = -deplzoom*5;
		if (deltaFov<-0.15*currentFov)
			deltaFov = -0.15*currentFov;
	}
	else
	{
		if (deltaFov>0)
		{
			deltaFov = deplzoom*5;
			if (deltaFov>20)
				deltaFov = 20;
		}
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
	if (flagAutoMove)
	{
		if (!move.targetObject.isNull())
		{
			// if zooming in, object may be moving so be sure to zoom to latest position
			move.aim = move.targetObject->getJ2000EquatorialPos(core->getNavigator());
			move.aim.normalize();
			move.aim*=2.;
		}

		move.coef+=move.speed*deltaTime*1000;
		if (move.coef>=1.)
		{
			flagAutoMove=false;
			move.coef=1.;
		}

		// Use a smooth function
		float smooth = 4.f;
		double c;
		if (zoomingMode==1)
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
		else if (zoomingMode==-1)
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

		Vec3d tmpStart(j2000ToMountFrame(move.start));
		Vec3d tmpAim(j2000ToMountFrame(move.aim));
		double ra_aim, de_aim, ra_start, de_start;
		StelUtils::rectToSphe(&ra_start, &de_start, tmpStart);
		StelUtils::rectToSphe(&ra_aim, &de_aim, tmpAim);

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
		setViewDirectionJ2000(mountFrameToJ2000(tmp));
	}
	else
	{
		if (flagTracking && objectMgr->getWasSelected()) // Equatorial vision vector locked on selected object
		{
			setViewDirectionJ2000(objectMgr->getSelectedObject()[0]->getJ2000EquatorialPos(core->getNavigator()));
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
			}
		}
	}
}

// Go and zoom to the selected object.
void StelMovementMgr::autoZoomIn(float moveDuration, bool allowManualZoom)
{
	if (!objectMgr->getWasSelected())
		return;

	if (StelApp::getInstance().getScriptMgr().scriptIsRunning())
		moveDuration /= StelApp::getInstance().getScriptMgr().getScriptRate();

	float manualMoveDuration;
	if (!getFlagTracking())
	{
		setFlagTracking(true);
		moveToObject(objectMgr->getSelectedObject()[0], moveDuration, 1);
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
		float satfov = objectMgr->getSelectedObject()[0]->getSatellitesFov(core->getNavigator());

		if (satfov>0.0 && currentFov*0.9>satfov)
			zoomTo(satfov, moveDuration);
		else
		{
			float closefov = objectMgr->getSelectedObject()[0]->getCloseViewFov(core->getNavigator());
			if (currentFov>closefov)
				zoomTo(closefov, moveDuration);
		}
	}
}


// Unzoom and go to the init position
void StelMovementMgr::autoZoomOut(float moveDuration, bool full)
{
	if (StelApp::getInstance().getScriptMgr().scriptIsRunning())
		moveDuration /= StelApp::getInstance().getScriptMgr().getScriptRate();

	if (objectMgr->getWasSelected() && !full)
	{
		// If the selected object has satellites, unzoom to satellites view
		// unless specified otherwise
		float satfov = objectMgr->getSelectedObject()[0]->getSatellitesFov(core->getNavigator());

		if (satfov>0.0 && currentFov<=satfov*0.9)
		{
			zoomTo(satfov, moveDuration);
			return;
		}

		// If the selected object is part of a Planet subsystem (other than sun),
		// unzoom to subsystem view
		satfov = objectMgr->getSelectedObject()[0]->getParentSatellitesFov((core->getNavigator()));
		if (satfov>0.0 && currentFov<=satfov*0.9)
		{
			zoomTo(satfov, moveDuration);
			return;
		}
	}

	zoomTo(initFov, moveDuration);
	if (flagAutoZoomOutResetsDirection)
		moveToJ2000(core->getNavigator()->altAzToJ2000(getInitViewingDirection()), moveDuration, -1);
	setFlagTracking(false);
	setFlagLockEquPos(false);
}


void StelMovementMgr::setFlagTracking(bool b)
{
	if (!b || !objectMgr->getWasSelected())
	{
		flagTracking=false;
	}
	else
	{
		moveToObject(objectMgr->getSelectedObject()[0], getAutoMoveDuration());
		flagTracking=true;
	}
}


////////////////////////////////////////////////////////////////////////////////
// Move to the given J2000 equatorial position
void StelMovementMgr::moveToJ2000(const Vec3d& aim, float moveDuration, int zooming)
{
	if (StelApp::getInstance().getScriptMgr().scriptIsRunning())
		moveDuration /= StelApp::getInstance().getScriptMgr().getScriptRate();

	zoomingMode = zooming;
	move.aim=aim;
	move.aim.normalize();
	move.aim*=2.;
	move.start=viewDirectionJ2000;
	move.start.normalize();
	move.speed=1.f/(moveDuration*1000);
	move.coef=0.;
	move.targetObject.clear();
	flagAutoMove = true;
}

void StelMovementMgr::moveToObject(const StelObjectP& target, float moveDuration, int zooming)
{
	if (StelApp::getInstance().getScriptMgr().scriptIsRunning())
		moveDuration /= StelApp::getInstance().getScriptMgr().getScriptRate();

	zoomingMode = zooming;
	move.aim=Vec3d(0);
	move.start=viewDirectionJ2000;
	move.start.normalize();
	move.speed=1.f/(moveDuration*1000);
	move.coef=0.;
	move.targetObject = target;
	flagAutoMove = true;
}

Vec3d StelMovementMgr::j2000ToMountFrame(const Vec3d& v) const
{
	switch (mountMode)
	{
		case MountAltAzimuthal:
			return core->getNavigator()->j2000ToAltAz(v);
		case MountEquinoxEquatorial:
			return core->getNavigator()->j2000ToEquinoxEqu(v);
		case MountGalactic:
			return core->getNavigator()->j2000ToGalactic(v);
	}
	Q_ASSERT(0);
	return Vec3d(0);
}

Vec3d StelMovementMgr::mountFrameToJ2000(const Vec3d& v) const
{
	switch (mountMode)
	{
		case MountAltAzimuthal:
			return core->getNavigator()->altAzToJ2000(v);
		case MountEquinoxEquatorial:
			return core->getNavigator()->equinoxEquToJ2000(v);
		case MountGalactic:
			return core->getNavigator()->galacticToJ2000(v);
	}
	Q_ASSERT(0);
	return Vec3d(0);
}

void StelMovementMgr::setViewDirectionJ2000(const Vec3d& v)
{
	core->getNavigator()->lookAtJ2000(v, getViewUpVectorJ2000());
	viewDirectionJ2000 = v;
	viewDirectionMountFrame = j2000ToMountFrame(v);
}

void StelMovementMgr::panView(double deltaAz, double deltaAlt)
{
	double azVision, altVision;
	StelUtils::rectToSphe(&azVision,&altVision,j2000ToMountFrame(viewDirectionJ2000));

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
	}
}


//! Make the first screen position correspond to the second (useful for mouse dragging)
void StelMovementMgr::dragView(int x1, int y1, int x2, int y2)
{
	Vec3d tempvec1, tempvec2;
	double az1, alt1, az2, alt2;
	const StelProjectorP prj = core->getProjection(StelCore::FrameJ2000);
	prj->unProject(x2,y2, tempvec2);
	prj->unProject(x1,y1, tempvec1);
	StelUtils::rectToSphe(&az1, &alt1, j2000ToMountFrame(tempvec1));
	StelUtils::rectToSphe(&az2, &alt2, j2000ToMountFrame(tempvec2));
	panView(az2-az1, alt1-alt2);
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

		if( zoomMove.start > zoomMove.aim )
		{
			// slow down as approach final view
			c = 1 - (1-zoomMove.coef)*(1-zoomMove.coef)*(1-zoomMove.coef);
		}
		else
		{
			// speed up as leave zoom target
			c = (zoomMove.coef)*(zoomMove.coef)*(zoomMove.coef);
		}

		setFov(zoomMove.start + (zoomMove.aim - zoomMove.start) * c);
		zoomMove.coef+=zoomMove.speed*deltaTime*1000;
		if (zoomMove.coef>=1.)
		{
			flagAutoZoom = 0;
			setFov(zoomMove.aim);
		}
	}
}

// Zoom to the given field of view
void StelMovementMgr::zoomTo(double aim_fov, float moveDuration)
{
	if (StelApp::getInstance().getScriptMgr().scriptIsRunning())
		moveDuration /= StelApp::getInstance().getScriptMgr().getScriptRate();

	zoomMove.aim=aim_fov;
	zoomMove.start=currentFov;
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
	return (flagAutoZoom ? zoomMove.aim : currentFov);
}

void StelMovementMgr::setMaxFov(double max)
{
	maxFov = max;
	if (currentFov > max)
	{
		setFov(max);
	}
}
