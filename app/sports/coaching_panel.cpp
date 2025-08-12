/**
 * @file coaching_panel.cpp
 * @brief Implementation of sports coaching interface for Olive integration
 * 
 * Qt-based UI implementation connecting Triangle Defense analysis with
 * Olive's video editing timeline and player interface.
 */

#include "coaching_panel.h"
#include <QApplication>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QSplitter>
#include <QTabWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace amt {
namespace sports {

// FormationDiagramWidget Implementation
class FormationDiagramWidget::DiagramImpl {
public:
    QGraphicsScene* scene;
    QGraphicsPixmapItem* field_background;
    std::vector<QGraphicsEllipseItem*> player_dots;
    std::vector<QGraphicsLineItem*> formation_lines;
    FormationData current_formation;
    QColor offense_color = QColor(226, 2, 26);    // Red
    QColor defense_color = QColor(255, 255, 255); // White
    bool show_player_numbers = true;
    
    DiagramImpl() : scene(new QGraphicsScene()) {
        field_background = nullptr;
    }
    
    ~DiagramImpl() {
        delete scene;
    }
};

FormationDiagramWidget::FormationDiagramWidget(QWidget* parent)
    : QGraphicsView(parent), m_diagram_impl(std::make_unique<DiagramImpl>()) {
    
    setScene(m_diagram_impl->scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);
    setMinimumSize(300, 200);
    
    // Set football field background
    drawField();
    
    qDebug() << "[Formation Diagram] Widget initialized";
}

FormationDiagramWidget::~FormationDiagramWidget() = default;

void FormationDiagramWidget::updateFormation(const FormationData& formation) {
    m_diagram_impl->current_formation = formation;
    
    // Update formation display
    highlightTriangleDefense(formation.type);
    
    // Update scene
    update();
    
    qDebug() << "[Formation Diagram] Updated to" << QString::fromStdString(
        formation_utils::formationToString(formation.type));
}

void FormationDiagramWidget::setPlayerPositions(const std::vector<PlayerPosition>& players) {
    // Clear existing player dots
    for (auto* dot : m_diagram_impl->player_dots) {
        m_diagram_impl->scene->removeItem(dot);
        delete dot;
    }
    m_diagram_impl->player_dots.clear();
    
    // Add new player positions
    for (const auto& player : players) {
        QGraphicsEllipseItem* dot = new QGraphicsEllipseItem(
            player.position.x - 8, player.position.y - 8, 16, 16);
        
        QColor color = (player.team == "offense") ? 
            m_diagram_impl->offense_color : m_diagram_impl->defense_color;
        
        dot->setBrush(QBrush(color));
        dot->setPen(QPen(Qt::white, 2));
        
        m_diagram_impl->scene->addItem(dot);
        m_diagram_impl->player_dots.push_back(dot);
        
        // Add jersey number if available and enabled
        if (m_diagram_impl->show_player_numbers && player.jersey_number > 0) {
            QGraphicsTextItem* number = m_diagram_impl->scene->addText(
                QString::number(player.jersey_number), QFont("Arial", 8, QFont::Bold));
            number->setDefaultTextColor(Qt::white);
            number->setPos(player.position.x - 6, player.position.y - 6);
        }
    }
    
    qDebug() << "[Formation Diagram] Updated" << players.size() << "player positions";
}

void FormationDiagramWidget::highlightTriangleDefense(FormationType type) {
    // Clear existing formation lines
    for (auto* line : m_diagram_impl->formation_lines) {
        m_diagram_impl->scene->removeItem(line);
        delete line;
    }
    m_diagram_impl->formation_lines.clear();
    
    // Draw Triangle Defense formation overlay
    QPen formation_pen(QColor(0, 212, 255), 3, Qt::DashLine); // Cyan highlight
    
    switch (type) {
        case FormationType::LARRY:
            drawLarryFormationLines(formation_pen);
            break;
        case FormationType::LINDA:
            drawLindaFormationLines(formation_pen);
            break;
        case FormationType::RITA:
            drawRitaFormationLines(formation_pen);
            break;
        default:
            // Generic 5-3-1 outline
            drawGeneric531Lines(formation_pen);
            break;
    }
}

void FormationDiagramWidget::drawField() {
    // Create football field background
    QPixmap field_pixmap(400, 300);
    field_pixmap.fill(QColor(74, 124, 58)); // Football field green
    
    QPainter painter(&field_pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw yard lines
    painter.setPen(QPen(Qt::white, 2));
    for (int i = 0; i <= 10; ++i) {
        int x = i * 40;
        painter.drawLine(x, 0, x, 300);
    }
    
    // Draw hash marks
    painter.setPen(QPen(Qt::white, 1));
    for (int i = 0; i <= 10; ++i) {
        int x = i * 40;
        painter.drawLine(x, 120, x, 130); // Top hash
        painter.drawLine(x, 170, x, 180); // Bottom hash
    }
    
    // Add to scene
    if (m_diagram_impl->field_background) {
        m_diagram_impl->scene->removeItem(m_diagram_impl->field_background);
        delete m_diagram_impl->field_background;
    }
    
    m_diagram_impl->field_background = m_diagram_impl->scene->addPixmap(field_pixmap);
    m_diagram_impl->scene->setSceneRect(field_pixmap.rect());
}

// CoachingInsightsWidget Implementation
CoachingInsightsWidget::CoachingInsightsWidget(QWidget* parent)
    : QWidget(parent) {
    
    setMinimumSize(300, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Insights display
    QLabel* insights_label = new QLabel("Coaching Insights", this);
    insights_label->setStyleSheet("font-weight: bold; font-size: 14px; color: #00d4ff;");
    layout->addWidget(insights_label);
    
    m_insights_display = new QTextEdit(this);
    m_insights_display->setReadOnly(true);
    m_insights_display->setStyleSheet(
        "QTextEdit {"
        "   background-color: #0f1419;"
        "   color: #ffffff;"
        "   border: 1px solid #2d3748;"
        "   border-radius: 4px;"
        "   padding: 8px;"
        "   font-family: 'Consolas', monospace;"
        "}"
    );
    layout->addWidget(m_insights_display);
    
    // CLS Analysis section
    QGroupBox* cls_group = new QGroupBox("CLS Analysis", this);
    cls_group->setStyleSheet(
        "QGroupBox {"
        "   font-weight: bold;"
        "   color: #00d4ff;"
        "   border: 1px solid #2d3748;"
        "   border-radius: 4px;"
        "   margin-top: 8px;"
        "   padding-top: 8px;"
        "}"
    );
    
    QVBoxLayout* cls_layout = new QVBoxLayout(cls_group);
    
    m_cls_score = new QLabel("CLS Score: --", this);
    m_cls_score->setStyleSheet("color: #ffffff; font-size: 12px;");
    cls_layout->addWidget(m_cls_score);
    
    m_confidence_bar = new QProgressBar(this);
    m_confidence_bar->setRange(0, 100);
    m_confidence_bar->setStyleSheet(
        "QProgressBar {"
        "   border: 1px solid #2d3748;"
        "   border-radius: 4px;"
        "   text-align: center;"
        "   color: #ffffff;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #00d4ff;"
        "   border-radius: 3px;"
        "}"
    );
    cls_layout->addWidget(m_confidence_bar);
    
    layout->addWidget(cls_group);
    
    // Alerts list
    QLabel* alerts_label = new QLabel("Formation Alerts", this);
    alerts_label->setStyleSheet("font-weight: bold; font-size: 12px; color: #00d4ff;");
    layout->addWidget(alerts_label);
    
    m_alerts_list = new QListWidget(this);
    m_alerts_list->setMaximumHeight(100);
    m_alerts_list->setStyleSheet(
        "QListWidget {"
        "   background-color: #0f1419;"
        "   color: #ffffff;"
        "   border: 1px solid #2d3748;"
        "   border-radius: 4px;"
        "}"
        "QListWidget::item {"
        "   padding: 4px;"
        "   border-bottom: 1px solid #2d3748;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: #00d4ff;"
        "}"
    );
    layout->addWidget(m_alerts_list);
    
    qDebug() << "[Coaching Insights] Widget initialized";
}

CoachingInsightsWidget::~CoachingInsightsWidget() = default;

void CoachingInsightsWidget::updateInsights(const std::vector<std::string>& insights) {
    QString html = "<div style='color: #ffffff; font-family: Consolas;'>";
    
    for (size_t i = 0; i < insights.size(); ++i) {
        QString insight = QString::fromStdString(insights[i]);
        
        // Color code different types of insights
        if (insight.contains("Formation Analysis")) {
            html += "<div style='color: #00d4ff; font-weight: bold;'>";
        } else if (insight.contains("Coaching Point")) {
            html += "<div style='color: #4a7c3a; font-weight: bold;'>";
        } else if (insight.contains("Key Focus")) {
            html += "<div style='color: #e2021a; font-weight: bold;'>";
        } else {
            html += "<div style='color: #a1a1a1;'>";
        }
        
        html += "• " + insight + "</div><br>";
    }
    
    html += "</div>";
    m_insights_display->setHtml(html);
    
    qDebug() << "[Coaching Insights] Updated with" << insights.size() << "insights";
}

void CoachingInsightsWidget::showCLSAnalysis(const CLSAnalysis& cls) {
    m_cls_score->setText(QString("CLS Score: %1/10").arg(cls.cls_score, 0, 'f', 1));
    m_confidence_bar->setValue(static_cast<int>(cls.cls_score * 10));
    
    // Add CLS details to insights
    QString cls_text = QString(
        "<b>Configuration:</b> %1<br>"
        "<b>Location:</b> %2<br>"
        "<b>Situation:</b> %3"
    ).arg(QString::fromStdString(cls.configuration))
     .arg(QString::fromStdString(cls.location))
     .arg(QString::fromStdString(cls.situation));
    
    m_insights_display->append(cls_text);
}

// CoachingPanel Implementation
CoachingPanel::CoachingPanel(QWidget* parent)
    : QWidget(parent), m_sports_core(nullptr), m_formation_detector(nullptr),
      m_video_player(nullptr), m_timeline(nullptr), m_current_timestamp(0.0) {
    
    setupUI();
    setupConnections();
    
    // Initialize timers
    m_analysis_timer = new QTimer(this);
    m_ui_update_timer = new QTimer(this);
    
    connect(m_analysis_timer, &QTimer::timeout, this, &CoachingPanel::processCurrentFrame);
    connect(m_ui_update_timer, &QTimer::timeout, this, &CoachingPanel::updateRealTimeDisplay);
    
    qDebug() << "[Coaching Panel] Initialized with Triangle Defense integration";
}

CoachingPanel::~CoachingPanel() = default;

void CoachingPanel::setupUI() {
    setMinimumSize(800, 600);
    setWindowTitle("AMT Cleats - Sports Analysis");
    
    // Main layout
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // Header with title and status
    QHBoxLayout* header_layout = new QHBoxLayout();
    
    QLabel* title = new QLabel("🏈 Triangle Defense Analysis", this);
    title->setStyleSheet(
        "font-size: 18px; font-weight: bold; color: #00d4ff; padding: 10px;"
    );
    header_layout->addWidget(title);
    
    header_layout->addStretch();
    
    m_status_label = new QLabel("Ready", this);
    m_status_label->setStyleSheet("color: #4a7c3a; font-weight: bold;");
    header_layout->addWidget(m_status_label);
    
    m_fps_label = new QLabel("FPS: --", this);
    m_fps_label->setStyleSheet("color: #a1a1a1; margin-left: 10px;");
    header_layout->addWidget(m_fps_label);
    
    main_layout->addLayout(header_layout);
    
    // Main content splitter
    QSplitter* main_splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left side: Formation diagram and controls
    QWidget* left_widget = new QWidget();
    QVBoxLayout* left_layout = new QVBoxLayout(left_widget);
    
    m_formation_diagram = new FormationDiagramWidget(this);
    left_layout->addWidget(m_formation_diagram);
    
    m_triangle_controls = new TriangleDefenseControls(this);
    left_layout->addWidget(m_triangle_controls);
    
    main_splitter->addWidget(left_widget);
    
    // Right side: Coaching insights
    m_insights_widget = new CoachingInsightsWidget(this);
    main_splitter->addWidget(m_insights_widget);
    
    // Set splitter proportions
    main_splitter->setSizes({500, 300});
    main_layout->addWidget(main_splitter);
    
    // Bottom controls
    QHBoxLayout* controls_layout = new QHBoxLayout();
    
    m_start_analysis_button = new QPushButton("▶ Start Analysis", this);
    m_start_analysis_button->setStyleSheet(
        "QPushButton {"
        "   background-color: #4a7c3a;"
        "   color: #ffffff;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #5a8c4a;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3a6c2a;"
        "}"
    );
    controls_layout->addWidget(m_start_analysis_button);
    
    m_export_clip_button = new QPushButton("📋 Export Clip", this);
    m_export_clip_button->setStyleSheet(
        "QPushButton {"
        "   background-color: #2d3748;"
        "   color: #ffffff;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3d4758;"
        "}"
    );
    controls_layout->addWidget(m_export_clip_button);
    
    m_mel_ai_sync_button = new QPushButton("🤖 M.E.L. AI Sync", this);
    m_mel_ai_sync_button->setStyleSheet(
        "QPushButton {"
        "   background-color: #00d4ff;"
        "   color: #000000;"
        "   border: none;"
        "   padding: 8px 16px;"
        "   border-radius: 4px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #00b4df;"
        "}"
    );
    controls_layout->addWidget(m_mel_ai_sync_button);
    
    controls_layout->addStretch();
    
    m_analysis_progress = new QProgressBar(this);
    m_analysis_progress->setVisible(false);
    controls_layout->addWidget(m_analysis_progress);
    
    main_layout->addLayout(controls_layout);
    
    // Apply dark theme
    setStyleSheet(
        "QWidget {"
        "   background-color: #1a1f2e;"
        "   color: #ffffff;"
        "}"
    );
}

void CoachingPanel::setupConnections() {
    connect(m_start_analysis_button, &QPushButton::clicked, 
            this, [this]() {
                if (m_analysis_timer->isActive()) {
                    stopRealTimeAnalysis();
                } else {
                    startRealTimeAnalysis();
                }
            });
    
    connect(m_export_clip_button, &QPushButton::clicked,
            this, &CoachingPanel::onExportButtonClicked);
    
    connect(m_mel_ai_sync_button, &QPushButton::clicked,
            this, &CoachingPanel::onMELAISyncButtonClicked);
    
    // Connect Triangle Defense controls
    connect(m_triangle_controls, &TriangleDefenseControls::formationTypeSelected,
            this, [this](FormationType type) {
                qDebug() << "[Coaching Panel] Formation type selected:" 
                         << QString::fromStdString(formation_utils::formationToString(type));
                m_formation_diagram->highlightTriangleDefense(type);
            });
}

void CoachingPanel::startRealTimeAnalysis() {
    if (!m_sports_core || !m_formation_detector) {
        QMessageBox::warning(this, "Error", "Sports analysis components not initialized");
        return;
    }
    
    m_status_label->setText("Analyzing...");
    m_status_label->setStyleSheet("color: #00d4ff; font-weight: bold;");
    m_start_analysis_button->setText("⏸ Stop Analysis");
    m_analysis_progress->setVisible(true);
    
    // Start analysis timer (30 FPS)
    m_analysis_timer->start(33); // ~30 FPS
    m_ui_update_timer->start(100); // Update UI 10 times per second
    
    qDebug() << "[Coaching Panel] Real-time analysis started";
}

void CoachingPanel::stopRealTimeAnalysis() {
    m_analysis_timer->stop();
    m_ui_update_timer->stop();
    
    m_status_label->setText("Ready");
    m_status_label->setStyleSheet("color: #4a7c3a; font-weight: bold;");
    m_start_analysis_button->setText("▶ Start Analysis");
    m_analysis_progress->setVisible(false);
    
    qDebug() << "[Coaching Panel] Real-time analysis stopped";
}

void CoachingPanel::processCurrentFrame() {
    if (!m_current_frame.isNull() && m_formation_detector) {
        // Convert QImage to cv::Mat for processing
        #ifdef ENABLE_OPENCV_INTEGRATION
        cv::Mat frame(m_current_frame.height(), m_current_frame.width(), 
                     CV_8UC4, (void*)m_current_frame.constBits(), 
                     m_current_frame.bytesPerLine());
        cv::cvtColor(frame, frame, cv::COLOR_BGRA2BGR);
        
        // Detect formation
        FormationData formation = m_formation_detector->detectFormation(frame);
        
        // Update display
        onFormationDetected(formation);
        #endif
    }
}

void CoachingPanel::updateRealTimeDisplay() {
    if (m_formation_detector) {
        double fps = m_formation_detector->getProcessingFPS();
        m_fps_label->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
        
        int progress = static_cast<int>(fps * 100 / 30.0); // Target 30 FPS
        m_analysis_progress->setValue(qMin(progress, 100));
    }
}

void CoachingPanel::onFormationDetected(const FormationData& formation) {
    m_current_formation = formation;
    
    // Update formation diagram
    m_formation_diagram->updateFormation(formation);
    
    // Generate coaching insights
    if (m_sports_core) {
        auto insights = m_sports_core->generateCoachingInsights(formation);
        m_insights_widget->updateInsights(insights);
        
        // Perform CLS analysis
        CLSAnalysis cls = m_sports_core->performCLSAnalysis(formation);
        m_insights_widget->showCLSAnalysis(cls);
    }
    
    // Emit signal for timeline integration
    emit formationMarkerRequested(m_current_timestamp, formation.type);
}

void CoachingPanel::onMELAISyncButtonClicked() {
    if (m_sports_core && m_formation_detector) {
        m_sports_core->connectToMELAI();
        m_formation_detector->connectToMELAI();
        
        QMessageBox::information(this, "M.E.L. AI", 
            "Connected to M.E.L. AI Master Intelligence System\n"
            "Empire coordination active for Triangle Defense analysis");
    }
}

// TriangleDefenseControls Implementation
TriangleDefenseControls::TriangleDefenseControls(QWidget* parent)
    : QGroupBox("Triangle Defense Controls", parent) {
    
    setStyleSheet(
        "QGroupBox {"
        "   font-weight: bold;"
        "   color: #00d4ff;"
        "   border: 1px solid #2d3748;"
        "   border-radius: 4px;"
        "   margin-top: 8px;"
        "   padding-top: 8px;"
        "}"
    );
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Formation selection
    QLabel* formation_label = new QLabel("Formation Type:", this);
    layout->addWidget(formation_label);
    
    m_formation_combo = new QComboBox(this);
    m_formation_combo->addItems({
        "Larry (Standard 5-3-1)",
        "Linda (Pass Coverage)",
        "Rita (Run Stopping)",
        "Ricky (Blitz Package)",
        "Leon (Balanced)",
        "Randy (Red Zone)",
        "Pat (Special Situations)"
    });
    layout->addWidget(m_formation_combo);
    
    // Current formation display
    m_current_formation = new QLabel("Current: Larry", this);
    m_current_formation->setStyleSheet("color: #4a7c3a; font-weight: bold;");
    layout->addWidget(m_current_formation);
    
    // Analysis controls
    m_analysis_button = new QPushButton("Enable Analysis", this);
    layout->addWidget(m_analysis_button);
    
    // M.E.L. AI status
    m_mel_ai_status = new QLabel("M.E.L. AI: Disconnected", this);
    m_mel_ai_status->setStyleSheet("color: #e2021a;");
    layout->addWidget(m_mel_ai_status);
    
    m_mel_ai_button = new QPushButton("Connect M.E.L. AI", this);
    layout->addWidget(m_mel_ai_button);
    
    // Detection threshold
    QLabel* threshold_label = new QLabel("Detection Threshold:", this);
    layout->addWidget(threshold_label);
    
    m_threshold_slider = new QSlider(Qt::Horizontal, this);
    m_threshold_slider->setRange(50, 95);
    m_threshold_slider->setValue(70);
    layout->addWidget(m_threshold_slider);
    
    // Connect signals
    connect(m_formation_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TriangleDefenseControls::onFormationComboChanged);
    connect(m_analysis_button, &QPushButton::clicked,
            this, &TriangleDefenseControls::onAnalysisToggled);
    connect(m_mel_ai_button, &QPushButton::clicked,
            this, &TriangleDefenseControls::onMELAIButtonClicked);
    connect(m_threshold_slider, &QSlider::valueChanged,
            this, &TriangleDefenseControls::onThresholdSliderChanged);
}

TriangleDefenseControls::~TriangleDefenseControls() = default;

void TriangleDefenseControls::onFormationDetected(const FormationData& formation) {
    QString formation_name = QString::fromStdString(
        formation_utils::formationToString(formation.type));
    m_current_formation->setText("Current: " + formation_name);
    
    // Update combo box to match detected formation
    int index = static_cast<int>(formation.type);
    if (index >= 0 && index < m_formation_combo->count()) {
        m_formation_combo->setCurrentIndex(index);
    }
}

void TriangleDefenseControls::onMELAIConnected(bool connected) {
    if (connected) {
        m_mel_ai_status->setText("M.E.L. AI: Connected");
        m_mel_ai_status->setStyleSheet("color: #4a7c3a; font-weight: bold;");
        m_mel_ai_button->setText("Disconnect M.E.L. AI");
    } else {
        m_mel_ai_status->setText("M.E.L. AI: Disconnected");
        m_mel_ai_status->setStyleSheet("color: #e2021a;");
        m_mel_ai_button->setText("Connect M.E.L. AI");
    }
}

void TriangleDefenseControls::onFormationComboChanged() {
    FormationType type = static_cast<FormationType>(m_formation_combo->currentIndex());
    emit formationTypeSelected(type);
}

void TriangleDefenseControls::onAnalysisToggled() {
    static bool enabled = false;
    enabled = !enabled;
    
    emit analysisEnabled(enabled);
    m_analysis_button->setText(enabled ? "Disable Analysis" : "Enable Analysis");
}

void TriangleDefenseControls::onMELAIButtonClicked() {
    // Toggle M.E.L. AI connection
    bool currently_connected = m_mel_ai_status->text().contains("Connected");
    onMELAIConnected(!currently_connected);
}

void TriangleDefenseControls::onThresholdSliderChanged() {
    double threshold = m_threshold_slider->value() / 100.0;
    emit detectionThresholdChanged(threshold);
}

} // namespace sports
} // namespace amt

#include "coaching_panel.moc"
