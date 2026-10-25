#include "Theme.h"

#include <QStyleFactory>

namespace Theme {

QString styleSheet()
{
    return QStringLiteral(R"QSS(
* {
    outline: none;
}
QWidget {
    color: #E7ECF5;
    font-family: "Segoe UI Variable Text", "Segoe UI", "Microsoft YaHei UI";
    font-size: 13px;
}
QMainWindow, QWidget#Root, QWidget#PageBody {
    background-color: #080A0F;
}
QFrame#Sidebar {
    background-color: #0A0D13;
    border: none;
    border-right: 1px solid #151A23;
}
QLabel#BrandTitle {
    font-size: 18px;
    font-weight: 700;
    color: #F4F7FC;
}
QLabel#BrandSub {
    font-size: 9px;
    color: #566072;
}
QLabel#NavSection {
    color: #4F596A;
    font-size: 10px;
    font-weight: 600;
    padding: 0 10px;
}
QPushButton#Nav {
    background: transparent;
    border: none;
    border-radius: 10px;
    color: #8B97AA;
    padding: 10px 12px;
    text-align: left;
    font-size: 13px;
    font-weight: 500;
}
QPushButton#Nav:hover {
    background-color: #11161E;
    color: #DCE4F2;
}
QPushButton#Nav:checked {
    background-color: rgba(255, 255, 255, 0.06);
    color: #F0F4FF;
    font-weight: 600;
}
QFrame#SidebarStatus {
    background-color: #0D1118;
    border: 1px solid #191F2A;
    border-radius: 12px;
}
QLabel#SidebarStatusTitle {
    color: #93A0B4;
    font-size: 11px;
    font-weight: 600;
}
QLabel#SidebarStatusValue {
    color: #E7ECF5;
    font-size: 12px;
    font-weight: 600;
}
QLabel#SidebarStatusHint {
    color: #616D82;
    font-size: 10px;
}
QLabel#SidebarFooter {
    color: #525D70;
    font-size: 10px;
}
QLabel#PageTitle {
    font-size: 26px;
    font-weight: 700;
    color: #F5F8FD;
}
QLabel#PageSub {
    color: #748095;
    font-size: 12px;
}
QLabel#SectionTitle {
    font-size: 15px;
    font-weight: 600;
    color: #EDF1F9;
}
QLabel#Muted {
    color: #8A95A9;
}
QLabel#FieldLabel {
    color: #8A95A9;
    font-size: 11px;
    font-weight: 600;
}
QLabel#Endpoint {
    color: #AFC0FF;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
}
QLabel#EndpointPath {
    color: #DCE5F5;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 12px;
}
QLabel#Toast {
    background-color: rgba(20, 26, 37, 0.97);
    border: 1px solid #2B3850;
    color: #E8EEF8;
    border-radius: 10px;
    padding: 10px 16px;
    font-size: 12px;
}
QLabel#Toast[variant="error"] {
    background-color: rgba(42, 20, 27, 0.97);
    border: 1px solid rgba(255, 107, 133, 0.45);
    color: #FFC2CE;
}
QLabel#Hint {
    color: #6E7A8F;
    font-size: 11px;
}
QLabel#StatTitle {
    color: #8A95A9;
    font-size: 11px;
    font-weight: 600;
}
QLabel#StatValue {
    font-size: 26px;
    font-weight: 700;
    color: #F3F6FC;
}
QLabel#StatUnit {
    color: #7C879B;
    font-size: 11px;
    padding-bottom: 4px;
}
QLabel#StatCaption {
    color: #6E7A8F;
    font-size: 10px;
}
QLabel#MeterValue {
    color: #E7ECF5;
    font-size: 12px;
    font-weight: 600;
}
QProgressBar#MeterBar {
    background-color: #1A2330;
    border: none;
    border-radius: 3px;
}
QProgressBar#MeterBar::chunk {
    border-radius: 3px;
    background-color: #6C8CFF;
}
QLabel#ServerPill {
    border-radius: 11px;
    padding: 5px 12px;
    font-size: 11px;
    font-weight: 600;
    background-color: rgba(138, 149, 169, 0.10);
    border: 1px solid #232D3F;
    color: #A4AEC0;
}
QLabel#ServerPill[state="running"] {
    background-color: rgba(61, 220, 151, 0.10);
    border: 1px solid rgba(61, 220, 151, 0.30);
    color: #6FE7B0;
}
QLabel#ServerPill[state="starting"], QLabel#ServerPill[state="stopping"] {
    background-color: rgba(245, 184, 74, 0.10);
    border: 1px solid rgba(245, 184, 74, 0.30);
    color: #F5CE7F;
}
QLabel#ServerPill[state="failed"] {
    background-color: rgba(255, 107, 133, 0.10);
    border: 1px solid rgba(255, 107, 133, 0.32);
    color: #FF93A8;
}
QLabel#Badge {
    border-radius: 7px;
    padding: 3px 8px;
    font-size: 10px;
    font-weight: 600;
}
QLabel#Badge[variant="accent"] {
    background-color: rgba(108, 140, 255, 0.14);
    color: #A9BBFF;
    border: 1px solid rgba(108, 140, 255, 0.24);
}
QLabel#Badge[variant="success"] {
    background-color: rgba(61, 220, 151, 0.12);
    color: #7BE8B4;
    border: 1px solid rgba(61, 220, 151, 0.24);
}
QLabel#Badge[variant="warning"] {
    background-color: rgba(245, 184, 74, 0.12);
    color: #F5CE7F;
    border: 1px solid rgba(245, 184, 74, 0.24);
}
QLabel#Badge[variant="danger"] {
    background-color: rgba(255, 107, 133, 0.12);
    color: #FF93A8;
    border: 1px solid rgba(255, 107, 133, 0.24);
}
QLabel#Badge[variant="muted"] {
    background-color: rgba(138, 149, 169, 0.10);
    color: #9AA6BA;
    border: 1px solid #242E40;
}
QLabel#MethodPill {
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 10px;
    font-weight: 700;
    padding: 3px 8px;
    border-radius: 6px;
}
QLabel#MethodPill[method="GET"] {
    background-color: rgba(90, 200, 250, 0.12);
    color: #7FD8FF;
}
QLabel#MethodPill[method="POST"] {
    background-color: rgba(61, 220, 151, 0.12);
    color: #7BE8B4;
}
QFrame#Card {
    background-color: #10141C;
    border: 1px solid #1A202B;
    border-radius: 16px;
}
QFrame#CardFlat {
    background-color: #0D1118;
    border: 1px solid #171D26;
    border-radius: 12px;
}
QFrame#Divider {
    background-color: #191F29;
    border: none;
    max-height: 1px;
    min-height: 1px;
}
QPushButton {
    background-color: #141922;
    border: 1px solid #222B3A;
    border-radius: 10px;
    padding: 7px 15px;
    color: #C6D0E2;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #1A2130;
    border-color: #2F3A50;
    color: #E6EBF5;
}
QPushButton:pressed {
    background-color: #10161F;
}
QPushButton:disabled {
    color: #5B667A;
    border-color: #1D2533;
    background-color: #121821;
}
QPushButton#Primary {
    background-color: #5C7CFA;
    color: #FFFFFF;
    border: none;
    border-radius: 10px;
    padding: 9px 18px;
    font-weight: 600;
}
QPushButton#Primary:hover {
    background-color: #6E8BFF;
}
QPushButton#Primary:pressed {
    background-color: #4E6AE0;
}
QPushButton#Primary:disabled {
    background-color: #222B3D;
    color: #5E6A80;
}
QPushButton#Danger {
    background-color: rgba(255, 107, 133, 0.12);
    border: 1px solid rgba(255, 107, 133, 0.34);
    color: #FF93A8;
    border-radius: 10px;
    padding: 9px 18px;
    font-weight: 600;
}
QPushButton#Danger:hover {
    background-color: rgba(255, 107, 133, 0.20);
}
QPushButton#Ghost {
    background-color: transparent;
    border: 1px solid #273247;
    border-radius: 9px;
    padding: 8px 14px;
    color: #C2CCDD;
}
QPushButton#Ghost:hover {
    background-color: #151C28;
    border-color: #3A4A66;
    color: #EDF1F9;
}
QPushButton#IconButton {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 5px;
}
QPushButton#IconButton:hover {
    background-color: #1A2230;
}
QLineEdit, QSpinBox, QComboBox {
    background-color: #0C1017;
    border: 1px solid #1E2634;
    border-radius: 10px;
    padding: 7px 10px;
    color: #E7ECF5;
    selection-background-color: #3D5BD9;
    min-height: 16px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: #5B7CFA;
}
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    color: #5F6B7F;
    background-color: #10151E;
    border-color: #1C2432;
}
QSpinBox {
    padding-right: 4px;
}
QSpinBox::up-button, QSpinBox::down-button {
    width: 16px;
    background: transparent;
    border: none;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QPlainTextEdit, QTextEdit {
    background-color: #0C1017;
    border: 1px solid #1E2634;
    border-radius: 10px;
    padding: 7px 9px;
    color: #E7ECF5;
    selection-background-color: #3D5BD9;
}
QPlainTextEdit#Log {
    background-color: #0A0E15;
    border: 1px solid #1B2432;
    border-radius: 10px;
    padding: 10px;
    color: #9FB0C7;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
}
QPlainTextEdit#CommandView {
    background-color: #0B1018;
    border: 1px solid #1D2636;
    border-radius: 9px;
    color: #AFC0FF;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
    padding: 9px;
}
QPlainTextEdit#KnowledgeResults {
    background-color: #0C1017;
    border: 1px solid #1E2634;
    border-radius: 12px;
    padding: 12px;
    color: #C3CEDF;
    font-size: 12px;
}
QPlainTextEdit#ChatInput {
    background-color: #0D121A;
    border: 1px solid #222C3C;
    border-radius: 10px;
    padding: 8px 10px;
    font-size: 13px;
}
QListWidget#ModelList {
    background: transparent;
    border: none;
    padding: 0;
}
QListWidget#ModelList::item {
    background: transparent;
    border: none;
}
QListWidget#ModelList::item:selected {
    background: transparent;
}
QTableWidget {
    background-color: #10141C;
    border: 1px solid #1A202B;
    border-radius: 14px;
    gridline-color: transparent;
    selection-background-color: #182238;
    selection-color: #F2F5FB;
}
QTableWidget::item {
    padding: 8px 12px;
    border-bottom: 1px solid #1A2230;
    color: #C7D1E2;
}
QTableWidget::item:selected {
    background-color: #182238;
    color: #F2F5FB;
}
QHeaderView::section {
    background-color: #0D1118;
    color: #8A96AB;
    border: none;
    border-bottom: 1px solid #1E2634;
    padding: 9px 12px;
    font-weight: 600;
    font-size: 11px;
}
QTableCornerButton::section {
    background-color: #0F141D;
    border: none;
}
QScrollArea {
    background: transparent;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px 2px 2px 0;
}
QScrollBar::handle:vertical {
    background-color: #232C3B;
    border-radius: 4px;
    min-height: 26px;
}
QScrollBar::handle:vertical:hover {
    background-color: #36435A;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0 2px 2px 2px;
}
QScrollBar::handle:horizontal {
    background-color: #232C3B;
    border-radius: 4px;
    min-width: 26px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
}
QSplitter::handle {
    background: transparent;
}
QToolTip {
    background-color: #151C28;
    color: #DCE4F2;
    border: 1px solid #2A3448;
    padding: 6px 8px;
    border-radius: 6px;
}
QMenu {
    background-color: #141A25;
    border: 1px solid #253044;
    border-radius: 8px;
    padding: 5px;
}
QMenu::item {
    padding: 6px 20px;
    border-radius: 6px;
}
QMenu::item:selected {
    background-color: #23304A;
}
QMessageBox {
    background-color: #10141C;
}
QMessageBox QLabel {
    color: #E7ECF5;
}
QDialog {
    background-color: #10141C;
}
QFrame#Bubble {
    border-radius: 12px;
}
QFrame#Bubble[role="assistant"] {
    background-color: #11161F;
    border: 1px solid #1C2330;
}
QFrame#Bubble[role="user"] {
    background-color: #1D2840;
    border: 1px solid #2A3958;
}
QLabel#BubbleText {
    background: transparent;
    color: #EAF0FA;
    font-size: 13px;
}
QLabel#BubbleMeta {
    background: transparent;
    color: #71809A;
    font-size: 9px;
}
)QSS");
}

QPalette palette()
{
    QPalette p;
    p.setColor(QPalette::Window, Background);
    p.setColor(QPalette::WindowText, Text);
    p.setColor(QPalette::Base, SurfaceAlt);
    p.setColor(QPalette::AlternateBase, Surface);
    p.setColor(QPalette::ToolTipBase, Surface);
    p.setColor(QPalette::ToolTipText, Text);
    p.setColor(QPalette::Text, Text);
    p.setColor(QPalette::Button, Surface);
    p.setColor(QPalette::ButtonText, Text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Link, Accent);
    p.setColor(QPalette::Highlight, Accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, Dim);
    p.setColor(QPalette::Disabled, QPalette::Text, Dim);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, Dim);
    p.setColor(QPalette::Disabled, QPalette::WindowText, Dim);
    return p;
}

QFont applicationFont()
{
    QFont font(QStringLiteral("Segoe UI"));
    font.setPointSizeF(9.75);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

} // namespace Theme
