/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Bokovoy <alexander.bokovoy@nokia.com>
**
** This file is part of the Quill Metadata package.
**
** Commercial Usage
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
**
****************************************************************************/

#include <QStringList>
#include <QLocale>
#include <QTextStream>
#include <exempi-2.0/exempi/xmpconsts.h>
#include <math.h>

#include "xmp.h"
#include "quillmetadataregions.h"

#include <QDebug>

QHash<QuillMetadata::Tag,XmpTag> Xmp::m_xmpTags;
QHash<Xmp::Tag,XmpRegionTag> Xmp::m_regionXmpTags;

bool Xmp::m_initialized = false;

XmpTag::XmpTag(const QString &schema, const QString &tag, TagType tagType) :
	schema(schema), tag(tag), tagType(tagType)
{
}

XmpTag::XmpTag() :
	schema(""), tag(""), tagType(TagTypeString)
{
}


XmpRegionTag::XmpRegionTag(const QString &schema, const QString &baseTag,
			   const QString &tag, TagType tagType) :
XmpTag(schema, tag, tagType), baseTag(baseTag)
{
}

XmpRegionTag::XmpRegionTag() :
	XmpTag(), baseTag("")
{
}

QString XmpRegionTag::getIndexedTag(int zeroBasedIndex)
{
    return baseTag + QString("%1").arg(zeroBasedIndex+1) + tag;
}



Xmp::Xmp()
{
    m_xmpPtr = xmp_new_empty();
}

Xmp::Xmp(const QString &fileName)
{
    xmp_init();
    XmpFilePtr xmpFilePtr = xmp_files_open_new(fileName.toAscii().constData(),
                                               XMP_OPEN_READ);
    m_xmpPtr = xmp_files_get_new_xmp(xmpFilePtr);
    xmp_files_close(xmpFilePtr, XMP_CLOSE_NOOPTION);
    xmp_files_free(xmpFilePtr);

    initTags();
}

Xmp::~Xmp()
{
    xmp_free(m_xmpPtr);
}

bool Xmp::isValid() const
{
    return (m_xmpPtr != 0);
}

bool Xmp::supportsEntry(QuillMetadata::Tag tag) const
{
    return m_xmpTags.contains(tag);
}

bool Xmp::hasEntry(QuillMetadata::Tag tag) const
{
    return (supportsEntry(tag) &&
            !m_xmpTags.values(tag).isEmpty());
}

bool Xmp::hasXmpEntry(QuillMetadata::Tag tag) const
{
    QList<XmpTag> xmpTags = m_xmpTags.values(tag);
    if (!xmpTags.isEmpty()) {
	foreach (XmpTag tag, xmpTags) {
	    if (xmp_has_property(m_xmpPtr,
				 tag.schema.toAscii().constData(),
				 tag.tag.toAscii().constData()))
		return true;
	}
    }
    return false;
}

bool Xmp::hasXmpEntry(Xmp::Tag tag, int zeroBasedIndex) const
{
    XmpRegionTag xmpTag = m_regionXmpTags.value(tag);
    if (xmpTag.tag.isEmpty())
	return false;

    if (xmpTag.baseTag.isEmpty())
	return (xmp_has_property(m_xmpPtr,
				 xmpTag.schema.toAscii().constData(),
				 xmpTag.tag.toAscii().constData()));
    else
	return (xmp_has_property(m_xmpPtr,
				 xmpTag.schema.toAscii().constData(),
				 xmpTag.getIndexedTag(zeroBasedIndex).toAscii().constData()));
}

QString Xmp::processXmpString(XmpStringPtr xmpString)
{
    return QString(xmp_string_cstr(xmpString)).trimmed();
}

void Xmp::readRegionListItem(const QString & qPropValue,
			     const QString & qPropName,
			     QuillMetadataRegionBag & regions) const
{
    QString searchString("mwg-rs:RegionList");
    QRegExp rx("(" + searchString + ".)(\\d+).");
    rx.indexIn(qPropName);

    bool isOk = false;
    int nRegionNumber = rx.cap(2).toInt(&isOk);
    // TODO: Sanity check for number of regions

    if (!isOk)  //RegionList.%d. wasn't found
	return;

    searchString.append(
	    "[" + QString::number(nRegionNumber) + "]");

    if (!qPropName.contains(searchString)) //RegionList[nRegionNumber] wasn't found
	return;

    while (regions.size() < nRegionNumber) {
	QuillMetadataRegion region;
	regions.append(region);
    }

    if (qPropName.contains("mwg-rs:Area")) {

	QRectF area = regions[nRegionNumber-1].area();

	if (qPropName.contains("stArea:h")) {
	    QPointF center = area.center();
	    area.setHeight(qPropValue.toFloat());
	    area.moveCenter(center);
	} else if (qPropName.contains("stArea:w")) {
	    QPointF center = area.center();
	    area.setWidth(qPropValue.toFloat());
	    area.moveCenter(center);
	} else if (qPropName.contains("stArea:x")) {
	    area.moveCenter(
		    QPointF(qPropValue.toFloat(), area.center().y()));
	} else if (qPropName.contains("stArea:y")) {
	    area.moveCenter(
		    QPointF(area.center().x(), qPropValue.toFloat()));
	}

	regions[nRegionNumber-1].setArea(area);

    } else if (qPropName.contains("mwg-rs:Name")) {

	regions[nRegionNumber-1].setName(qPropValue);

    } else if (qPropName.contains("mwg-rs:Type")) {

	regions[nRegionNumber-1].setRegionType(qPropValue);

    } else if (qPropName.contains("mwg-rs:Extensions")) {

	;

    }

    return;
}

QVariant Xmp::entry(QuillMetadata::Tag tag) const
{
    if (!supportsEntry(tag))
        return QVariant();

    QList<XmpTag> xmpTags = m_xmpTags.values(tag);

    XmpStringPtr xmpStringPtr = xmp_string_new();

    foreach (XmpTag xmpTag, xmpTags) {
        uint32_t propBits;

	if (xmp_get_property(m_xmpPtr,
			     xmpTag.schema.toAscii().constData(),
                             xmpTag.tag.toAscii().constData(),
                             xmpStringPtr,
                             &propBits)) {

            if (XMP_IS_PROP_ARRAY(propBits)) {
		QStringList list;
                int i = 1;
                while (xmp_get_array_item(m_xmpPtr,
                                          xmpTag.schema.toAscii().constData(),
                                          xmpTag.tag.toAscii().constData(),
                                          i,
                                          xmpStringPtr,
                                          &propBits)) {
                    QString string = processXmpString(xmpStringPtr);
                    if (!string.isEmpty())
                        list << string;
                    i++;
                }

                if (!list.isEmpty()) {
                    xmp_string_free(xmpStringPtr);
                    return QVariant(list);
                }


	    } else if (XMP_IS_PROP_STRUCT(propBits)) {

		XmpIterOptions iterOpts = XMP_ITER_OMITQUALIFIERS;

		XmpIteratorPtr xmpIterPtr = xmp_iterator_new(
			m_xmpPtr, xmpTag.schema.toAscii().constData(),
			xmpTag.tag.toAscii().constData(),
			iterOpts);

		XmpStringPtr schema = xmp_string_new();
		XmpStringPtr propName = xmp_string_new();
		XmpStringPtr propValue = xmp_string_new();

		uint32_t options;
		QuillMetadataRegionBag regions;

		bool bSuccess = xmp_iterator_next(
			xmpIterPtr, schema, propName,
			propValue, &options);

		while (bSuccess)
		{
		    QString qPropValue	= processXmpString(propValue);
		    QString qPropName	= processXmpString(propName);
		    //qDebug() << qPropName << qPropValue;

		    if (qPropName.contains("mwg-rs:AppliedToDimensions")) {

			if (qPropName.contains("stDim:h")) {
			    regions.setFullImageSize(
				    QSize(regions.fullImageSize().width(), qPropValue.toInt()));
			} else if (qPropName.contains("stDim:w")) {
			    regions.setFullImageSize(
				    QSize(qPropValue.toInt(), regions.fullImageSize().height()));
			}

		    }

		    else if (qPropName.contains("mwg-rs:RegionList")) {

			this->readRegionListItem(qPropValue, qPropName, regions);

		    }

		    bSuccess = xmp_iterator_next(
					    xmpIterPtr, schema, propName,
					    propValue, &options);
		} // while (bSuccess)

		xmp_string_free(schema);
		xmp_string_free(propName);
		xmp_string_free(propValue);

		QVariant var;
		var.setValue(regions);
		return var;


	    } else {
                QString string = processXmpString(xmpStringPtr);
                if (!string.isEmpty()) {
                    xmp_string_free(xmpStringPtr);
                    switch (tag) {
                        case QuillMetadata::Tag_GPSLatitude:
                        case QuillMetadata::Tag_GPSLongitude: {
                            // Degrees and minutes are separated with a ','
                            QStringList elements = string.split(",");
                            QLocale c(QLocale::C);
                            double value = 0, term = 0;
                            for (int i = 0, power = 1; i < elements.length(); i ++, power *= 60) {
                                term = c.toDouble(elements[i]);
                                if (i == elements.length() - 1) {
                                    term = c.toDouble(elements[i].mid(0, elements[i].length() - 2));
                                }
                                value += term / power;
                            }

                            return QVariant(value);
                        }

                        case QuillMetadata::Tag_GPSLatitudeRef:
                        case QuillMetadata::Tag_GPSLongitudeRef: {
                            // The 'W', 'E', 'N' or 'S' character is the rightmost character
                            // in the field
                            return QVariant(string.right(1));
                        }

                        case QuillMetadata::Tag_GPSImgDirection:
                        case QuillMetadata::Tag_GPSAltitude: {
                            const int separator = string.indexOf("/");
                            const int len = string.length();
                            QLocale c(QLocale::C);

                            double numerator = c.toDouble(string.mid(0, separator));
                            double denominator = c.toDouble(string.mid(separator + 1, len - separator - 1));

                            if (denominator && separator != -1) {
                                return QVariant(numerator / denominator);
                            }
                            else {
                                return QVariant(numerator);
                            }
                        }

                        default:
                            return QVariant(string);
                    }
                }
            }
	}
    }

    xmp_string_free(xmpStringPtr);
    return QVariant();
}

void Xmp::setEntry(QuillMetadata::Tag tag, const QVariant &entry)
{
    if (!supportsEntry(tag))
        return;

    if (!m_xmpPtr) {
        m_xmpPtr = xmp_new_empty();
    }

    switch (tag) {
        case QuillMetadata::Tag_GPSAltitude: {
            QString altitude = entry.toString();

            // Check the existence of a slash. If there's not one, append it
            if (!altitude.contains("/")) {
                altitude.append("/1");
            }

            // Sanity check: if the input starts with "-", remove it and update the reference field
            if (altitude.startsWith("-")) {
                setXmpEntry(QuillMetadata::Tag_GPSAltitudeRef, QVariant(1));
                setXmpEntry(tag, QVariant(altitude.mid(1)));
            }
            else {
                setXmpEntry(QuillMetadata::Tag_GPSAltitudeRef, QVariant(0));
                setXmpEntry(tag, QVariant(altitude));
            }

            break;
        }
        case QuillMetadata::Tag_GPSLatitude:
        case QuillMetadata::Tag_GPSLongitude: {
            QString value = entry.toString();
            const QString refPositive(tag == QuillMetadata::Tag_GPSLatitude ? "N" : "E");
            const QString refNegative(tag == QuillMetadata::Tag_GPSLatitude ? "S" : "W");

            if (!value.contains(",")) {
                double val = value.toDouble();
                QString parsedEntry;
                val = fabs(val);
                double remains = (val - trunc(val)) * 3600;

                QTextStream(&parsedEntry) << trunc(val) << ","
                        << trunc(remains / 60) << ","
                        << remains - trunc(remains / 60) * 60;

                if (value.startsWith("-")) {
                    parsedEntry.append(refNegative);
                }
                else {
                    parsedEntry.append(refPositive);
                }

                setXmpEntry(tag, QVariant(parsedEntry));
            }
            else if (value.endsWith(refPositive) || value.endsWith(refNegative)) {
                if (value.startsWith("-")) {
                    value.replace(value.right(1), refNegative);
                    setXmpEntry(tag, QVariant(value.mid(1)));
                }
            } else {
                value.append(value.startsWith("-") ? refNegative : refPositive);
                setXmpEntry(tag, QVariant(value));
            }

            break;
        }
        case QuillMetadata::Tag_GPSImgDirection: {
            QString direction;
            const int slash = entry.toString().indexOf("/");
            double value;

            // Check the existence of a slash. If there's not one, append it
            if (slash == -1) {
                value = entry.toDouble();
                value = fmod(value, 360);
                if (value < 0) {
                    value = 360 + value;
                }
                QTextStream(&direction) << value << "/1";
            }
            else {
                QLocale c(QLocale::C);
                value = c.toDouble(direction.mid(0, slash)) / c.toDouble(direction.mid(slash + 1, direction.length() - slash - 1));
                value = fmod(value, 360);
                if (value < 0) {
                    value = 360 + value;
                }
                QTextStream(&direction) << value << "/1";
            }

            setXmpEntry(tag, QVariant(direction));
            break;
        }
	case QuillMetadata::Tag_Regions: {
	    QuillMetadataRegionBag regions = entry.value<QuillMetadataRegionBag>();

	    {
		XmpTag xmpTag = m_xmpTags.value(QuillMetadata::Tag_Regions);
		if (regions.count() == 0) { // No regions to be written: delete all
		    removeEntry(QuillMetadata::Tag_Regions);
		    break;
		}

		if (regions.count() > 0) { // Regions to be written: create if needed
		    if (!hasXmpEntry(QuillMetadata::Tag_Regions)) {
			setXmpEntry(QuillMetadata::Tag_Regions, QVariant(""));

			XmpRegionTag xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionList);
			setXmpEntry(XmpTag(xmpTag.schema,
					   xmpTag.tag,
					   xmpTag.tagType), "");
		    }
		}


		// Write all tags of all regions.
		// RegionAppliedToDimensionsH
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionAppliedToDimensionsH);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.tag, xmpTag.tagType),
			    regions.fullImageSize().height());

		// RegionAppliedToDimensionsW
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionAppliedToDimensionsW);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.tag, xmpTag.tagType),
			    regions.fullImageSize().width());
	    }

	    XmpRegionTag xmpTag;
	    int nRegion;

	    for (nRegion = 0; nRegion < regions.count(); nRegion++) {

		// Region name
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionName);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.getIndexedTag(nRegion), xmpTag.tagType),
			    regions[nRegion].name());

		// Region type
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionType);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.getIndexedTag(nRegion), xmpTag.tagType),
			    regions[nRegion].regionType());

		// RegionAreaX,
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionAreaX);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.getIndexedTag(nRegion), xmpTag.tagType),
			    regions[nRegion].area().center().x());

		// RegionAreaY
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionAreaY);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.getIndexedTag(nRegion), xmpTag.tagType),
			    regions[nRegion].area().center().y());

		// RegionAreaH
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionAreaH);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.getIndexedTag(nRegion), xmpTag.tagType),
			    regions[nRegion].area().height());

		// RegionAreaW
		xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionAreaW);
		setXmpEntry(XmpTag(xmpTag.schema, xmpTag.getIndexedTag(nRegion), xmpTag.tagType),
			    regions[nRegion].area().width());
	    }

	    // Delete regions that aren't valid anymore
	    xmpTag = m_regionXmpTags.value(Xmp::Tag_RegionListItem);
	    while (hasXmpEntry(Xmp::Tag_RegionListItem, nRegion)) {
		xmp_delete_property(m_xmpPtr, xmpTag.schema.toAscii().constData(),
				    xmpTag.getIndexedTag(nRegion).toAscii().constData());
		nRegion++;
	    }

	    break;
	}
    default:
        setXmpEntry(tag, entry);
        break;
    }
}

void Xmp::setXmpEntry(QuillMetadata::Tag tag, const QVariant &entry)
{
    QList<XmpTag> xmpTags = m_xmpTags.values(tag);
    foreach (XmpTag xmpTag, xmpTags) {
	setXmpEntry(xmpTag, entry);
    }
}

void Xmp::setXmpEntry(Xmp::Tag tag, const QVariant &entry)
{
    XmpTag xmpTag = m_regionXmpTags.value(tag);
    setXmpEntry(xmpTag, entry);
}

void Xmp::setXmpEntry(XmpTag xmpTag, const QVariant &entry)
{
    xmp_delete_property(m_xmpPtr,
			xmpTag.schema.toAscii().constData(),
			xmpTag.tag.toAscii().constData());

    if (xmpTag.tagType == XmpTag::TagTypeString){
	xmp_set_property(m_xmpPtr,
			 xmpTag.schema.toAscii().constData(),
			 xmpTag.tag.toAscii().constData(),
			 entry.toString().toUtf8().constData(), 0);
    }
    else if (xmpTag.tagType == XmpTag::TagTypeStruct){
	xmp_set_property(m_xmpPtr,
			 xmpTag.schema.toAscii().constData(),
			 xmpTag.tag.toAscii().constData(),
			 entry.toString().toUtf8().constData(), XMP_PROP_VALUE_IS_STRUCT);
    }
    else if (xmpTag.tagType == XmpTag::TagTypeArray){
	xmp_set_property(m_xmpPtr,
			 xmpTag.schema.toAscii().constData(),
			 xmpTag.tag.toAscii().constData(),
			 entry.toString().toUtf8().constData(), XMP_PROP_VALUE_IS_ARRAY);
    }
    else if (xmpTag.tagType == XmpTag::TagTypeStringList) {
	QStringList list = entry.toStringList();
	foreach (QString string, list)
	    xmp_append_array_item(m_xmpPtr,
				  xmpTag.schema.toAscii().constData(),
				  xmpTag.tag.toAscii().constData(),
				  XMP_PROP_ARRAY_IS_UNORDERED,
				  string.toUtf8().constData(), 0);
    }
    else if (xmpTag.tagType == XmpTag::TagTypeAltLang) {
	xmp_set_localized_text(m_xmpPtr,
			       xmpTag.schema.toAscii().constData(),
			       xmpTag.tag.toAscii().constData(),
			       "", "x-default",
			       entry.toString().toUtf8().constData(), 0);
    }
    else if (xmpTag.tagType == XmpTag::TagTypeReal) {
	xmp_set_property_float(m_xmpPtr,
			       xmpTag.schema.toAscii().constData(),
			       xmpTag.tag.toAscii().constData(),
			       entry.toReal(), 0);
    }
    else if (xmpTag.tagType == XmpTag::TagTypeInteger) {
	xmp_set_property_int32(m_xmpPtr,
			       xmpTag.schema.toAscii().constData(),
			       xmpTag.tag.toAscii().constData(),
			       entry.toInt(), 0);
    }
}

void Xmp::removeEntry(QuillMetadata::Tag tag)
{
    if (!supportsEntry(tag))
        return;

    if (!m_xmpPtr)
        return;

    QList<XmpTag> xmpTags = m_xmpTags.values(tag);

    foreach (XmpTag xmpTag, xmpTags) {
        xmp_delete_property(m_xmpPtr,
                            xmpTag.schema.toAscii().constData(),
                            xmpTag.tag.toAscii().constData());
    }
}


bool Xmp::write(const QString &fileName) const
{
    XmpPtr ptr = m_xmpPtr;

    if (!ptr)
        ptr = xmp_new_empty();

    XmpFilePtr xmpFilePtr = xmp_files_open_new(fileName.toAscii().constData(),
                                               XMP_OPEN_FORUPDATE);
    bool result;

    if (xmp_files_can_put_xmp(xmpFilePtr, ptr))
        result = xmp_files_put_xmp(xmpFilePtr, ptr);
    else
        result = false;

    // Crash safety can be ignored here by selecting Nooption since
    // QuillFile already has crash safety measures.
    if (result)
        result = xmp_files_close(xmpFilePtr, XMP_CLOSE_NOOPTION);
    xmp_files_free(xmpFilePtr);

    if (!m_xmpPtr)
        xmp_free(ptr);

    return result;
}

void Xmp::initTags()
{
    if (m_initialized)
        return;

    m_initialized = true;

    m_xmpTags.insertMulti(QuillMetadata::Tag_Creator,
                          XmpTag(NS_DC, "creator", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Subject,
                          XmpTag(NS_DC, "subject", XmpTag::TagTypeStringList));
    m_xmpTags.insertMulti(QuillMetadata::Tag_City,
                          XmpTag(NS_PHOTOSHOP, "City", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Country,
                          XmpTag(NS_PHOTOSHOP, "Country", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_City,
                          XmpTag(NS_IPTC4XMP, "LocationShownCity", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Country,
                          XmpTag(NS_IPTC4XMP, "LocationShownCountry", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Location,
                          XmpTag(NS_IPTC4XMP, "LocationShownSublocation", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Rating,
                          XmpTag(NS_XAP, "Rating", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Timestamp,
                          XmpTag(NS_XAP, "MetadataDate", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Description,
                          XmpTag(NS_DC, "description", XmpTag::TagTypeAltLang));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Orientation,
                          XmpTag(NS_EXIF, "Orientation", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Orientation,
                          XmpTag(NS_TIFF, "Orientation", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_Title,
                          XmpTag(NS_DC, "title", XmpTag::TagTypeAltLang));

    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSLatitude,
                          XmpTag(NS_EXIF, "GPSLatitude", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSLongitude,
                          XmpTag(NS_EXIF, "GPSLongitude", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSAltitude,
                          XmpTag(NS_EXIF, "GPSAltitude", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSAltitudeRef,
                          XmpTag(NS_EXIF, "GPSAltitudeRef", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSImgDirection,
                          XmpTag(NS_EXIF, "GPSImgDirection", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSImgDirectionRef,
                          XmpTag(NS_EXIF, "GPSImgDirectionRef", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSVersionID,
                          XmpTag(NS_EXIF, "GPSVersionID", XmpTag::TagTypeString));

    // Workaround for missing reference tags: we'll extract them from the
    // Latitude and Longitude tags
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSLatitudeRef,
                              XmpTag(NS_EXIF, "GPSLatitude", XmpTag::TagTypeString));
    m_xmpTags.insertMulti(QuillMetadata::Tag_GPSLongitudeRef,
                              XmpTag(NS_EXIF, "GPSLongitude", XmpTag::TagTypeString));

    m_xmpTags.insertMulti(QuillMetadata::Tag_Regions,
			  XmpTag("http://www.metadataworkinggroup.com/schemas/regions/",
				 "mwg-rs:Regions", XmpTag::TagTypeStruct));

    /***/

    m_regionXmpTags.insert(Xmp::Tag_RegionAppliedToDimensions,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"", "mwg-rs:Regions/mwg-rs:AppliedToDimensions",
					XmpTag::TagTypeStruct));

    m_regionXmpTags.insert(Xmp::Tag_RegionAppliedToDimensionsH,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"", "mwg-rs:Regions/mwg-rs:AppliedToDimensions/stDim:h",
					XmpTag::TagTypeInteger));

    m_regionXmpTags.insert(Xmp::Tag_RegionAppliedToDimensionsW,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"", "mwg-rs:Regions/mwg-rs:AppliedToDimensions/stDim:w",
					XmpTag::TagTypeInteger));

    m_regionXmpTags.insert(Xmp::Tag_RegionList,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"", "mwg-rs:Regions/mwg-rs:RegionList",
					XmpTag::TagTypeArray));

    m_regionXmpTags.insert(Xmp::Tag_RegionListItem,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]",
					XmpTag::TagTypeStruct));

    m_regionXmpTags.insert(Xmp::Tag_RegionName,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]/mwg-rs:Name",
					XmpTag::TagTypeString));

    m_regionXmpTags.insert(Xmp::Tag_RegionType,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]/mwg-rs:Type",
					XmpTag::TagTypeString));

    m_regionXmpTags.insert(Xmp::Tag_RegionAreaH,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]/mwg-rs:Area/stArea:h",
					XmpTag::TagTypeReal));

    m_regionXmpTags.insert(Xmp::Tag_RegionAreaW,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]/mwg-rs:Area/stArea:w",
					XmpTag::TagTypeReal));

    m_regionXmpTags.insert(Xmp::Tag_RegionAreaX,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]/mwg-rs:Area/stArea:x",
					XmpTag::TagTypeReal));

    m_regionXmpTags.insert(Xmp::Tag_RegionAreaY,
			   XmpRegionTag("http://www.metadataworkinggroup.com/schemas/regions/",
					"mwg-rs:Regions/mwg-rs:RegionList[", "]/mwg-rs:Area/stArea:y",
					XmpTag::TagTypeReal));
}
