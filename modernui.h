#pragma once

#include <QFrame>
#include <QLabel>
#include <QFont>
#include <QString>

namespace ModernUi {

    // Glass panel. Ο selector με objectName δεν "χύνεται" στα παιδιά (labels κ.λπ.)
    inline QFrame* makePanel(QWidget* parent = nullptr) {
        QFrame* f = new QFrame(parent);
        f->setObjectName("GlassPanel");
        f->setStyleSheet(
            "QFrame#GlassPanel {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "              stop:0 rgba(30,41,59,200), stop:1 rgba(10,16,30,225));"
            "  border: 1px solid rgba(255,255,255,40);"
            "  border-radius: 24px;"
            "}");
        return f;
    }

    // Μικρός τίτλος: κεφαλαία, απαλό γκρι, απόσταση γραμμάτων
    inline QLabel* makeCaption(const QString& text, QWidget* parent = nullptr,
        int pt = 10, qreal spacing = 2.0) {
        QLabel* l = new QLabel(text.toUpper(), parent);
        QFont f("Segoe UI", pt, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
        l->setFont(f);
        l->setStyleSheet("color: rgba(203,213,225,230); background: transparent; border: none;");
        return l;
    }

    // Τιμή: Segoe UI bold
    inline QLabel* makeValue(const QString& text, QWidget* parent = nullptr,
        int pt = 20, const QString& color = "white") {
        QLabel* l = new QLabel(text, parent);
        l->setFont(QFont("Segoe UI", pt, QFont::Bold));
        l->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(color));
        return l;
    }

} // namespace ModernUi