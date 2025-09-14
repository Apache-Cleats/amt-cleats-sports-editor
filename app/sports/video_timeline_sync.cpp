/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Video Timeline Synchronization Service Implementation
  Real-time coordination between video playback and Triangle Defense data
***/

#include "video_timeline_sync.h"

#include <QDebug>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QToolTip>
#include <QApplication>
#include <QRandomGenerator>
#include <QEasingCurve>
#include <QtMath>
#include <QJsonDocument>
#include <QJsonParseError>

#include "panel/timeline/timeline.h"

namespace olive {

VideoTimelineSync::VideoTimelineSync(QObject* parent)
  : QObject(parent)
  , triangle_defense_sync_(nullptr)
  , timeline_panel_(nullptr)
  , is_initialized_(false)
  , real_time_sync_active_(false)
  , current_video_position_(0)
  , last_sync_timestamp_(0)
  , video_playback_rate_(1.0)
  , video_playing_(false)
  , sync_timer_(nullptr)
  , marker_animation_timer_(nullptr)
  , statistics_timer_(nullptr)
  , cleanup_timer_(nullptr)
  , sync_precision_ms_(100)
  , marker_animations_enabled_(true)
  , max_queue_size_(1000)
  , event_processing_active_(false)
  , max_markers_(10000)
  , timeline_view_(nullptr)
  , timeline_scene_(nullptr)
  , scroll_animation_(nullptr)
  , marker_animation_group_(nullptr)
  , auto_cleanup_enabled_(true)
  , cleanup_interval_ms_(300000) // 5 minutes
  , marker_retention_time_ms_(3600000) // 1 hour
  , rendering_optimizations_enabled_(true)
  , latency_history_size_(100)
  , debug_mode_enabled_(false)
{
  qInfo() << "Initializing Video Timeline Sync";
  
  // Initialize marker colors
  marker_colors_[TimelineMarkerType::Formation] = QColor(50, 150, 255);        // Blue
  marker_colors_[TimelineMarkerType::TriangleCall] = QColor(255, 100, 50);     // Orange
  marker_colors_[TimelineMarkerType::CoachingAlert] = QColor(255, 50, 50);     // Red
  marker_colors_[TimelineMarkerType::ManualAnnotation] = QColor(100, 255, 100); // Green
  marker_colors_[TimelineMarkerType::MELScore] = QColor(200, 100, 255);        // Purple
  marker_colors_[TimelineMarkerType::VideoEvent] = QColor(150, 150, 150);      // Gray
  marker_colors_[TimelineMarkerType::Highlight] = QColor(255, 255, 100);       // Yellow
  
  // Initialize marker visibility
  marker_visibility_[TimelineMarkerType::Formation] = true;
  marker_visibility_[TimelineMarkerType::TriangleCall] = true;
  marker_visibility_[TimelineMarkerType::CoachingAlert] = true;
  marker_visibility_[TimelineMarkerType::ManualAnnotation] = true;
  marker_visibility_[TimelineMarkerType::MELScore] = true;
  marker_visibility_[TimelineMarkerType::VideoEvent] = false; // Hidden by default
  marker_visibility_[TimelineMarkerType::Highlight] = true;
  
  // Setup graphics components
  timeline_scene_ = new QGraphicsScene(this);
  timeline_view_ = new QGraphicsView(timeline_scene_);
  timeline_view_->setRenderHint(QPainter::Antialiasing);
  timeline_view_->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
  timeline_view_->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
  
  // Setup animation groups
  marker_animation_group_ = new QParallelAnimationGroup(this);
  scroll_animation_ = new QPropertyAnimation(this);
  
  SetupTimers();
  
  qInfo() << "Video Timeline Sync initialized";
}

VideoTimelineSync::~VideoTimelineSync()
{
  Shutdown();
}

bool VideoTimelineSync::Initialize(TriangleDefenseSync* triangle_sync)
{
  if (is_initialized_) {
    qWarning() << "Video Timeline Sync already initialized";
    return true;
  }

  triangle_defense_sync_ = triangle_sync;

  if (triangle_defense_sync_) {
    // Connect Triangle Defense sync signals
    connect(triangle_defense_sync_, &TriangleDefenseSync::FormationDetected,
            this, &VideoTimelineSync::OnFormationDetected);
    connect(triangle_defense_sync_, &TriangleDefenseSync::FormationUpdated,
            this, &VideoTimelineSync::OnFormationUpdated);
    connect(triangle_defense_sync_, &TriangleDefenseSync::TriangleCallRecommended,
            this, &VideoTimelineSync::OnTriangleCallRecommended);
    connect(triangle_defense_sync_, &TriangleDefenseSync::CoachingAlertReceived,
            this, &VideoTimelineSync::OnCoachingAlertReceived);
    connect(triangle_defense_sync_, &TriangleDefenseSync::CriticalAlertReceived,
            this, &VideoTimelineSync::OnCriticalAlertReceived);
    connect(triangle_defense_sync_, &TriangleDefenseSync::MELResultsUpdated,
            this, &VideoTimelineSync::OnMELResultsUpdated);
    connect(triangle_defense_sync_, &TriangleDefenseSync::PipelineStatusChanged,
            this, &VideoTimelineSync::OnPipelineStatusChanged);

    qInfo() << "Triangle Defense sync connected to Video Timeline Sync";
  }

  // Start statistics timer
  if (statistics_timer_) {
    statistics_timer_->start();
  }

  is_initialized_ = true;
  qInfo() << "Video Timeline Sync initialization completed";

  return true;
}

void VideoTimelineSync::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Video Timeline Sync";

  // Stop real-time sync if active
  if (real_time_sync_active_) {
    StopRealTimeSync();
  }

  // Stop all timers
  if (sync_timer_ && sync_timer_->isActive()) {
    sync_timer_->stop();
  }
  if (marker_animation_timer_ && marker_animation_timer_->isActive()) {
    marker_animation_timer_->stop();
  }
  if (statistics_timer_ && statistics_timer_->isActive()) {
    statistics_timer_->stop();
  }
  if (cleanup_timer_ && cleanup_timer_->isActive()) {
    cleanup_timer_->stop();
  }

  // Stop all animations
  if (marker_animation_group_) {
    marker_animation_group_->stop();
  }
  if (scroll_animation_) {
    scroll_animation_->stop();
  }

  // Clear all markers and events
  ClearMarkers();
  
  {
    QMutexLocker locker(&event_mutex_);
    event_queue_.clear();
  }

  // Clean up graphics
  if (timeline_scene_) {
    timeline_scene_->clear();
  }

  is_initialized_ = false;
  qInfo() << "Video Timeline Sync shutdown complete";
}

void VideoTimelineSync::ConnectTimelinePanel(TimelinePanel* timeline_panel)
{
  if (timeline_panel_) {
    // Disconnect previous panel
    disconnect(timeline_panel_, nullptr, this, nullptr);
  }

  timeline_panel_ = timeline_panel;

  if (timeline_panel_) {
    // Connect timeline panel signals
    connect(timeline_panel_, &TimelinePanel::PlaybackStarted,
            this, &VideoTimelineSync::OnVideoPlaybackStarted);
    connect(timeline_panel_, &TimelinePanel::PlaybackStopped,
            this, &VideoTimelineSync::OnVideoPlaybackStopped);
    connect(timeline_panel_, &TimelinePanel::PositionChanged,
            this, &VideoTimelineSync::OnVideoPositionChanged);
    connect(timeline_panel_, &TimelinePanel::SeekPerformed,
            this, &VideoTimelineSync::OnVideoSeekPerformed);
    connect(timeline_panel_, &TimelinePanel::RateChanged,
            this, &VideoTimelineSync::OnVideoRateChanged);

    qInfo() << "Timeline panel connected to Video Timeline Sync";
  }
}

void VideoTimelineSync::StartRealTimeSync()
{
  if (!is_initialized_) {
    qWarning() << "Video Timeline Sync not initialized";
    return;
  }

  if (real_time_sync_active_) {
    qWarning() << "Real-time sync already active";
    return;
  }

  qInfo() << "Starting real-time timeline synchronization";

  real_time_sync_active_ = true;
  event_processing_active_ = true;

  // Start sync timer
  if (sync_timer_) {
    sync_timer_->start();
  }

  // Start marker animation timer if animations enabled
  if (marker_animations_enabled_ && marker_animation_timer_) {
    marker_animation_timer_->start();
  }

  // Start cleanup timer if auto-cleanup enabled
  if (auto_cleanup_enabled_ && cleanup_timer_) {
    cleanup_timer_->start();
  }

  sync_latency_timer_.start();
  emit SyncStarted();

  qInfo() << "Real-time timeline synchronization started";
}

void VideoTimelineSync::StopRealTimeSync()
{
  if (!real_time_sync_active_) {
    return;
  }

  qInfo() << "Stopping real-time timeline synchronization";

  real_time_sync_active_ = false;
  event_processing_active_ = false;

  // Stop timers
  if (sync_timer_ && sync_timer_->isActive()) {
    sync_timer_->stop();
  }
  if (marker_animation_timer_ && marker_animation_timer_->isActive()) {
    marker_animation_timer_->stop();
  }
  if (cleanup_timer_ && cleanup_timer_->isActive()) {
    cleanup_timer_->stop();
  }

  // Stop animations
  if (marker_animation_group_) {
    marker_animation_group_->stop();
  }

  emit SyncStopped();
  qInfo() << "Real-time timeline synchronization stopped";
}

void VideoTimelineSync::UpdateVideoPosition(qint64 timestamp)
{
  current_video_position_ = timestamp;

  if (real_time_sync_active_) {
    // Create sync event for position update
    SyncEvent event;
    event.event_id = QString("pos_update_%1").arg(timestamp);
    event.event_type = "position_update";
    event.video_timestamp = timestamp;
    event.system_timestamp = QDateTime::currentMSecsSinceEpoch();
    event.source_component = "video_timeline_sync";
    event.requires_ui_update = true;

    {
      QMutexLocker locker(&event_mutex_);
      if (event_queue_.size() < max_queue_size_) {
        event_queue_.enqueue(event);
      }
    }

    emit SyncPositionChanged(timestamp);
  }
}

QString VideoTimelineSync::AddTimelineMarker(const TimelineMarker& marker)
{
  TimelineMarker validated_marker = marker;
  ValidateMarkerData(validated_marker);

  if (validated_marker.marker_id.isEmpty()) {
    validated_marker.marker_id = GenerateMarkerId(validated_marker.type, validated_marker.timestamp);
  }

  {
    QMutexLocker locker(&marker_mutex_);
    
    // Check if we're at marker limit
    if (timeline_markers_.size() >= max_markers_) {
      qWarning() << "Maximum marker limit reached, not adding marker";
      return QString();
    }
    
    timeline_markers_[validated_marker.marker_id] = validated_marker;
  }

  // Create graphics item for marker
  if (IsMarkerVisible(validated_marker)) {
    TimelineMarkerItem* marker_item = new TimelineMarkerItem(validated_marker);
    timeline_scene_->addItem(marker_item);
    marker_graphics_[validated_marker.marker_id] = marker_item;
    
    // Animate marker if enabled
    if (marker_animations_enabled_ && validated_marker.animated) {
      AnimateMarker(validated_marker.marker_id);
    }
  }

  // Update statistics
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.markers_created++;
    sync_statistics_.active_markers = timeline_markers_.size();
  }

  emit MarkerAdded(validated_marker);
  emit TimelineDataChanged();

  qDebug() << "Timeline marker added:" << validated_marker.marker_id 
           << "type:" << static_cast<int>(validated_marker.type)
           << "timestamp:" << validated_marker.timestamp;

  return validated_marker.marker_id;
}

bool VideoTimelineSync::RemoveTimelineMarker(const QString& marker_id)
{
  {
    QMutexLocker locker(&marker_mutex_);
    if (!timeline_markers_.contains(marker_id)) {
      return false;
    }
    timeline_markers_.remove(marker_id);
  }

  // Remove graphics item
  if (marker_graphics_.contains(marker_id)) {
    QGraphicsItem* item = marker_graphics_.take(marker_id);
    timeline_scene_->removeItem(item);
    delete item;
  }

  // Remove from animated markers list
  animated_markers_.removeAll(marker_id);

  // Update statistics
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.active_markers = timeline_markers_.size();
  }

  emit MarkerRemoved(marker_id);
  emit TimelineDataChanged();

  qDebug() << "Timeline marker removed:" << marker_id;
  return true;
}

bool VideoTimelineSync::UpdateTimelineMarker(const QString& marker_id, const TimelineMarker& updated_marker)
{
  {
    QMutexLocker locker(&marker_mutex_);
    if (!timeline_markers_.contains(marker_id)) {
      return false;
    }
    
    TimelineMarker validated_marker = updated_marker;
    ValidateMarkerData(validated_marker);
    validated_marker.marker_id = marker_id; // Preserve original ID
    
    timeline_markers_[marker_id] = validated_marker;
  }

  // Update graphics item
  if (marker_graphics_.contains(marker_id)) {
    TimelineMarkerItem* marker_item = static_cast<TimelineMarkerItem*>(marker_graphics_[marker_id]);
    if (marker_item) {
      marker_item->setMarker(updated_marker);
    }
  }

  emit MarkerUpdated(updated_marker);
  emit TimelineDataChanged();

  qDebug() << "Timeline marker updated:" << marker_id;
  return true;
}

QList<TimelineMarker> VideoTimelineSync::GetMarkersInRange(qint64 start_timestamp, qint64 end_timestamp) const
{
  QMutexLocker locker(&marker_mutex_);
  
  QList<TimelineMarker> markers_in_range;
  
  for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
    const TimelineMarker& marker = it.value();
    if (marker.timestamp >= start_timestamp && marker.timestamp <= end_timestamp) {
      markers_in_range.append(marker);
    }
  }
  
  // Sort by timestamp
  std::sort(markers_in_range.begin(), markers_in_range.end(),
           [](const TimelineMarker& a, const TimelineMarker& b) {
             return a.timestamp < b.timestamp;
           });
  
  return markers_in_range;
}

TimelineMarker VideoTimelineSync::GetMarkerAt(qint64 timestamp, TimelineMarkerType type) const
{
  QMutexLocker locker(&marker_mutex_);
  
  TimelineMarker closest_marker;
  qint64 min_distance = LLONG_MAX;
  
  for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
    const TimelineMarker& marker = it.value();
    
    if (marker.type == type || type == TimelineMarkerType::Formation) { // Formation as wildcard
      qint64 distance = qAbs(marker.timestamp - timestamp);
      if (distance < min_distance) {
        min_distance = distance;
        closest_marker = marker;
      }
    }
  }
  
  // Only return if within reasonable range (1 second)
  if (min_distance <= 1000) {
    return closest_marker;
  }
  
  return TimelineMarker();
}

void VideoTimelineSync::ClearMarkers()
{
  {
    QMutexLocker locker(&marker_mutex_);
    timeline_markers_.clear();
  }
  
  // Clear graphics items
  for (QGraphicsItem* item : marker_graphics_) {
    timeline_scene_->removeItem(item);
    delete item;
  }
  marker_graphics_.clear();
  
  animated_markers_.clear();
  
  // Update statistics
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.active_markers = 0;
  }
  
  emit TimelineDataChanged();
  qInfo() << "All timeline markers cleared";
}

void VideoTimelineSync::SetMarkerTypeVisible(TimelineMarkerType type, bool visible)
{
  marker_visibility_[type] = visible;
  
  // Update existing marker visibility
  QMutexLocker locker(&marker_mutex_);
  for (auto it = timeline_markers_.begin(); it != timeline_markers_.end(); ++it) {
    const TimelineMarker& marker = it.value();
    if (marker.type == type) {
      if (marker_graphics_.contains(marker.marker_id)) {
        QGraphicsItem* item = marker_graphics_[marker.marker_id];
        item->setVisible(visible);
      }
    }
  }
  
  qDebug() << "Marker type visibility changed:" << static_cast<int>(type) << "visible:" << visible;
}

SyncStatistics VideoTimelineSync::GetSyncStatistics() const
{
  QMutexLocker locker(&stats_mutex_);
  return sync_statistics_;
}

void VideoTimelineSync::SetSyncPrecision(int precision_ms)
{
  sync_precision_ms_ = qBound(10, precision_ms, 1000); // 10ms to 1s range
  
  if (sync_timer_) {
    sync_timer_->setInterval(sync_precision_ms_);
  }
  
  qInfo() << "Sync precision set to:" << sync_precision_ms_ << "ms";
}

void VideoTimelineSync::SetMarkerAnimations(bool enabled)
{
  marker_animations_enabled_ = enabled;
  
  if (!enabled && marker_animation_timer_) {
    marker_animation_timer_->stop();
    
    // Stop all current animations
    if (marker_animation_group_) {
      marker_animation_group_->stop();
    }
  } else if (enabled && real_time_sync_active_ && marker_animation_timer_) {
    marker_animation_timer_->start();
  }
  
  qInfo() << "Marker animations" << (enabled ? "enabled" : "disabled");
}

QJsonObject VideoTimelineSync::ExportTimelineData() const
{
  QMutexLocker locker(&marker_mutex_);
  
  QJsonObject timeline_data;
  timeline_data["version"] = "1.0";
  timeline_data["export_timestamp"] = QDateTime::currentMSecsSinceEpoch();
  timeline_data["marker_count"] = timeline_markers_.size();
  
  QJsonArray markers_array;
  for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
    const TimelineMarker& marker = it.value();
    
    QJsonObject marker_obj;
    marker_obj["marker_id"] = marker.marker_id;
    marker_obj["type"] = static_cast<int>(marker.type);
    marker_obj["timestamp"] = marker.timestamp;
    marker_obj["label"] = marker.label;
    marker_obj["description"] = marker.description;
    marker_obj["color"] = marker.color.name();
    marker_obj["height_scale"] = marker.height_scale;
    marker_obj["animated"] = marker.animated;
    marker_obj["metadata"] = marker.metadata;
    marker_obj["user_created"] = marker.user_created;
    marker_obj["priority"] = marker.priority;
    
    markers_array.append(marker_obj);
  }
  
  timeline_data["markers"] = markers_array;
  timeline_data["statistics"] = QJsonObject::fromVariantMap(QVariantMap{
    {"markers_created", static_cast<qint64>(sync_statistics_.markers_created)},
    {"events_processed", static_cast<qint64>(sync_statistics_.events_processed)},
    {"sync_operations", static_cast<qint64>(sync_statistics_.sync_operations)}
  });
  
  return timeline_data;
}

bool VideoTimelineSync::ImportTimelineData(const QJsonObject& timeline_data)
{
  if (!timeline_data.contains("markers") || !timeline_data["markers"].isArray()) {
    qWarning() << "Invalid timeline data format";
    return false;
  }
  
  QJsonArray markers_array = timeline_data["markers"].toArray();
  int imported_count = 0;
  
  for (const QJsonValue& marker_value : markers_array) {
    if (!marker_value.isObject()) {
      continue;
    }
    
    QJsonObject marker_obj = marker_value.toObject();
    
    TimelineMarker marker;
    marker.marker_id = marker_obj["marker_id"].toString();
    marker.type = static_cast<TimelineMarkerType>(marker_obj["type"].toInt());
    marker.timestamp = marker_obj["timestamp"].toVariant().toLongLong();
    marker.label = marker_obj["label"].toString();
    marker.description = marker_obj["description"].toString();
    marker.color = QColor(marker_obj["color"].toString());
    marker.height_scale = marker_obj["height_scale"].toDouble();
    marker.animated = marker_obj["animated"].toBool();
    marker.metadata = marker_obj["metadata"].toObject();
    marker.user_created = marker_obj["user_created"].toBool();
    marker.priority = marker_obj["priority"].toInt();
    
    if (AddTimelineMarker(marker).isEmpty()) {
      qWarning() << "Failed to import marker:" << marker.marker_id;
    } else {
      imported_count++;
    }
  }
  
  qInfo() << "Imported" << imported_count << "timeline markers";
  return imported_count > 0;
}

void VideoTimelineSync::OnFormationDetected(const FormationData& formation)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Processing formation detection for timeline:" << formation.formation_id;
  CreateFormationMarker(formation);
  
  // Update statistics
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.events_processed++;
  }
}

void VideoTimelineSync::OnFormationUpdated(const FormationData& formation)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  // Find existing marker and update it
  QString marker_id = QString("formation_%1").arg(formation.formation_id);
  
  QMutexLocker locker(&marker_mutex_);
  if (timeline_markers_.contains(marker_id)) {
    TimelineMarker& marker = timeline_markers_[marker_id];
    
    // Update marker with new formation data
    marker.description = QString("Formation: %1 (Confidence: %2%)")
                        .arg(static_cast<int>(formation.type))
                        .arg(formation.confidence * 100, 0, 'f', 1);
    marker.metadata["confidence"] = formation.confidence;
    marker.metadata["mel_score"] = formation.mel_results.combined_score;
    
    // Update graphics if visible
    if (marker_graphics_.contains(marker_id)) {
      TimelineMarkerItem* marker_item = static_cast<TimelineMarkerItem*>(marker_graphics_[marker_id]);
      if (marker_item) {
        marker_item->setMarker(marker);
      }
    }
    
    emit MarkerUpdated(marker);
  }
}

void VideoTimelineSync::OnTriangleCallRecommended(TriangleCall call, const FormationData& formation)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Processing Triangle call for timeline:" << static_cast<int>(call);
  CreateTriangleCallMarker(call, formation);
}

void VideoTimelineSync::OnCoachingAlertReceived(const CoachingAlert& alert)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Processing coaching alert for timeline:" << alert.alert_id;
  CreateCoachingAlertMarker(alert);
}

void VideoTimelineSync::OnCriticalAlertReceived(const CoachingAlert& alert)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  // Create critical alert marker with special styling
  CreateCoachingAlertMarker(alert);
  
  // Animate critical alerts
  QString marker_id = QString("alert_%1").arg(alert.alert_id);
  if (marker_animations_enabled_) {
    AnimateMarker(marker_id);
  }
}

void VideoTimelineSync::OnMELResultsUpdated(const QString& formation_id, const MELResult& results)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Processing M.E.L. results for timeline:" << formation_id;
  CreateMELScoreMarker(formation_id, results);
}

void VideoTimelineSync::OnVideoPlaybackStarted()
{
  video_playing_ = true;
  qDebug() << "Video playback started - timeline sync active";
}

void VideoTimelineSync::OnVideoPlaybackStopped()
{
  video_playing_ = false;
  qDebug() << "Video playback stopped - timeline sync paused";
}

void VideoTimelineSync::OnVideoPositionChanged(qint64 position)
{
  UpdateVideoPosition(position);
}

void VideoTimelineSync::OnVideoSeekPerformed(qint64 position)
{
  current_video_position_ = position;
  
  // Create seek event marker if enabled
  if (debug_mode_enabled_) {
    TimelineMarker seek_marker;
    seek_marker.type = TimelineMarkerType::VideoEvent;
    seek_marker.timestamp = position;
    seek_marker.label = "Seek";
    seek_marker.description = QString("Video seek to %1").arg(position);
    seek_marker.color = QColor(100, 100, 100);
    seek_marker.height_scale = 0.3;
    seek_marker.user_created = false;
    seek_marker.priority = -1; // Low priority
    
    AddTimelineMarker(seek_marker);
  }
  
  qDebug() << "Video seek performed to:" << position;
}

void VideoTimelineSync::OnVideoRateChanged(double rate)
{
  video_playback_rate_ = rate;
  
  // Adjust sync timer interval based on playback rate
  if (sync_timer_) {
    int adjusted_interval = static_cast<int>(sync_precision_ms_ / qMax(0.1, rate));
    sync_timer_->setInterval(adjusted_interval);
  }
  
  qDebug() << "Video playback rate changed to:" << rate;
}

void VideoTimelineSync::SetupTimers()
{
  // Sync timer for real-time updates
  sync_timer_ = new QTimer(this);
  sync_timer_->setInterval(sync_precision_ms_);
  connect(sync_timer_, &QTimer::timeout, this, &VideoTimelineSync::OnSyncTimer);
  
  // Marker animation timer
  marker_animation_timer_ = new QTimer(this);
  marker_animation_timer_->setInterval(50); // 20 FPS animation
  connect(marker_animation_timer_, &QTimer::timeout, this, &VideoTimelineSync::OnMarkerAnimationTimer);
  
  // Statistics timer
  statistics_timer_ = new QTimer(this);
  statistics_timer_->setInterval(10000); // 10 seconds
  connect(statistics_timer_, &QTimer::timeout, this, &VideoTimelineSync::OnStatisticsTimer);
  
  // Cleanup timer
  cleanup_timer_ = new QTimer(this);
  cleanup_timer_->setInterval(cleanup_interval_ms_);
  connect(cleanup_timer_, &QTimer::timeout, this, &VideoTimelineSync::CleanupOldMarkers);
}

void VideoTimelineSync::OnSyncTimer()
{
  if (!real_time_sync_active_) {
    return;
  }
  
  // Process event queue
  ProcessEventQueue();
  
  // Update sync statistics
  UpdateSyncStatistics();
  
  // Calculate and emit latency
  CalculateSyncLatency();
}

void VideoTimelineSync::OnMarkerAnimationTimer()
{
  if (!marker_animations_enabled_ || animated_markers_.isEmpty()) {
    return;
  }
  
  // Update animated markers
  for (const QString& marker_id : animated_markers_) {
    if (marker_graphics_.contains(marker_id)) {
      TimelineMarkerItem* marker_item = static_cast<TimelineMarkerItem*>(marker_graphics_[marker_id]);
      if (marker_item) {
        // Update animation frame
        marker_item->startPulseAnimation();
      }
    }
  }
}

void VideoTimelineSync::OnStatisticsTimer()
{
  UpdateSyncStatistics();
  emit StatisticsUpdated(sync_statistics_);
}

void VideoTimelineSync::ProcessEventQueue()
{
  if (!event_processing_active_) {
    return;
  }
  
  QMutexLocker locker(&event_mutex_);
  
  int processed_count = 0;
  const int max_events_per_cycle = 50; // Process up to 50 events per cycle
  
  while (!event_queue_.isEmpty() && processed_count < max_events_per_cycle) {
    SyncEvent event = event_queue_.dequeue();
    locker.unlock();
    
    ProcessSyncEvent(event);
    processed_count++;
    
    locker.relock();
  }
  
  if (processed_count > 0) {
    QMutexLocker stats_locker(&stats_mutex_);
    sync_statistics_.events_processed += processed_count;
    sync_statistics_.queued_events = event_queue_.size();
  }
}

void VideoTimelineSync::CleanupOldMarkers()
{
  if (!auto_cleanup_enabled_) {
    return;
  }
  
  qint64 current_time = QDateTime::currentMSecsSinceEpoch();
  qint64 cutoff_time = current_time - marker_retention_time_ms_;
  
  QStringList markers_to_remove;
  
  {
    QMutexLocker locker(&marker_mutex_);
    
    for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
      const TimelineMarker& marker = it.value();
      
      // Only cleanup auto-generated markers, preserve user-created ones
      if (!marker.user_created && marker.timestamp < (current_video_position_ - marker_retention_time_ms_)) {
        markers_to_remove.append(marker.marker_id);
      }
    }
  }
  
  for (const QString& marker_id : markers_to_remove) {
    RemoveTimelineMarker(marker_id);
  }
  
  if (!markers_to_remove.isEmpty()) {
    qDebug() << "Cleaned up" << markers_to_remove.size() << "old markers";
  }
}

void VideoTimelineSync::ProcessSyncEvent(const SyncEvent& event)
{
  if (event.event_type == "position_update") {
    // Update timeline view position if needed
    if (event.requires_ui_update) {
      UpdateTimelineView();
    }
  }
  
  emit SyncEventProcessed(event);
}

void VideoTimelineSync::CreateFormationMarker(const FormationData& formation)
{
  TimelineMarker marker;
  marker.marker_id = QString("formation_%1").arg(formation.formation_id);
  marker.type = TimelineMarkerType::Formation;
  marker.timestamp = formation.video_timestamp;
  marker.label = QString("F%1").arg(static_cast<int>(formation.type));
  marker.description = QString("Formation: %1 (Confidence: %2%)")
                      .arg(static_cast<int>(formation.type))
                      .arg(formation.confidence * 100, 0, 'f', 1);
  marker.color = GetMarkerColor(TimelineMarkerType::Formation, formation.confidence);
  marker.height_scale = 0.8 + (formation.confidence * 0.2); // 0.8 to 1.0 based on confidence
  marker.animated = formation.confidence > 0.8; // Animate high-confidence formations
  marker.user_created = false;
  marker.priority = static_cast<int>(formation.confidence * 10);
  
  // Add metadata
  marker.metadata["formation_id"] = formation.formation_id;
  marker.metadata["formation_type"] = static_cast<int>(formation.type);
  marker.metadata["confidence"] = formation.confidence;
  marker.metadata["hash_position"] = formation.hash_position;
  marker.metadata["field_zone"] = formation.field_zone;
  marker.metadata["mel_score"] = formation.mel_results.combined_score;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::CreateTriangleCallMarker(TriangleCall call, const FormationData& formation)
{
  TimelineMarker marker;
  marker.marker_id = QString("triangle_%1_%2").arg(formation.formation_id).arg(static_cast<int>(call));
  marker.type = TimelineMarkerType::TriangleCall;
  marker.timestamp = formation.video_timestamp;
  marker.label = QString("T%1").arg(static_cast<int>(call));
  marker.description = QString("Triangle Call: %1").arg(static_cast<int>(call));
  marker.color = GetMarkerColor(TimelineMarkerType::TriangleCall);
  marker.height_scale = 0.9;
  marker.animated = true; // Triangle calls are always animated
  marker.user_created = false;
  marker.priority = 8; // High priority
  
  // Add metadata
  marker.metadata["formation_id"] = formation.formation_id;
  marker.metadata["triangle_call"] = static_cast<int>(call);
  marker.metadata["confidence"] = formation.confidence;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::CreateCoachingAlertMarker(const CoachingAlert& alert)
{
  TimelineMarker marker;
  marker.marker_id = QString("alert_%1").arg(alert.alert_id);
  marker.type = TimelineMarkerType::CoachingAlert;
  marker.timestamp = alert.video_timestamp;
  marker.label = QString("A%1").arg(alert.priority_level);
  marker.description = QString("Alert: %1 (Priority: %2)")
                      .arg(alert.alert_type)
                      .arg(alert.priority_level);
  marker.color = GetMarkerColor(TimelineMarkerType::CoachingAlert);
  marker.height_scale = 0.6 + (alert.priority_level * 0.1); // Scale with priority
  marker.animated = alert.priority_level >= 4; // Animate high-priority alerts
  marker.user_created = false;
  marker.priority = alert.priority_level + 5; // Offset for priority
  
  // Add metadata
  marker.metadata["alert_id"] = alert.alert_id;
  marker.metadata["alert_type"] = alert.alert_type;
  marker.metadata["priority_level"] = alert.priority_level;
  marker.metadata["target_staff"] = alert.target_staff;
  marker.metadata["message"] = alert.message;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::CreateMELScoreMarker(const QString& formation_id, const MELResult& results)
{
  TimelineMarker marker;
  marker.marker_id = QString("mel_%1").arg(formation_id);
  marker.type = TimelineMarkerType::MELScore;
  marker.timestamp = results.processing_timestamp;
  marker.label = QString("M%1").arg(static_cast<int>(results.combined_score));
  marker.description = QString("M.E.L. Score: %1 (M:%2 E:%3 L:%4)")
                      .arg(results.combined_score, 0, 'f', 1)
                      .arg(results.making_score, 0, 'f', 1)
                      .arg(results.efficiency_score, 0, 'f', 1)
                      .arg(results.logical_score, 0, 'f', 1);
  marker.color = GetMarkerColor(TimelineMarkerType::MELScore);
  marker.height_scale = 0.7;
  marker.animated = results.combined_score > 85.0; // Animate high scores
  marker.user_created = false;
  marker.priority = static_cast<int>(results.combined_score / 10);
  
  // Add metadata
  marker.metadata["formation_id"] = formation_id;
  marker.metadata["making_score"] = results.making_score;
  marker.metadata["efficiency_score"] = results.efficiency_score;
  marker.metadata["logical_score"] = results.logical_score;
  marker.metadata["combined_score"] = results.combined_score;
  marker.metadata["stage_status"] = results.stage_status;
  
  AddTimelineMarker(marker);
}

QColor VideoTimelineSync::GetMarkerColor(TimelineMarkerType type, double confidence) const
{
  QColor base_color = marker_colors_.value(type, QColor(150, 150, 150));
  
  if (confidence < 1.0) {
    // Adjust alpha based on confidence
    int alpha = static_cast<int>(confidence * 255);
    base_color.setAlpha(alpha);
  }
  
  return base_color;
}

double VideoTimelineSync::GetMarkerHeight(TimelineMarkerType type, int priority) const
{
  double base_height = 0.8;
  
  switch (type) {
    case TimelineMarkerType::Formation:
      base_height = 0.8;
      break;
    case TimelineMarkerType::TriangleCall:
      base_height = 0.9;
      break;
    case TimelineMarkerType::CoachingAlert:
      base_height = 0.7;
      break;
    case TimelineMarkerType::ManualAnnotation:
      base_height = 1.0;
      break;
    case TimelineMarkerType::MELScore:
      base_height = 0.6;
      break;
    case TimelineMarkerType::VideoEvent:
      base_height = 0.3;
      break;
    case TimelineMarkerType::Highlight:
      base_height = 1.0;
      break;
  }
  
  // Adjust based on priority
  base_height += (priority * 0.02); // Small adjustment per priority level
  
  return qBound(0.1, base_height, 1.0);
}

QString VideoTimelineSync::GenerateMarkerId(TimelineMarkerType type, qint64 timestamp) const
{
  QString type_prefix;
  switch (type) {
    case TimelineMarkerType::Formation:
      type_prefix = "formation";
      break;
    case TimelineMarkerType::TriangleCall:
      type_prefix = "triangle";
      break;
    case TimelineMarkerType::CoachingAlert:
      type_prefix = "alert";
      break;
    case TimelineMarkerType::ManualAnnotation:
      type_prefix = "manual";
      break;
    case TimelineMarkerType::MELScore:
      type_prefix = "mel";
      break;
    case TimelineMarkerType::VideoEvent:
      type_prefix = "video";
      break;
    case TimelineMarkerType::Highlight:
      type_prefix = "highlight";
      break;
    default:
      type_prefix = "marker";
      break;
  }
  
  return QString("%1_%2_%3")
         .arg(type_prefix)
         .arg(timestamp)
         .arg(QRandomGenerator::global()->generate() % 10000);
}

void VideoTimelineSync::UpdateSyncStatistics()
{
  QMutexLocker locker(&stats_mutex_);
  
  sync_statistics_.last_sync_time = QDateTime::currentDateTime();
  sync_statistics_.active_markers = timeline_markers_.size();
  
  {
    QMutexLocker event_locker(&event_mutex_);
    sync_statistics_.queued_events = event_queue_.size();
  }
  
  sync_statistics_.sync_operations++;
  
  // Calculate timeline accuracy score based on sync performance
  if (sync_statistics_.average_latency_ms > 100.0) {
    sync_statistics_.timeline_accuracy_score = qMax(50.0, 100.0 - sync_statistics_.average_latency_ms / 10.0);
  } else {
    sync_statistics_.timeline_accuracy_score = qMin(100.0, 100.0 - sync_statistics_.average_latency_ms / 100.0);
  }
}

void VideoTimelineSync::ValidateMarkerData(TimelineMarker& marker) const
{
  // Ensure valid timestamp
  if (marker.timestamp < 0) {
    marker.timestamp = 0;
  }
  
  // Ensure valid height scale
  marker.height_scale = qBound(0.1, marker.height_scale, 1.0);
  
  // Ensure valid priority
  marker.priority = qBound(-10, marker.priority, 10);
  
  // Ensure valid color
  if (!marker.color.isValid()) {
    marker.color = GetMarkerColor(marker.type);
  }
  
  // Set default label if empty
  if (marker.label.isEmpty()) {
    marker.label = QString("M%1").arg(static_cast<int>(marker.type));
  }
}

bool VideoTimelineSync::IsMarkerVisible(const TimelineMarker& marker) const
{
  return marker_visibility_.value(marker.type, true);
}

void VideoTimelineSync::AnimateMarker(const QString& marker_id)
{
  if (!marker_animations_enabled_ || !marker_graphics_.contains(marker_id)) {
    return;
  }
  
  TimelineMarkerItem* marker_item = static_cast<TimelineMarkerItem*>(marker_graphics_[marker_id]);
  if (marker_item) {
    marker_item->setAnimated(true);
    marker_item->startPulseAnimation();
    
    if (!animated_markers_.contains(marker_id)) {
      animated_markers_.append(marker_id);
    }
  }
}

void VideoTimelineSync::UpdateTimelineView()
{
  // Update timeline view based on current video position
  // This would integrate with the actual timeline panel rendering
  if (timeline_view_ && timeline_scene_) {
    // Calculate scroll position based on current video position
    // Implementation would depend on timeline panel specifics
  }
}

void VideoTimelineSync::CalculateSyncLatency()
{
  if (sync_latency_timer_.isValid()) {
    double latency = sync_latency_timer_.elapsed();
    
    {
      QMutexLocker locker(&stats_mutex_);
      
      // Add to latency history
      latency_history_.enqueue(latency);
      if (latency_history_.size() > latency_history_size_) {
        latency_history_.dequeue();
      }
      
      // Calculate average latency
      double total_latency = 0.0;
      for (double l : latency_history_) {
        total_latency += l;
      }
      sync_statistics_.average_latency_ms = total_latency / latency_history_.size();
    }
    
    emit SyncLatencyChanged(sync_statistics_.average_latency_ms);
    sync_latency_timer_.restart();
  }
}

// TimelineMarkerItem Implementation
TimelineMarkerItem::TimelineMarkerItem(const TimelineMarker& marker, QGraphicsItem* parent)
  : QGraphicsItem(parent)
  , marker_(marker)
  , is_animated_(false)
  , is_hovered_(false)
  , pulse_animation_(nullptr)
  , icon_item_(nullptr)
  , label_item_(nullptr)
{
  setAcceptHoverEvents(true);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  updateVisuals();
}

QRectF TimelineMarkerItem::boundingRect() const
{
  return QRectF(-10, -20, 20, 40);
}

void TimelineMarkerItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)
  
  painter->setRenderHint(QPainter::Antialiasing);
  
  // Draw marker shape based on type
  QColor color = marker_.color;
  if (is_hovered_) {
    color = color.lighter(120);
  }
  
  painter->setBrush(color);
  painter->setPen(QPen(color.darker(150), 1));
  
  QRectF rect = boundingRect();
  rect.setHeight(rect.height() * marker_.height_scale);
  
  switch (marker_.type) {
    case TimelineMarkerType::Formation:
      painter->drawEllipse(rect);
      break;
    case TimelineMarkerType::TriangleCall:
      painter->drawPolygon(QPolygonF() << QPointF(0, rect.top()) 
                                      << QPointF(rect.right(), rect.bottom())
                                      << QPointF(rect.left(), rect.bottom()));
      break;
    case TimelineMarkerType::CoachingAlert:
      painter->drawRect(rect);
      break;
    default:
      painter->drawEllipse(rect);
      break;
  }
  
  // Draw label if visible
  if (!marker_.label.isEmpty() && rect.width() > 15) {
    painter->setPen(QPen(Qt::white));
    painter->drawText(rect, Qt::AlignCenter, marker_.label);
  }
}

void TimelineMarkerItem::setMarker(const TimelineMarker& marker)
{
  marker_ = marker;
  updateVisuals();
  update();
}

void TimelineMarkerItem::setAnimated(bool animated)
{
  is_animated_ = animated;
  
  if (animated && !pulse_animation_) {
    pulse_animation_ = new QPropertyAnimation(this, "opacity");
    pulse_animation_->setDuration(1000);
    pulse_animation_->setLoopCount(-1);
    pulse_animation_->setKeyValueAt(0.0, 1.0);
    pulse_animation_->setKeyValueAt(0.5, 0.5);
    pulse_animation_->setKeyValueAt(1.0, 1.0);
  }
}

void TimelineMarkerItem::startPulseAnimation()
{
  if (pulse_animation_ && is_animated_) {
    pulse_animation_->start();
  }
}

void TimelineMarkerItem::stopPulseAnimation()
{
  if (pulse_animation_) {
    pulse_animation_->stop();
    setOpacity(1.0);
  }
}

void TimelineMarkerItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  Q_UNUSED(event)
  // Handle marker click
}

void TimelineMarkerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  Q_UNUSED(event)
  // Emit marker clicked signal through parent VideoTimelineSync
}

void TimelineMarkerItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  Q_UNUSED(event)
  is_hovered_ = true;
  update();
  
  // Show tooltip
  QString tooltip = VideoTimelineSyncUtils::CreateMarkerTooltip(marker_);
  setToolTip(tooltip);
}

void TimelineMarkerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  Q_UNUSED(event)
  is_hovered_ = false;
  update();
}

void TimelineMarkerItem::updateVisuals()
{
  // Update visual properties based on marker data
  setVisible(true);
  setZValue(marker_.priority);
}

// VideoTimelineSyncUtils namespace implementation
namespace VideoTimelineSyncUtils {

int CalculateOptimalSyncPrecision(double frame_rate, double playback_rate)
{
  // Calculate optimal sync precision based on frame rate
  double frame_duration_ms = 1000.0 / frame_rate;
  double adjusted_frame_duration = frame_duration_ms / playback_rate;
  
  // Use half frame duration as sync precision, with reasonable bounds
  int precision = static_cast<int>(adjusted_frame_duration / 2.0);
  return qBound(10, precision, 200); // 10ms to 200ms range
}

double TimestampToTimelinePosition(qint64 timestamp, qint64 duration, double timeline_width)
{
  if (duration <= 0) {
    return 0.0;
  }
  
  double ratio = static_cast<double>(timestamp) / duration;
  return ratio * timeline_width;
}

qint64 TimelinePositionToTimestamp(double position, qint64 duration, double timeline_width)
{
  if (timeline_width <= 0) {
    return 0;
  }
  
  double ratio = position / timeline_width;
  return static_cast<qint64>(ratio * duration);
}

QString CreateMarkerTooltip(const TimelineMarker& marker)
{
  QString tooltip = QString("<b>%1</b><br>").arg(marker.label);
  tooltip += QString("Type: %1<br>").arg(static_cast<int>(marker.type));
  tooltip += QString("Time: %1ms<br>").arg(marker.timestamp);
  
  if (!marker.description.isEmpty()) {
    tooltip += QString("Description: %1<br>").arg(marker.description);
  }
  
  if (marker.metadata.contains("confidence")) {
    tooltip += QString("Confidence: %1%<br>")
              .arg(marker.metadata["confidence"].toDouble() * 100, 0, 'f', 1);
  }
  
  return tooltip;
}

bool IsValidMarkerTimestamp(qint64 timestamp, qint64 video_duration)
{
  return timestamp >= 0 && timestamp <= video_duration;
}

double CalculateRenderingPerformance(int marker_count, qint64 timeline_duration)
{
  // Simple performance score based on marker density
  double markers_per_second = static_cast<double>(marker_count) / (timeline_duration / 1000.0);
  
  if (markers_per_second < 1.0) {
    return 100.0; // Excellent performance
  } else if (markers_per_second < 5.0) {
    return 90.0; // Good performance
  } else if (markers_per_second < 10.0) {
    return 70.0; // Fair performance
  } else {
    return 50.0; // Poor performance
  }
}

} // namespace VideoTimelineSyncUtils

} // namespace olive
