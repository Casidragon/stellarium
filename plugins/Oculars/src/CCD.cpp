/*
 * portions Copyright (C) 2010 Bernhard Reutner-Fischer
 * portions Copyright (C) 2010 Timothy Reaves
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

#include "CCD.hpp"
#include "Ocular.hpp"

#include <QDebug>
#include <QSettings>

CCD::CCD()
{
}

CCD::CCD(const QObject& other)
{
	Q_ASSERT(other);
	Q_ASSERT(other->metaObject()->className() == "CCD");
	this->m_name = other.property("name").toString();
	this->m_chipHeight = other.property("chipHeight").toFloat();
	this->m_chipWidth = other.property("chipWidth").toFloat();
	this->m_pixelHeight = other.property("pixelHeight").toFloat();
	this->m_pixelWidth = other.property("pixelWidth").toFloat();
	this->m_resolutionX = other.property("resolutionX").toInt();
	this->m_resolutionY = other.property("resolutionY").toInt();
}

CCD::~CCD()
{
}


static QMap<int, QString> mapping;

QMap<int, QString> CCD::propertyMap()
{
	if(mapping.isEmpty()) {
		mapping = QMap<int, QString>();
		mapping[0] = "name";
		mapping[1] = "chipHeight";
		mapping[2] = "chipWidth";
		mapping[3] = "pixelHeight";
		mapping[4] = "pixelWidth";
		mapping[5] = "resolutionX";
		mapping[6] = "resolutionY";
	}
	return mapping;
}


/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark Instance Methods
#endif
/* ********************************************************************* */
QString CCD::name() const
{
	return m_name;
}

void CCD::setName(QString name)
{
	m_name = name;
}

int CCD::resolutionX()  const 
{
	return m_resolutionX;
}

void CCD::setResolutionX(int resolution)
{
	m_resolutionX = resolution;
}

int CCD::resolutionY()  const 
{
	return m_resolutionY;
}

void CCD::setResolutionY(int resolution)
{
	m_resolutionY = resolution;
}

float CCD::chipWidth()  const 
{
	return m_chipWidth;
}

void CCD::setChipWidth(float width)
{
	m_chipWidth = width;
}

float CCD::chipHeight()  const 
{
	return m_chipHeight;
}

void CCD::setChipHeight(float height)
{
	m_chipHeight = height;
}

float CCD::pixelWidth()  const 
{
	return m_pixelWidth;
}

void CCD::setPixelWidth(float width)
{
	m_pixelWidth = width;
}

float CCD::pixelHeight()  const 
{
	return m_pixelHeight;
}

void CCD::setPixelHeight(float height)
{
	m_pixelHeight = height;
}

float CCD::getActualFOVx(Ocular *ocular) const
{
	float FOVx = (m_chipWidth * 206.265) / ocular->effectiveFocalLength();
	return FOVx;
}

float CCD::getActualFOVy(Ocular *ocular) const
{
	float FOVy = (m_chipHeight * 206.265) / ocular->effectiveFocalLength();
	return FOVy;
}

/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark Static Methods
#endif
/* ********************************************************************* */
CCD* CCD::ccdFromSettings(QSettings* theSettings, int ccdIndex)
{
	CCD* ccd = new CCD();
	QString prefix = "ccd/" + QVariant(ccdIndex).toString() + "/";
	ccd->setName(theSettings->value(prefix + "name", "").toString());
	ccd->setResolutionX(theSettings->value(prefix + "resolutionX", "0").toInt());
	ccd->setResolutionY(theSettings->value(prefix + "resolutionY", "0").toInt());
	ccd->setChipWidth(theSettings->value(prefix + "chip_width", "0.0").toFloat());
	ccd->setChipHeight(theSettings->value(prefix + "chip_height", "0.0").toFloat());
	ccd->setPixelWidth(theSettings->value(prefix + "pixel_width", "0.0").toFloat());
	ccd->setPixelHeight(theSettings->value(prefix + "pixel_height", "0.0").toFloat());
	
	return ccd;
}

CCD* CCD::ccdModel()
{
	CCD* model = new CCD();
	model->setName("My CCD");
	model->setChipHeight(36.8);
	model->setChipWidth(36.8);
	model->setPixelHeight(9);
	model->setPixelWidth(9);
	model->setResolutionX(4096);
	model->setResolutionY(4096);
	return model;
}
