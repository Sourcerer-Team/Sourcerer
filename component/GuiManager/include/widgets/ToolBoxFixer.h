#ifndef TOOLBOXFIXER_H
#define TOOLBOXFIXER_H

#include <StylesheetManager.h>

#include <QColor>
#include <QEvent>
#include <QObject>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QToolBox>
#include <QWidget>

class ToolBoxFixer : public QObject {
  Q_OBJECT
public:
  explicit ToolBoxFixer(QToolBox *toolBox, qreal cornerRadius = 6.0,
                        int tabHeight = 38)
      : QObject(toolBox), m_toolBox(toolBox), m_cornerRadius(cornerRadius),
        m_tabHeight(tabHeight) {
    setColorsBasedOnTheme();
  }

  // Attach to a QToolBox instance
  static ToolBoxFixer *apply(QToolBox *toolBox, qreal cornerRadius = 6.0,
                             int tabHeight = 38) {
    ToolBoxFixer *fixer = new ToolBoxFixer(toolBox, cornerRadius, tabHeight);
    toolBox->installEventFilter(fixer);
    fixer->attachToButtons();
    return fixer;
  }

  // Color & Style Customization Setters
  void setNormalBackgroundColor(const QColor &color) { m_normalBg = color; }
  void setHoverBackgroundColor(const QColor &color) { m_hoverBg = color; }
  void setSelectedBackgroundColor(const QColor &color) { m_selectedBg = color; }
  void setBorderColor(const QColor &color) { m_borderColor = color; }
  void setSelectedBorderColor(const QColor &color) {
    m_selectedBorderColor = color;
  }
  void setTextColor(const QColor &color) { m_textColor = color; }
  void setSelectedTextColor(const QColor &color) {
    m_selectedTextColor = color;
  }
  void setCornerRadius(qreal radius) { m_cornerRadius = radius; }
  void setTabHeight(int height) { m_tabHeight = height; }

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    // Auto-hook dynamically added tabs
    if (obj == m_toolBox && (event->type() == QEvent::ChildAdded ||
                             event->type() == QEvent::LayoutRequest)) {
      attachToButtons();
      return false;
    }

    // Intercept header button painting
    if (event->type() == QEvent::Paint) {
      setColorsBasedOnTheme();
      QWidget *btn = qobject_cast<QWidget *>(obj);
      if (btn && m_toolBox) {
        int tabIndex = findTabIndex(btn);

        if (tabIndex != -1) {
          QPainter painter(btn);
          painter.setRenderHint(QPainter::Antialiasing);
          painter.setRenderHint(QPainter::SmoothPixmapTransform);

          // 0.5px adjustment for crisp 1px vector borders
          QRectF rect = QRectF(btn->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
          QIcon icon = m_toolBox->itemIcon(tabIndex);
          QString text = m_toolBox->itemText(tabIndex);

          // 1. Evaluate Tab States
          QStyleOptionToolButton opt;
          opt.initFrom(btn);

          bool isSelected = (tabIndex == m_toolBox->currentIndex());
          bool isHovered = (opt.state & QStyle::State_MouseOver);
          bool isPressed = (opt.state & QStyle::State_Sunken);

          // 2. Resolve Flat Colors
          QColor currentBg = m_normalBg;
          QColor currentBorder = m_borderColor;
          QColor currentText = m_textColor;

          if (isSelected) {
            currentBg = m_selectedBg;
            currentBorder = m_selectedBorderColor;
            currentText = m_selectedTextColor;
          } else if (isPressed) {
            currentBg = m_hoverBg.darker(110);
            currentBorder = m_selectedBorderColor;
          } else if (isHovered) {
            currentBg = m_hoverBg;
            currentBorder = m_selectedBorderColor;
          }

          // 3. DRAW FLAT BACKGROUND & 1PX BORDER
          painter.setBrush(currentBg);
          painter.setPen(QPen(currentBorder, 1));
          painter.drawRoundedRect(rect, m_cornerRadius, m_cornerRadius);

          // 4. DETERMINE ICON SIZE
          QSize iconSize(20, 20);
          int iconDim = iconSize.height();

          int padding = 10;
          int yCenter = btn->height() / 2;

          // 5. DRAW ICON (Vertically Centered)
          QRect iconRect(padding, yCenter - (iconDim / 2), iconDim, iconDim);
          if (!icon.isNull()) {
            QIcon::Mode mode =
                btn->isEnabled() ? QIcon::Normal : QIcon::Disabled;
            QIcon::State state = isSelected ? QIcon::On : QIcon::Off;
            icon.paint(&painter, iconRect, Qt::AlignCenter, mode, state);
          }

          // 6. DRAW TEXT (Vertically Centered)
          int textX = icon.isNull() ? padding : (iconRect.right() + padding);
          QRect textRect(textX, 0, btn->width() - textX - padding,
                         btn->height());

          painter.setFont(btn->font());
          painter.setPen(currentText);
          painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

          return true; // Suppress default native painting
        }
      }
    }
    return QObject::eventFilter(obj, event);
  }

private:
  void attachToButtons() {
    const auto children = m_toolBox->findChildren<QWidget *>();
    for (QWidget *child : children) {
      if (qstrcmp(child->metaObject()->className(), "QToolBoxButton") == 0) {
        child->setMinimumHeight(m_tabHeight);
        child->removeEventFilter(this);
        child->installEventFilter(this);
      }
    }
  }

  int findTabIndex(QWidget *btn) const {
    const auto children = m_toolBox->findChildren<QWidget *>();
    int index = 0;
    for (QWidget *child : children) {
      if (qstrcmp(child->metaObject()->className(), "QToolBoxButton") == 0) {
        if (child == btn)
          return index;
        index++;
      }
    }
    return -1;
  }

  void setColorsBasedOnTheme() {
    const auto theme = StylesheetManager::StylesheetManager::currentStylesheet();
    if (theme == StylesheetManager::StylesheetKey::SourcererSolarized) {
      m_normalBg = QColor("#002B36");
      m_hoverBg = QColor("#002B36");
      m_selectedBg = QColor("#0E4A5A");
      m_borderColor = QColor("#073642");
      m_selectedBorderColor = QColor("#073642");
      m_textColor = QColor("#93A1A1");
      m_selectedTextColor = QColor("#FDF6E3");
    } else if (theme == StylesheetManager::StylesheetKey::SourcererGruvbox) {
      m_normalBg = QColor("#282828");
      m_hoverBg = QColor("#282828");
      m_selectedBg = QColor("#504945");
      m_borderColor = QColor("#3C3836");
      m_selectedBorderColor = QColor("#3C3836");
      m_textColor = QColor("#EBDBB2");
      m_selectedTextColor = QColor("#FBF1C7");
    } else if (theme == StylesheetManager::StylesheetKey::SourcererMonokai) {
      m_normalBg = QColor("#272822");
      m_hoverBg = QColor("#272822");
      m_selectedBg = QColor("#49483E");
      m_borderColor = QColor("#3E3D32");
      m_selectedBorderColor = QColor("#3E3D32");
      m_textColor = QColor("#F8F8F2");
      m_selectedTextColor = QColor("#FFFFFF");
    } else if (theme == StylesheetManager::StylesheetKey::SourcererBlue) {
      m_normalBg = QColor("#212D43");
      m_hoverBg = QColor("#212D43");
      m_selectedBg = QColor("#205472");
      m_borderColor = QColor("#3A4D68");
      m_selectedBorderColor = QColor("#3A4D68");
      m_textColor = QColor("#E6ECF5");
      m_selectedTextColor = QColor("#FFFFFF");
    } else if (theme == StylesheetManager::StylesheetKey::SourcererPurple) {
      m_normalBg = QColor("#332F3F");
      m_hoverBg = QColor("#332F3F");
      m_selectedBg = QColor("#374659");
      m_borderColor = QColor("#6E6A7A");
      m_selectedBorderColor = QColor("#6E6A7A");
      m_textColor = QColor("#E8E6EE");
      m_selectedTextColor = QColor("#FFFFFF");
    } else if (theme != StylesheetManager::StylesheetKey::SourcererLight) {
      m_normalBg = QColor("#353535");
      m_hoverBg = QColor("#353535");
      m_selectedBg = QColor("#384D59");
      m_borderColor = QColor("#6D6E6E");
      m_selectedBorderColor = QColor("#6D6E6E");
      m_textColor = QColor("#FFFFFF");
      m_selectedTextColor = QColor("#FFFFFF");
    } else {
      m_normalBg = QColor("#F3F6F8");
      m_hoverBg = QColor("#F3F6F8");
      m_selectedBg = QColor("#E7EEF4");
      m_borderColor = QColor("#B8C0C8");
      m_selectedBorderColor = QColor("#B8C0C8");
      m_textColor = QColor("#202020");
      m_selectedTextColor = QColor("#202020");
    }
  }

  QToolBox *m_toolBox;
  qreal m_cornerRadius;
  int m_tabHeight;

  QColor m_normalBg;
  QColor m_hoverBg;
  QColor m_selectedBg;
  QColor m_borderColor;
  QColor m_selectedBorderColor;
  QColor m_textColor;
  QColor m_selectedTextColor;
};

#endif // TOOLBOXFIXER_H