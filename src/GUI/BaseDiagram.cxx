// BaseDiagram.cxx - part of GUI launcher using Qt5
//
// Written by James Turner, started December 2014.
//
// Copyright (C) 2014 James Turner <zakalawe@mac.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "BaseDiagram.hxx"

#include <limits>

#include <QDebug>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QPainter>
#include <QQmlEngine>
#include <QVector2D>

#include <Navaids/navrecord.hxx>
#include <Navaids/positioned.hxx>
#include <Airports/airport.hxx>
#include <Navaids/PolyLine.hxx>
#include <Navaids/NavDataCache.hxx>
#include <Navaids/airways.hxx>

#include "QmlColoredImageProvider.hxx"
#include "QtLauncher_fwd.hxx"

using namespace flightgear;

/* equatorial and polar earth radius */
const double rec  = 6378137;          // earth radius, equator (?)
const double rpol = 6356752.314;      // earth radius, polar   (?)

const double MINIMUM_SCALE = 0.002;
const double MAXIMUM_SCALE = 2.0;

//Returns Earth radius at a given latitude (Ellipsoide equation with two equal axis)
static double earth_radius_lat( double lat )
{
    double a = cos(lat)/rec;
    double b = sin(lat)/rpol;
    return 1.0 / sqrt( a * a + b * b );
}

BaseDiagram::BaseDiagram(QQuickItem* pr) :
    QQuickPaintedItem(pr),
    m_autoScalePan(true),
    m_wheelAngleDeltaAccumulator(0)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setFlag(ItemHasContents);
    setOpaquePainting(true);
    setAntialiasing(true);
}

QTransform BaseDiagram::transform() const
{
    QTransform t;
    t.translate(width() / 2, height() / 2); // center projection origin in the widget
    t.scale(m_scale, m_scale);

    // apply any pan offset that exists
    t.translate(m_panOffset.x(), m_panOffset.y());
    // center the bounding box (may not be at the origin)
    t.translate(-m_bounds.center().x(), -m_bounds.center().y());
    return t;
}

void BaseDiagram::clearIgnoredNavaids()
{
    m_ignored.clear();
}

void BaseDiagram::addIgnoredNavaid(FGPositionedRef pos)
{
    if (isNavaidIgnored(pos))
        return;
    m_ignored.push_back(pos);
}

void BaseDiagram::extendRect(QRectF &r, const QPointF &p)
{
    if (p.x() < r.left()) {
        r.setLeft(p.x());
    } else if (p.x() > r.right()) {
        r.setRight(p.x());
    }

    if (p.y() < r.top()) {
        r.setTop(p.y());
    } else if (p.y() > r.bottom()) {
        r.setBottom(p.y());
    }
}

QRect BaseDiagram::rect() const
{
    return QRect(0, 0,
                 static_cast<int>(width()),
                 static_cast<int>(height()));
}

void BaseDiagram::paint(QPainter* p)
{
    //p->setRenderHints(QPainter::Antialiasing);
    p->fillRect(rect(), QColor(0x3f, 0x3f, 0x3f));

    if (m_autoScalePan) {
        // fit bounds within our available space, allowing for a margin
        const int MARGIN = 32; // pixels
        double ratioInX = (width() - MARGIN * 2) / m_bounds.width();
        double ratioInY = (height() - MARGIN * 2) / m_bounds.height();
        m_scale = std::min(ratioInX, ratioInY);
        SG_CLAMP_RANGE(m_scale, MINIMUM_SCALE, MAXIMUM_SCALE);
    }

    m_baseDeviceTransform = p->deviceTransform();
    m_viewportTransform = transform();
    p->setWorldTransform(m_viewportTransform * m_baseDeviceTransform);
    m_projectedPositions.clear();

    paintPolygonData(p);
    paintNavaids(p);
    paintContents(p);
}

void BaseDiagram::paintAirplaneIcon(QPainter* painter, const SGGeod& geod, int headingDeg)
{
    QPointF pos = project(geod);
    QPixmap pix(":/airplane-icon");
    pos = m_viewportTransform.map(pos);
    painter->save();
    painter->setWorldTransform(m_baseDeviceTransform);

    painter->translate(pos.x(), pos.y());
    painter->rotate(headingDeg);

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    QRect airplaneIconRect = pix.rect();
    airplaneIconRect.moveCenter(QPoint(0,0));
    painter->drawPixmap(airplaneIconRect, pix);

    painter->restore();
}

void BaseDiagram::paintCarrierIcon(QPainter* painter, const SGGeod& geod, int headingDeg)
{
    if (m_carrierPixmap.isNull()) {
        auto iconProvider = QmlColoredImageProvider::instance();
        QSize sz;
        m_carrierPixmap = iconProvider->requestPixmap("aircraft-carrier?#ff00ff", &sz, {});
    }

    QPointF pos = project(geod);
    pos = m_viewportTransform.map(pos);
    painter->save();
    painter->setWorldTransform(m_baseDeviceTransform);

    painter->translate(pos.x(), pos.y());
    painter->rotate(headingDeg);

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    QRect carrierIconRect = m_carrierPixmap.rect();
    carrierIconRect.moveCenter(QPoint(0,0));
    painter->drawPixmap(carrierIconRect, m_carrierPixmap);

    painter->restore();
}

void BaseDiagram::paintPolygonData(QPainter* painter)
{
    QTransform invT = m_viewportTransform.inverted();
    const auto geom = rect();
    SGGeod topLeft = unproject(invT.map(geom.topLeft()), m_projectionCenter);
    SGGeod viewCenter = unproject(invT.map(geom.center()), m_projectionCenter);
    SGGeod bottomRight = unproject(invT.map(geom.bottomRight()), m_projectionCenter);

    double drawRangeNm = std::max(SGGeodesy::distanceNm(viewCenter, topLeft),
                                  SGGeodesy::distanceNm(viewCenter, bottomRight));

    flightgear::PolyLineList lines(flightgear::PolyLine::linesNearPos(viewCenter, drawRangeNm,
                                                                      flightgear::PolyLine::COASTLINE));

    QPen waterPen(QColor(64, 64, 255), 1);
    waterPen.setCosmetic(true);
    painter->setPen(waterPen);
    for (auto line : lines) {
        paintGeodVec(painter, line->points());
    }

    lines = flightgear::PolyLine::linesNearPos(viewCenter, drawRangeNm,
                                              flightgear::PolyLine::URBAN);
    for (auto line : lines) {
        fillClosedGeodVec(painter, QColor(192, 192, 96), line->points());
    }

    lines = flightgear::PolyLine::linesNearPos(viewCenter, drawRangeNm,
                                              flightgear::PolyLine::RIVER);

    painter->setPen(waterPen);
    for (auto line : lines) {
        paintGeodVec(painter, line->points());
    }


    lines = flightgear::PolyLine::linesNearPos(viewCenter, drawRangeNm,
                                              flightgear::PolyLine::LAKE);

    for (auto line : lines) {
        fillClosedGeodVec(painter, QColor(128, 128, 255), line->points());
    }
}

void BaseDiagram::paintGeodVec(QPainter* painter, const flightgear::SGGeodVec& vec)
{
    QVector<QPointF> projected;
    projected.reserve(vec.size());
    for (auto v: vec) {
        projected.append(project(v));
    }

    painter->drawPolyline(projected.data(), projected.size());
}

void BaseDiagram::fillClosedGeodVec(QPainter* painter, const QColor& color, const flightgear::SGGeodVec& vec)
{
    QVector<QPointF> projected;
    projected.reserve(vec.size());
    flightgear::SGGeodVec::const_iterator it;
    for (it=vec.begin(); it != vec.end(); ++it) {
        projected.append(project(*it));
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(projected.data(), projected.size());
}

class MapFilter : public FGPositioned::TypeFilter
{
public:

    MapFilter(LauncherController::AircraftType aircraft)
    {
        addType(FGPositioned::FIX);
        addType(FGPositioned::NDB);
        addType(FGPositioned::VOR);

        // this doesn't work, since the cache does not actually contain TACAN entries
        // addType(FGPositioned::TACAN);
        // instead we look for DMEs and filter on the name
        addType(FGPositioned::DME);

        if (aircraft == LauncherController::Helicopter) {
            addType(FGPositioned::HELIPAD);
        }

        if (aircraft == LauncherController::Seaplane) {
            addType(FGPositioned::SEAPORT);
        }

        // always add regular aiports, since they may contain helipads,
        // and seaplanes may have wheels on their floats
        addType(FGPositioned::AIRPORT);
    }

    bool pass(FGPositioned* aPos) const override
    {
        bool ok = TypeFilter::pass(aPos);

        const auto ty = aPos->type();
        if (ok && (ty == FGPositioned::FIX)) {
            // ignore fixes which end in digits
            if (aPos->ident().length() > 4 && isdigit(aPos->ident()[3]) && isdigit(aPos->ident()[4])) {
                return false;
            }
        }

        if (ok && (ty == FGPositioned::DME)) {
            if (!simgear::strutils::ends_with(aPos->name(), "TACAN")) {
                return false;
            }
        }

        return ok;
    }
};

void BaseDiagram::splitItems(const FGPositionedList& in, FGPositionedList& navaids,
                             FGPositionedList& ports)
{
    for (auto p : in) {
        if (FGPositioned::isAirportType(p)) {
            ports.push_back(p);
        } else {
            navaids.push_back(p);
        }
    }
}

bool orderAirportsByRunwayLength(const FGPositionedRef& a,
                                 const FGPositionedRef& b)
{
    FGAirport* aptA = static_cast<FGAirport*>(a.ptr());
    FGAirport* aptB = static_cast<FGAirport*>(b.ptr());

    return aptA->longestRunway()->lengthFt() > aptB->longestRunway()->lengthFt();
}

void BaseDiagram::paintNavaids(QPainter* painter)
{
    painter->save();
    QTransform invT = m_viewportTransform.inverted();

    SGGeod topLeft = unproject(invT.map(rect().topLeft()), m_projectionCenter);
    SGGeod viewCenter = unproject(invT.map(rect().center()), m_projectionCenter);
    SGGeod bottomRight = unproject(invT.map(rect().bottomRight()), m_projectionCenter);

    double drawRangeNm = std::max(SGGeodesy::distanceNm(viewCenter, topLeft),
                                  SGGeodesy::distanceNm(viewCenter, bottomRight));

    MapFilter f(m_aircraftType);
    FGPositionedList items = FGPositioned::findWithinRange(viewCenter, drawRangeNm, &f);

    FGPositionedList navaids, ports;
    splitItems(items, navaids, ports);

    if (ports.size() >= 40) {
        FGPositionedList::iterator middle = ports.begin() + 40;
        std::partial_sort(ports.begin(), middle, ports.end(),
                          orderAirportsByRunwayLength);
        ports.resize(40);
    }

    // paint airways underneath
    paintAirways(painter, navaids);

    m_labelRects.clear();
    m_labelRects.reserve(items.size());
    painter->setTransform(m_baseDeviceTransform);

    for (auto port : ports) {
        paintNavaid(painter, port);
    }

    for (auto nav : navaids) {
        paintNavaid(painter, nav);
    }

    painter->restore();
}

void BaseDiagram::paintAirways(QPainter* painter, const FGPositionedList& navs)
{
    // FIXME: specify the network

    struct AirwaySegment
    {
        AirwaySegment(PositionedID t, PositionedID f, int a) :
            to(t), from(f), airway(a)
        {}

        PositionedID to, from;
        int airway;
    };

    std::vector<AirwaySegment> segmentsToDraw;

    for (auto n : navs) {
        const auto navGuid = n->guid();
        AirwayEdgeVec edges = NavDataCache::instance()->airwayEdgesFrom(Airway::HighLevel, n->guid());

        for (auto e : edges) {
            const auto edgeEndGuid = e.second;
            auto it = std::find_if(segmentsToDraw.begin(), segmentsToDraw.end(),
                                   [edgeEndGuid, navGuid](const AirwaySegment& segment)
            {
                return ((segment.to == edgeEndGuid) && (segment.from == navGuid)) ||
                        ((segment.from == edgeEndGuid) && (segment.to == navGuid));
            });
            if (it == segmentsToDraw.end()) {
                segmentsToDraw.emplace_back(navGuid, edgeEndGuid, e.first);
            }
        }
    }

    QVector<QLineF> lines;
    lines.reserve(segmentsToDraw.size());

    for (auto seg : segmentsToDraw) {
        QPointF p1 = projectedPosition(seg.from);
        QPointF p2 = projectedPosition(seg.to);

        const auto d = QVector2D(p2 - p1).normalized();
        p1 += (d.toPointF() * 100);
        p2 += (d.toPointF() * -100);

        lines.append(QLineF(p1, p2));
    }

    QPen linePen(Qt::cyan, 1);
    linePen.setCosmetic(true);
    painter->setPen(linePen);
    painter->drawLines(lines);

    // find airway names

    // draw airway name on top

}

QRect boundsOfLines(const QVector<QLineF>& lines)
{
    QRect r;
    Q_FOREACH(const QLineF& l, lines) {
        r = r.united(QRectF(l.p1(), l.p2()).toRect());
    }

    return r;
}

void BaseDiagram::paintNavaid(QPainter* painter, const FGPositionedRef &pos)
{
    if (isNavaidIgnored(pos))
        return;

    bool drawAsIcon = true;
    const double minRunwayLengthFt = (16 / m_scale) * SG_METER_TO_FEET;
    const FGPositioned::Type ty(pos->type());
    const bool isNDB = (ty == FGPositioned::NDB);
    QRect iconRect;

    if (ty == FGPositioned::AIRPORT) {
        FGAirport* apt = static_cast<FGAirport*>(pos.ptr());
        if (apt->hasHardRunwayOfLengthFt(minRunwayLengthFt)) {

            drawAsIcon = false;
            QVector<QLineF> lines = projectAirportRuwaysWithCenter(apt, m_projectionCenter);

            painter->save();
            painter->setTransform(m_viewportTransform * m_baseDeviceTransform);

            QPen pen(QColor(0x03, 0x83, 0xbf), 8);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLines(lines);

            QPen linePen(Qt::white, 2);
            linePen.setCosmetic(true);
            painter->setPen(linePen);
            painter->drawLines(lines);

            iconRect = m_viewportTransform.mapRect(boundsOfLines(lines));

            painter->restore();
        }
    }

    if (drawAsIcon) {
        QPixmap pm = iconForPositioned(pos);
        QPointF loc = m_viewportTransform.map(projectedPosition(pos));
        const auto sz = pm.size() / pm.devicePixelRatio();
        iconRect = QRect(QPoint(0,0), sz);
        iconRect.moveCenter(loc.toPoint());
        painter->drawPixmap(iconRect, pm);
    }

   // compute label text so we can measure it
    QString label;
    if (FGPositioned::isAirportType(pos.ptr())) {
        label = QString::fromStdString(pos->name());
        label = fixNavaidName(label);
    } else {
        label = QString::fromStdString(pos->ident());
    }

    if (ty == FGPositioned::NDB) {
        FGNavRecord* nav = static_cast<FGNavRecord*>(pos.ptr());
        label.append("\n").append(QString::number(nav->get_freq() / 100));
    } else if (ty == FGPositioned::VOR) {
        FGNavRecord* nav = static_cast<FGNavRecord*>(pos.ptr());
        label.append("\n").append(QString::number(nav->get_freq() / 100.0, 'f', 1));
    }

    QRect textBounds = painter->boundingRect(QRect(0, 0, 100, 100),
                                             Qt::TextWordWrap, label);
    int textFlags;
    textBounds = rectAndFlagsForLabel(pos->guid(), iconRect,
                                      textBounds.size(),
                                      textFlags);

    painter->setPen(isNDB ? QColor(0x9b, 0x5d, 0xa2) : QColor(0x03, 0x83, 0xbf));
    painter->drawText(textBounds, textFlags, label);
}

bool BaseDiagram::isNavaidIgnored(const FGPositionedRef &pos) const
{
    return m_ignored.contains(pos);
}

bool BaseDiagram::isLabelRectAvailable(const QRect &r) const
{
    Q_FOREACH(const QRect& lr, m_labelRects) {
        if (lr.intersects(r))
            return false;
    }

    return true;
}

int BaseDiagram::textFlagsForLabelPosition(LabelPosition pos)
{
#if 0
    switch (pos) {
    case LABEL_RIGHT:       return Qt::AlignLeft | Qt::AlignVCenter;
    case LABEL_ABOVE:       return Qt::AlignHCenter | Qt::A
    }
#endif
    return 0;
}

QRect BaseDiagram::rectAndFlagsForLabel(PositionedID guid, const QRect& item,
                                        const QSize &bounds,
                                        int& flags) const
{
    m_labelRects.append(item);
    int pos = m_labelPositions.value(guid, LABEL_RIGHT);
    bool firstAttempt = true;
    flags = Qt::TextWordWrap;

    while (pos < LAST_POSITION) {
        QRect r = labelPositioned(item, bounds, static_cast<LabelPosition>(pos));
        if (isLabelRectAvailable(r)) {
            m_labelRects.append(r);
            m_labelPositions[guid] = static_cast<LabelPosition>(pos);
            flags |= textFlagsForLabelPosition(static_cast<LabelPosition>(pos));
            return r;
        } else if (firstAttempt && (pos != LABEL_RIGHT)) {
            pos = LABEL_RIGHT;
        } else {
            ++pos;
        }

        firstAttempt = false;
    }

    return QRect(item.x(), item.y(), bounds.width(), bounds.height());
}

QRect BaseDiagram::labelPositioned(const QRect& itemRect,
                                   const QSize& bounds,
                                   LabelPosition lp) const
{
    const int SHORT_MARGIN = 4;
    const int DIAGONAL_MARGIN = 12;

    QPoint topLeft = itemRect.topLeft();

    switch (lp) {
    // cardinal compass points are short (close in)
    case LABEL_RIGHT:
        topLeft = QPoint(itemRect.right() + SHORT_MARGIN,
                     itemRect.center().y() - bounds.height() / 2);
        break;
    case LABEL_ABOVE:
        topLeft = QPoint(itemRect.center().x() - (bounds.width() / 2),
                     itemRect.top() - (SHORT_MARGIN + bounds.height()));
        break;
    case LABEL_BELOW:
        topLeft = QPoint(itemRect.center().x() - (bounds.width() / 2),
                     itemRect.bottom() + SHORT_MARGIN);
        break;
    case LABEL_LEFT:
        topLeft = QPoint(itemRect.left() - (SHORT_MARGIN + bounds.width()),
                     itemRect.center().y() - bounds.height() / 2);
        break;

    // first diagonals are further out (to hopefully have a better chance
    // of finding clear space

    case LABEL_NE:
        topLeft = QPoint(itemRect.right() + DIAGONAL_MARGIN,
                     itemRect.top() - (DIAGONAL_MARGIN + bounds.height()));
        break;

    case LABEL_NW:
        topLeft = QPoint(itemRect.left() - (DIAGONAL_MARGIN + bounds.width()),
                     itemRect.top() - (DIAGONAL_MARGIN + bounds.height()));
        break;

    case LABEL_SE:
        topLeft = QPoint(itemRect.right() + DIAGONAL_MARGIN,
                     itemRect.bottom() + DIAGONAL_MARGIN);
        break;

    case LABEL_SW:
        topLeft = QPoint(itemRect.left() - (DIAGONAL_MARGIN + bounds.width()),
                     itemRect.bottom() + DIAGONAL_MARGIN);
        break;
    default:
        qWarning() << Q_FUNC_INFO << "Implement me";

    }

    return QRect(topLeft, bounds);
}

void BaseDiagram::mousePressEvent(QMouseEvent *me)
{
    if (!hasActiveFocus()) {
        forceActiveFocus(Qt::MouseFocusReason);
    }

    m_lastMousePos = me->pos();
    m_didPan = false;
    me->accept();
}

void BaseDiagram::mouseMoveEvent(QMouseEvent *me)
{
    m_autoScalePan = false;

    QPointF delta = me->pos() - m_lastMousePos;
    m_lastMousePos = me->pos();

    // offset is stored in metres so we don't have to modify it when
    // zooming
    m_panOffset += (delta / m_scale);
    m_didPan = true;

    update();
}

int intSign(int v)
{
    return (v == 0) ? 0 : (v < 0) ? -1 : 1;
}

void BaseDiagram::wheelEvent(QWheelEvent *we)
{
    m_autoScalePan = false;

    int delta = we->angleDelta().y();
    if (delta == 0)
        return;

    if (intSign(m_wheelAngleDeltaAccumulator) != intSign(delta)) {
        m_wheelAngleDeltaAccumulator = 0;
    }

    m_wheelAngleDeltaAccumulator += delta;
    if (m_wheelAngleDeltaAccumulator > 120) {
        m_wheelAngleDeltaAccumulator = 0;

        m_scale *= 1.5;

    } else if (m_wheelAngleDeltaAccumulator < -120) {
        m_wheelAngleDeltaAccumulator = 0;

        m_scale *= 0.75;
    }

    SG_CLAMP_RANGE(m_scale, MINIMUM_SCALE, MAXIMUM_SCALE);
    update();
}

void BaseDiagram::paintContents(QPainter* painter)
{
    Q_UNUSED(painter);
}

void BaseDiagram::recomputeBounds(bool doResetZoom)
{
    m_bounds = QRectF();
    doComputeBounds();

    if (doResetZoom) {
        resetZoom();
    }

    update();
}

void BaseDiagram::resetZoom()
{
    m_autoScalePan = true;
    m_scale = 1.0;
    m_panOffset = QPointF();
    update();
}

void BaseDiagram::doComputeBounds()
{
    // no-op in the base class
}

void BaseDiagram::extendBounds(const QPointF& p, double radiusM)
{
    // this check added after a bug where apt.dat reports SCSL as
    // https://airportguide.com/airport/info/AG0003 (British Columbia)
    // but the groundnet is for
    // https://en.wikipedia.org/wiki/Salar_de_Atacama_Airport
    // causing a rather large airport boundary.
    QVector2D v(p - m_bounds.center());
    if (v.length() > 100000.0f) {
        qWarning() << "Excessively distant point" << p << v.length();
        return;
    }

    if (radiusM > 0.0) {
        extendRect(m_bounds, p - QPointF(radiusM, radiusM));
        extendRect(m_bounds, p + QPointF(radiusM, radiusM));
    } else {
        extendRect(m_bounds, p);
    }
}

QPointF BaseDiagram::project(const SGGeod& geod, const SGGeod& center)
{
    double r = earth_radius_lat(geod.getLatitudeRad());
    double ref_lat = center.getLatitudeRad(),
    ref_lon = center.getLongitudeRad(),
    lat = geod.getLatitudeRad(),
    lon = geod.getLongitudeRad(),
    lonDiff = lon - ref_lon;

    double c = acos( sin(ref_lat) * sin(lat) + cos(ref_lat) * cos(lat) * cos(lonDiff) );
    if (c == 0.0) {
        // angular distance from center is 0
        return QPointF(0.0, 0.0);
    }

    double k = c / sin(c);
    double x, y;
    if (ref_lat == (90 * SG_DEGREES_TO_RADIANS))
    {
        x = (SGD_PI / 2 - lat) * sin(lonDiff);
        y = -(SGD_PI / 2 - lat) * cos(lonDiff);
    }
    else if (ref_lat == -(90 * SG_DEGREES_TO_RADIANS))
    {
        x = (SGD_PI / 2 + lat) * sin(lonDiff);
        y = (SGD_PI / 2 + lat) * cos(lonDiff);
    }
    else
    {
        x = k * cos(lat) * sin(lonDiff);
        y = k * ( cos(ref_lat) * sin(lat) - sin(ref_lat) * cos(lat) * cos(lonDiff) );
    }

    // flip for top-left origin
    return QPointF(x, -y) * r;
}

SGGeod BaseDiagram::unproject(const QPointF& xy, const SGGeod& center)
{
    double r = earth_radius_lat(center.getLatitudeRad());
    double lat = 0,
           lon = 0,
           ref_lat = center.getLatitudeRad(),
           ref_lon = center.getLongitudeRad(),
           rho = QVector2D(xy).length(),
           c = rho/r;

    if (rho == 0) {
        return center;
    }

    // invert y to balance the equivalent in project()
    double x = xy.x(),
            y = -xy.y();
    lat = asin( cos(c) * sin(ref_lat) + (y * sin(c) * cos(ref_lat)) / rho);

    if (ref_lat == (90 * SG_DEGREES_TO_RADIANS)) // north pole
    {
        lon = ref_lon + atan(-x/y);
    }
    else if (ref_lat == -(90 * SG_DEGREES_TO_RADIANS)) // south pole
    {
        lon = ref_lon + atan(x/y);
    }
    else
    {
        lon = ref_lon + atan(x* sin(c) / (rho * cos(ref_lat) * cos(c) - y * sin(ref_lat) * sin(c)));
    }

    return SGGeod::fromRad(lon, lat);
}

QPointF BaseDiagram::project(const SGGeod& geod) const
{
    return project(geod, m_projectionCenter);
}

QPixmap BaseDiagram::iconForPositioned(const FGPositionedRef& pos,
                                       const IconOptions& options)
{
    // if airport type, check towered or untowered
    bool small = options.testFlag(SmallIcons);

    bool isTowered = false;
    if (FGPositioned::isAirportType(pos.ptr())) {
        FGAirport* apt = static_cast<FGAirport*>(pos.ptr());
        isTowered = apt->hasTower();
    }

    switch (pos->type()) {
    case FGPositioned::VOR:
        if (static_cast<FGNavRecord*>(pos.ptr())->isVORTAC())
            return QPixmap(":/vortac-icon");

        if (static_cast<FGNavRecord*>(pos.ptr())->hasDME())
            return QPixmap(":/vor-dme-icon");

        return QPixmap(":/vor-icon");

    // our filter only passes DMEs which are TACANs, so this is correct,
    // until we actually record TACANs in the NavCache
    case FGPositioned::DME:
        return QPixmap(":/tacan-icon");

    case FGPositioned::AIRPORT:
        return iconForAirport(static_cast<FGAirport*>(pos.ptr()), options);

    case FGPositioned::HELIPORT:
        return QPixmap(":/heliport-icon");
    case FGPositioned::SEAPORT:
        return QPixmap(isTowered ? ":/seaport-tower-icon" : ":/seaport-icon");
    case FGPositioned::NDB:
        return QPixmap(small ? ":/ndb-small-icon" : ":/ndb-icon");
    case FGPositioned::FIX:
        return QPixmap(":/waypoint-icon");

    default:
        break;
    }

    return QPixmap();
}

QPixmap BaseDiagram::iconForAirport(FGAirport* apt, const IconOptions& options)
{
    if (apt->isClosed()) {
        return QPixmap(":/airport-closed-icon");
    }

    if (!apt->hasHardRunwayOfLengthFt(1500)) {
        return QPixmap(apt->hasTower() ? ":/airport-tower-icon" : ":/airport-icon");
    }

    if (options.testFlag(LargeAirportPlans) && apt->hasHardRunwayOfLengthFt(8500)) {
        QPixmap result(96, 96);
        result.setDevicePixelRatio(2.0);
        QRect logicalRect = QRect(0, 0, 48, 48);

        result.fill(Qt::transparent);
        {
            QPainter p(&result);
            p.setRenderHint(QPainter::Antialiasing, true);
            QRectF b = logicalRect.adjusted(4, 4, -4, -4);
            QVector<QLineF> lines = projectAirportRuwaysIntoRect(apt, b);

            p.setPen(QPen(QColor(0x03, 0x83, 0xbf), 8));
            p.drawLines(lines);

            p.setPen(QPen(Qt::white, 2));
            p.drawLines(lines);
        }
        return result;
    }

    QPixmap result(50, 50);
    result.setDevicePixelRatio(2.0);
    result.fill(Qt::transparent);

    {
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);

        p.setBrush(apt->hasTower() ? QColor(0x03, 0x83, 0xbf) :
                                     QColor(0x9b, 0x5d, 0xa2));
        p.drawEllipse(QPointF(13, 13), 10, 10);

        FGRunwayRef r = apt->longestRunway();

        p.setPen(QPen(Qt::white, 2));
        p.translate(13, 13);
        p.rotate(r->headingDeg());
        p.drawLine(0, -8, 0, 8);
    }

    return result;
}

QVector<QLineF> BaseDiagram::projectAirportRuwaysWithCenter(FGAirportRef apt, const SGGeod& c)
{
    QVector<QLineF> r;
    for (auto rwy : apt->getRunwaysWithoutReciprocals()) {
        QPointF p1 = project(rwy->geod(), c);
        QPointF p2 = project(rwy->end(), c);
        r.append(QLineF(p1, p2));
    }

    return r;
}

void BaseDiagram::setAircraftType(LauncherController::AircraftType type)
{
    m_aircraftType = type;
    update();
}

QVector<QLineF> BaseDiagram::projectAirportRuwaysIntoRect(FGAirportRef apt, const QRectF &bounds)
{
    QVector<QLineF> r = projectAirportRuwaysWithCenter(apt, apt->geod());

    QRectF extent;
    Q_FOREACH(const QLineF& l, r) {
        extendRect(extent, l.p1());
        extendRect(extent, l.p2());
    }

 // find constraining scale factor
    double ratioInX = bounds.width() / extent.width();
    double ratioInY = bounds.height() / extent.height();

    QTransform t;
    t.translate(bounds.left(), bounds.top());
    t.scale(std::min(ratioInX, ratioInY),
            std::min(ratioInX, ratioInY));
    t.translate(-extent.left(), -extent.top()); // move unscaled to 0,0

    for (int i=0; i<r.size(); ++i) {
        r[i] = t.map(r[i]);
    }

    return r;
}

QPointF BaseDiagram::projectedPosition(PositionedID pid) const
{
    if (m_projectedPositions.contains(pid))
        return m_projectedPositions.value(pid);

    FGPositionedRef pos = NavDataCache::instance()->loadById(pid);
    if (!pos)
        return {};

    QPointF pt = project(pos->geod());
    m_projectedPositions[pid] = pt;
    return pt;
}

QPointF BaseDiagram::projectedPosition(FGPositionedRef pos) const
{
    if (m_projectedPositions.contains(pos->guid()))
        return m_projectedPositions.value(pos->guid());

    QPointF pt = project(pos->geod());
    m_projectedPositions[pos->guid()] = pt;
    return pt;
}
