#ifndef SPORTS_INTEGRATION_H
#define SPORTS_INTEGRATION_H

/**
 * @file sports_integration.h
 * @brief Main integration interface for AMT Cleats Sports Analysis with Olive Video Editor
 * 
 * This file provides the primary integration point between Olive's video editing
 * capabilities and AMT Cleats sports analysis features including Triangle Defense
 * methodology, M.E.L. AI coordination, and coaching workflow tools.
 * 
 * Part of AnalyzeMyTeam (Company 01) - Flagship Football Intelligence Empire
 * Integration with Olive Video Editor for professional sports video analysis.
 */

#include <QObject>
#include <QWidget>
#include <QDockWidget>
#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <QImage>
#include <memory>

#include "sports_analysis_core.h"
#include "formation_detector.h"
#include "coaching_panel.h"

// Forward declarations for Olive integration
class QMenuBar;
class QMenu;
class QAction;
class QToolBar;
class QStatusBar;

namespace amt {
namespace sports {

/**
 * @class SportsIntegration
 * @brief Main integration class connecting AMT Cleats sports analysis to Olive
 */
class SportsIntegration : public QObject {
    Q_OBJECT

public:
    explicit SportsIntegration(QMainWindow* olive_main_window, QObject* parent = nullptr);
    ~SportsIntegration();
    
    // Initialization and Setup
    bool initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Integration with Olive Components
    void connectToVideoPlayer(QObject* video_player);
    void connectToTimeline(QObject* timeline);
    void connectToProject(QObject* project);
    void connectToSequence(QObject* sequence);
    
    // Sports Analysis Core Access
    SportsAnalysisCore* getSportsCore() const { return m_sports_core.get(); }
    FormationDetector* getFormationDetector() const { return m_formation_detector.get(); }
    CoachingPanel* getCoachingPanel() const { return m_coaching_panel; }
    
    // UI Integration
    void addSportsMenus();
    void addSportsToolbars();
    void addSportsDockWidgets();
    void removeSportsUI();
    
    // Video Processing Integration
    void processVideoFrame(const QImage& frame, double timestamp);
    void onVideoLoaded(const QString& filename);
    void onVideoPositionChanged(double seconds);
    void onVideoPlayStateChanged(bool playing);
    
    // Timeline Integration
    void addFormationMarker(double timestamp, FormationType formation);
    void removeFormationMarker(double timestamp);
    void exportFormationTimeline(const QString& filename);
    
    // Export and Import
    void exportCoachingClip(double start_time, double end_time, const QString& filename);
    void exportTacticalDiagram(const QString& filename);
    void exportCoachingReport(const QString& filename);
    void importFormationData(const QString& filename);
    
    // Configuration
    void loadSportsSettings();
    void saveSportsSettings();
    void resetSportsSettings();
    
    // M.E.L. AI Empire Integration
    void connectToMELAI();
    void syncWithAnalyzeMyTeam();
    void sendDataToEmpire(const FormationData& formation);
    
    // Performance Monitoring
    double getAnalysisFPS() const;
    int getTotalFormationsDetected() const;
    void resetPerformanceCounters();

public slots:
    // Video Processing Slots
    void onFrameChanged(const QImage& frame);
    void onTimelinePositionChanged(double seconds);
    void onVideoFileLoaded(const QString& filename);
    
    // Sports Analysis Slots
    void onFormationDetected(const FormationData& formation);
    void onPlayerPositionsUpdated(const std::vector<PlayerPosition>& players);
    void onCLSAnalysisComplete(const CLSAnalysis& cls);
    
    // UI Slots
    void onSportsAnalysisToggled(bool enabled);
    void onCoachingPanelRequested();
    void onFormationReportRequested();
    void onMELAISyncRequested();
    void onTriangleDefenseHelpRequested();

signals:
    // Sports Analysis Signals
    void formationDetected(const FormationData& formation);
    void coachingInsightGenerated(const QString& insight);
    void melAIConnected(bool connected);
    void analysisPerformanceUpdated(double fps, int formations_detected);
    
    // Integration Signals
    void timelineMarkerRequested(double timestamp, int marker_type);
    void exportRequested(const QString& type, const QString& filename);
    void settingsChanged();

private slots:
    void updateAnalysisDisplay();
    void processRealTimeAnalysis();
    void handleSportsMenuAction();
    void handleToolbarAction();

private:
    void setupSportsComponents();
    void setupUIIntegration();
    void setupSignalConnections();
    void createSportsMenus();
    void createSportsToolbars();
    void createSportsDockWidgets();
    
    // Olive Integration Helpers
    QAction* createSportsAction(const QString& text, const QString& tooltip, 
                               const QString& icon_path = QString());
    void integrateSportsTimeline();
    void setupVideoProcessingPipeline();
    
    // Sports Analysis Helpers
    void initializeSportsAnalysis();
    void startRealTimeProcessing();
    void stopRealTimeProcessing();
    
    // Empire Integration Helpers
    void initializeEmpireConnection();
    void sendFormationToEmpire(const FormationData& formation);
    void receiveEmpireRecommendations();
    
    // Olive Main Window Reference
    QMainWindow* m_olive_main_window;
    
    // Sports Analysis Components
    std::unique_ptr<SportsAnalysisCore> m_sports_core;
    std::unique_ptr<FormationDetector> m_formation_detector;
    CoachingPanel* m_coaching_panel;
    
    // UI Integration Components
    QDockWidget* m_coaching_dock;
    QMenu* m_sports_menu;
    QToolBar* m_sports_toolbar;
    QAction* m_toggle_analysis_action;
    QAction* m_coaching_panel_action;
    QAction* m_formation_report_action;
    QAction* m_mel_ai_sync_action;
    QAction* m_triangle_defense_help_action;
    
    // Video Integration
    QObject* m_video_player;
    QObject* m_timeline;
    QObject* m_project;
    QObject* m_sequence;
    
    // Real-time Processing
    QTimer* m_analysis_timer;
    QTimer* m_ui_update_timer;
    bool m_real_time_analysis_enabled;
    
    // Current Analysis State
    QImage m_current_frame;
    double m_current_timestamp;
    FormationData m_current_formation;
    std::vector<PlayerPosition> m_current_players;
    
    // Performance Tracking
    int m_total_formations_detected;
    double m_analysis_fps;
    bool m_mel_ai_connected;
    
    // Settings
    QString m_settings_file_path;
    bool m_auto_start_analysis;
    bool m_show_formation_overlays;
    double m_detection_threshold;
    
    // Initialization State
    bool m_initialized;
};

/**
 * @class SportsTimelineIntegration
 * @brief Specialized class for integrating sports markers with Olive's timeline
 */
class SportsTimelineIntegration : public QObject {
    Q_OBJECT

public:
    explicit SportsTimelineIntegration(QObject* olive_timeline, QObject* parent = nullptr);
    ~SportsTimelineIntegration();
    
    void addFormationMarker(double timestamp, FormationType formation, double confidence);
    void removeFormationMarker(double timestamp);
    void clearAllFormationMarkers();
    
    void addPlayBoundary(double start_time, double end_time, const QString& play_name);
    void addCoachingNote(double timestamp, const QString& note);
    
    // Export timeline data
    void exportTimelineData(const QString& filename);
    void importTimelineData(const QString& filename);

public slots:
    void onTimelinePositionChanged(double seconds);
    void onFormationDetected(double timestamp, FormationType formation);

signals:
    void markerClicked(double timestamp, FormationType formation);
    void playBoundarySelected(double start_time, double end_time);
    void coachingNoteRequested(double timestamp);

private:
    QObject* m_olive_timeline;
    struct TimelineMarker {
        double timestamp;
        FormationType formation;
        double confidence;
        QString note;
    };
    std::vector<TimelineMarker> m_formation_markers;
    
    struct PlayBoundary {
        double start_time;
        double end_time;
        QString play_name;
        FormationType primary_formation;
    };
    std::vector<PlayBoundary> m_play_boundaries;
};

/**
 * @class SportsExportManager
 * @brief Handles all sports-specific export functionality
 */
class SportsExportManager : public QObject {
    Q_OBJECT

public:
    explicit SportsExportManager(SportsIntegration* integration, QObject* parent = nullptr);
    ~SportsExportManager();
    
    // Coaching Exports
    bool exportCoachingClip(double start_time, double end_time, const QString& filename);
    bool exportTacticalDiagram(const FormationData& formation, const QString& filename);
    bool exportFormationReport(const std::vector<FormationData>& formations, const QString& filename);
    bool exportCoachingPresentation(const QString& filename);
    
    // Data Exports
    bool exportFormationTimeline(const QString& filename);
    bool exportPlayerStatistics(const QString& filename);
    bool exportCLSAnalysisReport(const QString& filename);
    
    // Empire Integration Exports
    bool exportToAnalyzeMyTeam(const QString& filename);
    bool exportToMELAI(const QString& filename);
    
    // Import Functions
    bool importFormationLibrary(const QString& filename);
    bool importCoachingTemplates(const QString& filename);

private:
    SportsIntegration* m_integration;
    QString m_default_export_path;
    
    // Export Helpers
    QString generateCoachingReportHTML(const std::vector<FormationData>& formations);
    QImage renderTacticalDiagram(const FormationData& formation);
    QString formatFormationTimeline(const std::vector<FormationData>& formations);
};

// Global Integration Functions
namespace integration {
    /**
     * @brief Initialize AMT Cleats sports integration with Olive
     * @param olive_main_window Pointer to Olive's main window
     * @return Pointer to SportsIntegration instance or nullptr on failure
     */
    SportsIntegration* initializeAMTCleats(QMainWindow* olive_main_window);
    
    /**
     * @brief Shutdown sports integration and cleanup resources
     * @param integration Pointer to SportsIntegration instance
     */
    void shutdownAMTCleats(SportsIntegration* integration);
    
    /**
     * @brief Check if AMT Cleats sports features are available
     * @return True if sports analysis is enabled and functional
     */
    bool isSportsAnalysisAvailable();
    
    /**
     * @brief Get version information for AMT Cleats integration
     * @return Version string with build information
     */
    QString getAMTCleatsVersion();
}

// Utility Macros for Integration
#ifdef ENABLE_SPORTS_ANALYSIS
    #define AMT_SPORTS_ENABLED true
    #define AMT_INITIALIZE_SPORTS(main_window) amt::sports::integration::initializeAMTCleats(main_window)
    #define AMT_SHUTDOWN_SPORTS(integration) amt::sports::integration::shutdownAMTCleats(integration)
#else
    #define AMT_SPORTS_ENABLED false
    #define AMT_INITIALIZE_SPORTS(main_window) nullptr
    #define AMT_SHUTDOWN_SPORTS(integration) do {} while(0)
#endif

} // namespace sports
} // namespace amt

#endif // SPORTS_INTEGRATION_H
