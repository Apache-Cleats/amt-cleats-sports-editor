/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Video Timeline Synchronization Service
  Real-time coordination between video playback and Triangle Defense data
***/

#ifndef VIDEOTIMELINESYNC_H
#define VIDEOTIMELINEYNC_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>

#include "triangle_defense_sync.h"

namespace olive {

// Forward declarations
class TimelinePanel;
class SequenceViewerPanel;

/**
 * @brief Timeline marker types for formation events
 */
enum class TimelineMarkerType {
  Formation,          // Formation detection marker
  TriangleCall,       // Triangle Defense call marker
  CoachingAlert,      // Coaching alert marker
  ManualAnnotation,   // User-added annotation
  MELScore,           // M.E.L. processing result
  VideoEvent,         // General video event (cut, transition)
  Highlight           // Important moment highlight
};

/**
 * @brief Timeline marker visual properties
 */
struct TimelineMarker {
  QString marker_id;
  TimelineMarkerType type;
  qint64 timestamp;
  QString label;
  QString description;
  QColor color;
  double height_scale; // 0.0 to 1.0, relative to timeline height
  bool animated;
  QJsonObject metadata;
  bool user_created;
  int priority; // Higher priority markers render on top
  
  TimelineMarker() : type(TimelineMarkerType::Formation), timestamp(0),
                     height_scale(1.0), animated(false), user_created(false), priority(0) {}
};

/**
 * @brief Sync event for timeline coordination
 */
struct SyncEvent {
  QString event_id;
  QString event_type;
  qint64 video_timestamp;
  qint64 system_timestamp;
  QJsonObject event_data;
  double confidence;
  QString source_component;
  bool requires_ui_update;
  
  SyncEvent() : video_timestamp(0), system_timestamp(0), confidence(1.0),
                requires_ui_update(true) {}
};

/**
 * @brief Timeline sync statistics
 */
struct SyncStatistics {
  qint64 events_processed;
  qint64 markers_created;
  qint64 sync_operations;
  double average_latency_ms;
  double timeline_accuracy_score;
  QDateTime last_sync_time;
  int active_markers;
  int queued_events;
  
  SyncStatistics() : events_processed(0), markers_created(0), sync_operations(0),
                     average_latency_ms(0.0), timeline_accuracy_score(100.0),
                     last_sync_time(QDateTime::currentDateTime()),
                     active_markers(0), queued_events(0) {}
};

/**
 * @brief Video timeline synchronization coordinator
 */
class VideoTimelineSync : public QObject
{
  Q_OBJECT

public:
  explicit VideoTimelineSync(QObject* parent = nullptr);
  virtual ~VideoTimelineSync();

  /**
   * @brief Initialize timeline synchronization
   */
  bool Initialize(TriangleDefenseSync* triangle_sync = nullptr);

  /**
   * @brief Shutdown timeline sync
   */
  void Shutdown();

  /**
   * @brief Connect timeline panel for synchronization
   */
  void ConnectTimelinePanel(TimelinePanel* timeline_panel);

  /**
   * @brief Start real-time synchronization
   */
  void StartRealTimeSync();

  /**
   * @brief Stop real-time synchronization
   */
  void StopRealTimeSync();

  /**
   * @brief Update current video position
   */
  void UpdateVideoPosition(qint64 timestamp);

  /**
   * @brief Add timeline marker
   */
  QString AddTimelineMarker(const TimelineMarker& marker);

  /**
   * @brief Remove timeline marker
   */
  bool RemoveTimelineMarker(const QString& marker_id);

  /**
   * @brief Update timeline marker
   */
  bool UpdateTimelineMarker(const QString& marker_id, const TimelineMarker& updated_marker);

  /**
   * @brief Get markers in time range
   */
  QList<TimelineMarker> GetMarkersInRange(qint64 start_timestamp, qint64 end_timestamp) const;

  /**
   * @brief Get marker at specific timestamp
   */
  TimelineMarker GetMarkerAt(qint64 timestamp, TimelineMarkerType type = TimelineMarkerType::Formation) const;

  /**
   * @brief Clear all markers
   */
  void ClearMarkers();

  /**
   * @brief Set marker visibility
   */
  void SetMarkerTypeVisible(TimelineMarkerType type, bool visible);

  /**
   * @brief Get sync statistics
   */
  SyncStatistics GetSyncStatistics() const;

  /**
   * @brief Set sync precision mode
   */
  void SetSyncPrecision(int precision_ms = 100);

  /**
   * @brief Enable/disable marker animations
   */
  void SetMarkerAnimations(bool enabled);

  /**
   * @brief Export timeline data
   */
  QJsonObject ExportTimelineData() const;

  /**
   * @brief Import timeline data
   */
  bool ImportTimelineData(const QJsonObject& timeline_data);

  /**
   * @brief Get current video position
   */
  qint64 GetCurrentVideoPosition() const { return current_video_position_; }

  /**
   * @brief Check if real-time sync is active
   */
  bool IsRealTimeSyncActive() const { return real_time_sync_active_; }

public slots:
  /**
   * @brief Handle formation detection events
   */
  void OnFormationDetected(const FormationData& formation);
  void OnFormationUpdated(const FormationData& formation);
  void OnFormationDeleted(const QString& formation_id);

  /**
   * @brief Handle Triangle Defense events
   */
  void OnTriangleCallRecommended(TriangleCall call, const FormationData& formation);
  void OnDefensiveShiftSuggested(const QString& shift_type, const QJsonObject& context);

  /**
   * @brief Handle coaching alert events
   */
  void OnCoachingAlertReceived(const CoachingAlert& alert);
  void OnCriticalAlertReceived(const CoachingAlert& alert);

  /**
   * @brief Handle M.E.L. pipeline events
   */
  void OnMELResultsUpdated(const QString& formation_id, const MELResult& results);
  void OnPipelineStatusChanged(const QString& stage, const QString& status);

  /**
   * @brief Handle video playback events
   */
  void OnVideoPlaybackStarted();
  void OnVideoPlaybackStopped();
  void OnVideoPositionChanged(qint64 position);
  void OnVideoSeekPerformed(qint64 position);
  void OnVideoRateChanged(double rate);

  /**
   * @brief Handle user interaction events
   */
  void OnTimelineClicked(qint64 timestamp);
  void OnMarkerClicked(const QString& marker_id);
  void OnMarkerRightClicked(const QString& marker_id, const QPoint& position);

signals:
  /**
   * @brief Timeline synchronization signals
   */
  void SyncStarted();
  void SyncStopped();
  void SyncPositionChanged(qint64 timestamp);
  void SyncLatencyChanged(double latency_ms);

  /**
   * @brief Marker signals
   */
  void MarkerAdded(const TimelineMarker& marker);
  void MarkerUpdated(const TimelineMarker& marker);
  void MarkerRemoved(const QString& marker_id);
  void MarkerClicked(const TimelineMarker& marker);

  /**
   * @brief Event signals
   */
  void SyncEventProcessed(const SyncEvent& event);
  void TimelineDataChanged();
  void StatisticsUpdated(const SyncStatistics& stats);

private slots:
  void OnSyncTimer();
  void OnMarkerAnimationTimer();
  void OnStatisticsTimer();
  void ProcessEventQueue();
  void CleanupOldMarkers();

private:
  void SetupTimers();
  void ProcessSyncEvent(const SyncEvent& event);
  void CreateFormationMarker(const FormationData& formation);
  void CreateTriangleCallMarker(TriangleCall call, const FormationData& formation);
  void CreateCoachingAlertMarker(const CoachingAlert& alert);
  void CreateMELScoreMarker(const QString& formation_id, const MELResult& results);
  
  void UpdateMarkerVisuals();
  void AnimateMarker(const QString& marker_id);
  void UpdateTimelineView();
  void CalculateSyncLatency();
  void OptimizeMarkerRendering();
  
  QColor GetMarkerColor(TimelineMarkerType type, double confidence = 1.0) const;
  double GetMarkerHeight(TimelineMarkerType type, int priority = 0) const;
  QString GenerateMarkerId(TimelineMarkerType type, qint64 timestamp) const;
  
  void UpdateSyncStatistics();
  void ValidateMarkerData(TimelineMarker& marker) const;
  bool IsMarkerVisible(const TimelineMarker& marker) const;
  void SortMarkersByPriority();

  // Core components
  TriangleDefenseSync* triangle_defense_sync_;
  TimelinePanel* timeline_panel_;
  
  // Synchronization state
  bool is_initialized_;
  bool real_time_sync_active_;
  qint64 current_video_position_;
  qint64 last_sync_timestamp_;
  double video_playback_rate_;
  bool video_playing_;
  
  // Timing and precision
  QTimer* sync_timer_;
  QTimer* marker_animation_timer_;
  QTimer* statistics_timer_;
  QTimer* cleanup_timer_;
  QElapsedTimer sync_latency_timer_;
  int sync_precision_ms_;
  bool marker_animations_enabled_;
  
  // Event processing
  mutable QMutex event_mutex_;
  QQueue<SyncEvent> event_queue_;
  int max_queue_size_;
  bool event_processing_active_;
  
  // Marker management
  mutable QMutex marker_mutex_;
  QMap<QString, TimelineMarker> timeline_markers_;
  QMap<TimelineMarkerType, bool> marker_visibility_;
  QMap<TimelineMarkerType, QColor> marker_colors_;
  QList<QString> animated_markers_;
  int max_markers_;
  
  // Visual rendering
  QGraphicsView* timeline_view_;
  QGraphicsScene* timeline_scene_;
  QMap<QString, QGraphicsItem*> marker_graphics_;
  QPropertyAnimation* scroll_animation_;
  QParallelAnimationGroup* marker_animation_group_;
  
  // Performance optimization
  bool auto_cleanup_enabled_;
  int cleanup_interval_ms_;
  qint64 marker_retention_time_ms_;
  bool rendering_optimizations_enabled_;
  
  // Statistics and monitoring
  mutable QMutex stats_mutex_;
  SyncStatistics sync_statistics_;
  QQueue<double> latency_history_;
  int latency_history_size_;
  
  // Configuration
  QJsonObject sync_configuration_;
  QString timeline_style_sheet_;
  bool debug_mode_enabled_;
};

/**
 * @brief Timeline marker graphics item
 */
class TimelineMarkerItem : public QGraphicsItem
{
public:
  explicit TimelineMarkerItem(const TimelineMarker& marker, QGraphicsItem* parent = nullptr);

  // QGraphicsItem interface
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setMarker(const TimelineMarker& marker);
  TimelineMarker getMarker() const { return marker_; }

  void setAnimated(bool animated);
  void startPulseAnimation();
  void stopPulseAnimation();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  void updateVisuals();
  void createTooltip();

  TimelineMarker marker_;
  bool is_animated_;
  bool is_hovered_;
  QPropertyAnimation* pulse_animation_;
  QGraphicsPixmapItem* icon_item_;
  QGraphicsTextItem* label_item_;
};

/**
 * @brief Timeline ruler for timestamp display
 */
class TimelineRuler : public QGraphicsItem
{
public:
  explicit TimelineRuler(qint64 duration_ms, QGraphicsItem* parent = nullptr);

  // QGraphicsItem interface
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void setDuration(qint64 duration_ms);
  void setCurrentPosition(qint64 position_ms);
  void setTimeScale(double pixels_per_second);

private:
  void calculateTickMarks();
  QString formatTimestamp(qint64 timestamp_ms) const;

  qint64 duration_ms_;
  qint64 current_position_ms_;
  double time_scale_;
  QList<qint64> major_ticks_;
  QList<qint64> minor_ticks_;
};

/**
 * @brief Utility functions for timeline synchronization
 */
namespace VideoTimelineSyncUtils {

/**
 * @brief Calculate optimal sync precision based on video properties
 */
int CalculateOptimalSyncPrecision(double frame_rate, double playback_rate);

/**
 * @brief Convert video timestamp to timeline position
 */
double TimestampToTimelinePosition(qint64 timestamp, qint64 duration, double timeline_width);

/**
 * @brief Convert timeline position to video timestamp
 */
qint64 TimelinePositionToTimestamp(double position, qint64 duration, double timeline_width);

/**
 * @brief Generate timeline marker tooltip text
 */
QString CreateMarkerTooltip(const TimelineMarker& marker);

/**
 * @brief Validate marker timestamp range
 */
bool IsValidMarkerTimestamp(qint64 timestamp, qint64 video_duration);

/**
 * @brief Calculate marker clustering for dense timelines
 */
QList<QList<TimelineMarker>> ClusterMarkers(const QList<TimelineMarker>& markers, 
                                           double cluster_threshold_ms = 1000.0);

/**
 * @brief Generate timeline export format
 */
QJsonObject ExportMarkersToFormat(const QList<TimelineMarker>& markers, 
                                 const QString& format = "json");

/**
 * @brief Calculate timeline rendering performance score
 */
double CalculateRenderingPerformance(int marker_count, qint64 timeline_duration);

} // namespace VideoTimelineSyncUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::TimelineMarkerType)
Q_DECLARE_METATYPE(olive::TimelineMarker)
Q_DECLARE_METATYPE(olive::SyncEvent)
Q_DECLARE_METATYPE(olive::SyncStatistics)

#endif // VIDEOTIMELINEYNC_H
