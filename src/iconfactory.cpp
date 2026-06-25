#include "iconfactory.h"

#include <QPainter>
#include <QPixmap>

namespace {

constexpr int IconSize = 64;

QPixmap fallbackPixmap(bool running)
{
    QPixmap pixmap(IconSize, IconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(running ? QColor(46, 160, 67) : QColor(130, 130, 130));
    painter.drawEllipse(QPointF(IconSize / 2.0, IconSize / 2.0), 28.0, 28.0);
    return pixmap;
}

void drawStatusDot(QPixmap *pixmap, bool running)
{
    QPainter painter(pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal diameter = qMax<qreal>(pixmap->width() / 5.0, 8.0);
    const qreal radius = diameter / 2.0;
    const QPointF center(pixmap->width() - radius - 2.0, pixmap->height() - radius - 2.0);

    painter.setPen(QPen(Qt::white, 2.0));
    painter.setBrush(running ? QColor(46, 160, 67) : QColor(150, 150, 150));
    painter.drawEllipse(center, radius, radius);
}

} // namespace

namespace IconFactory {

QIcon build(bool running)
{
    QPixmap source(QStringLiteral(":/assets/qwenpaw.png"));
    QPixmap pixmap;

    if (source.isNull()) {
        pixmap = fallbackPixmap(running);
    } else {
        pixmap = QPixmap(IconSize, IconSize);
        pixmap.fill(Qt::transparent);

        const QPixmap scaled = source.scaled(
            IconSize,
            IconSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);

        QPainter painter(&pixmap);
        painter.drawPixmap(
            (IconSize - scaled.width()) / 2,
            (IconSize - scaled.height()) / 2,
            scaled);
    }

    drawStatusDot(&pixmap, running);
    return QIcon(pixmap);
}

} // namespace IconFactory

