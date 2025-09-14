/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Video Timeline Synchronization Service Implementation
  Real-time coordination between video playback and Triangle Defense data
***/

#include "video_timeline_sync.h"

#include <QDebug>
#include <QPainter>
#include <QGraphicsProxyWidget>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QToolTip>
#include <QApplication>
#include <QScreen>
#include <QtMath>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRandomGenerator>

#include "panel/timeline/timeline.h"
#include "panel/sequenceviewer/sequenceviewer.h"

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
  qInfo() << "Initializing Video Timeline Synchronization";
  
  // Initialize marker visibility settings
  marker_visibility_[TimelineMarkerType::Formation] = true;
  marker_visibility_[TimelineMarkerType::TriangleCall] = true;
  marker_visibility_[TimelineMarkerType::CoachingAlert] = true;
  marker_visibility_[TimelineMarkerType::ManualAnnotation] = true;
  marker_visibility_[TimelineMarkerType::MELScore] = true;
  marker_visibility_[TimelineMarkerType::VideoEvent] = false;
  marker_visibility_[TimelineMarkerType::Highlight] = true;
  
  // Initialize marker colors
  marker_colors_[TimelineMarkerType::Formation] = QColor(0, 120, 215); // Blue
  marker_colors_[TimelineMarkerType::TriangleCall] = QColor(255, 69, 0); // Red-Orange
  marker_colors_[TimelineMarkerType::CoachingAlert] = QColor(255, 215, 0); // Gold
  marker_colors_[TimelineMarkerType::ManualAnnotation] = QColor(128, 0, 128); // Purple
  marker_colors_[TimelineMarkerType::MELScore] = QColor(0, 128, 0); // Green
  marker_colors_[TimelineMarkerType::VideoEvent] = QColor(128, 128, 128); // Gray
  marker_colors_[TimelineMarkerType::Highlight] = QColor(255, 20, 147); // Deep Pink
  
  // Setup graphics components
  timeline_scene_ = new QGraphicsScene(this);
  timeline_view_ = new QGraphicsView(timeline_scene_);
  timeline_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  timeline_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  timeline_view_->setRenderHint(QPainter::Antialiasing);
  timeline_view_->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  
  // Initialize animation groups
  marker_animation_group_ = new QParallelAnimationGroup(this);
  scroll_animation_ = new QPropertyAnimation(timeline_view_->horizontalScrollBar(), "value", this);
  scroll_animation_->setDuration(300);
  scroll_animation_->setEasingCurve(QEasingCurve::OutCubic);
  
  SetupTimers();
  
  qInfo() << "Video Timeline Synchronization initialized";
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
    // Connect to Triangle Defense sync signals
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

    qInfo() << "Connected to Triangle Defense Sync";
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

  // Stop animations
  if (marker_animation_group_ && marker_animation_group_->state() == QAbstractAnimation::Running) {
    marker_animation_group_->stop();
  }
  if (scroll_animation_ && scroll_animation_->state() == QAbstractAnimation::Running) {
    scroll_animation_->stop();
  }

  // Clear markers and graphics
  ClearMarkers();
  if (timeline_scene_) {
    timeline_scene_->clear();
  }

  // Clear event queue
  {
    QMutexLocker locker(&event_mutex_);
    while (!event_queue_.isEmpty()) {
      event_queue_.dequeue();
    }
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
    // Connect timeline panel signals for video events
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

  // Start timers
  if (sync_timer_) {
    sync_timer_->start();
  }
  if (marker_animation_timer_ && marker_animations_enabled_) {
    marker_animation_timer_->start();
  }
  if (statistics_timer_) {
    statistics_timer_->start();
  }
  if (cleanup_timer_ && auto_cleanup_enabled_) {
    cleanup_timer_->start();
  }

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

  emit SyncStopped();
  qInfo() << "Real-time timeline synchronization stopped";
}

void VideoTimelineSync::UpdateVideoPosition(qint64 timestamp)
{
  current_video_position_ = timestamp;
  
  if (real_time_sync_active_) {
    // Update timeline view position
    UpdateTimelineView();
    
    // Emit position change signal
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
    
    // Check if we've reached the maximum marker limit
    if (timeline_markers_.size() >= max_markers_) {
      qWarning() << "Maximum marker limit reached, removing oldest markers";
      CleanupOldMarkers();
    }
    
    timeline_markers_[validated_marker.marker_id] = validated_marker;
  }

  // Create graphics item for the marker
  if (IsMarkerVisible(validated_marker)) {
    TimelineMarkerItem* marker_item = new TimelineMarkerItem(validated_marker);
    marker_graphics_[validated_marker.marker_id] = marker_item;
    timeline_scene_->addItem(marker_item);
    
    // Position the marker on the timeline
    double timeline_width = timeline_scene_->width();
    double position = VideoTimelineSyncUtils::TimestampToTimelinePosition(
      validated_marker.timestamp, 
      current_video_position_ + 300000, // Assume 5 minutes visible
      timeline_width
    );
    marker_item->setPos(position, 50 - (validated_marker.height_scale * 40));
    
    // Start animation if enabled
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
  bool removed = false;
  
  {
    QMutexLocker locker(&marker_mutex_);
    removed = timeline_markers_.remove(marker_id) > 0;
  }

  if (removed) {
    // Remove graphics item
    if (marker_graphics_.contains(marker_id)) {
      QGraphicsItem* item = marker_graphics_[marker_id];
      timeline_scene_->removeItem(item);
      delete item;
      marker_graphics_.remove(marker_id);
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
  }

  return removed;
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
      
      // Reposition if timestamp changed
      double timeline_width = timeline_scene_->width();
      double position = VideoTimelineSyncUtils::TimestampToTimelinePosition(
        updated_marker.timestamp,
        current_video_position_ + 300000,
        timeline_width
      );
      marker_item->setPos(position, 50 - (updated_marker.height_scale * 40));
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
      if (IsMarkerVisible(marker)) {
        markers_in_range.append(marker);
      }
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
      if (distance < min_distance && distance < 1000) { // Within 1 second
        min_distance = distance;
        closest_marker = marker;
      }
    }
  }
  
  return closest_marker;
}

void VideoTimelineSync::ClearMarkers()
{
  {
    QMutexLocker locker(&marker_mutex_);
    timeline_markers_.clear();
  }

  // Clear graphics items
  for (auto it = marker_graphics_.begin(); it != marker_graphics_.end(); ++it) {
    timeline_scene_->removeItem(it.value());
    delete it.value();
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
  
  // Update visibility of existing markers
  {
    QMutexLocker locker(&marker_mutex_);
    for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
      const TimelineMarker& marker = it.value();
      if (marker.type == type) {
        if (marker_graphics_.contains(marker.marker_id)) {
          marker_graphics_[marker.marker_id]->setVisible(visible);
        }
      }
    }
  }
  
  qInfo() << "Marker type visibility changed:" << static_cast<int>(type) << "visible:" << visible;
}

SyncStatistics VideoTimelineSync::GetSyncStatistics() const
{
  QMutexLocker locker(&stats_mutex_);
  return sync_statistics_;
}

void VideoTimelineSync::SetSyncPrecision(int precision_ms)
{
  sync_precision_ms_ = qBound(50, precision_ms, 1000); // Clamp between 50ms and 1000ms
  
  if (sync_timer_) {
    sync_timer_->setInterval(sync_precision_ms_);
  }
  
  qInfo() << "Sync precision set to:" << sync_precision_ms_ << "ms";
}

void VideoTimelineSync::SetMarkerAnimations(bool enabled)
{
  marker_animations_enabled_ = enabled;
  
  if (enabled && real_time_sync_active_ && marker_animation_timer_) {
    marker_animation_timer_->start();
  } else if (marker_animation_timer_) {
    marker_animation_timer_->stop();
  }
  
  qInfo() << "Marker animations" << (enabled ? "enabled" : "disabled");
}

QJsonObject VideoTimelineSync::ExportTimelineData() const
{
  QMutexLocker locker(&marker_mutex_);
  
  QJsonObject timeline_data;
  timeline_data["version"] = "1.0";
  timeline_data["timestamp"] = QDateTime::currentMSecsSinceEpoch();
  timeline_data["video_duration"] = current_video_position_;
  
  QJsonArray markers_array;
  for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
    const TimelineMarker& marker = it.value();
    
    QJsonObject marker_obj;
    marker_obj["id"] = marker.marker_id;
    marker_obj["type"] = static_cast<int>(marker.type);
    marker_obj["timestamp"] = marker.timestamp;
    marker_obj["label"] = marker.label;
    marker_obj["description"] = marker.description;
    marker_obj["color"] = marker.color.name();
    marker_obj["height_scale"] = marker.height_scale;
    marker_obj["animated"] = marker.animated;
    marker_obj["user_created"] = marker.user_created;
    marker_obj["priority"] = marker.priority;
    marker_obj["metadata"] = marker.metadata;
    
    markers_array.append(marker_obj);
  }
  
  timeline_data["markers"] = markers_array;
  
  // Add statistics
  timeline_data["statistics"] = QJsonObject{
    {"total_markers", timeline_markers_.size()},
    {"events_processed", sync_statistics_.events_processed},
    {"sync_operations", sync_statistics_.sync_operations},
    {"average_latency_ms", sync_statistics_.average_latency_ms}
  };
  
  qInfo() << "Timeline data exported with" << timeline_markers_.size() << "markers";
  return timeline_data;
}

bool VideoTimelineSync::ImportTimelineData(const QJsonObject& timeline_data)
{
  if (!timeline_data.contains("markers") || !timeline_data["markers"].isArray()) {
    qWarning() << "Invalid timeline data format";
    return false;
  }
  
  // Clear existing markers
  ClearMarkers();
  
  QJsonArray markers_array = timeline_data["markers"].toArray();
  int imported_count = 0;
  
  for (const QJsonValue& marker_value : markers_array) {
    QJsonObject marker_obj = marker_value.toObject();
    
    TimelineMarker marker;
    marker.marker_id = marker_obj["id"].toString();
    marker.type = static_cast<TimelineMarkerType>(marker_obj["type"].toInt());
    marker.timestamp = marker_obj["timestamp"].toVariant().toLongLong();
    marker.label = marker_obj["label"].toString();
    marker.description = marker_obj["description"].toString();
    marker.color = QColor(marker_obj["color"].toString());
    marker.height_scale = marker_obj["height_scale"].toDouble();
    marker.animated = marker_obj["animated"].toBool();
    marker.user_created = marker_obj["user_created"].toBool();
    marker.priority = marker_obj["priority"].toInt();
    marker.metadata = marker_obj["metadata"].toObject();
    
    if (VideoTimelineSyncUtils::IsValidMarkerTimestamp(marker.timestamp, current_video_position_)) {
      AddTimelineMarker(marker);
      imported_count++;
    }
  }
  
  qInfo() << "Timeline data imported successfully:" << imported_count << "markers";
  return true;
}

void VideoTimelineSync::OnFormationDetected(const FormationData& formation)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Creating timeline marker for formation:" << formation.formation_id;
  CreateFormationMarker(formation);
  
  // Update sync statistics
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.events_processed++;
    sync_statistics_.last_sync_time = QDateTime::currentDateTime();
  }
}

void VideoTimelineSync::OnFormationUpdated(const FormationData& formation)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  // Update existing formation marker
  QString marker_id = QString("formation_%1").arg(formation.formation_id);
  
  QMutexLocker locker(&marker_mutex_);
  if (timeline_markers_.contains(marker_id)) {
    TimelineMarker updated_marker = timeline_markers_[marker_id];
    updated_marker.metadata["confidence"] = formation.confidence;
    updated_marker.metadata["mel_combined_score"] = formation.mel_results.combined_score;
    updated_marker.description = QString("Formation %1 (Updated - Confidence: %2%)")
                                 .arg(formation.formation_id)
                                 .arg(formation.confidence * 100, 0, 'f', 1);
    
    UpdateTimelineMarker(marker_id, updated_marker);
  }
}

void VideoTimelineSync::OnTriangleCallRecommended(TriangleCall call, const FormationData& formation)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Creating timeline marker for Triangle call:" << static_cast<int>(call);
  CreateTriangleCallMarker(call, formation);
}

void VideoTimelineSync::OnCoachingAlertReceived(const CoachingAlert& alert)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Creating timeline marker for coaching alert:" << alert.alert_id;
  CreateCoachingAlertMarker(alert);
}

void VideoTimelineSync::OnCriticalAlertReceived(const CoachingAlert& alert)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  // Create a more prominent marker for critical alerts
  TimelineMarker marker;
  marker.type = TimelineMarkerType::CoachingAlert;
  marker.timestamp = alert.video_timestamp;
  marker.label = "CRITICAL";
  marker.description = QString("CRITICAL ALERT: %1").arg(alert.message);
  marker.color = QColor(255, 0, 0); // Bright red
  marker.height_scale = 1.0; // Full height
  marker.animated = true;
  marker.priority = 10; // Highest priority
  marker.metadata["alert_id"] = alert.alert_id;
  marker.metadata["alert_type"] = alert.alert_type;
  marker.metadata["priority_level"] = alert.priority_level;
  marker.metadata["target_staff"] = alert.target_staff;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::OnMELResultsUpdated(const QString& formation_id, const MELResult& results)
{
  if (!real_time_sync_active_) {
    return;
  }
  
  qDebug() << "Creating timeline marker for M.E.L. results:" << formation_id;
  CreateMELScoreMarker(formation_id, results);
}

void VideoTimelineSync::OnVideoPlaybackStarted()
{
  video_playing_ = true;
  
  if (real_time_sync_active_) {
    sync_latency_timer_.start();
  }
  
  qDebug() << "Video playback started - timeline sync active";
}

void VideoTimelineSync::OnVideoPlaybackStopped()
{
  video_playing_ = false;
  qDebug() << "Video playback stopped";
}

void VideoTimelineSync::OnVideoPositionChanged(qint64 position)
{
  UpdateVideoPosition(position);
}

void VideoTimelineSync::OnVideoSeekPerformed(qint64 position)
{
  UpdateVideoPosition(position);
  
  // Add seek event marker if enabled
  if (marker_visibility_[TimelineMarkerType::VideoEvent]) {
    TimelineMarker marker;
    marker.type = TimelineMarkerType::VideoEvent;
    marker.timestamp = position;
    marker.label = "SEEK";
    marker.description = QString("Video seek to %1")
                        .arg(VideoTimelineSyncUtils::VideoTimestampToGameClock(position, 0));
    marker.color = marker_colors_[TimelineMarkerType::VideoEvent];
    marker.height_scale = 0.3;
    marker.animated = false;
    marker.user_created = false;
    marker.priority = 1;
    
    AddTimelineMarker(marker);
  }
}

void VideoTimelineSync::OnVideoRateChanged(double rate)
{
  video_playback_rate_ = rate;
  
  // Adjust sync precision based on playback rate
  if (video_playback_rate_ > 1.0) {
    SetSyncPrecision(static_cast<int>(sync_precision_ms_ / video_playback_rate_));
  } else {
    SetSyncPrecision(100); // Default precision
  }
  
  qDebug() << "Video playback rate changed to:" << rate << "sync precision adjusted";
}

void VideoTimelineSync::SetupTimers()
{
  // Main synchronization timer
  sync_timer_ = new QTimer(this);
  sync_timer_->setInterval(sync_precision_ms_);
  connect(sync_timer_, &QTimer::timeout, this, &VideoTimelineSync::OnSyncTimer);
  
  // Marker animation timer
  marker_animation_timer_ = new QTimer(this);
  marker_animation_timer_->setInterval(50); // 20 FPS for smooth animations
  connect(marker_animation_timer_, &QTimer::timeout, this, &VideoTimelineSync::OnMarkerAnimationTimer);
  
  // Statistics update timer
  statistics_timer_ = new QTimer(this);
  statistics_timer_->setInterval(5000); // 5 seconds
  connect(statistics_timer_, &QTimer::timeout, this, &VideoTimelineSync::UpdateSyncStatistics);
  
  // Cleanup timer
  cleanup_timer_ = new QTimer(this);
  cleanup_timer_->setInterval(cleanup_interval_ms_);
  connect(cleanup_timer_, &QTimer::timeout, this, &VideoTimelineSync::CleanupOldMarkers);
}

void VideoTimelineSync::CreateFormationMarker(const FormationData& formation)
{
  TimelineMarker marker;
  marker.type = TimelineMarkerType::Formation;
  marker.timestamp = formation.video_timestamp;
  marker.label = QString("F%1").arg(static_cast<int>(formation.type));
  marker.description = QString("Formation %1 - %2 (Confidence: %3%)")
                      .arg(formation.formation_id)
                      .arg(static_cast<int>(formation.type))
                      .arg(formation.confidence * 100, 0, 'f', 1);
  marker.color = marker_colors_[TimelineMarkerType::Formation];
  marker.height_scale = 0.8;
  marker.animated = formation.confidence > 0.8; // Animate high-confidence formations
  marker.priority = static_cast<int>(formation.confidence * 10);
  marker.metadata["formation_id"] = formation.formation_id;
  marker.metadata["formation_type"] = static_cast<int>(formation.type);
  marker.metadata["confidence"] = formation.confidence;
  marker.metadata["hash_position"] = formation.hash_position;
  marker.metadata["field_zone"] = formation.field_zone;
  marker.metadata["mel_making_score"] = formation.mel_results.making_score;
  marker.metadata["mel_efficiency_score"] = formation.mel_results.efficiency_score;
  marker.metadata["mel_logical_score"] = formation.mel_results.logical_score;
  marker.metadata["mel_combined_score"] = formation.mel_results.combined_score;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::CreateTriangleCallMarker(TriangleCall call, const FormationData& formation)
{
  TimelineMarker marker;
  marker.type = TimelineMarkerType::TriangleCall;
  marker.timestamp = formation.video_timestamp;
  marker.label = QString("T%1").arg(static_cast<int>(call));
  marker.description = QString("Triangle Defense Call: %1 for Formation %2")
                      .arg(static_cast<int>(call))
                      .arg(formation.formation_id);
  marker.color = marker_colors_[TimelineMarkerType::TriangleCall];
  marker.height_scale = 1.0; // Full height for important calls
  marker.animated = true;
  marker.priority = 8; // High priority
  marker.metadata["triangle_call"] = static_cast<int>(call);
  marker.metadata["formation_id"] = formation.formation_id;
  marker.metadata["formation_type"] = static_cast<int>(formation.type);
  marker.metadata["confidence"] = formation.confidence;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::CreateCoachingAlertMarker(const CoachingAlert& alert)
{
  TimelineMarker marker;
  marker.type = TimelineMarkerType::CoachingAlert;
  marker.timestamp = alert.video_timestamp;
  marker.label = QString("A%1").arg(alert.priority_level);
  marker.description = QString("Alert: %1 (Priority: %2)")
                      .arg(alert.message)
                      .arg(alert.priority_level);
  
  // Color intensity based on priority
  QColor base_color = marker_colors_[TimelineMarkerType::CoachingAlert];
  int alpha = 150 + (alert.priority_level * 20); // More opaque for higher priority
  marker.color = QColor(base_color.red(), base_color.green(), base_color.blue(), qMin(255, alpha));
  
  marker.height_scale = 0.2 + (alert.priority_level * 0.15); // Taller for higher priority
  marker.animated = alert.priority_level >= 3;
  marker.priority = alert.priority_level;
  marker.metadata["alert_id"] = alert.alert_id;
  marker.metadata["alert_type"] = alert.alert_type;
  marker.metadata["priority_level"] = alert.priority_level;
  marker.metadata["target_staff"] = alert.target_staff;
  marker.metadata["acknowledged"] = alert.acknowledged;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::CreateMELScoreMarker(const QString& formation_id, const MELResult& results)
{
  TimelineMarker marker;
  marker.type = TimelineMarkerType::MELScore;
  marker.timestamp = results.processing_timestamp;
  marker.label = QString("M%1").arg(static_cast<int>(results.combined_score));
  marker.description = QString("M.E.L. Score: %1 (Making: %2, Efficiency: %3, Logical: %4)")
                      .arg(results.combined_score, 0, 'f', 1)
                      .arg(results.making_score, 0, 'f', 1)
                      .arg(results.efficiency_score, 0, 'f', 1)
                      .arg(results.logical_score, 0, 'f', 1);
  
  // Color based on score quality
  if (results.combined_score >= 80.0) {
    marker.color = QColor(0, 255, 0); // Green for high scores
  } else if (results.combined_score >= 60.0) {
    marker.color = QColor(255, 255, 0); // Yellow for medium scores
  } else {
    marker.color = QColor(255, 165, 0); // Orange for low scores
  }
  
  marker.height_scale = results.combined_score / 100.0; // Height represents score
  marker.animated = results.combined_score >= 85.0; // Animate excellent scores
  marker.priority = static_cast<int>(results.combined_score / 10);
  marker.metadata["formation_id"] = formation_id;
  marker.metadata["making_score"] = results.making_score;
  marker.metadata["efficiency_score"] = results.efficiency_score;
  marker.metadata["logical_score"] = results.logical_score;
  marker.metadata["combined_score"] = results.combined_score;
  marker.metadata["stage_status"] = results.stage_status;
  
  AddTimelineMarker(marker);
}

void VideoTimelineSync::UpdateTimelineView()
{
  if (!timeline_view_ || !real_time_sync_active_) {
    return;
  }
  
  // Calculate the position to center the current video position
  double timeline_width = timeline_scene_->width();
  double current_position = VideoTimelineSyncUtils::TimestampToTimelinePosition(
    current_video_position_,
    current_video_position_ + 300000, // 5-minute window
    timeline_width
  );
  
  // Smoothly scroll to current position
  double target_scroll = current_position - (timeline_view_->width() / 2);
  target_scroll = qBound(0.0, target_scroll, timeline_width - timeline_view_->width());
  
  if (scroll_animation_->state() != QAbstractAnimation::Running) {
    scroll_animation_->setStartValue(timeline_view_->horizontalScrollBar()->value());
    scroll_animation_->setEndValue(static_cast<int>(target_scroll));
    scroll_animation_->start();
  }
}

void VideoTimelineSync::AnimateMarker(const QString& marker_id)
{
  if (!marker_animations_enabled_ || !marker_graphics_.contains(marker_id)) {
    return;
  }
  
  TimelineMarkerItem* marker_item = static_cast<TimelineMarkerItem*>(marker_graphics_[marker_id]);
  if (marker_item) {
    marker_item->startPulseAnimation();
    
    if (!animated_markers_.contains(marker_id)) {
      animated_markers_.append(marker_id);
    }
  }
}

void VideoTimelineSync::OnSyncTimer()
{
  if (!real_time_sync_active_) {
    return;
  }
  
  // Process queued events
  ProcessEventQueue();
  
  // Calculate sync latency
  CalculateSyncLatency();
  
  // Update statistics
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.sync_operations++;
  }
}

void VideoTimelineSync::OnMarkerAnimationTimer()
{
  if (!marker_animations_enabled_) {
    return;
  }
  
  // Update marker animations
  UpdateMarkerVisuals();
}

void VideoTimelineSync::ProcessEventQueue()
{
  QMutexLocker locker(&event_mutex_);
  
  while (!event_queue_.isEmpty() && event_processing_active_) {
    SyncEvent event = event_queue_.dequeue();
    locker.unlock();
    
    ProcessSyncEvent(event);
    
    locker.relock();
  }
  
  // Update queue size statistic
  {
    QMutexLocker stats_locker(&stats_mutex_);
    sync_statistics_.queued_events = event_queue_.size();
  }
}

void VideoTimelineSync::ProcessSyncEvent(const SyncEvent& event)
{
  qDebug() << "Processing sync event:" << event.event_type << "timestamp:" << event.video_timestamp;
  
  // Create appropriate timeline marker based on event type
  if (event.event_type == "formation_detected") {
    // Event data should contain formation information
    // This would be processed in OnFormationDetected
  } else if (event.event_type == "triangle_call") {
    // Event data should contain call information
    // This would be processed in OnTriangleCallRecommended
  }
  
  emit SyncEventProcessed(event);
  
  {
    QMutexLocker locker(&stats_mutex_);
    sync_statistics_.events_processed++;
  }
}

void VideoTimelineSync::CalculateSyncLatency()
{
  if (!sync_latency_timer_.isValid()) {
    return;
  }
  
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
  
  emit SyncLatencyChanged(latency);
  sync_latency_timer_.restart();
}

void VideoTimelineSync::UpdateMarkerVisuals()
{
  // Update animated markers
  for (const QString& marker_id : animated_markers_) {
    if (marker_graphics_.contains(marker_id)) {
      TimelineMarkerItem* marker_item = static_cast<TimelineMarkerItem*>(marker_graphics_[marker_id]);
      // Animation updates would be handled by the marker item itself
    }
  }
}

void VideoTimelineSync::CleanupOldMarkers()
{
  if (!auto_cleanup_enabled_) {
    return;
  }
  
  qint64 cutoff_time = QDateTime::currentMSecsSinceEpoch() - marker_retention_time_ms_;
  QStringList markers_to_remove;
  
  {
    QMutexLocker locker(&marker_mutex_);
    
    for (auto it = timeline_markers_.constBegin(); it != timeline_markers_.constEnd(); ++it) {
      const TimelineMarker& marker = it.value();
      
      // Don't remove user-created markers or high-priority markers
      if (!marker.user_created && marker.priority < 5) {
        // Remove markers older than retention time
        if (marker.timestamp < (current_video_position_ - marker_retention_time_ms_)) {
          markers_to_remove.append(marker.marker_id);
        }
      }
    }
  }
  
  // Remove old markers
  for (const QString& marker_id : markers_to_remove) {
    RemoveTimelineMarker(marker_id);
  }
  
  if (!markers_to_remove.isEmpty()) {
    qInfo() << "Cleaned up" << markers_to_remove.size() << "old timeline markers";
  }
}

void VideoTimelineSync::UpdateSyncStatistics()
{
  QMutexLocker locker(&stats_mutex_);
  
  // Calculate timeline accuracy score based on latency and event processing
  double accuracy_score = 100.0;
  if (sync_statistics_.average_latency_ms > 100.0) {
    accuracy_score -= (sync_statistics_.average_latency_ms - 100.0) * 0.1;
  }
  
  sync_statistics_.timeline_accuracy_score = qMax(0.0, accuracy_score);
  sync_statistics_.last_sync_time = QDateTime::currentDateTime();
  
  emit StatisticsUpdated(sync_statistics_);
}

QColor VideoTimelineSync::GetMarkerColor(TimelineMarkerType type, double confidence) const
{
  QColor base_color = marker_colors_.value(type, QColor(128, 128, 128));
  
  // Adjust alpha based on confidence
  int alpha = static_cast<int>(150 + (confidence * 105)); // 150-255 range
  base_color.setAlpha(qBound(0, alpha, 255));
  
  return base_color;
}

double VideoTimelineSync::GetMarkerHeight(TimelineMarkerType type, int priority) const
{
  double base_height = 0.5;
  
  switch (type) {
    case TimelineMarkerType::Formation:
      base_height = 0.7;
      break;
    case TimelineMarkerType::TriangleCall:
      base_height = 1.0;
      break;
    case TimelineMarkerType::CoachingAlert:
      base_height = 0.6 + (priority * 0.08);
      break;
    case TimelineMarkerType::MELScore:
      base_height = 0.5;
      break;
    case TimelineMarkerType::ManualAnnotation:
      base_height = 0.8;
      break;
    case TimelineMarkerType::VideoEvent:
      base_height = 0.3;
      break;
    case TimelineMarkerType::Highlight:
      base_height = 0.9;
      break;
  }
  
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
    case TimelineMarkerType::MELScore:
      type_prefix = "mel";
      break;
    case TimelineMarkerType::ManualAnnotation:
      type_prefix = "manual";
      break;
    case TimelineMarkerType::VideoEvent:
      type_prefix = "video";
      break;
    case TimelineMarkerType::Highlight:
      type_prefix = "highlight";
      break;
  }
  
  return QString("%1_%2_%3")
         .arg(type_prefix)
         .arg(timestamp)
         .arg(QRandomGenerator::global()->generate() % 10000, 4, 10, QChar('0'));
}

void VideoTimelineSync::ValidateMarkerData(TimelineMarker& marker) const
{
  // Ensure timestamp is valid
  if (marker.timestamp < 0) {
    marker.timestamp = 0;
  }
  
  // Ensure height scale is within valid range
  marker.height_scale = qBound(0.1, marker.height_scale, 1.0);
  
  // Ensure priority is within valid range
  marker.priority = qBound(0, marker.priority, 10);
  
  // Set default color if not specified
  if (!marker.color.isValid()) {
    marker.color = GetMarkerColor(marker.type);
  }
  
  // Set default labels if empty
  if (marker.label.isEmpty()) {
    marker.label = QString("M%1").arg(static_cast<int>(marker.type));
  }
}

bool VideoTimelineSync::IsMarkerVisible(const TimelineMarker& marker) const
{
  return marker_visibility_.value(marker.type, true);
}

void VideoTimelineSync::SortMarkersByPriority()
{
  // This would be used when rendering to ensure high-priority markers appear on top
  // The actual sorting would happen during rendering in the graphics view
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
  
  if (marker_.animated) {
    setAnimated(true);
  }
}

QRectF TimelineMarkerItem::boundingRect() const
{
  return QRectF(-10, -20, 20, 40 * marker_.height_scale);
}

void TimelineMarkerItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)
  
  painter->setRenderHint(QPainter::Antialiasing);
  
  // Draw marker shape based on type
  QColor marker_color = marker_.color;
  if (is_hovered_) {
    marker_color = marker_color.lighter(120);
  }
  
  painter->setPen(QPen(marker_color.darker(120), 1));
  painter->setBrush(marker_color);
  
  QRectF rect = boundingRect();
  
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
    case TimelineMarkerType::MELScore:
      painter->drawRoundedRect(rect, 3, 3);
      break;
    default:
      painter->drawEllipse(rect);
      break;
  }
  
  // Draw label if there's space
  if (rect.height() > 15) {
    painter->setPen(marker_color.lightness() > 128 ? Qt::black : Qt::white);
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
    pulse_animation_->setEasingCurve(QEasingCurve::InOutSine);
  }
}

void TimelineMarkerItem::startPulseAnimation()
{
  if (pulse_animation_) {
    pulse_animation_->stop();
    pulse_animation_->setStartValue(0.6);
    pulse_animation_->setEndValue(1.0);
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
  // Handle mouse press - could emit clicked signal
}

void TimelineMarkerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  Q_UNUSED(event)
  // Handle mouse release - emit clicked signal to parent
}

void TimelineMarkerItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  Q_UNUSED(event)
  is_hovered_ = true;
  createTooltip();
  update();
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
  setToolTip(VideoTimelineSyncUtils::CreateMarkerTooltip(marker_));
}

void TimelineMarkerItem::createTooltip()
{
  // Tooltip is set in updateVisuals()
}

// VideoTimelineSyncUtils namespace implementation
namespace VideoTimelineSyncUtils {

int CalculateOptimalSyncPrecision(double frame_rate, double playback_rate)
{
  // Base precision on frame rate and playback speed
  double frame_duration_ms = 1000.0 / frame_rate;
  double adjusted_duration = frame_duration_ms / playback_rate;
  
  // Aim for 2-3 frames precision
  int precision = static_cast<int>(adjusted_duration * 2.5);
  
  return qBound(50, precision, 500);
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
  tooltip += QString("Time: %1<br>").arg(VideoTimestampToGameClock(marker.timestamp, 0));
  tooltip += QString("Description: %1<br>").arg(marker.description);
  
  if (marker.priority > 0) {
    tooltip += QString("Priority: %1<br>").arg(marker.priority);
  }
  
  return tooltip;
}

bool IsValidMarkerTimestamp(qint64 timestamp, qint64 video_duration)
{
  return timestamp >= 0 && (video_duration <= 0 || timestamp <= video_duration);
}

QString VideoTimestampToGameClock(qint64 video_timestamp, qint64 game_start_timestamp)
{
  qint64 game_time_ms = video_timestamp - game_start_timestamp;
  
  int total_seconds = static_cast<int>(game_time_ms / 1000);
  int hours = total_seconds / 3600;
  int minutes = (total_seconds % 3600) / 60;
  int seconds = total_seconds % 60;
  int milliseconds = static_cast<int>(game_time_ms % 1000);
  
  if (hours > 0) {
    return QString("%1:%2:%3.%4")
           .arg(hours)
           .arg(minutes, 2, 10, QChar('0'))
           .arg(seconds, 2, 10, QChar('0'))
           .arg(milliseconds, 3, 10, QChar('0'));
  } else {
    return QString("%1:%2.%3")
           .arg(minutes, 2, 10, QChar('0'))
           .arg(seconds, 2, 10, QChar('0'))
           .arg(milliseconds, 3, 10, QChar('0'));
  }
}

} // namespace VideoTimelineSyncUtils

} // namespace olive
