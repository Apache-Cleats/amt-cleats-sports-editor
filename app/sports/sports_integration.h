/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Sports Integration Main Coordinator
  Central hub for Triangle Defense, M.E.L. pipeline, and video timeline coordination
***/

#ifndef SPORTSINTEGRATION_H
#define SPORTSINTEGRATION_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QQueue>
#include <QMap>
#include <QStringList>
#include <QSettings>
#include <QNetworkAccessManager>

#include "superset_panel.h"
#include "kafka_publisher.h"
#include "triangle_defense_sync.h"
#include "minio_client.h"

namespace olive {

// Forward declarations
class VideoTimelineSync;
class FormationOverlay;
class CoachingAlertWidget;
class TimelinePanel;
class SequenceViewerPanel;

/**
 * @brief Integration state enumeration
 */
enum class IntegrationState {
  Disconnected,
  Connecting,
  Connected,
  Synchronizing,
  Error,
  Maintenance
};

/**
 * @brief Sports analysis mode
 */
enum class AnalysisMode {
  RealTime,       // Live analysis during playback
  Batch,          // Analyze entire video offline
  Manual,         // Manual formation marking
  Hybrid          // Combination of real-time and manual
};

/**
 * @brief Video processing stage
 */
enum class ProcessingStage {
  VideoUpload,
  MetadataExtraction,
  FormationDetection,
  MELProcessing,
  TriangleAnalysis,
  FabricatorGeneration,
  DashboardSync,
  Complete
};

/**
 * @brief System health status
 */
struct SystemHealth {
  bool kafka_connected;
  bool supabase_connected;
  bool minio_connected;
  bool superset_accessible;
  bool mel_pipeline_active;
  double cpu_usage;
  double memory_usage;
  qint64 disk_space_available;
  int active_formations;
  int pending_alerts;
  QString last_error;
  QDateTime last_health_check;
  
  SystemHealth() : kafka_connected(false), supabase_connected(false),
                   minio_connected(false), superset_accessible(false),
                   mel_pipeline_active(false), cpu_usage(0.0), memory_usage(0.0),
                   disk_space_available(0), active_formations(0), pending_alerts(0),
                   last_health_check(QDateTime::currentDateTime()) {}
};

/**
 * @brief Processing pipeline status
 */
struct PipelineStatus {
  QString video_id;
  ProcessingStage current_stage;
  double completion_percentage;
  QDateTime start_time;
  QDateTime estimated_completion;
  QStringList completed_stages;
  QStringList failed_stages;
  QString current_operation;
  QJsonObject stage_metrics;
  
  PipelineStatus() : current_stage(ProcessingStage::VideoUpload),
                     completion_percentage(0.0),
                     start_time(QDateTime::currentDateTime()) {}
};

/**
 * @brief Main sports integration coordinator
 */
class SportsIntegration : public QObject
{
  Q_OBJECT

public:
  explicit SportsIntegration(QObject* parent = nullptr);
  virtual ~SportsIntegration();

  /**
   * @brief Initialize sports integration system
   */
  bool Initialize(const QJsonObject& configuration = QJsonObject());

  /**
   * @brief Shutdown sports integration gracefully
   */
  void Shutdown();

  /**
   * @brief Connect video editor panels
   */
  void ConnectTimelinePanel(TimelinePanel* timeline_panel);
  void ConnectSequenceViewerPanel(SequenceViewerPanel* viewer_panel);

  /**
   * @brief Get integration components
   */
  SupersetPanel* GetSupersetPanel() const { return superset_panel_; }
  KafkaPublisher* GetKafkaPublisher() const { return kafka_publisher_; }
  TriangleDefenseSync* GetTriangleDefenseSync() const { return triangle_defense_sync_; }
  MinIOClient* GetMinIOClient() const { return minio_client_; }
  VideoTimelineSync* GetVideoTimelineSync() const { return video_timeline_sync_; }
  FormationOverlay* GetFormationOverlay() const { return formation_overlay_; }
  CoachingAlertWidget* GetCoachingAlertWidget() const { return coaching_alert_widget_; }

  /**
   * @brief Load video for analysis
   */
  QString LoadVideoForAnalysis(const QString& video_path, const QJsonObject& metadata = QJsonObject());

  /**
   * @brief Start real-time analysis
   */
  bool StartRealTimeAnalysis(const QString& video_id);

  /**
   * @brief Stop real-time analysis
   */
  void StopRealTimeAnalysis();

  /**
   * @brief Process video in batch mode
   */
  QString ProcessVideoBatch(const QString& video_path, AnalysisMode mode = AnalysisMode::Batch);

  /**
   * @brief Get current system health
   */
  SystemHealth GetSystemHealth() const;

  /**
   * @brief Get pipeline status for video
   */
  PipelineStatus GetPipelineStatus(const QString& video_id) const;

  /**
   * @brief Get integration state
   */
  IntegrationState GetIntegrationState() const { return integration_state_; }

  /**
   * @brief Get comprehensive statistics
   */
  QJsonObject GetIntegrationStatistics() const;

  /**
   * @brief Force system health check
   */
  void PerformHealthCheck();

  /**
   * @brief Get active video session info
   */
  QJsonObject GetActiveVideoSession() const;

  /**
   * @brief Export analysis results
   */
  bool ExportAnalysisResults(const QString& video_id, const QString& export_path,
                            const QStringList& export_formats = QStringList());

  /**
   * @brief Import formation data
   */
  bool ImportFormationData(const QString& import_path, const QString& video_id);

public slots:
  /**
   * @brief Handle video playback events
   */
  void OnVideoLoaded(const QString& video_path);
  void OnVideoPlaybackStarted();
  void OnVideoPlaybackStopped();
  void OnVideoPositionChanged(qint64 position);
  void OnVideoSeekPerformed(qint64 position);
  void OnVideoRateChanged(double rate);

  /**
   * @brief Handle formation events
   */
  void OnFormationDetected(const FormationData& formation);
  void OnManualFormationMarked(qint64 timestamp, FormationType type);
  void OnFormationDeleted(const QString& formation_id);

  /**
   * @brief Handle coaching events
   */
  void OnCoachingAlertGenerated(const CoachingAlert& alert);
  void OnCoachingAlertAcknowledged(const QString& alert_id);
  void OnTriangleCallMade(TriangleCall call, const FormationData& formation);

  /**
   * @brief Handle system events
   */
  void OnSystemError(const QString& component, const QString& error);
  void OnComponentStatusChanged(const QString& component, bool connected);
  void OnConfigurationChanged(const QJsonObject& new_config);

  /**
   * @brief Handle data synchronization
   */
  void OnDataSyncRequired();
  void OnDataSyncCompleted();
  void OnDataSyncFailed(const QString& error);

signals:
  /**
   * @brief Integration status signals
   */
  void IntegrationStateChanged(IntegrationState state);
  void ComponentConnected(const QString& component);
  void ComponentDisconnected(const QString& component);
  void SystemHealthUpdated(const SystemHealth& health);

  /**
   * @brief Video processing signals
   */
  void VideoLoadStarted(const QString& video_id);
  void VideoLoadCompleted(const QString& video_id);
  void VideoLoadFailed(const QString& video_id, const QString& error);
  void ProcessingStageChanged(const QString& video_id, ProcessingStage stage);
  void ProcessingCompleted(const QString& video_id);
  void ProcessingFailed(const QString& video_id, const QString& error);

  /**
   * @brief Analysis signals
   */
  void RealTimeAnalysisStarted(const QString& video_id);
  void RealTimeAnalysisStopped(const QString& video_id);
  void FormationAnalysisCompleted(const QString& video_id, int formation_count);
  void TriangleDefenseAnalysisCompleted(const QString& video_id, const QJsonObject& results);

  /**
   * @brief Alert and notification signals
   */
  void CriticalAlertGenerated(const CoachingAlert& alert);
  void SystemWarningIssued(const QString& warning);
  void PerformanceIssueDetected(const QString& issue);

  /**
   * @brief Data synchronization signals
   */
  void DataSyncStarted();
  void DataSyncProgress(int percentage);
  void DataSyncFinished(bool success);

private slots:
  void OnHealthCheckTimer();
  void OnStatsUpdateTimer();
  void OnMaintenanceTimer();
  void OnVideoUploadCompleted(const VideoMetadata& metadata);
  void OnFormationProcessingCompleted(const QString& formation_id);
  void OnMELPipelineCompleted(const QString& formation_id, const MELResult& results);
  void OnFabricatorResultGenerated(const FabricatorResult& result);

private:
  void SetupComponents();
  void SetupTimers();
  void LoadConfiguration();
  void SaveConfiguration();
  void ConnectComponentSignals();
  void InitializeAnalysisPipeline();
  void SetupHealthMonitoring();
  
  void UpdateIntegrationState(IntegrationState new_state);
  void UpdateSystemHealth();
  void UpdatePipelineStatus(const QString& video_id, ProcessingStage stage, double progress = 0.0);
  void ProcessVideoStage(const QString& video_id, ProcessingStage stage);
  void HandleComponentError(const QString& component, const QString& error);
  void PerformAutomaticMaintenance();
  
  bool ValidateSystemRequirements();
  bool TestComponentConnections();
  void StartComponentMonitoring();
  void StopComponentMonitoring();
  void CleanupResources();
  void OptimizePerformance();

  // Core components
  SupersetPanel* superset_panel_;
  KafkaPublisher* kafka_publisher_;
  TriangleDefenseSync* triangle_defense_sync_;
  MinIOClient* minio_client_;
  VideoTimelineSync* video_timeline_sync_;
  FormationOverlay* formation_overlay_;
  CoachingAlertWidget* coaching_alert_widget_;
  
  // Video editor panel connections
  TimelinePanel* timeline_panel_;
  SequenceViewerPanel* viewer_panel_;
  
  // System state
  IntegrationState integration_state_;
  AnalysisMode current_analysis_mode_;
  bool is_initialized_;
  bool real_time_analysis_active_;
  QString active_video_id_;
  qint64 current_video_position_;
  
  // Configuration
  QJsonObject configuration_;
  QSettings* settings_;
  QString config_file_path_;
  
  // Health monitoring
  SystemHealth system_health_;
  QTimer* health_check_timer_;
  QTimer* stats_update_timer_;
  QTimer* maintenance_timer_;
  
  // Pipeline tracking
  mutable QMutex pipeline_mutex_;
  QMap<QString, PipelineStatus> active_pipelines_;
  QQueue<QString> processing_queue_;
  int max_concurrent_pipelines_;
  
  // Statistics tracking
  mutable QMutex stats_mutex_;
  struct IntegrationStats {
    qint64 videos_processed;
    qint64 formations_detected;
    qint64 triangle_calls_made;
    qint64 coaching_alerts_generated;
    qint64 uptime_seconds;
    double average_processing_time;
    double system_performance_score;
    QDateTime start_time;
    
    IntegrationStats() : videos_processed(0), formations_detected(0),
                         triangle_calls_made(0), coaching_alerts_generated(0),
                         uptime_seconds(0), average_processing_time(0.0),
                         system_performance_score(0.0),
                         start_time(QDateTime::currentDateTime()) {}
  } stats_;
  
  // Error handling and resilience
  QStringList recent_errors_;
  QTimer* error_recovery_timer_;
  int consecutive_failures_;
  bool automatic_recovery_enabled_;
  
  // Performance optimization
  QThread* background_thread_;
  bool performance_monitoring_enabled_;
  QTimer* performance_optimization_timer_;
  
  // Networking for external integrations
  QNetworkAccessManager* network_manager_;
  
  // Component status tracking
  QMap<QString, bool> component_status_;
  QMap<QString, QDateTime> last_component_check_;
  
  // Data synchronization
  bool data_sync_in_progress_;
  QTimer* data_sync_timer_;
  int data_sync_interval_minutes_;
};

/**
 * @brief Configuration helper class
 */
class SportsIntegrationConfig
{
public:
  static QJsonObject GetDefaultConfiguration();
  static bool ValidateConfiguration(const QJsonObject& config);
  static QJsonObject MergeConfigurations(const QJsonObject& base, const QJsonObject& overlay);
  static void SaveConfiguration(const QJsonObject& config, const QString& file_path);
  static QJsonObject LoadConfiguration(const QString& file_path);
  
  // Configuration keys
  static const QString SUPABASE_URL;
  static const QString SUPABASE_API_KEY;
  static const QString KAFKA_BOOTSTRAP_SERVERS;
  static const QString MINIO_ENDPOINT;
  static const QString MINIO_ACCESS_KEY;
  static const QString MINIO_SECRET_KEY;
  static const QString SUPERSET_BASE_URL;
  static const QString WEBSOCKET_URL;
  
  // Feature flags
  static const QString ENABLE_REAL_TIME_ANALYSIS;
  static const QString ENABLE_GPU_ACCELERATION;
  static const QString ENABLE_CLOUD_SYNC;
  static const QString ENABLE_TELEMETRY;
  static const QString ENABLE_AUTO_RECOVERY;
  
  // Performance settings
  static const QString MAX_CONCURRENT_PIPELINES;
  static const QString HEALTH_CHECK_INTERVAL;
  static const QString DATA_SYNC_INTERVAL;
  static const QString CACHE_SIZE_LIMIT;
  static const QString PROCESSING_TIMEOUT;
};

/**
 * @brief System requirements checker
 */
class SystemRequirementsChecker
{
public:
  struct Requirements {
    QString component;
    QString requirement;
    QString current_value;
    bool satisfied;
    QString recommendation;
  };
  
  static QList<Requirements> CheckAllRequirements();
  static bool CheckMinimumRequirements();
  static QStringList GetRecommendations();
  static QJsonObject GetSystemInfo();
  
private:
  static Requirements CheckCPU();
  static Requirements CheckMemory();
  static Requirements CheckDiskSpace();
  static Requirements CheckNetworkConnectivity();
  static Requirements CheckVideoCodecs();
  static Requirements CheckGPU();
  static Requirements CheckDependencies();
};

/**
 * @brief Utility functions for sports integration
 */
namespace SportsIntegrationUtils {

/**
 * @brief Format system performance metrics for display
 */
QString FormatPerformanceMetrics(const QJsonObject& metrics);

/**
 * @brief Calculate system performance score
 */
double CalculatePerformanceScore(const SystemHealth& health);

/**
 * @brief Generate unique video session ID
 */
QString GenerateVideoSessionId();

/**
 * @brief Estimate processing time for video
 */
int EstimateProcessingTime(const VideoMetadata& metadata, AnalysisMode mode);

/**
 * @brief Validate video compatibility
 */
bool IsVideoCompatible(const QString& video_path);

/**
 * @brief Get optimal analysis settings
 */
QJsonObject GetOptimalAnalysisSettings(const VideoMetadata& metadata);

/**
 * @brief Create processing pipeline configuration
 */
QJsonObject CreatePipelineConfig(const QString& video_id, AnalysisMode mode);

/**
 * @brief Calculate resource requirements
 */
QJsonObject CalculateResourceRequirements(const QList<QString>& active_videos);

} // namespace SportsIntegrationUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::IntegrationState)
Q_DECLARE_METATYPE(olive::AnalysisMode)
Q_DECLARE_METATYPE(olive::ProcessingStage)
Q_DECLARE_METATYPE(olive::SystemHealth)
Q_DECLARE_METATYPE(olive::PipelineStatus)

#endif // SPORTSINTEGRATION_H
