/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Triangle Defense Synchronization Service Implementation
  Real-time M.E.L. pipeline integration with video timeline
***/

#include "triangle_defense_sync.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QtMath>

namespace olive {

TriangleDefenseSync::TriangleDefenseSync(QObject* parent)
  : QObject(parent)
  , supabase_url_("https://your-project.supabase.co")
  , api_key_("")
  , websocket_url_("")
  , real_time_enabled_(true)
  , sync_interval_ms_(1000)
  , cache_retention_hours_(24)
  , max_cached_formations_(10000)
  , network_manager_(nullptr)
  , websocket_(nullptr)
  , current_reply_(nullptr)
  , sync_timer_(nullptr)
  , cache_cleanup_timer_(nullptr)
  , heartbeat_timer_(nullptr)
  , is_connected_(false)
  , is_initialized_(false)
  , current_video_timestamp_(0)
  , last_sync_timestamp_(0)
  , last_data_fetch_(0)
  , video_playing_(false)
  , video_rate_(1.0)
  , reconnection_attempts_(0)
  , reconnect_timer_(nullptr)
  , last_heartbeat_(0)
  , heartbeat_received_(false)
{
  // Register metatypes
  qRegisterMetaType<FormationType>("FormationType");
  qRegisterMetaType<TriangleCall>("TriangleCall");
  qRegisterMetaType<FormationData>("FormationData");
  qRegisterMetaType<CoachingAlert>("CoachingAlert");
  qRegisterMetaType<MELResult>("MELResult");

  SetupDatabase();
  SetupNetworking();
  SetupTimers();
}

TriangleDefenseSync::~TriangleDefenseSync()
{
  Shutdown();
}

bool TriangleDefenseSync::Initialize(const QString& supabase_url, const QString& api_key,
                                   const QString& websocket_url)
{
  if (is_initialized_) {
    qWarning() << "TriangleDefenseSync already initialized";
    return true;
  }

  supabase_url_ = supabase_url;
  api_key_ = api_key;
  websocket_url_ = websocket_url;

  qInfo() << "Initializing Triangle Defense Sync"
          << "URL:" << supabase_url_
          << "WebSocket:" << websocket_url_;

  // Initialize database
  if (!cache_db_.isOpen()) {
    qCritical() << "Failed to open cache database";
    return false;
  }

  // Setup WebSocket if URL provided
  if (!websocket_url_.isEmpty()) {
    SetupWebSocket();
  }

  // Start sync timer
  if (sync_timer_) {
    sync_timer_->start();
  }

  // Load initial data
  LoadFormationData();
  LoadCoachingAlerts();

  is_initialized_ = true;
  emit SyncStatusChanged(true);

  qInfo() << "Triangle Defense Sync initialized successfully";
  return true;
}

void TriangleDefenseSync::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Triangle Defense Sync";

  // Stop timers
  if (sync_timer_ && sync_timer_->isActive()) {
    sync_timer_->stop();
  }
  if (cache_cleanup_timer_ && cache_cleanup_timer_->isActive()) {
    cache_cleanup_timer_->stop();
  }
  if (heartbeat_timer_ && heartbeat_timer_->isActive()) {
    heartbeat_timer_->stop();
  }
  if (reconnect_timer_ && reconnect_timer_->isActive()) {
    reconnect_timer_->stop();
  }

  // Close WebSocket
  if (websocket_) {
    websocket_->close();
  }

  // Cancel network requests
  if (current_reply_) {
    current_reply_->abort();
    current_reply_ = nullptr;
  }

  // Close database
  if (cache_db_.isOpen()) {
    cache_db_.close();
  }

  is_initialized_ = false;
  is_connected_ = false;
  
  emit SyncStatusChanged(false);
  
  qInfo() << "Triangle Defense Sync shutdown complete";
}

FormationData TriangleDefenseSync::GetFormationAt(qint64 video_timestamp) const
{
  QMutexLocker locker(&cache_mutex_);
  
  // Check exact timestamp match first
  if (formation_cache_.contains(video_timestamp)) {
    stats_.cache_hits++;
    return formation_cache_[video_timestamp];
  }

  // Find closest formations for interpolation
  FormationData closest_before, closest_after;
  qint64 min_before_diff = LLONG_MAX;
  qint64 min_after_diff = LLONG_MAX;
  bool found_before = false, found_after = false;

  for (auto it = formation_cache_.constBegin(); it != formation_cache_.constEnd(); ++it) {
    qint64 timestamp_diff = it.key() - video_timestamp;
    
    if (timestamp_diff <= 0 && qAbs(timestamp_diff) < min_before_diff) {
      min_before_diff = qAbs(timestamp_diff);
      closest_before = it.value();
      found_before = true;
    } else if (timestamp_diff > 0 && timestamp_diff < min_after_diff) {
      min_after_diff = timestamp_diff;
      closest_after = it.value();
      found_after = true;
    }
  }

  // Return interpolated formation if we have both before and after
  if (found_before && found_after && min_before_diff < 5000 && min_after_diff < 5000) {
    // Only interpolate if formations are within 5 seconds
    stats_.cache_hits++;
    return TriangleDefenseUtils::InterpolateFormation(closest_before, closest_after, video_timestamp);
  }

  // Return closest formation if within reasonable range
  if (found_before && min_before_diff < 10000) { // 10 seconds
    stats_.cache_hits++;
    return closest_before;
  }
  
  if (found_after && min_after_diff < 10000) { // 10 seconds
    stats_.cache_hits++;
    return closest_after;
  }

  // No formation found
  stats_.cache_misses++;
  return FormationData();
}

QList<FormationData> TriangleDefenseSync::GetFormationsInRange(qint64 start_timestamp, qint64 end_timestamp) const
{
  QMutexLocker locker(&cache_mutex_);
  
  QList<FormationData> formations;
  
  for (auto it = formation_cache_.constBegin(); it != formation_cache_.constEnd(); ++it) {
    if (it.key() >= start_timestamp && it.key() <= end_timestamp) {
      formations.append(it.value());
    }
  }
  
  // Sort by timestamp
  std::sort(formations.begin(), formations.end(), 
           [](const FormationData& a, const FormationData& b) {
             return a.video_timestamp < b.video_timestamp;
           });
  
  return formations;
}

QList<CoachingAlert> TriangleDefenseSync::GetActiveAlerts() const
{
  QMutexLocker locker(&cache_mutex_);
  
  QList<CoachingAlert> active_alerts;
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  
  for (auto it = alert_cache_.constBegin(); it != alert_cache_.constEnd(); ++it) {
    const CoachingAlert& alert = it.value();
    
    // Consider alerts active if not acknowledged and less than 1 hour old
    if (!alert.acknowledged && (current_time - alert.alert_timestamp) < 3600000) {
      active_alerts.append(alert);
    }
  }
  
  // Sort by priority (highest first) then by timestamp (newest first)
  std::sort(active_alerts.begin(), active_alerts.end(),
           [](const CoachingAlert& a, const CoachingAlert& b) {
             if (a.priority_level != b.priority_level) {
               return a.priority_level > b.priority_level;
             }
             return a.alert_timestamp > b.alert_timestamp;
           });
  
  return active_alerts;
}

bool TriangleDefenseSync::AcknowledgeAlert(const QString& alert_id)
{
  QMutexLocker locker(&cache_mutex_);
  
  if (!alert_cache_.contains(alert_id)) {
    qWarning() << "Alert not found for acknowledgment:" << alert_id;
    return false;
  }
  
  alert_cache_[alert_id].acknowledged = true;
  
  // Update in database
  QSqlQuery query(cache_db_);
  query.prepare("UPDATE coaching_alerts SET acknowledged = 1 WHERE alert_id = ?");
  query.addBindValue(alert_id);
  
  if (!query.exec()) {
    qWarning() << "Failed to update alert acknowledgment:" << query.lastError().text();
    return false;
  }
  
  qInfo() << "Alert acknowledged:" << alert_id;
  return true;
}

QJsonObject TriangleDefenseSync::GetPipelineStatus() const
{
  QMutexLocker locker(&cache_mutex_);
  return pipeline_status_;
}

void TriangleDefenseSync::RefreshFormationData()
{
  qInfo() << "Refreshing formation data";
  
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  qint64 window_start = current_video_timestamp_ - 300000; // 5 minutes before
  qint64 window_end = current_video_timestamp_ + 300000;   // 5 minutes after
  
  FetchFormationsFromSupabase(window_start, window_end);
  FetchAlertsFromSupabase();
}

void TriangleDefenseSync::SetVideoTimestamp(qint64 timestamp)
{
  current_video_timestamp_ = timestamp;
  
  // Check if we need to fetch more data
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  if (current_time - last_data_fetch_ > 30000) { // Fetch every 30 seconds
    RefreshFormationData();
    last_data_fetch_ = current_time;
  }
}

void TriangleDefenseSync::SetRealTimeMode(bool enabled)
{
  real_time_enabled_ = enabled;
  
  if (enabled && websocket_ && websocket_->state() == QAbstractSocket::UnconnectedState) {
    websocket_->open(QUrl(websocket_url_));
  } else if (!enabled && websocket_) {
    websocket_->close();
  }
  
  qInfo() << "Real-time mode" << (enabled ? "enabled" : "disabled");
}

QJsonObject TriangleDefenseSync::GetSyncStatistics() const
{
  QJsonObject stats;
  stats["formations_processed"] = static_cast<qint64>(stats_.formations_processed);
  stats["alerts_processed"] = static_cast<qint64>(stats_.alerts_processed);
  stats["sync_operations"] = static_cast<qint64>(stats_.sync_operations);
  stats["network_requests"] = static_cast<qint64>(stats_.network_requests);
  stats["cache_hits"] = static_cast<qint64>(stats_.cache_hits);
  stats["cache_misses"] = static_cast<qint64>(stats_.cache_misses);
  stats["cache_hit_ratio"] = static_cast<double>(stats_.cache_hits) / 
                            qMax(1LL, stats_.cache_hits + stats_.cache_misses);
  stats["uptime_seconds"] = stats_.start_time.secsTo(QDateTime::currentDateTime());
  stats["is_connected"] = is_connected_;
  stats["cached_formations"] = formation_cache_.size();
  stats["cached_alerts"] = alert_cache_.size();
  
  return stats;
}

void TriangleDefenseSync::OnVideoPlaybackChanged(bool playing, qint64 position)
{
  video_playing_ = playing;
  current_video_timestamp_ = position;
  
  if (playing && real_time_enabled_) {
    if (sync_timer_ && !sync_timer_->isActive()) {
      sync_timer_->start();
    }
  } else {
    if (sync_timer_ && sync_timer_->isActive()) {
      sync_timer_->stop();
    }
  }
}

void TriangleDefenseSync::OnVideoSeekPerformed(qint64 new_position)
{
  current_video_timestamp_ = new_position;
  
  // Force immediate sync after seek
  OnSyncTimer();
}

void TriangleDefenseSync::OnVideoRateChanged(double rate)
{
  video_rate_ = rate;
  
  // Adjust sync interval based on playback rate
  if (sync_timer_) {
    int adjusted_interval = static_cast<int>(sync_interval_ms_ / qMax(0.1, rate));
    sync_timer_->setInterval(adjusted_interval);
  }
}

void TriangleDefenseSync::OnManualFormationMarked(qint64 timestamp, FormationType type, double confidence)
{
  FormationData formation;
  formation.formation_id = QString("manual_%1").arg(timestamp);
  formation.type = type;
  formation.confidence = confidence;
  formation.video_timestamp = timestamp;
  formation.detection_timestamp = QDateTime::currentMSecsSinceEpoch();
  formation.recommended_call = DetermineTriangleCall(formation);
  
  UpdateFormationCache(formation);
  UpdateSupabaseFormation(formation);
  
  emit FormationDetected(formation);
  emit FormationClassified(type, confidence, timestamp);
  
  qInfo() << "Manual formation marked:" << FormationTypeToString(type) 
          << "at" << timestamp << "confidence:" << confidence;
}

void TriangleDefenseSync::OnTriangleCallOverride(qint64 timestamp, TriangleCall call, const QString& reason)
{
  FormationData formation = GetFormationAt(timestamp);
  if (formation.formation_id.isEmpty()) {
    qWarning() << "No formation found for Triangle call override at timestamp:" << timestamp;
    return;
  }
  
  formation.recommended_call = call;
  formation.field_context["override_reason"] = reason;
  formation.field_context["manual_override"] = true;
  
  UpdateFormationCache(formation);
  UpdateSupabaseFormation(formation);
  
  emit TriangleCallRecommended(call, formation);
  
  qInfo() << "Triangle call overridden:" << TriangleCallToString(call) 
          << "at" << timestamp << "reason:" << reason;
}

void TriangleDefenseSync::OnSupabaseDataReceived()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  if (!reply) {
    return;
  }
  
  if (reply->error() != QNetworkReply::NoError) {
    qWarning() << "Supabase request failed:" << reply->errorString();
    HandleConnectionError(reply->errorString());
    reply->deleteLater();
    return;
  }
  
  QByteArray data = reply->readAll();
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  
  if (error.error != QJsonParseError::NoError) {
    qWarning() << "Failed to parse Supabase response:" << error.errorString();
    reply->deleteLater();
    return;
  }
  
  QJsonArray formations_array = doc.array();
  int formations_processed = 0;
  
  for (const QJsonValue& value : formations_array) {
    QJsonObject obj = value.toObject();
    
    FormationData formation;
    formation.formation_id = obj["id"].toString();
    formation.type = ParseFormationType(obj["formation_type"].toString());
    formation.confidence = obj["confidence"].toDouble();
    formation.video_timestamp = obj["video_timestamp"].toVariant().toLongLong();
    formation.detection_timestamp = obj["detection_timestamp"].toVariant().toLongLong();
    formation.hash_position = obj["hash_position"].toString();
    formation.field_zone = obj["field_zone"].toString();
    formation.player_positions = obj["player_positions"].toObject();
    formation.field_context = obj["field_context"].toObject();
    
    // Process M.E.L. results if available
    if (obj.contains("mel_results")) {
      QJsonObject mel_obj = obj["mel_results"].toObject();
      formation.mel_results.making_score = mel_obj["making_score"].toDouble();
      formation.mel_results.efficiency_score = mel_obj["efficiency_score"].toDouble();
      formation.mel_results.logical_score = mel_obj["logical_score"].toDouble();
      formation.mel_results.combined_score = mel_obj["combined_score"].toDouble();
      formation.mel_results.stage_status = mel_obj["stage_status"].toString();
      formation.mel_results.detailed_metrics = mel_obj["detailed_metrics"].toObject();
    }
    
    formation.recommended_call = DetermineTriangleCall(formation);
    
    UpdateFormationCache(formation);
    formations_processed++;
    
    emit FormationDetected(formation);
  }
  
  qInfo() << "Processed" << formations_processed << "formations from Supabase";
  emit DataRefreshed(formations_processed, 0);
  
  reply->deleteLater();
  current_reply_ = nullptr;
}

void TriangleDefenseSync::OnWebSocketConnected()
{
  qInfo() << "WebSocket connected for real-time updates";
  is_connected_ = true;
  reconnection_attempts_ = 0;
  
  if (reconnect_timer_ && reconnect_timer_->isActive()) {
    reconnect_timer_->stop();
  }
  
  if (heartbeat_timer_ && !heartbeat_timer_->isActive()) {
    heartbeat_timer_->start();
  }
  
  emit SyncStatusChanged(true);
}

void TriangleDefenseSync::OnWebSocketDisconnected()
{
  qWarning() << "WebSocket disconnected";
  is_connected_ = false;
  
  if (heartbeat_timer_ && heartbeat_timer_->isActive()) {
    heartbeat_timer_->stop();
  }
  
  AttemptReconnection();
  emit SyncStatusChanged(false);
}

void TriangleDefenseSync::OnWebSocketMessageReceived(const QString& message)
{
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
  
  if (error.error != QJsonParseError::NoError) {
    qWarning() << "Failed to parse WebSocket message:" << error.errorString();
    return;
  }
  
  QJsonObject obj = doc.object();
  QString event_type = obj["event"].toString();
  
  if (event_type == "formation_detected") {
    ProcessFormationDetection(obj["data"].toObject());
  } else if (event_type == "coaching_alert") {
    ProcessCoachingAlert(obj["data"].toObject());
  } else if (event_type == "mel_pipeline_update") {
    ProcessMELPipelineUpdate(obj["data"].toObject());
  } else if (event_type == "heartbeat_response") {
    heartbeat_received_ = true;
    last_heartbeat_ = QDateTime::currentMSecsSinceEpoch();
  }
}

void TriangleDefenseSync::OnNetworkReplyFinished()
{
  OnSupabaseDataReceived();
}

void TriangleDefenseSync::OnSyncTimer()
{
  if (!is_initialized_ || !video_playing_) {
    return;
  }
  
  stats_.sync_operations++;
  last_sync_timestamp_ = QDateTime::currentMSecsSinceEpoch();
  
  // Get current formation
  FormationData current_formation = GetFormationAt(current_video_timestamp_);
  
  if (!current_formation.formation_id.isEmpty()) {
    // Check if Triangle call has changed
    static TriangleCall last_call = TriangleCall::NoCall;
    if (current_formation.recommended_call != last_call) {
      emit TriangleCallRecommended(current_formation.recommended_call, current_formation);
      last_call = current_formation.recommended_call;
    }
    
    // Check for high urgency situations
    double urgency = TriangleDefenseUtils::CalculateDefensiveUrgency(current_formation);
    if (urgency > 0.8) { // High urgency threshold
      CoachingAlert alert;
      alert.alert_id = QString("urgency_%1").arg(current_video_timestamp_);
      alert.alert_type = "high_urgency_formation";
      alert.message = QString("High urgency %1 formation detected - %2 recommended")
                     .arg(FormationTypeToString(current_formation.type))
                     .arg(TriangleCallToString(current_formation.recommended_call));
      alert.priority_level = 4;
      alert.video_timestamp = current_video_timestamp_;
      alert.alert_timestamp = QDateTime::currentMSecsSinceEpoch();
      alert.related_formation = current_formation;
      
      UpdateAlertCache(alert);
      emit CoachingAlertReceived(alert);
    }
  }
}

void TriangleDefenseSync::OnCacheCleanupTimer()
{
  CleanupOldData();
}

void TriangleDefenseSync::OnHeartbeatTimer()
{
  SendHeartbeat();
  
  // Check if we received heartbeat response
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  if (current_time - last_heartbeat_ > 30000) { // 30 seconds timeout
    qWarning() << "Heartbeat timeout, attempting reconnection";
    AttemptReconnection();
  }
}

void TriangleDefenseSync::SetupDatabase()
{
  // Create unique connection name
  db_connection_name_ = QString("triangle_defense_sync_%1")
                       .arg(reinterpret_cast<quintptr>(this));
  
  // Setup database path
  QString app_data_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(app_data_path);
  QString db_path = QDir(app_data_path).filePath("triangle_defense_cache.db");
  
  // Open SQLite database
  cache_db_ = QSqlDatabase::addDatabase("QSQLITE", db_connection_name_);
  cache_db_.setDatabaseName(db_path);
  
  if (!cache_db_.open()) {
    qCritical() << "Failed to open cache database:" << cache_db_.lastError().text();
    return;
  }
  
  // Create tables
  QSqlQuery query(cache_db_);
  
  // Formations table
  query.exec(R"(
    CREATE TABLE IF NOT EXISTS formations (
      formation_id TEXT PRIMARY KEY,
      formation_type INTEGER,
      confidence REAL,
      video_timestamp INTEGER,
      detection_timestamp INTEGER,
      recommended_call INTEGER,
      hash_position TEXT,
      field_zone TEXT,
      mel_making_score REAL,
      mel_efficiency_score REAL,
      mel_logical_score REAL,
      mel_combined_score REAL,
      player_positions TEXT,
      field_context TEXT,
      mel_detailed_metrics TEXT
    )
  )");
  
  // Coaching alerts table
  query.exec(R"(
    CREATE TABLE IF NOT EXISTS coaching_alerts (
      alert_id TEXT PRIMARY KEY,
      alert_type TEXT,
      message TEXT,
      target_staff TEXT,
      priority_level INTEGER,
      alert_timestamp INTEGER,
      video_timestamp INTEGER,
      acknowledged INTEGER DEFAULT 0,
      context_data TEXT
    )
  )");
  
  // Create indexes
  query.exec("CREATE INDEX IF NOT EXISTS idx_formations_timestamp ON formations(video_timestamp)");
  query.exec("CREATE INDEX IF NOT EXISTS idx_alerts_timestamp ON coaching_alerts(alert_timestamp)");
  
  qInfo() << "Cache database initialized:" << db_path;
}

void TriangleDefenseSync::SetupNetworking()
{
  network_manager_ = new QNetworkAccessManager(this);
  connect(network_manager_, &QNetworkAccessManager::finished,
          this, &TriangleDefenseSync::OnNetworkReplyFinished);
}

void TriangleDefenseSync::SetupWebSocket()
{
  if (websocket_) {
    websocket_->deleteLater();
  }
  
  websocket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
  
  connect(websocket_, &QWebSocket::connected, this, &TriangleDefenseSync::OnWebSocketConnected);
  connect(websocket_, &QWebSocket::disconnected, this, &TriangleDefenseSync::OnWebSocketDisconnected);
  connect(websocket_, &QWebSocket::textMessageReceived, this, &TriangleDefenseSync::OnWebSocketMessageReceived);
  
  if (real_time_enabled_) {
    websocket_->open(QUrl(websocket_url_));
  }
}

void TriangleDefenseSync::SetupTimers()
{
  // Sync timer
  sync_timer_ = new QTimer(this);
  sync_timer_->setInterval(sync_interval_ms_);
  connect(sync_timer_, &QTimer::timeout, this, &TriangleDefenseSync::OnSyncTimer);
  
  // Cache cleanup timer (every hour)
  cache_cleanup_timer_ = new QTimer(this);
  cache_cleanup_timer_->setInterval(3600000);
  connect(cache_cleanup_timer_, &QTimer::timeout, this, &TriangleDefenseSync::OnCacheCleanupTimer);
  cache_cleanup_timer_->start();
  
  // Heartbeat timer (every 15 seconds)
  heartbeat_timer_ = new QTimer(this);
  heartbeat_timer_->setInterval(15000);
  connect(heartbeat_timer_, &QTimer::timeout, this, &TriangleDefenseSync::OnHeartbeatTimer);
  
  // Reconnect timer
  reconnect_timer_ = new QTimer(this);
  reconnect_timer_->setInterval(5000); // 5 seconds
  connect(reconnect_timer_, &QTimer::timeout, this, &TriangleDefenseSync::AttemptReconnection);
}

void TriangleDefenseSync::LoadFormationData()
{
  QSqlQuery query(cache_db_);
  query.prepare("SELECT * FROM formations ORDER BY video_timestamp DESC LIMIT ?");
  query.addBindValue(max_cached_formations_);
  
  if (!query.exec()) {
    qWarning() << "Failed to load formations from cache:" << query.lastError().text();
    return;
  }
  
  QMutexLocker locker(&cache_mutex_);
  formation_cache_.clear();
  
  while (query.next()) {
    FormationData formation;
    formation.formation_id = query.value("formation_id").toString();
    formation.type = static_cast<FormationType>(query.value("formation_type").toInt());
    formation.confidence = query.value("confidence").toDouble();
    formation.video_timestamp = query.value("video_timestamp").toLongLong();
    formation.detection_timestamp = query.value("detection_timestamp").toLongLong();
    formation.recommended_call = static_cast<TriangleCall>(query.value("recommended_call").toInt());
    formation.hash_position = query.value("hash_position").toString();
    formation.field_zone = query.value("field_zone").toString();
    
    // Parse JSON fields
    formation.player_positions = QJsonDocument::fromJson(
      query.value("player_positions").toString().toUtf8()).object();
    formation.field_context = QJsonDocument::fromJson(
      query.value("field_context").toString().toUtf8()).object();
    
    // Load M.E.L. results
    formation.mel_results.making_score = query.value("mel_making_score").toDouble();
    formation.mel_results.efficiency_score = query.value("mel_efficiency_score").toDouble();
    formation.mel_results.logical_score = query.value("mel_logical_score").toDouble();
    formation.mel_results.combined_score = query.value("mel_combined_score").toDouble();
    formation.mel_results.detailed_metrics = QJsonDocument::fromJson(
      query.value("mel_detailed_metrics").toString().toUtf8()).object();
    
    formation_cache_[formation.video_timestamp] = formation;
  }
  
  qInfo() << "Loaded" << formation_cache_.size() << "formations from cache";
}

void TriangleDefenseSync::LoadCoachingAlerts()
{
  QSqlQuery query(cache_db_);
  query.prepare(R"(
    SELECT * FROM coaching_alerts 
    WHERE alert_timestamp > ? 
    ORDER BY alert_timestamp DESC
  )");
  
  qint64 cutoff_time = QDateTime::currentMSecsSinceEpoch() - (cache_retention_hours_ * 3600000);
  query.addBindValue(cutoff_time);
  
  if (!query.exec()) {
    qWarning() << "Failed to load alerts from cache:" << query.lastError().text();
    return;
  }
  
  QMutexLocker locker(&cache_mutex_);
  alert_cache_.clear();
  
  while (query.next()) {
    CoachingAlert alert;
    alert.alert_id = query.value("alert_id").toString();
    alert.alert_type = query.value("alert_type").toString();
    alert.message = query.value("message").toString();
    alert.target_staff = query.value("target_staff").toString();
    alert.priority_level = query.value("priority_level").toInt();
    alert.alert_timestamp = query.value("alert_timestamp").toLongLong();
    alert.video_timestamp = query.value("video_timestamp").toLongLong();
    alert.acknowledged = query.value("acknowledged").toBool();
    alert.context_data = QJsonDocument::fromJson(
      query.value("context_data").toString().toUtf8()).object();
    
    alert_cache_[alert.alert_id] = alert;
  }
  
  qInfo() << "Loaded" << alert_cache_.size() << "alerts from cache";
}

FormationType TriangleDefenseSync::ParseFormationType(const QString& type_string) const
{
  QString type = type_string.toLower();
  if (type == "larry") return FormationType::Larry;
  if (type == "linda") return FormationType::Linda;
  if (type == "rita") return FormationType::Rita;
  if (type == "ricky") return FormationType::Ricky;
  if (type == "randy") return FormationType::Randy;
  if (type == "pat") return FormationType::Pat;
  return FormationType::Unknown;
}

TriangleCall TriangleDefenseSync::ParseTriangleCall(const QString& call_string) const
{
  QString call = call_string.toLower();
  if (call == "strong_side") return TriangleCall::StrongSide;
  if (call == "weak_side") return TriangleCall::WeakSide;
  if (call == "middle_hash") return TriangleCall::MiddleHash;
  if (call == "left_hash") return TriangleCall::LeftHash;
  if (call == "right_hash") return TriangleCall::RightHash;
  if (call == "red_zone") return TriangleCall::RedZone;
  if (call == "goal_line") return TriangleCall::GoalLine;
  return TriangleCall::NoCall;
}

QString TriangleDefenseSync::FormationTypeToString(FormationType type) const
{
  switch (type) {
    case FormationType::Larry: return "Larry";
    case FormationType::Linda: return "Linda";
    case FormationType::Rita: return "Rita";
    case FormationType::Ricky: return "Ricky";
    case FormationType::Randy: return "Randy";
    case FormationType::Pat: return "Pat";
    default: return "Unknown";
  }
}

QString TriangleDefenseSync::TriangleCallToString(TriangleCall call) const
{
  switch (call) {
    case TriangleCall::StrongSide: return "Strong Side";
    case TriangleCall::WeakSide: return "Weak Side";
    case TriangleCall::MiddleHash: return "Middle Hash";
    case TriangleCall::LeftHash: return "Left Hash";
    case TriangleCall::RightHash: return "Right Hash";
    case TriangleCall::RedZone: return "Red Zone";
    case TriangleCall::GoalLine: return "Goal Line";
    default: return "No Call";
  }
}

void TriangleDefenseSync::UpdateFormationCache(const FormationData& formation)
{
  QMutexLocker locker(&cache_mutex_);
  formation_cache_[formation.video_timestamp] = formation;
  stats_.formations_processed++;
  
  // Store in database
  QSqlQuery query(cache_db_);
  query.prepare(R"(
    INSERT OR REPLACE INTO formations 
    (formation_id, formation_type, confidence, video_timestamp, detection_timestamp,
     recommended_call, hash_position, field_zone, mel_making_score, mel_efficiency_score,
     mel_logical_score, mel_combined_score, player_positions, field_context, mel_detailed_metrics)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
  )");
  
  query.addBindValue(formation.formation_id);
  query.addBindValue(static_cast<int>(formation.type));
  query.addBindValue(formation.confidence);
  query.addBindValue(formation.video_timestamp);
  query.addBindValue(formation.detection_timestamp);
  query.addBindValue(static_cast<int>(formation.recommended_call));
  query.addBindValue(formation.hash_position);
  query.addBindValue(formation.field_zone);
  query.addBindValue(formation.mel_results.making_score);
  query.addBindValue(formation.mel_results.efficiency_score);
  query.addBindValue(formation.mel_results.logical_score);
  query.addBindValue(formation.mel_results.combined_score);
  query.addBindValue(QJsonDocument(formation.player_positions).toJson(QJsonDocument::Compact));
  query.addBindValue(QJsonDocument(formation.field_context).toJson(QJsonDocument::Compact));
  query.addBindValue(QJsonDocument(formation.mel_results.detailed_metrics).toJson(QJsonDocument::Compact));
  
  if (!query.exec()) {
    qWarning() << "Failed to cache formation:" << query.lastError().text();
  }
}

void TriangleDefenseSync::UpdateAlertCache(const CoachingAlert& alert)
{
  QMutexLocker locker(&cache_mutex_);
  alert_cache_[alert.alert_id] = alert;
  stats_.alerts_processed++;
  
  // Store in database
  QSqlQuery query(cache_db_);
  query.prepare(R"(
    INSERT OR REPLACE INTO coaching_alerts
    (alert_id, alert_type, message, target_staff, priority_level,
     alert_timestamp, video_timestamp, acknowledged, context_data)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
  )");
  
  query.addBindValue(alert.alert_id);
  query.addBindValue(alert.alert_type);
  query.addBindValue(alert.message);
  query.addBindValue(alert.target_staff);
  query.addBindValue(alert.priority_level);
  query.addBindValue(alert.alert_timestamp);
  query.addBindValue(alert.video_timestamp);
  query.addBindValue(alert.acknowledged ? 1 : 0);
  query.addBindValue(QJsonDocument(alert.context_data).toJson(QJsonDocument::Compact));
  
  if (!query.exec()) {
    qWarning() << "Failed to cache alert:" << query.lastError().text();
  }
}

TriangleCall TriangleDefenseSync::DetermineTriangleCall(const FormationData& formation) const
{
  // Determine Triangle Defense call based on formation characteristics
  
  // Red zone scenarios
  if (formation.field_zone.contains("Red Zone", Qt::CaseInsensitive)) {
    if (formation.type == FormationType::Larry || formation.type == FormationType::Linda) {
      return TriangleCall::GoalLine; // Tight formations near goal line
    }
    return TriangleCall::RedZone;
  }
  
  // Hash position influence
  if (formation.hash_position == "L") {
    return TriangleCall::LeftHash;
  } else if (formation.hash_position == "R") {
    return TriangleCall::RightHash;
  } else if (formation.hash_position == "M") {
    return TriangleCall::MiddleHash;
  }
  
  // Formation type influence
  switch (formation.type) {
    case FormationType::Larry:
    case FormationType::Linda:
      // Tight formations - focus on strong side
      return TriangleCall::StrongSide;
      
    case FormationType::Rita:
    case FormationType::Randy:
      // Loose formations - cover weak side
      return TriangleCall::WeakSide;
      
    case FormationType::Ricky:
      // Balanced approach for moderate formations
      return formation.mel_results.combined_score > 70.0 ? 
             TriangleCall::StrongSide : TriangleCall::WeakSide;
      
    default:
      return TriangleCall::NoCall;
  }
}

bool TriangleDefenseSync::FetchFormationsFromSupabase(qint64 start_time, qint64 end_time)
{
  if (supabase_url_.isEmpty() || api_key_.isEmpty()) {
    qWarning() << "Supabase credentials not configured";
    return false;
  }
  
  QUrl url(supabase_url_ + "/rest/v1/formations");
  QUrlQuery query;
  query.addQueryItem("video_timestamp", QString("gte.%1").arg(start_time));
  query.addQueryItem("video_timestamp", QString("lte.%1").arg(end_time));
  query.addQueryItem("order", "video_timestamp.asc");
  query.addQueryItem("limit", "1000");
  url.setQuery(query);
  
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setRawHeader("Authorization", QString("Bearer %1").arg(api_key_).toUtf8());
  request.setRawHeader("apikey", api_key_.toUtf8());
  
  if (current_reply_) {
    current_reply_->abort();
  }
  
  current_reply_ = network_manager_->get(request);
  stats_.network_requests++;
  
  qDebug() << "Fetching formations from Supabase:" << start_time << "to" << end_time;
  return true;
}

void TriangleDefenseSync::ProcessFormationDetection(const QJsonObject& detection)
{
  FormationData formation;
  formation.formation_id = detection["formation_id"].toString();
  formation.type = ParseFormationType(detection["formation_type"].toString());
  formation.confidence = detection["confidence"].toDouble();
  formation.video_timestamp = detection["video_timestamp"].toVariant().toLongLong();
  formation.detection_timestamp = QDateTime::currentMSecsSinceEpoch();
  formation.hash_position = detection["hash_position"].toString();
  formation.field_zone = detection["field_zone"].toString();
  formation.player_positions = detection["player_positions"].toObject();
  formation.field_context = detection["field_context"].toObject();
  formation.recommended_call = DetermineTriangleCall(formation);
  
  UpdateFormationCache(formation);
  
  emit FormationDetected(formation);
  emit FormationClassified(formation.type, formation.confidence, formation.video_timestamp);
  emit TriangleCallRecommended(formation.recommended_call, formation);
  
  qInfo() << "Real-time formation detected:" << FormationTypeToString(formation.type)
          << "at" << formation.video_timestamp;
}

void TriangleDefenseSync::CleanupOldData()
{
  qint64 cutoff_time = QDateTime::currentMSecsSinceEpoch() - (cache_retention_hours_ * 3600000);
  
  // Clean formations cache
  {
    QMutexLocker locker(&cache_mutex_);
    auto it = formation_cache_.begin();
    while (it != formation_cache_.end()) {
      if (it.value().detection_timestamp < cutoff_time) {
        it = formation_cache_.erase(it);
      } else {
        ++it;
      }
    }
  }
  
  // Clean database
  QSqlQuery query(cache_db_);
  query.prepare("DELETE FROM formations WHERE detection_timestamp < ?");
  query.addBindValue(cutoff_time);
  query.exec();
  
  query.prepare("DELETE FROM coaching_alerts WHERE alert_timestamp < ?");
  query.addBindValue(cutoff_time);
  query.exec();
  
  qDebug() << "Cleaned up old cache data";
}

void TriangleDefenseSync::SendHeartbeat()
{
  if (websocket_ && websocket_->state() == QAbstractSocket::ConnectedState) {
    QJsonObject heartbeat;
    heartbeat["event"] = "heartbeat";
    heartbeat["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    heartbeat["client_id"] = "apache-cleats";
    
    QJsonDocument doc(heartbeat);
    websocket_->sendTextMessage(doc.toJson(QJsonDocument::Compact));
    
    heartbeat_received_ = false; // Reset flag
  }
}

void TriangleDefenseSync::AttemptReconnection()
{
  if (is_connected_ || !real_time_enabled_) {
    return;
  }
  
  reconnection_attempts_++;
  qInfo() << "Attempting WebSocket reconnection, attempt:" << reconnection_attempts_;
  
  if (websocket_) {
    websocket_->open(QUrl(websocket_url_));
  }
  
  if (reconnection_attempts_ < 10) {
    if (reconnect_timer_ && !reconnect_timer_->isActive()) {
      reconnect_timer_->start();
    }
  } else {
    qWarning() << "Max reconnection attempts reached, stopping auto-reconnect";
    if (reconnect_timer_) {
      reconnect_timer_->stop();
    }
  }
}

// MELPipelineProcessor implementation and other utility functions would continue here...
// Due to length constraints, I'll include the key remaining methods

void TriangleDefenseSync::ProcessMELPipelineUpdate(const QJsonObject& update)
{
  QString formation_id = update["formation_id"].toString();
  QString stage = update["stage"].toString();
  QString status = update["status"].toString();
  QJsonObject metrics = update["metrics"].toObject();
  
  // Update pipeline status
  {
    QMutexLocker locker(&cache_mutex_);
    pipeline_status_[stage] = update;
  }
  
  // Update formation with M.E.L. results if available
  if (!formation_id.isEmpty() && status == "completed") {
    QMutexLocker locker(&cache_mutex_);
    
    // Find formation by ID
    for (auto& formation : formation_cache_) {
      if (formation.formation_id == formation_id) {
        // Update M.E.L. results based on stage
        if (stage == "making") {
          formation.mel_results.making_score = metrics["score"].toDouble();
        } else if (stage == "efficiency") {
          formation.mel_results.efficiency_score = metrics["score"].toDouble();
        } else if (stage == "logical") {
          formation.mel_results.logical_score = metrics["score"].toDouble();
        }
        
        // Recalculate combined score
        formation.mel_results.combined_score = 
          (formation.mel_results.making_score + 
           formation.mel_results.efficiency_score + 
           formation.mel_results.logical_score) / 3.0;
        
        formation.mel_results.detailed_metrics = metrics;
        formation.mel_results.processing_timestamp = QDateTime::currentMSecsSinceEpoch();
        
        // Re-determine Triangle call with updated M.E.L. data
        formation.recommended_call = DetermineTriangleCall(formation);
        
        UpdateFormationCache(formation);
        emit MELResultsUpdated(formation_id, formation.mel_results);
        emit FormationUpdated(formation);
        break;
      }
    }
  }
  
  emit PipelineStatusChanged(stage, status);
}

void TriangleDefenseSync::ProcessCoachingAlert(const QJsonObject& alert_obj)
{
  CoachingAlert alert;
  alert.alert_id = alert_obj["alert_id"].toString();
  alert.alert_type = alert_obj["alert_type"].toString();
  alert.message = alert_obj["message"].toString();
  alert.target_staff = alert_obj["target_staff"].toString();
  alert.priority_level = alert_obj["priority_level"].toInt();
  alert.video_timestamp = alert_obj["video_timestamp"].toVariant().toLongLong();
  alert.alert_timestamp = QDateTime::currentMSecsSinceEpoch();
  alert.context_data = alert_obj["context_data"].toObject();
  
  UpdateAlertCache(alert);
  
  emit CoachingAlertReceived(alert);
  
  if (alert.priority_level >= 4) {
    emit CriticalAlertReceived(alert);
  }
  
  qInfo() << "Coaching alert received:" << alert.alert_type 
          << "priority:" << alert.priority_level;
}

// TriangleDefenseUtils namespace implementation
namespace TriangleDefenseUtils {

double CalculateFormationSimilarity(const FormationData& f1, const FormationData& f2)
{
  if (f1.type != f2.type) {
    return 0.0; // Different formation types are not similar
  }
  
  double similarity = 0.0;
  
  // Type similarity (already checked above)
  similarity += 0.4;
  
  // Confidence similarity
  double conf_diff = qAbs(f1.confidence - f2.confidence);
  similarity += (1.0 - conf_diff) * 0.2;
  
  // Hash position similarity
  if (f1.hash_position == f2.hash_position) {
    similarity += 0.2;
  }
  
  // Field zone similarity
  if (f1.field_zone == f2.field_zone) {
    similarity += 0.1;
  }
  
  // M.E.L. score similarity
  double mel_diff = qAbs(f1.mel_results.combined_score - f2.mel_results.combined_score);
  similarity += (1.0 - mel_diff / 100.0) * 0.1;
  
  return qBound(0.0, similarity, 1.0);
}

FormationData InterpolateFormation(const FormationData& before, const FormationData& after,
                                  qint64 target_timestamp)
{
  // Simple interpolation - in practice you might want more sophisticated blending
  double ratio = static_cast<double>(target_timestamp - before.video_timestamp) /
                (after.video_timestamp - before.video_timestamp);
  
  FormationData interpolated = before; // Start with before formation
  interpolated.video_timestamp = target_timestamp;
  
  // Interpolate confidence
  interpolated.confidence = before.confidence + 
                           (after.confidence - before.confidence) * ratio;
  
  // Interpolate M.E.L. scores
  interpolated.mel_results.making_score = before.mel_results.making_score +
    (after.mel_results.making_score - before.mel_results.making_score) * ratio;
  
  interpolated.mel_results.efficiency_score = before.mel_results.efficiency_score +
    (after.mel_results.efficiency_score - before.mel_results.efficiency_score) * ratio;
  
  interpolated.mel_results.logical_score = before.mel_results.logical_score +
    (after.mel_results.logical_score - before.mel_results.logical_score) * ratio;
  
  interpolated.mel_results.combined_score = before.mel_results.combined_score +
    (after.mel_results.combined_score - before.mel_results.combined_score) * ratio;
  
  // Use the more recent formation type and call if they differ
  if (ratio > 0.5) {
    interpolated.type = after.type;
    interpolated.recommended_call = after.recommended_call;
    interpolated.hash_position = after.hash_position;
    interpolated.field_zone = after.field_zone;
  }
  
  return interpolated;
}

bool ValidateFormationData(const FormationData& formation)
{
  if (formation.formation_id.isEmpty()) {
    return false;
  }
  
  if (formation.confidence < 0.0 || formation.confidence > 1.0) {
    return false;
  }
  
  if (formation.video_timestamp <= 0) {
    return false;
  }
  
  if (formation.type == FormationType::Unknown) {
    return false;
  }
  
  return true;
}

QString VideoTimestampToGameClock(qint64 video_timestamp, qint64 game_start_timestamp)
{
  qint64 game_time_ms = video_timestamp - game_start_timestamp;
  
  // Convert to game clock format (assumes 15-minute quarters)
  int total_seconds = static_cast<int>(game_time_ms / 1000);
  int quarter = (total_seconds / 900) + 1; // 900 seconds = 15 minutes
  int quarter_seconds = total_seconds % 900;
  int clock_seconds = 900 - quarter_seconds; // Count down
  
  int minutes = clock_seconds / 60;
  int seconds = clock_seconds % 60;
  
  return QString("Q%1 %2:%3")
         .arg(quarter)
         .arg(minutes, 2, 10, QChar('0'))
         .arg(seconds, 2, 10, QChar('0'));
}

QString DetermineHashPosition(const QJsonObject& field_context)
{
  double field_x = field_context["field_x"].toDouble();
  
  // Assuming field coordinates where 0 is left hash, 50 is middle, 100 is right hash
  if (field_x < 20) {
    return "L"; // Left hash
  } else if (field_x > 80) {
    return "R"; // Right hash
  } else {
    return "M"; // Middle hash
  }
}

double CalculateDefensiveUrgency(const FormationData& formation)
{
  double urgency = 0.0;
  
  // Formation type urgency
  switch (formation.type) {
    case FormationType::Larry:
    case FormationType::Linda:
      urgency += 0.8; // Tight formations are urgent
      break;
    case FormationType::Rita:
      urgency += 0.6; // Moderate urgency
      break;
    case FormationType::Ricky:
    case FormationType::Randy:
      urgency += 0.4; // Lower urgency for loose formations
      break;
    default:
      urgency += 0.2;
  }
  
  // Field zone urgency
  if (formation.field_zone.contains("Red Zone", Qt::CaseInsensitive)) {
    urgency += 0.3;
  } else if (formation.field_zone.contains("Goal Line", Qt::CaseInsensitive)) {
    urgency += 0.5;
  }
  
  // M.E.L. score influence
  if (formation.mel_results.combined_score > 85.0) {
    urgency += 0.2; // High M.E.L. score increases urgency
  }
  
  // Confidence influence
  urgency *= formation.confidence; // Scale by detection confidence
  
  return qBound(0.0, urgency, 1.0);
}

} // namespace TriangleDefenseUtils

} // namespace olive
