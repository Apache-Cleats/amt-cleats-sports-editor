#ifndef COACHING_PANEL_H
#define COACHING_PANEL_H

/**
 * @file coaching_panel.h
 * @brief Sports coaching interface panel for Olive video editor
 * 
 * Integrates Triangle Defense analysis with Olive's Qt-based UI system.
 * Provides real-time coaching insights, formation overlays, and tactical analysis.
 * 
 * Part of AMT Cleats Sports Editor - connecting video editing with football intelligence.
 */

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QTextEdit>
#include <QListWidget>
#include <QGroupBox>
#include <QProgressBar>
#include <QTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include "sports_analysis_core.h"
#include "formation_detector.h"

namespace amt {
namespace sports {

/**
 * @class FormationDiagramWidget
 * @brief Visual diagram showing detected formations and Triangle Defense setup
 */
class FormationDiagramWidget : public QGraphicsView {
    Q_OBJECT

public:
    explicit FormationDiagramWidget(QWidget* parent = nullptr);
    ~FormationDiagramWidget();
    
    void updateFormation(const FormationData& formation);
    void setPlayerPositions(const std::vector<PlayerPosition>& players);
    void highlightTriangleDefense(FormationType type);
    
    // Visual Configuration
    void setFieldDimensions(int width, int height);
    void enablePlayerNumbers(bool enabled);
    void setTeamColors(const QColor& offense, const QColor& defense);

public slots:
    void clearDiagram();
    void exportDiagram(const QString& filename);

signals:
    void formationClicked(FormationType type);
    void playerSelected(int jersey_number);

private:
    class DiagramImpl;
    std::unique_ptr<DiagramImpl> m_diagram_impl;
    
    void drawField();
    void drawPlayers();
    void drawFormationLines();
    void updateTriangleDefenseOverlay();
};

/**
 * @class CoachingInsightsWidget  
 * @brief Real-time coaching recommendations and analysis display
 */
class CoachingInsightsWidget : public QWidget {
    Q_OBJECT

public:
    explicit CoachingInsightsWidget(QWidget* parent = nullptr);
    ~CoachingInsightsWidget();
    
    void updateInsights(const std::vector<std::string>& insights);
    void addFormationAlert(const QString& message);
    void showCLSAnalysis(const CLSAnalysis& cls);

public slots:
    void clearInsights();
    void exportInsights();

private:
    QTextEdit* m_insights_display;
    QListWidget* m_alerts_list;
    QLabel* m_cls_score;
    QProgressBar* m_confidence_bar;
};

/**
 * @class TriangleDefenseControls
 * @brief Control panel for Triangle Defense configuration and analysis
 */
class TriangleDefenseControls : public QGroupBox {
    Q_OBJECT

public:
    explicit TriangleDefenseControls(QWidget* parent = nullptr);
    ~TriangleDefenseControls();

public slots:
    void onFormationDetected(const FormationData& formation);
    void onMELAIConnected(bool connected);

signals:
    void formationTypeSelected(FormationType type);
    void analysisEnabled(bool enabled);
    void detectionThresholdChanged(double threshold);

private slots:
    void onFormationComboChanged();
    void onAnalysisToggled();
    void onThresholdSliderChanged();
    void onMELAIButtonClicked();

private:
    QComboBox* m_formation_combo;
    QPushButton* m_analysis_button;
    QPushButton* m_mel_ai_button;
    QSlider* m_threshold_slider;
    QLabel* m_current_formation;
    QLabel* m_mel_ai_status;
    QSpinBox* m_confidence_spin;
};

/**
 * @class CoachingPanel
 * @brief Main sports coaching panel integrating with Olive video editor
 */
class CoachingPanel : public QWidget {
    Q_OBJECT

public:
    explicit CoachingPanel(QWidget* parent = nullptr);
    ~CoachingPanel();
    
    // Integration with Olive
    void connectToVideoPlayer(QObject* video_player);
    void connectToTimeline(QObject* timeline);
    void setCurrentFrame(const QImage& frame);
    
    // Sports Analysis Integration
    void setSportsAnalysisCore(SportsAnalysisCore* core);
    void setFormationDetector(FormationDetector* detector);
    
    // Real-time Analysis
    void startRealTimeAnalysis();
    void stopRealTimeAnalysis();
    
    // Export Functions
    void exportCoachingClip(double start_time, double end_time);
    void exportFormationReport();
    void exportTacticalDiagram();

public slots:
    void onFrameChanged(const QImage& frame);
    void onTimelinePositionChanged(double seconds);
    void onVideoLoaded(const QString& filename);
    
    // Sports-specific slots
    void onFormationDetected(const FormationData& formation);
    void onPlayerPositionsUpdated(const std::vector<PlayerPosition>& players);
    void onCLSAnalysisComplete(const CLSAnalysis& cls);

signals:
    void coachingClipRequested(double start_time, double end_time);
    void formationMarkerRequested(double timestamp, FormationType type);
    void tacticalNoteRequested(const QString& note);

private slots:
    void processCurrentFrame();
    void updateRealTimeDisplay();
    void onExportButtonClicked();
    void onMELAISyncButtonClicked();

private:
    void setupUI();
    void setupConnections();
    void updateFormationDisplay();
    void updateCoachingInsights();
    
    // UI Components
    FormationDiagramWidget* m_formation_diagram;
    CoachingInsightsWidget* m_insights_widget;
    TriangleDefenseControls* m_triangle_controls;
    
    // Control Buttons
    QPushButton* m_start_analysis_button;
    QPushButton* m_export_clip_button;
    QPushButton* m_mel_ai_sync_button;
    QPushButton* m_formation_report_button;
    
    // Status Display
    QLabel* m_status_label;
    QLabel* m_fps_label;
    QProgressBar* m_analysis_progress;
    
    // Timers
    QTimer* m_analysis_timer;
    QTimer* m_ui_update_timer;
    
    // Sports Analysis Components
    SportsAnalysisCore* m_sports_core;
    FormationDetector* m_formation_detector;
    
    // Current Analysis Data
    FormationData m_current_formation;
    std::vector<PlayerPosition> m_current_players;
    CLSAnalysis m_current_cls;
    
    // Integration with Olive
    QObject* m_video_player;
    QObject* m_timeline;
    QImage m_current_frame;
    double m_current_timestamp;
};

/**
 * @class SportsTimelineMarker
 * @brief Custom timeline markers for formation changes and coaching points
 */
class SportsTimelineMarker : public QWidget {
    Q_OBJECT

public:
    explicit SportsTimelineMarker(FormationType type, double timestamp, QWidget* parent = nullptr);
    
    FormationType getFormationType() const { return m_formation_type; }
    double getTimestamp() const { return m_timestamp; }
    
    void setHighlighted(bool highlighted);
    void setCoachingNote(const QString& note);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

signals:
    void markerClicked(double timestamp);
    void markerRightClicked(double timestamp, FormationType type);

private:
    FormationType m_formation_type;
    double m_timestamp;
    QString m_coaching_note;
    bool m_highlighted;
    QColor m_marker_color;
};

} // namespace sports
} // namespace amt

#endif // COACHING_PANEL_H
