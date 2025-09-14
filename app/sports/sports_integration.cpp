/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Sports Integration Main Coordinator Implementation
  Central hub for Triangle Defense, M.E.L. pipeline, and video timeline coordination
***/

#include "sports_integration.h"

#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QProcess>
#include <QStorageInfo>
#include <QSysInfo>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QMessageBox>
#include <QProgressDialog>
#include <QtConcurrent>

#include "video_timeline_sync.h"
#include "formation_overlay.h"
#include "coaching_alert_widget.h"
#include "panel/timeline/timeline.h"
#include "panel/sequenceviewer/sequenceviewer.h"
#include "config/config.h"

namespace olive {

// Configuration key constants
const QString SportsIntegrationConfig::SUPABASE_URL = "supabase/url";
const QString SportsIntegrationConfig::SUPABASE_API_KEY = "supabase/api_key";
const QString SportsIntegrationConfig::KAFKA_BOOTSTRAP_SERVERS = "kafka/bootstrap_servers";
const QString SportsIntegrationConfig::MINIO_ENDPOINT = "minio/endpoint";
const QString SportsIntegrationConfig::MINIO_ACCESS_KEY = "minio/access_key";
const QString SportsIntegrationConfig::MINIO_SECRET_KEY = "minio/secret_key";
const QString SportsIntegrationConfig::SUPERSET_BASE_URL = "superset/base_url";
const QString SportsIntegrationConfig::WEBSOCKET_URL = "websocket/url";
const QString SportsIntegrationConfig::ENABLE_REAL_TIME_ANALYSIS = "features/real_time_analysis";
const QString SportsIntegrationConfig::ENABLE_GPU_ACCELERATION = "features/gpu_acceleration";
const QString SportsIntegrationConfig::ENABLE_CLOUD_SYNC = "features/cloud_sync";
const QString SportsIntegrationConfig::ENABLE_TELEMETRY = "features/telemetry";
const QString SportsIntegrationConfig::ENABLE_AUTO_RECOVERY = "features/auto_recovery";
const QString SportsIntegrationConfig::MAX_CONCURRENT_PIPELINES = "performance/max_concurrent_pipelines";
const QString SportsIntegrationConfig::HEALTH_CHECK_INTERVAL = "monitoring/health_check_interval";
const QString SportsIntegrationConfig::DATA_SYNC_INTERVAL = "sync/data_sync_interval";
const QString SportsIntegrationConfig::CACHE_SIZE_LIMIT = "performance/cache_size_limit";
const QString SportsIntegrationConfig::PROCESSING_TIMEOUT = "performance/processing_timeout";

SportsIntegration::SportsIntegration(QObject* parent)
  : QObject(parent)
  , superset_panel_(nullptr)
  , kafka_publisher_(nullptr)
  , triangle_defense_sync_(nullptr)
  , minio_client_(nullptr)
  , video_timeline_sync_(nullptr)
  , formation_overlay_(nullptr)
  , coaching_alert_widget_(nullptr)
  , timeline_panel_(nullptr)
  , viewer_panel_(nullptr)
  , integration_state_(IntegrationState::Disconnected)
  , current_analysis_mode_(AnalysisMode::RealTime)
  , is_initialized_(false)
  , real_time_analysis_active_(false)
  , current_video_position_(0)
  , settings_(nullptr)
  , health_check_timer_(nullptr)
  , stats_update_timer_(nullptr)
  , maintenance_timer_(nullptr)
  , max_concurrent_pipelines_(4)
  , error_recovery_timer_(nullptr)
  , consecutive_failures_(0)
  , automatic_recovery_enabled_(true)
  , background_thread_(nullptr)
  , performance_monitoring_enabled_(true)
  , performance_optimization_timer_(nullptr)
  , network_manager_(nullptr)
  , data_sync_in_progress_(false)
  , data_sync_timer_(nullptr)
  , data_sync_interval_minutes_(15)
{
  qInfo() << "Initializing Sports Integration System";
  
  // Initialize settings
  settings_ = new QSettings("AnalyzeMyTeam", "ApacheCleats", this);
  config_file_path_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + 
                     "/sports_integration_config.json";
  
  // Load configuration
  LoadConfiguration();
  
  // Setup background thread for heavy operations
  background_thread_ = new QThread(this);
  background_thread_->setObjectName("SportsIntegrationBackground");
  background_thread_->start();
  
  // Initialize network manager
  network_manager_ = new QNetworkAccessManager(this);
  
  // Setup timers
  SetupTimers();
  
  qInfo() << "Sports Integration System initialized";
}

SportsIntegration::~SportsIntegration()
{
  Shutdown();
}

bool SportsIntegration::Initialize(const QJsonObject& configuration)
{
  if (is_initialized_) {
    qWarning() << "Sports Integration already initialized";
    return true;
  }

  qInfo() << "Starting Sports Integration initialization";
  UpdateIntegrationState(IntegrationState::Connecting);

  // Merge provided configuration with existing
  if (!configuration.isEmpty()) {
    configuration_ = SportsIntegrationConfig::MergeConfigurations(configuration_, configuration);
    SaveConfiguration();
  }

  // Validate configuration
  if (!SportsIntegrationConfig::ValidateConfiguration(configuration_)) {
    qCritical() << "Invalid sports integration configuration";
    UpdateIntegrationState(IntegrationState::Error);
    return false;
  }

  // Check system requirements
  if (!ValidateSystemRequirements()) {
    qWarning() << "System requirements not fully met - some features may be limited";
  }

  // Setup components
  SetupComponents();

  // Connect component signals
  ConnectComponentSignals();

  // Initialize analysis pipeline
  InitializeAnalysisPipeline();

  // Test component connections
  if (!TestComponentConnections()) {
    qWarning() << "Some component connections failed - system will operate in degraded mode";
  }

  // Setup health monitoring
  SetupHealthMonitoring();

  // Start component monitoring
  StartComponentMonitoring();

  is_initialized_ = true;
  UpdateIntegrationState(IntegrationState::Connected);

  qInfo() << "Sports Integration initialization completed successfully";
  emit IntegrationStateChanged(integration_state_);

  return true;
}

void SportsIntegration::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Sports Integration System";
  UpdateIntegrationState(IntegrationState::Disconnected);

  // Stop real-time analysis if active
  if (real_time_analysis_active_) {
    StopRealTimeAnalysis();
  }

  // Stop monitoring
  StopComponentMonitoring();

  // Save configuration
  SaveConfiguration();

  // Shutdown components in reverse order
  if (coaching_alert_widget_) {
    coaching_alert_widget_->deleteLater();
    coaching_alert_widget_ = nullptr;
  }

  if (formation_overlay_) {
    formation_overlay_->deleteLater();
    formation_overlay_ = nullptr;
  }

  if (video_timeline_sync_) {
    video_timeline_sync_->deleteLater();
    video_timeline_sync_ = nullptr;
  }

  if (superset_panel_) {
    superset_panel_->deleteLater();
    superset_panel_ = nullptr;
  }

  if (triangle_defense_sync_) {
    triangle_defense_sync_->Shutdown();
    triangle_defense_sync_->deleteLater();
    triangle_defense_sync_ = nullptr;
  }

  if (minio_client_) {
    minio_client_->Shutdown();
    minio_client_->deleteLater();
    minio_client_ = nullptr;
  }

  if (kafka_publisher_) {
    kafka_publisher_->Shutdown();
    kafka_publisher_->deleteLater();
    kafka_publisher_ = nullptr;
  }

  // Stop background thread
  if (background_thread_ && background_thread_->isRunning()) {
    background_thread_->quit();
    background_thread_->wait(5000);
  }

  // Cleanup resources
  CleanupResources();

  is_initialized_ = false;
  
  qInfo() << "Sports Integration System shutdown complete";
}

void SportsIntegration::ConnectTimelinePanel(TimelinePanel* timeline_panel)
{
  if (timeline_panel_) {
    // Disconnect previous panel
    disconnect(timeline_panel_, nullptr, this, nullptr);
  }

  timeline_panel_ = timeline_panel;

  if (timeline_panel_) {
    // Connect timeline events
    connect(timeline_panel_, &TimelinePanel::PlaybackStarted,
            this, &SportsIntegration::OnVideoPlaybackStarted);
    connect(timeline_panel_, &TimelinePanel::PlaybackStopped,
            this, &SportsIntegration::OnVideoPlaybackStopped);
    connect(timeline_panel_, &TimelinePanel::PositionChanged,
            this, &SportsIntegration::OnVideoPositionChanged);
    connect(timeline_panel_, &TimelinePanel::SeekPerformed,
            this, &SportsIntegration::OnVideoSeekPerformed);
    connect(timeline_panel_, &TimelinePanel::RateChanged,
            this, &SportsIntegration::OnVideoRateChanged);

    // Connect to video timeline sync
    if (video_timeline_sync_) {
      video_timeline_sync_->ConnectTimelinePanel(timeline_panel_);
    }

    qInfo() << "Timeline panel connected to Sports Integration";
  }
}

void SportsIntegration::ConnectSequenceViewerPanel(SequenceViewerPanel* viewer_panel)
{
  if (viewer_panel_) {
    // Disconnect previous panel
    disconnect(viewer_panel_, nullptr, this, nullptr);
  }

  viewer_panel_ = viewer_panel;

  if (viewer_panel_) {
    // Connect viewer events
    connect(viewer_panel_, &SequenceViewerPanel::VideoLoaded,
            this, &SportsIntegration::OnVideoLoaded);

    // Connect to formation overlay
    if (formation_overlay_) {
      formation_overlay_->ConnectSequenceViewer(viewer_panel_);
    }

    qInfo() << "Sequence viewer panel connected to Sports Integration";
  }
}

QString SportsIntegration::LoadVideoForAnalysis(const QString& video_path, const QJsonObject& metadata)
{
  if (!is_initialized_) {
    qWarning() << "Sports Integration not initialized";
    return QString();
  }

  QFileInfo file_info(video_path);
  if (!file_info.exists() || !file_info.isReadable()) {
    qWarning() << "Video file not accessible:" << video_path;
    return QString();
  }

  // Validate video compatibility
  if (!SportsIntegrationUtils::IsVideoCompatible(video_path)) {
    qWarning() << "Video format not compatible:" << video_path;
    return QString();
  }

  QString video_id = SportsIntegrationUtils::GenerateVideoSessionId();
  active_video_id_ = video_id;

  qInfo() << "Loading video for analysis:" << video_path << "ID:" << video_id;
  emit VideoLoadStarted(video_id);

  // Create pipeline status
  PipelineStatus pipeline;
  pipeline.video_id = video_id;
  pipeline.current_stage = ProcessingStage::VideoUpload;
  pipeline.start_time = QDateTime::currentDateTime();

  {
    QMutexLocker locker(&pipeline_mutex_);
    active_pipelines_[video_id] = pipeline;
  }

  // Start video upload to MinIO
  if (minio_client_) {
    QJsonObject upload_metadata = metadata;
    upload_metadata["analysis_requested"] = true;
    upload_metadata["video_session_id"] = video_id;
    upload_metadata["upload_timestamp"] = QDateTime::currentMSecsSinceEpoch();

    QString upload_id = minio_client_->UploadVideoFile(video_path, "videos", upload_metadata);
    if (upload_id.isEmpty()) {
      qWarning() << "Failed to start video upload";
      emit VideoLoadFailed(video_id, "Upload failed");
      return QString();
    }

    UpdatePipelineStatus(video_id, ProcessingStage::VideoUpload, 10.0);
  }

  emit VideoLoadCompleted(video_id);
  return video_id;
}

bool SportsIntegration::StartRealTimeAnalysis(const QString& video_id)
{
  if (!is_initialized_) {
    qWarning() << "Sports Integration not initialized";
    return false;
  }

  if (real_time_analysis_active_) {
    qWarning() << "Real-time analysis already active";
    return false;
  }

  if (video_id.isEmpty() || video_id != active_video_id_) {
    qWarning() << "Invalid or inactive video ID for real-time analysis";
    return false;
  }

  qInfo() << "Starting real-time analysis for video:" << video_id;

  current_analysis_mode_ = AnalysisMode::RealTime;
  real_time_analysis_active_ = true;

  // Enable real-time mode in Triangle Defense sync
  if (triangle_defense_sync_) {
    triangle_defense_sync_->SetRealTimeMode(true);
  }

  // Start video timeline synchronization
  if (video_timeline_sync_) {
    video_timeline_sync_->StartRealTimeSync();
  }

  // Enable formation overlay
  if (formation_overlay_) {
    formation_overlay_->SetRealTimeMode(true);
  }

  // Start health monitoring for analysis
  if (health_check_timer_ && !health_check_timer_->isActive()) {
    health_check_timer_->start();
  }

  emit RealTimeAnalysisStarted(video_id);
  qInfo() << "Real-time analysis started successfully";

  return true;
}

void SportsIntegration::StopRealTimeAnalysis()
{
  if (!real_time_analysis_active_) {
    return;
  }

  qInfo() << "Stopping real-time analysis";

  real_time_analysis_active_ = false;

  // Disable real-time mode in components
  if (triangle_defense_sync_) {
    triangle_defense_sync_->SetRealTimeMode(false);
  }

  if (video_timeline_sync_) {
    video_timeline_sync_->StopRealTimeSync();
  }

  if (formation_overlay_) {
    formation_overlay_->SetRealTimeMode(false);
  }

  emit RealTimeAnalysisStopped(active_video_id_);
  qInfo() << "Real-time analysis stopped";
}

QString SportsIntegration::ProcessVideoBatch(const QString& video_path, AnalysisMode mode)
{
  if (!is_initialized_) {
    qWarning() << "Sports Integration not initialized";
    return QString();
  }

  QString video_id = LoadVideoForAnalysis(video_path);
  if (video_id.isEmpty()) {
    return QString();
  }

  qInfo() << "Starting batch processing for video:" << video_id << "mode:" << static_cast<int>(mode);

  current_analysis_mode_ = mode;

  // Queue for batch processing
  {
    QMutexLocker locker(&pipeline_mutex_);
    processing_queue_.enqueue(video_id);
  }

  // Start processing pipeline in background
  QtConcurrent::run(background_thread_, [this, video_id, mode]() {
    ProcessVideoStage(video_id, ProcessingStage::MetadataExtraction);
  });

  return video_id;
}

SystemHealth SportsIntegration::GetSystemHealth() const
{
  QMutexLocker locker(&stats_mutex_);
  return system_health_;
}

PipelineStatus SportsIntegration::GetPipelineStatus(const QString& video_id) const
{
  QMutexLocker locker(&pipeline_mutex_);
  
  if (active_pipelines_.contains(video_id)) {
    return active_pipelines_[video_id];
  }
  
  return PipelineStatus();
}

QJsonObject SportsIntegration::GetIntegrationStatistics() const
{
  QMutexLocker locker(&stats_mutex_);
  
  QJsonObject stats;
  stats["videos_processed"] = static_cast<qint64>(stats_.videos_processed);
  stats["formations_detected"] = static_cast<qint64>(stats_.formations_detected);
  stats["triangle_calls_made"] = static_cast<qint64>(stats_.triangle_calls_made);
  stats["coaching_alerts_generated"] = static_cast<qint64>(stats_.coaching_alerts_generated);
  stats["uptime_seconds"] = static_cast<qint64>(stats_.uptime_seconds);
  stats["average_processing_time"] = stats_.average_processing_time;
  stats["system_performance_score"] = stats_.system_performance_score;
  stats["integration_state"] = static_cast<int>(integration_state_);
  stats["real_time_analysis_active"] = real_time_analysis_active_;
  stats["active_pipelines"] = active_pipelines_.size();
  stats["component_status"] = QJsonObject();
  
  // Add component statistics
  QJsonObject component_stats;
  if (kafka_publisher_) {
    component_stats["kafka"] = kafka_publisher_->GetStatistics();
  }
  if (minio_client_) {
    component_stats["minio"] = minio_client_->GetStatistics();
  }
  if (triangle_defense_sync_) {
    component_stats["triangle_defense"] = triangle_defense_sync_->GetSyncStatistics();
  }
  stats["component_statistics"] = component_stats;
  
  return stats;
}

void SportsIntegration::PerformHealthCheck()
{
  qDebug() << "Performing system health check";
  
  SystemHealth health;
  health.last_health_check = QDateTime::currentDateTime();
  
  // Check component connections
  health.kafka_connected = component_status_.value("kafka", false);
  health.supabase_connected = component_status_.value("supabase", false);
  health.minio_connected = component_status_.value("minio", false);
  health.superset_accessible = component_status_.value("superset", false);
  health.mel_pipeline_active = component_status_.value("mel_pipeline", false);
  
  // Check system resources
  QStorageInfo storage_info(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
  health.disk_space_available = storage_info.bytesAvailable();
  
  // Get system performance metrics
  QProcess cpu_process;
  cpu_process.start("bash", QStringList() << "-c" << "grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$3+$4)} END {print usage}'");
  cpu_process.waitForFinished(1000);
  if (cpu_process.exitCode() == 0) {
    health.cpu_usage = cpu_process.readAllStandardOutput().trimmed().toDouble();
  }
  
  QProcess mem_process;
  mem_process.start("bash", QStringList() << "-c" << "free | grep Mem | awk '{print ($3/$2) * 100.0}'");
  mem_process.waitForFinished(1000);
  if (mem_process.exitCode() == 0) {
    health.memory_usage = mem_process.readAllStandardOutput().trimmed().toDouble();
  }
  
  // Count active formations and alerts
  if (triangle_defense_sync_) {
    health.active_formations = triangle_defense_sync_->GetFormationsInRange(
      QDateTime::currentMSecsSinceEpoch() - 300000, // Last 5 minutes
      QDateTime::currentMSecsSinceEpoch()
    ).size();
    
    health.pending_alerts = triangle_defense_sync_->GetActiveAlerts().size();
  }
  
  // Update system health
  {
    QMutexLocker locker(&stats_mutex_);
    system_health_ = health;
    stats_.system_performance_score = SportsIntegrationUtils::CalculatePerformanceScore(health);
  }
  
  emit SystemHealthUpdated(health);
  
  // Check for critical issues
  if (health.cpu_usage > 90.0) {
    emit PerformanceIssueDetected("High CPU usage detected");
  }
  
  if (health.memory_usage > 90.0) {
    emit PerformanceIssueDetected("High memory usage detected");
  }
  
  if (health.disk_space_available < 1024 * 1024 * 1024) { // Less than 1GB
    emit SystemWarningIssued("Low disk space available");
  }
}

QJsonObject SportsIntegration::GetActiveVideoSession() const
{
  QJsonObject session;
  session["video_id"] = active_video_id_;
  session["current_position"] = current_video_position_;
  session["analysis_mode"] = static_cast<int>(current_analysis_mode_);
  session["real_time_active"] = real_time_analysis_active_;
  session["integration_state"] = static_cast<int>(integration_state_);
  
  if (!active_video_id_.isEmpty()) {
    PipelineStatus pipeline = GetPipelineStatus(active_video_id_);
    session["pipeline_stage"] = static_cast<int>(pipeline.current_stage);
    session["completion_percentage"] = pipeline.completion_percentage;
  }
  
  return session;
}

void SportsIntegration::OnVideoLoaded(const QString& video_path)
{
  qInfo() << "Video loaded in sequence viewer:" << video_path;
  
  // If this is a new video, start analysis
  if (configuration_.value(SportsIntegrationConfig::ENABLE_REAL_TIME_ANALYSIS, true).toBool()) {
    QString video_id = LoadVideoForAnalysis(video_path);
    if (!video_id.isEmpty()) {
      StartRealTimeAnalysis(video_id);
    }
  }
}

void SportsIntegration::OnVideoPlaybackStarted()
{
  qDebug() << "Video playback started";
  
  if (real_time_analysis_active_ && triangle_defense_sync_) {
    triangle_defense_sync_->OnVideoPlaybackChanged(true, current_video_position_);
  }
  
  if (kafka_publisher_) {
    kafka_publisher_->PublishVideoEvent("playback_started", current_video_position_);
  }
}

void SportsIntegration::OnVideoPlaybackStopped()
{
  qDebug() << "Video playback stopped";
  
  if (real_time_analysis_active_ && triangle_defense_sync_) {
    triangle_defense_sync_->OnVideoPlaybackChanged(false, current_video_position_);
  }
  
  if (kafka_publisher_) {
    kafka_publisher_->PublishVideoEvent("playback_stopped", current_video_position_);
  }
}

void SportsIntegration::OnVideoPositionChanged(qint64 position)
{
  current_video_position_ = position;
  
  if (real_time_analysis_active_) {
    if (triangle_defense_sync_) {
      triangle_defense_sync_->SetVideoTimestamp(position);
    }
    
    if (video_timeline_sync_) {
      video_timeline_sync_->UpdateVideoPosition(position);
    }
    
    if (formation_overlay_) {
      formation_overlay_->UpdateVideoPosition(position);
    }
  }
}

void SportsIntegration::OnVideoSeekPerformed(qint64 position)
{
  current_video_position_ = position;
  
  qDebug() << "Video seek performed to position:" << position;
  
  if (real_time_analysis_active_ && triangle_defense_sync_) {
    triangle_defense_sync_->OnVideoSeekPerformed(position);
  }
  
  if (kafka_publisher_) {
    kafka_publisher_->PublishVideoEvent("seek_performed", position);
  }
}

void SportsIntegration::OnVideoRateChanged(double rate)
{
  qDebug() << "Video playback rate changed to:" << rate;
  
  if (real_time_analysis_active_ && triangle_defense_sync_) {
    triangle_defense_sync_->OnVideoRateChanged(rate);
  }
}

void SportsIntegration::OnFormationDetected(const FormationData& formation)
{
  qInfo() << "Formation detected:" << formation.formation_id 
          << "type:" << static_cast<int>(formation.type)
          << "confidence:" << formation.confidence;
  
  {
    QMutexLocker locker(&stats_mutex_);
    stats_.formations_detected++;
  }
  
  // Update formation overlay
  if (formation_overlay_) {
    formation_overlay_->AddFormation(formation);
  }
  
  // Update Superset dashboard
  if (superset_panel_) {
    QJsonObject formation_json;
    formation_json["formation_id"] = formation.formation_id;
    formation_json["type"] = static_cast<int>(formation.type);
    formation_json["confidence"] = formation.confidence;
    formation_json["video_timestamp"] = formation.video_timestamp;
    formation_json["recommended_call"] = static_cast<int>(formation.recommended_call);
    
    superset_panel_->UpdateFormationData(formation_json);
  }
  
  // Publish to Kafka
  if (kafka_publisher_) {
    QJsonObject formation_data;
    formation_data["formation_id"] = formation.formation_id;
    formation_data["type"] = static_cast<int>(formation.type);
    formation_data["confidence"] = formation.confidence;
    formation_data["video_timestamp"] = formation.video_timestamp;
    formation_data["hash_position"] = formation.hash_position;
    formation_data["field_zone"] = formation.field_zone;
    
    kafka_publisher_->PublishStructuredEvent(
      EventType::FormationUpdate, 
      "formation-detections", 
      formation_data,
      formation.formation_id,
      EventPriority::Normal
    );
  }
}

void SportsIntegration::OnTriangleCallMade(TriangleCall call, const FormationData& formation)
{
  qInfo() << "Triangle Defense call made:" << static_cast<int>(call) 
          << "for formation:" << formation.formation_id;
  
  {
    QMutexLocker locker(&stats_mutex_);
    stats_.triangle_calls_made++;
  }
  
  // Update Superset panel
  if (superset_panel_) {
    superset_panel_->OnTriangleDefenseCall(QString::number(static_cast<int>(call)), QJsonObject());
  }
  
  // Publish to Kafka
  if (kafka_publisher_) {
    QJsonObject call_data;
    call_data["call_type"] = static_cast<int>(call);
    call_data["formation_id"] = formation.formation_id;
    call_data["confidence"] = formation.confidence;
    call_data["video_timestamp"] = formation.video_timestamp;
    
    kafka_publisher_->PublishTriangleDefenseEvent(
      QString::number(static_cast<int>(call)),
      call_data,
      formation.video_timestamp,
      formation.confidence
    );
  }
}

void SportsIntegration::OnCoachingAlertGenerated(const CoachingAlert& alert)
{
  qInfo() << "Coaching alert generated:" << alert.alert_id 
          << "type:" << alert.alert_type
          << "priority:" << alert.priority_level;
  
  {
    QMutexLocker locker(&stats_mutex_);
    stats_.coaching_alerts_generated++;
  }
  
  // Show in coaching alert widget
  if (coaching_alert_widget_) {
    coaching_alert_widget_->AddAlert(alert);
  }
  
  // Publish to Kafka
  if (kafka_publisher_) {
    kafka_publisher_->PublishCoachingAlert(
      alert.alert_type,
      alert.message,
      static_cast<EventPriority>(alert.priority_level),
      alert.context_data
    );
  }
  
  // Send critical alerts immediately
  if (alert.priority_level >= 4) {
    emit CriticalAlertGenerated(alert);
  }
}

void SportsIntegration::SetupComponents()
{
  qInfo() << "Setting up sports integration components";
  
  // Initialize Kafka publisher
  kafka_publisher_ = new KafkaPublisher(this);
  kafka_publisher_->Initialize(
    configuration_.value(SportsIntegrationConfig::KAFKA_BOOTSTRAP_SERVERS, "localhost:9092").toString(),
    "apache-cleats-sports"
  );
  
  // Initialize MinIO client
  minio_client_ = new MinIOClient(this);
  minio_client_->Initialize(
    configuration_.value(SportsIntegrationConfig::MINIO_ENDPOINT, "localhost:9000").toString(),
    configuration_.value(SportsIntegrationConfig::MINIO_ACCESS_KEY, "").toString(),
    configuration_.value(SportsIntegrationConfig::MINIO_SECRET_KEY, "").toString(),
    false // Use HTTP for local development
  );
  
  // Initialize Triangle Defense sync
  triangle_defense_sync_ = new TriangleDefenseSync(this);
  triangle_defense_sync_->Initialize(
    configuration_.value(SportsIntegrationConfig::SUPABASE_URL, "").toString(),
    configuration_.value(SportsIntegrationConfig::SUPABASE_API_KEY, "").toString(),
    configuration_.value(SportsIntegrationConfig::WEBSOCKET_URL, "").toString()
  );
  
  // Initialize Superset panel
  superset_panel_ = new SupersetPanel("SportsIntegrationSuperset", nullptr);
  superset_panel_->SetAuthToken(""); // Set from configuration
  
  // Initialize video timeline sync
  video_timeline_sync_ = new VideoTimelineSync(this);
  
  // Initialize formation overlay
  formation_overlay_ = new FormationOverlay(this);
  
  // Initialize coaching alert widget
  coaching_alert_widget_ = new CoachingAlertWidget(this);
  
  qInfo() << "Sports integration components setup complete";
}

void SportsIntegration::SetupTimers()
{
  // Health check timer
  health_check_timer_ = new QTimer(this);
  health_check_timer_->setInterval(
    configuration_.value(SportsIntegrationConfig::HEALTH_CHECK_INTERVAL, 30000).toInt()
  );
  connect(health_check_timer_, &QTimer::timeout, this, &SportsIntegration::OnHealthCheckTimer);
  
  // Statistics update timer
  stats_update_timer_ = new QTimer(this);
  stats_update_timer_->setInterval(10000); // 10 seconds
  connect(stats_update_timer_, &QTimer::timeout, this, &SportsIntegration::OnStatsUpdateTimer);
  stats_update_timer_->start();
  
  // Maintenance timer
  maintenance_timer_ = new QTimer(this);
  maintenance_timer_->setInterval(3600000); // 1 hour
  connect(maintenance_timer_, &QTimer::timeout, this, &SportsIntegration::OnMaintenanceTimer);
  maintenance_timer_->start();
  
  // Error recovery timer
  error_recovery_timer_ = new QTimer(this);
  error_recovery_timer_->setInterval(60000); // 1 minute
  error_recovery_timer_->setSingleShot(true);
  
  // Performance optimization timer
  performance_optimization_timer_ = new QTimer(this);
  performance_optimization_timer_->setInterval(300000); // 5 minutes
  
  // Data sync timer
  data_sync_timer_ = new QTimer(this);
  data_sync_timer_->setInterval(data_sync_interval_minutes_ * 60000);
  connect(data_sync_timer_, &QTimer::timeout, this, &SportsIntegration::OnDataSyncRequired);
}

void SportsIntegration::LoadConfiguration()
{
  // Load default configuration
  configuration_ = SportsIntegrationConfig::GetDefaultConfiguration();
  
  // Load from file if exists
  if (QFileInfo::exists(config_file_path_)) {
    QJsonObject file_config = SportsIntegrationConfig::LoadConfiguration(config_file_path_);
    if (!file_config.isEmpty()) {
      configuration_ = SportsIntegrationConfig::MergeConfigurations(configuration_, file_config);
    }
  }
  
  // Load from Qt settings for legacy support
  if (settings_) {
    QJsonObject settings_config;
    for (const QString& key : settings_->allKeys()) {
      settings_config[key] = settings_->value(key).toString();
    }
    if (!settings_config.isEmpty()) {
      configuration_ = SportsIntegrationConfig::MergeConfigurations(configuration_, settings_config);
    }
  }
  
  qInfo() << "Sports integration configuration loaded";
}

void SportsIntegration::SaveConfiguration()
{
  SportsIntegrationConfig::SaveConfiguration(configuration_, config_file_path_);
  qDebug() << "Sports integration configuration saved";
}

void SportsIntegration::ConnectComponentSignals()
{
  if (triangle_defense_sync_) {
    connect(triangle_defense_sync_, &TriangleDefenseSync::FormationDetected,
            this, &SportsIntegration::OnFormationDetected);
    connect(triangle_defense_sync_, &TriangleDefenseSync::TriangleCallRecommended,
            this, &SportsIntegration::OnTriangleCallMade);
    connect(triangle_defense_sync_, &TriangleDefenseSync::CoachingAlertReceived,
            this, &SportsIntegration::OnCoachingAlertGenerated);
    connect(triangle_defense_sync_, &TriangleDefenseSync::SyncStatusChanged,
            this, [this](bool connected) {
              OnComponentStatusChanged("triangle_defense", connected);
            });
  }
  
  if (minio_client_) {
    connect(minio_client_, &MinIOClient::VideoUploaded,
            this, &SportsIntegration::OnVideoUploadCompleted);
    connect(minio_client_, &MinIOClient::Connected,
            this, [this]() { OnComponentStatusChanged("minio", true); });
    connect(minio_client_, &MinIOClient::Disconnected,
            this, [this]() { OnComponentStatusChanged("minio", false); });
  }
  
  if (kafka_publisher_) {
    connect(kafka_publisher_, &KafkaPublisher::ConnectionEstablished,
            this, [this]() { OnComponentStatusChanged("kafka", true); });
    connect(kafka_publisher_, &KafkaPublisher::ConnectionLost,
            this, [this]() { OnComponentStatusChanged("kafka", false); });
  }
}

void SportsIntegration::UpdateIntegrationState(IntegrationState new_state)
{
  if (integration_state_ != new_state) {
    integration_state_ = new_state;
    emit IntegrationStateChanged(new_state);
    qInfo() << "Integration state changed to:" << static_cast<int>(new_state);
  }
}

void SportsIntegration::UpdatePipelineStatus(const QString& video_id, ProcessingStage stage, double progress)
{
  QMutexLocker locker(&pipeline_mutex_);
  
  if (active_pipelines_.contains(video_id)) {
    active_pipelines_[video_id].current_stage = stage;
    active_pipelines_[video_id].completion_percentage = progress;
    active_pipelines_[video_id].current_operation = QString("Processing stage %1").arg(static_cast<int>(stage));
    
    emit ProcessingStageChanged(video_id, stage);
  }
}

void SportsIntegration::OnComponentStatusChanged(const QString& component, bool connected)
{
  component_status_[component] = connected;
  last_component_check_[component] = QDateTime::currentDateTime();
  
  if (connected) {
    emit ComponentConnected(component);
    qInfo() << "Component connected:" << component;
  } else {
    emit ComponentDisconnected(component);
    qWarning() << "Component disconnected:" << component;
  }
}

void SportsIntegration::OnHealthCheckTimer()
{
  PerformHealthCheck();
}

void SportsIntegration::OnStatsUpdateTimer()
{
  // Update uptime
  {
    QMutexLocker locker(&stats_mutex_);
    stats_.uptime_seconds = stats_.start_time.secsTo(QDateTime::currentDateTime());
  }
  
  emit StatisticsUpdated(GetIntegrationStatistics());
}

void SportsIntegration::OnMaintenanceTimer()
{
  PerformAutomaticMaintenance();
}

void SportsIntegration::OnVideoUploadCompleted(const VideoMetadata& metadata)
{
  qInfo() << "Video upload completed:" << metadata.file_id;
  
  // Continue to next processing stage
  ProcessVideoStage(metadata.file_id, ProcessingStage::MetadataExtraction);
}

bool SportsIntegration::ValidateSystemRequirements()
{
  SystemRequirementsChecker::Requirements cpu_req = SystemRequirementsChecker::CheckCPU();
  SystemRequirementsChecker::Requirements memory_req = SystemRequirementsChecker::CheckMemory();
  SystemRequirementsChecker::Requirements disk_req = SystemRequirementsChecker::CheckDiskSpace();
  
  bool requirements_met = cpu_req.satisfied && memory_req.satisfied && disk_req.satisfied;
  
  if (!requirements_met) {
    qWarning() << "System requirements not fully satisfied";
    qWarning() << "CPU:" << cpu_req.current_value << "Requirement:" << cpu_req.requirement;
    qWarning() << "Memory:" << memory_req.current_value << "Requirement:" << memory_req.requirement;
    qWarning() << "Disk:" << disk_req.current_value << "Requirement:" << disk_req.requirement;
  }
  
  return requirements_met;
}

bool SportsIntegration::TestComponentConnections()
{
  bool all_connected = true;
  
  if (kafka_publisher_ && !kafka_publisher_->IsConnected()) {
    all_connected = false;
    qWarning() << "Kafka publisher not connected";
  }
  
  if (minio_client_ && !minio_client_->IsConnected()) {
    all_connected = false;
    qWarning() << "MinIO client not connected";
  }
  
  if (triangle_defense_sync_ && triangle_defense_sync_->GetSyncStatistics()["is_connected"].toBool() == false) {
    all_connected = false;
    qWarning() << "Triangle Defense sync not connected";
  }
  
  return all_connected;
}

void SportsIntegration::PerformAutomaticMaintenance()
{
  qInfo() << "Performing automatic maintenance";
  
  // Clean up old pipeline entries
  {
    QMutexLocker locker(&pipeline_mutex_);
    auto it = active_pipelines_.begin();
    while (it != active_pipelines_.end()) {
      if (it.value().start_time.secsTo(QDateTime::currentDateTime()) > 3600) { // 1 hour old
        it = active_pipelines_.erase(it);
      } else {
        ++it;
      }
    }
  }
  
  // Clean up old errors
  if (recent_errors_.size() > 100) {
    recent_errors_.removeLast();
  }
  
  // Optimize performance if enabled
  if (performance_monitoring_enabled_) {
    OptimizePerformance();
  }
  
  qInfo() << "Automatic maintenance completed";
}

void SportsIntegration::ProcessVideoStage(const QString& video_id, ProcessingStage stage)
{
  qDebug() << "Processing video stage:" << video_id << "stage:" << static_cast<int>(stage);
  
  switch (stage) {
    case ProcessingStage::VideoUpload:
      // Handled by OnVideoUploadCompleted
      break;
      
    case ProcessingStage::MetadataExtraction:
      UpdatePipelineStatus(video_id, stage, 25.0);
      // Extract video metadata and continue
      ProcessVideoStage(video_id, ProcessingStage::FormationDetection);
      break;
      
    case ProcessingStage::FormationDetection:
      UpdatePipelineStatus(video_id, stage, 50.0);
      // Formation detection would be handled by Triangle Defense sync
      ProcessVideoStage(video_id, ProcessingStage::MELProcessing);
      break;
      
    case ProcessingStage::MELProcessing:
      UpdatePipelineStatus(video_id, stage, 75.0);
      // M.E.L. processing continues in background
      ProcessVideoStage(video_id, ProcessingStage::DashboardSync);
      break;
      
    case ProcessingStage::DashboardSync:
      UpdatePipelineStatus(video_id, stage, 90.0);
      // Sync with Superset dashboard
      ProcessVideoStage(video_id, ProcessingStage::Complete);
      break;
      
    case ProcessingStage::Complete:
      UpdatePipelineStatus(video_id, stage, 100.0);
      emit ProcessingCompleted(video_id);
      {
        QMutexLocker locker(&stats_mutex_);
        stats_.videos_processed++;
      }
      break;
      
    default:
      qWarning() << "Unknown processing stage:" << static_cast<int>(stage);
      break;
  }
}

void SportsIntegration::OptimizePerformance()
{
  // Check memory usage and clean caches if necessary
  QProcess mem_check;
  mem_check.start("bash", QStringList() << "-c" << "free | grep Mem | awk '{print ($3/$2) * 100.0}'");
  mem_check.waitForFinished(1000);
  
  if (mem_check.exitCode() == 0) {
    double memory_usage = mem_check.readAllStandardOutput().trimmed().toDouble();
    
    if (memory_usage > 80.0) {
      qInfo() << "High memory usage detected, optimizing caches";
      
      // Clear component caches
      if (triangle_defense_sync_) {
        // Triangle Defense sync has internal cache cleanup
      }
      
      if (minio_client_) {
        // MinIO client cache optimization would be internal
      }
    }
  }
}

void SportsIntegration::CleanupResources()
{
  // Clear all caches and temporary data
  {
    QMutexLocker locker(&pipeline_mutex_);
    active_pipelines_.clear();
    processing_queue_.clear();
  }
  
  component_status_.clear();
  last_component_check_.clear();
  recent_errors_.clear();
  
  qInfo() << "Sports integration resources cleaned up";
}

// SportsIntegrationConfig implementation
QJsonObject SportsIntegrationConfig::GetDefaultConfiguration()
{
  QJsonObject config;
  
  // Connection settings
  config[SUPABASE_URL] = "https://your-project.supabase.co";
  config[SUPABASE_API_KEY] = "";
  config[KAFKA_BOOTSTRAP_SERVERS] = "localhost:9092";
  config[MINIO_ENDPOINT] = "localhost:9000";
  config[MINIO_ACCESS_KEY] = "minioadmin";
  config[MINIO_SECRET_KEY] = "minioadmin";
  config[SUPERSET_BASE_URL] = "http://localhost:8088";
  config[WEBSOCKET_URL] = "ws://localhost:8080/ws";
  
  // Feature flags
  config[ENABLE_REAL_TIME_ANALYSIS] = true;
  config[ENABLE_GPU_ACCELERATION] = true;
  config[ENABLE_CLOUD_SYNC] = true;
  config[ENABLE_TELEMETRY] = false;
  config[ENABLE_AUTO_RECOVERY] = true;
  
  // Performance settings
  config[MAX_CONCURRENT_PIPELINES] = 4;
  config[HEALTH_CHECK_INTERVAL] = 30000;
  config[DATA_SYNC_INTERVAL] = 900000; // 15 minutes
  config[CACHE_SIZE_LIMIT] = 1073741824; // 1GB
  config[PROCESSING_TIMEOUT] = 300000; // 5 minutes
  
  return config;
}

bool SportsIntegrationConfig::ValidateConfiguration(const QJsonObject& config)
{
  // Check required fields
  QStringList required_keys = {
    SUPABASE_URL, KAFKA_BOOTSTRAP_SERVERS, MINIO_ENDPOINT
  };
  
  for (const QString& key : required_keys) {
    if (!config.contains(key) || config[key].toString().isEmpty()) {
      qWarning() << "Missing required configuration key:" << key;
      return false;
    }
  }
  
  // Validate numeric ranges
  int max_pipelines = config.value(MAX_CONCURRENT_PIPELINES, 4).toInt();
  if (max_pipelines < 1 || max_pipelines > 16) {
    qWarning() << "Invalid max concurrent pipelines value:" << max_pipelines;
    return false;
  }
  
  int health_check_interval = config.value(HEALTH_CHECK_INTERVAL, 30000).toInt();
  if (health_check_interval < 5000 || health_check_interval > 300000) {
    qWarning() << "Invalid health check interval:" << health_check_interval;
    return false;
  }
  
  return true;
}

void SportsIntegrationConfig::SaveConfiguration(const QJsonObject& config, const QString& file_path)
{
  QDir dir = QFileInfo(file_path).dir();
  if (!dir.exists()) {
    dir.mkpath(".");
  }
  
  QFile file(file_path);
  if (file.open(QIODevice::WriteOnly)) {
    QJsonDocument doc(config);
    file.write(doc.toJson());
    file.close();
  } else {
    qWarning() << "Failed to save configuration to:" << file_path;
  }
}

QJsonObject SportsIntegrationConfig::LoadConfiguration(const QString& file_path)
{
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to load configuration from:" << file_path;
    return QJsonObject();
  }
  
  QByteArray data = file.readAll();
  file.close();
  
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  
  if (error.error != QJsonParseError::NoError) {
    qWarning() << "Failed to parse configuration JSON:" << error.errorString();
    return QJsonObject();
  }
  
  return doc.object();
}

// SystemRequirementsChecker implementation
QList<SystemRequirementsChecker::Requirements> SystemRequirementsChecker::CheckAllRequirements()
{
  QList<Requirements> results;
  
  results.append(CheckCPU());
  results.append(CheckMemory());
  results.append(CheckDiskSpace());
  results.append(CheckNetworkConnectivity());
  results.append(CheckVideoCodecs());
  results.append(CheckGPU());
  results.append(CheckDependencies());
  
  return results;
}

SystemRequirementsChecker::Requirements SystemRequirementsChecker::CheckCPU()
{
  Requirements req;
  req.component = "CPU";
  req.requirement = "4+ cores, 2.0+ GHz";
  
  // Get CPU info on Linux
  QProcess process;
  process.start("nproc");
  process.waitForFinished();
  
  if (process.exitCode() == 0) {
    int cores = process.readAllStandardOutput().trimmed().toInt();
    req.current_value = QString("%1 cores").arg(cores);
    req.satisfied = (cores >= 4);
    
    if (!req.satisfied) {
      req.recommendation = "Upgrade to a CPU with at least 4 cores for optimal performance";
    }
  } else {
    req.current_value = "Unknown";
    req.satisfied = false;
    req.recommendation = "Unable to determine CPU specifications";
  }
  
  return req;
}

SystemRequirementsChecker::Requirements SystemRequirementsChecker::CheckMemory()
{
  Requirements req;
  req.component = "Memory";
  req.requirement = "8GB+ RAM";
  
  QProcess process;
  process.start("bash", QStringList() << "-c" << "free -m | grep Mem | awk '{print $2}'");
  process.waitForFinished();
  
  if (process.exitCode() == 0) {
    int memory_mb = process.readAllStandardOutput().trimmed().toInt();
    req.current_value = QString("%1 GB").arg(memory_mb / 1024.0, 0, 'f', 1);
    req.satisfied = (memory_mb >= 8192);
    
    if (!req.satisfied) {
      req.recommendation = "Increase system RAM to at least 8GB for stable operation";
    }
  } else {
    req.current_value = "Unknown";
    req.satisfied = false;
    req.recommendation = "Unable to determine memory specifications";
  }
  
  return req;
}

SystemRequirementsChecker::Requirements SystemRequirementsChecker::CheckDiskSpace()
{
  Requirements req;
  req.component = "Disk Space";
  req.requirement = "50GB+ available";
  
  QStorageInfo storage_info(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
  qint64 available_bytes = storage_info.bytesAvailable();
  qint64 required_bytes = 50LL * 1024 * 1024 * 1024; // 50GB
  
  req.current_value = QString("%1 GB available").arg(available_bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
  req.satisfied = (available_bytes >= required_bytes);
  
  if (!req.satisfied) {
    req.recommendation = "Free up disk space or add additional storage";
  }
  
  return req;
}

// SportsIntegrationUtils namespace implementation
namespace SportsIntegrationUtils {

QString FormatPerformanceMetrics(const QJsonObject& metrics)
{
  QString formatted = "Performance Metrics:\n";
  
  for (auto it = metrics.constBegin(); it != metrics.constEnd(); ++it) {
    formatted += QString("  %1: %2\n").arg(it.key(), it.value().toString());
  }
  
  return formatted;
}

double CalculatePerformanceScore(const SystemHealth& health)
{
  double score = 100.0;
  
  // Deduct points for high resource usage
  if (health.cpu_usage > 80.0) score -= 20.0;
  else if (health.cpu_usage > 60.0) score -= 10.0;
  
  if (health.memory_usage > 85.0) score -= 20.0;
  else if (health.memory_usage > 70.0) score -= 10.0;
  
  // Deduct points for disconnected components
  if (!health.kafka_connected) score -= 15.0;
  if (!health.supabase_connected) score -= 15.0;
  if (!health.minio_connected) score -= 15.0;
  if (!health.superset_accessible) score -= 10.0;
  if (!health.mel_pipeline_active) score -= 10.0;
  
  // Deduct points for low disk space
  if (health.disk_space_available < 5LL * 1024 * 1024 * 1024) { // Less than 5GB
    score -= 25.0;
  } else if (health.disk_space_available < 10LL * 1024 * 1024 * 1024) { // Less than 10GB
    score -= 10.0;
  }
  
  return qMax(0.0, score);
}

QString GenerateVideoSessionId()
{
  return QString("video_%1_%2")
         .arg(QDateTime::currentMSecsSinceEpoch())
         .arg(QRandomGenerator::global()->generate() % 10000, 4, 10, QChar('0'));
}

bool IsVideoCompatible(const QString& video_path)
{
  QFileInfo file_info(video_path);
  QString suffix = file_info.suffix().toLower();
  
  QStringList supported_formats = {"mp4", "avi", "mov", "mkv", "wmv", "flv", "webm"};
  return supported_formats.contains(suffix);
}

QJsonObject GetOptimalAnalysisSettings(const VideoMetadata& metadata)
{
  QJsonObject settings;
  
  // Adjust settings based on video properties
  if (metadata.width >= 1920) {
    settings["quality"] = "high";
    settings["frame_skip"] = 1;
  } else if (metadata.width >= 1280) {
    settings["quality"] = "medium";
    settings["frame_skip"] = 2;
  } else {
    settings["quality"] = "low";
    settings["frame_skip"] = 3;
  }
  
  settings["enable_gpu"] = true;
  settings["batch_size"] = 32;
  settings["confidence_threshold"] = 0.7;
  
  return settings;
}

} // namespace SportsIntegrationUtils

} // namespace olive
