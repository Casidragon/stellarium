/*
 * Copyright (C) 2009 Timothy Reaves
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

#ifndef OCULAR_HPP_
#define OCULAR_HPP_

#include <QDebug>
#include <QObject>
#include <QString>
#include <QSettings>

class Telescope;

class Ocular : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString name READ name WRITE setName)
	Q_PROPERTY(double appearentFOV READ appearentFOV WRITE setAppearentFOV)
	Q_PROPERTY(double effectiveFocalLength READ effectiveFocalLength WRITE setEffectiveFocalLength)
	Q_PROPERTY(double fieldStop READ fieldStop WRITE setFieldStop)
public:
	Ocular();
	Q_INVOKABLE Ocular(const QObject& other);
	virtual ~Ocular();
	static Ocular* ocularFromSettings(QSettings* theSettings, int ocularIndex);
	static Ocular* ocularModel();

	const QString name() const;
	void setName(QString aName);
	double appearentFOV() const;
	void setAppearentFOV(double fov);
	double effectiveFocalLength() const;
	void setEffectiveFocalLength(double fl);
	double fieldStop() const;
	void setFieldStop(double fs);

	double actualFOV(Telescope *telescope) const;
	double magnification(Telescope *telescope) const;
	QMap<int, QString> propertyMap();

private:
	QString m_name;
	double m_appearentFOV;
	double m_effectiveFocalLength;
	double m_fieldStop;
};


#endif /* OCULAR_HPP_ */
